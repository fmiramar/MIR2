// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/ConstantQ.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
int main() {
    std::array<double,192> magnitudes{};
    std::array<double,12> result{};
    for (int bpo : {12,24,36,48}) for (double tuning : {415.,440.,466.}) {
        const int bands=4*bpo;
        const double low=55*tuning/440;
        for(int selected=0;selected<bands;++selected) {
            magnitudes.fill(0);magnitudes[selected]=.5;
            mir2::cqChroma(magnitudes.data(),bands,low,bpo,tuning,0,0,result.data());
            // A1 starts at MIDI 33; use integer arithmetic as an independent tie oracle.
            const int expected=(33+(24*selected+bpo)/(2*bpo))%12;
            for(int pc=0;pc<12;++pc) assert(std::abs(result[pc]-(pc==expected?.25:0))<1.e-12);
        }
    }
    // SynthDefs store controls as float32: retain documented upward half-bin ties.
    for (int midi=30;midi<90;++midi) for(double tuning:{415.3,440.,466.1}) {
        const double minimum=static_cast<float>(tuning*std::exp2((midi-69)/12.));
        magnitudes.fill(0);magnitudes[1]=1;
        mir2::cqChroma(magnitudes.data(),24,minimum,24,static_cast<float>(tuning),0,0,result.data());
        assert(result[(midi+1)%12]==1);
    }
    magnitudes.fill(0);magnitudes[0]=1;magnitudes[12]=2;magnitudes[3]=2;magnitudes[7]=.1;
    for(int normalization=0;normalization<4;++normalization) {
        mir2::cqChroma(magnitudes.data(),24,55,12,440,normalization,.01,result.data());
        const double divisor=normalization==0?1:(normalization==1?5:(normalization==2?9:std::sqrt(41.)));
        assert(std::abs(result[9]-5/divisor)<1.e-12);
        assert(std::abs(result[0]-4/divisor)<1.e-12);
        assert(result[4]==0);
    }
    magnitudes.fill(0);
    mir2::cqChroma(magnitudes.data(),24,55,12,440,3,0,result.data());
    for(double x:result)assert(x==0);
    // End-to-end kernel output versus direct time correlation + integer folding.
    for(double sr:{44100.,48000.,96000.}) {
        const double minimum=110;const int bpo=12,bands=36;
        const int n=mir2::cqFrameSize(sr,minimum,bpo,0);
        std::vector<mir2::Complex> work(n);std::vector<double> signal(n);
        int offsets[bands+1];
        int count=mir2::buildCQKernels(work.data(),n,sr,minimum,bpo,bands,0,1.e-4,offsets,nullptr);
        assert(count>0);std::vector<mir2::CQEntry> entries(count);
        mir2::buildCQKernels(work.data(),n,sr,minimum,bpo,bands,0,1.e-4,offsets,entries.data(),count);
        for(int i=0;i<n;++i){signal[i]=.3*std::sin(mir2::kTwoPi*220*i/sr)+.2*std::sin(mir2::kTwoPi*277.18263*i/sr);work[i]={signal[i],0};}
        mir2::fft(work.data(),n,false);
        mir2::constantQ(work.data(),entries.data(),offsets,bands,magnitudes.data());
        mir2::cqChroma(magnitudes.data(),bands,minimum,bpo,440,0,0,result.data());
        std::array<double,12> oracle{};
        for(int b=0;b<bands;++b){
            const double hz=minimum*std::exp2(b/12.);const int length=mir2::cqKernelLength(sr,hz,12,0);
            const int offset=(n-length)/2;double real=0,imag=0;
            for(int j=0;j<length;++j){const double w=(1-std::cos(mir2::kTwoPi*j/length))/length;
                const double angle=mir2::kTwoPi*hz*(j-(length-1)*.5)/sr;
                real+=signal[offset+j]*w*std::cos(angle);imag+=signal[offset+j]*w*std::sin(angle);}
            oracle[(45+b)%12]+=4*(real*real+imag*imag);
        }
        for(int pc=0;pc<12;++pc)assert(std::abs(oracle[pc]-result[pc])<1.e-4);
    }
    std::cout<<"ChromaCQT folding, tuning, tie, threshold, silence and direct correlation oracles passed\n";
}
