// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Rhythm.hpp"
#include "mir2/SpectralAdvanced.hpp"

namespace {
using namespace mir2plugin;
struct RhythmUnit : Unit {
    mir2::RhythmWorkspace work;
    mir2::CausalBeatClock clock;
    mir2::RhythmEstimate estimate;
    double* ring;
    double* histogram;
    int writeIndex, filled, poolBlocks, untilSample, hopFrames, untilAnalysis;
    double pooled, seconds, updateTime, warmup, prior, continuity, tempoSmooth, phaseSmooth, decay;
    bool normalize;
};

enum class RhythmOutput { autocorrelation, fourier, histogram, tempo, tracker, statistics, clarity };

void failRhythm(Unit* unit) {
    Print("MIR2 rhythm: invalid dimensions/input rate or insufficient real-time memory\n");
    SETCALC(ClearUnitOutputs); unit->mDone = true; ClearUnitOutputs(unit, 1);
}

bool initializeRhythm(RhythmUnit* unit) {
    unit->work = {}; unit->clock = {}; unit->estimate = {};
    unit->ring = unit->histogram = nullptr;
    unit->writeIndex = unit->filled = 0; unit->pooled = 0.0;
    const double controlRate = unit->mWorld->mSampleRate / unit->mWorld->mFullRate.mBufLength;
    unit->seconds = 1.0 / controlRate;
    const double requestedRate = IN0(6), window = IN0(4), hop = IN0(5);
    unit->work.minBPM = IN0(1); unit->work.maxBPM = IN0(2); unit->work.numBins = mir2::boundedInteger(IN0(3));
    if (INRATE(0) == calc_FullRate || !std::isfinite(requestedRate) || requestedRate < 20.0 || requestedRate > 200.0
        || !std::isfinite(window) || window < 0.5 || window > 16.0 || !std::isfinite(hop) || hop < 0.02 || hop > 2.0
        || !std::isfinite(unit->work.minBPM) || !std::isfinite(unit->work.maxBPM) || unit->work.minBPM < 20.0 || unit->work.maxBPM > 600.0 || unit->work.maxBPM <= unit->work.minBPM
        || unit->work.numBins < 8 || unit->work.numBins > 512) return false;
    unit->poolBlocks = std::max(1, static_cast<int>(std::lround(controlRate / requestedRate)));
    unit->untilSample = unit->poolBlocks;
    unit->work.featureRate = controlRate / unit->poolBlocks;
    unit->work.windowFrames = std::max(3, static_cast<int>(std::ceil(window * unit->work.featureRate)));
    unit->work.fftSize = mir2::nextPowerOfTwo(2 * unit->work.windowFrames);
    unit->hopFrames = std::max(1, static_cast<int>(std::lround(hop * unit->work.featureRate)));
    unit->untilAnalysis = unit->hopFrames;
    unit->updateTime = unit->hopFrames / unit->work.featureRate;
    unit->normalize = IN0(7) > 0.0f;
    unit->prior = mir2::clamp(static_cast<double>(IN0(8)), unit->work.minBPM, unit->work.maxBPM);
    unit->continuity = mir2::clamp(static_cast<double>(IN0(9)), 0.0, 20.0);
    unit->tempoSmooth = std::max(0.0, static_cast<double>(IN0(10)));
    unit->phaseSmooth = std::max(unit->seconds, static_cast<double>(IN0(11)));
    unit->warmup = std::max(0.0, static_cast<double>(IN0(13)));
    unit->decay = std::max(0.0, static_cast<double>(IN0(14)));
    const auto count = static_cast<std::size_t>(unit->work.windowFrames);
    const auto bins = static_cast<std::size_t>(unit->work.numBins);
    unit->ring = allocate<double>(unit->mWorld, count);
    unit->work.frame = allocate<double>(unit->mWorld, count);
    unit->work.prefix = allocate<double>(unit->mWorld, count + 1);
    unit->work.fft = allocate<mir2::Complex>(unit->mWorld, static_cast<std::size_t>(unit->work.fftSize));
    unit->work.correlation = allocate<double>(unit->mWorld, count);
    unit->work.autocorr = allocate<double>(unit->mWorld, bins);
    unit->work.fourier = allocate<double>(unit->mWorld, bins);
    unit->work.evidence = allocate<double>(unit->mWorld, bins);
    unit->work.phases = allocate<double>(unit->mWorld, bins);
    unit->histogram = allocate<double>(unit->mWorld, bins);
    if (!unit->ring || !unit->work.frame || !unit->work.prefix || !unit->work.fft || !unit->work.correlation
        || !unit->work.autocorr || !unit->work.fourier || !unit->work.evidence || !unit->work.phases || !unit->histogram) return false;
    std::fill(unit->ring, unit->ring + count, 0.0);
    std::fill(unit->histogram, unit->histogram + bins, 0.0);
    return true;
}

void destroyRhythm(RhythmUnit* unit) {
    release(unit->mWorld, unit->ring); release(unit->mWorld, unit->histogram);
    release(unit->mWorld, unit->work.frame); release(unit->mWorld, unit->work.prefix);
    release(unit->mWorld, unit->work.fft); release(unit->mWorld, unit->work.correlation);
    release(unit->mWorld, unit->work.autocorr); release(unit->mWorld, unit->work.fourier);
    release(unit->mWorld, unit->work.evidence); release(unit->mWorld, unit->work.phases);
}

template<RhythmOutput output>
void rhythmNext(RhythmUnit* unit, int) {
    const double input = std::max(0.0, mir2::sanitize(IN0(0)));
    const bool lock = IN0(12) > 0.0f;
    bool beat, subdivision;
    unit->clock.advance(unit->seconds, input, unit->phaseSmooth, lock, beat, subdivision);
    unit->pooled = std::max(unit->pooled, input);
    bool analyzed = false;
    if (--unit->untilSample <= 0) {
        unit->untilSample = unit->poolBlocks;
        unit->ring[unit->writeIndex] = unit->pooled; unit->pooled = 0.0;
        unit->writeIndex = (unit->writeIndex + 1) % unit->work.windowFrames;
        unit->filled = std::min(unit->work.windowFrames, unit->filled + 1);
        if (--unit->untilAnalysis <= 0) {
            unit->untilAnalysis = unit->hopFrames;
            if (unit->filled / unit->work.featureRate >= unit->warmup) {
                for (int index = 0; index < unit->filled; ++index)
                    unit->work.frame[index] = unit->ring[(unit->writeIndex-unit->filled+unit->work.windowFrames+index)%unit->work.windowFrames];
                mir2::rhythmEvidence(unit->work, unit->filled, unit->normalize);
                if (unit->clock.sinceActivity > unit->work.windowFrames / unit->work.featureRate && !lock) unit->clock = {};
                unit->estimate = mir2::chooseRhythm(unit->work, unit->clock.bpm, unit->prior, unit->continuity);
                unit->clock.observe(unit->estimate, unit->updateTime, unit->tempoSmooth, unit->phaseSmooth, lock);
                if (unit->work.prefix[unit->filled] <= 1.0e-20)
                    std::fill(unit->histogram, unit->histogram + unit->work.numBins, 0.0);
                const double alpha = unit->decay > 0.0 ? std::exp(-unit->updateTime / unit->decay) : 0.0;
                for (int bin = 0; bin < unit->work.numBins; ++bin)
                    unit->histogram[bin] = alpha*unit->histogram[bin] + (1.0-alpha)*unit->work.evidence[bin];
                analyzed = true;
            }
        }
    }
    const double confidence = unit->clock.bpm > 0.0
        && unit->clock.sinceActivity < std::max(1.0, 120.0 / unit->clock.bpm) ? unit->clock.confidence : 0.0;
    const double bpm = (lock || confidence >= 0.12) ? unit->clock.bpm : 0.0;
    if (output == RhythmOutput::tracker) {
        ZOUT0(0) = beat ? 1.0f : 0.0f; ZOUT0(1) = subdivision ? 1.0f : 0.0f;
        ZOUT0(2) = static_cast<float>(bpm); ZOUT0(3) = static_cast<float>(unit->clock.phase);
        ZOUT0(4) = static_cast<float>(confidence);
    } else if (output == RhythmOutput::tempo) {
        ZOUT0(0) = static_cast<float>(bpm); ZOUT0(1) = static_cast<float>(confidence);
    } else if (analyzed) {
        if (output == RhythmOutput::clarity) {
            ZOUT0(0) = static_cast<float>(unit->estimate.clarity);
        } else if (output == RhythmOutput::statistics) {
            ZOUT0(0) = static_cast<float>(bpm); ZOUT0(1) = static_cast<float>(confidence);
            ZOUT0(2) = static_cast<float>(unit->estimate.secondaryBPM);
            ZOUT0(3) = static_cast<float>(unit->estimate.dominance);
            ZOUT0(4) = static_cast<float>(unit->estimate.entropy);
        } else {
            double sum = 0.0;
            if (output == RhythmOutput::histogram && unit->normalize)
                for (int bin = 0; bin < unit->work.numBins; ++bin) sum += unit->histogram[bin];
            for (int bin = 0; bin < unit->work.numBins; ++bin) {
                double value = output == RhythmOutput::autocorrelation ? unit->work.autocorr[bin]
                    : output == RhythmOutput::fourier ? unit->work.fourier[bin] : unit->histogram[bin];
                if (output == RhythmOutput::histogram && unit->normalize) value = sum > 1.0e-20 ? value / sum : 0.0;
                ZOUT0(bin) = static_cast<float>(mir2::sanitize(value));
            }
        }
    }
}

#define MIR2_RHYTHM(Name, Kind, Outputs) \
    struct Name : RhythmUnit {}; \
    void Name##_Ctor(Name* unit) { \
        if (!initializeRhythm(unit) || static_cast<int>(unit->mNumOutputs) != (Outputs)) { failRhythm(unit); return; } \
        SETCALC(rhythmNext<RhythmOutput::Kind>); ClearUnitOutputs(unit, 1); \
    } \
    void Name##_Dtor(Name* unit) { destroyRhythm(unit); }
