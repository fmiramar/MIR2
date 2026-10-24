// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Auditory.hpp"
namespace mir2 {
constexpr int kPNCCMaxBands = 128;
// Squared frequency response of the Hohmann analytic fourth-order gammatone.
// Each sampled power response is area normalized; negligible tails are omitted.
inline void pnccWeights(double* weights,int bins,double sampleRate,int fftSize,
                        int bands,double low,double high) noexcept {
    const double a=erbRate(low),b=erbRate(high);
    for(int band=0;band<bands;++band){
        const double center=inverseERBRate(a+(b-a)*band/(bands-1));
        const double bandwidth=(24.7+center/9.265)/(kPi*720.0/(64.0*36.0));
        const double radius=std::exp(-kTwoPi*bandwidth/sampleRate);
        double peak=0,total=0;
        for(int bin=0;bin<bins;++bin){
            const double angle=kTwoPi*(static_cast<double>(bin)/fftSize-center/sampleRate);
            const double oneMinusRadius=1-radius;
            const double sine=std::sin(angle*.5);
            // Equivalent to 1+r*r-2*r*cos(angle), without near-pole cancellation.
            const double denominator=oneMinusRadius*oneMinusRadius+4*radius*sine*sine;
            const double value=std::pow(oneMinusRadius*oneMinusRadius/denominator,4);
            weights[band*bins+bin]=value;peak=std::max(peak,value);
        }
        for(int bin=0;bin<bins;++bin){
            double& value=weights[band*bins+bin];
            if(value<peak*0.000025)value=0; // amplitude ratio 0.005, squared
            total+=value;
        }
        if(total>0)for(int bin=0;bin<bins;++bin)weights[band*bins+bin]/=total;
    }
}
inline double asymmetricPNCC(double input,double previous,double rise,double fall) noexcept {
    const double coefficient=input>=previous?rise:fall;
    return coefficient*previous+(1-coefficient)*input;
}
struct PNCCState {
    double history[5*kPNCCMaxBands],low[kPNCCMaxBands],floor[kPNCCMaxBands];
    double peak[kPNCCMaxBands],ratio[kPNCCMaxBands],compressed[kPNCCMaxBands];
    double medium[kPNCCMaxBands],weighted[kPNCCMaxBands];
    int bands,index,frames;bool initialized,meanInitialized;
    double mean,rise,fall,decay,meanDecay;
    void initialize(int count,double hopSeconds) noexcept {
        bands=clamp(count,4,kPNCCMaxBands);index=frames=0;initialized=meanInitialized=false;mean=0;
        std::fill(history,history+5*kPNCCMaxBands,0.0);
        std::fill(low,low+kPNCCMaxBands,0.0);std::fill(floor,floor+kPNCCMaxBands,0.0);
        std::fill(peak,peak+kPNCCMaxBands,0.0);std::fill(compressed,compressed+kPNCCMaxBands,0.0);
        const double ratioTime=hopSeconds/.01;
        rise=meanDecay=std::pow(.999,ratioTime);fall=std::pow(.5,ratioTime);decay=std::pow(.85,ratioTime);
    }
    bool process(const double* power,int smoothing=4) noexcept {
        for(int b=0;b<bands;++b)history[index*bands+b]=std::max(0.0,sanitize(power[b]));
        index=(index+1)%5;frames=std::min(5,frames+1);
        if(frames<3)return false;
        const int center=(index+2)%5;
        for(int b=0;b<bands;++b){
            medium[b]=0;for(int t=0;t<5;++t)medium[b]+=history[t*bands+b]/5;
            if(!initialized)low[b]=.9*medium[b];
            else low[b]=asymmetricPNCC(medium[b],low[b],rise,fall);
            const double rectified=std::max(0.0,medium[b]-low[b]);
            if(!initialized)floor[b]=rectified;
            else floor[b]=asymmetricPNCC(rectified,floor[b],rise,fall);
            const double masked=rectified>=decay*peak[b]?rectified:.2*peak[b];
            const double selected=medium[b]>=2*low[b]?std::max(masked,floor[b]):floor[b];
            peak[b]=std::max(decay*peak[b],rectified);
            ratio[b]=medium[b]>1.e-30?selected/medium[b]:0;
        }
        initialized=true;
        const int radius=clamp(smoothing,0,bands-1);double average=0;
        for(int b=0;b<bands;++b){
            const int lo=std::max(0,b-radius),hi=std::min(bands-1,b+radius);double sum=0;
            for(int j=lo;j<=hi;++j)sum+=ratio[j];
            weighted[b]=history[center*bands+b]*sum/(hi-lo+1);average+=weighted[b]/bands;
        }
        if(!meanInitialized&&average>1.e-30){mean=average;meanInitialized=true;}
        else mean=meanDecay*mean+(1-meanDecay)*average;
        for(int b=0;b<bands;++b)compressed[b]=weighted[b]>0?std::pow(weighted[b]/std::max(1.e-30,mean),1.0/15.0):0;
        return true;
    }
    void cepstra(double* output,int count,bool include0) const noexcept {
        for(int c=0;c<count;++c){
            const int order=c+(include0?0:1);double sum=0;
            for(int b=0;b<bands;++b)sum+=compressed[b]*std::cos(kPi*order*(b+.5)/bands);
            output[c]=sum*std::sqrt((order==0?1.0:2.0)/bands);
        }
    }
};
} // namespace mir2
