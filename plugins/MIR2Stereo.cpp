// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Stereo.hpp"

namespace {
using namespace mir2plugin;

struct StereoTimeState : Unit {
    double* left;
    double* right;
    int count, position, filled;
    mir2::StereoSums sums;
};

bool stereoTimeInit(StereoTimeState* unit) {
    const double seconds = mir2::clamp(mir2::sanitize(IN0(2)), 0.001, 2.0);
    unit->count = std::max(2, static_cast<int>(std::lround(seconds * unit->mWorld->mSampleRate)));
    unit->position = 0; unit->filled = 0; unit->sums = {};
    unit->left = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    unit->right = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->count));
    ZOUT0(0) = 0.0f;
    if (!unit->left || !unit->right || INRATE(0) != calc_FullRate || INRATE(1) != calc_FullRate) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    std::fill(unit->left, unit->left + unit->count, 0.0);
    std::fill(unit->right, unit->right + unit->count, 0.0);
    return true;
}

mir2::StereoMeasurements stereoTimeProcess(StereoTimeState* unit) {
    const float* left = IN(0); const float* right = IN(1);
    for (int i = 0; i < unit->mWorld->mFullRate.mBufLength; ++i) {
        const double l = mir2::clamp(mir2::sanitize(left[i]), -1.0e6, 1.0e6);
        const double r = mir2::clamp(mir2::sanitize(right[i]), -1.0e6, 1.0e6);
        unit->sums.add(unit->left[unit->position], unit->right[unit->position], -1.0);
        unit->sums.add(l, r);
        unit->left[unit->position] = l; unit->right[unit->position] = r;
        unit->position = (unit->position + 1) % unit->count;
        unit->filled = std::min(unit->count, unit->filled + 1);
        // Rebase once per window to prevent indefinite subtract/add roundoff drift.
        if (unit->position == 0) {
            unit->sums = {};
            for (int j = 0; j < unit->count; ++j) unit->sums.add(unit->left[j], unit->right[j]);
        }
    }
    if (unit->filled < unit->count) return {0.0, 0.0, 0.0, 0.0};
    return mir2::stereoMeasurements(unit->sums, unit->count);
}

void stereoTimeDestroy(StereoTimeState* unit) {
    release(unit->mWorld, unit->left); release(unit->mWorld, unit->right);
}

#define MIR2_STEREO_TIME(NAME, MEMBER) \
struct NAME : StereoTimeState {}; \
void NAME##_next(NAME* unit, int) { ZOUT0(0) = static_cast<float>(stereoTimeProcess(unit).MEMBER); } \
void NAME##_Ctor(NAME* unit) { if (stereoTimeInit(unit)) SETCALC(NAME##_next); } \
void NAME##_Dtor(NAME* unit) { stereoTimeDestroy(unit); }
MIR2_STEREO_TIME(StereoBalance, balance)
MIR2_STEREO_TIME(StereoWidth, width)
MIR2_STEREO_TIME(StereoCorrelation, correlation)
MIR2_STEREO_TIME(MidSideRatio, midSideDB)
#undef MIR2_STEREO_TIME

struct StereoFrequencyState : Unit {
    double* left;
    double* right;
    double* window;
    mir2::Complex* fftLeft;
    mir2::Complex* fftRight;
    mir2::StereoBin* bins;
    int size, position, filled, hop, countdown, frames;
    mir2::StereoSpectralMeasurements result;
};

bool stereoFrequencyInit(StereoFrequencyState* unit) {
    unit->left = nullptr; unit->right = nullptr; unit->window = nullptr;
    unit->fftLeft = nullptr; unit->fftRight = nullptr; unit->bins = nullptr;
    unit->size = mir2::clamp(static_cast<int>(mir2::sanitize(IN0(2))), 256, 8192);
    unit->hop = mir2::clamp(static_cast<int>(std::lround(unit->size * mir2::clamp(mir2::sanitize(IN0(3)), 0.0625, 1.0))), 1, unit->size);
    unit->position = 0; unit->filled = 0; unit->countdown = unit->size; unit->frames = 0; unit->result = {};
    ClearUnitOutputs(unit, 1);
    if (!mir2::isPowerOfTwo(unit->size) || INRATE(0) != calc_FullRate || INRATE(1) != calc_FullRate) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    const auto count = static_cast<std::size_t>(unit->size);
    unit->left = allocate<double>(unit->mWorld, count);
    unit->right = allocate<double>(unit->mWorld, count);
    unit->window = allocate<double>(unit->mWorld, count);
    unit->fftLeft = allocate<mir2::Complex>(unit->mWorld, count);
    unit->fftRight = allocate<mir2::Complex>(unit->mWorld, count);
    unit->bins = allocate<mir2::StereoBin>(unit->mWorld, count / 2 + 1);
    if (!unit->left || !unit->right || !unit->window || !unit->fftLeft || !unit->fftRight || !unit->bins) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    for (int i = 0; i < unit->size; ++i) unit->window[i] = 0.5 - 0.5 * std::cos(mir2::kTwoPi * i / unit->size);
    for (int i = 0; i <= unit->size / 2; ++i) unit->bins[i] = {};
    return true;
}

