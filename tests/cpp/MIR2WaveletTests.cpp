// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Wavelet.hpp"
#include <cassert>
#include <vector>
#include <random>
#include <iostream>
int main() {
    std::mt19937 random(19); std::uniform_real_distribution<double> noise(-1,1);
    for(int order=1;order<=4;++order) for(int size:{32,64,256,1024,8192})
        for(int levels=1;levels<=mir2::waveletMaxLevels(size);++levels) {
            std::vector<double> input(static_cast<std::size_t>(size)),work(input.size()),scratch(input.size());
            for(auto& v:input)v=noise(random);
            work=input; const auto filter=mir2::daubechiesFilter(order);
            mir2::waveletForward(work.data(),scratch.data(),size,levels,filter);
            double energy=0,transformed=0;for(int i=0;i<size;++i){energy+=input[i]*input[i];transformed+=work[i]*work[i];}
            assert(std::abs(energy-transformed)<1.e-8*energy);
            double bands[14]{},previous[14]{};
            mir2::waveletEnergies(work.data(),size,levels,bands);
            double total=0;for(int i=0;i<=levels;++i)total+=bands[i];
            assert(std::abs(total-energy/size)<1.e-10);
            auto m=mir2::waveletMeasurements(bands,previous,levels,48000);
            assert(m.entropy>=0 && m.entropy<=1 && m.flux==0 && m.centroid>=0 && m.spread>=0);
            std::copy(bands,bands+14,previous);
            assert(mir2::waveletMeasurements(bands,previous,levels,48000).flux==0);
            mir2::waveletInverse(work.data(),scratch.data(),size,levels,filter);
            for(int i=0;i<size;++i)assert(std::abs(work[i]-input[i])<1.e-10);
        }
    double haar[]{1,1,-1,-1},scratch[4]{};
    mir2::waveletForward(haar,scratch,4,2,mir2::daubechiesFilter(1));
    assert(std::abs(haar[0])<1.e-14 && std::abs(haar[1]-2)<1.e-14 && haar[2]==0 && haar[3]==0);
    std::cout<<"Wavelet reconstruction, energy and Haar oracle passed\n";
}
