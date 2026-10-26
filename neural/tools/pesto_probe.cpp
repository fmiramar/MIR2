// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2neural/PestoModel.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using Clock = std::chrono::steady_clock;

static std::vector<float> read(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto bytes = file.tellg();
    if (!file || bytes <= 0 || bytes % 4 != 0 || bytes > 256 * 1024 * 1024)
        throw std::runtime_error("Invalid float32 file: " + path);
    std::vector<float> result(static_cast<std::size_t>(bytes) / sizeof(float));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(result.data()), bytes))
        throw std::runtime_error("Cannot read " + path);
    return result;
}

static void write(std::ofstream& file, const float* data, std::size_t count) {
    file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(count * sizeof(float)));
    if (!file) throw std::runtime_error("Cannot write result");
}

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("Usage: pesto_probe model.f32 audio.f32 output-prefix");
        const auto initStart = Clock::now();
        const auto weights = read(argv[1]);
        mir2neural::Model model;
        if (!model.bind(weights.data(), weights.size())) throw std::runtime_error("Invalid model pack");
        mir2neural::Workspace workspace{};
        std::vector<float> frame(static_cast<std::size_t>(model.windowSamples), 0.f);
        const double initMs = std::chrono::duration<double, std::milli>(Clock::now() - initStart).count();
        const auto audio = read(argv[2]);
        const std::size_t hop = static_cast<std::size_t>(model.hopSamples);
        if (audio.size() % hop) throw std::runtime_error("Audio must contain complete hops");
        const std::string prefix = argv[3];
        for (const auto& ext : {".logpower.f32", ".activations.f32", ".results.f32", ".timing.csv"})
            if (std::ifstream(prefix + ext).good()) throw std::runtime_error("Refusing to overwrite " + prefix + ext);
        std::ofstream features(prefix + ".logpower.f32", std::ios::binary);
        std::ofstream activations(prefix + ".activations.f32", std::ios::binary);
        std::ofstream results(prefix + ".results.f32", std::ios::binary);
        std::ofstream timing(prefix + ".timing.csv");
        timing << "frame,frontend_us,network_us,total_us\n";
        float logPower[mir2neural::Network::inputBins];
        float probabilities[mir2neural::Network::outputBins];
        std::vector<double> times;
        for (std::size_t offset = 0; offset < audio.size(); offset += hop) {
            const auto start = Clock::now();
            std::move(frame.begin() + static_cast<std::ptrdiff_t>(hop), frame.end(), frame.begin());
            std::copy_n(audio.data() + offset, hop, frame.end() - static_cast<std::ptrdiff_t>(hop));
            model.frontend(frame.data(), logPower);
            const auto frontendEnd = Clock::now();
            mir2neural::Result result{};
            if (!model.network.infer(logPower, workspace, result, probabilities))
                throw std::runtime_error("Inference failed");
            const auto end = Clock::now();
            const double total = std::chrono::duration<double, std::micro>(end - start).count();
            times.push_back(total);
            timing << offset / hop << ','
                   << std::chrono::duration<double, std::micro>(frontendEnd - start).count() << ','
                   << std::chrono::duration<double, std::micro>(end - frontendEnd).count() << ',' << total << '\n';
            const float row[] = {result.frequency, result.confidence, result.energy, result.midi};
            write(features, logPower, mir2neural::Network::inputBins);
            write(activations, probabilities, mir2neural::Network::outputBins);
            write(results, row, 4);
        }
        std::sort(times.begin(), times.end());
        auto percentile = [&](double p) { return times[static_cast<std::size_t>(p * static_cast<double>(times.size() - 1))]; };
        std::cout << std::setprecision(7) << "{\"frames\":" << times.size()
                  << ",\"init_ms\":" << initMs << ",\"model_bytes\":" << weights.size() * sizeof(float)
                  << ",\"state_bytes\":" << sizeof(model) + sizeof(workspace) + frame.size() * sizeof(float)
                  << ",\"median_us\":" << percentile(.5) << ",\"p95_us\":" << percentile(.95)
                  << ",\"p99_us\":" << percentile(.99) << ",\"max_us\":" << times.back() << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
