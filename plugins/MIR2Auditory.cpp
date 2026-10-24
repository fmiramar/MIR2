// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Auditory.hpp"

namespace {
using namespace mir2plugin;
constexpr int kMaxAuditoryBands = 128;
struct AuditoryFFTUnit : Unit {
    double* magnitudes;
    double* weights;
    double bands[kMaxAuditoryBands];
    int fftSize, numBands, numCoeffs, include0, spectrumMode;
    bool orthonormal;
    double floor, lifter;
};

bool initializeAuditoryFFT(AuditoryFFTUnit* unit, mir2::AuditoryScale scale) {
    unit->magnitudes = unit->weights = nullptr;
    unit->fftSize = mir2::boundedInteger(IN0(11));
    unit->numCoeffs = mir2::boundedInteger(IN0(1));
    unit->numBands = mir2::boundedInteger(IN0(2));
    unit->include0 = IN0(7) > 0.0f ? 1 : 0;
    unit->spectrumMode = mir2::boundedInteger(IN0(6));
    unit->floor = std::max(1.0e-30, static_cast<double>(IN0(8)));
    unit->orthonormal = IN0(9) > 0.0f;
    unit->lifter = std::max(0.0, static_cast<double>(IN0(10)));
    if (!mir2::isPowerOfTwo(unit->fftSize) || unit->fftSize < 256 || unit->fftSize > 8192
        || unit->numBands < 4 || unit->numBands > kMaxAuditoryBands
        || unit->numCoeffs < 1 || unit->numCoeffs > unit->numBands
        || static_cast<int>(unit->mNumOutputs) != unit->numCoeffs) return false;
    const int bins = unit->fftSize / 2 + 1;
    unit->magnitudes = allocate<double>(unit->mWorld, static_cast<std::size_t>(bins));
    unit->weights = allocate<double>(unit->mWorld, static_cast<std::size_t>(bins * unit->numBands));
    if (!unit->magnitudes || !unit->weights) return false;
    const double nyquist = unit->mWorld->mSampleRate * 0.5;
    const double low = mir2::clamp(static_cast<double>(IN0(3)), 0.0, nyquist - 1.0);
    const double requestedHigh = IN0(4);
    const double high = requestedHigh <= 0.0 ? nyquist : mir2::clamp(requestedHigh, low + 1.0, nyquist);
    double edges[kMaxAuditoryBands + 2];
    mir2::makeAuditoryEdges(edges, unit->numBands, low, high, scale);
    mir2::makeAuditoryWeights(unit->weights, bins, unit->mWorld->mSampleRate / unit->fftSize,
        edges, unit->numBands, IN0(5) > 0.0f);
    return true;
}
void failAuditory(Unit* unit) {
    Print("MIR2 auditory: invalid dimensions or insufficient real-time memory\n");
    SETCALC(ClearUnitOutputs); unit->mDone = true; ClearUnitOutputs(unit, 1);
}
void destroyAuditoryFFT(AuditoryFFTUnit* unit) {
    release(unit->mWorld, unit->magnitudes); release(unit->mWorld, unit->weights);
}

template<bool cepstrum>
void auditoryFFTNext(AuditoryFFTUnit* unit, int) {
    if (IN0(0) < 0.0f) return;
    SndBuf* buffer = resolveBuffer(unit, 0);
    if (!buffer) return;
    { LOCK_SNDBUF(buffer); if (!fillMagnitudes(buffer, unit->fftSize, unit->magnitudes)) return; }
    mir2::auditoryBands(unit->magnitudes, unit->fftSize / 2 + 1, unit->weights,
        unit->numBands, unit->spectrumMode, unit->bands);
    if (cepstrum) {
        for (int band = 0; band < unit->numBands; ++band)
            unit->bands[band] = std::log(std::max(unit->floor, unit->bands[band]));
        for (int out = 0; out < unit->numCoeffs; ++out) {
            const int coefficient = out + (unit->include0 ? 0 : 1);
            double value = mir2::dctII(unit->bands, unit->numBands, coefficient, unit->orthonormal);
            if (unit->lifter > 0.0) value *= 1.0 + 0.5 * unit->lifter * std::sin(mir2::kPi * coefficient / unit->lifter);
            ZOUT0(out) = static_cast<float>(mir2::sanitize(value));
        }
    } else {
        for (int band = 0; band < unit->numBands; ++band) ZOUT0(band) = static_cast<float>(unit->bands[band]);
    }
}
#define MIR2_AUDITORY(Name, Scale, Cepstrum) \
    struct Name : AuditoryFFTUnit {}; \
    void Name##_Ctor(Name* unit) { \
        if (!initializeAuditoryFFT(unit, mir2::AuditoryScale::Scale) \
            || (Cepstrum && unit->numCoeffs + (unit->include0 ? 0 : 1) > unit->numBands)) { failAuditory(unit); return; } \
        SETCALC(auditoryFFTNext<Cepstrum>); ClearUnitOutputs(unit, 1); \
    } \
    void Name##_Dtor(Name* unit) { destroyAuditoryFFT(unit); }
