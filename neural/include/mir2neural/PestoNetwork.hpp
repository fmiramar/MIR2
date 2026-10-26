// SPDX-License-Identifier: GPL-3.0-or-later
// Native adaptation of PESTO by Alain Riou / Sony CSL Paris (LGPL-3.0),
// model.py and utils/reduce_activations.py at revision
// 62bc0c9702558f19af4593752947fb9db1eadac9:
// https://github.com/SonyCSLParis/pesto
// This adaptation is distributed under the GPL; no trained weights are embedded.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace mir2neural {

struct Result {
    float frequency = 0.0f;
    float confidence = 0.0f;
    float energy = 0.0f;
    float midi = 0.0f;
};

enum class Progress { running, done, failed };

// One workspace per simultaneous inference. The model view is immutable and can
// be shared. Neither binding nor inference allocates or accesses a file.
struct Workspace {
    std::array<float, 40 * 219> a {};
    std::array<float, 40 * 219> b {};
    std::array<float, 219> normalized {};
    std::array<float, 251> energy {};
    std::array<float, 384> probabilities {};
    std::array<float, 40 * (219 + 38 + 7)> padded {};
    Result result {};
    Progress progress = Progress::failed;
    std::size_t cumulativeMACs = 0;
    int stage = 0;
    int outputChannel = 0;
};

// Deliberately restricted to the pinned mir-1k_g7 checkpoint architecture. A
// differently shaped PESTO model must not silently reuse this implementation.
class Network {
public:
    static constexpr int inputBins = 251;
    static constexpr int croppedBins = 219;
    static constexpr int outputBins = 384;
    static constexpr std::size_t weightCount = 130575;
    // Includes zero-padded convolution MACs and Toeplitz; small normalization,
    // activation, confidence and decoder operations are additional fixed work.
    static constexpr std::size_t inferenceMACs = 28457298;

    // Caller retains ownership of immutable weights for this object's lifetime.
    // Flat C-order layout: layernorm W/B; conv1 W/B; prefilt0 W/B; prefilt1 W/B;
    // pointwise layers 0,3,6,9 W/B; Toeplitz W; confidence conv W/B;
    // confidence linear W/B; shift. See the exporter for shape validation.
    bool bind(const float* weights, std::size_t count) noexcept {
        weights_ = nullptr;
        shiftBins_ = 0;
        if (weights == nullptr || count != weightCount)
            return false;
        for (std::size_t i = 0; i < count; ++i)
            if (!std::isfinite(weights[i]))
                return false;
        // Bound the conversion independently of the host's floating-point
        // rounding mode. The selected checkpoint rounds to 20 bins.
        const double shifted = static_cast<double>(weights[count - 1] * 3.0f);
        if (std::abs(shifted) > 1000000.0)
            return false;
        const double lower = std::floor(shifted);
        const double fraction = shifted - lower;
        int rounded = static_cast<int>(lower);
        if (fraction > 0.5 || (fraction == 0.5 && (rounded % 2 != 0)))
            ++rounded; // torch.round uses ties to even, including negatives.
        shiftBins_ = ((rounded % outputBins) + outputBins) % outputBins;
        weights_ = weights;
        return true;
    }

    bool valid() const noexcept { return weights_ != nullptr; }

