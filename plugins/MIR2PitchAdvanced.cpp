// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/PitchAdvanced.hpp"

namespace {
using namespace mir2plugin;
enum class AdvancedKind { candidates, probability, ambiguity, swipe, consensus, salience, multiple, melody };
struct AdvancedPitchUnit : Unit {
    PitchState state;
    mir2::SwipeWorkspace swipe;
    mir2::PitchTracker tracker;
    mir2::MelodyTracker melody;
    double *magnitude, *white, *residual;
    float outputs[2 * mir2::kPitchCandidates + 1];
    int mode, capacity, grid, bpo;
    AdvancedKind kind;
};

void destroyAdvanced(AdvancedPitchUnit* unit) {
    destroyPitchState(unit->mWorld, unit->state);
    release(unit->mWorld, unit->swipe.frequencies);
    release(unit->mWorld, unit->swipe.erbFrequencies);
    release(unit->mWorld, unit->swipe.kernels);
    release(unit->mWorld, unit->swipe.loudness);
    release(unit->mWorld, unit->swipe.salience);
    release(unit->mWorld, unit->magnitude);
    release(unit->mWorld, unit->white);
    release(unit->mWorld, unit->residual);
}
void writeSalience(AdvancedPitchUnit* unit, bool silence) {
    SndBuf* buffer = resolveBuffer(unit, 8);
    unit->outputs[0] = 0.0f;
    if (!buffer) return;
    LOCK_SNDBUF(buffer);
    if (!buffer->data || buffer->channels != 1 || buffer->samples < unit->grid) return;
    for (int i = 0; i < unit->grid; ++i)
        buffer->data[i] = silence ? 0.0f : float(mir2::sanitize(unit->swipe.salience[i]));
    unit->outputs[0] = 1.0f;
    unit->outputs[1] = float(unit->grid);
}
void analyzeAdvanced(AdvancedPitchUnit* unit) {
    auto& state = unit->state;
    copyCurrentFrame(state);
    const double sr = unit->mWorld->mSampleRate, low = IN0(1), high = IN0(2);
    mir2::PitchWorkspace time {state.fftData, state.prefixEnergy, state.difference,
                             state.cmndf, state.nsdf, state.frameSize, state.fftSize};
    double rms = 0.0;
    double mean = 0.0;
    for (int i = 0; i < state.frameSize; ++i) mean += state.frame[i];
    mean /= state.frameSize;
    for (int i = 0; i < state.frameSize; ++i) rms += (state.frame[i] - mean) * (state.frame[i] - mean);
    rms = std::sqrt(rms / state.frameSize);
    std::fill(unit->outputs, unit->outputs + 2 * mir2::kPitchCandidates + 1, 0.0f);
    if (rms < 1.0e-7) {
        unit->tracker.reset(); unit->melody = {};
        if (unit->kind == AdvancedKind::salience) writeSalience(unit, true);
        return;
    }
    mir2::CandidateList candidates;
    if (unit->mode <= 2 || unit->kind == AdvancedKind::consensus) {
        mir2::preparePitchEvidence(state.frame, time, rms);
        candidates = mir2::timePitchCandidates(time, sr, low, high, unit->mode,
                                              unit->kind == AdvancedKind::probability ? mir2::boundedInteger(IN0(6)) : 32);
    }
    if (unit->mode == 4 || unit->kind == AdvancedKind::consensus) {
        mir2::analyzeSwipe(state.frame, unit->swipe);
        candidates = mir2::salienceCandidates(unit->swipe.salience, unit->swipe.frequencies,
                                             unit->grid, unit->bpo);
    } else if (unit->mode == 3) {
        mir2::pitchMagnitude(state.frame, state.frameSize, state.fftData, unit->magnitude);
        mir2::whitenPitchSpectrum(unit->magnitude, unit->white, state.frameSize + 1, sr / state.fftSize);
        if (unit->kind == AdvancedKind::multiple) {
            candidates = mir2::multiPitch(unit->white, unit->residual, state.frameSize + 1,
                sr / state.fftSize, unit->swipe.frequencies, unit->grid, unit->bpo,
                unit->swipe.salience, unit->capacity, mir2::clamp(double(IN0(6)), 0.0, 1.0));
        } else {
            mir2::harmonicPitchSalience(unit->white, state.frameSize + 1, sr / state.fftSize,
                unit->swipe.frequencies, unit->grid, unit->swipe.salience);
            double maximum = 0.0;
            for (int i = 0; i < unit->grid; ++i) maximum = std::max(maximum, unit->swipe.salience[i]);
            // Relative harmonic salience, not calibrated probability.
            if (maximum > 1.0e-20)
                for (int i = 0; i < unit->grid; ++i) unit->swipe.salience[i] /= maximum;
            candidates = mir2::salienceCandidates(unit->swipe.salience, unit->swipe.frequencies,
                                                 unit->grid, unit->bpo, 0.15);
            const double tonality = mir2::pitchSpectralTonality(unit->white, state.frameSize + 1,
                                                               sr / state.fftSize);
            if (tonality < 0.1) candidates.clear();
            else candidates.normalize(tonality);
        }
    }
    const auto uncertainty = mir2::pitchUncertainty(candidates);
    const double hz = candidates.count ? candidates.values[0].frequencyHz : 0.0;
    const double score = candidates.count ? candidates.values[0].score : 0.0;
    switch (unit->kind) {
    case AdvancedKind::candidates:
    case AdvancedKind::multiple:
        for (int c = 0; c < std::min(unit->capacity, candidates.count); ++c) {
            unit->outputs[c] = float(candidates.values[static_cast<std::size_t>(c)].frequencyHz);
            unit->outputs[unit->capacity + c] = float(candidates.values[static_cast<std::size_t>(c)].probability);
        }
        if (unit->kind == AdvancedKind::multiple) unit->outputs[2 * unit->capacity] = float(candidates.count);
        break;
    case AdvancedKind::probability: {
        const bool tracking = IN0(10) > 0.0f;
        const auto result = tracking ? unit->tracker.process(candidates)
            : mir2::ContourResult{candidates.mass() >= 0.5 ? hz : 0.0,
                                   candidates.count ? candidates.values[0].probability : 0.0, 0.0};
        unit->outputs[0] = float(result.frequency);
        unit->outputs[1] = float(result.frequency > 0.0 ? result.confidence : 0.0);
        unit->outputs[2] = float(candidates.mass());
        unit->outputs[3] = float(uncertainty.octave);
        break;
    }
    case AdvancedKind::ambiguity:
        unit->outputs[0] = float(uncertainty.entropy); unit->outputs[1] = float(uncertainty.octave);
        unit->outputs[2] = float(uncertainty.competitor); break;
    case AdvancedKind::swipe:
        unit->outputs[0] = score >= IN0(6) ? float(hz) : 0.0f;
        unit->outputs[1] = float(score); unit->outputs[2] = score >= IN0(6) ? 1.0f : 0.0f;
        unit->outputs[3] = float(uncertainty.octave); break;
    case AdvancedKind::consensus: {
        const auto yin = mir2::analyzeYIN(time, sr, low, high, 0.1, rms);
        const auto mpm = mir2::analyzeMPM(time, sr, low, high, 0.93, rms);
        const int mask = mir2::boundedInteger(IN0(6));
        double frequencies[] {yin.frequency, mpm.frequency, score >= 0.2 ? hz : 0.0};
        double confidence[] {yin.confidence * ((mask & 1) != 0), mpm.confidence * ((mask & 2) != 0),
                             score * ((mask & 4) != 0)};
        const auto result = mir2::pitchConsensus(frequencies, confidence, 3);
        unit->outputs[0] = float(result.frequency); unit->outputs[1] = float(result.agreement);
        unit->outputs[2] = float(result.spread); unit->outputs[3] = float(result.octaveConflict); break;
    }
    case AdvancedKind::salience: writeSalience(unit, false); break;
    case AdvancedKind::melody: {
        const auto result = unit->melody.process(candidates, double(state.hopSamples) / sr,
                                                mir2::clamp(double(IN0(6)), 0.0, 1.0));
        unit->outputs[0] = float(result.frequency); unit->outputs[1] = float(result.confidence);
        unit->outputs[2] = result.frequency > 0.0 ? 1.0f : 0.0f; break;
    }
    }
}
void AdvancedPitch_next(AdvancedPitchUnit* unit, int) {
    if (unit->kind == AdvancedKind::salience) unit->outputs[0] = 0.0f;
    auto& state = unit->state;
    const int samples = unit->mWorld->mFullRate.mBufLength;
    const float* input = IN(0);
    for (int sample = 0; sample < samples; ++sample) {
        state.ring[state.writeIndex] = float(mir2::sanitize(input[sample]));
        state.writeIndex = (state.writeIndex + 1) % state.frameSize;
        state.filled = std::min(state.frameSize, state.filled + 1);
        if (--state.samplesUntilAnalysis <= 0 && state.filled == state.frameSize) {
            analyzeAdvanced(unit); state.samplesUntilAnalysis = state.hopSamples;
        }
    }
    for (unsigned int i = 0; i < unit->mNumOutputs; ++i) ZOUT0(i) = unit->outputs[i];
}
void initializeAdvanced(AdvancedPitchUnit* unit, AdvancedKind kind) {
    clearPitchState(unit->state);
    unit->swipe = {}; unit->tracker = {}; unit->melody = {};
    unit->magnitude = unit->white = unit->residual = nullptr;
    std::fill(unit->outputs, unit->outputs + 2 * mir2::kPitchCandidates + 1, 0.0f);
    unit->kind = kind;
    unit->mode = mir2::clamp(mir2::boundedInteger(IN0(5)), 0, 4);
    unit->capacity = mir2::clamp(mir2::boundedInteger(IN0(7)), 1, mir2::kPitchCandidates);
    unit->bpo = mir2::clamp(mir2::boundedInteger(IN0(9)), 12, 96);
    const double low = IN0(1), high = IN0(2), sr = unit->mWorld->mSampleRate;
    bool valid = INRATE(0) == calc_FullRate && std::isfinite(low) && std::isfinite(high)
        && low > 0.0 && high > low && high < sr * 0.5 && high / low < 1024.0
        && initializePitchState(unit, unit->state, mir2::boundedInteger(IN0(3)), IN0(4));
    unit->grid = mir2::pitchGridSize(low, high, unit->bpo);
    if (valid && (unit->mode >= 3 || kind == AdvancedKind::consensus)) {
        auto& swipe = unit->swipe;
        swipe.frameSize = unit->state.frameSize; swipe.fftData = unit->state.fftData;
        swipe.gridSize = unit->grid; swipe.sampleRate = sr; swipe.binsPerOctave = unit->bpo;
        swipe.frequencies = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->grid));
        swipe.salience = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->grid));
        valid = swipe.frequencies && swipe.salience;
        if (valid) for (int i = 0; i < unit->grid; ++i)
            swipe.frequencies[i] = low * std::exp2(double(i) / unit->bpo);
        if (unit->mode == 4 || kind == AdvancedKind::consensus) {
            swipe.erbSize = int(std::floor(mir2::pitchERB(sr * 0.5) / 0.1));
            swipe.erbFrequencies = allocate<double>(unit->mWorld, static_cast<std::size_t>(swipe.erbSize));
            swipe.kernels = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->grid * swipe.erbSize));
            swipe.loudness = allocate<double>(unit->mWorld, static_cast<std::size_t>(6 * swipe.erbSize));
            valid = valid && swipe.erbFrequencies && swipe.kernels && swipe.loudness;
            if (valid) mir2::initializeSwipe(swipe, low);
        } else {
            unit->magnitude = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->state.frameSize + 1));
            unit->white = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->state.frameSize + 1));
            unit->residual = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->state.frameSize + 1));
            valid = valid && unit->magnitude && unit->white && unit->residual;
        }
    }
    if (!valid) {
        Print("MIR2 advanced pitch: invalid audio input, frame/range, or insufficient RT memory\n");
        destroyAdvanced(unit); SETCALC(ClearUnitOutputs); unit->mDone = true;
    } else SETCALC(AdvancedPitch_next);
    ClearUnitOutputs(unit, 1);
}
#define MIR2_ADVANCED_UNIT(Name, Kind) \
    struct Name : AdvancedPitchUnit {}; \
    void Name##_Ctor(Name* unit) { initializeAdvanced(unit, AdvancedKind::Kind); } \
    void Name##_Dtor(Name* unit) { destroyAdvanced(unit); }
