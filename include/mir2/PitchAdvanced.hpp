// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Pitch.hpp"
#include <array>
#include <limits>

namespace mir2 {

constexpr int kPitchCandidates = 16;
constexpr int kPitchGridCapacity = 1024;
constexpr int kContourHistory = 32;
struct PitchCandidate {
    double frequencyHz = 0.0, score = 0.0, probability = 0.0;
    double periodicity = 0.0, harmonicity = 0.0, octavePenalty = 0.0;
};
struct CandidateList {
    std::array<PitchCandidate, kPitchCandidates> values {};
    int count = 0;
    void clear() noexcept { count = 0; values.fill({}); }
    void add(PitchCandidate candidate) noexcept {
        if (!finite(candidate.frequencyHz) || candidate.frequencyHz <= 0.0
            || !finite(candidate.score) || candidate.score <= 0.0) return;
        for (int i = 0; i < count; ++i) {
            if (centsDistance(values[static_cast<std::size_t>(i)].frequencyHz, candidate.frequencyHz) < 12.0) {
                if (candidate.score > values[static_cast<std::size_t>(i)].score) values[static_cast<std::size_t>(i)] = candidate;
                return;
            }
        }
        if (count < kPitchCandidates) values[static_cast<std::size_t>(count++)] = candidate;
        else {
            int weakest = 0;
            for (int i = 1; i < count; ++i)
                if (values[static_cast<std::size_t>(i)].score < values[static_cast<std::size_t>(weakest)].score) weakest = i;
            if (candidate.score > values[static_cast<std::size_t>(weakest)].score) values[static_cast<std::size_t>(weakest)] = candidate;
        }
    }
    void sort() noexcept {
        std::sort(values.begin(), values.begin() + count,
                  [](const PitchCandidate& a, const PitchCandidate& b) { return a.score > b.score; });
    }
    double mass() const noexcept {
        double total = 0.0;
        for (int i = 0; i < count; ++i) total += values[static_cast<std::size_t>(i)].probability;
        return clamp(total, 0.0, 1.0);
    }
    void normalize(double voicedMass = 1.0) noexcept {
        double total = 0.0;
        for (int i = 0; i < count; ++i) total += values[static_cast<std::size_t>(i)].score;
        for (int i = 0; i < count; ++i)
            values[static_cast<std::size_t>(i)].probability = total > 0 ? voicedMass * values[static_cast<std::size_t>(i)].score / total : 0.0;
        sort();
    }
};

// Exact CDF of Beta(2,18), avoiding a special-function dependency.
inline double pitchThresholdCDF(double x) noexcept {
    x = clamp(x, 0.0, 1.0);
    return 1.0 - std::pow(1.0 - x, 18.0) * (1.0 + 18.0 * x);
}
inline CandidateList timePitchCandidates(const PitchWorkspace& w, double sr,
                                         double low, double high, int method,
                                         int thresholds = 32) noexcept {
    CandidateList result;
    int minLag, maxLag;
    lagBounds(sr, low, high, w.frameSize, minLag, maxLag);
    minLag = clamp(minLag, 2, w.frameSize / 2 - 1);
    maxLag = clamp(maxLag, minLag + 1, w.frameSize / 2);
    if (method == 2) {
        // pYIN Stage 1: threshold prior, first trough below threshold, 0.01 fallback mass.
        int selectedLags[128] {};
        double probabilities[128] {};
        int used = 0, absoluteMin = minLag;
        for (int lag = minLag; lag <= maxLag; ++lag)
            if (w.cmndf[lag] < w.cmndf[absoluteMin]) absoluteMin = lag;
        thresholds = clamp(thresholds, 4, 128);
        for (int t = 0; t < thresholds; ++t) {
            const double threshold = (t + 0.5) / thresholds;
            const double prior = pitchThresholdCDF(double(t + 1) / thresholds)
                - pitchThresholdCDF(double(t) / thresholds);
            int selected = -1;
            for (int lag = minLag; lag <= maxLag; ++lag) {
                if (w.cmndf[lag] <= w.cmndf[lag - 1]
                    && w.cmndf[lag] < w.cmndf[lag + 1] && w.cmndf[lag] < threshold) {
                    selected = lag; break;
                }
            }
            const double probability = prior * (selected >= 0 ? 1.0 : 0.01);
            if (selected < 0) selected = absoluteMin;
            int index = 0;
            while (index < used && selectedLags[index] != selected) ++index;
            if (index == used) selectedLags[used++] = selected;
            probabilities[index] += probability;
        }
        for (int i = 0; i < used; ++i) {
            const int lag = selectedLags[i];
            const double hz = refinedFrequency(w.cmndf, lag, sr, false);
            if (hz >= low && hz <= high)
                result.add({hz, probabilities[i], probabilities[i],
                            clamp(1.0 - w.cmndf[lag], 0.0, 1.0), 0.0, 0.0});
        }
        result.sort();
    } else {
        const double* curve = method == 1 ? w.nsdf : w.cmndf;
        double best = 0.0;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            const bool extremum = method == 1
                ? curve[lag] >= curve[lag - 1] && curve[lag] > curve[lag + 1]
                : curve[lag] <= curve[lag - 1] && curve[lag] < curve[lag + 1];
            const double score = clamp(method == 1 ? curve[lag] : 1.0 - curve[lag], 0.0, 1.0);
            if (!extremum || score < 0.05) continue;
            const double hz = refinedFrequency(curve, lag, sr, method == 1);
            if (hz < low || hz > high) continue;
            result.add({hz, score * score, 0.0, score, 0.0, 0.0});
            best = std::max(best, score);
        }
        result.normalize(best);
    }
    return result;
}
struct PitchUncertainty { double entropy = 0.0, octave = 0.0, competitor = 0.0; };
inline PitchUncertainty pitchUncertainty(const CandidateList& list) noexcept {
    PitchUncertainty r;
    if (list.count < 2) return r;
    double mass = 0.0;
    for (int i = 0; i < list.count; ++i) mass += list.values[static_cast<std::size_t>(i)].probability;
    if (mass <= 0.0) return r;
    double competitor = 0.0;
    for (int i = 0; i < list.count; ++i) {
        const auto& c = list.values[static_cast<std::size_t>(i)];
        const double p = c.probability / mass;
        if (p > 0.0) r.entropy -= p * std::log(p);
        if (i > 0 && octaveRelated(c.frequencyHz, list.values[0].frequencyHz)
            && c.probability > competitor) {
            competitor = c.probability; r.competitor = c.frequencyHz;
        }
    }
    r.entropy /= std::log(double(list.count));
    r.octave = clamp(competitor / std::max(1.0e-20, list.values[0].probability), 0.0, 1.0);
    return r;
}

