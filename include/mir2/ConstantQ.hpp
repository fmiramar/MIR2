// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "FFT.hpp"
#include "Spectral.hpp"

namespace mir2 {
constexpr int kCQMaxBins = 192;
constexpr int kCQMaxEntries = 262144;
struct CQEntry { int bin; double real, imag; };
inline int cqKernelLength(double sampleRate, double frequency, int binsPerOctave,
                          double gamma) noexcept {
    const double bandwidth = (std::exp2(1.0 / binsPerOctave) - 1.0) * frequency + gamma;
    return std::max(4, static_cast<int>(std::ceil(sampleRate / bandwidth)));
}
inline int cqFrameSize(double sampleRate, double minimum, int binsPerOctave,
                       double gamma, int limit = 32768) noexcept {
    if (!finite(minimum) || !finite(gamma) || minimum <= 0 || gamma < 0 || binsPerOctave < 1)
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
    int count = 0; offsets[0] = 0;
    for (int band = 0; band < bands; ++band) {
        const double frequency = minimum * std::exp2(static_cast<double>(band) / binsPerOctave);
        const int length = cqKernelLength(sampleRate,frequency,binsPerOctave,gamma);
        if (length > frameSize || frequency >= sampleRate * 0.5) return -1;
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
// Uniform-Hz interpolation of log power, followed by orthonormal DCT-II.
inline void cqCepstra(const double* magnitude,int bands,double minimum,int binsPerOctave,
                      double* uniform,int uniformCount,double* coefficients,int count,
                      bool include0,double floor) noexcept {
    const double maximum=minimum*std::exp2(static_cast<double>(bands-1)/binsPerOctave);
    for(int i=0;i<uniformCount;++i){
        const double hz=minimum+(maximum-minimum)*i/(uniformCount-1);
        const double coordinate=clamp(binsPerOctave*std::log2(hz/minimum),0.0,static_cast<double>(bands-1));
        const int left=static_cast<int>(coordinate),right=std::min(bands-1,left+1);
        const double mix=coordinate-left;
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
