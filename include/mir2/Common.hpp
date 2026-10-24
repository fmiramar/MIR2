// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace mir2 {

constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr double kTwoPi = 2.0 * kPi;

template <typename T>
inline T clamp(T value, T low, T high) noexcept {
    return std::max(low, std::min(value, high));
}

inline bool finite(double value) noexcept {
    return std::isfinite(value);
}

inline double sanitize(double value) noexcept {
    return finite(value) ? value : 0.0;
}

// Sanitize and bound before conversion: converting a finite float outside the
// int range is undefined, even if the integer is clamped immediately afterwards.
inline int boundedInteger(double value) noexcept {
    return static_cast<int>(clamp(sanitize(value),
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max())));
}

inline bool isPowerOfTwo(int value) noexcept {
    return value > 0 && (value & (value - 1)) == 0;
}

inline int nextPowerOfTwo(int value) noexcept {
    int result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

inline double centsDistance(double first, double second) noexcept {
    if (first <= 0.0 || second <= 0.0)
        return 1.0e9;
    return std::abs(1200.0 * std::log2(first / second));
}

inline double melSlaney(double hz) noexcept {
    constexpr double breakFrequency = 1000.0;
    constexpr double linearSpacing = 200.0 / 3.0;
    const double logStep = std::log(6.4) / 27.0;
    if (hz < breakFrequency)
        return hz / linearSpacing;
    return 15.0 + std::log(hz / breakFrequency) / logStep;
}

inline double inverseMelSlaney(double mel) noexcept {
    constexpr double breakFrequency = 1000.0;
    constexpr double linearSpacing = 200.0 / 3.0;
    const double logStep = std::log(6.4) / 27.0;
    if (mel < 15.0)
        return mel * linearSpacing;
    return breakFrequency * std::exp(logStep * (mel - 15.0));
}

inline double melHTK(double hz) noexcept {
    return 2595.0 * std::log10(1.0 + hz / 700.0);
}

inline double inverseMelHTK(double mel) noexcept {
    return 700.0 * (std::pow(10.0, mel / 2595.0) - 1.0);
}

} // namespace mir2
