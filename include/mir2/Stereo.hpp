// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "FFT.hpp"

namespace mir2 {

struct StereoSums {
    double left = 0.0, right = 0.0, ll = 0.0, rr = 0.0, lr = 0.0;
    void add(double l, double r, double sign = 1.0) noexcept {
        left += sign * l; right += sign * r;
        ll += sign * l * l; rr += sign * r * r; lr += sign * l * r;
    }
};

struct StereoMeasurements { double balance, width, correlation, midSideDB; };

inline StereoMeasurements stereoMeasurements(const StereoSums& sum, int count) noexcept {
    if (count < 1 || sum.ll + sum.rr <= 1.0e-24) return {0.0, 0.0, 0.0, 0.0};
    const double total = std::max(1.0e-24, sum.ll + sum.rr);
    const double mid = std::max(0.0, 0.25 * (total + 2.0 * sum.lr));
    const double side = std::max(0.0, 0.25 * (total - 2.0 * sum.lr));
    const double lvar = std::max(0.0, sum.ll - sum.left * sum.left / count);
    const double rvar = std::max(0.0, sum.rr - sum.right * sum.right / count);
    const double covariance = sum.lr - sum.left * sum.right / count;
    const double corr = lvar * rvar > 1.0e-24 ? covariance / std::sqrt(lvar * rvar) : 0.0;
    return {clamp((sum.rr - sum.ll) / total, -1.0, 1.0),
            std::sqrt(clamp(side / (mid + side), 0.0, 1.0)),
            clamp(corr, -1.0, 1.0),
            clamp(10.0 * std::log10((mid + 1.0e-24) / (side + 1.0e-24)), -120.0, 120.0)};
}

struct StereoBin { double ll = 0.0, rr = 0.0, crossReal = 0.0, crossImag = 0.0; };

inline void averageStereoBin(StereoBin& state, Complex left, Complex right,
                             double retention) noexcept {
    const double fresh = 1.0 - retention;
    state.ll = retention * state.ll + fresh * (left.real * left.real + left.imag * left.imag);
    state.rr = retention * state.rr + fresh * (right.real * right.real + right.imag * right.imag);
    state.crossReal = retention * state.crossReal + fresh * (left.real * right.real + left.imag * right.imag);
    state.crossImag = retention * state.crossImag + fresh * (left.imag * right.real - left.real * right.imag);
}

struct StereoSpectralMeasurements { double coherence, panning, phase, phaseConcentration; };

inline StereoSpectralMeasurements stereoSpectralMeasurements(const StereoBin* bins,
                                                             int first, int last) noexcept {
    double energy = 0.0, coherenceEnergy = 0.0, coherence = 0.0, pan = 0.0;
    double real = 0.0, imag = 0.0, crossWeight = 0.0;
    for (int k = first; k <= last; ++k) {
        const auto& bin = bins[k];
        const double weight = bin.ll + bin.rr;
        const double lm = std::sqrt(std::max(0.0, bin.ll));
        const double rm = std::sqrt(std::max(0.0, bin.rr));
        if (weight <= 1.0e-24) continue;
        energy += weight;
        pan += weight * (rm - lm) / std::max(1.0e-24, rm + lm);
        const double crossPower = bin.crossReal * bin.crossReal + bin.crossImag * bin.crossImag;
        if (bin.ll * bin.rr > 1.0e-24) {
            const double sharedWeight = 2.0 * lm * rm;
            coherence += sharedWeight * clamp(crossPower / (bin.ll * bin.rr), 0.0, 1.0);
            coherenceEnergy += sharedWeight;
        }
        real += bin.crossReal; imag += bin.crossImag;
        crossWeight += std::sqrt(crossPower);
    }
    const double length = std::hypot(real, imag);
    return {coherenceEnergy > 1.0e-24 ? coherence / coherenceEnergy : 0.0,
            energy > 1.0e-24 ? clamp(pan / energy, -1.0, 1.0) : 0.0,
            length > 1.0e-24 ? std::atan2(imag, real) : 0.0,
            crossWeight > 1.0e-24 ? clamp(length / crossWeight, 0.0, 1.0) : 0.0};
}

} // namespace mir2