    // Input is natural log POWER, not dB or magnitude. PESTO.forward changes
    // its dB tensor in place by ln(10)/10 before the encoder reads it. Confidence
    // uses exp(input); cropping is 16:-16. Outputs are ungated model estimates.
    // Invalid/non-finite inputs or arithmetic return false and zero outputs.
    bool begin(const float* logPower, Workspace& work) const noexcept {
        work.result = {};
        work.progress = Progress::failed;
        work.cumulativeMACs = 0;
        work.stage = 0;
        work.outputChannel = 0;
        work.probabilities.fill(0.0f);
        if (!valid() || logPower == nullptr)
            return false;
        for (int i = 0; i < inputBins; ++i) {
            if (!std::isfinite(logPower[i]))
                return false;
            work.energy[static_cast<std::size_t>(i)] = std::exp(logPower[i]);
            if (!std::isfinite(work.energy[static_cast<std::size_t>(i)]))
                return false;
        }

        const float* cursor = weights_;
        const float* normWeight = take(cursor, croppedBins);
        const float* normBias = take(cursor, croppedBins);
        double mean = 0.0;
        for (int i = 0; i < croppedBins; ++i)
            mean += static_cast<double>(logPower[i + 16]);
        mean /= croppedBins;
        double variance = 0.0;
        for (int i = 0; i < croppedBins; ++i) {
            const double delta = static_cast<double>(logPower[i + 16]) - mean;
            variance += delta * delta;
        }
        const float meanFloat = static_cast<float>(mean);
        const float inverseStd = static_cast<float>(1.0 / std::sqrt(variance / croppedBins + 1e-5));
        for (int i = 0; i < croppedBins; ++i)
            work.normalized[static_cast<std::size_t>(i)] =
                ((logPower[i + 16] - meanFloat) * inverseStd) * normWeight[i] + normBias[i];

        work.progress = Progress::running;
        return true;
    }

    // An atomic job processes at most four convolution output channels, or
    // sixteen Toeplitz outputs. The final small reductions run with the last
    // Toeplitz job. A scheduler can compare its
    // accumulated MAC credit with nextCost() before calling stepOne().
    static std::size_t nextCost(const Workspace& work) noexcept {
        if (work.progress != Progress::running)
            return 0;
        if (work.stage == 7)
            return static_cast<std::size_t>(std::min(16, outputBins - work.outputChannel) * 657);
        const auto index = static_cast<std::size_t>(work.stage);
        return static_cast<std::size_t>(std::min(4, outputs_[index] - work.outputChannel))
            * static_cast<std::size_t>(inputs_[index] * kernels_[index] * croppedBins);
    }

    Progress stepOne(Workspace& work) const noexcept {
        if (work.progress != Progress::running)
            return work.progress;
        if (!valid())
            return fail(work);
        const auto cost = nextCost(work);
        if (work.stage == 7) {
            const float* weights = weights_ + 129421;
            const int end = std::min(outputBins, work.outputChannel + 16);
            // Vectorize across independent output rows while preserving each
            // row's reduction order. Serial dot products caused a late-hop
            // burst despite the Toeplitz layer's small nominal MAC count.
            float sums[16]{};
            for (int i = 0; i < croppedBins * 3; ++i) {
                const float value = work.a[static_cast<std::size_t>(i)];
                const float* kernel = weights + outputBins - 1 - work.outputChannel + i;
                for (int lane = 0; lane < 16; ++lane)
                    sums[lane] += value * kernel[-lane];
            }
            for (int output = work.outputChannel; output < end; ++output) {
                const float sum = sums[output - work.outputChannel];
                if (!std::isfinite(sum)) return fail(work);
                work.probabilities[static_cast<std::size_t>(output)] = sum;
            }
            work.outputChannel = end;
            work.cumulativeMACs += cost;
            if (end < outputBins) return work.progress;
            if (!finish(work))
                return fail(work);
            work.progress = Progress::done;
            return work.progress;
        }
        const auto stage = static_cast<std::size_t>(work.stage);
        const float* input = work.stage == 0 ? work.normalized.data()
            : ((work.stage % 2) == 1 ? work.a.data() : work.b.data());
        float* output = (work.stage % 2) == 0 ? work.a.data() : work.b.data();
        const float* weights = weights_ + offsets_[stage];
        const float* biases = weights + inputs_[stage] * outputs_[stage] * kernels_[stage];
        const int endChannel = std::min(outputs_[stage], work.outputChannel + 4);
        convolution(input, output, inputs_[stage], endChannel, kernels_[stage],
                    weights, biases, work.stage == 1 || work.stage == 2,
                    work.padded.data(), work.outputChannel);
        for (int i = work.outputChannel * croppedBins; i < endChannel * croppedBins; ++i)
            if (!std::isfinite(output[i]))
                return fail(work);
        work.cumulativeMACs += cost;
        work.outputChannel = endChannel;
        if (work.outputChannel == outputs_[stage]) {
            work.outputChannel = 0;
            ++work.stage;
        }
        return work.progress;
    }