inline bool primeHarmonic(int n) noexcept {
    if (n == 1) return true;
    if (n < 2) return false;
    for (int d = 2; d * d <= n; ++d) if (n % d == 0) return false;
    return true;
}
inline double pitchERB(double hz) noexcept { return 21.4 * std::log10(1.0 + hz / 229.0); }
inline double pitchERBInverse(double erb) noexcept { return 229.0 * (std::pow(10.0, erb / 21.4) - 1.0); }
inline double swipePrimeKernel(double frequency, double candidate) noexcept {
    if (frequency <= 0.0 || candidate <= 0.0) return 0.0;
    const double q = frequency / candidate;
    const int first = std::max(1, int(std::floor(q - 0.75)));
    const int last = int(std::ceil(q + 0.75));
    double value = 0.0;
    for (int h = first; h <= last; ++h) {
        if (!primeHarmonic(h)) continue;
        const double distance = std::abs(q - h);
        if (distance < 0.25) value += std::cos(kTwoPi * q);
        else if (distance < 0.75) value += 0.5 * std::cos(kTwoPi * q);
    }
    return value / std::sqrt(frequency);
}
struct SwipeWorkspace {
    Complex* fftData = nullptr;
    double* frequencies = nullptr; // gridSize
    double* erbFrequencies = nullptr; // erbSize
    double* kernels = nullptr; // gridSize * erbSize, prepared once
    double* loudness = nullptr; // windows * erbSize
    double* salience = nullptr; // gridSize
    int frameSize = 0, gridSize = 0, erbSize = 0, windows = 0, binsPerOctave = 60;
    double sampleRate = 48000.0;
};
inline int pitchGridSize(double low, double high, int bpo) noexcept {
    if (!finite(low) || !finite(high) || low <= 0.0 || high <= low || bpo <= 0) return 0;
    return clamp(int(std::floor(std::log2(high / low) * bpo)) + 1, 2, kPitchGridCapacity);
}
inline void initializeSwipe(SwipeWorkspace& w, double low) noexcept {
    w.windows = 1;
    for (int n = 256; n < w.frameSize; n *= 2) ++w.windows;
    for (int j = 0; j < w.erbSize; ++j) w.erbFrequencies[j] = pitchERBInverse((j + 1) * 0.1);
    for (int i = 0; i < w.gridSize; ++i) {
        w.frequencies[i] = low * std::exp2(double(i) / w.binsPerOctave);
        double norm = 0.0;
        for (int j = 0; j < w.erbSize; ++j) {
            const double value = swipePrimeKernel(w.erbFrequencies[j], w.frequencies[i]);
            w.kernels[i * w.erbSize + j] = value;
            if (value > 0.0) norm += value * value;
        }
        norm = std::sqrt(std::max(norm, 1.0e-30));
        for (int j = 0; j < w.erbSize; ++j) w.kernels[i * w.erbSize + j] /= norm;
    }
}
inline void analyzeSwipe(const float* frame, SwipeWorkspace& w) noexcept {
    double mean = 0.0;
    for (int i = 0; i < w.frameSize; ++i) mean += sanitize(frame[i]);
    mean /= w.frameSize;
    for (int window = 0, n = 256; window < w.windows; ++window, n *= 2) {
        // All windows have the same centre. The longest frame sets causal latency.
        const int start = (w.frameSize - n) / 2;
        for (int i = 0; i < n; ++i)
            w.fftData[i] = {(sanitize(frame[start + i]) - mean)
                * (0.5 - 0.5 * std::cos(kTwoPi * i / (n - 1))), 0.0};
        std::fill(w.fftData + n, w.fftData + 2 * n, Complex{0.0, 0.0});
        fft(w.fftData, 2 * n, false);
        for (int j = 0; j < w.erbSize; ++j) {
            const double bin = w.erbFrequencies[j] * (2 * n) / w.sampleRate;
            const int b = clamp(int(bin), 0, n - 1);
            const double fraction = clamp(bin - b, 0.0, 1.0);
            const double a = std::hypot(w.fftData[b].real, w.fftData[b].imag);
            const double c = std::hypot(w.fftData[b + 1].real, w.fftData[b + 1].imag);
            w.loudness[window * w.erbSize + j] = std::sqrt(a + fraction * (c - a));
        }
    }
    for (int i = 0; i < w.gridSize; ++i) {
        const double logWindow = clamp(std::log2(8.0 * w.sampleRate / w.frequencies[i]) - 8.0,
                                       0.0, double(w.windows - 1));
        const int first = int(logWindow), second = std::min(first + 1, w.windows - 1);
        const double blend = logWindow - first;
        double scores[2] {};
        for (int which = 0; which < 2; ++which) {
            const double* loud = w.loudness + (which == 0 ? first : second) * w.erbSize;
            double norm = 0.0, score = 0.0;
            for (int j = 0; j < w.erbSize; ++j) {
                if (w.erbFrequencies[j] >= w.frequencies[i] * 0.25) norm += loud[j] * loud[j];
                score += loud[j] * w.kernels[i * w.erbSize + j];
            }
            scores[which] = norm > 1.0e-20 ? score / std::sqrt(norm) : 0.0;
        }
        w.salience[i] = clamp(scores[0] + blend * (scores[1] - scores[0]), 0.0, 1.0);
    }
}
inline CandidateList salienceCandidates(const double* salience, const double* frequencies,
                                         int size, int bpo, double minimum = 0.01) noexcept {
    CandidateList result;
    for (int i = 0; i < size; ++i) {
        const double s = sanitize(salience[i]);
        if (s < minimum || (i > 0 && s < salience[i - 1])
            || (i + 1 < size && s <= salience[i + 1])) continue;
        const double offset = i > 0 && i + 1 < size
            ? parabolicOffset(salience[i - 1], s, salience[i + 1]) : 0.0;
        result.add({frequencies[i] * std::exp2(offset / bpo), s, 0.0, 0.0, s, 0.0});
    }
    result.sort();
    result.normalize(result.count ? clamp(result.values[0].score, 0.0, 1.0) : 0.0);
    return result;
}

