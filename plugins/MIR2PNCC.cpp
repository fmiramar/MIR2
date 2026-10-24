// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/PNCC.hpp"
namespace {
using namespace mir2plugin;
struct PNCC : Unit {
    mir2::PNCCState state;
    double* ring;double* weights;mir2::Complex* work;
    double magnitude[8193],power[mir2::kPNCCMaxBands],coefficients[64];
    int size,fftSize,bins,position,filled,hop,countdown;
    double previousInput;
};
void PNCC_next(PNCC* unit,int){
    const float* input=IN(0);
    const double preEmphasis=mir2::clamp(static_cast<double>(mir2::sanitize(IN0(6))),0.0,1.0);
    for(int sample=0;sample<unit->mWorld->mFullRate.mBufLength;++sample){
        const double value=mir2::sanitize(input[sample]);
        unit->ring[unit->position]=value-preEmphasis*unit->previousInput;unit->previousInput=value;
        unit->position=(unit->position+1)%unit->size;unit->filled=std::min(unit->size,unit->filled+1);
        if(--unit->countdown<=0 && unit->filled==unit->size){
            for(int i=0;i<unit->size;++i){
                const double window=.54-.46*std::cos(mir2::kTwoPi*i/(unit->size-1));
                unit->work[i]={unit->ring[(unit->position+i)%unit->size]*window,0};
            }
            std::fill(unit->work+unit->size,unit->work+unit->fftSize,mir2::Complex{0,0});
            mir2::fft(unit->work,unit->fftSize,false);
            for(int i=0;i<unit->bins;++i)unit->magnitude[i]=std::hypot(unit->work[i].real,unit->work[i].imag)/unit->size;
            mir2::auditoryBands(unit->magnitude,unit->bins,unit->weights,unit->state.bands,1,unit->power);
            if(unit->state.process(unit->power,mir2::boundedInteger(IN0(8))))
                unit->state.cepstra(unit->coefficients,static_cast<int>(unit->mNumOutputs),IN0(7)>0);
            unit->countdown=unit->hop;
        }
    }
    for(int i=0;i<static_cast<int>(unit->mNumOutputs);++i)OUT0(i)=static_cast<float>(unit->coefficients[i]);
}
void PNCC_Ctor(PNCC* unit){
    unit->ring=nullptr;unit->weights=nullptr;unit->work=nullptr;
    unit->position=unit->filled=0;unit->previousInput=0;
    std::fill(unit->coefficients,unit->coefficients+64,0.0);
    const double sr=unit->mWorld->mSampleRate;
    const double frameTime=mir2::clamp(static_cast<double>(mir2::sanitize(IN0(4))),.005,.1);
    unit->size=mir2::boundedInteger(std::ceil(sr*frameTime));unit->fftSize=32;
    while(unit->fftSize<2*unit->size)unit->fftSize*=2;
    unit->bins=unit->fftSize/2+1;
    unit->hop=std::max(1,mir2::boundedInteger(std::round(sr*mir2::clamp(static_cast<double>(mir2::sanitize(IN0(5))),.001,.1))));
    unit->countdown=unit->size;
    const int bands=mir2::clamp(mir2::boundedInteger(IN0(1)),4,mir2::kPNCCMaxBands);
    const double low=std::max(0.0,static_cast<double>(mir2::sanitize(IN0(2))));
    const double high=std::min(IN0(3)>0?static_cast<double>(IN0(3)):8000.0,sr*.45);
    unit->state.initialize(bands,unit->hop/sr);
    bool valid=INRATE(0)==calc_FullRate && unit->size<=8192 && high>low
        && unit->mNumOutputs<=64 && static_cast<int>(unit->mNumOutputs)+(IN0(7)>0?0:1)<=bands;
    if(valid){
        unit->ring=allocate<double>(unit->mWorld,static_cast<std::size_t>(unit->size));
        unit->weights=allocate<double>(unit->mWorld,static_cast<std::size_t>(bands*unit->bins));
        unit->work=allocate<mir2::Complex>(unit->mWorld,static_cast<std::size_t>(unit->fftSize));
        valid=unit->ring && unit->weights && unit->work;
    }
    if(!valid){Print("PNCC: invalid range, frame exceeds8192 samples, or insufficient RT memory\n");SETCALC(ClearUnitOutputs);unit->mDone=true;ClearUnitOutputs(unit,1);return;}
    mir2::pnccWeights(unit->weights,unit->bins,sr,unit->fftSize,bands,low,high);
    SETCALC(PNCC_next);ClearUnitOutputs(unit,1);
}
void PNCC_Dtor(PNCC* unit){release(unit->mWorld,unit->ring);release(unit->mWorld,unit->weights);release(unit->mWorld,unit->work);}
}
namespace mir2plugin{void registerPNCCUnits(){DefineDtorUnit(PNCC);}}