    // May exceed macBudget by one atomic job; cumulativeMACs exposes its true
    // cost so a caller can carry debt into the next server block. Zero budget
    // performs no work.
    Progress step(Workspace& work, std::size_t macBudget) const noexcept {
        const auto start = work.cumulativeMACs;
        while (work.progress == Progress::running && work.cumulativeMACs - start < macBudget)
            stepOne(work);
        return work.progress;
    }

    bool infer(const float* logPower, Workspace& work, Result& result,
               float* activations = nullptr) const noexcept {
        result = {};
        if (activations != nullptr)
            std::fill_n(activations, outputBins, 0.0f);
        if (!begin(logPower, work) || step(work, inferenceMACs) != Progress::done)
            return false;
        result = work.result;
        if (activations != nullptr)
            std::copy(work.probabilities.begin(), work.probabilities.end(), activations);
        return true;
    }

private:
    inline static constexpr std::array<int, 7> inputs_ { 1, 40, 40, 40, 30, 30, 10 };
    inline static constexpr std::array<int, 7> outputs_ { 40, 40, 40, 30, 30, 10, 3 };
    inline static constexpr std::array<int, 7> kernels_ { 39, 39, 39, 1, 1, 1, 1 };
    inline static constexpr std::array<std::size_t, 7> offsets_ { 438, 2038, 64478, 126918, 128148, 129078, 129388 };
    const float* weights_ = nullptr;
    int shiftBins_ = 0;

    static Progress fail(Workspace& work) noexcept {
        work.result = {};
        work.probabilities.fill(0.0f);
        work.progress = Progress::failed;
        return work.progress;
    }

    bool finish(Workspace& work) const noexcept {
        const float* cursor = weights_ + 130461;
        const float maxLogit = *std::max_element(work.probabilities.begin(), work.probabilities.end());
        float denominator = 0.0f;
        for (float& value : work.probabilities) {
            value = std::exp(value - maxLogit);
            denominator += value;
        }
        for (float& value : work.probabilities)
            value /= denominator;

        const float* confidenceConvWeight = take(cursor, 39);
        const float confidenceConvBias = *take(cursor, 1);
        const float* confidenceLinearWeight = take(cursor, 72);
        float confidenceLogit = *take(cursor, 1);
        for (int output = 0; output < 71; ++output) {
            float sum = confidenceConvBias;
            for (int i = 0; i < 39; ++i)
                sum += work.energy[static_cast<std::size_t>(output * 3 + i)] * confidenceConvWeight[i];
            if (!std::isfinite(sum))
                return false;
            confidenceLogit += std::max(0.0f, sum) * confidenceLinearWeight[output];
        }
        float energySum = 0.0f;
        float logEnergySum = 0.0f;
        // Match log(exp(x)), including float underflow, rather than replacing
        // it with x: the upstream confidence classifier really takes this log.
        for (float value : work.energy) {
            energySum += value;
            logEnergySum += std::log(value);
        }
        const float geometricMean = std::exp(logEnergySum / inputBins);
        const float arithmeticMean = std::max(energySum / inputBins, 1e-8f);
        confidenceLogit += (geometricMean / arithmeticMean) * confidenceLinearWeight[71];
        const float confidence = 1.0f / (1.0f + std::exp(-confidenceLogit));
        if (!std::isfinite(energySum) || !std::isfinite(confidenceLogit) || !std::isfinite(confidence))
            return false;

        int best = 0;
        float bestProbability = -1.0f;
        for (int i = 0; i < outputBins; ++i) {
            const float probability = work.probabilities[static_cast<std::size_t>((i + shiftBins_) % outputBins)];
            if (probability > bestProbability) {
                best = i;
                bestProbability = probability;
            }
        }
        float weighted = 0.0f;
        float mass = 0.0f;
        for (int offset = -2; offset <= 2; ++offset) {
            // Boundary bins repeat after clamping, as in upstream gather().
            const int index = std::clamp(best + offset, 0, outputBins - 1);
            const float probability = work.probabilities[static_cast<std::size_t>((index + shiftBins_) % outputBins)];
            weighted += probability * (static_cast<float>(index) / 3.0f);
            mass += probability;
        }
        work.result.midi = weighted / mass;
        work.result.frequency = 440.0f * std::exp2((work.result.midi - 69.0f) / 12.0f);
        work.result.confidence = confidence;
        work.result.energy = energySum;
        std::rotate(work.probabilities.begin(), work.probabilities.begin() + shiftBins_, work.probabilities.end());
        return true;
    }

