// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Temporal.hpp"
#include "mir2/Stereo.hpp"
#include "mir2/FeatureDynamics.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void near(double actual, double expected, double tolerance, const char* message) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
        std::cerr << "FAIL: " << message << ": got " << actual << ", expected " << expected << '\n';
        std::exit(1);
    }
}
}

int main() {
    const std::array<double, 4> constant { 1, 1, 1, 1 };
    auto envelope = mir2::envelopeStatistics(constant.data(), 4, 0.1, -60);
    near(envelope.centroid, 0.2, 1e-12, "constant centroid uses frame centres");
    near(envelope.spread, std::sqrt(0.0125), 1e-12, "uniform temporal spread");
    near(envelope.skewness, 0, 1e-12, "symmetric skewness");
    near(envelope.kurtosis, 1.64, 1e-12, "uniform discrete Pearson kurtosis");
    near(envelope.complexity, 0, 1e-12, "constant dynamics");
    near(envelope.active, 1, 1e-12, "constant active ratio");
    near(envelope.strongDecay, std::sqrt(2.0), 1e-12, "strong decay energy convention");
    const std::array<double, 4> zeros {};
    envelope = mir2::envelopeStatistics(zeros.data(), 4, 0.1, -60);
    near(envelope.silence, 1, 1e-12, "silent occupancy");
    near(envelope.active, 0, 1e-12, "silent effective duration");
    const std::array<double, 4> impulse { 1, 0, 0, 0 };
    envelope = mir2::envelopeStatistics(impulse.data(), 4, 0.1, -60);
    near(envelope.centroid, 0.05, 1e-12, "first-frame centroid");
    near(envelope.active, 0.25, 1e-12, "relative active occupancy");
    near(envelope.silence, 0.75, 1e-12, "absolute silent occupancy");
    std::array<double, 100> fade {};
    for (std::size_t i = 0; i < fade.size(); ++i) fade[i] = std::pow(10.0, (-30.0 + 6.0 * i * 0.01) / 20.0);
    envelope = mir2::envelopeStatistics(fade.data(), 100, 0.01, -60);
    near(envelope.fadeSlope, 6.0, 1e-10, "known exponential fade dB slope");
    near(envelope.fadeFit, 1.0, 1e-10, "exact fade fit");
    require(envelope.decrease < 0.0, "rising envelope has negative decrease");

    std::array<double, 21> triangle {};
    for (int i = 0; i <= 20; ++i) triangle[static_cast<std::size_t>(i)] = i <= 10 ? i * 0.1 : (20 - i) * 0.1;
    const auto event = mir2::measureAttack(triangle.data(), 21, 0.01, 0.1, 0.9);
    require(event.validAttack && event.validDecay, "triangle yields both intervals");
    near(event.attack, 0.08, 1e-12, "interpolated 10-90 attack");
    near(event.decay, 0.08, 1e-12, "interpolated 90-10 decay");
    near(event.slope, 10, 1e-12, "attack slope units");
    const auto truncated = mir2::measureAttack(triangle.data(), 12, 0.01, 0.1, 0.9);
    require(!truncated.validDecay, "incomplete decay is invalid");
    require(!mir2::measureAttack(zeros.data(), 4, 0.01, 0.1, 0.9).validAttack, "silence is not attack");

    mir2::StereoSums sums;
    for (int i = 0; i < 100; ++i) { const double x = std::sin(mir2::kTwoPi * i / 100); sums.add(x + 1, x + 3); }
    auto stereo = mir2::stereoMeasurements(sums, 100);
    near(stereo.correlation, 1, 1e-12, "Pearson removes unequal DC means");
    sums = {};
    for (int i = 0; i < 100; ++i) { const double x = std::sin(mir2::kTwoPi * i / 100); sums.add(x, x); }
    stereo = mir2::stereoMeasurements(sums, 100);
    near(stereo.width, 0, 1e-12, "mono width"); near(stereo.balance, 0, 1e-12, "mono balance");
    near(stereo.correlation, 1, 1e-12, "mono correlation"); near(stereo.midSideDB, 120, 1e-12, "mono M/S ceiling");
    sums = {};
    for (int i = 0; i < 100; ++i) { const double x = std::sin(mir2::kTwoPi * i / 100); sums.add(x, -x); }
    stereo = mir2::stereoMeasurements(sums, 100);
    near(stereo.width, 1, 1e-12, "antiphase width"); near(stereo.correlation, -1, 1e-12, "antiphase correlation");
    near(stereo.midSideDB, -120, 1e-12, "antiphase M/S floor");
    sums = {}; sums.add(1, 0); stereo = mir2::stereoMeasurements(sums, 1);
    near(stereo.balance, -1, 1e-12, "left-only balance");
    mir2::StereoBin bin;
    mir2::averageStereoBin(bin, {0, 1}, {1, 0}, 0);
    auto spectral = mir2::stereoSpectralMeasurements(&bin, 0, 0);
    near(spectral.phase, mir2::kPi / 2, 1e-12, "phase sign L leads R");
    near(spectral.coherence, 1, 1e-12, "coherent bin");
    near(spectral.phaseConcentration, 1, 1e-12, "single-bin concentration");
    mir2::averageStereoBin(bin, {0, -1}, {1, 0}, 0.5);
    spectral = mir2::stereoSpectralMeasurements(&bin, 0, 0);
    near(spectral.coherence, 0, 1e-12, "decorrelation requires cross-spectrum averaging");
    std::array<mir2::StereoBin, 2> bands {{{1, 0, 0, 0}, {0, 4, 0, 0}}};
    near(mir2::stereoSpectralMeasurements(bands.data(), 0, 0).panning, -1, 1e-12, "low-band left pan");
    near(mir2::stereoSpectralMeasurements(bands.data(), 1, 1).panning, 1, 1e-12, "high-band right pan");
    near(mir2::stereoSpectralMeasurements(bands.data(), 0, 1).panning, 0.6, 1e-12, "energy-weighted panorama");

    std::array<double, 5> ramp {{-2, -1, 0, 1, 2}};
    near(mir2::regressionDelta(ramp.data(), 2, 0.1), 10, 1e-12, "HTK delta slope seconds");
    std::array<double, 9> parabola {};
    std::array<double, 5> deltas {};
    for (int i = 0; i < 9; ++i) parabola[static_cast<std::size_t>(i)] = std::pow((i - 4) * 0.1, 2);
    for (int i = 0; i < 5; ++i) deltas[static_cast<std::size_t>(i)] = mir2::regressionDelta(parabola.data() + i, 2, 0.1);
    near(mir2::regressionDelta(deltas.data(), 2, 0.1), 2, 1e-12, "cascaded delta acceleration");
    std::array<double, 5> outlier {{1, 3, 1000, 2, 4}};
    near(mir2::featureMedian(outlier.data(), 5), 3, 1e-12, "median rejects isolated outlier");
    near(mir2::smoothFeature(0, 1, 1, 1), 1 - std::exp(-1), 1e-12, "one-pole 1/e time");
    near(mir2::smoothFeature(2, std::numeric_limits<double>::infinity(), 0, 0.01), 0, 1e-12, "nonfinite feature sanitization");

    constexpr int iterations = 2000;
    std::vector<double> benchmarkEnvelope(1000);
    for (int i = 0; i < 1000; ++i) benchmarkEnvelope[static_cast<std::size_t>(i)] = 0.1 + 0.09 * std::sin(i * 0.017);
    volatile double sink = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) sink = sink + mir2::envelopeStatistics(benchmarkEnvelope.data(), 1000, 0.01, -60).fadeSlope;
    const double microseconds = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / iterations;
    std::cout << "MIR2 temporal/stereo/feature analytical tests passed\n";
    std::cout << "Envelope full-statistics kernel, 1000 frames: " << microseconds << " us/update (" << sink << ")\n";
    return 0;
}