inline void pitchMagnitude(const float* frame, int n, Complex* scratch, double* magnitudes) noexcept {
    double mean = 0.0;
    for (int i = 0; i < n; ++i) mean += sanitize(frame[i]);
    mean /= n;
    for (int i = 0; i < n; ++i)
        scratch[i] = {(sanitize(frame[i]) - mean) * (0.5 - 0.5 * std::cos(kTwoPi * i / (n - 1))), 0.0};
    std::fill(scratch + n, scratch + 2 * n, Complex{0.0, 0.0});
    fft(scratch, 2 * n, false);
    for (int i = 0; i <= n; ++i) magnitudes[i] = std::hypot(scratch[i].real, scratch[i].imag);
}
inline void whitenPitchSpectrum(const double* magnitude, double* white, int bins, double binHz) noexcept {
    double centers[32], gains[30];
    double maximum = 0.0;
    for (int i = 0; i < bins; ++i) maximum = std::max(maximum, magnitude[i]);
    if (maximum <= 1.0e-10) { std::fill(white, white + bins, 0.0); return; }
    for (int b = 0; b < 32; ++b) centers[b] = pitchERBInverse(double(b + 1));
    for (int b = 1; b <= 30; ++b) {
        double power = 0.0;
        const int first = clamp(int(centers[b - 1] / binHz), 0, bins - 1);
        const int last = clamp(int(std::ceil(centers[b + 1] / binHz)), 0, bins - 1);
        for (int k = first; k <= last; ++k) {
            const double f = k * binHz;
            const double weight = f <= centers[b]
                ? (f - centers[b - 1]) / (centers[b] - centers[b - 1])
                : (centers[b + 1] - f) / (centers[b + 1] - centers[b]);
            power += std::max(0.0, weight) * magnitude[k] * magnitude[k];
        }
        const double sigma = std::sqrt(power / (2 * (bins - 1)));
        gains[b - 1] = std::pow(std::max(sigma, maximum * 1.0e-4), -0.67);
    }
    int band = 1;
    for (int k = 0; k < bins; ++k) {
        const double hz = k * binHz;
        while (band < 29 && hz > centers[band + 1]) ++band;
        const double t = clamp((hz - centers[band]) / (centers[band + 1] - centers[band]), 0.0, 1.0);
        white[k] = magnitude[k] * (gains[band - 1] + t * (gains[band] - gains[band - 1]));
    }
}
inline double harmonicPitchScore(const double* spectrum, int bins, double binHz,
                                 double f0, double alpha = 52.0) noexcept {
    double sum = 0.0;
    const int harmonics = std::min(80, int(std::min(10000.0, (bins - 1) * binHz) / f0));
    for (int h = 1; h <= harmonics; ++h) {
        const double position = h * f0 / binHz;
        const int middle = clamp(int(std::lround(position)), 1, bins - 2);
        // Interpolated peak amplitude: 1/2-bin search uncertainty with 2x padding.
        double amplitude = spectrum[middle];
        if (std::abs(position - middle) > 0.25) {
            const int neighbor = middle + (position > middle ? 1 : -1);
            amplitude = std::max(amplitude, spectrum[neighbor]);
        }
        sum += (f0 + alpha) / (h * f0 + 320.0) * amplitude;
    }
    return sum;
}
inline void harmonicPitchSalience(const double* spectrum, int bins, double binHz,
                                  const double* frequencies, int count, double* salience) noexcept {
    for (int i = 0; i < count; ++i)
        salience[i] = harmonicPitchScore(spectrum, bins, binHz, frequencies[i]);
}
inline double pitchSpectralTonality(const double* spectrum, int bins, double binHz) noexcept {
    const int first = clamp(int(80.0 / binHz), 1, bins - 1);
    const int last = clamp(int(10000.0 / binHz), first, bins - 1);
    double sum = 0.0, logSum = 0.0;
    for (int i = first; i <= last; ++i) sum += std::max(0.0, spectrum[i]);
    if (sum <= 1.0e-20) return 0.0;
    const double mean = sum / (last - first + 1);
    for (int i = first; i <= last; ++i) logSum += std::log(std::max(mean * 1.0e-8, spectrum[i]));
    const double flatness = std::exp(logSum / (last - first + 1)) / mean;
    return clamp((0.8 - flatness) / 0.4, 0.0, 1.0);
}
inline CandidateList multiPitch(const double* white, double* residual, int bins,
                                double binHz, const double* frequencies, int grid,
                                int bpo, double* salience, int maximum,
                                double threshold = 0.15, double cancellation = 0.9) noexcept {
    CandidateList output;
    if (pitchSpectralTonality(white, bins, binHz) < 0.1) return output;
    std::copy(white, white + bins, residual);
    double total = 0.0, bestObjective = 0.0, firstScore = 0.0;
    maximum = clamp(maximum, 1, 6);
    for (int voice = 0; voice < maximum; ++voice) {
        harmonicPitchSalience(residual, bins, binHz, frequencies, grid, salience);
        for (int i = 0; i < grid; ++i)
            for (int j = 0; j < output.count; ++j)
                if (centsDistance(frequencies[i], output.values[static_cast<std::size_t>(j)].frequencyHz) < 70.0) salience[i] = 0.0;
        int peak = 0;
        for (int i = 1; i < grid; ++i) if (salience[i] > salience[peak]) peak = i;
        const double score = salience[peak];
        if (voice == 0) firstScore = score;
        if (score <= 1.0e-10 || score < firstScore * threshold) break;
        const double objective = (total + score) / std::pow(double(voice + 1), 0.7);
        if (voice > 0 && objective <= bestObjective) break;
        total += score; bestObjective = objective;
        const double offset = peak > 0 && peak + 1 < grid
            ? parabolicOffset(salience[peak - 1], score, salience[peak + 1]) : 0.0;
        double f0 = frequencies[peak] * std::exp2(offset / bpo);
        // Refine from the fundamental lobe where present; otherwise retain grid interpolation.
        const int fundamental = clamp(int(std::lround(f0 / binHz)), 1, bins - 2);
        if (residual[fundamental] > residual[fundamental - 1]
            && residual[fundamental] > residual[fundamental + 1]) {
            const double refined = (fundamental + parabolicOffset(std::log(std::max(1.0e-20, residual[fundamental - 1])),
                std::log(std::max(1.0e-20, residual[fundamental])),
                std::log(std::max(1.0e-20, residual[fundamental + 1])))) * binHz;
            if (centsDistance(f0, refined) < 40.0) f0 = refined;
        }
        output.add({f0, score, clamp(score / firstScore, 0.0, 1.0), 0.0, score, 0.0});
        const int harmonics = std::min(80, int((bins - 2) * binHz / f0));
        for (int h = 1; h <= harmonics; ++h) {
            const int center = clamp(int(std::lround(h * f0 / binHz)), 1, bins - 2);
            const double weight = clamp(cancellation * (f0 + 52.0) / (h * f0 + 320.0), 0.0, 1.0);
            // Full Hann main-lobe neighborhood (four bins on the 2x-padded FFT).
            for (int k = std::max(1, center - 4); k <= std::min(bins - 1, center + 4); ++k)
                residual[k] = std::max(0.0, residual[k] - weight * white[k]);
        }
    }
    return output;
}

