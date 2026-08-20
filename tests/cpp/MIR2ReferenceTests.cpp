// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/FFT.hpp"
#include "mir2/Pitch.hpp"
#include "mir2/Spectral.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void requireNear(double actual, double expected, double tolerance,
                 const std::string& message) {
    require(std::abs(actual - expected) <= tolerance,
            message + " actual=" + std::to_string(actual)
                + " expected=" + std::to_string(expected));
}

struct PitchHarness {
    std::vector<float> frame;
    std::vector<mir2::Complex> fft;
    std::vector<double> prefix;
    std::vector<double> difference;
    std::vector<double> cmndf;
    std::vector<double> nsdf;
    mir2::PitchWorkspace workspace;

    explicit PitchHarness(int size)
        : frame(static_cast<std::size_t>(size), 0.0f),
          fft(static_cast<std::size_t>(size * 2)),
          prefix(static_cast<std::size_t>(size + 1)),
          difference(static_cast<std::size_t>(size)),
          cmndf(static_cast<std::size_t>(size)),
          nsdf(static_cast<std::size_t>(size)),
          workspace { fft.data(), prefix.data(), difference.data(), cmndf.data(),
                      nsdf.data(), size, size * 2 } {}

    double prepare() {
        double rms = 0.0;
        mir2::preparePitchEvidence(frame.data(), workspace, rms);
        return rms;
    }
};

void fillSine(std::vector<float>& signal, double sampleRate, double frequency) {
    for (std::size_t index = 0; index < signal.size(); ++index)
        signal[index] = static_cast<float>(0.5 * std::sin(
            mir2::kTwoPi * frequency * static_cast<double>(index) / sampleRate));
}

void testFFT() {
    std::vector<mir2::Complex> values(16, { 0.0, 0.0 });
    values[3].real = 1.0;
    const auto original = values;
    mir2::fft(values.data(), static_cast<int>(values.size()), false);
    mir2::fft(values.data(), static_cast<int>(values.size()), true);
    for (std::size_t index = 0; index < values.size(); ++index) {
        requireNear(values[index].real, original[index].real, 1.0e-12,
                    "FFT inverse real");
        requireNear(values[index].imag, original[index].imag, 1.0e-12,
                    "FFT inverse imaginary");
    }
}

void testSpectralDescriptors() {
    const double magnitudes[] = { 0.0, 0.0, 1.0, 0.0, 0.0 };
    requireNear(mir2::spectralCentroid(magnitudes, 5, 100.0, 0.0, 0.0, 0),
                200.0, 1.0e-12, "single-bin centroid");
    const double flat[] = { 1.0, 1.0, 1.0, 1.0 };
    requireNear(mir2::spectralFlatness(flat, 4, 100.0, 0.0, 0.0, 1, 1.0e-20),
                1.0, 1.0e-12, "uniform spectrum flatness");
    requireNear(mir2::spectralFlatness(magnitudes, 5, 100.0, 0.0, 0.0, 1, 1.0e-20),
                0.0, 1.0e-6, "tonal spectrum flatness");
    requireNear(mir2::spectralPercentile(magnitudes, 5, 100.0, 0.5, 0.0, 0.0,
                                         1, false),
                200.0, 1.0e-12, "known percentile");
    const double previous[] = { 0.0, 1.0, 0.0 };
    const double current[] = { 0.0, 2.0, 1.0 };
    require(mir2::spectralFlux(current, previous, 3, true, false) > 1.4,
            "positive spectral flux");
}

void testFilterBankAndDCT() {
    double edges[42] {};
    double bands[40] {};
    std::vector<double> magnitude(513, 1.0);
    mir2::makeMelEdges(edges, 40, 20.0, 20000.0, 0);
    require(edges[0] >= 19.9 && edges[41] <= 20000.1,
            "Slaney mel edges are bounded");
    mir2::triangularBands(magnitude.data(), 513, 48000.0 / 1024.0,
                          edges, 40, 1, bands);
    for (double value : bands)
        require(std::isfinite(value) && value >= 0.0, "finite mel band");
    const double constant[] = { 1.0, 1.0, 1.0, 1.0 };
    requireNear(mir2::dctII(constant, 4, 0, true), 2.0, 1.0e-12,
                "orthonormal DCT coefficient zero");
    requireNear(mir2::dctII(constant, 4, 1, true), 0.0, 1.0e-12,
                "orthonormal DCT non-DC coefficient");
}

