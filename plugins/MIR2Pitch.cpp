// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"

namespace {

using namespace mir2plugin;

inline bool validPitchConfiguration(Unit* unit, PitchState& state,
                                    int frameSize, double hop,
                                    double minFreq, double maxFreq) noexcept {
    if (minFreq <= 0.0 || maxFreq <= minFreq
        || maxFreq >= unit->mWorld->mSampleRate * 0.5
        || !initializePitchState(unit, state, frameSize, hop)) {
        Print("MIR2 pitch analyzer: invalid frequency, frameSize, hop, or RT memory configuration\n");
        destroyPitchState(unit->mWorld, state);
        return false;
    }
    return true;
}

struct PitchYIN : Unit {
    PitchState state;
};

void PitchYIN_next(PitchYIN* unit, int inNumSamples) {
    processPitchInput(unit, unit->state, inNumSamples, IN0(1), IN0(2),
                      IN0(5), PitchAlgorithm::yin);
    ZOUT0(0) = static_cast<float>(unit->state.result.frequency);
    ZOUT0(1) = static_cast<float>(unit->state.result.confidence);
    ZOUT0(2) = static_cast<float>(unit->state.result.voiced);
}

void PitchYIN_Ctor(PitchYIN* unit) {
    clearPitchState(unit->state);
    if (!validPitchConfiguration(unit, unit->state, mir2::boundedInteger(IN0(3)),
                                 IN0(4), IN0(1), IN0(2))) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(PitchYIN_next);
    ClearUnitOutputs(unit, 1);
}

void PitchYIN_Dtor(PitchYIN* unit) {
    destroyPitchState(unit->mWorld, unit->state);
}

struct PitchMPM : Unit {
    PitchState state;
};

void PitchMPM_next(PitchMPM* unit, int inNumSamples) {
    processPitchInput(unit, unit->state, inNumSamples, IN0(1), IN0(2),
                      IN0(5), PitchAlgorithm::mpm);
    ZOUT0(0) = static_cast<float>(unit->state.result.frequency);
    ZOUT0(1) = static_cast<float>(unit->state.result.confidence);
    ZOUT0(2) = static_cast<float>(unit->state.result.voiced);
}

void PitchMPM_Ctor(PitchMPM* unit) {
    clearPitchState(unit->state);
    if (!validPitchConfiguration(unit, unit->state, mir2::boundedInteger(IN0(3)),
                                 IN0(4), IN0(1), IN0(2))) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(PitchMPM_next);
    ClearUnitOutputs(unit, 1);
}

void PitchMPM_Dtor(PitchMPM* unit) {
    destroyPitchState(unit->mWorld, unit->state);
}

struct Periodicity : Unit {
    PitchState state;
};

void Periodicity_next(Periodicity* unit, int inNumSamples) {
    const int method = mir2::clamp(mir2::boundedInteger(IN0(5)), 0, 1);
    const PitchAlgorithm algorithm = method == 0 ? PitchAlgorithm::mpm : PitchAlgorithm::yin;
    const double parameter = algorithm == PitchAlgorithm::mpm ? 0.93 : 0.1;
    processPitchInput(unit, unit->state, inNumSamples, IN0(1), IN0(2),
                      parameter, algorithm);
    ZOUT0(0) = static_cast<float>(mir2::clamp(
        unit->state.result.periodicity, 0.0, 1.0));
}

void Periodicity_Ctor(Periodicity* unit) {
    clearPitchState(unit->state);
    if (!validPitchConfiguration(unit, unit->state, mir2::boundedInteger(IN0(3)),
                                 IN0(4), IN0(1), IN0(2))) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(Periodicity_next);
    ZOUT0(0) = 0.0f;
}

void Periodicity_Dtor(Periodicity* unit) {
    destroyPitchState(unit->mWorld, unit->state);
}

struct Voicing : Unit {
    PitchState state;
};

void Voicing_next(Voicing* unit, int inNumSamples) {
    processPitchInput(unit, unit->state, inNumSamples, IN0(3), IN0(4),
                      0.93, PitchAlgorithm::mpm);
    const int method = mir2::clamp(mir2::boundedInteger(IN0(5)), 0, 1);
    const double probability = method == 0
        ? unit->state.result.periodicity : unit->state.voicingProbability;
    const double cleanProbability = mir2::clamp(probability, 0.0, 1.0);
    ZOUT0(0) = cleanProbability > 0.0
        && cleanProbability >= mir2::clamp(static_cast<double>(IN0(6)), 0.0, 1.0)
        ? 1.0f : 0.0f;
    ZOUT0(1) = static_cast<float>(cleanProbability);
}

void Voicing_Ctor(Voicing* unit) {
    clearPitchState(unit->state);
    if (!validPitchConfiguration(unit, unit->state, mir2::boundedInteger(IN0(1)),
                                 IN0(2), IN0(3), IN0(4))) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(Voicing_next);
    ClearUnitOutputs(unit, 1);
}

void Voicing_Dtor(Voicing* unit) {
    destroyPitchState(unit->mWorld, unit->state);
}

struct Pitch2 : Unit {
    PitchState state;
    double smoothedFrequency;
    double heldFrequency;
};

void Pitch2_next(Pitch2* unit, int inNumSamples) {
    const int algorithmIndex = mir2::clamp(mir2::boundedInteger(IN0(5)), 0, 1);
    const PitchAlgorithm algorithm = algorithmIndex == 0
        ? PitchAlgorithm::yin : PitchAlgorithm::mpm;
    double parameter = IN0(6);
    if (parameter < 0.0)
        parameter = algorithm == PitchAlgorithm::yin ? 0.1 : 0.93;
    processPitchInput(unit, unit->state, inNumSamples, IN0(1), IN0(2),
                      parameter, algorithm);
    if (unit->state.result.frequency > 0.0)
        unit->heldFrequency = unit->state.result.frequency;
    const double target = unit->state.result.frequency > 0.0
        ? unit->state.result.frequency : (IN0(8) > 0.0f ? unit->heldFrequency : 0.0);
    const double smoothTime = std::max(0.0, static_cast<double>(IN0(7)));
    if (smoothTime <= 0.0 || unit->smoothedFrequency <= 0.0 || target <= 0.0) {
        unit->smoothedFrequency = target;
    } else {
        const int sampleCount = INRATE(0) == calc_FullRate
            ? unit->mWorld->mFullRate.mBufLength : inNumSamples;
        const double blockTime = static_cast<double>(sampleCount)
            / unit->mWorld->mSampleRate;
        const double coefficient = std::exp(-blockTime / smoothTime);
        const double logFrequency = coefficient * std::log(unit->smoothedFrequency)
            + (1.0 - coefficient) * std::log(target);
        unit->smoothedFrequency = std::exp(logFrequency);
    }
    ZOUT0(0) = static_cast<float>(mir2::sanitize(unit->smoothedFrequency));
    ZOUT0(1) = static_cast<float>(unit->state.result.confidence);
    ZOUT0(2) = static_cast<float>(unit->state.result.voiced);
    ZOUT0(3) = static_cast<float>(unit->state.result.octaveAmbiguity);
}

void Pitch2_Ctor(Pitch2* unit) {
    clearPitchState(unit->state);
    unit->smoothedFrequency = 0.0;
    unit->heldFrequency = 0.0;
    if (!validPitchConfiguration(unit, unit->state, mir2::boundedInteger(IN0(3)),
                                 IN0(4), IN0(1), IN0(2))) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(Pitch2_next);
    ClearUnitOutputs(unit, 1);
}

void Pitch2_Dtor(Pitch2* unit) {
    destroyPitchState(unit->mWorld, unit->state);
}

} // namespace

namespace mir2plugin {

void registerPitchUnits() {
    DefineDtorUnit(PitchYIN);
    DefineDtorUnit(PitchMPM);
    DefineDtorUnit(Periodicity);
    DefineDtorUnit(Voicing);
    DefineDtorUnit(Pitch2);
}

} // namespace mir2plugin