MIR2_RHYTHM(Tempogram, autocorrelation, unit->work.numBins)
MIR2_RHYTHM(FourierTempogram, fourier, unit->work.numBins)
MIR2_RHYTHM(BeatHistogram, histogram, unit->work.numBins)
MIR2_RHYTHM(Tempo, tempo, 2)
MIR2_RHYTHM(BeatTrack3, tracker, 5)
MIR2_RHYTHM(AutoTrack2, tracker, 5)
MIR2_RHYTHM(BeatStatistics2, statistics, 5)
MIR2_RHYTHM(PulseClarity, clarity, 1)
#undef MIR2_RHYTHM

struct OnsetStatsUnit : Unit { mir2::OnsetWindow window; double now, seconds; };
void onsetStatsNext(OnsetStatsUnit* unit, int) {
    unit->window.update(unit->now, IN0(0)); unit->now += unit->seconds;
    double rate, mean, deviation, cv;
    unit->window.statistics(rate, mean, deviation, cv);
    ZOUT0(0) = static_cast<float>(rate);
    if (unit->mNumOutputs > 1) { ZOUT0(1) = static_cast<float>(mean); ZOUT0(2) = static_cast<float>(deviation); ZOUT0(3) = static_cast<float>(cv); }
}
bool initializeOnsetStats(OnsetStatsUnit* unit) {
    unit->window = {}; unit->now = 0.0;
    unit->seconds = unit->mWorld->mFullRate.mBufLength / unit->mWorld->mSampleRate;
    unit->window.window = IN0(1); unit->window.minGap = std::max(0.0, static_cast<double>(IN0(2)));
    if (INRATE(0) == calc_FullRate || !std::isfinite(unit->window.window) || unit->window.window < .05 || unit->window.window > 60.0) return false;
    unit->window.capacity = static_cast<int>(std::ceil(unit->window.window / unit->seconds)) + 2;
    unit->window.times = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->window.capacity));
    return unit->window.times != nullptr;
}
struct OnsetRate : OnsetStatsUnit {};
struct OnsetStatistics2 : OnsetStatsUnit {};
void OnsetRate_Ctor(OnsetRate* unit) { if (!initializeOnsetStats(unit)) { failRhythm(unit); return; } SETCALC(onsetStatsNext); ClearUnitOutputs(unit, 1); }
void OnsetStatistics2_Ctor(OnsetStatistics2* unit) { if (!initializeOnsetStats(unit)) { failRhythm(unit); return; } SETCALC(onsetStatsNext); ClearUnitOutputs(unit, 1); }
void OnsetRate_Dtor(OnsetRate* unit) { release(unit->mWorld, unit->window.times); }
void OnsetStatistics2_Dtor(OnsetStatistics2* unit) { release(unit->mWorld, unit->window.times); }