void testPitchAtRates() {
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0 }) {
        PitchHarness harness(4096);
        fillSine(harness.frame, sampleRate, 220.0);
        const double rms = harness.prepare();
        const auto yin = mir2::analyzeYIN(harness.workspace, sampleRate,
                                          40.0, 1000.0, 0.1, rms);
        const auto mpm = mir2::analyzeMPM(harness.workspace, sampleRate,
                                          40.0, 1000.0, 0.93, rms);
        requireNear(yin.frequency, 220.0, 2.0, "YIN sine frequency");
        requireNear(mpm.frequency, 220.0, 2.0, "MPM sine frequency");
        require(yin.confidence > 0.9 && mpm.confidence > 0.9,
                "pitch confidence on sine");
    }
}

void testPitchFrameMatrix() {
    struct Case {
        int frameSize;
        double frequency;
    };
    const Case cases[] = {
        { 256, 1000.0 }, { 512, 500.0 }, { 1024, 250.0 },
        { 2048, 220.0 }, { 4096, 110.0 }, { 8192, 55.0 }
    };
    for (const auto& item : cases) {
        PitchHarness harness(item.frameSize);
        fillSine(harness.frame, 48000.0, item.frequency);
        const double rms = harness.prepare();
        const auto yin = mir2::analyzeYIN(harness.workspace, 48000.0,
                                          item.frequency * 0.7,
                                          item.frequency * 1.4, 0.1, rms);
        const auto mpm = mir2::analyzeMPM(harness.workspace, 48000.0,
                                          item.frequency * 0.7,
                                          item.frequency * 1.4, 0.93, rms);
        const double tolerance = item.frequency * 0.015;
        requireNear(yin.frequency, item.frequency, tolerance,
                    "YIN frame-size matrix");
        requireNear(mpm.frequency, item.frequency, tolerance,
                    "MPM frame-size matrix");
        require(yin.octaveAmbiguity >= 0.0 && yin.octaveAmbiguity <= 1.0
                && mpm.octaveAmbiguity >= 0.0 && mpm.octaveAmbiguity <= 1.0,
                "octave ambiguity bounds");
    }
}

void testMissingFundamentalAndSilence() {
    constexpr double sampleRate = 48000.0;
    PitchHarness harness(4096);
    for (std::size_t index = 0; index < harness.frame.size(); ++index) {
        const double time = static_cast<double>(index) / sampleRate;
        double sample = 0.0;
        for (int harmonic = 2; harmonic <= 6; ++harmonic)
            sample += std::sin(mir2::kTwoPi * 100.0 * harmonic * time)
                / static_cast<double>(harmonic);
        harness.frame[index] = static_cast<float>(sample * 0.2);
    }
    double rms = harness.prepare();
    const auto mpm = mir2::analyzeMPM(harness.workspace, sampleRate,
                                      60.0, 1000.0, 0.9, rms);
    requireNear(mpm.frequency, 100.0, 3.0, "MPM missing fundamental");
    std::fill(harness.frame.begin(), harness.frame.end(), 0.0f);
    rms = harness.prepare();
    const auto silent = mir2::analyzeYIN(harness.workspace, sampleRate,
                                         40.0, 1000.0, 0.1, rms);
    require(silent.frequency == 0.0 && silent.confidence == 0.0
            && silent.voiced == 0.0, "silence pitch policy");
}

void testNoiseAndFiniteGuards() {
    PitchHarness harness(2048);
    std::mt19937 generator(12345);
    std::uniform_real_distribution<float> distribution(-0.5f, 0.5f);
    for (float& sample : harness.frame)
        sample = distribution(generator);
    const double rms = harness.prepare();
    const auto result = mir2::analyzeYIN(harness.workspace, 48000.0,
                                         50.0, 2000.0, 0.1, rms);
    require(std::isfinite(result.frequency) && std::isfinite(result.confidence),
            "noise remains finite");
    harness.frame[10] = std::numeric_limits<float>::quiet_NaN();
    harness.prepare();
    for (double value : harness.cmndf)
        require(std::isfinite(value), "NaN input is sanitized");
}

} // namespace

int main() {
    testFFT();
    testSpectralDescriptors();
    testFilterBankAndDCT();
    testPitchAtRates();
    testPitchFrameMatrix();
    testMissingFundamentalAndSilence();
    testNoiseAndFiniteGuards();
    std::cout << "MIR2_REFERENCE_TESTS_OK\n";
    return 0;
}
