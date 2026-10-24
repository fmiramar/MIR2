// SPDX-License-Identifier: GPL-3.0-or-later
// Independent modulation-spectrum kernels. See docs/rhythm-modulation-implementation.md.
#pragma once
#include "Common.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mir2 {

struct ModulationTiming {
    int poolBlocks = 0, frames = 0, hopFrames = 0;
    double featureRate = 0.0;
};

inline bool modulationTiming(double sampleRate, int blockSize, double requestedRate,
                             double windowTime, double hopTime, ModulationTiming& timing) noexcept {
    if (!finite(sampleRate) || sampleRate <= 0.0 || blockSize < 1 || !finite(requestedRate)
        || requestedRate < 20.0 || requestedRate > 200.0 || !finite(windowTime)
        || windowTime < 0.25 || windowTime > 12.0 || !finite(hopTime)
        || hopTime < 0.02 || hopTime > 2.0) return false;
    const double controlRate = sampleRate / blockSize;
    timing.poolBlocks = std::max(1, boundedInteger(std::round(controlRate / requestedRate)));
    timing.featureRate = controlRate / timing.poolBlocks;
    timing.frames = std::max(3, boundedInteger(std::ceil(windowTime * timing.featureRate)));
    timing.hopFrames = std::max(1, boundedInteger(std::ceil(hopTime * timing.featureRate)));
    return true;
}

struct ModulationConfig {
    int bands = 0, bins = 0, frames = 0, normalization = 0;
    double featureRate = 0.0, minHz = 0.25, maxHz = 10.0;
    bool removeMean = true, phase = false, fluctuation = false;
    double smoothBands = 0.0, smoothBins = 0.0;
};

inline bool validModulationConfig(const ModulationConfig& config) noexcept {
    if (config.bands < 1 || config.bands > 32 || config.bins < 2 || config.bins > 128
        || config.frames < 3 || !finite(config.featureRate) || config.featureRate <= 0.0
        || !finite(config.minHz) || !finite(config.maxHz) || config.minHz < 0.0
        || config.maxHz <= config.minHz || config.maxHz > config.featureRate * 0.4
        || config.normalization < 0 || config.normalization > 2
        || !finite(config.smoothBands) || !finite(config.smoothBins)
        || config.smoothBands < 0.0 || config.smoothBands > 3.0
        || config.smoothBins < 0.0 || config.smoothBins > 3.0) return false;
    const auto matrix = static_cast<std::size_t>(config.bands) * static_cast<std::size_t>(config.bins);
    return matrix * (config.phase ? 2U : 1U) <= 1024U
        && matrix * static_cast<std::size_t>(config.frames) <= 524288U;
}

inline double modulationHz(const ModulationConfig& config, int bin) noexcept {
    return config.minHz + (config.maxHz - config.minHz) * bin / (config.bins - 1);
}

// Log-power is a declared MIR2 compression convention, not a phon/sone model.
inline double compressModulationInput(double input, int compression, double gain) noexcept {
    const double value = sanitize(input);
    return compression == 1 ? std::log1p(std::max(0.0, value) * std::max(0.0, sanitize(gain))) : value;
}

// Uniform time pooling deliberately averages held values over control blocks.
// FFT consumers therefore describe the held envelope in elapsed time, not a
// sequence in which each repeated control value is counted as a new FFT frame.
struct ModulationStream {
    ModulationConfig config;
    double* ring = nullptr;
    double* frame = nullptr;
    double pooled[32] {};
    int poolBlocks = 1, poolCount = 0, hopFrames = 1, untilAnalysis = 1;
    int position = 0, filled = 0, compression = 0;
    double gain = 1.0;

    void initialize(const ModulationConfig& settings, const ModulationTiming& timing,
                    int inputCompression, double inputGain) noexcept {
        config = settings; poolBlocks = timing.poolBlocks; hopFrames = timing.hopFrames;
        untilAnalysis = 1; poolCount = position = filled = 0;
        compression = inputCompression; gain = inputGain;
        std::fill(pooled, pooled + 32, 0.0);
        std::fill(ring, ring + config.bands * config.frames, 0.0);
    }
    bool push(const double* values) noexcept {
        for (int band = 0; band < config.bands; ++band) {
            const double bounded = clamp(sanitize(values[band]), -1.0e12, 1.0e12);
            pooled[band] += compressModulationInput(bounded, compression, gain);
        }
        if (++poolCount < poolBlocks) return false;
        for (int band = 0; band < config.bands; ++band) {
            ring[band * config.frames + position] = pooled[band] / poolBlocks;
            pooled[band] = 0.0;
        }
        poolCount = 0; position = (position + 1) % config.frames;
        filled = std::min(config.frames, filled + 1);
        if (filled < config.frames || --untilAnalysis > 0) return false;
        untilAnalysis = hopFrames;
        for (int band = 0; band < config.bands; ++band)
            for (int sample = 0; sample < config.frames; ++sample)
                frame[band * config.frames + sample] = ring[band * config.frames + (position + sample) % config.frames];
        return true;
    }
};

// Symmetric smooth weighting normalized to one at 4 Hz. This is an explicit
// fluctuation emphasis, not a calibrated psychoacoustic fluctuation-strength unit.
inline double modulationFluctuationWeight(double hz) noexcept {
    return hz > 0.0 ? 2.0 / (hz / 4.0 + 4.0 / hz) : 0.0;
}

