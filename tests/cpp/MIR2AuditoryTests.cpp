// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Auditory.hpp"
#include "mir2/FFT.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>
namespace {
void check(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
void near(double value,double expected,double tolerance,const char* message){if(!std::isfinite(value)||std::abs(value-expected)>tolerance){std::cerr<<message<<" actual "<<value<<" expected "<<expected<<'\n';std::exit(1);}}
}
int main(){
    for(double hz:{0.,20.,1000.,20000.}){
        near(mir2::inverseBarkTraunmuller(mir2::barkTraunmuller(hz)),hz,1e-8,"Bark inverse");
        near(mir2::inverseERBRate(mir2::erbRate(hz)),hz,1e-8,"ERB inverse");
    }
    near(mir2::barkTraunmuller(1000),26.81*1000/2960-.53,1e-12,"Traunmuller mapping");
    near(mir2::erbRate(1000),21.4*std::log10(5.37),1e-12,"Glasberg Moore ERB mapping");
    double edges[26],band[24];std::vector<double> weights(24*1025),magnitude(1025,1.0);
    for(auto scale:{mir2::AuditoryScale::bark,mir2::AuditoryScale::erb,mir2::AuditoryScale::linear}){
        mir2::makeAuditoryEdges(edges,24,20,20000,scale);
        near(edges[0],20,1e-8,"low edge");near(edges[25],20000,1e-8,"high edge");
        mir2::makeAuditoryWeights(weights.data(),1025,48000./2048.,edges,24,true);
        mir2::auditoryBands(magnitude.data(),1025,weights.data(),24,1,band);
        for(double b:band)near(b,1,1e-12,"flat power normalized bands");
        near(mir2::dctII(band,24,0,true),std::sqrt(24.),1e-10,"DCT constant c0");
        for(int c=1;c<24;++c)near(mir2::dctII(band,24,c,true),0,1e-10,"DCT constant nonzero coefficients");
    }
    double dctInput[24];for(int i=0;i<24;++i)dctInput[i]=std::sin(i*.23)+i*.1;
    double inputEnergy=0,cepstralEnergy=0;for(int c=0;c<24;++c){inputEnergy+=dctInput[c]*dctInput[c];double value=mir2::dctII(dctInput,24,c,true);cepstralEnergy+=value*value;}
    near(cepstralEnergy,inputEnergy,1e-10,"orthonormal DCT Parseval");
    mir2::GammatoneFilter gt;gt.initialize(48000,1000);
    const double radius=std::hypot(gt.poleReal,gt.poleImag),gain=gt.gain;
    for(int n=0;n<400;++n){
        const double combinatorial=(n+1.)*(n+2.)*(n+3.)/6.;
        const double expected=2*std::pow(gain,8)*combinatorial*combinatorial*std::pow(radius,2*n);
        near(gt.processPower(n==0?1.:0.),expected,1e-16,"gammatone closed-form impulse power");
    }
    for(double sampleRate:{44100.,48000.,96000.}){
        gt.initialize(sampleRate,1000);double sum=0;int count=0;
        for(int n=0;n<static_cast<int>(sampleRate);++n){const double power=gt.processPower(.5*std::sin(mir2::kTwoPi*1000*n/sampleRate));if(n>sampleRate*.5){sum+=power;++count;}}
        near(sum/count,.125,1e-5,"gammatone center-tone RMS power");
        gt.initialize(sampleRate,1000);sum=0;count=0;
        for(int n=0;n<static_cast<int>(sampleRate);++n){const double power=gt.processPower(.5*std::sin(mir2::kTwoPi*5000*n/sampleRate));if(n>sampleRate*.5){sum+=power;++count;}}
        check(sum/count<1e-8,"gammatone out-of-band rejection");
    }
    near(mir2::phonToSone(0),0,0,"silence sones");near(mir2::phonToSone(40),1,0,"40 phon one sone");near(mir2::phonToSone(60),4,0,"60 phon four sones");
    for(int size:{1024,2048,4096}){
        std::vector<mir2::Complex> frame(static_cast<std::size_t>(size));std::vector<double> mag(static_cast<std::size_t>(size/2+1));
        near(mir2::loudnessExcitation(mag.data(),size,48000,100,.375),0,0,"silent loudness excitation");
        for(int n=0;n<size;++n)frame[static_cast<std::size_t>(n)]={.001*std::sin(mir2::kTwoPi*1000*n/48000)*(.5-.5*std::cos(mir2::kTwoPi*n/size)),0};
        mir2::fft(frame.data(),size,false);for(int bin=0;bin<=size/2;++bin)mag[static_cast<std::size_t>(bin)]=std::hypot(frame[static_cast<std::size_t>(bin)].real,frame[static_cast<std::size_t>(bin)].imag);
        const double phon=10*std::log10(1+mir2::loudnessExcitation(mag.data(),size,48000,100,.375));
        near(phon,40,1.5,"loudness calibrated 1 kHz 40 dB SPL across FFT sizes");
    }
    std::vector<mir2::GammatoneFilter> bank(64);for(int b=0;b<64;++b)bank[static_cast<std::size_t>(b)].initialize(48000,mir2::inverseERBRate(mir2::erbRate(50)+(mir2::erbRate(20000)-mir2::erbRate(50))*b/63));
    volatile double sink=0;const auto start=std::chrono::steady_clock::now();
    for(int n=0;n<48000;++n)for(auto& filter:bank)sink+=filter.processPower(std::sin(n*.1));
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"Auditory tests passed; 64 gammatone bands, 1 s at 48 kHz: "<<seconds<<" CPU seconds; guard "<<sink<<'\n';
}