MIR2_ADVANCED_UNIT(FundamentalCandidates, candidates)
MIR2_ADVANCED_UNIT(PitchProb, probability)
MIR2_ADVANCED_UNIT(PitchAmbiguity, ambiguity)
MIR2_ADVANCED_UNIT(PitchSWIPE, swipe)
MIR2_ADVANCED_UNIT(PitchConsensus, consensus)
MIR2_ADVANCED_UNIT(PitchSalienceBuf, salience)
MIR2_ADVANCED_UNIT(MultiPitch, multiple)
MIR2_ADVANCED_UNIT(PredominantPitch, melody)
#undef MIR2_ADVANCED_UNIT

struct PitchContour : Unit { mir2::PitchTracker tracker; mir2::ContourResult result; };
void PitchContour_next(PitchContour* unit, int) {
    if (IN0(5) > 0.0f) {
        mir2::CandidateList candidates;
        const int count = mir2::clamp(mir2::boundedInteger(IN0(0)), 1, mir2::kPitchCandidates);
        for (int i = 0; i < count; ++i) {
            const double p = mir2::clamp(mir2::sanitize(IN0(7 + count + i)), 0.0, 1.0);
            candidates.add({mir2::sanitize(IN0(7 + i)), p, p, 0.0, 0.0, 0.0});
        }
        const double mass = candidates.mass();
        double total = 0.0;
        for (int i = 0; i < candidates.count; ++i) total += candidates.values[static_cast<std::size_t>(i)].probability;
        if (total > 1.0) candidates.normalize(mass);
        unit->result = unit->tracker.process(candidates, std::max(1.0f, IN0(1)),
            std::max(0.0f, IN0(2)), std::max(0.0f, IN0(3)), std::max(0.0f, IN0(4)), mir2::boundedInteger(IN0(6)));
    }
    ZOUT0(0) = float(unit->result.frequency); ZOUT0(1) = float(unit->result.confidence);
    ZOUT0(2) = float(unit->result.jump);
}
void PitchContour_Ctor(PitchContour* unit) {
    unit->tracker = {}; unit->result = {}; SETCALC(PitchContour_next);
    ClearUnitOutputs(unit, 1);
}

