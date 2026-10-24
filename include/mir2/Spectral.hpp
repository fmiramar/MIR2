// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cmath>

namespace mir2 {

inline double spectralWeight(double magnitude, int spectrumMode) noexcept {
    const double clean = std::max(0.0, sanitize(magnitude));
    return spectrumMode == 0 ? clean : clean * clean;
}

inline void frequencyBins(int numBins, double binHz, double minFreq,
                          double maxFreq, int& first, int& last) noexcept {
    const double nyquist = binHz * static_cast<double>(numBins - 1);
    const double upper = maxFreq <= 0.0 ? nyquist : std::min(maxFreq, nyquist);
    first = clamp(boundedInteger(std::ceil(std::max(0.0, minFreq) / binHz)), 0, numBins - 1);
    last = clamp(boundedInteger(std::floor(upper / binHz)), first, numBins - 1);
}

inline double spectralCentroid(const double* magnitude, int numBins,
                               double binHz, double minFreq, double maxFreq,
                               int spectrumMode) noexcept {
    int first = 0;
    int last = 0;
    frequencyBins(numBins, binHz, minFreq, maxFreq, first, last);
    double weighted = 0.0;
    double total = 0.0;
    for (int bin = first; bin <= last; ++bin) {
        const double weight = spectralWeight(magnitude[bin], spectrumMode);
        weighted += weight * static_cast<double>(bin) * binHz;
        total += weight;
    }
    return total > 1.0e-30 ? weighted / total : 0.0;
}

inline double spectralFlatness(const double* magnitude, int numBins,
                               double binHz, double minFreq, double maxFreq,
                               int spectrumMode, double floor) noexcept {
    int first = 0;
    int last = 0;
    frequencyBins(numBins, binHz, minFreq, maxFreq, first, last);
    const int count = last - first + 1;
    double logSum = 0.0;
    double arithmetic = 0.0;
    double rawTotal = 0.0;
    const double safeFloor = std::max(1.0e-30, floor);
    for (int bin = first; bin <= last; ++bin) {
        const double value = spectralWeight(magnitude[bin], spectrumMode);
        rawTotal += value;
        arithmetic += std::max(value, safeFloor);
        logSum += std::log(std::max(value, safeFloor));
    }
    if (rawTotal <= 1.0e-30 || count <= 0)
        return 0.0;
    const double geometric = std::exp(logSum / static_cast<double>(count));
    return clamp(geometric / (arithmetic / static_cast<double>(count)), 0.0, 1.0);
}

inline double spectralPercentile(const double* magnitude, int numBins,
                                 double binHz, double fraction,
                                 double minFreq, double maxFreq,
                                 int spectrumMode, bool interpolate) noexcept {
    int first = 0;
    int last = 0;
    frequencyBins(numBins, binHz, minFreq, maxFreq, first, last);
    double total = 0.0;
    for (int bin = first; bin <= last; ++bin)
        total += spectralWeight(magnitude[bin], spectrumMode);
    if (total <= 1.0e-30)
        return 0.0;
    const double target = clamp(fraction, 0.0, 1.0) * total;
    double accumulated = 0.0;
    for (int bin = first; bin <= last; ++bin) {
        const double value = spectralWeight(magnitude[bin], spectrumMode);
        const double previous = accumulated;
        accumulated += value;
        if (accumulated >= target) {
            if (!interpolate || value <= 1.0e-30)
                return static_cast<double>(bin) * binHz;
            const double within = clamp((target - previous) / value, 0.0, 1.0);
            return (static_cast<double>(bin) - 0.5 + within) * binHz;
        }
    }
    return static_cast<double>(last) * binHz;
}

inline double spectralFlux(const double* current, const double* previous,
                           int count, bool positive, bool normalize) noexcept {
    double sum = 0.0;
    double norm = 0.0;
    for (int index = 0; index < count; ++index) {
        double difference = sanitize(current[index]) - sanitize(previous[index]);
        if (positive)
            difference = std::max(0.0, difference);
        sum += difference * difference;
        norm += current[index] * current[index];
    }
    const double result = std::sqrt(std::max(0.0, sum));
    if (!normalize)
        return result;
    return norm > 1.0e-30 ? result / std::sqrt(norm) : 0.0;
}

inline void makeMelEdges(double* edgesHz, int numBands, double minFreq,
                         double maxFreq, int melMode) noexcept {
    const auto toMel = melMode == 0 ? melSlaney : melHTK;
    const auto fromMel = melMode == 0 ? inverseMelSlaney : inverseMelHTK;
    const double low = toMel(minFreq);
    const double high = toMel(maxFreq);
    for (int index = 0; index < numBands + 2; ++index) {
        const double position = static_cast<double>(index) / static_cast<double>(numBands + 1);
        edgesHz[index] = fromMel(low + position * (high - low));
    }
}

inline void triangularBands(const double* magnitude, int numBins, double binHz,
                            const double* edgesHz, int numBands,
                            int spectrumMode, double* bands) noexcept {
    for (int band = 0; band < numBands; ++band) {
        const double left = edgesHz[band];
        const double center = edgesHz[band + 1];
        const double right = edgesHz[band + 2];
        double sum = 0.0;
        double weightSum = 0.0;
        const int first = clamp(static_cast<int>(std::floor(left / binHz)), 0, numBins - 1);
        const int last = clamp(static_cast<int>(std::ceil(right / binHz)), first, numBins - 1);
        for (int bin = first; bin <= last; ++bin) {
            const double hz = static_cast<double>(bin) * binHz;
            double weight = 0.0;
            if (hz >= left && hz <= center && center > left)
                weight = (hz - left) / (center - left);
            else if (hz > center && hz <= right && right > center)
                weight = (right - hz) / (right - center);
            sum += weight * spectralWeight(magnitude[bin], spectrumMode);
            weightSum += weight;
        }
        bands[band] = weightSum > 0.0 ? sum / weightSum : 0.0;
    }
}

inline double dctII(const double* input, int size, int coefficient,
                    bool orthonormal) noexcept {
    double result = 0.0;
    for (int index = 0; index < size; ++index) {
        result += input[index] * std::cos(kPi * (static_cast<double>(index) + 0.5)
                                         * static_cast<double>(coefficient)
                                         / static_cast<double>(size));
    }
    if (orthonormal) {
        const double scale = coefficient == 0
            ? std::sqrt(1.0 / static_cast<double>(size))
            : std::sqrt(2.0 / static_cast<double>(size));
        result *= scale;
    }
    return sanitize(result);
}

} // namespace mir2
