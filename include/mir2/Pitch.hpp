// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"
#include "FFT.hpp"

#include <algorithm>
#include <cmath>

namespace mir2 {

struct PitchWorkspace {
    Complex* fftData;
    double* prefixEnergy;
    double* difference;
    double* cmndf;
    double* nsdf;
    int frameSize;
    int fftSize;
};

struct PitchResult {
    double frequency;
    double confidence;
    double voiced;
    double octaveAmbiguity;
    double periodicity;
};

inline double parabolicOffset(double left, double center, double right) noexcept {
    const double denominator = left - 2.0 * center + right;
    if (std::abs(denominator) < 1.0e-20)
        return 0.0;
    return clamp(0.5 * (left - right) / denominator, -1.0, 1.0);
}

inline bool preparePitchEvidence(const float* frame, PitchWorkspace& workspace,
                                 double& rms) noexcept {
    const int size = workspace.frameSize;
    if (frame == nullptr || workspace.fftData == nullptr || size <= 0
        || workspace.fftSize < size * 2)
        return false;

    double mean = 0.0;
    for (int index = 0; index < size; ++index)
        mean += sanitize(frame[index]);
    mean /= static_cast<double>(size);

    workspace.prefixEnergy[0] = 0.0;
    double totalEnergy = 0.0;
    for (int index = 0; index < size; ++index) {
        const double sample = sanitize(frame[index]) - mean;
        workspace.fftData[index] = { sample, 0.0 };
        totalEnergy += sample * sample;
        workspace.prefixEnergy[index + 1] = totalEnergy;
    }
    for (int index = size; index < workspace.fftSize; ++index)
        workspace.fftData[index] = { 0.0, 0.0 };

    rms = std::sqrt(totalEnergy / static_cast<double>(size));
    if (totalEnergy <= 1.0e-20) {
        std::fill(workspace.difference, workspace.difference + size, 0.0);
        std::fill(workspace.cmndf, workspace.cmndf + size, 1.0);
        std::fill(workspace.nsdf, workspace.nsdf + size, 0.0);
        return false;
    }

    fft(workspace.fftData, workspace.fftSize, false);
    for (int index = 0; index < workspace.fftSize; ++index) {
        const double power = workspace.fftData[index].real * workspace.fftData[index].real
            + workspace.fftData[index].imag * workspace.fftData[index].imag;
        workspace.fftData[index] = { power, 0.0 };
    }
    fft(workspace.fftData, workspace.fftSize, true);

    workspace.difference[0] = 0.0;
    workspace.cmndf[0] = 1.0;
    workspace.nsdf[0] = 1.0;
    double cumulativeDifference = 0.0;
    for (int lag = 1; lag < size; ++lag) {
        const double firstEnergy = workspace.prefixEnergy[size - lag];
        const double secondEnergy = totalEnergy - workspace.prefixEnergy[lag];
        const double denominator = std::max(0.0, firstEnergy + secondEnergy);
        const double autocorrelation = workspace.fftData[lag].real;
        const double difference = std::max(0.0, denominator - 2.0 * autocorrelation);
        workspace.difference[lag] = difference;
        cumulativeDifference += difference;
        workspace.cmndf[lag] = cumulativeDifference > 1.0e-30
            ? difference * static_cast<double>(lag) / cumulativeDifference : 1.0;
        workspace.nsdf[lag] = denominator > 1.0e-30
            ? clamp(2.0 * autocorrelation / denominator, -1.0, 1.0) : 0.0;
    }
    return true;
}

inline void lagBounds(double sampleRate, double minFreq, double maxFreq,
                      int frameSize, int& minLag, int& maxLag) noexcept {
    const double safeMin = std::max(1.0, minFreq);
    const double safeMax = std::max(safeMin, maxFreq);
    minLag = clamp(static_cast<int>(std::floor(sampleRate / safeMax)), 2, frameSize - 2);
    maxLag = clamp(static_cast<int>(std::ceil(sampleRate / safeMin)), minLag + 1, frameSize / 2);
}

inline double refinedFrequency(const double* curve, int lag, double sampleRate,
                               bool peak) noexcept {
    const double offset = peak
        ? parabolicOffset(-curve[lag - 1], -curve[lag], -curve[lag + 1])
        : parabolicOffset(curve[lag - 1], curve[lag], curve[lag + 1]);
    const double period = static_cast<double>(lag) + offset;
    return period > 0.0 ? sampleRate / period : 0.0;
}

inline bool octaveRelated(double first, double second) noexcept {
    if (first <= 0.0 || second <= 0.0)
        return false;
    const double ratio = first / second;
    constexpr double toleranceCents = 80.0;
    return centsDistance(ratio, 0.25) <= toleranceCents
        || centsDistance(ratio, 0.5) <= toleranceCents
        || centsDistance(ratio, 2.0) <= toleranceCents
        || centsDistance(ratio, 4.0) <= toleranceCents;
}

inline double yinOctaveAmbiguity(const PitchWorkspace& workspace, int selectedLag,
                                 double selectedScore, int minLag, int maxLag,
                                 double sampleRate) noexcept {
    const double selectedFrequency = refinedFrequency(workspace.cmndf, selectedLag, sampleRate, false);
    double competitor = 0.0;
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        if (lag == selectedLag || workspace.cmndf[lag] > workspace.cmndf[lag - 1]
            || workspace.cmndf[lag] > workspace.cmndf[lag + 1])
            continue;
        const double frequency = refinedFrequency(workspace.cmndf, lag, sampleRate, false);
        if (octaveRelated(frequency, selectedFrequency))
            competitor = std::max(competitor, clamp(1.0 - workspace.cmndf[lag], 0.0, 1.0));
    }
    const double denominator = selectedScore + competitor;
    return denominator > 1.0e-20 ? clamp(competitor / denominator, 0.0, 1.0) : 0.0;
}