struct PitchDeviation : Unit { double previous; };
void PitchDeviation_next(PitchDeviation* unit, int) {
    const double hz = mir2::sanitize(IN0(0));
    ZOUT0(0) = float(mir2::pitchDeviation(hz, IN0(1), mir2::boundedInteger(IN0(2)), unit->previous));
    unit->previous = hz > 0.0 ? hz : 0.0;
}
void PitchDeviation_Ctor(PitchDeviation* unit) { unit->previous = 0.0; SETCALC(PitchDeviation_next); ZOUT0(0) = 0.0f; }

struct PitchHistoryUnit : Unit {
    double *cents, *weights, *orderedCents, *orderedWeights;
    int size, index, filled, downsample, countdown, analysisCountdown;
    double featureRate;
    mir2::VibratoResult vibrato;
};
void destroyHistory(PitchHistoryUnit* unit) {
    release(unit->mWorld, unit->cents); release(unit->mWorld, unit->weights);
    release(unit->mWorld, unit->orderedCents); release(unit->mWorld, unit->orderedWeights);
}
bool initializeHistory(PitchHistoryUnit* unit, double time, bool vibrato) {
    unit->cents = unit->weights = unit->orderedCents = unit->orderedWeights = nullptr;
    unit->index = unit->filled = 0; unit->vibrato = {};
    const double controlRate = unit->mWorld->mSampleRate / unit->mWorld->mFullRate.mBufLength;
    unit->downsample = vibrato ? std::max(1, int(std::floor(controlRate / 100.0))) : 1;
    unit->countdown = unit->downsample; unit->analysisCountdown = 1;
    unit->featureRate = controlRate / unit->downsample;
    unit->size = mir2::clamp(int(std::lround(mir2::clamp(time, 0.05, 5.0) * unit->featureRate)), 4, 16384);
    unit->cents = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->weights = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->orderedCents = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->orderedWeights = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    const bool valid = unit->cents && unit->weights && unit->orderedCents && unit->orderedWeights;
    if (valid) {
        std::fill(unit->cents, unit->cents + unit->size, 0.0);
        std::fill(unit->weights, unit->weights + unit->size, 0.0);
    }
    return valid;
}
bool appendHistory(PitchHistoryUnit* unit) {
    if (--unit->countdown > 0) return false;
    unit->countdown = unit->downsample;
    const double hz = mir2::sanitize(IN0(0));
    unit->cents[unit->index] = hz > 0.0 ? 1200.0 * std::log2(hz / 440.0) : 0.0;
    unit->weights[unit->index] = hz > 0.0 ? mir2::clamp(mir2::sanitize(IN0(1)), 0.0, 1.0) : 0.0;
    unit->index = (unit->index + 1) % unit->size;
    unit->filled = std::min(unit->size, unit->filled + 1);
    return true;
}
struct PitchStability : PitchHistoryUnit {};
void PitchStability_next(PitchStability* unit, int) {
    appendHistory(unit);
    const auto result = IN0(0) > 0.0f && IN0(1) > 0.0f
        ? mir2::pitchStability(unit->cents, unit->weights, unit->size) : mir2::StabilityResult{};
    ZOUT0(0) = float(result.stability); ZOUT0(1) = float(result.deviation);
}
void PitchStability_Ctor(PitchStability* unit) {
    if (initializeHistory(unit, IN0(2), false)) SETCALC(PitchStability_next);
    else { destroyHistory(unit); SETCALC(ClearUnitOutputs); unit->mDone = true; }
    ClearUnitOutputs(unit, 1);
}
void PitchStability_Dtor(PitchStability* unit) { destroyHistory(unit); }
struct Vibrato2 : PitchHistoryUnit {};
void Vibrato2_next(Vibrato2* unit, int) {
    if (appendHistory(unit) && --unit->analysisCountdown <= 0) {
        unit->analysisCountdown = std::max(1, int(unit->featureRate / 20.0));
        if (unit->filled == unit->size) {
            for (int i = 0; i < unit->size; ++i) {
                const int source = (unit->index + i) % unit->size;
                unit->orderedCents[i] = unit->cents[source]; unit->orderedWeights[i] = unit->weights[source];
            }
            unit->vibrato = mir2::analyzeVibrato(unit->orderedCents, unit->orderedWeights, unit->size,
                unit->featureRate, IN0(2), IN0(3), IN0(4));
        }
    }
    if (IN0(0) <= 0.0f || IN0(1) <= 0.0f) unit->vibrato = {};
    ZOUT0(0) = float(unit->vibrato.present); ZOUT0(1) = float(unit->vibrato.rate);
    ZOUT0(2) = float(unit->vibrato.extent); ZOUT0(3) = float(unit->vibrato.confidence);
}
void Vibrato2_Ctor(Vibrato2* unit) {
    if (initializeHistory(unit, IN0(5), true)) SETCALC(Vibrato2_next);
    else { destroyHistory(unit); SETCALC(ClearUnitOutputs); unit->mDone = true; }
    ClearUnitOutputs(unit, 1);
}
void Vibrato2_Dtor(Vibrato2* unit) { destroyHistory(unit); }
} // namespace
namespace mir2plugin {
void registerPitchAdvancedUnits() {
    DefineDtorUnit(FundamentalCandidates); DefineDtorUnit(PitchProb); DefineDtorUnit(PitchAmbiguity);
    DefineDtorUnit(PitchSWIPE); DefineDtorUnit(PitchConsensus); DefineDtorUnit(PitchSalienceBuf);
    DefineDtorUnit(MultiPitch); DefineDtorUnit(PredominantPitch); DefineSimpleUnit(PitchContour);
    DefineSimpleUnit(PitchDeviation); DefineDtorUnit(PitchStability); DefineDtorUnit(Vibrato2);
}
} // namespace mir2plugin