struct ContourResult { double frequency = 0.0, confidence = 0.0, jump = 0.0; };
struct PitchTracker {
    double costs[kPitchCandidates + 1] {};
    double previousHz[kPitchCandidates + 1] {};
    double historyHz[kContourHistory][kPitchCandidates + 1] {};
    double historyProbability[kContourHistory][kPitchCandidates + 1] {};
    int parent[kContourHistory][kPitchCandidates + 1] {};
    int previousCount = 0, steps = 0, index = 0;
    double lastOutput = 0.0;
    void reset() noexcept { *this = PitchTracker{}; }
    ContourResult process(const CandidateList& candidates, double maxJump = 600.0,
                          double transitionWeight = 1.0, double octavePenalty = 1.0,
                          double unvoicedPenalty = 2.0, int lag = 0) noexcept {
        double next[kPitchCandidates + 1] {}, hz[kPitchCandidates + 1] {};
        const int states = candidates.count + 1;
        double sum = 0.0;
        for (int c = 0; c < candidates.count; ++c) sum += candidates.values[static_cast<std::size_t>(c)].probability;
        sum = clamp(sum, 0.0, 1.0);
        int bestState = 0;
        double bestCost = std::numeric_limits<double>::infinity();
        for (int c = 0; c < states; ++c) {
            hz[c] = c < candidates.count ? candidates.values[static_cast<std::size_t>(c)].frequencyHz : 0.0;
            const double probability = c < candidates.count ? candidates.values[static_cast<std::size_t>(c)].probability : 1.0 - sum;
            double cost = 0.0;
            int bestParent = 0;
            if (steps > 0) {
                cost = std::numeric_limits<double>::infinity();
                for (int p = 0; p < previousCount; ++p) {
                    double transition = 0.0;
                    if ((hz[c] > 0.0) != (previousHz[p] > 0.0)) transition = unvoicedPenalty;
                    else if (hz[c] > 0.0) {
                        const double distance = centsDistance(hz[c], previousHz[p]);
                        transition = transitionWeight * distance / std::max(1.0, maxJump);
                        if (distance > maxJump) transition += 8.0;
                        if (octaveRelated(hz[c], previousHz[p])) transition += octavePenalty;
                    }
                    if (costs[p] + transition < cost) { cost = costs[p] + transition; bestParent = p; }
                }
            }
            next[c] = cost - std::log(std::max(1.0e-12, probability));
            parent[index][c] = bestParent;
            historyHz[index][c] = hz[c];
            historyProbability[index][c] = probability;
            if (next[c] < bestCost) { bestCost = next[c]; bestState = c; }
        }
        for (int c = 0; c < states; ++c) { costs[c] = next[c] - bestCost; previousHz[c] = hz[c]; }
        previousCount = states;
        const int safeLag = clamp(lag, 0, kContourHistory - 1);
        int row = index, state = bestState;
        for (int i = 0; i < std::min(steps, safeLag); ++i) {
            state = parent[row][state]; row = (row + kContourHistory - 1) % kContourHistory;
        }
        ContourResult result;
        if (steps >= safeLag) {
            result.frequency = historyHz[row][state];
            result.confidence = result.frequency > 0.0 ? historyProbability[row][state] : 0.0;
            result.jump = result.frequency > 0.0 && lastOutput > 0.0
                ? 1200.0 * std::log2(result.frequency / lastOutput) : 0.0;
            lastOutput = result.frequency;
        }
        index = (index + 1) % kContourHistory; ++steps;
        if (steps > 1000000) steps = kContourHistory;
        return result;
    }
};

