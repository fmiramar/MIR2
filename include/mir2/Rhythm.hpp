// SPDX-License-Identifier: GPL-3.0-or-later
// Independent causal rhythm algorithms; equations and sources in docs/rhythm-implementation.md.
#pragma once
#include "FFT.hpp"
#include <algorithm>
#include <cmath>

namespace mir2 {

inline double wrapUnit(double value) noexcept { return value - std::floor(value); }
inline double phaseDistance(double target, double current) noexcept {
    return wrapUnit(target - current + 0.5) - 0.5;
}

struct RhythmWorkspace {
    double* frame;       // chronological, nonnegative novelty, capacity windowFrames
    double* prefix;      // centered-frame squared energy prefix, capacity windowFrames+1
    Complex* fft;        // capacity fftSize >= nextPowerOfTwo(2*windowFrames)
    double* correlation; // capacity windowFrames
    double* autocorr;    // tempo-bin outputs, capacity numBins
    double* fourier;
    double* evidence;
    double* phases;
    int windowFrames, fftSize, numBins;
    double featureRate, minBPM, maxBPM;
};

inline double rhythmBinBPM(const RhythmWorkspace& work, int bin) noexcept {
    return work.minBPM + (work.maxBPM - work.minBPM) * bin / (work.numBins - 1);
}

struct FourierRhythmResult { double magnitude; double normalized; double phase; };

// Direct Hann-windowed Fourier projection at an arbitrary BPM, phase referred
// to the newest sample. Recurrence avoids trig calls for every sample.
inline FourierRhythmResult rhythmFourier(const double* frame, int count,
                                         double featureRate, double bpm,
                                         double mean) noexcept {
    if (count < 3 || featureRate <= 0.0 || bpm <= 0.0) return {0,0,0};
    const double omega = kTwoPi * bpm / (60.0 * featureRate);
    const double rootR = std::cos(omega), rootI = -std::sin(omega);
    const double windowOmega = kTwoPi / (count - 1);
    const double windowR = std::cos(windowOmega), windowI = std::sin(windowOmega);
    double r = 1.0, i = 0.0, wr = 1.0, wi = 0.0;
    double real = 0.0, imaginary = 0.0, l1 = 0.0;
    for (int n = 0; n < count; ++n) {
        const double value = (0.5 - 0.5 * wr) * (frame[n] - mean);
        real += value * r; imaginary += value * i; l1 += std::abs(value);
        const double nextR = r * rootR - i * rootI;
        i = r * rootI + i * rootR; r = nextR;
        const double nextW = wr * windowR - wi * windowI;
        wi = wr * windowI + wi * windowR; wr = nextW;
    }
    const double magnitude = std::hypot(real, imaginary);
    return {magnitude, l1 > 1.0e-20 ? clamp(magnitude / l1, 0.0, 1.0) : 0.0,
            wrapUnit((omega * (count - 1) + std::atan2(imaginary, real)) / kTwoPi)};
}

// Positive mean-centered overlap-normalized correlation. Fourier and ACF use
// trailing windows; no future samples or zero-padding disguised as evidence.
inline void rhythmEvidence(RhythmWorkspace& work, int count, bool normalize = true) noexcept {
    std::fill(work.autocorr, work.autocorr + work.numBins, 0.0);
    std::fill(work.fourier, work.fourier + work.numBins, 0.0);
    std::fill(work.evidence, work.evidence + work.numBins, 0.0);
    std::fill(work.phases, work.phases + work.numBins, 0.0);
    if (count < 3) return;
    double mean = 0.0;
    for (int n = 0; n < count; ++n) mean += work.frame[n];
    mean /= count;
    work.prefix[0] = 0.0;
    for (int n = 0; n < work.fftSize; ++n) {
        const double value = n < count ? work.frame[n] - mean : 0.0;
        work.fft[n] = {value, 0.0};
        if (n < count) work.prefix[n + 1] = work.prefix[n] + value * value;
    }
    const double energy = work.prefix[count];
    if (energy <= 1.0e-20) return;
    fft(work.fft, work.fftSize, false);
    for (int n = 0; n < work.fftSize; ++n)
        work.fft[n] = {work.fft[n].real * work.fft[n].real + work.fft[n].imag * work.fft[n].imag, 0.0};
    fft(work.fft, work.fftSize, true);
    for (int lag = 0; lag < count; ++lag) {
        const double denominator = std::sqrt(std::max(0.0, work.prefix[count - lag]
            * (work.prefix[count] - work.prefix[lag])));
        work.correlation[lag] = denominator > 1.0e-20
            ? clamp(work.fft[lag].real / denominator, 0.0, 1.0) : 0.0;
    }
    for (int bin = 0; bin < work.numBins; ++bin) {
        const double bpm = rhythmBinBPM(work, bin);
        const double lag = 60.0 * work.featureRate / bpm;
        // Require two periods for a tempo claim; unsupported slow bins remain zero.
        if (lag < 1.0 || 2.0 * lag > count - 1) continue;
        const int low = static_cast<int>(std::floor(lag));
        const double fraction = lag - low;
        const double correlation = (1.0-fraction) * work.correlation[low]
            + fraction * work.correlation[low + 1];
        const FourierRhythmResult fourier = rhythmFourier(work.frame, count, work.featureRate, bpm, mean);
        if (normalize) {
            work.autocorr[bin] = correlation;
            work.fourier[bin] = fourier.normalized;
        } else {
            const double rawLow = std::max(0.0, work.fft[low].real) / (count-low);
            const double rawHigh = std::max(0.0, work.fft[low+1].real) / (count-low-1);
            work.autocorr[bin] = (1.0-fraction) * rawLow + fraction * rawHigh;
            work.fourier[bin] = fourier.magnitude;
        }
        work.evidence[bin] = correlation * fourier.normalized;
        work.phases[bin] = fourier.phase;
    }
}

struct RhythmEstimate {
    double bpm, confidence, phase, secondaryBPM, dominance, entropy, clarity;
};

inline RhythmEstimate chooseRhythm(const RhythmWorkspace& work, double previousBPM,
                                   double priorBPM, double continuity) noexcept {
    RhythmEstimate result {};
    double sum = 0.0, maximum = 0.0, bestScore = 0.0;
    int best = -1;
    for (int bin = 0; bin < work.numBins; ++bin) {
        const double evidence = work.evidence[bin];
        sum += evidence; maximum = std::max(maximum, evidence);
        const double bpm = rhythmBinBPM(work, bin);
        const double reference = previousBPM > 0.0 ? previousBPM : priorBPM;
        const double octaveDistance = reference > 0.0 ? std::log2(bpm / reference) : 0.0;
        const double strength = previousBPM > 0.0 ? std::max(0.0, continuity) : 0.35;
        double weight = std::exp(-strength * octaveDistance * octaveDistance);
        // Explicit resistance to switching to twice/half the established tempo.
        if (previousBPM > 0.0 && std::abs(std::abs(octaveDistance)-1.0) < 0.1) weight *= 0.7;
        const double score = evidence * weight;
        if (score > bestScore) { bestScore = score; best = bin; }
    }
    if (best < 0 || sum <= 1.0e-20 || maximum <= 1.0e-12) return result;
    const double bpm = rhythmBinBPM(work, best);
    double second = 0.0;
    for (int bin = 0; bin < work.numBins; ++bin) {
        const double p = work.evidence[bin] / sum;
        if (p > 0.0) result.entropy -= p * std::log(p);
        if (std::abs(std::log2(rhythmBinBPM(work, bin) / bpm)) > 0.1
            && work.evidence[bin] > second) {
            second = work.evidence[bin]; result.secondaryBPM = rhythmBinBPM(work, bin);
        }
    }
    result.entropy /= std::log(static_cast<double>(work.numBins));
    const double selected = work.evidence[best];
    result.dominance = selected / (selected + second + 1.0e-30);
    result.clarity = clamp(2.0 * maximum * (1.0 - result.entropy), 0.0, 1.0);
    result.confidence = clamp(2.0 * selected * (1.0 - 0.5 * result.entropy)
        * (0.5 + 0.5 * result.dominance), 0.0, 1.0);
    result.bpm = bpm;
    result.phase = work.phases[best];
    return result;
}

struct CausalBeatClock {
    double bpm = 0.0, phase = 0.0, confidence = 0.0, correction = 0.0;
    double sinceActivity = 0.0;
    bool active = false;
    void observe(const RhythmEstimate& estimate, double updateTime,
                 double tempoSmooth, double phaseSmooth, bool lock) noexcept {
        confidence = estimate.confidence;
        if (lock && active) return;
        if (estimate.bpm <= 0.0 || estimate.confidence < 0.12) return;
        if (!active) { bpm = estimate.bpm; phase = estimate.phase; active = true; correction = 0.0; return; }
        const double blend = tempoSmooth > 0.0 ? -std::expm1(-updateTime / tempoSmooth) : 1.0;
        bpm += blend * (estimate.bpm - bpm);
        const double period = 60.0 / bpm;
        correction = clamp(phaseDistance(estimate.phase, phase) / std::max(updateTime, phaseSmooth),
            -0.25 / period, 0.25 / period);
    }
    // Beat and halfway subdivision are one-step impulses; phase never reverses.
    void advance(double seconds, double activity, double phaseSmooth, bool lock,
                 bool& beat, bool& subBeat) noexcept {
        beat = subBeat = false;
        sinceActivity = activity > 1.0e-6 ? 0.0 : sinceActivity + seconds;
        if (!active || bpm <= 0.0) return;
        const double nominal = bpm / 60.0;
        const double delta = seconds * std::max(nominal * 0.5, nominal + (lock ? 0.0 : correction));
        const double before = phase;
        phase = wrapUnit(phase + delta);
        correction *= std::exp(-seconds / std::max(seconds, phaseSmooth));
        const bool audible = lock || (confidence >= 0.12 && sinceActivity < std::max(1.0, 120.0 / bpm));
        beat = audible && before + delta >= 1.0;
        subBeat = audible && (beat || (before < 0.5 && before + delta >= 0.5));
        if (!audible && !lock) confidence = 0.0;
    }
};

struct OnsetWindow {
    double* times = nullptr;
    int capacity = 0, head = 0, count = 0;
    double window = 4.0, minGap = 0.0, last = -1.0e30;
    bool high = false;
    void update(double now, double trigger) noexcept {
        while (count > 0 && times[head] <= now - window) {
            head = (head + 1) % capacity; --count;
        }
        const bool positive = sanitize(trigger) > 0.0;
        if (positive && !high && now - last >= minGap) {
            if (count == capacity) { head = (head + 1) % capacity; --count; }
            times[(head + count) % capacity] = now; ++count; last = now;
        }
        high = positive;
    }
    void statistics(double& rate, double& mean, double& deviation, double& cv) const noexcept {
        rate = count / window; mean = deviation = cv = 0.0;
        if (count < 2) return;
        double sum = 0.0, sumSquares = 0.0;
        for (int index = 1; index < count; ++index) {
            const double interval = times[(head+index)%capacity] - times[(head+index-1)%capacity];
            sum += interval; sumSquares += interval * interval;
        }
        mean = sum / (count - 1);
        deviation = std::sqrt(std::max(0.0, sumSquares / (count - 1) - mean * mean));
        cv = mean > 0.0 ? deviation / mean : 0.0;
    }
};

struct TempoMoment { double weight, weightedLog, weightedLogSquared; };
struct TempoWindow {
    TempoMoment* values = nullptr;
    int size = 0, index = 0, count = 0, accepted = 0;
    double weight = 0.0, sum = 0.0, sumSquared = 0.0;
    double update(double bpm, double confidence, double minimumConfidence) noexcept {
        const TempoMoment old = values[index];
        weight -= old.weight; sum -= old.weightedLog; sumSquared -= old.weightedLogSquared;
        TempoMoment next {};
        if (finite(bpm) && bpm > 0.0 && finite(confidence) && confidence >= minimumConfidence) {
            next.weight = clamp(confidence, 0.0, 1.0);
            const double logTempo = std::log2(bpm);
            next.weightedLog = next.weight * logTempo;
            next.weightedLogSquared = next.weight * logTempo * logTempo;
        }
        accepted += (next.weight > 0.0 ? 1 : 0) - (old.weight > 0.0 ? 1 : 0);
        values[index] = next; index = (index + 1) % size; count = std::min(size, count + 1);
        weight += next.weight; sum += next.weightedLog; sumSquared += next.weightedLogSquared;
        if (weight < 1.0e-8 || accepted < 2) return 0.0;
        const double mean = sum / weight;
        const double deviation = std::sqrt(std::max(0.0, sumSquared / weight - mean * mean));
        return std::exp(-12.0 * deviation);
    }
};

struct TempoDerivative {
    double previousLog = 0.0, result = 0.0;
    bool valid = false;
    double update(double bpm, double confidence, double seconds,
                  double smoothing, double minimumConfidence) noexcept {
        const double alpha = smoothing > 0.0 ? std::exp(-seconds / smoothing) : 0.0;
        double target = 0.0;
        if (finite(bpm) && bpm > 0.0 && finite(confidence) && confidence >= minimumConfidence) {
            const double current = std::log(bpm);
            if (valid) target = 100.0 * (current - previousLog) / seconds;
            previousLog = current; valid = true;
        } else valid = false;
        result = alpha * result + (1.0-alpha) * target;
        return sanitize(result);
    }
};

struct BeatEnergy {
    double sum = 0.0, peak = 0.0, rms = 0.0, completedPeak = 0.0;
    int samples = 0;
    bool armed = false, previousBeat = false;
    void startBlock(double beat) noexcept {
        const bool high = finite(beat) && beat > 0.0;
        if (high && !previousBeat) {
            if (armed && samples > 0) { rms = std::sqrt(sum / samples); completedPeak = peak; }
            sum = peak = 0.0; samples = 0; armed = true;
        }
        previousBeat = high;
    }
    void sample(double input, int maximumSamples) noexcept {
        if (!armed) return;
        if (samples >= maximumSamples) { armed = false; rms = completedPeak = 0.0; return; }
        const double value = sanitize(input);
        sum += value * value; peak = std::max(peak, std::abs(value)); ++samples;
    }
};

} // namespace mir2