MIR2_AUDITORY(BarkBands, bark, false)
MIR2_AUDITORY(ERBBands, erb, false)
MIR2_AUDITORY(BFCC, bark, true)
MIR2_AUDITORY(LFCC, linear, true)
#undef MIR2_AUDITORY

struct GammatoneUnit : Unit {
    mir2::GammatoneFilter filters[kMaxAuditoryBands];
    double envelopes[kMaxAuditoryBands];
    double transformed[kMaxAuditoryBands];
    double decay, floor;
    int numBands, numCoeffs, hopSamples, untilOutput, include0, compression;
};
bool initializeGammatone(GammatoneUnit* unit) {
    unit->numCoeffs = mir2::boundedInteger(IN0(1));
    unit->numBands = mir2::boundedInteger(IN0(2));
    const double sampleRate = unit->mWorld->mSampleRate;
    const double low = mir2::clamp(static_cast<double>(IN0(3)), 20.0, sampleRate * 0.45 - 1.0);
    const double high = IN0(4) <= 0.0f ? sampleRate * 0.45
        : mir2::clamp(static_cast<double>(IN0(4)), low + 1.0, sampleRate * 0.45);
    const double smoothing = mir2::clamp(static_cast<double>(IN0(5)), 0.0001, 2.0);
    unit->decay = std::exp(-1.0 / (sampleRate * smoothing));
    unit->hopSamples = std::max(1, static_cast<int>(sampleRate * mir2::clamp(static_cast<double>(IN0(6)), 0.001, 1.0)));
    unit->untilOutput = unit->hopSamples;
    unit->include0 = IN0(7) > 0.0f ? 1 : 0;
    unit->compression = mir2::boundedInteger(IN0(8));
    unit->floor = std::max(1.0e-30, static_cast<double>(IN0(9)));
    if (INRATE(0) != calc_FullRate || unit->numBands < 4 || unit->numBands > kMaxAuditoryBands
        || unit->numCoeffs < 1 || unit->numCoeffs > unit->numBands || static_cast<int>(unit->mNumOutputs) != unit->numCoeffs) return false;
    const double a = mir2::erbRate(low), b = mir2::erbRate(high);
    for (int band = 0; band < unit->numBands; ++band) {
        const double center = mir2::inverseERBRate(a + (b-a) * band / (unit->numBands - 1));
        unit->filters[band].initialize(sampleRate, center);
        unit->envelopes[band] = 0.0;
    }
    return true;
}
template<bool cepstrum>
void gammatoneNext(GammatoneUnit* unit, int) {
    const float* input = IN(0);
    const int samples = unit->mWorld->mFullRate.mBufLength;
    for (int sample = 0; sample < samples; ++sample) {
        for (int band = 0; band < unit->numBands; ++band) {
            const double power = unit->filters[band].processPower(input[sample]);
            unit->envelopes[band] = unit->decay * unit->envelopes[band] + (1.0-unit->decay) * power;
            if (unit->envelopes[band] < 1.0e-30) unit->envelopes[band] = 0.0;
        }
        if (--unit->untilOutput > 0) continue;
        unit->untilOutput = unit->hopSamples;
        if (cepstrum) {
            for (int band = 0; band < unit->numBands; ++band)
                unit->transformed[band] = unit->compression == 0
                    ? std::cbrt(std::sqrt(unit->envelopes[band]))
                    : std::log(std::max(unit->floor, unit->envelopes[band]));
            for (int out = 0; out < unit->numCoeffs; ++out)
                ZOUT0(out) = static_cast<float>(mir2::dctII(unit->transformed, unit->numBands,
                    out + (unit->include0 ? 0 : 1), true));
        } else {
            for (int band = 0; band < unit->numBands; ++band)
                ZOUT0(band) = static_cast<float>(unit->envelopes[band]);
        }
    }
}
struct GammatoneBands : GammatoneUnit {};
void GammatoneBands_Ctor(GammatoneBands* unit) {
    if (!initializeGammatone(unit)) { failAuditory(unit); return; }
    SETCALC(gammatoneNext<false>); ClearUnitOutputs(unit, 1);
}
struct GFCC : GammatoneUnit {};
void GFCC_Ctor(GFCC* unit) {
    if (!initializeGammatone(unit) || unit->numCoeffs + (unit->include0 ? 0 : 1) > unit->numBands) { failAuditory(unit); return; }
    SETCALC(gammatoneNext<true>); ClearUnitOutputs(unit, 1);
}