struct ConsensusResult { double frequency = 0.0, agreement = 0.0, spread = 0.0, octaveConflict = 0.0; };
inline ConsensusResult pitchConsensus(const double* frequencies, const double* confidence, int count,
                                      double tolerance = 50.0) noexcept {
    ConsensusResult r;
    int best = -1;
    double bestWeight = 0.0, total = 0.0;
    for (int i = 0; i < count; ++i) {
        if (frequencies[i] <= 0.0 || confidence[i] <= 0.0) continue;
        total += confidence[i];
        double weight = 0.0;
        for (int j = 0; j < count; ++j)
            if (frequencies[j] > 0.0 && centsDistance(frequencies[i], frequencies[j]) <= tolerance)
                weight += confidence[j];
        if (weight > bestWeight) { best = i; bestWeight = weight; }
    }
    if (best < 0 || total <= 0.0) return r;
    double mean = 0.0;
    for (int i = 0; i < count; ++i)
        if (frequencies[i] > 0.0 && centsDistance(frequencies[i], frequencies[best]) <= tolerance)
            mean += confidence[i] * std::log2(frequencies[i]);
    r.frequency = std::exp2(mean / bestWeight);
    r.agreement = bestWeight / total;
    double variance = 0.0, octaves = 0.0;
    for (int i = 0; i < count; ++i) if (frequencies[i] > 0.0) {
        const double cents = centsDistance(frequencies[i], r.frequency);
        variance += confidence[i] * cents * cents;
        if (octaveRelated(frequencies[i], r.frequency)) octaves += confidence[i];
    }
    r.spread = std::sqrt(variance / total); r.octaveConflict = octaves / total;
    return r;
}

