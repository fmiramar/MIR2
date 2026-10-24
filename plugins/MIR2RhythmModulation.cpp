// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/RhythmModulation.hpp"

namespace {
using namespace mir2plugin;
struct ModulationUnit : Unit {
    mir2::ModulationConfig config;
    mir2::ModulationStream stream;
    double *window, *realBasis, *imagBasis, *magnitudes, *phases, *scratch;
    double windowSum;
};

void modulationFail(ModulationUnit* unit) {
    Print("MIR2 modulation: invalid dimensions/rates, work limit, or insufficient real-time memory\n");
    SETCALC(ClearUnitOutputs); unit->mDone = true; ClearUnitOutputs(unit, 1);
}
void modulationNext(ModulationUnit* unit, int) {
    double values[32];
    for (int band = 0; band < unit->config.bands; ++band) values[band] = IN0(15 + band);
    if (!unit->stream.push(values)) return;
    mir2::modulationSpectrum(unit->stream.frame, unit->config, unit->window, unit->windowSum,
        unit->realBasis, unit->imagBasis, unit->magnitudes, unit->phases);
    mir2::shapeModulationPattern(unit->magnitudes, unit->scratch, unit->config);
    const int count = unit->config.bands * unit->config.bins;
    for (int index = 0; index < count; ++index) {
        ZOUT0(index) = static_cast<float>(unit->magnitudes[index]);
        if (unit->config.phase) ZOUT0(index + count) = static_cast<float>(unit->phases[index]);
    }
}
bool modulationInit(ModulationUnit* unit) {
    unit->config = {}; unit->stream = {};
    unit->window = unit->realBasis = unit->imagBasis = unit->magnitudes = unit->phases = unit->scratch = nullptr;
    if (unit->mNumInputs < 16) return false;
    auto& config = unit->config;
    config.bands = mir2::boundedInteger(IN0(0)); config.bins = mir2::boundedInteger(IN0(1));
    config.minHz = IN0(2); config.maxHz = IN0(3); config.normalization = mir2::boundedInteger(IN0(7));
    config.removeMean = IN0(8) > 0; config.phase = IN0(9) > 0;
    config.fluctuation = IN0(12) > 0; config.smoothBands = IN0(13); config.smoothBins = IN0(14);
    mir2::ModulationTiming timing;
    if (!mir2::modulationTiming(unit->mWorld->mSampleRate, unit->mWorld->mFullRate.mBufLength,
        IN0(6), IN0(4), IN0(5), timing)) return false;
    config.frames = timing.frames; config.featureRate = timing.featureRate;
    const int compression = mir2::boundedInteger(IN0(10));
    const double gain = IN0(11);
    if (!mir2::validModulationConfig(config) || static_cast<int>(unit->mNumInputs) != 15 + config.bands
        || static_cast<int>(unit->mNumOutputs) != config.bands * config.bins * (config.phase ? 2 : 1)
        || compression < 0 || compression > 1 || !std::isfinite(gain) || gain < 0.0 || gain > 1.0e6) return false;
    for (int input = 0; input < 15; ++input) if (INRATE(input) != calc_ScalarRate) return false;
    for (int band = 0; band < config.bands; ++band) if (INRATE(15 + band) == calc_FullRate) return false;
    const auto frames = static_cast<std::size_t>(config.frames);
    const auto basis = frames * static_cast<std::size_t>(config.bins);
    const auto history = frames * static_cast<std::size_t>(config.bands);
    const auto count = static_cast<std::size_t>(config.bands * config.bins);
    unit->stream.ring = allocate<double>(unit->mWorld, history);
    unit->stream.frame = allocate<double>(unit->mWorld, history);
    unit->window = allocate<double>(unit->mWorld, frames);
    unit->realBasis = allocate<double>(unit->mWorld, basis);
    unit->imagBasis = allocate<double>(unit->mWorld, basis);
    unit->magnitudes = allocate<double>(unit->mWorld, count);
    unit->phases = config.phase ? allocate<double>(unit->mWorld, count) : nullptr;
    unit->scratch = allocate<double>(unit->mWorld, count);
    if (!unit->stream.ring || !unit->stream.frame || !unit->window || !unit->realBasis
        || !unit->imagBasis || !unit->magnitudes || !unit->scratch || (config.phase && !unit->phases)) return false;
    unit->stream.initialize(config, timing, compression, gain);
    unit->windowSum = mir2::prepareModulationBasis(config, unit->window, unit->realBasis, unit->imagBasis);
    return true;
}
void modulationDestroy(ModulationUnit* unit) {
    release(unit->mWorld, unit->stream.ring); release(unit->mWorld, unit->stream.frame);
    release(unit->mWorld, unit->window); release(unit->mWorld, unit->realBasis); release(unit->mWorld, unit->imagBasis);
    release(unit->mWorld, unit->magnitudes); release(unit->mWorld, unit->phases); release(unit->mWorld, unit->scratch);
}
#define MIR2_MODULATION(Name) \
    struct Name : ModulationUnit {}; \
    void Name##_Ctor(Name* unit) { \
        if (!modulationInit(unit)) { modulationFail(unit); return; } \
        SETCALC(modulationNext); ClearUnitOutputs(unit, 1); \
    } \
    void Name##_Dtor(Name* unit) { modulationDestroy(unit); }
MIR2_MODULATION(RhythmTransform)
MIR2_MODULATION(RhythmPattern)
#undef MIR2_MODULATION
} // namespace

namespace mir2plugin {
void registerRhythmModulationUnits() {
    DefineDtorUnit(RhythmTransform); DefineDtorUnit(RhythmPattern);
}
} // namespace mir2plugin