struct Loudness2 : Unit { double* magnitudes; int fftSize; double excitation; int elapsedSamples; };
void Loudness2_next(Loudness2* unit, int) {
    unit->elapsedSamples = std::min(unit->elapsedSamples + unit->mWorld->mFullRate.mBufLength,
        static_cast<int>(unit->mWorld->mSampleRate * 60.0));
    if (IN0(0) < 0.0f) return;
    SndBuf* buffer = resolveBuffer(unit, 0);
    if (!buffer) return;
    { LOCK_SNDBUF(buffer); if (!fillMagnitudes(buffer, unit->fftSize, unit->magnitudes)) return; }
    const double target = mir2::loudnessExcitation(unit->magnitudes, unit->fftSize,
        unit->mWorld->mSampleRate, mir2::clamp(static_cast<double>(IN0(1)), 20.0, 140.0),
        mir2::clamp(static_cast<double>(IN0(4)), 1.0e-6, 1.0));
    const double time = std::max(0.0, static_cast<double>(IN0(target > unit->excitation ? 2 : 3)));
    const double coefficient = time > 0.0
        ? std::exp(-unit->elapsedSamples / (time * unit->mWorld->mSampleRate)) : 0.0;
    unit->excitation = target + coefficient * (unit->excitation - target);
    if (unit->excitation < 1.0e-12) unit->excitation = 0.0;
    unit->elapsedSamples = 0;
    const double phon = 10.0 * std::log10(1.0 + unit->excitation);
    ZOUT0(0) = static_cast<float>(mir2::phonToSone(phon));
    ZOUT0(1) = static_cast<float>(phon);
}
void Loudness2_Ctor(Loudness2* unit) {
    unit->magnitudes = nullptr; unit->excitation = 0.0; unit->elapsedSamples = 0;
    unit->fftSize = mir2::boundedInteger(IN0(5));
    if (!mir2::isPowerOfTwo(unit->fftSize) || unit->fftSize < 256 || unit->fftSize > 8192 || unit->mNumOutputs != 2) { failAuditory(unit); return; }
    unit->magnitudes = allocate<double>(unit->mWorld, static_cast<std::size_t>(unit->fftSize / 2 + 1));
    if (!unit->magnitudes) { failAuditory(unit); return; }
    SETCALC(Loudness2_next); ClearUnitOutputs(unit, 1);
}
void Loudness2_Dtor(Loudness2* unit) { release(unit->mWorld, unit->magnitudes); }
} // namespace

namespace mir2plugin {
void registerAuditoryUnits() {
    DefineDtorUnit(BarkBands);
    DefineDtorUnit(ERBBands);
    DefineDtorUnit(BFCC);
    DefineDtorUnit(LFCC);
    DefineSimpleUnit(GammatoneBands);
    DefineSimpleUnit(GFCC);
    DefineDtorUnit(Loudness2);
}
} // namespace mir2plugin
