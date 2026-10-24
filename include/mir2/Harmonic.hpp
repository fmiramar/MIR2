// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SpectralAdvanced.hpp"
#include <array>
namespace mir2 {
constexpr int kMaxHarmonics=64;
struct HarmonicFrame {
    double frequencies[kMaxHarmonics]{}, amplitudes[kMaxHarmonics]{};
    int bins[kMaxHarmonics]{};
    int count=0,capacity=0;
    double f0=0.0;
};
inline HarmonicFrame associateHarmonics(const SpectralPeak* peaks,int count,double f0,
                                       int capacity,double toleranceCents=50.0) noexcept {
    HarmonicFrame result;result.capacity=clamp(capacity,1,kMaxHarmonics);result.f0=f0;
    std::fill(result.bins,result.bins+kMaxHarmonics,-1);
    if(!finite(f0)||f0<=0.0)return result;
    toleranceCents=clamp(sanitize(toleranceCents),0.0,600.0);
    for(int i=0;i<count;++i) {
        const double ratio=peaks[i].frequency/f0;
        if(!finite(ratio)||ratio<0.5||ratio>result.capacity+0.5)continue;
        const int h=int(std::lround(ratio));
        if(h<1||h>result.capacity||centsDistance(peaks[i].frequency,h*f0)>toleranceCents)continue;
        if(peaks[i].magnitude>result.amplitudes[h-1]) {
            if(result.amplitudes[h-1]==0.0)++result.count;
            result.frequencies[h-1]=peaks[i].frequency;result.amplitudes[h-1]=peaks[i].magnitude;result.bins[h-1]=peaks[i].bin;
        }
    }
    return result;
}
struct HarmonicSummary {
    double inharmonicity=0.0,tristimulus[3]{},oddEven=0.0,centroid=0.0,deviation=0.0,spread=0.0,variation=0.0;
};
inline HarmonicSummary summarizeHarmonics(const HarmonicFrame& frame,const HarmonicFrame* previous=nullptr) noexcept {
    HarmonicSummary result;
    double sum=0,power=0,odd=0,even=0,weightedFrequency=0,inharm=0;
    for(int i=0;i<frame.capacity;++i) {
        const double a=frame.amplitudes[i],p=a*a;sum+=a;power+=p;
        weightedFrequency+=a*frame.frequencies[i];
        inharm+=p*std::abs(frame.frequencies[i]-(i+1)*frame.f0);
        result.tristimulus[i==0?0:(i<4?1:2)]+=a;
        (i%2==0?odd:even)+=p;
    }
    if(sum<=1e-30)return result;
    result.centroid=weightedFrequency/sum;
    result.inharmonicity=frame.f0>0?inharm/(frame.f0*power):0;
    // A finite ceiling expresses the otherwise infinite odd/even power ratio.
    result.oddEven=std::min(1.0e6,odd/std::max(1.0e-30,even));
    for(double& t:result.tristimulus)t/=sum;
    double deviation=0,spread=0,dot=0,previousEnergy=0;
    for(int i=0;i<frame.capacity;++i) {
        const double a=frame.amplitudes[i];
        spread+=a*(frame.frequencies[i]-result.centroid)*(frame.frequencies[i]-result.centroid);
        if(a>0) {
            const double left=i>0?frame.amplitudes[i-1]:a;
            const double right=i+1<frame.capacity?frame.amplitudes[i+1]:a;
            const double envelope=(left+a+right)/3.0;
            deviation+=a*std::abs(20.0*std::log10(a/std::max(1e-20,envelope)));
        }
        if(previous) {dot+=a*previous->amplitudes[i];previousEnergy+=previous->amplitudes[i]*previous->amplitudes[i];}
    }
    result.deviation=deviation/sum;result.spread=std::sqrt(spread/sum);
    if(previous&&previousEnergy>1e-30)result.variation=clamp(1.0-dot/std::sqrt(power*previousEnergy),0.0,1.0);
    return result;
}
inline double harmonicEnergyRatio(const double* magnitude,int bins,const HarmonicFrame& harmonics,int radius=1) noexcept {
    double total=0,selected=0;
    for(int bin=0;bin<bins;++bin) {
        const double power=magnitude[bin]*magnitude[bin];total+=power;
        bool matched=false;
        for(int h=0;h<harmonics.capacity;++h)
            if(harmonics.bins[h]>=0&&std::abs(bin-harmonics.bins[h])<=radius){matched=true;break;}
        if(matched)selected+=power;
    }
    return total>1e-30?clamp(selected/total,0.0,1.0):0.0;
}
inline double sensoryRoughness(const SpectralPeak* peaks,int count,int model=0,bool normalize=true) noexcept {
    double scale=0;for(int i=0;i<count;++i)scale=std::max(scale,peaks[i].magnitude);
    if(scale<=1e-30)return 0;
    if(!normalize)scale=1;
    double result=0;
    for(int i=0;i<count;++i)for(int j=i+1;j<count;++j) {
        const double fmin=std::min(peaks[i].frequency,peaks[j].frequency);
        const double x=0.24*std::abs(peaks[i].frequency-peaks[j].frequency)/(0.0207*fmin+18.96);
        const double a=peaks[i].magnitude/scale,b=peaks[j].magnitude/scale;
        const double amplitude=model==0?std::min(a,b):a*b;
        result+=5.0*amplitude*(std::exp(-3.51*x)-std::exp(-5.75*x));
    }
    return std::max(0.0,result);
}
} // namespace mir2
