// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

namespace mir2 {

// HTK symmetric regression, evaluated N frames late in a causal implementation.
inline double regressionDelta(const double* values, int radius, double frameTime) noexcept {
    if (radius < 1 || frameTime <= 0.0) return 0.0;
    double numerator = 0.0, denominator = 0.0;
    for (int n = 1; n <= radius; ++n) {
        numerator += n * (sanitize(values[radius + n]) - sanitize(values[radius - n]));
        denominator += 2.0 * n * n;
    }
    return numerator / (denominator * frameTime);
}

inline double featureMedian(double* scratch, int count) noexcept {
    if (count <= 0) return 0.0;
    for (int i = 0; i < count; ++i) scratch[i] = sanitize(scratch[i]);
    std::sort(scratch, scratch + count);
    const int centre = count / 2;
    return count % 2 ? scratch[centre] : 0.5 * (scratch[centre - 1] + scratch[centre]);
}

inline double smoothFeature(double previous, double value, double seconds,
                            double frameTime) noexcept {
    value = sanitize(value);
    if (seconds <= 0.0 || frameTime <= 0.0) return value;
    const double coefficient = std::exp(-frameTime / seconds);
    return coefficient * sanitize(previous) + (1.0 - coefficient) * value;
}

} // namespace mir2