struct MelodyTrack {
    double hz = 0.0, meanCents = 0.0, variance = 0.0, salience = 0.0;
    double duration = 0.0, gap = 0.0, continuity = 0.0;
    bool matched = false;
};
struct MelodyTracker {
    MelodyTrack tracks[kPitchCandidates] {};
    int selected = -1;
    ContourResult process(const CandidateList& peaks, double dt,
                          double minDuration = 0.06, double maxGap = 0.08) noexcept {
        for (auto& track : tracks) { track.matched = false; track.gap += dt; }
        for (int c = 0; c < peaks.count; ++c) {
            const auto& candidate = peaks.values[static_cast<std::size_t>(c)];
            int best = -1; double distance = 80.0 + 2000.0 * dt;
            for (int t = 0; t < kPitchCandidates; ++t) {
                if (tracks[t].hz <= 0.0 || tracks[t].matched || tracks[t].gap > maxGap) continue;
                const double d = centsDistance(tracks[t].hz, candidate.frequencyHz);
                if (d < distance) { distance = d; best = t; }
            }
            if (best < 0) {
                for (int t = 0; t < kPitchCandidates; ++t)
                    if (tracks[t].hz <= 0.0 || tracks[t].gap > maxGap) { best = t; break; }
                if (best < 0) continue;
                tracks[best] = {}; tracks[best].meanCents = 1200.0 * std::log2(candidate.frequencyHz);
            }
            auto& track = tracks[best];
            const double alpha = dt / (0.25 + dt);
            const double delta = 1200.0 * std::log2(candidate.frequencyHz) - track.meanCents;
            track.meanCents += alpha * delta;
            track.variance = (1.0 - alpha) * (track.variance + alpha * delta * delta);
            track.salience = track.duration <= 0.0 ? candidate.score
                : (1.0 - alpha) * track.salience + alpha * candidate.score;
            track.continuity = track.duration <= 0.0 ? 1.0
                : (1.0 - alpha) * track.continuity + alpha * std::exp(-distance / 200.0);
            track.hz = candidate.frequencyHz; track.duration += dt; track.gap = 0.0; track.matched = true;
        }
        int best = -1; double bestScore = 0.0, total = 0.0;
        for (int t = 0; t < kPitchCandidates; ++t) {
            auto& track = tracks[t];
            if (track.gap > maxGap) track = {};
            if (!track.matched || track.duration < minDuration) continue;
            const double maturity = std::min(1.0, track.duration / 0.25);
            const double score = track.salience * (0.5 + 0.5 * maturity)
                * (0.5 + 0.5 * track.continuity) * (t == selected ? 1.15 : 1.0);
            total += score;
            if (score > bestScore) { bestScore = score; best = t; }
        }
        selected = best;
        return best < 0 ? ContourResult{} : ContourResult{tracks[best].hz,
            clamp(bestScore / std::max(1.0e-20, total), 0.0, 1.0), 0.0};
    }
};

