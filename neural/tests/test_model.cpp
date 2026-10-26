// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2neural/PestoModel.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

int main() {
    using namespace mir2neural;
    const std::size_t table = Model::headerSize + Network::weightCount;
    const std::size_t kernels = table + Network::inputBins * Model::descriptorSize;
    std::vector<float> pack(kernels + 2, 0.f);
    pack[0] = 734201; pack[1] = 1; pack[2] = 48000; pack[3] = 480;
    pack[4] = 8192; pack[5] = 251; pack[6] = Network::weightCount;
    pack[7] = 2; pack[8] = static_cast<float>(pack.size());
    for (int i = 0; i < Network::inputBins; ++i) {
        const auto at = table + static_cast<std::size_t>(i) * Model::descriptorSize;
        pack[at] = 4096; pack[at + 1] = 1; pack[at + 2] = 0;
        pack[at + 3] = 1; pack[at + 4] = 1;
    }
    pack[kernels] = 1.f;
    Model model;
    assert(model.bind(pack.data(), pack.size(), 48000));
    assert(!model.bind(pack.data(), pack.size(), 44100));
    assert(!model.valid());
    assert(!model.bind(nullptr, pack.size()));
    assert(!model.bind(pack.data(), 31));
    assert(!model.bind(pack.data(), pack.size() - 1));
    for (const auto offset : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(3),
                              std::size_t(4), std::size_t(5), std::size_t(6), std::size_t(7),
                              std::size_t(8), table, table + 1, table + 2, table + 3, table + 4}) {
        const auto saved = pack[offset];
        for (float bad : {-1.f, .5f, 1e20f, std::numeric_limits<float>::infinity(),
                          std::numeric_limits<float>::quiet_NaN()}) {
            // Some positive fractions are legitimate sqrt-length scalars.
            if (offset == table + 4 && bad == .5f) continue;
            pack[offset] = bad;
            assert(!model.bind(pack.data(), pack.size()));
        }
        pack[offset] = saved;
    }
    pack[9] = 1; assert(!model.bind(pack.data(), pack.size())); pack[9] = 0;
    pack[table + 1] = 8192; assert(!model.bind(pack.data(), pack.size())); pack[table + 1] = 1;
    pack[kernels] = std::numeric_limits<float>::quiet_NaN();
    assert(!model.bind(pack.data(), pack.size())); pack[kernels] = 1;
    assert(model.bind(pack.data(), pack.size()));
    std::vector<float> frame(8192, 0.f);
    float features[Network::inputBins];
    model.frontend(frame.data(), features);
    const float silence = 20.f * std::log10(std::numeric_limits<float>::epsilon()) * (std::log(10.f) / 10.f);
    for (float v : features) assert(v == silence);
    frame[4096] = .25f;
    model.frontend(frame.data(), features);
    for (float v : features) assert(std::abs(v - 2.f * std::log(.25f)) < 1e-6f);
    return 0;
}
