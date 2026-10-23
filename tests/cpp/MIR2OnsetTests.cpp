// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Onset.hpp"
#include "mir2/FFT.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#ifdef MIR2_NATIVE_ONSETS
#include "onsetsds.h"
#endif

struct Frame { std::vector<double> magnitude, phase; double time; };
struct Fixture { const char* name; std::vector<double> events; std::vector<Frame> frames; };
Fixture fixture(int type, double sr, int size, double hop) {
    Fixture f;
    const char* names[] = {"percussion", "soft_harmonic", "vibrato", "polyphonic", "silence", "quiet_noise", "release"};
    f.name = names[type];
    if (type < 4) f.events = type == 2 ? std::vector<double>{0.5} : std::vector<double>{0.5,1.2,1.9,2.6,3.3,4.0};
    if (type == 6) f.events = {0.5};
    std::mt19937 random(71); std::uniform_real_distribution<double> noise(-1,1);
    std::vector<double> audio(static_cast<std::size_t>(sr * 5.0), 0.0);
    double phase = 0;
    for (std::size_t i = 0; i < audio.size(); ++i) {
        const double t = static_cast<double>(i) / sr;
        phase += mir2::kTwoPi * 220 * std::pow(2.0, 0.02 * std::sin(mir2::kTwoPi * 5.5 * t)) / sr;
        if (type == 5) audio[i] = noise(random) * 1.e-6;
        for (double onset : f.events) {
            const double x = t - onset;
            if (x < 0) continue;
            if (type == 0) audio[i] += 0.4 * noise(random) * std::exp(-x / .035);
            if (type == 1) {
                const double env = std::min(1.0, x / .05) * std::exp(-x / .17);
                for (int h = 1; h <= 6; ++h) audio[i] += .12 / h * env * std::sin(mir2::kTwoPi * (220 + onset * 20) * h * x);
            }
            if (type == 2 && x < 4) audio[i] += .25 * std::min(1.0,x/.01) * std::sin(phase);
            if (type == 3) {
                for (double freq : {220.,277.18,329.63}) audio[i] += .08 * std::exp(-x/.25) * std::sin(mir2::kTwoPi * freq * x);
            }
            if (type == 6 && x < 2) audio[i] += .25 * std::min(1.,x/.01) * std::sin(mir2::kTwoPi * 220 * x);
        }
    }
    const int stride = static_cast<int>(size * hop);
    std::vector<mir2::Complex> fft(size);
    for (int end = size; end <= static_cast<int>(audio.size()); end += stride) {
        for (int i = 0; i < size; ++i) fft[i] = {audio[end-size+i] * (.5-.5*std::cos(mir2::kTwoPi*i/size)),0};
        mir2::fft(fft.data(),size,false);
        Frame frame; frame.time = end / sr;
        for (int i = 0; i <= size/2; ++i) {
            frame.magnitude.push_back(std::hypot(fft[i].real,fft[i].imag));
            frame.phase.push_back(std::atan2(fft[i].imag,fft[i].real));
        }
        f.frames.push_back(std::move(frame));
    }
    return f;
}
struct Score { int tp=0, fp=0, fn=0; double error=0; };
Score score(const std::vector<double>& detected,const std::vector<double>& events) {
    Score s; std::vector<bool> matched(events.size(),false);
    for (double detection : detected) {
        int best = -1; double distance=.08;
        for (std::size_t i=0;i<events.size();++i)
            if (!matched[i] && std::abs(detection-events[i])<distance) { best=static_cast<int>(i); distance=std::abs(detection-events[i]); }
        if (best<0) ++s.fp; else { ++s.tp; matched[best]=true; s.error+=distance; }
    }
    s.fn=static_cast<int>(events.size())-s.tp; return s;
}
// Original MIR2 v0.1 detector retained only as a behavioral benchmark.
std::vector<double> oldDetector(const Fixture& f,int method,double threshold,double dt) {
    std::vector<double> previous; std::vector<double> detected;
    double mean=0,variance=0; int frames=0,gap=100000;
    for (const auto& frame:f.frames) {
        auto values=frame.magnitude; for(auto& v:values) v=std::log(std::max(1.e-6,v));
        double strength=0;
        if (!previous.empty()) for(int i=0;i<static_cast<int>(values.size());++i) {
            double reference=previous[i];
            if(method==2) for(int j=std::max(0,i-3);j<=std::min(static_cast<int>(values.size())-1,i+3);++j) reference=std::max(reference,previous[j]);
            strength+=method==0? std::abs(values[i]-reference):std::max(0.,values[i]-reference);
        }
        strength/=values.size();
        bool trigger=frames>=1 && gap>=std::ceil(.03/dt) && strength>mean+threshold*std::sqrt(std::max(0.,variance)) && strength>1.e-6;
        if(trigger) {detected.push_back(frame.time);gap=0;}else ++gap;
        double c=std::exp(-dt/std::max(dt,.05)),d=strength-mean;
        mean=c*mean+(1-c)*strength;variance=c*variance+(1-c)*d*d;
        previous=values;++frames;
    }
    return detected;
}
int main() {
    assert(std::abs(mir2::complexDeviation(2,.7,2,.7))<1.e-12);
    assert(std::abs(mir2::complexDeviation(2,0,2,mir2::kPi)-4)<1.e-12);
    double previous[]{0,1,0}, current[]{0,0,1};
    assert(mir2::superFluxEvidence(current,previous,3,3)==0);
    assert(mir2::superFluxEvidence(current,previous,3,1)>0);
    const char* methods[]={"flux","fluxPos","superflux","power","magsum","complex","rcomplex","phase","wphase","mkl","hfc"};
    std::cout<<"implementation,sample_rate,fft_size,hop,fixture,method,threshold,tp,fp,fn,mean_error_ms\n";
    for(double sr:{44100.,48000.,96000.}) for(double hop:{.25,.5}) {
        const int size=2048;
        for(int type=0;type<7;++type) {
            auto f=fixture(type,sr,size,hop);
            auto emit=[&](const char* name,int method,double threshold,const std::vector<double>& detected) {
                auto s=score(detected,f.events);
                std::cout<<name<<','<<sr<<','<<size<<','<<hop<<','<<f.name<<','<<methods[method]<<','<<threshold<<','<<s.tp<<','<<s.fp<<','<<s.fn<<','<<(s.tp?s.error*1000/s.tp:0)<<'\n';
            };
            for(double threshold:{.15,.3,.5,.7}) {
                for(int method=0;method<11;++method) {
                    mir2::OnsetConfig config;config.fftSize=size;config.sampleRate=sr;config.hop=hop;config.method=static_cast<mir2::OnsetMethod>(method);
                    std::vector<double> memory(mir2::OnsetState::storageCount(config));mir2::OnsetState state;state.initialize(config,memory.data());
                    mir2::OnsetControls controls;controls.threshold=threshold;
                    std::vector<double> detected;
                    for(const auto& frame:f.frames) {
                        auto result=state.process(frame.magnitude.data(),frame.phase.data(),controls);
                        assert(std::isfinite(result.strength)&&result.strength>=0&&result.strength<=1);
                        assert(std::isfinite(result.raw));
                        if(result.trigger)detected.push_back(frame.time);
                    }
                    if(type==4||type==5)assert(detected.empty());
                    if(type==0 && method==2) {auto s=score(detected,f.events);assert(s.tp==6 && s.fp==0);}
                    emit("revised",method,threshold,detected);
                    if(method<3)emit("v0.1",method,threshold,oldDetector(f,method,threshold,size*hop/sr));
#ifdef MIR2_NATIVE_ONSETS
                    if(method>=3&&method<=9) {
                        OnsetsDS ods{}; int span=static_cast<int>(std::ceil(.15*sr/(size*hop)));
                        std::vector<float> data(onsetsds_memneeded(method-3,size,span)/sizeof(float)+1);
                        onsetsds_init(&ods,data.data(),ODS_FFT_SC3_POLAR,method-3,size,span,static_cast<float>(sr));
                        onsetsds_setrelax(&ods,1.f,static_cast<size_t>(size*hop));
                        ods.thresh=static_cast<float>(threshold);ods.floor=.1f;ods.mingap=static_cast<unsigned>(std::ceil(.03*sr/(size*hop)));
                        std::vector<float> packed(size);detected.clear();
                        for(const auto& frame:f.frames) {
                            packed[0]=static_cast<float>(frame.magnitude[0]);packed[1]=static_cast<float>(frame.magnitude.back());
                            for(int b=1;b<size/2;++b){packed[2*b]=static_cast<float>(frame.magnitude[b]);packed[2*b+1]=static_cast<float>(frame.phase[b]);}
                            if(onsetsds_process(&ods,packed.data()))detected.push_back(frame.time);
                        }
                        emit("native_3.14.1",method,threshold,detected);
                    }
#endif
                }
            }
        }
    }
}
