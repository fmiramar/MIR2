// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/PNCC.hpp"
#include "mir2/FFT.hpp"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <array>
#include <complex>
#ifndef MIR2_TEST_SOURCE_DIR
#define MIR2_TEST_SOURCE_DIR "."
#endif
int main(){
    std::ifstream fixture(std::string(MIR2_TEST_SOURCE_DIR)+"/tests/reference/pncc-stages.csv");assert(fixture.good());
    std::string line;std::getline(fixture,line);
    mir2::PNCCState state;state.initialize(8,.01);double powers[8]{},coefficients[4]{};
    for(int t=0;t<160;++t){
        for(int b=0;b<8;++b){
            double power=(b+1)*.02;
            if(t<8||(t>=100&&t<120))power=0;
            if(t>=25&&t<60 && (b==2||b==3))power*=3;
            if(t>=45&&t<70)power*=1+.6*std::pow(std::sin(t*.7+b),2);
            if(t>=70)power*=10;
            powers[b]=power;
        }
        const bool ready=state.process(powers);assert(ready==(t>=2));
        if(!ready)continue;
        state.cepstra(coefficients,4,true);
        assert(static_cast<bool>(std::getline(fixture,line)));std::stringstream fields(line);std::string field;std::vector<double> expected;
        while(std::getline(fields,field,','))expected.push_back(std::stod(field));
        assert(expected.size()==38 && expected[0]==t);
        auto near=[](double a,double b){assert(std::abs(a-b)<1.e-10*std::max(1.,std::abs(b)));};
        near(state.mean,expected[1]);
        for(int b=0;b<8;++b){near(state.medium[b],expected[static_cast<std::size_t>(2+b)]);near(state.low[b],expected[static_cast<std::size_t>(10+b)]);near(state.floor[b],expected[static_cast<std::size_t>(18+b)]);near(state.weighted[b],expected[static_cast<std::size_t>(26+b)]);}
        for(int c=0;c<4;++c)near(coefficients[c],expected[static_cast<std::size_t>(34+c)]);
    }
    for(double sr:{44100.,48000.,96000.})for(int fftSize:{1024,2048,4096,8192}){
        const int bins=fftSize/2+1,bands=40;std::vector<double> weights(static_cast<std::size_t>(bands*bins));
        mir2::pnccWeights(weights.data(),bins,sr,fftSize,bands,200,8000);
        for(int b=0;b<bands;++b){double sum=0;for(int i=0;i<bins;++i){const double w=weights[static_cast<std::size_t>(b*bins+i)];assert(std::isfinite(w)&&w>=0);sum+=w;}assert(std::abs(sum-1)<1.e-10);}
    }
    // Nonzero startup checks the published 0.9*Q initialization, which a
    // silence-leading fixture cannot distinguish from zero initialization.
    mir2::PNCCState start; start.initialize(4, .01);
    double nonzero[]{1,2,3,4};
    assert(!start.process(nonzero)); assert(!start.process(nonzero));
    assert(start.process(nonzero));
    for(int band=0;band<4;++band){
        assert(std::abs(start.medium[band]-.6*nonzero[band])<1e-14);
        assert(std::abs(start.low[band]-.54*nonzero[band])<1e-14);
        assert(std::abs(start.floor[band]-.06*nonzero[band])<1e-14);
        assert(std::abs(start.weighted[band]-.1*nonzero[band])<1e-14);
    }
    assert(std::abs(start.mean-.25)<1e-14);
    mir2::PNCCState faster, slower;
    faster.initialize(4,.005); slower.initialize(4,.02);
    assert(std::abs(faster.rise*faster.rise-.999)<1e-14);
    assert(std::abs(slower.fall-.25)<1e-14);
    assert(std::abs(slower.decay-.7225)<1e-14);
    // Mean normalization is invariant to a fixed common power gain.
    mir2::PNCCState normal, boosted;normal.initialize(4,.01);boosted.initialize(4,.01);
    for(int frame=0;frame<100;++frame){
        double a[4],b[4];
        for(int band=0;band<4;++band){
            a[band]=(1+band)*(.1+std::pow(std::sin(frame*.11+band),2));
            b[band]=a[band]*37;
        }
        const bool ready=normal.process(a);assert(boosted.process(b)==ready);
        if(ready)for(int band=0;band<4;++band)
            assert(std::abs(normal.compressed[band]-boosted.compressed[band])<1e-12);
    }
    // Independent frontend oracle: Fourier transform the closed-form impulse
    // response of four cascaded complex one-pole filters. This avoids evaluating
    // the production rational frequency-response expression a second time.
    for(double sampleRate:{44100.,48000.,96000.}){
        constexpr int transform=2048, impulseLength=32768, bandCount=8;
        constexpr int binCount=transform/2+1;
        std::vector<double> weights(static_cast<std::size_t>(bandCount*binCount));
        std::vector<mir2::Complex> impulse(impulseLength);
        std::array<double,binCount> reference{};
        mir2::pnccWeights(weights.data(),binCount,sampleRate,transform,bandCount,0,8000);
        for(int band=0;band<bandCount;++band){
            const double fraction=static_cast<double>(band)/(bandCount-1);
            const double center=(std::pow(1+.00437*8000,fraction)-1)/.00437;
            const double erb=24.7+center/9.265;
            const double radius=std::exp(-2*mir2::kPi*erb/(sampleRate*(5*mir2::kPi/16)));
            const std::complex<double> pole=std::polar(radius,mir2::kTwoPi*center/sampleRate);
            std::complex<double> term=std::pow(1-radius,4);
            for(int n=0;n<impulseLength;++n){
                impulse[static_cast<std::size_t>(n)]={term.real(),term.imag()};
                term*=pole*(static_cast<double>(n+4)/(n+1));
            }
            mir2::fft(impulse.data(),impulseLength,false);
            double maximum=0,sum=0;
            for(int bin=0;bin<binCount;++bin){
                const auto value=impulse[static_cast<std::size_t>(bin*(impulseLength/transform))];
                reference[static_cast<std::size_t>(bin)]=value.real*value.real+value.imag*value.imag;
                maximum=std::max(maximum,reference[static_cast<std::size_t>(bin)]);
            }
            for(auto& value:reference){if(value<maximum*.000025)value=0;sum+=value;}
            for(int bin=0;bin<binCount;++bin)
                assert(std::abs(weights[static_cast<std::size_t>(band*binCount+bin)]-reference[static_cast<std::size_t>(bin)]/sum)<1e-10);
        }
    }
    std::cout<<"PNCC stage oracle, nonzero startup, hop scaling, gain invariance and impulse-response frontend oracle passed\n";
}
