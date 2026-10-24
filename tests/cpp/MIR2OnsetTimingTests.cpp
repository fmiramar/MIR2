// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Onset.hpp"
#include "mir2/RunningMedian.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
void requireFinite(const mir2::OnsetResult& result) {
    assert(std::isfinite(result.trigger));
    assert(result.trigger == 0.0 || result.trigger == 1.0);
    assert(std::isfinite(result.raw));
    assert(std::isfinite(result.strength));
    assert(result.strength >= 0.0 && result.strength <= 1.0);
}

std::vector<int> pulseFrames(double sampleRate, int fftSize, double hop,
                             int requestedGap, int pulseGap) {
    mir2::OnsetConfig config;
    config.sampleRate = sampleRate;
    config.fftSize = fftSize;
    config.hop = hop;
    config.medianTime = 0.0;
    config.method = mir2::OnsetMethod::power;
    std::vector<double> memory(mir2::OnsetState::storageCount(config));
    mir2::OnsetState state;
    state.initialize(config, memory.data());
    mir2::OnsetControls controls;
    controls.whitening = false;
    controls.minGap = static_cast<double>(requestedGap) * fftSize * hop / sampleRate;
    const int bins = fftSize / 2 + 1;
    std::vector<double> magnitude(static_cast<std::size_t>(bins), 0.0);
    std::vector<double> phase(static_cast<std::size_t>(bins), 0.0);
    std::vector<int> detections;
    for (int frame = 0; frame < 20 + pulseGap; ++frame) {
        const double value = frame == 8 || frame == 8 + pulseGap
            ? static_cast<double>(fftSize) * 0.5 : 0.0;
        std::fill(magnitude.begin(), magnitude.end(), value);
        const auto result = state.process(magnitude.data(), phase.data(), controls);
        requireFinite(result);
        if (result.trigger != 0.0) detections.push_back(frame);
    }
    return detections;
}

void checkGapBoundary() {
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
        for (int fftSize : {1024, 2048})
            for (double hop : {0.25, 0.5})
                for (int gap : {4, 7, 11}) {
                    const auto atBoundary = pulseFrames(sampleRate, fftSize, hop, gap, gap);
                    assert((atBoundary == std::vector<int>{9, 9 + gap}));
                    const auto tooSoon = pulseFrames(sampleRate, fftSize, hop, gap, gap - 1);
                    assert((tooSoon == std::vector<int>{9}));
                }
}

void checkHistoryDuration() {
    for (double sampleRate : {44100.0, 48000.0, 96000.0})
        for (int fftSize : {256, 512, 1024, 2048, 4096, 8192})
            for (double hop : {0.0625, 0.25, 0.5})
                for (double duration : {0.0, 0.001, 0.15, 10.0}) {
                    mir2::OnsetConfig config;
                    config.sampleRate = sampleRate;
                    config.fftSize = fftSize;
                    config.hop = hop;
                    config.medianTime = duration;
                    // Derive the required history directly from audio samples,
                    // independently of OnsetState's history-count helper.
                    const double samplesPerFrame = fftSize * hop;
                    const int expected = std::max(3, static_cast<int>(
                        std::ceil(duration / (samplesPerFrame / sampleRate))));
                    assert(expected <= mir2::RunningMedian::maxCapacity);
                    assert(mir2::OnsetState::historyCount(config) == expected);
                    std::vector<double> memory(mir2::OnsetState::storageCount(config));
                    mir2::OnsetState state;
                    state.initialize(config, memory.data());
                    assert(state.historySize == expected);
                    const double actualDuration = state.historySize * samplesPerFrame / sampleRate;
                    assert(actualDuration + 1.0e-12 >= duration);
                    if (expected > 3)
                        assert(actualDuration - duration <= samplesPerFrame / sampleRate + 1.0e-12);
                }
    mir2::OnsetConfig excessive;
    excessive.fftSize = 256;
    excessive.hop = 0.0625;
    excessive.sampleRate = 192000.0;
    excessive.medianTime = 10.0;
    assert(mir2::OnsetState::historyCount(excessive) == 0);
    assert(mir2::OnsetState::storageCount(excessive) == 0);
}

void checkEffectiveHop() {
    assert(mir2::onsetHopSamples(256, 0.0625, 64) == 64);
    assert(mir2::onsetHopSamples(1024, 0.3, 64) == 256);
    assert(mir2::onsetHopSamples(2048, 0.5, 64) == 1024);
    for (int fftSize : {256, 512, 1024, 2048, 4096, 8192})
        for (int blockSize : {16, 64, 128})
            for (double hop : {0.0625, 0.25, 0.3, 0.5}) {
                const int actual = mir2::onsetHopSamples(fftSize, hop, blockSize);
                assert(actual >= blockSize && actual % blockSize == 0);
                const int requested = static_cast<int>(fftSize * hop);
                if (requested < blockSize) assert(actual == blockSize);
                else assert(actual <= requested && requested - actual < blockSize);
            }
}

