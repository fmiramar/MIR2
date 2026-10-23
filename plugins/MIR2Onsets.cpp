// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Onset.hpp"

namespace {
using namespace mir2plugin;
struct Onsets2 : Unit {
    mir2::OnsetState state;
    double* memory;
    double* spectrum;
    double* phases;
    float strength;
};
void Onsets2_next(Onsets2* unit, int) {
    ZOUT0(0) = 0.f;
    ZOUT0(1) = unit->strength;
    SndBuf* buffer = resolveBuffer(unit, 0);
    if (!buffer) return;
    {
        LOCK_SNDBUF(buffer);
        if (!readFFTFrame(buffer, unit->state.config.fftSize, unit->spectrum, unit->phases)) return;
    }
    mir2::OnsetControls controls;
    controls.threshold = mir2::sanitize(IN0(1));
    controls.relaxTime = mir2::sanitize(IN0(3));
    controls.minGap = mir2::sanitize(IN0(4));
    controls.maxFilterWidth = mir2::clamp(static_cast<int>(mir2::sanitize(IN0(6))), 0, 25);
    controls.floor = mir2::sanitize(IN0(7));
    controls.whitening = IN0(10) > 0.f;
    controls.logGain = mir2::sanitize(IN0(11));
    controls.silenceThreshold = mir2::sanitize(IN0(12));
    controls.noveltyFloor = mir2::sanitize(IN0(14));
    controls.releaseTime = mir2::sanitize(IN0(15));
    const auto result = unit->state.process(unit->spectrum, unit->phases, controls);
    unit->strength = static_cast<float>(result.strength);
    ZOUT0(0) = static_cast<float>(result.trigger);
    ZOUT0(1) = unit->strength;
}
void Onsets2_Ctor(Onsets2* unit) {
    unit->memory = unit->spectrum = unit->phases = nullptr;
    unit->strength = 0.f;
    mir2::OnsetConfig config;
    config.fftSize = static_cast<int>(mir2::sanitize(IN0(8)));
    config.sampleRate = unit->mWorld->mSampleRate;
    config.hop = config.fftSize > 0
        ? static_cast<double>(mir2::onsetHopSamples(config.fftSize, IN0(9), unit->mWorld->mFullRate.mBufLength)) / config.fftSize
        : 0.5;
    config.medianTime = mir2::clamp(static_cast<double>(mir2::sanitize(IN0(5))), 0.0, 10.0);
    config.lag = mir2::clamp(static_cast<int>(mir2::sanitize(IN0(13))), 0, 16);
    config.method = static_cast<mir2::OnsetMethod>(mir2::clamp(static_cast<int>(mir2::sanitize(IN0(2))), 0, 10));
    const bool valid = mir2::isPowerOfTwo(config.fftSize) && config.fftSize >= 256 && config.fftSize <= 8192
        && config.fftSize >= unit->mWorld->mFullRate.mBufLength
        && mir2::OnsetState::storageCount(config) > 0;
    if (valid) {
        unit->memory = allocate<double>(unit->mWorld, mir2::OnsetState::storageCount(config));
        unit->spectrum = allocate<double>(unit->mWorld, static_cast<std::size_t>(2 * (config.fftSize / 2 + 1)));
    }
    if (!unit->memory || !unit->spectrum) {
        Print("Onsets2: invalid FFT/history size or insufficient real-time memory\n");
        SETCALC(ClearUnitOutputs); unit->mDone = true; ClearUnitOutputs(unit, 1); return;
    }
    unit->phases = unit->spectrum + config.fftSize / 2 + 1;
    unit->state.initialize(config, unit->memory);
    SETCALC(Onsets2_next);
    ZOUT0(0) = ZOUT0(1) = 0.f;
}
void Onsets2_Dtor(Onsets2* unit) {
    release(unit->mWorld, unit->memory); release(unit->mWorld, unit->spectrum);
}

struct PhaseFeature : Unit {
    double* memory;
    int fftSize, frames, kind;
    float value;
};
struct FFTComplexDev2 : PhaseFeature {};
struct FFTPhaseDev2 : PhaseFeature {};
struct FFTMKL2 : PhaseFeature {};
void PhaseFeature_next(PhaseFeature* unit, int) {
    ZOUT0(0) = unit->value;
    SndBuf* buffer = resolveBuffer(unit, 0);
    if (!buffer) return;
    const int bins = unit->fftSize / 2 + 1;
    double* magnitude = unit->memory;
    double* phase = magnitude + bins;
    {
        LOCK_SNDBUF(buffer);
        if (!readFFTFrame(buffer, unit->fftSize, magnitude, phase)) return;
    }
    const auto method = unit->kind == 0
        ? (IN0(1) > 0.f ? mir2::OnsetMethod::rectifiedComplex : mir2::OnsetMethod::complex)
        : unit->kind == 1
        ? (IN0(1) > 0.f ? mir2::OnsetMethod::weightedPhase : mir2::OnsetMethod::phase)
        : mir2::OnsetMethod::modifiedKL;
    const double floor = std::max(1.e-20, static_cast<double>(mir2::sanitize(IN0(3))));
    unit->value = static_cast<float>(mir2::onsetEvidence(method, magnitude, phase,
        phase + bins, phase + 2 * bins, phase + 3 * bins, bins,
        floor, IN0(2) > 0.f, unit->frames >= 2));
    unit->frames = std::min(3, unit->frames + 1);
    ZOUT0(0) = unit->value;
}
void initializePhaseFeature(PhaseFeature* unit, int kind) {
    unit->memory = nullptr; unit->frames = 0; unit->kind = kind; unit->value = 0.f;
    unit->fftSize = static_cast<int>(mir2::sanitize(IN0(4)));
    if (mir2::isPowerOfTwo(unit->fftSize) && unit->fftSize >= 256 && unit->fftSize <= 8192)
        unit->memory = allocate<double>(unit->mWorld, static_cast<std::size_t>(5 * (unit->fftSize / 2 + 1)));
    if (!unit->memory) {
        Print("MIR2 phase feature: invalid FFT/history size or insufficient real-time memory\n");
        SETCALC(ClearUnitOutputs); unit->mDone = true; ClearUnitOutputs(unit, 1); return;
    }
    std::fill(unit->memory, unit->memory + 5 * (unit->fftSize / 2 + 1), 0.0);
    SETCALC(PhaseFeature_next); ZOUT0(0) = 0.f;
}
void FFTComplexDev2_Ctor(FFTComplexDev2* unit) { initializePhaseFeature(unit, 0); }
void FFTPhaseDev2_Ctor(FFTPhaseDev2* unit) { initializePhaseFeature(unit, 1); }
void FFTMKL2_Ctor(FFTMKL2* unit) { initializePhaseFeature(unit, 2); }
void FFTComplexDev2_Dtor(FFTComplexDev2* unit) { release(unit->mWorld, unit->memory); }
void FFTPhaseDev2_Dtor(FFTPhaseDev2* unit) { release(unit->mWorld, unit->memory); }
void FFTMKL2_Dtor(FFTMKL2* unit) { release(unit->mWorld, unit->memory); }
} // namespace
namespace mir2plugin {
void registerOnsetUnits() {
    DefineDtorUnit(Onsets2);
    DefineDtorUnit(FFTComplexDev2);
    DefineDtorUnit(FFTPhaseDev2);
    DefineDtorUnit(FFTMKL2);
}
} // namespace mir2plugin
