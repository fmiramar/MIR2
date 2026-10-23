// SPDX-License-Identifier: GPL-3.0-or-later
// Definitions and primary references: docs/spectral-auditory-implementation.md.
#pragma once

#include "Spectral.hpp"
#include <algorithm>
#include <cmath>

namespace mir2 {

struct SpectralPeak { double frequency; double magnitude; int bin; };

// Caller owns output; no allocation. Retain strongest capacity peaks, then sort
// by frequency. A flat top is assigned to its first bin. Endpoints are excluded.
inline int extractSpectralPeaks(const double* magnitude, int numBins,
                               double binHz, double minFreq, double maxFreq,
                               double relativeThreshold, SpectralPeak* peaks,
                               int capacity, bool interpolate = true) noexcept {
    if (!magnitude || !peaks || numBins < 3 || capacity < 1 || binHz <= 0.0)
        return 0;
    int first, last;
    frequencyBins(numBins, binHz, minFreq, maxFreq, first, last);
    first = std::max(1, first);
    last = std::min(numBins - 2, last);
    double maximum = 0.0;
    for (int bin = first; bin <= last; ++bin)
        maximum = std::max(maximum, spectralWeight(magnitude[bin], 0));
    if (maximum <= 1.0e-30) return 0;
    const double threshold = maximum * clamp(sanitize(relativeThreshold), 0.0, 1.0);
    int count = 0;
    for (int bin = first; bin <= last; ++bin) {
        const double center = spectralWeight(magnitude[bin], 0);
        const double left = spectralWeight(magnitude[bin - 1], 0);
        const double right = spectralWeight(magnitude[bin + 1], 0);
        if (center < threshold || center <= left || center < right) continue;
        double offset = 0.0, height = center;
        if (interpolate && left > 1.0e-30 && right > 1.0e-30) {
            const double a = std::log(left), b = std::log(center), c = std::log(right);
            const double curvature = a - 2.0 * b + c;
            if (curvature < -1.0e-20) {
                offset = clamp(0.5 * (a - c) / curvature, -0.5, 0.5);
                height = std::exp(b - 0.25 * (a - c) * offset);
            }
        }
        SpectralPeak candidate { (static_cast<double>(bin) + offset) * binHz, height, bin };
        if (candidate.frequency < minFreq || (maxFreq > 0.0 && candidate.frequency > maxFreq))
            continue;
        if (count < capacity) peaks[count++] = candidate;
        else {
            int weakest = 0;
            for (int index = 1; index < count; ++index)
                if (peaks[index].magnitude < peaks[weakest].magnitude) weakest = index;
            if (candidate.magnitude > peaks[weakest].magnitude) peaks[weakest] = candidate;
        }
    }
    std::sort(peaks, peaks + count, [](const SpectralPeak& a, const SpectralPeak& b) {
        return a.frequency < b.frequency;
    });
    return count;
}

enum class SpectralDescriptor {
    crest, spread, slope, power, entropy, decrease, complexity, strongPeak,
    hfc, bandwidth, irregularity, variation, rms, energy, bandRatio, sparsity
};

// Selected range is inclusive. Empty/out-of-Nyquist ranges produce zero.
inline bool spectralRange(int numBins, double binHz, double minFreq,
                          double maxFreq, int& first, int& last) noexcept {
    if (numBins < 1 || binHz <= 0.0 || !finite(minFreq) || !finite(maxFreq)) return false;
    const double low = std::max(0.0, minFreq);
    const double high = maxFreq <= 0.0 ? (numBins - 1) * binHz : std::min(maxFreq, (numBins - 1) * binHz);
    if (low > high) return false;
    first = std::max(0, static_cast<int>(std::ceil(low / binHz)));
    last = std::min(numBins - 1, static_cast<int>(std::floor(high / binHz)));
    return first <= last;
}

inline double spectralDescriptor(const double* magnitudes, const double* previous,
                                 int numBins, double binHz, double minFreq,
                                 double maxFreq, int spectrumMode,
                                 SpectralDescriptor descriptor,
                                 double parameter = 2.0, int mode = 0) noexcept {
    int first, last;
    if (!spectralRange(numBins, binHz, minFreq, maxFreq, first, last)) return 0.0;
    const int count = last - first + 1;
    double sum = 0.0, squares = 0.0, maximum = 0.0, moment = 0.0;
    for (int bin = first; bin <= last; ++bin) {
        const double w = spectralWeight(magnitudes[bin], spectrumMode);
        sum += w; squares += w * w; maximum = std::max(maximum, w);
        moment += w * static_cast<double>(bin) * binHz;
    }
    if (sum <= 1.0e-30) return 0.0;
    const double centroid = moment / sum;
    switch (descriptor) {
    case SpectralDescriptor::crest: return maximum * count / sum;
    case SpectralDescriptor::power: return sum / count;
    case SpectralDescriptor::energy: return sum;
    case SpectralDescriptor::rms: return std::sqrt(sum / count); // power weights supplied by caller
    case SpectralDescriptor::strongPeak: return maximum / sum;
    case SpectralDescriptor::bandRatio: {
        double total = 0.0;
        for (int bin = 0; bin < numBins; ++bin) total += spectralWeight(magnitudes[bin], spectrumMode);
        return total > 1.0e-30 ? sum / total : 0.0;
    }
    case SpectralDescriptor::sparsity:
        return count > 1 ? clamp((std::sqrt(static_cast<double>(count)) - sum / std::sqrt(squares))
                                 / (std::sqrt(static_cast<double>(count)) - 1.0), 0.0, 1.0) : 1.0;
    case SpectralDescriptor::entropy: {
        double entropy = 0.0;
        for (int bin = first; bin <= last; ++bin) {
            const double p = spectralWeight(magnitudes[bin], spectrumMode) / sum;
            if (p > 0.0) entropy -= p * std::log(p);
        }
        if (mode == 0) return count > 1 ? entropy / std::log(static_cast<double>(count)) : 0.0;
        return mode == 2 ? entropy / std::log(2.0) : entropy;
    }
    case SpectralDescriptor::spread:
    case SpectralDescriptor::bandwidth: {
        const double p = descriptor == SpectralDescriptor::spread ? 2.0 : clamp(sanitize(parameter), 0.25, 8.0);
        double momentP = 0.0;
        for (int bin = first; bin <= last; ++bin)
            momentP += spectralWeight(magnitudes[bin], spectrumMode)
                * std::pow(std::abs(bin * binHz - centroid), p);
        return std::pow(momentP / sum, 1.0 / p);
    }
    case SpectralDescriptor::slope: {
        const double centerHz = 0.5 * (first + last) * binHz;
        double numerator = 0.0, denominator = 0.0;
        for (int bin = first; bin <= last; ++bin) {
            const double x = bin * binHz - centerHz;
            numerator += x * spectralWeight(magnitudes[bin], spectrumMode);
            denominator += x * x;
        }
        return denominator > 0.0 ? numerator / (denominator * sum) : 0.0;
    }
    case SpectralDescriptor::decrease: {
        const double reference = spectralWeight(magnitudes[first], spectrumMode);
        double numerator = 0.0, denominator = 0.0;
        for (int bin = first + 1; bin <= last; ++bin) {
            const double w = spectralWeight(magnitudes[bin], spectrumMode);
            numerator += (w - reference) / (bin - first);
            denominator += w;
        }
        return denominator > 1.0e-30 ? numerator / denominator : 0.0;
    }
    case SpectralDescriptor::complexity: {
        double peaks = 0.0;
        const double threshold = maximum * clamp(sanitize(parameter), 0.0, 1.0);
        for (int bin = std::max(1, first); bin <= std::min(last, numBins - 2); ++bin) {
            const double w = spectralWeight(magnitudes[bin], spectrumMode);
            if (w >= threshold && w > spectralWeight(magnitudes[bin - 1], spectrumMode)
                && w >= spectralWeight(magnitudes[bin + 1], spectrumMode))
                peaks += mode == 0 ? 1.0 : w / maximum;
        }
        return peaks;
    }
    case SpectralDescriptor::hfc: {
        double result = 0.0;
        for (int bin = first; bin <= last; ++bin) {
            const double f = static_cast<double>(bin) * binHz;
            result += spectralWeight(magnitudes[bin], spectrumMode) * (mode == 0 ? f : f * f);
        }
        return result;
    }
    case SpectralDescriptor::irregularity: {
        double numerator = 0.0;
        if (mode == 0) {
            for (int bin = first; bin < last; ++bin) {
                const double delta = spectralWeight(magnitudes[bin], spectrumMode)
                    - spectralWeight(magnitudes[bin + 1], spectrumMode);
                numerator += delta * delta;
            }
            return numerator / squares;
        }
        for (int bin = first + 1; bin < last; ++bin) {
            const double a = spectralWeight(magnitudes[bin - 1], spectrumMode);
            const double b = spectralWeight(magnitudes[bin], spectrumMode);
            const double c = spectralWeight(magnitudes[bin + 1], spectrumMode);
            numerator += std::abs(b - (a + b + c) / 3.0);
        }
        return numerator; // Krimphoff raw L1 deviation, amplitude-dependent.
    }
    case SpectralDescriptor::variation: {
        if (!previous) return 0.0;
        double cross = 0.0, oldSquares = 0.0;
        for (int bin = first; bin <= last; ++bin) {
            const double old = spectralWeight(previous[bin], spectrumMode);
            cross += spectralWeight(magnitudes[bin], spectrumMode) * old;
            oldSquares += old * old;
        }
        return oldSquares > 1.0e-30 ? clamp(1.0 - cross / std::sqrt(squares * oldSquares), 0.0, 1.0) : 0.0;
    }
    }
    return 0.0;
}

inline void spectralContrast(const double* magnitudes, int first, int last,
                             int spectrumMode, double quantile, double floor,
                             double* scratch, double& contrast, double& valley) noexcept {
    const int count = last - first + 1;
    contrast = valley = 0.0;
    if (count < 1) return;
    for (int bin = first; bin <= last; ++bin)
        scratch[bin - first] = spectralWeight(magnitudes[bin], spectrumMode);
    std::sort(scratch, scratch + count);
    if (scratch[count - 1] <= 1.0e-30) return;
    const int selected = clamp(static_cast<int>(std::ceil(count * clamp(quantile, 0.001, 0.5))), 1, count);
    double low = 0.0, high = 0.0;
    for (int index = 0; index < selected; ++index) {
        low += scratch[index]; high += scratch[count - 1 - index];
    }
    low /= selected; high /= selected;
    const double scale = spectrumMode == 0 ? 20.0 : 10.0;
    const double safeFloor = std::max(1.0e-30, sanitize(floor));
    valley = scale * std::log10(std::max(low, safeFloor));
    contrast = scale * std::log10(std::max(high, safeFloor)) - valley;
}

} // namespace mir2
