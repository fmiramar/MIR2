// SPDX-License-Identifier: GPL-3.0-or-later
#include "SC_PlugIn.hpp"
#include "mir2neural/PestoStream.hpp"
#include <new>

static InterfaceTable* ft = nullptr;

struct PitchPESTO : Unit {
    mir2neural::Stream* stream;
    const float* modelData;
    int modelSamples;
    int modelIndex;
};

static void PitchPESTO_next(PitchPESTO* unit, int) {
    ClearUnitOutputs(unit, 1);
    if (!unit->stream || IN0(1) != static_cast<float>(unit->modelIndex)) return;
    SndBuf* buffer = unit->mWorld->mSndBufs + unit->modelIndex;
    LOCK_SNDBUF_SHARED(buffer);
    // Reallocation/free invalidates the nonowning model view. Never dereference
    // it again after a storage change. In-place writes are forbidden by the
    // documented model-buffer contract, as with shared lookup tables.
    if (buffer->data != unit->modelData || buffer->samples != unit->modelSamples
        || buffer->channels != 1) {
        SETCALC(ClearUnitOutputs);
        return;
    }
    unit->stream->process(IN(0), unit->mWorld->mFullRate.mBufLength);
    if (!unit->stream->ready()) return;
    const auto result = unit->stream->result;
    const float threshold = std::isfinite(IN0(2)) ? std::clamp(IN0(2), 0.f, 1.f) : .8f;
    const bool voiced = result.confidence >= threshold && result.energy > 1e-10f
        && result.frequency > 0.f;
    OUT0(0) = voiced ? result.frequency : 0.f;
    OUT0(1) = result.confidence;
    OUT0(2) = voiced ? 1.f : 0.f;
    OUT0(3) = result.energy;
}

static void PitchPESTO_Ctor(PitchPESTO* unit) {
    unit->stream = nullptr;
    unit->modelData = nullptr;
    unit->modelSamples = 0;
    unit->modelIndex = -1;
    SETCALC(ClearUnitOutputs);
    ClearUnitOutputs(unit, 1);
    const float index = IN0(1);
    if (INRATE(0) != calc_FullRate || !std::isfinite(index) || index < 0.f
        || static_cast<double>(index) >= unit->mWorld->mNumSndBufs
        || index != std::floor(index)) {
        Print("PitchPESTO: requires audio input and a loaded global model Buffer\n");
        return;
    }
    unit->modelIndex = static_cast<int>(index);
    SndBuf* buffer = unit->mWorld->mSndBufs + unit->modelIndex;
    LOCK_SNDBUF_SHARED(buffer);
    if (!buffer->data || buffer->channels != 1 || buffer->samples <= 0) {
        Print("PitchPESTO: model Buffer is not ready; load it and wait for s.sync\n");
        return;
    }
    void* memory = RTAlloc(unit->mWorld, sizeof(mir2neural::Stream));
    if (!memory) { Print("PitchPESTO: insufficient RT memory\n"); return; }
    unit->stream = new (memory) mir2neural::Stream;
    const double rate = unit->mWorld->mSampleRate;
    if (rate < 16000. || rate > 96000. || rate != std::floor(rate)
        || !unit->stream->bind(buffer->data, static_cast<std::size_t>(buffer->samples), static_cast<int>(rate))) {
        Print("PitchPESTO: invalid model pack or sample-rate mismatch\n");
        return;
    }
    unit->modelData = buffer->data;
    unit->modelSamples = buffer->samples;
    SETCALC(PitchPESTO_next);
}

static void PitchPESTO_Dtor(PitchPESTO* unit) {
    if (unit->stream) {
        unit->stream->~Stream();
        RTFree(unit->mWorld, unit->stream);
        unit->stream = nullptr;
    }
}

PluginLoad(MIR2Neural) {
    ft = inTable;
    DefineDtorUnit(PitchPESTO);
}