struct TempoStability : Unit { mir2::TempoWindow window; double minConfidence; };
void TempoStability_next(TempoStability* unit, int) {
    ZOUT0(0) = static_cast<float>(unit->window.update(IN0(0), IN0(1), unit->minConfidence));
}
void TempoStability_Ctor(TempoStability* unit) {
    unit->window = {}; unit->minConfidence = mir2::clamp(static_cast<double>(IN0(3)), 0.0, 1.0);
    const double duration = IN0(2), rate = unit->mWorld->mSampleRate / unit->mWorld->mFullRate.mBufLength;
    if (INRATE(0) == calc_FullRate || INRATE(1) == calc_FullRate || !std::isfinite(duration) || duration < .05 || duration > 60.0) { failRhythm(unit); return; }
    unit->window.size = std::max(2, static_cast<int>(std::ceil(duration * rate)));
    unit->window.values = allocate<mir2::TempoMoment>(unit->mWorld, static_cast<std::size_t>(unit->window.size));
    if (!unit->window.values) { failRhythm(unit); return; }
    std::fill(unit->window.values, unit->window.values + unit->window.size, mir2::TempoMoment {});
    SETCALC(TempoStability_next); ClearUnitOutputs(unit, 1);
}
void TempoStability_Dtor(TempoStability* unit) { release(unit->mWorld, unit->window.values); }

