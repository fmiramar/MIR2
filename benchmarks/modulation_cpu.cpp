// SPDX-License-Identifier: GPL-3.0-or-later
// Build with: c++ -std=c++17 -O3 -Iinclude benchmarks/modulation_cpu.cpp -o modulation_cpu
#include "mir2/RhythmModulation.hpp"
#include <chrono>
#include <iostream>
#include <vector>
int main() {
    std::cout << "bands,bins,frames,projection_terms,mean_ms,max_ms\n";
    double sink=0;
    for(int bands : {2,24,32}) {
        mir2::ModulationConfig c; c.bands=bands;c.bins=bands==32?32:16;
        c.frames=bands==32?512:400;c.featureRate=100;c.minHz=.5;c.maxHz=8;
        c.normalization=1;c.fluctuation=true;c.smoothBands=.5;c.smoothBins=.5;
        if(!mir2::validModulationConfig(c)) return 1;
        std::vector<double> input(c.frames*c.bands),window(c.frames),re(c.frames*c.bins),im(re.size());
        std::vector<double> output(c.bands*c.bins),scratch(output.size());
        for(int band=0;band<c.bands;++band)for(int sample=0;sample<c.frames;++sample)
            input[band*c.frames+sample]=1+.5*std::cos(mir2::kTwoPi*(band%7+1)*sample/c.featureRate);
        const double sum=mir2::prepareModulationBasis(c,window.data(),re.data(),im.data());
        double elapsed=0,maximum=0;
        for(int iteration=0;iteration<200;++iteration) {
            const auto start=std::chrono::steady_clock::now();
            mir2::modulationSpectrum(input.data(),c,window.data(),sum,re.data(),im.data(),output.data(),nullptr);
            mir2::shapeModulationPattern(output.data(),scratch.data(),c);
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            elapsed+=ms;maximum=std::max(maximum,ms);sink+=output[iteration%output.size()];
        }
        std::cout<<c.bands<<','<<c.bins<<','<<c.frames<<','<<c.bands*c.bins*c.frames<<','<<elapsed/200<<','<<maximum<<'\n';
    }
    return sink>0?0:1;
}
