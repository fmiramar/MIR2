// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Wavelet.hpp"
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
#include <chrono>
int main() {
    std::mt19937 random(19); std::uniform_real_distribution<double> noise(-1,1);
    for(int order=1;order<=4;++order) for(int size:{32,64,256,1024,8192})
        for(int levels=1;levels<=mir2::waveletMaxLevels(size);++levels) {
            std::vector<double> input(static_cast<std::size_t>(size)),work(input.size()),scratch(input.size());
            for(auto& v:input)v=noise(random);
            work=input; const auto filter=mir2::daubechiesFilter(order);
            mir2::waveletForward(work.data(),scratch.data(),size,levels,filter);
            double energy=0,transformed=0;for(int i=0;i<size;++i){energy+=input[static_cast<std::size_t>(i)]*input[static_cast<std::size_t>(i)];transformed+=work[static_cast<std::size_t>(i)]*work[static_cast<std::size_t>(i)];}
            assert(std::abs(energy-transformed)<1.e-8*energy);
            double bands[14]{},previous[14]{};
            mir2::waveletEnergies(work.data(),size,levels,bands);
            double total=0;for(int i=0;i<=levels;++i)total+=bands[i];
            assert(std::abs(total-energy/size)<1.e-10);
            auto m=mir2::waveletMeasurements(bands,previous,levels,48000);
            assert(m.entropy>=0 && m.entropy<=1 && m.flux>0 && m.centroid>=0 && m.spread>=0);
            std::copy(bands,bands+14,previous);
            assert(mir2::waveletMeasurements(bands,previous,levels,48000).flux==0);
            mir2::waveletInverse(work.data(),scratch.data(),size,levels,filter);
            for(int i=0;i<size;++i)assert(std::abs(work[static_cast<std::size_t>(i)]-input[static_cast<std::size_t>(i)])<1.e-10);
        }
    double haar[]{1,1,-1,-1},scratch[4]{};
    mir2::waveletForward(haar,scratch,4,2,mir2::daubechiesFilter(1));
    assert(std::abs(haar[0])<1.e-14 && std::abs(haar[1]-2)<1.e-14 && haar[2]==0 && haar[3]==0);
    // Exercise the production streaming adapter through different host block boundaries.
    for (int frameSize : {32, 128, 1024}) for (int blockSize : {17, 64, 127, 256}) {
        const auto n = static_cast<std::size_t>(frameSize);
        std::vector<double> input(n * 7), frame(n), output(n), scratchBuffer(n);
        for (auto& value : input) value = noise(random);
        int position = 0;
        for (int block = 0; block < frameSize * 7; block += blockSize) {
            const int limit = std::min(frameSize * 7, block + blockSize);
            for (int sample = block; sample < limit; ++sample) {
                const double actual = mir2::waveletReconstructSample(input[static_cast<std::size_t>(sample)],
                    frame.data(), output.data(), scratchBuffer.data(), position, frameSize,
                    mir2::waveletMaxLevels(frameSize), mir2::daubechiesFilter(4), 0);
                const double expected = sample < frameSize ? 0.0 : input[static_cast<std::size_t>(sample - frameSize)];
                assert(std::abs(actual - expected) < 1e-10);
            }
        }
    }
    // Independent filter invariants, including all even-shift orthogonality products.
    for (int order = 1; order <= 4; ++order) {
        const auto filter = mir2::daubechiesFilter(order);
        double sum = 0.0;
        for (int i = 0; i < filter.taps; ++i) sum += filter.low[i];
        assert(std::abs(sum - std::sqrt(2.0)) < 1e-13);
        for (int shift = 0; shift < filter.taps; shift += 2) {
            double product = 0.0;
            for (int i = 0; i + shift < filter.taps; ++i) product += filter.low[i] * filter.low[i + shift];
            assert(std::abs(product - (shift == 0 ? 1.0 : 0.0)) < 1e-13);
        }
    }
    double silent[4]{}, oneScale[4]{1,0,0,0};
    assert(mir2::waveletMeasurements(oneScale,silent,3,48000).flux == 1.0);
    std::vector<double> benchmark(1024), work(1024), temporary(1024);
    for (auto& value : benchmark) value = noise(random);
    volatile double sink = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 10000; ++iteration) {
        work = benchmark;
        mir2::waveletForward(work.data(),temporary.data(),1024,6,mir2::daubechiesFilter(4));
        mir2::waveletInverse(work.data(),temporary.data(),1024,6,mir2::daubechiesFilter(4));
        sink = sink + work[0];
    }
    const double micros = std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/10000;
    std::cout<<"Wavelet reconstruction, streaming block invariance, energy and Haar oracle passed\n";
    std::cout<<"db4 forward+inverse 1024/6: "<<micros<<" us/frame ("<<sink<<")\n";
}
