// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"
#include "RunningMedian.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mir2 {

// Independently implemented from Dixon (DAFx 2006), Stowell/Plumbley
// (ICMC 2007), and Boeck/Widmer (DAFx 2013). No OnsetsDS code is used.
enum class OnsetMethod : int {
    flux = 0, positiveFlux, superFlux, power, magnitudeSum,
    complex, rectifiedComplex, phase, weightedPhase, modifiedKL, hfc
};

inline double wrapPhase(double value) noexcept {
    return std::remainder(sanitize(value), kTwoPi);
}

inline double complexDeviation(double magnitude, double phase,
                               double predictedMagnitude,
                               double predictedPhase) noexcept {
    const double delta = wrapPhase(phase - predictedPhase);
    // |a exp(i phi) - b exp(i theta)|, including the factor two.
    return std::sqrt(std::max(0.0, magnitude * magnitude
        + predictedMagnitude * predictedMagnitude
        - 2.0 * magnitude * predictedMagnitude * std::cos(delta)));
}

// Histories are always advanced, including bins below the magnitude gate.
inline double onsetEvidence(OnsetMethod method, const double* magnitude,
                            const double* phase, double* previousMagnitude,
                            double* previousPhase, double* previousDifference,
                            int count, double floor, bool normalize,
                            bool ready) noexcept {
    double value = 0.0;
    for (int bin = 0; bin < count; ++bin) {
        const double current = std::max(0.0, sanitize(magnitude[bin]));
        const double old = previousMagnitude[bin];
        const double angle = sanitize(phase[bin]);
        const double delta = wrapPhase(angle - previousPhase[bin]);
        const double acceleration = wrapPhase(delta - previousDifference[bin]);
        const bool interior = bin > 0 && bin + 1 < count;
        if (ready) {
            switch (method) {
            case OnsetMethod::flux: value += std::abs(current - old); break;
            case OnsetMethod::positiveFlux: value += std::max(0.0, current - old); break;
            case OnsetMethod::power: value += current * current; break;
            case OnsetMethod::magnitudeSum: value += current; break;
            case OnsetMethod::complex:
            case OnsetMethod::rectifiedComplex:
                if (interior && current > floor
                    && (method == OnsetMethod::complex || current >= old))
                    value += complexDeviation(current, angle, old,
                        previousPhase[bin] + previousDifference[bin]);
                break;
            case OnsetMethod::phase:
            case OnsetMethod::weightedPhase:
                if (interior && current > floor)
                    value += std::abs(acceleration)
                        * (method == OnsetMethod::weightedPhase ? current : 1.0);
                break;
            case OnsetMethod::modifiedKL:
                if (interior)
                    value += std::log1p(current / (old + std::max(1.0e-20, floor)));
                break;
            case OnsetMethod::hfc:
                value += static_cast<double>(bin) * current * current;
                break;
            case OnsetMethod::superFlux: break; // filtered-band path below
            }
        }
        previousMagnitude[bin] = current;
        previousPhase[bin] = angle;
        previousDifference[bin] = delta;
    }
    return sanitize(normalize ? value / static_cast<double>(count) : value);
}

constexpr int kOnsetMaxEdges = 256;

// Quarter-tone triangular bank, rounded to unique FFT bins. Its pitch scale
// makes the suppression neighborhood comparable across frequency and FFT size.
inline int makeOnsetBandEdges(int* edges, int fftSize, double sampleRate) noexcept {
    const double step = std::pow(2.0, 1.0 / 24.0);
    const double upper = std::min(17000.0, 0.5 * sampleRate);
    double frequency = 440.0;
    while (frequency > 30.0) frequency /= step;
    int count = 0;
    while (frequency <= upper * step && count < kOnsetMaxEdges) {
        const int bin = clamp(static_cast<int>(std::lround(
            frequency * static_cast<double>(fftSize) / sampleRate)), 0, fftSize / 2);
        if (count == 0 || bin > edges[count - 1]) edges[count++] = bin;
        frequency *= step;
    }
    return count;
}

