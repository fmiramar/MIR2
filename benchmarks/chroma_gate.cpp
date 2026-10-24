// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone research comparison, deliberately not a recognition-accuracy test.
#include "mir2/ConstantQ.hpp"
#include "mir2/Tonal.hpp"
#include <array>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <vector>
using namespace mir2;
using Chroma = std::array<double, 12>;
struct CQ {
    int size, bands, bpo;
    double minimum, sr, tuning;
    std::vector<Complex> work;
    std::vector<CQEntry> entries;
    std::array<int,193> offsets{};
    std::array<double,192> magnitudes{};
    CQ(double rate, double low, int density, double reference)
        : size(cqFrameSize(rate, low * reference / 440, density, 0)),
          bands(density * 5), bpo(density), minimum(low * reference / 440),
          sr(rate), tuning(reference), work(static_cast<std::size_t>(size)) {
        assert(size > 0);
        const int count = buildCQKernels(work.data(),size,sr,minimum,bpo,bands,0,1e-4,offsets.data(),nullptr);
        assert(count > 0);
        entries.resize(static_cast<std::size_t>(count));
        assert(buildCQKernels(work.data(),size,sr,minimum,bpo,bands,0,1e-4,
            offsets.data(),entries.data(),count) == count);
    }
    Chroma analyze(const std::vector<double>& signal) {
        for (int i=0;i<size;++i) work[i]={signal[i],0};
        fft(work.data(),size,false);
        constantQ(work.data(),entries.data(),offsets.data(),bands,magnitudes.data());
        Chroma output{};
        cqChroma(magnitudes.data(),bands,minimum,bpo,tuning,2,0,output.data());
        return output;
    }
};
std::array<Chroma,2> baseline(const std::vector<double>& signal,double sr,int size,
                             double low,double high,double tuning) {
    assert(size <= 8192); // The actual public MIR2 FFT-size limit.
    std::vector<Complex> work(static_cast<std::size_t>(size));
    std::vector<double> magnitude(static_cast<std::size_t>(size/2+1));
    const int offset=static_cast<int>(signal.size())-size;
    for(int i=0;i<size;++i) work[i]={signal[offset+i]*(.5-.5*std::cos(kTwoPi*i/size)),0};
    fft(work.data(),size,false);
    for(int i=0;i<=size/2;++i) magnitude[i]=std::hypot(work[i].real,work[i].imag);
    SpectralPeak peaks[128];
    const int count=extractSpectralPeaks(magnitude.data(),size/2+1,sr/size,
        low,high,.01,peaks,128);
    std::array<Chroma,2> output{};
    harmonicPitchClassProfile(peaks,count,output[0].data(),12,tuning,1,2,2,4./3);
    fftChromagram(magnitude.data(),size/2+1,sr/size,output[1].data(),12,tuning,low,high,2);
    return output;
}
std::array<double,2> score(const Chroma& x,const std::vector<int>& notes) {
    double mass=0,norm=0;
    for(int note:notes) mass+=x[note%12];
    for(double v:x) norm+=v*v;
    return {mass, norm>0 ? mass/std::sqrt(norm*notes.size()) : 0};
}
int main() {
    std::cout<<std::fixed<<std::setprecision(6)
        <<"sample_rate,min_freq,bins_per_octave,cqt_samples,fft_samples,scene,tuning,algorithm,target_mass,cosine_similarity,centre_age_ms\n";
    for(double sr:{44100.,48000.,96000.}) for(int density:{12,24,36}) {
        const double low=density==12?55:110;
        for(double tuning:{440.,440*std::exp2(25./1200)}) {
            if(!cqFrameSize(sr,low*tuning/440,density,0)) continue;
            CQ cq(sr,low,density,tuning);
            for(int scene=0;scene<5;++scene) {
                std::array<std::array<double,2>,5> totals{};
                const int base=scene==4 ? 72 : (density==12 ? 36 : 48);
                for(int transpose=0;transpose<12;++transpose) {
                    std::vector<int> notes{base+transpose};
                    if(scene==1) notes.push_back(base+transpose+1);
                    if(scene==2 || scene==3) {notes.push_back(base+transpose+4);notes.push_back(base+transpose+7);}
                    std::vector<double> signal(static_cast<std::size_t>(cq.size));
                    for(int note:notes) {
                        const int harmonics=scene==3?4:1;
                        for(int h=1;h<=harmonics;++h) {
                            double phase=.37*note;
                            for(int i=0;i<cq.size;++i) {
                                // Phase-continuous semitone change 20 ms before common CQT centre.
                                const double old=scene==4 && i<cq.size/2-static_cast<int>(.02*sr) ? 1 : 0;
                                const double hz=tuning*std::exp2((note-69+old)/12.)*h;
                                phase+=kTwoPi*hz/sr;
                                signal[i]+=.2/notes.size()/h*std::sin(phase);
                            }
                        }
                    }
                    const double high=cq.minimum*std::exp2(static_cast<double>(cq.bands-1)/density);
                    auto longFFT=baseline(signal,sr,8192,cq.minimum,high,tuning);
                    auto shortFFT=baseline(signal,sr,4096,cq.minimum,high,tuning);
                    const std::array<Chroma,5> outputs{cq.analyze(signal),longFFT[0],longFFT[1],shortFFT[0],shortFFT[1]};
                    for(int a=0;a<5;++a) {
                        const auto value=score(outputs[a],notes);
                        for(int m=0;m<2;++m) { assert(std::isfinite(value[m])); totals[a][m]+=value[m]/12; }
                    }
                }
                const char* names[]{"folded_cqt","hpcp8192","chromagram8192","hpcp4096","chromagram4096"};
                const char* scenes[]{"single","semitone_pair","major_triad","harmonic_triad","semitone_change"};
                for(int a=0;a<5;++a) {
                    const int frame=a==0?cq.size:(a<3?8192:4096);
                    std::cout<<sr<<','<<low<<','<<density<<','<<cq.size<<','<<frame<<','<<scenes[scene]<<','
                        <<tuning<<','<<names[a]<<','<<totals[a][0]<<','<<totals[a][1]<<','<<500.0*frame/sr<<'\n';
                }
            }
        }
    }
}
