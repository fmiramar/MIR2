// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/ConstantQ.hpp"
namespace {
using namespace mir2plugin;
struct CQUnit : Unit {
    double* ring;
    mir2::Complex* work;
    mir2::CQEntry* entries;
    int offsets[mir2::kCQMaxBins+1];
    double magnitude[mir2::kCQMaxBins],outputs[mir2::kCQMaxBins],uniform[1024];
    int size,bands,bpo,position,filled,hop,countdown,uniformCount,coefficients;
    double minimum;
    bool cepstral;
};
void CQ_next(CQUnit* unit,int){
    const float* input=IN(0);
    for(int i=0;i<unit->mWorld->mFullRate.mBufLength;++i){
        unit->ring[unit->position]=mir2::sanitize(input[i]);
        unit->position=(unit->position+1)%unit->size;
        unit->filled=std::min(unit->size,unit->filled+1);
        if(--unit->countdown<=0 && unit->filled==unit->size){
            for(int j=0;j<unit->size;++j)unit->work[j]={unit->ring[(unit->position+j)%unit->size],0};
            mir2::fft(unit->work,unit->size,false);
            mir2::constantQ(unit->work,unit->entries,unit->offsets,unit->bands,unit->magnitude);
            if(unit->cepstral)mir2::cqCepstra(unit->magnitude,unit->bands,unit->minimum,unit->bpo,
                unit->uniform,unit->uniformCount,unit->outputs,unit->coefficients,IN0(10)>0,
                std::max(1.e-30,static_cast<double>(mir2::sanitize(IN0(11)))));
            else for(int j=0;j<unit->bands;++j)unit->outputs[j]=unit->magnitude[j];
            unit->countdown=unit->hop;
        }
    }
    for(int i=0;i<static_cast<int>(unit->mNumOutputs);++i)OUT0(i)=static_cast<float>(unit->outputs[i]);
}
void CQ_init(CQUnit* unit,bool cepstral){
    unit->ring=nullptr;unit->work=nullptr;unit->entries=nullptr;
    unit->minimum=mir2::sanitize(IN0(1));
    unit->bands=mir2::clamp(static_cast<int>(mir2::sanitize(IN0(2))),2,mir2::kCQMaxBins);
    unit->bpo=mir2::clamp(static_cast<int>(mir2::sanitize(IN0(3))),6,48);
    const double gamma=std::max(0.0,static_cast<double>(mir2::sanitize(IN0(4))));
    const double sparsity=mir2::clamp(static_cast<double>(mir2::sanitize(IN0(6))),1.e-8,.01);
    unit->size=mir2::cqFrameSize(unit->mWorld->mSampleRate,unit->minimum,unit->bpo,gamma);
    unit->position=unit->filled=0;unit->cepstral=cepstral;
    unit->uniformCount=cepstral?mir2::clamp(static_cast<int>(mir2::sanitize(IN0(9))),16,1024):16;
    unit->coefficients=cepstral?static_cast<int>(unit->mNumOutputs):0;
    std::fill(unit->outputs,unit->outputs+mir2::kCQMaxBins,0.0);
    bool valid=unit->size>0 && INRATE(0)==calc_FullRate
        && unit->minimum*std::exp2(static_cast<double>(unit->bands-1)/unit->bpo)<unit->mWorld->mSampleRate*.5
        && static_cast<int>(unit->mNumOutputs)<=mir2::kCQMaxBins;
    if(valid){
        unit->ring=allocate<double>(unit->mWorld,static_cast<std::size_t>(unit->size));
        unit->work=allocate<mir2::Complex>(unit->mWorld,static_cast<std::size_t>(unit->size));
        valid=unit->ring && unit->work;
    }
    if(valid){
        const int entries=mir2::buildCQKernels(unit->work,unit->size,unit->mWorld->mSampleRate,
            unit->minimum,unit->bpo,unit->bands,gamma,sparsity,unit->offsets,nullptr);
        valid=entries>0;
        if(valid){
            unit->entries=allocate<mir2::CQEntry>(unit->mWorld,static_cast<std::size_t>(entries));
            valid=unit->entries!=nullptr;
            if(valid)valid=mir2::buildCQKernels(unit->work,unit->size,unit->mWorld->mSampleRate,
                unit->minimum,unit->bpo,unit->bands,gamma,sparsity,unit->offsets,unit->entries,entries)==entries;
        }
    }
    if(!valid){
        Print("MIR2 CQT: invalid range, required frame exceeds 32768, kernel budget exceeded, or insufficient RT memory\n");
        SETCALC(ClearUnitOutputs);unit->mDone=true;ClearUnitOutputs(unit,1);return;
    }
    unit->hop=mir2::clamp(static_cast<int>(std::lround(unit->size*mir2::clamp(static_cast<double>(mir2::sanitize(IN0(5))),.0625,1.0))),1,unit->size);
    unit->countdown=unit->size;
    SETCALC(CQ_next);ClearUnitOutputs(unit,1);
}
void CQ_destroy(CQUnit* unit){release(unit->mWorld,unit->ring);release(unit->mWorld,unit->work);release(unit->mWorld,unit->entries);}
struct CQT : CQUnit {};
struct VQT : CQUnit {};
struct CQCC : CQUnit {};
void CQT_Ctor(CQT* unit){CQ_init(unit,false);} void CQT_Dtor(CQT* unit){CQ_destroy(unit);}
void VQT_Ctor(VQT* unit){CQ_init(unit,false);} void VQT_Dtor(VQT* unit){CQ_destroy(unit);}
void CQCC_Ctor(CQCC* unit){CQ_init(unit,true);} void CQCC_Dtor(CQCC* unit){CQ_destroy(unit);}
}
namespace mir2plugin {void registerConstantQUnits(){DefineDtorUnit(CQT);DefineDtorUnit(VQT);DefineDtorUnit(CQCC);}}
