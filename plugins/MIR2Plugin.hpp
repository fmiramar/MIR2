// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FFT_UGens.h"
#include "SC_PlugIn.hpp"

#include "mir2/Pitch.hpp"
#include "mir2/Spectral.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

extern InterfaceTable* ft;

namespace mir2plugin {

template <typename T>
inline T* allocate(World* world, std::size_t count) noexcept {
    if (count == 0)
        return nullptr;
    return static_cast<T*>(RTAlloc(world, count * sizeof(T)));
}

template <typename T>
inline void release(World* world, T*& pointer) noexcept {
    if (pointer != nullptr) {
        RTFree(world, pointer);
        pointer = nullptr;
    }
}

inline SndBuf* resolveBuffer(Unit* unit, int inputIndex) noexcept {
    const float value = IN0(inputIndex);
    if (!std::isfinite(value) || value < 0.0f
        || static_cast<double>(value) >= static_cast<double>(unit->mWorld->mNumSndBufs)
            + static_cast<double>(unit->mParent->localBufNum) + 1.0)
        return nullptr;
    const std::uint32_t index = static_cast<std::uint32_t>(value);
    World* world = unit->mWorld;
    if (index < world->mNumSndBufs)
        return world->mSndBufs + index;
    const int localIndex = static_cast<int>(index - world->mNumSndBufs);
    Graph* graph = unit->mParent;
    if (localIndex >= 0 && localIndex <= graph->localBufNum)
        return graph->mLocalSndBufs + localIndex;
    return nullptr;
}

// Read without changing the shared FFT buffer's representation. The caller
// holds the SndBuf lock; phase-sensitive analyzers can share this chain safely.
inline bool readFFTFrame(SndBuf* buffer, int expectedFFTSize,
                         double* magnitudes, double* phases = nullptr) noexcept {
    if (buffer == nullptr || buffer->data == nullptr
        || buffer->samples != expectedFFTSize || magnitudes == nullptr)
        return false;
    const int interiorBins = (buffer->samples - 2) >> 1;
    const auto* rectangular = reinterpret_cast<const SCComplexBuf*>(buffer->data);
    magnitudes[0] = std::abs(mir2::sanitize(static_cast<double>(rectangular->dc)));
    magnitudes[interiorBins + 1] = std::abs(mir2::sanitize(static_cast<double>(rectangular->nyq)));
    if (phases) {
        phases[0] = rectangular->dc < 0.0f ? mir2::kPi : 0.0;
        phases[interiorBins + 1] = rectangular->nyq < 0.0f ? mir2::kPi : 0.0;
    }
    for (int index = 0; index < interiorBins; ++index) {
        double magnitude, phase;
        if (buffer->coord == coord_Polar) {
            const auto* polar = reinterpret_cast<const SCPolarBuf*>(buffer->data);
            magnitude = mir2::sanitize(static_cast<double>(polar->bin[index].mag));
            phase = mir2::sanitize(static_cast<double>(polar->bin[index].phase));
            if (magnitude < 0.0) { magnitude = -magnitude; phase += mir2::kPi; }
        } else {
            const double real = mir2::sanitize(static_cast<double>(rectangular->bin[index].real));
            const double imaginary = mir2::sanitize(static_cast<double>(rectangular->bin[index].imag));
            magnitude = std::hypot(real, imaginary);
            phase = phases ? std::atan2(imaginary, real) : 0.0;
        }
        magnitudes[index + 1] = magnitude;
        if (phases) phases[index + 1] = phase;
    }
    return true;
}

inline bool fillMagnitudes(SndBuf* buffer, int expectedFFTSize,
                           double* magnitudes) noexcept {
    return readFFTFrame(buffer, expectedFFTSize, magnitudes);
}

struct PitchState {
    float* ring;
    float* frame;
    mir2::Complex* fftData;
    double* prefixEnergy;
    double* difference;
    double* cmndf;
    double* nsdf;
    int frameSize;
    int fftSize;
    int writeIndex;
    int filled;
    int hopSamples;
    int samplesUntilAnalysis;
    mir2::PitchResult result;
    double voicingProbability;
};

inline void clearPitchState(PitchState& state) noexcept {
    state.ring = nullptr;
    state.frame = nullptr;
    state.fftData = nullptr;
    state.prefixEnergy = nullptr;
    state.difference = nullptr;
    state.cmndf = nullptr;
    state.nsdf = nullptr;
    state.frameSize = 0;
    state.fftSize = 0;
    state.writeIndex = 0;
    state.filled = 0;
    state.hopSamples = 1;
    state.samplesUntilAnalysis = 0;
    state.result = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    state.voicingProbability = 0.0;
}

inline bool initializePitchState(Unit* unit, PitchState& state,
                                 int requestedFrameSize, double hop) noexcept {
    clearPitchState(state);
    state.frameSize = mir2::clamp(requestedFrameSize, 256, 8192);
    if (!mir2::isPowerOfTwo(state.frameSize))
        return false;
    state.fftSize = state.frameSize * 2;
    state.hopSamples = mir2::clamp(
        static_cast<int>(std::lround(static_cast<double>(state.frameSize)
                                     * mir2::clamp(hop, 0.0625, 1.0))),
        1, state.frameSize);
    state.samplesUntilAnalysis = state.frameSize;
    const auto frameCount = static_cast<std::size_t>(state.frameSize);
    const auto fftCount = static_cast<std::size_t>(state.fftSize);
    state.ring = allocate<float>(unit->mWorld, frameCount);
    state.frame = allocate<float>(unit->mWorld, frameCount);
    state.fftData = allocate<mir2::Complex>(unit->mWorld, fftCount);
    state.prefixEnergy = allocate<double>(unit->mWorld, frameCount + 1U);
    state.difference = allocate<double>(unit->mWorld, frameCount);
    state.cmndf = allocate<double>(unit->mWorld, frameCount);
    state.nsdf = allocate<double>(unit->mWorld, frameCount);
    const bool valid = state.ring != nullptr && state.frame != nullptr
        && state.fftData != nullptr && state.prefixEnergy != nullptr
        && state.difference != nullptr && state.cmndf != nullptr
        && state.nsdf != nullptr;
    if (valid) {
        std::fill(state.ring, state.ring + state.frameSize, 0.0f);
        std::fill(state.frame, state.frame + state.frameSize, 0.0f);
    }
    return valid;
}

inline void destroyPitchState(World* world, PitchState& state) noexcept {
    release(world, state.ring);
    release(world, state.frame);
    release(world, state.fftData);
    release(world, state.prefixEnergy);
    release(world, state.difference);
    release(world, state.cmndf);
    release(world, state.nsdf);
}

inline void copyCurrentFrame(PitchState& state) noexcept {
    for (int index = 0; index < state.frameSize; ++index) {
        const int source = (state.writeIndex + index) % state.frameSize;
        state.frame[index] = state.ring[source];
    }
}

enum class PitchAlgorithm : int { yin = 0, mpm = 1 };

inline void analyzePitchFrame(PitchState& state, double sampleRate,
                              double minFreq, double maxFreq, double parameter,
                              PitchAlgorithm algorithm) noexcept {
    copyCurrentFrame(state);
    mir2::PitchWorkspace workspace { state.fftData, state.prefixEnergy,
                                    state.difference, state.cmndf, state.nsdf,
                                    state.frameSize, state.fftSize };
    double rms = 0.0;
    if (!mir2::preparePitchEvidence(state.frame, workspace, rms)) {
        state.result = { 0.0, 0.0, 0.0, 0.0, 0.0 };
        state.voicingProbability = 0.0;
        return;
    }
    state.result = algorithm == PitchAlgorithm::yin
        ? mir2::analyzeYIN(workspace, sampleRate, minFreq, maxFreq, parameter, rms)
        : mir2::analyzeMPM(workspace, sampleRate, minFreq, maxFreq, parameter, rms);
    state.voicingProbability = mir2::frameVoicingProbability(
        state.frame, state.frameSize, sampleRate, maxFreq,
        state.result.periodicity, rms);
}

inline void processPitchInput(Unit* unit, PitchState& state, int inNumSamples,
                              double minFreq, double maxFreq, double parameter,
                              PitchAlgorithm algorithm) noexcept {
    const bool audioRate = INRATE(0) == calc_FullRate;
    const float* input = IN(0);
    const int sampleCount = audioRate
        ? unit->mWorld->mFullRate.mBufLength : inNumSamples;
    for (int sample = 0; sample < sampleCount; ++sample) {
        const float value = audioRate ? input[sample] : input[0];
        state.ring[state.writeIndex] = std::isfinite(value) ? value : 0.0f;
        state.writeIndex = (state.writeIndex + 1) % state.frameSize;
        state.filled = std::min(state.frameSize, state.filled + 1);
        --state.samplesUntilAnalysis;
        if (state.filled == state.frameSize && state.samplesUntilAnalysis <= 0) {
            analyzePitchFrame(state, unit->mWorld->mSampleRate, minFreq,
                              maxFreq, parameter, algorithm);
            state.samplesUntilAnalysis = state.hopSamples;
        }
    }
}

void registerSpectralUnits();
void registerPitchUnits();
void registerOnsetUnits();
void registerTemporalUnits();
void registerStereoUnits();
void registerFeatureDynamicsUnits();
void registerAdvancedSpectralUnits();
void registerAuditoryUnits();
void registerPitchAdvancedUnits();
void registerWaveletUnits();
void registerConstantQUnits();



} // namespace mir2plugin
