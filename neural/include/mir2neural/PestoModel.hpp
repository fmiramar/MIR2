// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "PestoNetwork.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace mir2neural {

// Explicit, versioned float32 model buffer. Exported assets stay separate from
// the plugin: no filesystem, interpreter, model download or kernel generation
// occurs here. Kernel coefficients retain upstream float32 precision.
class Model {
public:
    static constexpr std::size_t headerSize = 32;
    static constexpr std::size_t descriptorSize = 5;
    static constexpr int maxWindow = 16384;

    bool bind(const float* data, std::size_t count, int expectedRate = 0) noexcept {
        valid_ = false;
        network = Network{};
        sampleRate = hopSamples = windowSamples = 0;
        frontendMACs = 0;
        if (!data || count < headerSize || count > 8000000) return false;
        for (std::size_t i = 0; i < count; ++i)
            if (!std::isfinite(data[i])) return false;
        if (data[0] != 734201.f || data[1] != 1.f) return false;
        const int sr = integer(data[2], 16000, 96000);
        if (sr != 16000 && sr != 44100 && sr != 48000 && sr != 96000) return false;
        if (expectedRate != 0 && sr != expectedRate) return false;
        const int hop = integer(data[3], 1, 960);
        const int width = integer(data[4], 1, maxWindow);
        if (hop != static_cast<int>(sr * 0.01 + 0.5) || width < hop
            || (width & (width - 1)) != 0) return false;
        if (data[5] != Network::inputBins || data[6] != Network::weightCount
            || data[8] != static_cast<float>(count)) return false;
        const int kernelCount = integer(data[7], 1, 7990000);
        if (kernelCount < 0) return false;
        for (std::size_t i = 9; i < headerSize; ++i)
            if (data[i] != 0.f) return false;
        const std::size_t tableOffset = headerSize + Network::weightCount;
        const std::size_t kernelOffset = tableOffset + Network::inputBins * descriptorSize;
        if (kernelOffset + static_cast<std::size_t>(kernelCount) != count) return false;
        for (int bin = 0; bin < Network::inputBins; ++bin) {
            const float* d = data + tableOffset + static_cast<std::size_t>(bin) * descriptorSize;
            const int start = integer(d[0], 0, width - 1);
            const int length = integer(d[1], 1, width);
            const int re = integer(d[2], 0, kernelCount - 1);
            const int im = integer(d[3], 0, kernelCount - 1);
            if (start < 0 || length < 0 || re < 0 || im < 0
                || start + length > width || re + length > kernelCount
                || im + length > kernelCount || d[4] <= 0.f
                || d[4] > std::sqrt(static_cast<float>(width)) + 1.f) return false;
            bins_[bin] = {start, length, data + kernelOffset + re,
                          data + kernelOffset + im, d[4]};
            frontendMACs += static_cast<std::size_t>(2 * length);
        }
        if (!network.bind(data + headerSize, Network::weightCount)) return false;
        sampleRate = sr;
        hopSamples = hop;
        windowSamples = width;
        valid_ = true;
        return true;
    }

    bool valid() const noexcept { return valid_; }
    std::size_t binCost(int bin) const noexcept {
        return static_cast<std::size_t>(2 * bins_[bin].length);
    }

    // Same cross-correlation and sqrt-length normalization as upstream nnAudio-
    // derived StreamingCQT; trimming zero taps changes no mathematical result.
    float logPowerBin(const float* frame, int bin) const noexcept {
        const Kernel& k = bins_[bin];
        float re = 0.f, im = 0.f;
        const float* x = frame + k.start;
        for (int i = 0; i < k.length; ++i) {
            re += x[i] * k.real[i];
            im += x[i] * k.imag[i];
        }
        re *= k.sqrtLength;
        im *= k.sqrtLength;
        const float magnitude = std::max(std::sqrt(re * re + im * im),
                                         std::numeric_limits<float>::epsilon());
        // Preserve ToLogMagnitude followed by the in-place dB conversion in
        // PESTO.forward, including float32 rounding order.
        return (20.f * std::log10(magnitude)) * (std::log(10.f) / 10.f);
    }

    void frontend(const float* frame, float* logPower) const noexcept {
        for (int i = 0; i < Network::inputBins; ++i)
            logPower[i] = logPowerBin(frame, i);
    }

    Network network;
    int sampleRate = 0;
    int hopSamples = 0;
    int windowSamples = 0;
    std::size_t frontendMACs = 0;

private:
    struct Kernel { int start, length; const float* real; const float* imag; float sqrtLength; };
    Kernel bins_[Network::inputBins]{};
    bool valid_ = false;
    static int integer(float value, int low, int high) noexcept {
        return std::isfinite(value) && value >= static_cast<float>(low)
            && value <= static_cast<float>(high) && value == std::floor(value)
            ? static_cast<int>(value) : -1;
    }
};

} // namespace mir2neural
