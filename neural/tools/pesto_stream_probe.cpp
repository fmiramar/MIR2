// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2neural/PestoStream.hpp"
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

int main(int argc, char** argv) {
    try {
        if (argc != 5)
            throw std::runtime_error("Usage: pesto_stream_probe model.f32 audio.f32 block-size output-prefix");
        std::size_t parsed = 0;
        const int blockSize = std::stoi(argv[3], &parsed);
        if (argv[3][parsed] != '\0' || blockSize <= 0 || blockSize > 960)
            throw std::runtime_error("Invalid block size");
        const auto start = Clock::now();
        const auto model = read(argv[1]);
        mir2neural::Stream stream;
        if (!stream.bind(model.data(), model.size(), 0))
            throw std::runtime_error("Invalid model pack");
        const auto initMs = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        const auto hop = static_cast<std::size_t>(stream.model.hopSamples);
        if (static_cast<std::size_t>(blockSize) > hop)
            throw std::runtime_error("Probe block size must not exceed a hop");
        auto input = read(argv[2]);
        if (input.size() % hop != 0)
            throw std::runtime_error("Audio must contain complete hops");
        const auto expectedFrames = input.size() / hop;
        const auto expectedSamples = input.size();
        // A single zero hop publishes the final input frame. Round up to a
        // complete server block so every measured call has the same deadline.
        const auto block = static_cast<std::size_t>(blockSize);
        input.resize(((input.size() + hop + block - 1) / block) * block, 0.f);
        const std::string prefix = argv[4];
        for (const auto& extension : {".results.f32", ".timing.csv"})
            if (std::ifstream(prefix + extension).good())
                throw std::runtime_error("Refusing to overwrite " + prefix + extension);
        std::ofstream output(prefix + ".results.f32", std::ios::binary);
        std::ofstream timing(prefix + ".timing.csv");
        if (!output || !timing) throw std::runtime_error("Cannot open probe output");
        timing << "block,end_sample,total_us,published,source_end_sample,ready\n";
        std::vector<double> warmed;
        warmed.reserve(input.size() / block);
        std::uint64_t updates = 0;
        const double deadline = blockSize * 1e6 / stream.model.sampleRate;
        std::size_t misses = 0;
        for (std::size_t offset = 0; offset < input.size(); offset += block) {
            const auto before = Clock::now();
            stream.process(input.data() + offset, blockSize);
            const auto after = Clock::now();
            const auto microseconds = std::chrono::duration<double, std::micro>(after - before).count();
            const auto published = stream.updates - updates;
            if (published > 1) throw std::runtime_error("Probe lost an intermediate stream output");
            if (published == 1) {
                updates = stream.updates;
                const float row[] = {stream.result.frequency, stream.result.confidence,
                                     stream.result.energy, stream.result.midi};
                output.write(reinterpret_cast<const char*>(row), sizeof(row));
                if (!output) throw std::runtime_error("Cannot write probe output");
                if (stream.resultInputSamples != updates * hop)
                    throw std::runtime_error("Stream timestamp does not match fixed one-hop latency");
            }
            timing << offset / block << ',' << offset + block << ',' << microseconds << ','
                   << published << ',' << stream.resultInputSamples << ',' << stream.ready() << '\n';
            if (offset >= 11 * hop) {
                warmed.push_back(microseconds);
                if (microseconds > deadline) ++misses;
            }
        }
        if (!timing || updates != expectedFrames || stream.resultInputSamples != expectedSamples)
            throw std::runtime_error("Stream frame count or final timestamp differs");
        if (warmed.empty()) throw std::runtime_error("Need more than 11 input hops for warmed timing");
        std::sort(warmed.begin(), warmed.end());
        auto percentile = [&](double p) {
            return warmed[static_cast<std::size_t>(p * static_cast<double>(warmed.size() - 1))];
        };
        std::cout << std::setprecision(8) << "{\"frames\":" << updates
                  << ",\"block_size\":" << blockSize << ",\"init_ms\":" << initMs
                  << ",\"state_bytes\":" << sizeof(stream) << ",\"model_bytes\":" << model.size() * sizeof(float)
                  << ",\"warmed_blocks\":" << warmed.size()
                  << ",\"median_us\":" << percentile(.5) << ",\"p95_us\":" << percentile(.95)
                  << ",\"p99_us\":" << percentile(.99) << ",\"max_us\":" << warmed.back()
                  << ",\"deadline_us\":" << deadline << ",\"deadline_misses\":" << misses << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