inline PitchResult analyzeYIN(PitchWorkspace& workspace, double sampleRate,
                              double minFreq, double maxFreq,
                              double threshold, double rms) noexcept {
    PitchResult result { 0.0, 0.0, 0.0, 0.0, 0.0 };
    if (rms <= 1.0e-8)
        return result;
    int minLag = 0;
    int maxLag = 0;
    lagBounds(sampleRate, minFreq, maxFreq, workspace.frameSize, minLag, maxLag);
    int selected = -1;
    const double safeThreshold = clamp(threshold, 0.01, 0.8);
    for (int lag = minLag; lag < maxLag; ++lag) {
        if (workspace.cmndf[lag] < safeThreshold) {
            selected = lag;
            while (selected + 1 <= maxLag
                   && workspace.cmndf[selected + 1] < workspace.cmndf[selected])
                ++selected;
            break;
        }
    }
    if (selected < 0) {
        selected = minLag;
        for (int lag = minLag + 1; lag <= maxLag; ++lag) {
            if (workspace.cmndf[lag] < workspace.cmndf[selected])
                selected = lag;
        }
    }
    const double score = clamp(1.0 - workspace.cmndf[selected], 0.0, 1.0);
    result.frequency = refinedFrequency(workspace.cmndf, selected, sampleRate, false);
    result.confidence = score;
    result.voiced = score >= std::max(0.5, 1.0 - safeThreshold * 2.0) ? 1.0 : 0.0;
    result.octaveAmbiguity = yinOctaveAmbiguity(workspace, selected, score,
                                                minLag, maxLag, sampleRate);
    result.periodicity = score;
    if (!finite(result.frequency) || result.voiced == 0.0)
        result.frequency = 0.0;
    return result;
}

inline double nsdfOctaveAmbiguity(const PitchWorkspace& workspace, int selectedLag,
                                  double selectedScore, int minLag, int maxLag,
                                  double sampleRate) noexcept {
    const double selectedFrequency = refinedFrequency(workspace.nsdf, selectedLag, sampleRate, true);
    double competitor = 0.0;
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        if (lag == selectedLag || workspace.nsdf[lag] < workspace.nsdf[lag - 1]
            || workspace.nsdf[lag] < workspace.nsdf[lag + 1])
            continue;
        const double frequency = refinedFrequency(workspace.nsdf, lag, sampleRate, true);
        if (octaveRelated(frequency, selectedFrequency))
            competitor = std::max(competitor, clamp(workspace.nsdf[lag], 0.0, 1.0));
    }
    const double denominator = selectedScore + competitor;
    return denominator > 1.0e-20 ? clamp(competitor / denominator, 0.0, 1.0) : 0.0;
}

inline PitchResult analyzeMPM(PitchWorkspace& workspace, double sampleRate,
                              double minFreq, double maxFreq,
                              double cutoff, double rms) noexcept {
    PitchResult result { 0.0, 0.0, 0.0, 0.0, 0.0 };
    if (rms <= 1.0e-8)
        return result;
    int minLag = 0;
    int maxLag = 0;
    lagBounds(sampleRate, minFreq, maxFreq, workspace.frameSize, minLag, maxLag);
    double highest = 0.0;
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        if (workspace.nsdf[lag] >= workspace.nsdf[lag - 1]
            && workspace.nsdf[lag] > workspace.nsdf[lag + 1])
            highest = std::max(highest, workspace.nsdf[lag]);
    }
    if (highest <= 0.0)
        return result;
    const double peakCutoff = clamp(cutoff, 0.5, 1.0) * highest;
    int selected = -1;
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        if (workspace.nsdf[lag] >= workspace.nsdf[lag - 1]
            && workspace.nsdf[lag] > workspace.nsdf[lag + 1]
            && workspace.nsdf[lag] >= peakCutoff) {
            selected = lag;
            break;
        }
    }
    if (selected < 0)
        return result;
    const double clarity = clamp(workspace.nsdf[selected], 0.0, 1.0);
    result.frequency = refinedFrequency(workspace.nsdf, selected, sampleRate, true);
    result.confidence = clarity;
    result.voiced = clarity >= 0.5 ? 1.0 : 0.0;
    result.octaveAmbiguity = nsdfOctaveAmbiguity(workspace, selected, clarity,
                                                 minLag, maxLag, sampleRate);
    result.periodicity = highest;
    if (!finite(result.frequency) || result.voiced == 0.0)
        result.frequency = 0.0;
    return result;
}

inline double frameVoicingProbability(const float* frame, int frameSize,
                                      double sampleRate, double maxFreq,
                                      double periodicity, double rms) noexcept {
    if (rms <= 1.0e-8)
        return 0.0;
    int crossings = 0;
    double previous = sanitize(frame[0]);
    for (int index = 1; index < frameSize; ++index) {
        const double current = sanitize(frame[index]);
        if ((current >= 0.0) != (previous >= 0.0))
            ++crossings;
        previous = current;
    }
    const double crossingFrequency = static_cast<double>(crossings) * sampleRate
        / (2.0 * static_cast<double>(frameSize));
    const double crossingEvidence = 1.0 - clamp((crossingFrequency - maxFreq)
        / std::max(1.0, maxFreq * 2.0), 0.0, 1.0);
    const double energyEvidence = clamp((rms - 1.0e-6) / (1.0e-3 - 1.0e-6), 0.0, 1.0);
    const double probability = clamp(periodicity, 0.0, 1.0)
        * energyEvidence * (0.65 + 0.35 * crossingEvidence);
    return clamp(probability, 0.0, 1.0);
}

} // namespace mir2