inline double prepareModulationBasis(const ModulationConfig& config, double* window,
                                      double* realBasis, double* imagBasis) noexcept {
    double sum = 0.0;
    for (int sample = 0; sample < config.frames; ++sample) {
        window[sample] = 0.5 - 0.5 * std::cos(kTwoPi * sample / (config.frames - 1));
        sum += window[sample];
    }
    for (int bin = 0; bin < config.bins; ++bin) {
        const double frequency = modulationHz(config, bin);
        for (int sample = 0; sample < config.frames; ++sample) {
            const double angle = kTwoPi * frequency * sample / config.featureRate;
            const int index = bin * config.frames + sample;
            realBasis[index] = window[sample] * std::cos(angle);
            imagBasis[index] = -window[sample] * std::sin(angle);
        }
    }
    return sum;
}

// Caller-owned chronological band-major input and work arrays. Hann coherent
// gain gives modulation amplitudes in input units. Phase is at newest sample.
inline void modulationSpectrum(const double* frame, const ModulationConfig& config,
                                const double* window, double windowSum,
                                const double* realBasis, const double* imagBasis,
                                double* magnitudes, double* phases) noexcept {
    const int elements = config.bands * config.bins;
    std::fill(magnitudes, magnitudes + elements, 0.0);
    if (phases) std::fill(phases, phases + elements, 0.0);
    if (windowSum <= 1.0e-20) return;
    for (int band = 0; band < config.bands; ++band) {
        const double* input = frame + band * config.frames;
        double mean = 0.0, maximum = 0.0;
        for (int sample = 0; sample < config.frames; ++sample) {
            mean += sanitize(input[sample]) * window[sample];
            maximum = std::max(maximum, std::abs(sanitize(input[sample])));
        }
        mean = config.removeMean ? mean / windowSum : 0.0;
        double variation = 0.0;
        for (int sample = 0; sample < config.frames; ++sample)
            variation = std::max(variation, std::abs(sanitize(input[sample]) - mean));
        if (variation <= 1.0e-12 * std::max(1.0, maximum)) continue;
        for (int bin = 0; bin < config.bins; ++bin) {
            double real = 0.0, imaginary = 0.0;
            const int offset = bin * config.frames;
            for (int sample = 0; sample < config.frames; ++sample) {
                const double value = sanitize(input[sample]) - mean;
                real += value * realBasis[offset + sample];
                imaginary += value * imagBasis[offset + sample];
            }
            const int output = band * config.bins + bin;
            const double frequency = modulationHz(config, bin);
            const double amplitude = std::hypot(real, imaginary) * (frequency == 0.0 ? 1.0 : 2.0) / windowSum;
            magnitudes[output] = sanitize(amplitude);
            if (phases && amplitude > 1.0e-12 * std::max(1.0, maximum)) {
                const double atNewest = std::atan2(imaginary, real)
                    + kTwoPi * frequency * (config.frames - 1) / config.featureRate;
                phases[output] = std::remainder(atNewest, kTwoPi);
            }
        }
    }
}

inline void smoothModulationAxis(const double* source, double* output, int bands,
                                 int bins, double sigma, bool acrossBands) noexcept {
    if (sigma <= 0.0) { std::copy(source, source + bands * bins, output); return; }
    const int radius = std::min(9, boundedInteger(std::ceil(3.0 * sigma)));
    double weights[19];
    for (int delta = -radius; delta <= radius; ++delta)
        weights[delta + radius] = std::exp(-0.5 * delta * delta / (sigma * sigma));
    for (int band = 0; band < bands; ++band) {
        for (int bin = 0; bin < bins; ++bin) {
            double sum = 0.0, total = 0.0;
            for (int delta = -radius; delta <= radius; ++delta) {
                const int otherBand = band + (acrossBands ? delta : 0);
                const int otherBin = bin + (acrossBands ? 0 : delta);
                if (otherBand < 0 || otherBand >= bands || otherBin < 0 || otherBin >= bins) continue;
                const double weight = weights[delta + radius];
                sum += source[otherBand * bins + otherBin] * weight; total += weight;
            }
            output[band * bins + bin] = total > 0.0 ? sum / total : 0.0;
        }
    }
}

inline void shapeModulationPattern(double* magnitudes, double* scratch,
                                   const ModulationConfig& config) noexcept {
    const int elements = config.bands * config.bins;
    if (config.fluctuation)
        for (int band = 0; band < config.bands; ++band)
            for (int bin = 0; bin < config.bins; ++bin)
                magnitudes[band * config.bins + bin] *= modulationFluctuationWeight(modulationHz(config, bin));
    smoothModulationAxis(magnitudes, scratch, config.bands, config.bins, config.smoothBands, true);
    smoothModulationAxis(scratch, magnitudes, config.bands, config.bins, config.smoothBins, false);
    const int groups = config.normalization == 2 ? config.bands : 1;
    const int groupSize = config.normalization == 2 ? config.bins : elements;
    if (config.normalization == 0) return;
    for (int group = 0; group < groups; ++group) {
        double squared = 0.0;
        for (int index = 0; index < groupSize; ++index) {
            const double value = magnitudes[group * groupSize + index]; squared += value * value;
        }
        const double divisor = std::sqrt(squared);
        for (int index = 0; index < groupSize; ++index)
            magnitudes[group * groupSize + index] = divisor > 1.0e-20
                ? magnitudes[group * groupSize + index] / divisor : 0.0;
    }
}

} // namespace mir2
