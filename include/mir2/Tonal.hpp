// SPDX-License-Identifier: GPL-3.0-or-later
// Paper equations and profile-data provenance: docs/tonal-harmonic-implementation.md.
#pragma once
#include "SpectralAdvanced.hpp"
#include <array>

namespace mir2 {
constexpr int kMaxChromaBins = 120;
inline double positiveModulo(double x, double modulus) noexcept {
    const double y = std::fmod(x, modulus); return y < 0.0 ? y + modulus : y;
}
inline void normalizeChroma(double* values, int size, int mode) noexcept {
    double denominator = 0.0;
    for (int i = 0; i < size; ++i) {
        values[i] = std::max(0.0, sanitize(values[i]));
        if (mode == 1) denominator = std::max(denominator, values[i]);
        else denominator += mode == 3 ? values[i] * values[i] : values[i];
    }
    if (mode == 3) denominator = std::sqrt(denominator);
    if (mode != 0 && denominator > 1.0e-30)
        for (int i = 0; i < size; ++i) values[i] /= denominator;
}
inline void addPitchClass(double* output, int size, double hz, double weight,
                          double tuning, int weighting, double windowSemitones) noexcept {
    if (hz <= 0.0 || tuning <= 0.0 || !finite(hz) || !finite(tuning) || weight <= 0.0) return;
    const double pitchClass = positiveModulo(9.0 + 12.0 * std::log2(hz / tuning), 12.0);
    if (weighting == 0) {
        const int bin = int(std::lround(pitchClass * size / 12.0)) % size;
        output[bin] += weight; return;
    }
    const double halfWindow = clamp(windowSemitones, 12.0 / size, 12.0) * 0.5;
    for (int bin = 0; bin < size; ++bin) {
        double delta = std::abs(pitchClass - double(bin) * 12.0 / size);
        delta = std::min(delta, 12.0 - delta);
        if (delta >= halfWindow) continue;
        const double w = std::cos(kPi * delta / (2.0 * halfWindow));
        output[bin] += weight * (weighting == 2 ? w * w : w);
    }
}
inline void harmonicPitchClassProfile(const SpectralPeak* peaks, int count, double* output,
                                      int size, double tuning = 440.0, int harmonics = 8,
                                      int weighting = 2, int normalization = 1,
                                      double windowSemitones = 4.0 / 3.0) noexcept {
    std::fill(output, output + size, 0.0);
    harmonics = clamp(harmonics, 1, 16);
    for (int p = 0; p < count; ++p) {
        double harmonicWeight = 1.0;
        for (int h = 1; h <= harmonics; ++h) {
            addPitchClass(output, size, peaks[p].frequency / h,
                peaks[p].magnitude * peaks[p].magnitude * harmonicWeight,
                tuning, weighting, windowSemitones);
            harmonicWeight *= 0.6;
        }
    }
    normalizeChroma(output, size, normalization);
}
inline void fftChromagram(const double* magnitude, int bins, double binHz, double* output,
                          int size, double tuning, double low, double high, int normalization) noexcept {
    std::fill(output, output + size, 0.0);
    int first, last;
    if (!spectralRange(bins, binHz, std::max(binHz, low), high, first, last) || tuning <= 0.0) return;
    for (int k = first; k <= last; ++k) {
        const double position = positiveModulo((9.0 + 12.0 * std::log2(k * binHz / tuning)) * size / 12.0, double(size));
        const int left = int(position);
        const double fraction = position - left, power = magnitude[k] * magnitude[k];
        output[left] += power * (1.0 - fraction); output[(left + 1) % size] += power * fraction;
    }
    normalizeChroma(output, size, normalization);
}
struct TuningResult { double frequency = 0.0, cents = 0.0, confidence = 0.0; };
inline TuningResult tuningFromPeaks(const SpectralPeak* peaks, int count) noexcept {
    double x = 0.0, y = 0.0, weight = 0.0;
    for (int p = 0; p < count; ++p) {
        if (peaks[p].frequency <= 0.0) continue;
        const double semitones = 12.0 * std::log2(peaks[p].frequency / 440.0);
        const double angle = kTwoPi * (semitones - std::round(semitones));
        const double w = peaks[p].magnitude * peaks[p].magnitude;
        x += w * std::cos(angle); y += w * std::sin(angle); weight += w;
    }
    if (weight <= 1.0e-30) return {};
    const double concentration = std::hypot(x, y) / weight;
    if (concentration <= 1.0e-12) return {};
    const double cents = 100.0 * std::atan2(y, x) / kTwoPi;
    return {440.0 * std::exp2(cents / 1200.0), cents, clamp(concentration, 0.0, 1.0)};
}
inline void foldChroma(const double* input, int size, double* output) noexcept {
    std::fill(output, output + 12, 0.0);
    for (int i = 0; i < size; ++i) {
        const int pc = int(std::lround(double(i) * 12.0 / size)) % 12;
        output[pc] += std::max(0.0, sanitize(input[i]));
    }
}
inline void quantizeCENS(const double* input, double* output) noexcept {
    double normalized[12]; std::copy(input, input + 12, normalized); normalizeChroma(normalized, 12, 2);
    constexpr double boundaries[] {0.05, 0.1, 0.2, 0.4};
    for (int i = 0; i < 12; ++i) {
        output[i] = 0.0;
        for (double threshold : boundaries) if (normalized[i] > threshold) output[i] += 0.25;
    }
}
inline void tonalCentroid(const double* chroma, double* output) noexcept {
    double total = 0.0; std::fill(output, output + 6, 0.0);
    constexpr double angular[] {7.0 * kPi / 6.0, 3.0 * kPi / 2.0, 2.0 * kPi / 3.0};
    for (int pc = 0; pc < 12; ++pc) {
        const double value = std::max(0.0, sanitize(chroma[pc])); total += value;
        for (int circle = 0; circle < 3; ++circle) {
            const double radius = circle == 2 ? 0.5 : 1.0;
            output[2 * circle] += value * radius * std::sin(angular[circle] * pc);
            output[2 * circle + 1] += value * radius * std::cos(angular[circle] * pc);
        }
    }
    if (total > 1.0e-30) for (int i = 0; i < 6; ++i) output[i] /= total;
}
inline double chromaEntropy(const double* chroma, int size) noexcept {
    double sum = 0.0, entropy = 0.0;
    for (int i = 0; i < size; ++i) sum += std::max(0.0, sanitize(chroma[i]));
    if (sum <= 1.0e-30 || size < 2) return 0.0;
    for (int i = 0; i < size; ++i) {
        const double p = std::max(0.0, sanitize(chroma[i])) / sum;
        if (p > 0.0) entropy -= p * std::log(p);
    }
    return entropy / std::log(double(size));
}
inline double chromaCrest(const double* chroma, int size) noexcept {
    double sum = 0.0, maximum = 0.0;
    for (int i = 0; i < size; ++i) { const double x = std::max(0.0, sanitize(chroma[i])); sum += x; maximum = std::max(maximum,x); }
    return sum > 1.0e-30 ? size * maximum / sum : 0.0;
}
// Numeric research profiles only; independently implemented correlation below.
// Krumhansl/Kessler (1982), Temperley (1999), Faraldo et al. EDMA ECIR (2016).
inline const double* keyProfile(int profile, int mode) noexcept {
    static constexpr double profiles[3][2][12] {
        {{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88},
         {6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17}},
        {{5,2,3.5,2,4.5,4,2,4.5,2,3.5,1.5,4}, {5,2,3.5,4.5,2,4,2,4.5,3.5,2,1.5,4}},
        {{0.16519551,0.04749026,0.08293076,0.06687112,0.09994645,0.09274123,0.05294487,0.13159476,0.05218986,0.07443653,0.06940723,0.06425150},
         {0.17235348,0.05336489,0.07610090,0.10043649,0.05621498,0.08527853,0.04979150,0.13451001,0.07458916,0.05003023,0.09187879,0.05545106}}
    };
    return profiles[clamp(profile, 0, 2)][clamp(mode, 0, 1)];
}
struct KeyResult { int root = -1, mode = -1; double strength = 0.0, margin = 0.0; };
inline KeyResult estimateKey(const double* chroma, int profile, double threshold = 0.0) noexcept {
    KeyResult result;
    double mean = 0.0, variance = 0.0;
    for (int i=0;i<12;++i) mean += std::max(0.0,sanitize(chroma[i]));
    mean /= 12;
    for (int i=0;i<12;++i) { const double x=std::max(0.0,sanitize(chroma[i]))-mean; variance+=x*x; }
    if (variance <= 1.0e-20) return result;
    double best=-2, second=-2;
    for(int mode=0;mode<2;++mode) {
        const double* values=keyProfile(profile,mode); double m=0,v=0;
        for(int i=0;i<12;++i) m+=values[i];
        m/=12;
        for(int i=0;i<12;++i) v+=(values[i]-m)*(values[i]-m);
        for(int root=0;root<12;++root) {
            double cross=0;
            for(int i=0;i<12;++i) cross+=(chroma[i]-mean)*(values[(i-root+12)%12]-m);
            const double score=cross/std::sqrt(variance*v);
            if(score>best) { second=best;best=score;result.root=root;result.mode=mode; }
            else second=std::max(second,score);
        }
    }
    result.strength=clamp(best,0.0,1.0); result.margin=clamp(best-second,0.0,1.0);
    if(best<threshold) result.root=result.mode=-1;
    return result;
}
// Quality order is stable: maj,min,dim,aug,7,maj7,min7,sus2,sus4.
constexpr int kChordQualities = 9;
constexpr int kChordStates = 12 * kChordQualities + 1;
inline int chordIntervals(int quality, const int*& intervals) noexcept {
    static constexpr int patterns[kChordQualities][4] {
        {0,4,7,0}, {0,3,7,0}, {0,3,6,0}, {0,4,8,0},
        {0,4,7,10}, {0,4,7,11}, {0,3,7,10}, {0,2,7,0}, {0,5,7,0}
    };
    if (quality < 0 || quality >= kChordQualities) { intervals = nullptr; return 0; }
    intervals = patterns[quality];
    return quality >= 4 && quality <= 6 ? 4 : 3;
}
inline double chordSimilarity(const double* chroma, int root, int quality) noexcept {
    const int* intervals = nullptr;
    const int tones = chordIntervals(quality, intervals);
    if (tones == 0 || root < 0 || root > 11) return 0.0;
    double energy = 0.0, total = 0.0;
    for (int i = 0; i < 12; ++i) {
        const double x = std::max(0.0, sanitize(chroma[i])); energy += x*x; total += x;
    }
    if (energy <= 1.0e-30) return 0.0;
    double matched = 0.0, weakest = total;
    for (int i = 0; i < tones; ++i) {
        const double x = std::max(0.0, sanitize(chroma[(root + intervals[i]) % 12]));
        matched += x; weakest = std::min(weakest, x);
    }
    const double coverage = clamp(weakest / (0.05 * total), 0.0, 1.0);
    return clamp(matched / std::sqrt(tones * energy), 0.0, 1.0) * coverage;
}
struct ChordResult { int root=-1,quality=-1; double strength=0.0; bool changed=false; };
struct ChordTracker {
    double costs[kChordStates] {};
    bool ready=false;
    int selected=24, qualities=2;
    void reset() noexcept { *this=ChordTracker{}; }
    ChordResult process(const double* chroma, double threshold=0.65,
                        double transitionPenalty=0.2, bool extended=false) noexcept {
        const int requestedQualities = extended ? kChordQualities : 2;
        if (qualities != requestedQualities) { reset(); qualities = requestedQualities; }
        const int noChord = qualities * 12, states = noChord + 1;
        double next[kChordStates], scores[kChordStates];
        double total=0.0, bestScore=0.0;
        for (int i=0; i<12; ++i) total += std::max(0.0,sanitize(chroma[i]));
        for (int i=0; i<noChord; ++i) {
            scores[i]=chordSimilarity(chroma,i%12,i/12); bestScore=std::max(bestScore,scores[i]);
        }
        scores[noChord]=total<=1.0e-20 ? 1.0 : clamp(sanitize(threshold),0.0,1.0);
        const double global=ready ? *std::min_element(costs,costs+states) : 0.0;
        double minimum=1.0e30; int best=noChord;
        for (int i=0; i<states; ++i) {
            const double prior=ready ? std::min(costs[i],global+std::max(0.0,sanitize(transitionPenalty))) : 0.0;
            next[i]=prior-std::log(std::max(scores[i],1.0e-12));
            // Treat roundoff-level equalities as ties. Lower quality/root wins
            // a new path; continuation costs can preserve an existing alias.
            if (next[i]<minimum-1.0e-12) { minimum=next[i]; best=i; }
        }
        if (total<=1.0e-20) { best=noChord; minimum=next[noChord]; }
        for (int i=0; i<states; ++i) costs[i]=next[i]-minimum;
        const bool changed=ready&&best!=selected; selected=best; ready=true;
        return {best==noChord?-1:best%12,best==noChord?-1:best/12,
                best==noChord?bestScore:scores[best],changed};
    }
};
} // namespace mir2
