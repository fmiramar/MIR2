// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Common.hpp"

namespace mir2 {

struct WaveletFilter { double low[8]; int taps; };

// Orthonormal extremal-phase Daubechies scaling coefficients, orders 1..4.
// Mathematical constants from Daubechies (1988), Table 1; not copied library code.
inline WaveletFilter daubechiesFilter(int order) noexcept {
    switch (order) {
    case 1: return {{0.7071067811865475244, 0.7071067811865475244}, 2};
    case 2: return {{0.4829629131445341434, 0.8365163037378079056,
                    0.2241438680420133810, -0.1294095225512603812}, 4};
    case 3: return {{0.3326705529500826159, 0.8068915093110925765,
                    0.4598775021184915701, -0.1350110200102545887,
                    -0.0854412738820266617, 0.0352262918857095366}, 6};
    default: return {{0.2303778133088965009, 0.7148465705529156471,
                     0.6308807679298589079, -0.0279837694168598542,
                     -0.1870348117190930841, 0.0308413818355607636,
                     0.0328830116668851997, -0.0105974017850690321}, 8};
    }
}

inline int waveletMaxLevels(int size) noexcept {
    int result = 0; while (size > 1) { size >>= 1; ++result; } return result;
}

// Periodic critically sampled analysis. Output: A_L, D_L, ..., D_1.
inline void waveletForward(double* data, double* scratch, int size, int levels,
                           const WaveletFilter& filter) noexcept {
    int length = size;
    for (int level = 0; level < levels; ++level) {
        const int half = length / 2;
        for (int j = 0; j < half; ++j) {
            double low = 0.0, high = 0.0;
            for (int k = 0; k < filter.taps; ++k) {
                const double x = data[(2 * j + k) % length];
                low += filter.low[k] * x;
                high += (k % 2 ? -1.0 : 1.0) * filter.low[filter.taps - 1 - k] * x;
            }
            scratch[j] = low; scratch[half + j] = high;
        }
        std::copy(scratch, scratch + length, data);
        length = half;
    }
}

// Transpose of each orthonormal analysis stage: exact periodic reconstruction.
inline void waveletInverse(double* data, double* scratch, int size, int levels,
                           const WaveletFilter& filter) noexcept {
    if (levels <= 0) return;
    int length = size >> (levels - 1);
    for (int level = 0; level < levels; ++level) {
        std::fill(scratch, scratch + length, 0.0);
        const int half = length / 2;
        for (int j = 0; j < half; ++j) {
            for (int k = 0; k < filter.taps; ++k) {
                const double high = (k % 2 ? -1.0 : 1.0) * filter.low[filter.taps - 1 - k];
                scratch[(2 * j + k) % length] += filter.low[k] * data[j] + high * data[half + j];
            }
        }
        std::copy(scratch, scratch + length, data);
        length *= 2;
    }
}

// One-sample streaming adapter. The transform block is independent of the host block.
inline double waveletReconstructSample(double input, double* frame, double* output,
                                       double* scratch, int& position, int size, int levels,
                                       const WaveletFilter& filter, int remove) noexcept {
    const double result = output[position];
    frame[position] = clamp(sanitize(input), -1.0e6, 1.0e6);
    if (++position == size) {
        waveletForward(frame, scratch, size, levels, filter);
        std::fill(frame, frame + clamp(remove, 0, size), 0.0);
        waveletInverse(frame, scratch, size, levels, filter);
        std::copy(frame, frame + size, output);
        position = 0;
    }
    return result;
}

inline void waveletEnergies(const double* coefficients, int size, int levels,
                            double* bands) noexcept {
    int offset = 0, length = size >> levels;
    for (int band = 0; band <= levels; ++band) {
        double energy = 0.0;
        for (int i = 0; i < length; ++i) energy += coefficients[offset + i] * coefficients[offset + i];
        bands[band] = energy / size;
        offset += length;
        if (band > 0) length *= 2;
    }
}

inline double waveletBandFrequency(int band, int levels, double sampleRate) noexcept {
    if (band == 0) return sampleRate / std::pow(2.0, levels + 2);
    const int detailLevel = levels - band + 1;
    return sampleRate / std::pow(2.0, detailLevel + 0.5);
}

struct WaveletMeasurements { double entropy, flux, centroid, spread; };
inline WaveletMeasurements waveletMeasurements(const double* bands, const double* previous,
                                                int levels, double sampleRate) noexcept {
    double total = 0.0, oldTotal = 0.0;
    for (int i = 0; i <= levels; ++i) { total += bands[i]; oldTotal += previous[i]; }
    WaveletMeasurements result {};
    if (total <= 1e-24) return result;
    for (int i = 0; i <= levels; ++i) {
        const double p = bands[i] / total;
        const double old = oldTotal > 1e-24 ? previous[i] / oldTotal : 0.0;
        if (p > 0.0) result.entropy -= p * std::log2(p);
        result.flux += std::pow(std::max(0.0, p - old), 2);
        result.centroid += p * waveletBandFrequency(i, levels, sampleRate);
    }
    for (int i = 0; i <= levels; ++i)
        result.spread += bands[i] / total * std::pow(waveletBandFrequency(i, levels, sampleRate) - result.centroid, 2);
    result.entropy /= std::log2(levels + 1.0);
    result.flux = std::sqrt(result.flux);
    result.spread = std::sqrt(result.spread);
    return result;
}
} // namespace mir2
