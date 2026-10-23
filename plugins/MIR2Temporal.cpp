// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Temporal.hpp"

namespace {
using namespace mir2plugin;

struct TemporalState : Unit {
    double* envelope;
    double* scratch;
    int count, written, position, frameSamples, pending;
    double squareSum, step;
    mir2::EnvelopeStatistics result;
};

bool temporalInit(TemporalState* unit, int windowInput, int frameInput) {
    unit->envelope = nullptr; unit->scratch = nullptr;
    const double frame = mir2::clamp(mir2::sanitize(IN0(frameInput)), 0.001, 0.1);
    const double window = mir2::clamp(mir2::sanitize(IN0(windowInput)), frame * 2.0, 10.0);
    unit->frameSamples = std::max(1, static_cast<int>(std::lround(frame * unit->mWorld->mSampleRate)));
    unit->step = unit->frameSamples / unit->mWorld->mSampleRate;
    unit->count = mir2::clamp(static_cast<int>(std::ceil(window / unit->step)), 2, 10000);
    unit->envelope = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->scratch = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->written = 0; unit->position = 0; unit->pending = 0; unit->squareSum = 0.0;
    unit->result = {};
    if (!unit->envelope || !unit->scratch || INRATE(0) != calc_FullRate) {
        SETCALC(ClearUnitOutputs); ClearUnitOutputs(unit, 1); unit->mDone = true;
        return false;
    }
    std::fill(unit->envelope, unit->envelope + unit->count, 0.0);
    ClearUnitOutputs(unit, 1);
    return true;
}

void temporalProcess(TemporalState* unit, double thresholdDB) {
    const float* input = IN(0);
    for (int sample = 0; sample < unit->mWorld->mFullRate.mBufLength; ++sample) {
        const double x = mir2::clamp(mir2::sanitize(input[sample]), -1.0e6, 1.0e6);
        unit->squareSum += x * x;
        if (++unit->pending == unit->frameSamples) {
            unit->envelope[unit->position] = std::sqrt(unit->squareSum / unit->frameSamples);
            unit->position = (unit->position + 1) % unit->count;
            unit->written = std::min(unit->count, unit->written + 1);
            unit->pending = 0; unit->squareSum = 0.0;
            if (unit->written == unit->count) {
                for (int i = 0; i < unit->count; ++i)
                    unit->scratch[i] = unit->envelope[(unit->position + i) % unit->count];
                unit->result = mir2::envelopeStatistics(unit->scratch, unit->count, unit->step, thresholdDB);
            }
        }
    }
}

void temporalDestroy(TemporalState* unit) {
    release(unit->mWorld, unit->envelope); release(unit->mWorld, unit->scratch);
}

#define MIR2_TEMPORAL(NAME, MEMBER) \
struct NAME : TemporalState {}; \
void NAME##_next(NAME* unit, int) { temporalProcess(unit, IN0(3)); ZOUT0(0) = static_cast<float>(unit->result.MEMBER); } \
void NAME##_Ctor(NAME* unit) { if (temporalInit(unit, 1, 2)) SETCALC(NAME##_next); } \
void NAME##_Dtor(NAME* unit) { temporalDestroy(unit); }

MIR2_TEMPORAL(TemporalCentroid, centroid)
MIR2_TEMPORAL(EnvelopeSpread, spread)
MIR2_TEMPORAL(EnvelopeSkewness, skewness)
MIR2_TEMPORAL(EnvelopeKurtosis, kurtosis)
MIR2_TEMPORAL(EffectiveDuration, active)
MIR2_TEMPORAL(DynamicComplexity, complexity)
MIR2_TEMPORAL(StrongDecay, strongDecay)
MIR2_TEMPORAL(TemporalDecrease, decrease)
#undef MIR2_TEMPORAL

struct SilenceRate : TemporalState {};
void SilenceRate_next(SilenceRate* unit, int) {
    temporalProcess(unit, IN0(1)); ZOUT0(0) = static_cast<float>(unit->result.silence);
}
void SilenceRate_Ctor(SilenceRate* unit) { if (temporalInit(unit, 2, 3)) SETCALC(SilenceRate_next); }
void SilenceRate_Dtor(SilenceRate* unit) { temporalDestroy(unit); }

struct FadeDetection : TemporalState {};
void FadeDetection_next(FadeDetection* unit, int) {
    temporalProcess(unit, IN0(5));
    const auto& r = unit->result;
    const double threshold = std::max(0.0, mir2::sanitize(IN0(3)));
    const bool valid = r.fadeFit >= mir2::clamp(mir2::sanitize(IN0(4)), 0.0, 1.0);
    ZOUT0(0) = valid && std::abs(r.fadeSlope) > threshold ? (r.fadeSlope > 0.0 ? 1.0f : -1.0f) : 0.0f;
    ZOUT0(1) = static_cast<float>(r.fadeSlope); ZOUT0(2) = static_cast<float>(r.fadeFit);
}
void FadeDetection_Ctor(FadeDetection* unit) { if (temporalInit(unit, 1, 2)) SETCALC(FadeDetection_next); }
void FadeDetection_Dtor(FadeDetection* unit) { temporalDestroy(unit); }

struct EventState : Unit {
    double* envelope;
    int count, written, frameSamples, pending;
    double squareSum, step, previousTrigger;
    bool recording;
    mir2::AttackMeasurement result;
};

bool eventInit(EventState* unit) {
    const double frame = mir2::clamp(mir2::sanitize(IN0(3)), 0.001, 0.1);
    const double window = mir2::clamp(mir2::sanitize(IN0(2)), frame * 2.0, 10.0);
    unit->frameSamples = std::max(1, static_cast<int>(std::lround(frame * unit->mWorld->mSampleRate)));
    unit->step = unit->frameSamples / unit->mWorld->mSampleRate;
    unit->count = mir2::clamp(static_cast<int>(std::ceil(window / unit->step)), 2, 10000);
    unit->envelope = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->written = 0; unit->pending = 0; unit->squareSum = 0.0; unit->previousTrigger = 0.0;
    unit->recording = false; unit->result = {};
    ClearUnitOutputs(unit, 1);
    if (!unit->envelope || INRATE(0) != calc_FullRate) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    return true;
}

void eventProcess(EventState* unit) {
    const float* input = IN(0);
    const float* trigger = IN(1);
    const bool audioTrigger = INRATE(1) == calc_FullRate;
    for (int i = 0; i < unit->mWorld->mFullRate.mBufLength; ++i) {
        const double tr = mir2::sanitize(trigger[audioTrigger ? i : 0]);
        if (tr > 0.0 && unit->previousTrigger <= 0.0) {
            unit->recording = true; unit->written = 0; unit->pending = 0; unit->squareSum = 0.0;
        }
        unit->previousTrigger = tr;
        if (!unit->recording) continue;
        const double x = mir2::clamp(mir2::sanitize(input[i]), -1.0e6, 1.0e6);
        unit->squareSum += x * x;
        if (++unit->pending == unit->frameSamples) {
            unit->envelope[unit->written++] = std::sqrt(unit->squareSum / unit->frameSamples);
            unit->pending = 0; unit->squareSum = 0.0;
            if (unit->written == unit->count) {
                unit->result = mir2::measureAttack(unit->envelope, unit->count, unit->step, IN0(4), IN0(5));
                unit->recording = false;
            }
        }
    }
}

#define MIR2_EVENT(NAME, MEMBER) \
struct NAME : EventState {}; \
void NAME##_next(NAME* unit, int) { eventProcess(unit); ZOUT0(0) = static_cast<float>(unit->result.MEMBER); } \
void NAME##_Ctor(NAME* unit) { if (eventInit(unit)) SETCALC(NAME##_next); } \
void NAME##_Dtor(NAME* unit) { release(unit->mWorld, unit->envelope); }
MIR2_EVENT(AttackTime, attack)
MIR2_EVENT(AttackSlope2, slope)
MIR2_EVENT(DecayTime, decay)
#undef MIR2_EVENT

struct LogAttackTime : EventState {};
void LogAttackTime_next(LogAttackTime* unit, int) {
    eventProcess(unit);
    ZOUT0(0) = static_cast<float>(unit->result.attack);
    ZOUT0(1) = unit->result.validAttack ? static_cast<float>(std::log10(unit->result.attack)) : 0.0f;
}
void LogAttackTime_Ctor(LogAttackTime* unit) { if (eventInit(unit)) SETCALC(LogAttackTime_next); }
void LogAttackTime_Dtor(LogAttackTime* unit) { release(unit->mWorld, unit->envelope); }

} // namespace

namespace mir2plugin {
void registerTemporalUnits() {
    DefineDtorUnit(TemporalCentroid); DefineDtorUnit(EnvelopeSpread);
    DefineDtorUnit(EnvelopeSkewness); DefineDtorUnit(EnvelopeKurtosis);
    DefineDtorUnit(EffectiveDuration); DefineDtorUnit(SilenceRate);
    DefineDtorUnit(DynamicComplexity); DefineDtorUnit(StrongDecay);
    DefineDtorUnit(TemporalDecrease); DefineDtorUnit(FadeDetection);
    DefineDtorUnit(AttackTime); DefineDtorUnit(AttackSlope2);
    DefineDtorUnit(DecayTime); DefineDtorUnit(LogAttackTime);
}
} // namespace mir2plugin
