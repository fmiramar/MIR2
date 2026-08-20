// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <algorithm>
#include <cmath>

namespace mir2 {

struct Complex {
    double real;
    double imag;
};

inline void fft(Complex* data, int size, bool inverse) noexcept {
    for (int index = 1, reversed = 0; index < size; ++index) {
        int bit = size >> 1;
        for (; reversed & bit; bit >>= 1)
            reversed ^= bit;
        reversed ^= bit;
        if (index < reversed)
            std::swap(data[index], data[reversed]);
    }

    for (int length = 2; length <= size; length <<= 1) {
        const double angle = (inverse ? kTwoPi : -kTwoPi) / static_cast<double>(length);
        const double rootReal = std::cos(angle);
        const double rootImag = std::sin(angle);
        for (int start = 0; start < size; start += length) {
            double currentReal = 1.0;
            double currentImag = 0.0;
            const int half = length >> 1;
            for (int offset = 0; offset < half; ++offset) {
                const Complex even = data[start + offset];
                const Complex odd = data[start + offset + half];
                const double oddReal = odd.real * currentReal - odd.imag * currentImag;
                const double oddImag = odd.real * currentImag + odd.imag * currentReal;
                data[start + offset] = { even.real + oddReal, even.imag + oddImag };
                data[start + offset + half] = { even.real - oddReal, even.imag - oddImag };
                const double nextReal = currentReal * rootReal - currentImag * rootImag;
                currentImag = currentReal * rootImag + currentImag * rootReal;
                currentReal = nextReal;
            }
        }
    }

    if (inverse) {
        const double scale = 1.0 / static_cast<double>(size);
        for (int index = 0; index < size; ++index) {
            data[index].real *= scale;
            data[index].imag *= scale;
        }
    }
}

} // namespace mir2
