// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Wavelet.hpp"

namespace {
using namespace mir2plugin;
struct WaveletState : Unit {
    double* ring; double* work; double* scratch;
    int size, levels, position, filled, hop, countdown;
    bool analyzed;
    double bands[14], previous[14];
    mir2::WaveletFilter filter;
    mir2::WaveletMeasurements result;
};

bool waveletInit(WaveletState* unit) {
    unit->ring = nullptr; unit->work = nullptr; unit->scratch = nullptr;
    unit->size = static_cast<int>(mir2::clamp(mir2::sanitize(IN0(1)), 32.0, 8192.0));
    unit->levels = static_cast<int>(mir2::clamp(mir2::sanitize(IN0(3)), 1.0, static_cast<double>(mir2::waveletMaxLevels(unit->size))));
    unit->filter = mir2::daubechiesFilter(static_cast<int>(mir2::clamp(mir2::sanitize(IN0(2)), 1.0, 4.0)));
    unit->hop = mir2::clamp(static_cast<int>(std::lround(unit->size * mir2::clamp(mir2::sanitize(IN0(4)), 0.0625, 1.0))), 1, unit->size);
    unit->position = 0; unit->filled = 0; unit->countdown = unit->size; unit->result = {};
    unit->analyzed = false;
    std::fill(unit->bands, unit->bands + 14, 0.0); std::fill(unit->previous, unit->previous + 14, 0.0);
    ClearUnitOutputs(unit, 1);
    if (!mir2::isPowerOfTwo(unit->size) || INRATE(0) != calc_FullRate) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    unit->ring = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->work = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->scratch = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    if (!unit->ring || !unit->work || !unit->scratch) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return false;
    }
    return true;
}
void waveletDestroy(WaveletState* unit) {
    release(unit->mWorld, unit->ring); release(unit->mWorld, unit->work); release(unit->mWorld, unit->scratch);
}
void waveletProcess(WaveletState* unit) {
    const float* input = IN(0);
    for (int i = 0; i < unit->mWorld->mFullRate.mBufLength; ++i) {
        unit->ring[unit->position] = mir2::clamp(mir2::sanitize(input[i]), -1e6, 1e6);
        unit->position = (unit->position + 1) % unit->size;
        unit->filled = std::min(unit->size, unit->filled + 1);
        if (--unit->countdown <= 0 && unit->filled == unit->size) {
            for (int j = 0; j < unit->size; ++j) unit->work[j] = unit->ring[(unit->position + j) % unit->size];
            mir2::waveletForward(unit->work, unit->scratch, unit->size, unit->levels, unit->filter);
            mir2::waveletEnergies(unit->work, unit->size, unit->levels, unit->bands);
            unit->result = mir2::waveletMeasurements(unit->bands, unit->previous, unit->levels, unit->mWorld->mSampleRate);
            if (!unit->analyzed) unit->result.flux = 0.0;
            unit->analyzed = true;
            std::copy(unit->bands, unit->bands + unit->levels + 1, unit->previous);
            unit->countdown = unit->hop;
        }
    }
}
struct Wavelets2 : WaveletState {};
void Wavelets2_next(Wavelets2* unit, int) {
    waveletProcess(unit);
    const int mode = static_cast<int>(mir2::clamp(mir2::sanitize(IN0(5)), 0.0, 2.0));
    double total = 0.0;
    for (int i = 0; i <= unit->levels; ++i) total += unit->bands[i];
    for (int i = 0; i < static_cast<int>(unit->mNumOutputs); ++i) {
        const double energy = i <= unit->levels ? unit->bands[i] : 0.0;
        const double p = total > 1e-24 ? energy / total : 0.0;
        const double value = mode == 0 ? energy : (mode == 1 ? std::sqrt(energy) : (p > 0.0 ? -p * std::log2(p) : 0.0));
        OUT0(i) = static_cast<float>(value);
    }
}
void Wavelets2_Ctor(Wavelets2* unit) { if (waveletInit(unit)) SETCALC(Wavelets2_next); }
void Wavelets2_Dtor(Wavelets2* unit) { waveletDestroy(unit); }
struct WaveletBands : Wavelets2 {};
void WaveletBands_Ctor(WaveletBands* unit) { if (waveletInit(unit)) SETCALC(Wavelets2_next); }
void WaveletBands_Dtor(WaveletBands* unit) { waveletDestroy(unit); }

#define MIR2_WAVELET(NAME, MEMBER) \
struct NAME : WaveletState {}; \
void NAME##_next(NAME* unit, int) { waveletProcess(unit); ZOUT0(0) = static_cast<float>(unit->result.MEMBER); } \
void NAME##_Ctor(NAME* unit) { if (waveletInit(unit)) SETCALC(NAME##_next); } \
void NAME##_Dtor(NAME* unit) { waveletDestroy(unit); }
MIR2_WAVELET(WaveletEntropy, entropy)
MIR2_WAVELET(WaveletFlux, flux)
MIR2_WAVELET(WaveletCentroid, centroid)
MIR2_WAVELET(WaveletSpread, spread)
#undef MIR2_WAVELET

struct WaveletDaub2 : Unit {
    double* frame; double* output; double* scratch;
    int size, levels, position;
    mir2::WaveletFilter filter;
};
void WaveletDaub2_next(WaveletDaub2* unit, int samples) {
    const float* input = IN(0); float* output = OUT(0);
    const int remove = static_cast<int>(mir2::clamp(mir2::sanitize(IN0(4)), 0.0, static_cast<double>(unit->size)));
    for (int i = 0; i < samples; ++i) {
        output[i] = static_cast<float>(mir2::waveletReconstructSample(input[i], unit->frame,
            unit->output, unit->scratch, unit->position, unit->size, unit->levels, unit->filter, remove));
    }
}
void WaveletDaub2_Ctor(WaveletDaub2* unit) {
    unit->size = static_cast<int>(mir2::clamp(mir2::sanitize(IN0(1)), 32.0, 8192.0));
    unit->levels = static_cast<int>(mir2::clamp(mir2::sanitize(IN0(3)), 1.0, static_cast<double>(mir2::waveletMaxLevels(unit->size))));
    unit->filter = mir2::daubechiesFilter(static_cast<int>(mir2::clamp(mir2::sanitize(IN0(2)), 1.0, 4.0)));
    unit->position = 0; unit->frame = nullptr; unit->output = nullptr; unit->scratch = nullptr;
    ZOUT0(0) = 0.0f;
    if (!mir2::isPowerOfTwo(unit->size) || INRATE(0) != calc_FullRate) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return;
    }
    unit->frame = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->output = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    unit->scratch = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->size));
    if (!unit->frame || !unit->output || !unit->scratch) {
        SETCALC(ClearUnitOutputs); unit->mDone = true; return;
    }
    std::fill(unit->output, unit->output + unit->size, 0.0);
    SETCALC(WaveletDaub2_next);
}
void WaveletDaub2_Dtor(WaveletDaub2* unit) {
    release(unit->mWorld, unit->frame); release(unit->mWorld, unit->output); release(unit->mWorld, unit->scratch);
}
} // namespace

namespace mir2plugin {
void registerWaveletUnits() {
    DefineDtorUnit(WaveletDaub2); DefineDtorUnit(Wavelets2); DefineDtorUnit(WaveletBands);
    DefineDtorUnit(WaveletEntropy); DefineDtorUnit(WaveletFlux);
    DefineDtorUnit(WaveletCentroid); DefineDtorUnit(WaveletSpread);
}
} // namespace mir2plugin