    static const float* take(const float*& cursor, std::size_t count) noexcept {
        const float* value = cursor;
        cursor += count;
        return value;
    }

    static void convolution(const float* input, float* output,
                            int inputChannels, int outputChannels, int kernelSize,
                            const float* weights, const float* biases,
                            bool residual, float* padded, int startChannel) noexcept {
        const int padding = kernelSize / 2;
        constexpr int paddedBins = croppedBins + 38 + 7;
        if (startChannel == 0)
            for (int in = 0; in < inputChannels; ++in) {
                float* target = padded + in * paddedBins;
                std::fill_n(target, paddedBins, 0.0f);
                std::copy_n(input + in * croppedBins, croppedBins, target + padding);
            }
        int out = startChannel;
        // Four channels by eight positions keep 32 accumulators in registers.
        // A direct scalar/output-lane loop would write them after every MAC,
        // making the two 40x40x39 convolutions memory-bandwidth limited.
        for (; out + 3 < outputChannels; out += 4) {
            const int weightStride = inputChannels * kernelSize;
            const float* w0 = weights + out * weightStride;
            const float* w1 = w0 + weightStride;
            const float* w2 = w1 + weightStride;
            const float* w3 = w2 + weightStride;
            for (int first = 0; first < croppedBins; first += 8) {
                float s0[8], s1[8], s2[8], s3[8];
                std::fill_n(s0, 8, biases[out]);
                std::fill_n(s1, 8, biases[out + 1]);
                std::fill_n(s2, 8, biases[out + 2]);
                std::fill_n(s3, 8, biases[out + 3]);
                for (int in = 0; in < inputChannels; ++in) {
                    const float* source = padded + in * paddedBins + first;
                    for (int k = 0; k < kernelSize; ++k) {
                        const int index = in * kernelSize + k;
                        for (int lane = 0; lane < 8; ++lane) {
                            const float value = source[k + lane];
                            s0[lane] += value * w0[index];
                            s1[lane] += value * w1[index];
                            s2[lane] += value * w2[index];
                            s3[lane] += value * w3[index];
                        }
                    }
                }
                const int lanes = std::min(8, croppedBins - first);
                for (int lane = 0; lane < lanes; ++lane) {
                    const int p = out * croppedBins + first + lane;
                    output[p] = leaky(s0[lane]) + (residual ? input[p] : 0.0f);
                    output[p + croppedBins] = leaky(s1[lane]) + (residual ? input[p + croppedBins] : 0.0f);
                    output[p + 2 * croppedBins] = leaky(s2[lane]) + (residual ? input[p + 2 * croppedBins] : 0.0f);
                    output[p + 3 * croppedBins] = leaky(s3[lane]) + (residual ? input[p + 3 * croppedBins] : 0.0f);
                }
            }
        }
        for (; out < outputChannels; ++out) {
            float* target = output + out * croppedBins;
            std::fill_n(target, croppedBins, biases[out]);
            for (int in = 0; in < inputChannels; ++in) {
                const float* source = input + in * croppedBins;
                const float* kernel = weights + (out * inputChannels + in) * kernelSize;
                for (int k = 0; k < kernelSize; ++k) {
                    const int offset = k - padding;
                    const int first = std::max(0, -offset);
                    const int last = std::min(croppedBins, croppedBins - offset);
                    const float gain = kernel[k];
                    // Frequency is contiguous: compilers vectorize independent
                    // output lanes without changing each lane's reduction order.
                    for (int bin = first; bin < last; ++bin)
                        target[bin] += source[bin + offset] * gain;
                }
            }
            for (int bin = 0; bin < croppedBins; ++bin) {
                const float value = target[bin];
                target[bin] = (value >= 0.0f ? value : 0.3f * value)
                    + (residual ? input[out * croppedBins + bin] : 0.0f);
            }
        }
    }

    static float leaky(float value) noexcept { return value >= 0.0f ? value : 0.3f * value; }
};

} // namespace mir2neural
