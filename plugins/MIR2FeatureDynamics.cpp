// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/FeatureDynamics.hpp"

namespace {
using namespace mir2plugin;
struct FeatureState : Unit {
    double* history;
    double* scratch;
    double* deltaHistory;
    int radius, count, position, filled, deltaFilled, deltaPosition;
    double result, previousTrigger;
};

bool featureInit(FeatureState* unit, bool median) {
    unit->radius = mir2::clamp(mir2::boundedInteger(IN0(1)), 1, median ? 127 : 32);
    unit->count = median ? unit->radius : 2 * unit->radius + 1;
    unit->history = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->scratch = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->deltaHistory = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->position = 0; unit->filled = 0; unit->deltaFilled = 0; unit->deltaPosition = 0;
    unit->result = 0.0; unit->previousTrigger = 0.0;
    ZOUT0(0) = 0.0f;
    if (!unit->history || !unit->scratch || !unit->deltaHistory) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    return true;
}

void featureDestroy(FeatureState* unit) {
    release(unit->mWorld, unit->history); release(unit->mWorld, unit->scratch);
    release(unit->mWorld, unit->deltaHistory);
}

// Negative trigger selects every control block. Nonnegative trigger samples only rising edges.
bool featureTick(FeatureState* unit) {
    const double trigger = mir2::sanitize(IN0(2));
    const bool tick = trigger < 0.0 || (trigger > 0.0 && unit->previousTrigger <= 0.0);
    unit->previousTrigger = trigger;
    return tick;
}

void featureProcess(FeatureState* unit, int method) {
    if (!featureTick(unit)) return;
    unit->history[unit->position] = mir2::sanitize(IN0(0));
    unit->position = (unit->position + 1) % unit->count;
    unit->filled = std::min(unit->count, unit->filled + 1);
    if (unit->filled < unit->count) return;
    for (int i = 0; i < unit->count; ++i)
        unit->scratch[i] = unit->history[(unit->position + i) % unit->count];
    if (method == 2) {
        unit->result = mir2::featureMedian(unit->scratch, unit->count); return;
    }
    const double requested = mir2::sanitize(IN0(3));
    const double step = requested > 0.0 ? requested : unit->mWorld->mFullRate.mBufLength / unit->mWorld->mSampleRate;
    const double delta = mir2::regressionDelta(unit->scratch, unit->radius, step);
    if (method == 0) { unit->result = delta; return; }
    unit->deltaHistory[unit->deltaPosition] = delta;
    unit->deltaPosition = (unit->deltaPosition + 1) % unit->count;
    unit->deltaFilled = std::min(unit->count, unit->deltaFilled + 1);
    if (unit->deltaFilled < unit->count) return;
    for (int i = 0; i < unit->count; ++i)
        unit->scratch[i] = unit->deltaHistory[(unit->deltaPosition + i) % unit->count];
    unit->result = mir2::regressionDelta(unit->scratch, unit->radius, step);
}

#define MIR2_FEATURE(NAME, METHOD) \
struct NAME : FeatureState {}; \
void NAME##_next(NAME* unit, int) { featureProcess(unit, METHOD); ZOUT0(0) = static_cast<float>(unit->result); } \
void NAME##_Ctor(NAME* unit) { if (featureInit(unit, METHOD == 2)) SETCALC(NAME##_next); } \
void NAME##_Dtor(NAME* unit) { featureDestroy(unit); }
MIR2_FEATURE(FeatureDelta, 0)
MIR2_FEATURE(FeatureDeltaDelta, 1)
MIR2_FEATURE(FeatureMedian, 2)
#undef MIR2_FEATURE

struct FeatureSmooth : Unit { double previous, trigger; bool initialized; };
void FeatureSmooth_next(FeatureSmooth* unit, int) {
    const double trigger = mir2::sanitize(IN0(2));
    const bool tick = trigger < 0.0 || (trigger > 0.0 && unit->trigger <= 0.0);
    unit->trigger = trigger;
    if (tick) {
        const double requested = mir2::sanitize(IN0(3));
        const double step = requested > 0.0 ? requested : unit->mWorld->mFullRate.mBufLength / unit->mWorld->mSampleRate;
        unit->previous = unit->initialized
            ? mir2::smoothFeature(unit->previous, IN0(0), mir2::sanitize(IN0(1)), step)
            : mir2::sanitize(IN0(0));
        unit->initialized = true;
    }
    ZOUT0(0) = static_cast<float>(unit->previous);
}
void FeatureSmooth_Ctor(FeatureSmooth* unit) {
    unit->previous = 0.0; unit->trigger = 0.0; unit->initialized = false;
    SETCALC(FeatureSmooth_next); ZOUT0(0) = 0.0f;
}
} // namespace

namespace mir2plugin {
void registerFeatureDynamicsUnits() {
    DefineDtorUnit(FeatureDelta); DefineDtorUnit(FeatureDeltaDelta);
    DefineDtorUnit(FeatureMedian); DefineSimpleUnit(FeatureSmooth);
}
} // namespace mir2plugin