inline void onsetLogBands(const double* magnitude, const int* edges, int bands,
                          double amplitudeScale, double logGain,
                          double* output) noexcept {
    for (int band = 0; band < bands; ++band) {
        const int left = edges[band], center = edges[band + 1], right = edges[band + 2];
        double sum = 0.0;
        for (int bin = left; bin < right; ++bin) {
            const double weight = bin < center
                ? static_cast<double>(bin - left) / static_cast<double>(center - left)
                : static_cast<double>(right - bin) / static_cast<double>(right - center);
            sum += weight * std::max(0.0, sanitize(magnitude[bin]));
        }
        output[band] = std::log1p(logGain * amplitudeScale * sum);
    }
}

inline double superFluxEvidence(const double* current, const double* previous,
                                int bands, int width) noexcept {
    const int radius = width > 0 ? width / 2 : 0;
    double sum = 0.0;
    for (int band = 0; band < bands; ++band) {
        double reference = previous[band];
        for (int neighbor = std::max(0, band - radius);
             neighbor <= std::min(bands - 1, band + radius); ++neighbor)
            reference = std::max(reference, previous[neighbor]);
        sum += std::max(0.0, current[band] - reference);
    }
    return bands > 0 ? sum / static_cast<double>(bands) : 0.0;
}

inline int onsetHopSamples(int fftSize, double hop, int blockSize) noexcept {
    const int block = std::max(1, blockSize);
    const int requested = static_cast<int>(fftSize * clamp(sanitize(hop), 0.0625, 1.0));
    return std::max(block, (requested / block) * block);
}

struct OnsetConfig {
    int fftSize = 1024;
    double sampleRate = 48000.0;
    double hop = 0.5;
    double medianTime = 0.15;
    int lag = 0;
    OnsetMethod method = OnsetMethod::superFlux;
};

struct OnsetControls {
    double threshold = 0.3;
    double relaxTime = 1.0;
    double minGap = 0.03;
    double floor = 1.0e-5;
    double silenceThreshold = 1.0e-4;
    double logGain = 100.0;
    double noveltyFloor = 0.01;
    double releaseTime = 0.15;
    int maxFilterWidth = 3;
    bool whitening = true;
};

struct OnsetResult { double trigger, strength, raw; };

struct OnsetState {
    OnsetConfig config;
    int edges[kOnsetMaxEdges];
    int bands, bins, historySize, frames, lag, bandIndex;
    double* processed;
    double* previousMagnitude;
    double* previousPhase;
    double* previousDifference;
    double* profile;
    double* currentBands;
    double* bandHistory;
    RunningMedian medianHistory;
    double frameTime, peak, priorNovelty, priorRaw, olderRaw, gapTime, previousRMS;
    bool armed, priorActive;

