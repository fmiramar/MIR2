// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/SpectralAdvanced.hpp"

namespace {
using namespace mir2plugin;
using mir2::SpectralDescriptor;
struct DescriptorUnit : Unit {
    double* magnitudes;
    double* previous;
    double* scratch;
    int fftSize;
    int numBands;
    bool ready;
};

bool initialize(DescriptorUnit* unit, int fftSize) {
    unit->magnitudes = unit->previous = unit->scratch = nullptr;
    unit->fftSize = fftSize;
    unit->ready = false;
    if (!mir2::isPowerOfTwo(fftSize) || fftSize < 256 || fftSize > 8192) return false;
    const auto bins = static_cast<std::size_t>(fftSize / 2 + 1);
    unit->magnitudes = allocate<double>(unit->mWorld, bins);
    unit->previous = allocate<double>(unit->mWorld, bins);
    unit->scratch = allocate<double>(unit->mWorld, bins);
    if (!unit->magnitudes || !unit->previous || !unit->scratch) return false;
    std::fill(unit->previous, unit->previous + bins, 0.0);
    return true;
}

bool load(DescriptorUnit* unit) {
    if (IN0(0) < 0.0f) return false;
    SndBuf* buffer = resolveBuffer(unit, 0);
    if (!buffer) return false;
    LOCK_SNDBUF(buffer);
    return fillMagnitudes(buffer, unit->fftSize, unit->magnitudes);
}

void destroy(DescriptorUnit* unit) {
    release(unit->mWorld, unit->magnitudes);
    release(unit->mWorld, unit->previous);
    release(unit->mWorld, unit->scratch);
}

void disable(DescriptorUnit* unit) {
    Print("MIR2 spectral: invalid dimensions or insufficient real-time memory\n");
    SETCALC(ClearUnitOutputs);
    unit->mDone = true;
    ClearUnitOutputs(unit, 1);
}

template<SpectralDescriptor descriptor>
void scalarNext(DescriptorUnit* unit, int) {
    if (!load(unit)) return;
    const int count = unit->fftSize / 2 + 1;
    const double hz = unit->mWorld->mSampleRate / unit->fftSize;
    ZOUT0(0) = static_cast<float>(mir2::sanitize(mir2::spectralDescriptor(
        unit->magnitudes, unit->ready ? unit->previous : nullptr,
        count, hz, IN0(1), IN0(2), mir2::boundedInteger(IN0(3)), descriptor,
        IN0(4), mir2::boundedInteger(IN0(5)))));
    if (descriptor == SpectralDescriptor::variation) {
        std::copy(unit->magnitudes, unit->magnitudes + count, unit->previous);
        unit->ready = true;
    }
}

#define MIR2_SCALAR(Name, Kind) \
    struct Name : DescriptorUnit {}; \
    void Name##_Ctor(Name* unit) { \
        if (!initialize(unit, mir2::boundedInteger(IN0(6)))) { disable(unit); return; } \
        SETCALC(scalarNext<SpectralDescriptor::Kind>); ClearUnitOutputs(unit, 1); \
    } \
    void Name##_Dtor(Name* unit) { destroy(unit); }

MIR2_SCALAR(FFTCrest2, crest)
MIR2_SCALAR(FFTSpread2, spread)
MIR2_SCALAR(FFTSlope2, slope)
MIR2_SCALAR(FFTPower2, power)
MIR2_SCALAR(SpecEntropy, entropy)
MIR2_SCALAR(SpecDecrease, decrease)
MIR2_SCALAR(SpecComplexity, complexity)
MIR2_SCALAR(SpecStrongPeak, strongPeak)
MIR2_SCALAR(SpecHFC, hfc)
MIR2_SCALAR(SpecBandwidth, bandwidth)
MIR2_SCALAR(SpecIrregularity, irregularity)
MIR2_SCALAR(SpecVariation, variation)
MIR2_SCALAR(SpecRMS, rms)
MIR2_SCALAR(SpecEnergy, energy)
MIR2_SCALAR(SpecBandEnergy, energy)
MIR2_SCALAR(SpecBandRatio, bandRatio)
MIR2_SCALAR(SpecSparsity, sparsity)
#undef MIR2_SCALAR

struct FFTPeak2 : DescriptorUnit {};
void FFTPeak2_next(FFTPeak2* unit, int) {
    if (!load(unit)) return;
    mir2::SpectralPeak peak {};
    const int found = mir2::extractSpectralPeaks(unit->magnitudes, unit->fftSize / 2 + 1,
        unit->mWorld->mSampleRate / unit->fftSize, IN0(1), IN0(2), 0.0, &peak, 1, IN0(3) > 0.0f);
    ZOUT0(0) = found ? static_cast<float>(peak.frequency) : 0.0f;
    ZOUT0(1) = found ? static_cast<float>(peak.magnitude) : 0.0f;
}
void FFTPeak2_Ctor(FFTPeak2* unit) {
    if (!initialize(unit, mir2::boundedInteger(IN0(4))) || unit->mNumOutputs != 2) { disable(unit); return; }
    SETCALC(FFTPeak2_next); ClearUnitOutputs(unit, 1);
}
void FFTPeak2_Dtor(FFTPeak2* unit) { destroy(unit); }

