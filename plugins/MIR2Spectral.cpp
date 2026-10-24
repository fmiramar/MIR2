// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"

namespace {

using namespace mir2plugin;

constexpr int kMaxBands = 128;

inline bool validFFTSize(int size) noexcept {
    return mir2::isPowerOfTwo(size) && size >= 256 && size <= 8192;
}

inline bool allocateMagnitudes(Unit* unit, double*& magnitudes,
                               int fftSize) noexcept {
    magnitudes = nullptr;
    if (!validFFTSize(fftSize)) {
        Print("MIR2: fftSize must be a power of two from 256 through 8192\n");
        return false;
    }
    magnitudes = allocate<double>(unit->mWorld,
                                  static_cast<std::size_t>(fftSize / 2 + 1));
    if (magnitudes == nullptr) {
        Print("MIR2: real-time memory allocation failed\n");
        return false;
    }
    std::fill(magnitudes, magnitudes + fftSize / 2 + 1, 0.0);
    return true;
}

template <typename UnitType>
inline bool loadSpectrum(UnitType* unit) noexcept {
    SndBuf* buffer = resolveBuffer(unit, 0);
    if (buffer == nullptr)
        return false;
    LOCK_SNDBUF(buffer);
    return fillMagnitudes(buffer, unit->fftSize, unit->magnitudes);
}

struct SpecCentroid2 : Unit {
    double* magnitudes;
    int fftSize;
    float value;
};

void SpecCentroid2_next(SpecCentroid2* unit, int) {
    ZOUT0(0) = unit->value;
    if (IN0(0) < 0.0f || !loadSpectrum(unit))
        return;
    const double binHz = unit->mWorld->mSampleRate / static_cast<double>(unit->fftSize);
    unit->value = static_cast<float>(mir2::spectralCentroid(
        unit->magnitudes, unit->fftSize / 2 + 1, binHz,
        IN0(1), IN0(2), mir2::boundedInteger(IN0(3))));
    ZOUT0(0) = unit->value;
}

void SpecCentroid2_Ctor(SpecCentroid2* unit) {
    unit->magnitudes = nullptr;
    unit->fftSize = mir2::boundedInteger(IN0(4));
    unit->value = 0.0f;
    if (!allocateMagnitudes(unit, unit->magnitudes, unit->fftSize)) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(SpecCentroid2_next);
    ZOUT0(0) = 0.0f;
}

void SpecCentroid2_Dtor(SpecCentroid2* unit) {
    release(unit->mWorld, unit->magnitudes);
}

struct SpecFlatness2 : Unit {
    double* magnitudes;
    int fftSize;
    float value;
};

void SpecFlatness2_next(SpecFlatness2* unit, int) {
    ZOUT0(0) = unit->value;
    if (IN0(0) < 0.0f || !loadSpectrum(unit))
        return;
    const double binHz = unit->mWorld->mSampleRate / static_cast<double>(unit->fftSize);
    unit->value = static_cast<float>(mir2::spectralFlatness(
        unit->magnitudes, unit->fftSize / 2 + 1, binHz,
        IN0(1), IN0(2), mir2::boundedInteger(IN0(3)), IN0(4)));
    ZOUT0(0) = unit->value;
}

void SpecFlatness2_Ctor(SpecFlatness2* unit) {
    unit->magnitudes = nullptr;
    unit->fftSize = mir2::boundedInteger(IN0(5));
    unit->value = 0.0f;
    if (!allocateMagnitudes(unit, unit->magnitudes, unit->fftSize)) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(SpecFlatness2_next);
    ZOUT0(0) = 0.0f;
}

void SpecFlatness2_Dtor(SpecFlatness2* unit) {
    release(unit->mWorld, unit->magnitudes);
}

struct SpecPcile2 : Unit {
    double* magnitudes;
    int fftSize;
    float value;
};

void SpecPcile2_next(SpecPcile2* unit, int) {
    ZOUT0(0) = unit->value;
    if (IN0(0) < 0.0f || !loadSpectrum(unit))
        return;
    const double binHz = unit->mWorld->mSampleRate / static_cast<double>(unit->fftSize);
    unit->value = static_cast<float>(mir2::spectralPercentile(
        unit->magnitudes, unit->fftSize / 2 + 1, binHz, IN0(1),
        IN0(2), IN0(3), mir2::boundedInteger(IN0(5)), IN0(4) > 0.0f));
    ZOUT0(0) = unit->value;
}

void SpecPcile2_Ctor(SpecPcile2* unit) {
    unit->magnitudes = nullptr;
    unit->fftSize = mir2::boundedInteger(IN0(6));
    unit->value = 0.0f;
    if (!allocateMagnitudes(unit, unit->magnitudes, unit->fftSize)) {
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(SpecPcile2_next);
    ZOUT0(0) = 0.0f;
}

void SpecPcile2_Dtor(SpecPcile2* unit) {
    release(unit->mWorld, unit->magnitudes);
}

struct MFCC2 : Unit {
    double* magnitudes;
    double edges[kMaxBands + 2];
    double bands[kMaxBands];
    int fftSize;
    int numBands;
    int numCoeffs;
    int include0;
    int melMode;
    int spectrumMode;
    int orthonormal;
    double logFloor;
    double lifter;
};

void MFCC2_next(MFCC2* unit, int) {
    if (IN0(0) < 0.0f || !loadSpectrum(unit))
        return;
    const double binHz = unit->mWorld->mSampleRate / static_cast<double>(unit->fftSize);
    mir2::triangularBands(unit->magnitudes, unit->fftSize / 2 + 1, binHz,
                          unit->edges, unit->numBands, unit->spectrumMode,
                          unit->bands);
    for (int band = 0; band < unit->numBands; ++band)
        unit->bands[band] = std::log(std::max(unit->logFloor, unit->bands[band]));
    for (int output = 0; output < unit->numCoeffs; ++output) {
        const int coefficient = output + (unit->include0 != 0 ? 0 : 1);
        double value = mir2::dctII(unit->bands, unit->numBands, coefficient,
                                   unit->orthonormal != 0);
        if (unit->lifter > 0.0) {
            value *= 1.0 + 0.5 * unit->lifter
                * std::sin(mir2::kPi * static_cast<double>(coefficient) / unit->lifter);
        }
        ZOUT0(output) = static_cast<float>(mir2::sanitize(value));
    }
}

void MFCC2_Ctor(MFCC2* unit) {
    unit->magnitudes = nullptr;
    unit->numCoeffs = mir2::clamp(mir2::boundedInteger(IN0(1)), 1, 64);
    unit->numBands = mir2::clamp(mir2::boundedInteger(IN0(2)), 4, kMaxBands);
    unit->include0 = IN0(5) > 0.0f ? 1 : 0;
    unit->melMode = mir2::clamp(mir2::boundedInteger(IN0(6)), 0, 1);
    unit->spectrumMode = mir2::clamp(mir2::boundedInteger(IN0(7)), 0, 1);
    unit->logFloor = std::max(1.0e-30, static_cast<double>(IN0(8)));
    unit->orthonormal = IN0(9) > 0.0f ? 1 : 0;
    unit->lifter = std::max(0.0, static_cast<double>(IN0(10)));
    unit->fftSize = mir2::boundedInteger(IN0(11));
    const double nyquist = unit->mWorld->mSampleRate * 0.5;
    const double minFreq = mir2::clamp(static_cast<double>(IN0(3)), 0.0, nyquist);
    const double requestedMax = IN0(4);
    const double maxFreq = requestedMax <= 0.0
        ? nyquist : mir2::clamp(static_cast<double>(requestedMax), minFreq + 1.0, nyquist);
    const int highestCoefficient = unit->numCoeffs - 1 + (unit->include0 != 0 ? 0 : 1);
    if (static_cast<int>(unit->mNumOutputs) != unit->numCoeffs
        || highestCoefficient >= unit->numBands
        || !allocateMagnitudes(unit, unit->magnitudes, unit->fftSize)) {
        Print("MFCC2: invalid output, coefficient, band, or FFT configuration\n");
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    mir2::makeMelEdges(unit->edges, unit->numBands, minFreq, maxFreq,
                        unit->melMode);
    std::fill(unit->bands, unit->bands + kMaxBands, 0.0);
    SETCALC(MFCC2_next);
    for (int output = 0; output < static_cast<int>(unit->mNumOutputs); ++output)
        ZOUT0(output) = 0.0f;
}

void MFCC2_Dtor(MFCC2* unit) {
    release(unit->mWorld, unit->magnitudes);
}

struct FFTFlux2 : Unit {
    double* magnitudes;
    double* previous;
    int fftSize;
    int initialized;
    float value;
};

void FFTFlux2_next(FFTFlux2* unit, int) {
    ZOUT0(0) = unit->value;
    if (IN0(0) < 0.0f || !loadSpectrum(unit))
        return;
    const int numBins = unit->fftSize / 2 + 1;
    const double binHz = unit->mWorld->mSampleRate / static_cast<double>(unit->fftSize);
    int first = 0;
    int last = 0;
    mir2::frequencyBins(numBins, binHz, IN0(4), IN0(5), first, last);
    const int spectrumMode = mir2::clamp(mir2::boundedInteger(IN0(3)), 0, 1);
    for (int bin = first; bin <= last; ++bin)
        unit->magnitudes[bin] = mir2::spectralWeight(unit->magnitudes[bin], spectrumMode);
    if (unit->initialized != 0) {
        unit->value = static_cast<float>(mir2::spectralFlux(
            unit->magnitudes + first, unit->previous + first,
            last - first + 1, IN0(2) > 0.0f, IN0(1) > 0.0f));
    } else {
        unit->initialized = 1;
        unit->value = 0.0f;
    }
    std::copy(unit->magnitudes, unit->magnitudes + numBins, unit->previous);
    ZOUT0(0) = unit->value;
}

void FFTFlux2_Ctor(FFTFlux2* unit) {
    unit->magnitudes = nullptr;
    unit->previous = nullptr;
    unit->fftSize = mir2::boundedInteger(IN0(6));
    unit->initialized = 0;
    unit->value = 0.0f;
    if (allocateMagnitudes(unit, unit->magnitudes, unit->fftSize)) {
        unit->previous = allocate<double>(unit->mWorld,
            static_cast<std::size_t>(unit->fftSize / 2 + 1));
        if (unit->previous != nullptr)
            std::fill(unit->previous, unit->previous + unit->fftSize / 2 + 1, 0.0);
    }
    if (unit->magnitudes == nullptr || unit->previous == nullptr) {
        Print("FFTFlux2: real-time memory allocation failed\n");
        SETCALC(ClearUnitOutputs);
        unit->mDone = true;
        ClearUnitOutputs(unit, 1);
        return;
    }
    SETCALC(FFTFlux2_next);
    ZOUT0(0) = 0.0f;
}

void FFTFlux2_Dtor(FFTFlux2* unit) {
    release(unit->mWorld, unit->magnitudes);
    release(unit->mWorld, unit->previous);
}

} // namespace

namespace mir2plugin {

void registerSpectralUnits() {
    DefineDtorUnit(SpecCentroid2);
    DefineDtorUnit(SpecFlatness2);
    DefineDtorUnit(SpecPcile2);
    DefineDtorUnit(MFCC2);
    DefineDtorUnit(FFTFlux2);
}

} // namespace mir2plugin