    static int historyCount(const OnsetConfig& config) noexcept {
        const double dt = config.fftSize * config.hop / config.sampleRate;
        const double requested = std::ceil(config.medianTime / dt);
        if (!finite(requested) || dt <= 0.0 || requested > RunningMedian::maxCapacity) return 0;
        return std::max(3, static_cast<int>(requested));
    }
    static int lagCount(const OnsetConfig& config) noexcept {
        return clamp(config.lag > 0 ? config.lag
            : static_cast<int>(std::lround(0.25 / config.hop)), 1, 16);
    }
    static std::size_t storageCount(const OnsetConfig& config) noexcept {
        const int count = historyCount(config);
        if (count == 0) return 0;
        return static_cast<std::size_t>(5 * (config.fftSize / 2 + 1)
            + kOnsetMaxEdges * (lagCount(config) + 1)) + RunningMedian::storageCount(count);
    }
    void initialize(const OnsetConfig& input, double* memory) noexcept {
        config = input;
        bins = config.fftSize / 2 + 1;
        bands = std::max(0, makeOnsetBandEdges(edges, config.fftSize, config.sampleRate) - 2);
        lag = lagCount(config);
        historySize = historyCount(config);
        std::fill(memory, memory + storageCount(config), 0.0);
        processed = memory; previousMagnitude = processed + bins;
        previousPhase = previousMagnitude + bins;
        previousDifference = previousPhase + bins; profile = previousDifference + bins;
        currentBands = profile + bins; bandHistory = currentBands + kOnsetMaxEdges;
        medianHistory.initialize(historySize, bandHistory + kOnsetMaxEdges * lag);
        frames = bandIndex = 0;
        frameTime = config.fftSize * config.hop / config.sampleRate;
        peak = priorNovelty = priorRaw = olderRaw = previousRMS = 0.0;
        gapTime = 1.0e6;
        armed = true; priorActive = false;
    }
    OnsetResult process(const double* magnitude, const double* phase,
                        const OnsetControls& controls) noexcept {
        const double amplitudeScale = 2.0 / static_cast<double>(config.fftSize);
        double energy = 0.0;
        for (int bin = 0; bin < bins; ++bin) {
            const double m = std::max(0.0, sanitize(magnitude[bin]));
            energy += m * m * ((bin == 0 || bin + 1 == bins) ? 1.0 : 2.0);
        }
        const double rms = std::sqrt(energy) / static_cast<double>(config.fftSize);
        const bool active = rms > std::max(0.0, controls.silenceThreshold);
        const double relaxation = std::max(frameTime, controls.relaxTime);
        const double decay = std::exp(-std::log(10.0) * frameTime / relaxation);
        double raw = 0.0;
        if (config.method == OnsetMethod::superFlux) {
            onsetLogBands(magnitude, edges, bands, amplitudeScale,
                std::max(0.01, controls.logGain), currentBands);
            if (frames >= lag)
                raw = superFluxEvidence(currentBands,
                    bandHistory + bandIndex * kOnsetMaxEdges, bands,
                    clamp(controls.maxFilterWidth, 0, 25));
            std::copy(currentBands, currentBands + bands,
                bandHistory + bandIndex * kOnsetMaxEdges);
            bandIndex = (bandIndex + 1) % lag;
        } else {
            for (int bin = 0; bin < bins; ++bin) {
                const double m = std::max(0.0, sanitize(magnitude[bin])) * amplitudeScale;
                profile[bin] = std::max(m, profile[bin] * decay);
                // MKL uses its own epsilon normalization; it is never whitened.
                processed[bin] = controls.whitening && config.method != OnsetMethod::modifiedKL
                    ? m / std::max(std::max(1.0e-12, controls.floor), profile[bin]) : m;
            }
            raw = onsetEvidence(config.method, processed, phase,
                previousMagnitude, previousPhase, previousDifference, bins,
                std::max(1.0e-12, controls.floor), true, frames >= 2);
        }
        // Abrupt offsets spread energy across bins. In the SuperFlux path,
        // reject these when window RMS falls faster than releaseTime.
        if (!active || (config.method == OnsetMethod::superFlux
            && controls.releaseTime > 0.0
            && rms < previousRMS * std::exp(-frameTime / controls.releaseTime))) raw = 0.0;
        previousRMS = rms;
        // Query preceding history, then replace its oldest value after deciding.
        const double median = medianHistory.value();
        peak = std::max(raw, peak * decay);
        const double reference = std::max(peak, std::max(1.0e-12, controls.noveltyFloor));
        const double novelty = clamp((raw - median) / reference, 0.0, 1.0);
        const double threshold = clamp(controls.threshold, 0.0, 1.0);
        // One-hop confirmation selects the preceding local maximum. A fresh
        // low crossing is required to rearm after a detection.
        gapTime = std::min(1.0e6, gapTime + frameTime);
        const bool trigger = frames >= 2 && priorActive && armed
            && priorNovelty > threshold && priorRaw > olderRaw
            && priorRaw >= raw && gapTime + 1.0e-12 >= std::max(0.0, controls.minGap);
        if (trigger) { gapTime = 0.0; armed = false; }
        if (novelty <= threshold * 0.5) armed = true;
        olderRaw = priorRaw; priorRaw = raw; priorNovelty = novelty; priorActive = active;
        medianHistory.update(raw);
        frames = std::min(1000000, frames + 1);
        return { trigger ? 1.0 : 0.0, novelty, raw };
    }
};

} // namespace mir2
