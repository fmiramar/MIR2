// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

namespace mir2 {

struct EnvelopeStatistics {
    double centroid = 0.0, spread = 0.0, skewness = 0.0, kurtosis = 0.0;
    double active = 0.0, silence = 0.0, complexity = 0.0, strongDecay = 0.0;
    double decrease = 0.0, fadeSlope = 0.0, fadeFit = 0.0;
};

// Chronological RMS envelope frames. Time coordinates are frame centres in seconds.
inline EnvelopeStatistics envelopeStatistics(const double* envelope, int count,
                                             double step, double thresholdDB) noexcept {
    EnvelopeStatistics result;
    if (count < 1 || step <= 0.0) return result;
    const double threshold = std::pow(10.0, clamp(sanitize(thresholdDB), -160.0, 0.0) / 20.0);
    double energy = 0.0, peak = 0.0, mean = 0.0, temporal = 0.0;
    int silent = 0;
    for (int i = 0; i < count; ++i) {
        const double value = std::max(0.0, sanitize(envelope[i]));
        const double power = value * value;
        energy += power;
        temporal += power * (static_cast<double>(i) + 0.5) * step;
        peak = std::max(peak, value);
        mean += value;
        silent += value <= threshold;
    }
    result.silence = static_cast<double>(silent) / count;
    if (energy <= 1.0e-24) return result;
    result.centroid = temporal / energy;
    mean /= count;
    const double loudness = 10.0 * std::log10(energy / count);
    const double centre = 0.5 * static_cast<double>(count - 1);
    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    double amplitudeSlope = 0.0, xx = 0.0, xy = 0.0, yy = 0.0, sumDB = 0.0;
    double variation = 0.0;
    int active = 0, loudFrames = 0;
    for (int i = 0; i < count; ++i) {
        const double value = std::max(0.0, sanitize(envelope[i]));
        const double t = (static_cast<double>(i) + 0.5) * step - result.centroid;
        const double power = value * value;
        m2 += power * t * t;
        m3 += power * t * t * t;
        m4 += power * t * t * t * t;
        active += value >= peak * 0.1 && value > 1.0e-12;
        const double x = (static_cast<double>(i) - centre) * step;
        const double db = 20.0 * std::log10(std::max(value, threshold));
        amplitudeSlope += x * value;
        xx += x * x;
        xy += x * db;
        yy += db * db;
        sumDB += db;
        if (value > threshold) {
            variation += (db - loudness) * (db - loudness);
            ++loudFrames;
        }
    }
    const double variance = m2 / energy;
    result.spread = std::sqrt(std::max(0.0, variance));
    if (variance > 1.0e-24) {
        result.skewness = m3 / energy / std::pow(variance, 1.5);
        result.kurtosis = m4 / energy / (variance * variance);
    }
    result.active = static_cast<double>(active) / count;
    result.complexity = loudFrames ? std::sqrt(variation / loudFrames) : 0.0;
    result.strongDecay = std::sqrt(energy * step / std::max(step * 0.5, result.centroid));
    if (xx > 0.0) {
        result.decrease = -amplitudeSlope / xx / std::max(mean, 1.0e-12);
        result.fadeSlope = xy / xx;
        const double dbVariance = std::max(0.0, yy - sumDB * sumDB / count);
        result.fadeFit = dbVariance > 1.0e-12 ? clamp(xy * xy / (xx * dbVariance), 0.0, 1.0) : 0.0;
    }
    return result;
}

struct AttackMeasurement {
    double attack = 0.0, decay = 0.0, slope = 0.0;
    bool validAttack = false, validDecay = false;
};

inline double envelopeCrossing(double a, double b, double threshold, int index,
                               double step) noexcept {
    const double fraction = b == a ? 0.0 : clamp((threshold - a) / (b - a), 0.0, 1.0);
    return (static_cast<double>(index - 1) + fraction) * step;
}

// Completed, externally triggered event. Thresholds are relative to its largest RMS peak.
inline AttackMeasurement measureAttack(const double* envelope, int count, double step,
                                       double lowRatio, double highRatio) noexcept {
    AttackMeasurement result;
    if (count < 2 || step <= 0.0) return result;
    lowRatio = clamp(sanitize(lowRatio), 0.001, 0.98);
    highRatio = clamp(sanitize(highRatio), lowRatio + 0.001, 0.999);
    int peakIndex = 0;
    double peak = 0.0;
    for (int i = 0; i < count; ++i) {
        if (sanitize(envelope[i]) > peak) { peak = envelope[i]; peakIndex = i; }
    }
    if (peak <= 1.0e-12) return result;
    const double low = peak * lowRatio, high = peak * highRatio;
    double lowTime = envelope[0] >= low ? 0.0 : -1.0;
    double highTime = envelope[0] >= high ? 0.0 : -1.0;
    for (int i = 1; i <= peakIndex; ++i) {
        if (lowTime < 0.0 && envelope[i] >= low)
            lowTime = envelopeCrossing(envelope[i - 1], envelope[i], low, i, step);
        if (highTime < 0.0 && envelope[i] >= high)
            highTime = envelopeCrossing(envelope[i - 1], envelope[i], high, i, step);
    }
    if (lowTime >= 0.0 && highTime > lowTime) {
        result.attack = highTime - lowTime;
        result.slope = (high - low) / result.attack;
        result.validAttack = true;
    }
    highTime = -1.0;
    lowTime = -1.0;
    for (int i = peakIndex + 1; i < count; ++i) {
        if (highTime < 0.0 && envelope[i] <= high)
            highTime = envelopeCrossing(envelope[i - 1], envelope[i], high, i, step);
        if (highTime >= 0.0 && envelope[i] <= low) {
            lowTime = envelopeCrossing(envelope[i - 1], envelope[i], low, i, step);
            break;
        }
    }
    if (highTime >= 0.0 && lowTime > highTime) {
        result.decay = lowTime - highTime;
        result.validDecay = true;
    }
    return result;
}

} // namespace mir2
