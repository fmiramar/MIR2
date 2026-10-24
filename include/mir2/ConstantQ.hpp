// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "FFT.hpp"
#include <limits>

namespace mir2 {
constexpr int kCQMaxBins = 192;
constexpr int kCQMaxEntries = 262144;
struct CQEntry { int bin; double real, imag; };
inline int cqKernelLength(double sampleRate, double frequency, int binsPerOctave,
                          double gamma) noexcept {
    if (!finite(sampleRate) || !finite(frequency) || !finite(gamma)
        || sampleRate <= 0.0 || frequency <= 0.0 || binsPerOctave < 1 || gamma < 0.0)
        return 0;
    const double bandwidth = (std::exp2(1.0 / binsPerOctave) - 1.0) * frequency + gamma;
    const double length = std::ceil(sampleRate / bandwidth);
    if (!finite(length) || length > std::numeric_limits<int>::max()) return 0;
    return std::max(4, static_cast<int>(length));
}
inline int cqFrameSize(double sampleRate, double minimum, int binsPerOctave,
                       double gamma, int limit = 32768) noexcept {
    if (!finite(sampleRate) || sampleRate <= 0 || limit < 32 || limit > 32768
        || !finite(minimum) || !finite(gamma) || minimum <= 0 || gamma < 0 || binsPerOctave < 1)
        return 0;
    const double bandwidth = (std::exp2(1.0 / binsPerOctave) - 1.0) * minimum + gamma;
    if (sampleRate / bandwidth > limit) return 0;
    int size = 32;
    while (size < cqKernelLength(sampleRate, minimum, binsPerOctave, gamma)) size *= 2;
    return size <= limit ? size : 0;
}
// Unit-sum periodic Hann analytic kernel, centered in the common trailing frame.
// Multiplying X by conjugate(FFT(kernel))/N implements its direct correlation.
inline void cqKernel(Complex* work, int frameSize, int length,
                     double frequency, double sampleRate) noexcept {
    std::fill(work, work + frameSize, Complex{0,0});
    const int offset = (frameSize - length) / 2;
    for (int i = 0; i < length; ++i) {
        const double window = (1.0 - std::cos(kTwoPi * i / length)) / length;
        const double angle = kTwoPi * frequency * (i - (length - 1) * 0.5) / sampleRate;
        work[offset + i] = {window * std::cos(angle), window * std::sin(angle)};
    }
    fft(work, frameSize, false);
}
// Invoke once with entries=nullptr to count; then fill exactly-sized storage.
inline int buildCQKernels(Complex* work, int frameSize, double sampleRate,
                         double minimum, int binsPerOctave, int bands, double gamma,
                         double sparsity, int* offsets, CQEntry* entries,
                         int capacity = kCQMaxEntries) noexcept {
    if (!work || !offsets || !isPowerOfTwo(frameSize) || frameSize < 32 || frameSize > 32768
        || bands < 2 || bands > kCQMaxBins || !finite(sparsity) || sparsity <= 0.0 || sparsity > 0.01)
        return -1;
    int count = 0; offsets[0] = 0;
    for (int band = 0; band < bands; ++band) {
        const double frequency = minimum * std::exp2(static_cast<double>(band) / binsPerOctave);
        const int length = cqKernelLength(sampleRate,frequency,binsPerOctave,gamma);
        // Keep the first Hann main-lobe null below Nyquist. Sidelobes remain finite.
        if (length < 4 || length > frameSize || frequency + 2.0 * sampleRate / length >= sampleRate * 0.5)
            return -1;
        cqKernel(work,frameSize,length,frequency,sampleRate);
        double peak = 0;
        for (int i=0;i<frameSize;++i) peak=std::max(peak,std::hypot(work[i].real,work[i].imag));
        for (int i=0;i<frameSize;++i) {
            if (std::hypot(work[i].real,work[i].imag) < peak * sparsity) continue;
            if (count >= capacity) return -1;
            if(entries) entries[count]={i,work[i].real/frameSize,-work[i].imag/frameSize};
            ++count;
        }
        offsets[band+1]=count;
    }
    return count;
}
inline void constantQ(const Complex* spectrum,const CQEntry* entries,const int* offsets,
                      int bands,double* magnitude) noexcept {
    for(int band=0;band<bands;++band){
        double real=0,imaginary=0;
        for(int j=offsets[band];j<offsets[band+1];++j){
            const auto& e=entries[j];const auto& x=spectrum[e.bin];
            real+=x.real*e.real-x.imag*e.imag;imaginary+=x.real*e.imag+x.imag*e.real;
        }
        magnitude[band]=2.0*std::hypot(real,imaginary);
    }
}
// Power folding into twelve C-first pitch classes. Half-semitone ties go up.
// A 4e-6-semitone tolerance absorbs float32 SynthDef frequency/reference rounding.
inline void cqChroma(const double* magnitude, int bands, double minimum, int binsPerOctave,
                     double tuning, int normalization, double threshold, double* chroma) noexcept {
    std::fill(chroma, chroma + 12, 0.0);
    const double firstMidi = 69.0 + 12.0 * std::log2(minimum / tuning);
    for (int band = 0; band < bands; ++band) {
        const int note = static_cast<int>(std::floor(firstMidi + 12.0 * band / binsPerOctave + 0.5 + 4.e-6));
        const int pc = (note % 12 + 12) % 12;
        chroma[pc] += magnitude[band] * magnitude[band];
    }
    const double maximum = *std::max_element(chroma, chroma + 12);
    double divisor = normalization == 1 ? maximum : 0.0;
    for (int pc = 0; pc < 12; ++pc) {
        if (chroma[pc] < maximum * threshold) chroma[pc] = 0.0;
        if (normalization == 2) divisor += chroma[pc];
        if (normalization == 3) divisor += chroma[pc] * chroma[pc];
    }
    if (normalization == 3) divisor = std::sqrt(divisor);
    if (normalization != 0 && divisor > 0.0)
        for (int pc = 0; pc < 12; ++pc) chroma[pc] /= divisor;
}
// Uniform-Hz interpolation of log power, followed by orthonormal DCT-II.
inline void cqCepstra(const double* magnitude,int bands,double minimum,int binsPerOctave,
                      double* uniform,int uniformCount,double* coefficients,int count,
                      bool include0,double floor) noexcept {
    const double maximum=minimum*std::exp2(static_cast<double>(bands-1)/binsPerOctave);
    for(int i=0;i<uniformCount;++i){
        const double hz=minimum+(maximum-minimum)*i/(uniformCount-1);
        const double coordinate=clamp(binsPerOctave*std::log2(hz/minimum),0.0,static_cast<double>(bands-1));
        const int left=static_cast<int>(coordinate),right=std::min(bands-1,left+1);
        const double leftHz = minimum * std::exp2(static_cast<double>(left) / binsPerOctave);
        const double rightHz = minimum * std::exp2(static_cast<double>(right) / binsPerOctave);
        const double mix = right == left ? 0.0 : clamp((hz - leftHz) / (rightHz - leftHz), 0.0, 1.0);
        uniform[i]=(1-mix)*std::log(std::max(floor,magnitude[left]*magnitude[left]))
            +mix*std::log(std::max(floor,magnitude[right]*magnitude[right]));
    }
    for(int c=0;c<count;++c){
        const int order=c+(include0?0:1);double sum=0;
        for(int i=0;i<uniformCount;++i)sum+=uniform[i]*std::cos(kPi*order*(i+.5)/uniformCount);
        coefficients[c]=sum*std::sqrt((order==0?1.0:2.0)/uniformCount);
    }
}
} // namespace mir2