inline double pitchDeviation(double hz, double reference, int mode, double previous = 0.0) noexcept {
    if (!finite(hz) || hz <= 0.0 || !finite(reference) || reference <= 0.0) return 0.0;
    if (mode == 2) reference = previous;
    if (reference <= 0.0) return 0.0;
    double cents = 1200.0 * std::log2(hz / reference);
    if (mode == 1) cents -= 100.0 * std::round(cents / 100.0);
    return cents;
}
struct StabilityResult { double stability = 0.0, deviation = 0.0; };
inline StabilityResult pitchStability(const double* cents, const double* weights, int count) noexcept {
    double weight = 0.0, sum = 0.0, square = 0.0;
    for (int i = 0; i < count; ++i) {
        weight += weights[i]; sum += weights[i] * cents[i]; square += weights[i] * cents[i] * cents[i];
    }
    if (weight <= 1.0e-12) return {};
    const double deviation = std::sqrt(std::max(0.0, square / weight - (sum / weight) * (sum / weight)));
    return {std::exp(-deviation / 50.0), deviation};
}
struct VibratoResult { double present = 0.0, rate = 0.0, extent = 0.0, confidence = 0.0; };
inline VibratoResult analyzeVibrato(const double* cents, const double* weights, int n,
                                   double sampleRate, double minRate, double maxRate,
                                   double minExtent) noexcept {
    VibratoResult r;
    if (n < 8 || sampleRate <= 0.0) return r;
    double sw = 0.0, sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (int i = 0; i < n; ++i) {
        const double t = (i - (n - 1) * 0.5) / sampleRate, w = weights[i];
        sw += w; sx += w * t; sy += w * cents[i]; sxx += w * t * t; sxy += w * t * cents[i];
    }
    if (sw < 0.8 * n) return r;
    const double denominator = sw * sxx - sx * sx;
    const double slope = denominator > 1.0e-20 ? (sw * sxy - sx * sy) / denominator : 0.0;
    const double mean = (sy - slope * sx) / sw;
    double energy = 0.0;
    for (int i = 0; i < n; ++i) {
        const double residual = cents[i] - mean - slope * (i - (n - 1) * 0.5) / sampleRate;
        energy += weights[i] * residual * residual;
    }
    if (energy <= 1.0e-12) return r;
    minRate = clamp(minRate, 0.5, sampleRate * 0.4);
    maxRate = clamp(maxRate, minRate, sampleRate * 0.45);
    double best = 0.0;
    for (double f = minRate; f <= maxRate + 0.0001; f += 0.05) {
        double cc = 0.0, ss = 0.0, cs = 0.0, yc = 0.0, ys = 0.0;
        const double angle = kTwoPi * f / sampleRate;
        const double cosine = std::cos(angle), sine = std::sin(angle);
        double c = 1.0, s = 0.0;
        for (int i = 0; i < n; ++i) {
            const double y = cents[i] - mean - slope * (i - (n - 1) * 0.5) / sampleRate;
            const double w = weights[i];
            cc += w * c * c; ss += w * s * s; cs += w * c * s; yc += w * y * c; ys += w * y * s;
            const double next = c * cosine - s * sine; s = s * cosine + c * sine; c = next;
        }
        const double det = cc * ss - cs * cs;
        if (det <= 1.0e-20) continue;
        const double a = (yc * ss - ys * cs) / det, b = (ys * cc - yc * cs) / det;
        const double fit = a * yc + b * ys;
        if (fit > best) { best = fit; r.rate = f; r.extent = 2.0 * std::hypot(a, b); }
    }
    r.confidence = clamp(best / energy, 0.0, 1.0) * clamp(sw / n, 0.0, 1.0);
    r.present = r.confidence >= 0.6 && r.extent >= minExtent && n / sampleRate >= 2.0 / minRate ? 1.0 : 0.0;
    if (r.present == 0.0) { r.rate = 0.0; r.extent = 0.0; }
    return r;
}
} // namespace mir2