void stereoFrequencyAnalyze(StereoFrequencyState* unit) {
    for (int i = 0; i < unit->size; ++i) {
        const int source = (unit->position + i) % unit->size;
        unit->fftLeft[i] = { unit->left[source] * unit->window[i], 0.0 };
        unit->fftRight[i] = { unit->right[source] * unit->window[i], 0.0 };
    }
    mir2::fft(unit->fftLeft, unit->size, false); mir2::fft(unit->fftRight, unit->size, false);
    unit->frames = std::min(unit->frames + 1, 1000000);
    const double average = mir2::clamp(mir2::sanitize(IN0(4)), 0.01, 10.0);
    const double retention = std::min(std::exp(-unit->hop / (unit->mWorld->mSampleRate * average)),
                                      static_cast<double>(unit->frames - 1) / unit->frames);
    for (int i = 0; i <= unit->size / 2; ++i)
        mir2::averageStereoBin(unit->bins[i], unit->fftLeft[i], unit->fftRight[i], retention);
    const double binWidth = unit->mWorld->mSampleRate / unit->size;
    const double minimum = mir2::clamp(mir2::sanitize(IN0(5)), 0.0, unit->mWorld->mSampleRate * 0.5);
    const double requestedMax = mir2::sanitize(IN0(6));
    const double maximum = requestedMax <= 0.0 ? unit->mWorld->mSampleRate * 0.5
        : mir2::clamp(requestedMax, 0.0, unit->mWorld->mSampleRate * 0.5);
    const int first = mir2::clamp(static_cast<int>(std::ceil(minimum / binWidth)), 0, unit->size / 2);
    const int last = mir2::clamp(static_cast<int>(std::floor(maximum / binWidth)), 0, unit->size / 2);
    unit->result = mir2::stereoSpectralMeasurements(unit->bins, first, last);
    // A single periodogram has identically unit coherence: withhold it until four frames exist.
    if (unit->frames < 4) unit->result.coherence = 0.0;
}

void stereoFrequencyProcess(StereoFrequencyState* unit) {
    const float* left = IN(0); const float* right = IN(1);
    for (int i = 0; i < unit->mWorld->mFullRate.mBufLength; ++i) {
        unit->left[unit->position] = mir2::clamp(mir2::sanitize(left[i]), -1.0e6, 1.0e6);
        unit->right[unit->position] = mir2::clamp(mir2::sanitize(right[i]), -1.0e6, 1.0e6);
        unit->position = (unit->position + 1) % unit->size;
        unit->filled = std::min(unit->size, unit->filled + 1);
        if (--unit->countdown <= 0 && unit->filled == unit->size) {
            stereoFrequencyAnalyze(unit); unit->countdown = unit->hop;
        }
    }
}

void stereoFrequencyDestroy(StereoFrequencyState* unit) {
    release(unit->mWorld, unit->left); release(unit->mWorld, unit->right);
    release(unit->mWorld, unit->window); release(unit->mWorld, unit->fftLeft);
    release(unit->mWorld, unit->fftRight); release(unit->mWorld, unit->bins);
}

#define MIR2_STEREO_FREQUENCY(NAME, MEMBER) \
struct NAME : StereoFrequencyState {}; \
void NAME##_next(NAME* unit, int) { stereoFrequencyProcess(unit); ZOUT0(0) = static_cast<float>(unit->result.MEMBER); } \
void NAME##_Ctor(NAME* unit) { if (stereoFrequencyInit(unit)) SETCALC(NAME##_next); } \
void NAME##_Dtor(NAME* unit) { stereoFrequencyDestroy(unit); }
MIR2_STEREO_FREQUENCY(StereoCoherence, coherence)
MIR2_STEREO_FREQUENCY(StereoPanning, panning)
#undef MIR2_STEREO_FREQUENCY

struct InterchannelPhase : StereoFrequencyState {};
void InterchannelPhase_next(InterchannelPhase* unit, int) {
    stereoFrequencyProcess(unit);
    ZOUT0(0) = static_cast<float>(unit->result.phase);
    ZOUT0(1) = static_cast<float>(unit->result.phaseConcentration);
}
void InterchannelPhase_Ctor(InterchannelPhase* unit) { if (stereoFrequencyInit(unit)) SETCALC(InterchannelPhase_next); }
void InterchannelPhase_Dtor(InterchannelPhase* unit) { stereoFrequencyDestroy(unit); }
} // namespace

namespace mir2plugin {
void registerStereoUnits() {
    DefineDtorUnit(StereoBalance); DefineDtorUnit(StereoWidth);
    DefineDtorUnit(StereoCorrelation); DefineDtorUnit(MidSideRatio);
    DefineDtorUnit(StereoCoherence); DefineDtorUnit(StereoPanning);
    DefineDtorUnit(InterchannelPhase);
}
} // namespace mir2plugin
