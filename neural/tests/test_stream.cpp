// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2neural/PestoStream.hpp"
#include <cassert>
#include <limits>
#include <vector>

int main() {
    using namespace mir2neural;
    const auto table = Model::headerSize + Network::weightCount;
    const auto taps = table + Network::inputBins * Model::descriptorSize;
    std::vector<float> pack(taps + 2, 0.f);
    pack[0] = 734201; pack[1] = 1; pack[2] = 44100; pack[3] = 441;
    pack[4] = 8192; pack[5] = 251; pack[6] = Network::weightCount;
    pack[7] = 2; pack[8] = static_cast<float>(pack.size());
    for (int i = 0; i < Network::inputBins; ++i) {
        const auto d = table + static_cast<std::size_t>(i) * Model::descriptorSize;
        pack[d] = 8191; pack[d + 1] = 1; pack[d + 2] = 0; pack[d + 3] = 1; pack[d + 4] = 1;
    }
    pack[taps] = 1;
    constexpr int hop = 441, frames = 22;
    std::vector<float> audio(hop * (frames + 1), 0.f);
    for (int n = 0; n < frames * hop; ++n)
        audio[static_cast<std::size_t>(n)] = .001f * static_cast<float>(n % 997);
    // At a hop endpoint, NaN is sanitized to silence by the streaming wrapper.
    audio[hop - 1] = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> baseline;
    for (int block : {1, 32, 64, 128, 256}) {
        Stream stream;
        assert(stream.bind(pack.data(), pack.size(), 44100));
        std::uint64_t seen = 0;
        std::vector<float> energies;
        for (int offset = 0; offset < static_cast<int>(audio.size()); offset += block) {
            const int count = std::min(block, static_cast<int>(audio.size()) - offset);
            stream.process(audio.data() + offset, count);
            const auto completedHop = stream.samples / hop;
            assert(stream.updates == (completedHop > 0 ? completedHop - 1 : 0));
            assert(stream.ready() == (stream.resultInputSamples >= 8192 && stream.updates > 0));
            if (stream.updates != seen) {
                assert(stream.updates == seen + 1);
                energies.push_back(stream.result.energy);
                const auto index = static_cast<std::size_t>(stream.updates * hop - 1);
                const float x = std::isfinite(audio[index]) ? audio[index] : 0.f;
                const float expected = 251.f * x * x;
                assert(std::abs(stream.result.energy - expected) < 1e-3f);
                seen = stream.updates;
            }
        }
        assert(energies.size() == frames);
        if (baseline.empty()) baseline = energies;
        else assert(energies == baseline);
        stream.reset();
        assert(stream.samples == 0 && stream.updates == 0 && !stream.ready());
    }
}
