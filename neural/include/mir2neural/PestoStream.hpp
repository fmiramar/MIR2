// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PestoModel.hpp"
#include <array>
#include <cstdint>

namespace mir2neural {

// A deterministic one-hop pipeline. At each hop boundary a trailing window is
// frozen; its work is distributed over the following hop and published exactly
// at that next boundary. No threads, allocation, locks, I/O or host clock reads.
// Public results include the same startup padding as the reference. The UGen
// adds a full-history warmup gate and a confidence threshold separately.
class Stream {
public:
    bool bind(const float* modelData, std::size_t count, int sampleRate) noexcept {
        reset();
        if (!model.bind(modelData, count, sampleRate)) return false;
        // Direct frontend dot products have much lower throughput than the
        // blocked network kernel. Measured cost weighting prevents the entire
        // frontend from bunching into the first callback of each hop.
        totalCost_ = model.frontendMACs * frontendWeight + Network::inferenceMACs;
        return true;
    }

    void reset() noexcept {
        ring_.fill(0.f);
        frame_.fill(0.f);
        result = {};
        updates = 0;
        samples = 0;
        resultInputSamples = 0;
        write_ = elapsed_ = bin_ = 0;
        consumed_ = 0;
        active_ = false;
        failed_ = false;
        networkBegun_ = false;
    }

    // Block boundaries may fall anywhere relative to the fixed 10 ms hop.
    void process(const float* input, int count) noexcept {
        if (!model.valid() || count <= 0 || !input) return;
        int offset = 0;
        while (offset < count) {
            const int chunk = std::min(count - offset, model.hopSamples - elapsed_);
            for (int i = 0; i < chunk; ++i) {
                const float x = input[offset + i];
                ring_[static_cast<std::size_t>(write_)] = std::isfinite(x) ? x : 0.f;
                if (++write_ == model.windowSamples) write_ = 0;
            }
            elapsed_ += chunk;
            samples += static_cast<std::uint64_t>(chunk);
            offset += chunk;
            if (active_) advance(static_cast<std::size_t>(elapsed_) * totalCost_
                                  / static_cast<std::size_t>(model.hopSamples));
            if (elapsed_ == model.hopSamples) {
                if (active_) {
                    // Every atomic job has been credited by this boundary.
                    result = !failed_ && work_.progress == Progress::done ? work_.result : Result{};
                    resultInputSamples = samples - static_cast<std::uint64_t>(model.hopSamples);
                    ++updates;
                }
                for (int i = 0; i < model.windowSamples; ++i) {
                    const int source = (write_ + i) % model.windowSamples;
                    frame_[static_cast<std::size_t>(i)] = ring_[static_cast<std::size_t>(source)];
                }
                elapsed_ = bin_ = 0;
                consumed_ = 0;
                active_ = true;
                failed_ = networkBegun_ = false;
            }
        }
    }

    bool ready() const noexcept {
        return updates > 0 && resultInputSamples >= static_cast<std::uint64_t>(model.windowSamples);
    }
    Model model;
    Result result{};
    std::uint64_t samples = 0;
    std::uint64_t updates = 0;
    std::uint64_t resultInputSamples = 0;

private:
    static constexpr std::size_t frontendWeight = 8;
    std::array<float, Model::maxWindow> ring_{};
    std::array<float, Model::maxWindow> frame_{};
    std::array<float, Network::inputBins> logPower_{};
    Workspace work_{};
    std::size_t totalCost_ = 0, consumed_ = 0;
    int write_ = 0, elapsed_ = 0, bin_ = 0;
    bool active_ = false, failed_ = false, networkBegun_ = false;

    void advance(std::size_t target) noexcept {
        if (failed_) return;
        while (bin_ < Network::inputBins) {
            const auto cost = model.binCost(bin_) * frontendWeight;
            if (cost > target - consumed_) return;
            logPower_[static_cast<std::size_t>(bin_)] = model.logPowerBin(frame_.data(), bin_);
            consumed_ += cost;
            ++bin_;
        }
        if (!networkBegun_) {
            if (!model.network.begin(logPower_.data(), work_)) { failed_ = true; return; }
            networkBegun_ = true;
        }
        while (work_.progress == Progress::running) {
            const auto cost = Network::nextCost(work_);
            if (cost > target - consumed_) return;
            const auto status = model.network.stepOne(work_);
            consumed_ += cost;
            if (status == Progress::failed) { failed_ = true; return; }
        }
    }
};
} // namespace mir2neural
