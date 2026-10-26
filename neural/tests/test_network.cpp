// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2neural/PestoNetwork.hpp"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

int main(int argc, char** argv) {
    using namespace mir2neural;
    std::vector<float> weights(Network::weightCount, 0.0f);
    Network model;
    Workspace work;
    Result result;
    std::array<float, Network::inputBins> input {};
    std::array<float, Network::outputBins> activations {};
    assert(!model.infer(input.data(), work, result, activations.data()));
    assert(!model.bind(nullptr, weights.size()));
    assert(!model.bind(weights.data(), weights.size() - 1));
    weights[100] = std::numeric_limits<float>::quiet_NaN();
    assert(!model.bind(weights.data(), weights.size()));
    weights[100] = 0.0f;
    assert(model.bind(weights.data(), weights.size()));
    assert(model.infer(input.data(), work, result, activations.data()));
    // All-zero weights give a uniform posterior. Argmax selects bin zero;
    // clamped ALWA samples [0,0,0,1,2], so mean MIDI is exactly 0.2.
    assert(std::abs(result.midi - 0.2f) < 1e-6f);
    assert(result.confidence == 0.5f);
    assert(result.energy == 251.0f);
    for (float value : activations)
        assert(std::abs(value - 1.0f / 384.0f) < 1e-7f);
    input[100] = std::numeric_limits<float>::infinity();
    assert(!model.infer(input.data(), work, result, activations.data()));
    assert(result.frequency == 0.0f && result.confidence == 0.0f);
    for (float value : activations) assert(value == 0.0f);
    input[100] = 0.0f;
    input[0] = 1000.0f; // finite input whose energy cannot be represented
    assert(!model.infer(input.data(), work, result));
    input[0] = 0.0f;

    // The incremental scheduler must reproduce the full pass exactly, and its
    // charged work must sum to the declared per-frame budget.
    assert(model.infer(input.data(), work, result, activations.data()));
    const auto fullResult = result;
    Workspace sliced;
    assert(model.begin(input.data(), sliced));
    assert(model.step(sliced, 0) == Progress::running);
    std::size_t charged = 0;
    while (sliced.progress == Progress::running) {
        charged += Network::nextCost(sliced);
        model.stepOne(sliced);
    }
    assert(sliced.progress == Progress::done);
    assert(charged == Network::inferenceMACs && sliced.cumulativeMACs == charged);
    assert(sliced.result.frequency == fullResult.frequency);
    assert(sliced.result.confidence == fullResult.confidence);
    assert(sliced.result.energy == fullResult.energy);
    assert(sliced.probabilities == activations);

    // Finite inputs and weights can still overflow a confidence convolution.
    // Reject its NaN before ReLU can accidentally turn it into zero.
    weights[130461] = std::numeric_limits<float>::max();
    weights[130462] = -std::numeric_limits<float>::max();
    input.fill(1.f);
    assert(model.bind(weights.data(), weights.size()));
    assert(!model.infer(input.data(), work, result));
    assert(result.frequency == 0.f && result.confidence == 0.f);
    weights[130461] = weights[130462] = 0.f;
    input.fill(0.f);

    // Optional externally generated oracle (never includes weights in source).
    // weights: 130575 float32 values in Network::bind order.
    // records: 251 log-power values, frequency/confidence/energy/midi, 384 bins.
    if (argc == 3) {
        std::ifstream modelFile(argv[1], std::ios::binary);
        modelFile.read(reinterpret_cast<char*>(weights.data()),
                       static_cast<std::streamsize>(weights.size() * sizeof(float)));
        assert(modelFile && modelFile.peek() == std::ifstream::traits_type::eof());
        assert(model.bind(weights.data(), weights.size()));
        std::ifstream fixtures(argv[2], std::ios::binary);
        assert(fixtures);
        std::array<float, 4> expected {};
        std::array<float, Network::outputBins> expectedActivations {};
        int records = 0;
        double maxCents = 0.0, maxActivation = 0.0, maxConfidence = 0.0;
        while (fixtures.read(reinterpret_cast<char*>(input.data()), sizeof(input))) {
            fixtures.read(reinterpret_cast<char*>(expected.data()), sizeof(expected));
            fixtures.read(reinterpret_cast<char*>(expectedActivations.data()), sizeof(expectedActivations));
            assert(fixtures && model.infer(input.data(), work, result, activations.data()));
            const double cents = std::abs(1200.0 * std::log2(result.frequency / expected[0]));
            maxCents = std::max(maxCents, cents);
            maxConfidence = std::max(maxConfidence, static_cast<double>(std::abs(result.confidence - expected[1])));
            assert(cents < 0.05);
            assert(std::abs(result.confidence - expected[1]) < 1e-4f);
            assert(std::abs(result.energy - expected[2]) < 2e-5f * std::max(expected[2], 1e-20f));
            assert(std::abs(result.midi - expected[3]) < 0.0005f);
            for (int i = 0; i < Network::outputBins; ++i) {
                const auto error = static_cast<double>(std::abs(activations[static_cast<std::size_t>(i)] - expectedActivations[static_cast<std::size_t>(i)]));
                maxActivation = std::max(maxActivation, error);
                assert(error < 1e-4);
            }
            ++records;
        }
        assert(fixtures.eof() && fixtures.gcount() == 0 && records > 0);
        std::cout << "oracle records=" << records << " max_cents=" << maxCents
                  << " max_activation=" << maxActivation << " max_confidence=" << maxConfidence << '\n';
        std::vector<double> times;
        times.reserve(500);
        for (int trial = 0; trial < 520; ++trial) {
            const auto start = std::chrono::steady_clock::now();
            assert(model.infer(input.data(), work, result));
            const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            if (trial >= 20) times.push_back(us);
        }
        std::sort(times.begin(), times.end());
        std::cout << "infer_us median=" << times[250] << " p95=" << times[475]
                  << " p99=" << times[495] << " max=" << times.back()
                  << " workspace_bytes=" << sizeof(work) << '\n';
    }
    std::cout << "PESTO_NETWORK_OK\n";
}