enum class BandKind { flatness, flux, power, contrast };
template<BandKind kind>
void bandNext(DescriptorUnit* unit, int) {
    if (!load(unit)) return;
    const int bins = unit->fftSize / 2 + 1;
    const double hz = unit->mWorld->mSampleRate / unit->fftSize;
    const int spectrumMode = mir2::boundedInteger(IN0(2));
    const double parameter = IN0(3);
    const int mode = mir2::boundedInteger(IN0(4));
    for (int band = 0; band < unit->numBands; ++band) {
        int first, last;
        const double low = IN0(6 + band), high = IN0(7 + band);
        double result = 0.0, valley = 0.0;
        if (mir2::spectralRange(bins, hz, low, high, first, last)) {
            // Adjacent bands do not count a boundary bin twice.
            if (band + 1 < unit->numBands && last * hz >= high) --last;
            if (first <= last) {
                if (kind == BandKind::flatness) {
                    double sum = 0.0, logSum = 0.0, raw = 0.0;
                    const double floor = std::max(1.0e-30, mir2::sanitize(parameter));
                    for (int bin = first; bin <= last; ++bin) {
                        const double value = mir2::spectralWeight(unit->magnitudes[bin], spectrumMode);
                        raw += value; sum += std::max(value, floor); logSum += std::log(std::max(value, floor));
                    }
                    if (raw > 1.0e-30) result = std::exp(logSum / (last-first+1)) / (sum / (last-first+1));
                } else if (kind == BandKind::power) {
                    for (int bin = first; bin <= last; ++bin)
                        result += mir2::spectralWeight(unit->magnitudes[bin], spectrumMode);
                    result /= last-first+1;
                } else if (kind == BandKind::contrast) {
                    mir2::spectralContrast(unit->magnitudes, first, last, spectrumMode,
                        parameter, 1.0e-20, unit->scratch, result, valley);
                } else if (unit->ready) {
                    double square = 0.0, norm = 0.0;
                    for (int bin = first; bin <= last; ++bin) {
                        const double current = mir2::spectralWeight(unit->magnitudes[bin], spectrumMode);
                        double difference = current - mir2::spectralWeight(unit->previous[bin], spectrumMode);
                        if (mode != 0) difference = std::max(0.0, difference);
                        square += difference * difference; norm += current * current;
                    }
                    result = std::sqrt(square);
                    if (parameter > 0.0) result = norm > 1.0e-30 ? result / std::sqrt(norm) : 0.0;
                }
            }
        }
        ZOUT0(band) = static_cast<float>(mir2::sanitize(result));
        if (kind == BandKind::contrast && mode != 0)
            ZOUT0(unit->numBands + band) = static_cast<float>(mir2::sanitize(valley));
    }
    if (kind == BandKind::flux) {
        std::copy(unit->magnitudes, unit->magnitudes + bins, unit->previous);
        unit->ready = true;
    }
}

#define MIR2_BANDS(Name, Kind) \
    struct Name : DescriptorUnit {}; \
    void Name##_Ctor(Name* unit) { \
        unit->numBands = mir2::boundedInteger(IN0(1)); \
        const int outputs = unit->numBands * ((BandKind::Kind == BandKind::contrast && IN0(4) != 0.0f) ? 2 : 1); \
        if (!initialize(unit, mir2::boundedInteger(IN0(5))) || unit->numBands < 1 || unit->numBands > 64 \
            || static_cast<int>(unit->mNumInputs) != 7 + unit->numBands || static_cast<int>(unit->mNumOutputs) != outputs) { disable(unit); return; } \
        SETCALC(bandNext<BandKind::Kind>); ClearUnitOutputs(unit, 1); \
    } \
    void Name##_Dtor(Name* unit) { destroy(unit); }
MIR2_BANDS(FFTSubbandFlatness2, flatness)
MIR2_BANDS(FFTSubbandFlux2, flux)
MIR2_BANDS(FFTSubbandPower2, power)
MIR2_BANDS(SpecContrast, contrast)
#undef MIR2_BANDS
} // namespace

namespace mir2plugin {
void registerAdvancedSpectralUnits() {
    DefineDtorUnit(FFTCrest2);
    DefineDtorUnit(FFTSpread2);
    DefineDtorUnit(FFTSlope2);
    DefineDtorUnit(FFTPower2);
    DefineDtorUnit(FFTPeak2);
    DefineDtorUnit(FFTSubbandFlatness2);
    DefineDtorUnit(FFTSubbandFlux2);
    DefineDtorUnit(FFTSubbandPower2);
    DefineDtorUnit(SpecEntropy);
    DefineDtorUnit(SpecDecrease);
    DefineDtorUnit(SpecComplexity);
    DefineDtorUnit(SpecContrast);
    DefineDtorUnit(SpecStrongPeak);
    DefineDtorUnit(SpecHFC);
    DefineDtorUnit(SpecBandwidth);
    DefineDtorUnit(SpecIrregularity);
    DefineDtorUnit(SpecVariation);
    DefineDtorUnit(SpecRMS);
    DefineDtorUnit(SpecEnergy);
    DefineDtorUnit(SpecBandEnergy);
    DefineDtorUnit(SpecBandRatio);
    DefineDtorUnit(SpecSparsity);
}
} // namespace mir2plugin