void checkDefaultSizes() {
    // Controlled spectra exercise every method at both the public default and
    // the documented example size. This is an algorithm smoke check, not an
    // acoustic accuracy corpus or a replacement for FFT/server integration.
    for (int fftSize : {1024, 2048})
        for (double sampleRate : {44100.0, 48000.0, 96000.0})
            for (int method = 0; method <= 10; ++method) {
                mir2::OnsetConfig config;
                config.fftSize = fftSize;
                config.sampleRate = sampleRate;
                config.method = static_cast<mir2::OnsetMethod>(method);
                std::vector<double> memory(mir2::OnsetState::storageCount(config));
                mir2::OnsetState state;
                state.initialize(config, memory.data());
                mir2::OnsetControls controls;
                const int bins = fftSize / 2 + 1;
                std::vector<double> magnitude(static_cast<std::size_t>(bins), 0.0);
                std::vector<double> phase(static_cast<std::size_t>(bins), 0.0);
                for (int frame = 0; frame < 300; ++frame) {
                    for (int bin = 0; bin < bins; ++bin) {
                        const bool active = frame >= 30 && frame < 240;
                        const double envelope = std::exp(-static_cast<double>((frame - 30 + 300) % 30) / 4.0);
                        magnitude[static_cast<std::size_t>(bin)] = active
                            ? fftSize * 0.02 * envelope / (1.0 + bin) : 0.0;
                        phase[static_cast<std::size_t>(bin)] = std::remainder(
                            static_cast<double>(frame) * (0.17 * bin + 0.013 * frame), mir2::kTwoPi);
                    }
                    const auto result = state.process(magnitude.data(), phase.data(), controls);
                    requireFinite(result);
                    if (frame < 30) assert(result.trigger == 0.0);
                }
            }
}

void medianCost(int capacity) {
    constexpr int updates = 262144;
    constexpr int batch = 1024;
    std::vector<double> memory(mir2::RunningMedian::storageCount(capacity));
    mir2::RunningMedian median;
    assert(median.initialize(capacity, memory.data()));
    std::uint32_t generator = 71;
    double checksum = 0.0, longestBatch = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int first = 0; first < updates; first += batch) {
        const auto batchStart = std::chrono::steady_clock::now();
        for (int i = 0; i < batch; ++i) {
            generator = 1664525U * generator + 1013904223U;
            // Keep all 32 bits: a 16-bit generator would repeat exactly at a
            // 65536-entry ring and make replacement artificially cheap.
            median.update(static_cast<double>(generator));
            checksum += median.value();
        }
        longestBatch = std::max(longestBatch, std::chrono::duration<double>(
            std::chrono::steady_clock::now() - batchStart).count());
    }
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    assert(std::isfinite(checksum) && checksum > 0.0);
    std::cout << "median_cost,capacity=" << capacity << ",updates=" << updates
              << ",mean_us=" << elapsed * 1.0e6 / updates
              << ",max_batch_mean_us=" << longestBatch * 1.0e6 / batch
              << ",checksum=" << checksum << '\n';
}

void callbackCost() {
    // Largest history from the documented ten-second range at the shortest
    // declared kernel hop in this matrix; actual server block sizes may impose
    // a longer hop. Initialization/allocation is excluded from timing.
    mir2::OnsetConfig config;
    config.fftSize = 256;
    config.sampleRate = 96000.0;
    config.hop = 0.0625;
    config.medianTime = 10.0;
    std::vector<double> memory(mir2::OnsetState::storageCount(config));
    mir2::OnsetState state;
    state.initialize(config, memory.data());
    mir2::OnsetControls controls;
    std::vector<double> magnitude(129, 0.0), phase(129, 0.0);
    constexpr int updates = 131072;
    double checksum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < updates; ++frame) {
        for (int bin = 0; bin < 129; ++bin)
            magnitude[static_cast<std::size_t>(bin)] = 1.0 + static_cast<double>((frame * 17 + bin * 31) % 127);
        const auto result = state.process(magnitude.data(), phase.data(), controls);
        requireFinite(result);
        checksum += result.raw;
    }
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    const double audioSeconds = updates * config.fftSize * config.hop / config.sampleRate;
    std::cout << "onset_kernel_cost,fft_size=" << config.fftSize
              << ",history=" << state.historySize << ",updates=" << updates
              << ",audio_samples=" << updates * config.fftSize * config.hop
              << ",mean_us=" << elapsed * 1.0e6 / updates
              << ",elapsed_over_audio=" << elapsed / audioSeconds
              << ",checksum=" << checksum << '\n';
}
} // namespace

int main() {
    checkGapBoundary();
    checkHistoryDuration();
    checkEffectiveHop();
    checkDefaultSizes();
    medianCost(3);
    medianCost(65536);
    callbackCost();
    // Wall-clock observations are reported, not asserted: machine load and
    // sanitizer instrumentation cannot establish a hard realtime deadline.
    std::cout << "Onset timing, history duration and default-size checks passed.\n";
}