struct TempoRubato : Unit { mir2::TempoDerivative derivative; double seconds; };
void TempoRubato_next(TempoRubato* unit, int) {
    ZOUT0(0) = static_cast<float>(unit->derivative.update(IN0(0), IN0(1), unit->seconds,
        std::max(0.0, static_cast<double>(IN0(2))), mir2::clamp(static_cast<double>(IN0(3)), 0.0, 1.0)));
}
void TempoRubato_Ctor(TempoRubato* unit) {
    unit->derivative = {}; unit->seconds = unit->mWorld->mFullRate.mBufLength / unit->mWorld->mSampleRate;
    if (INRATE(0) == calc_FullRate || INRATE(1) == calc_FullRate) { failRhythm(unit); return; }
    SETCALC(TempoRubato_next); ClearUnitOutputs(unit, 1);
}

struct BeatLoudness : Unit { mir2::BeatEnergy energy; int maximumSamples; };
void BeatLoudness_next(BeatLoudness* unit, int) {
    unit->energy.startBlock(IN0(1));
    const float* input = IN(0);
    for (int sample = 0; sample < unit->mWorld->mFullRate.mBufLength; ++sample)
        unit->energy.sample(input[sample], unit->maximumSamples);
    ZOUT0(0) = static_cast<float>(unit->energy.rms); ZOUT0(1) = static_cast<float>(unit->energy.completedPeak);
}
void BeatLoudness_Ctor(BeatLoudness* unit) {
    unit->energy = {};
    unit->maximumSamples = static_cast<int>(unit->mWorld->mSampleRate * mir2::clamp(static_cast<double>(IN0(2)), 0.1, 60.0));
    if (INRATE(0) != calc_FullRate || INRATE(1) == calc_FullRate || unit->mNumOutputs != 2) { failRhythm(unit); return; }
    SETCALC(BeatLoudness_next); ClearUnitOutputs(unit, 1);
}

struct BeatBandRatio : Unit {
    double* magnitudes;
    double sums[64];
    double total, elapsed, seconds, maxInterval;
    int fftSize, numBands;
    bool armed, high;
};
void BeatBandRatio_next(BeatBandRatio* unit, int) {
    const bool high = IN0(1) > 0.0f;
    if (high && !unit->high) {
        for (int band = 0; band < unit->numBands; ++band) {
            ZOUT0(band) = unit->armed && unit->total > 1.0e-20 ? static_cast<float>(unit->sums[band] / unit->total) : 0.0f;
            unit->sums[band] = 0.0;
        }
        unit->total = unit->elapsed = 0.0; unit->armed = true;
    }
    unit->high = high;
    if (!unit->armed) return;
    unit->elapsed += unit->seconds;
    if (unit->elapsed > unit->maxInterval) { unit->armed = false; ClearUnitOutputs(unit, 1); return; }
    if (IN0(0) < 0.0f) return;
    SndBuf* buffer = resolveBuffer(unit, 0); if (!buffer) return;
    { LOCK_SNDBUF(buffer); if (!fillMagnitudes(buffer, unit->fftSize, unit->magnitudes)) return; }
    const int bins = unit->fftSize / 2 + 1;
    const double binHz = unit->mWorld->mSampleRate / unit->fftSize;
    for (int bin = 0; bin < bins; ++bin) unit->total += mir2::spectralWeight(unit->magnitudes[bin], 1);
    for (int band = 0; band < unit->numBands; ++band) {
        int first, last;
        const double low = IN0(5 + band), highHz = IN0(6 + band);
        if (!mir2::spectralRange(bins, binHz, low, highHz, first, last)) continue;
        if (band+1 < unit->numBands && last*binHz >= highHz) --last;
        for (int bin = first; bin <= last; ++bin) unit->sums[band] += mir2::spectralWeight(unit->magnitudes[bin], 1);
    }
}
void BeatBandRatio_Ctor(BeatBandRatio* unit) {
    unit->magnitudes = nullptr; unit->total = unit->elapsed = 0.0; unit->armed = unit->high = false;
    unit->fftSize = mir2::boundedInteger(IN0(3)); unit->numBands = mir2::boundedInteger(IN0(2));
    unit->seconds = unit->mWorld->mFullRate.mBufLength / unit->mWorld->mSampleRate;
    unit->maxInterval = mir2::clamp(static_cast<double>(IN0(4)), 0.1, 60.0);
    std::fill(unit->sums, unit->sums + 64, 0.0);
    if (INRATE(1) == calc_FullRate || !mir2::isPowerOfTwo(unit->fftSize) || unit->fftSize < 256 || unit->fftSize > 8192
        || unit->numBands < 1 || unit->numBands > 64 || static_cast<int>(unit->mNumOutputs) != unit->numBands
        || static_cast<int>(unit->mNumInputs) != unit->numBands + 6) { failRhythm(unit); return; }
    unit->magnitudes = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->fftSize/2+1));
    if (!unit->magnitudes) { failRhythm(unit); return; }
    SETCALC(BeatBandRatio_next); ClearUnitOutputs(unit, 1);
}
void BeatBandRatio_Dtor(BeatBandRatio* unit) { release(unit->mWorld, unit->magnitudes); }
} // namespace

namespace mir2plugin {
void registerRhythmUnits() {
    DefineDtorUnit(Tempogram); DefineDtorUnit(FourierTempogram); DefineDtorUnit(BeatHistogram);
    DefineDtorUnit(Tempo); DefineDtorUnit(BeatTrack3); DefineDtorUnit(AutoTrack2);
    DefineDtorUnit(BeatStatistics2); DefineDtorUnit(PulseClarity);
    DefineDtorUnit(OnsetRate); DefineDtorUnit(OnsetStatistics2); DefineDtorUnit(TempoStability);
    DefineSimpleUnit(TempoRubato); DefineSimpleUnit(BeatLoudness); DefineDtorUnit(BeatBandRatio);
}
} // namespace mir2plugin
