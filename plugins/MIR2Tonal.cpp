// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/Tonal.hpp"
namespace {
using namespace mir2plugin;
inline int indexInput(double value,int low,int high) { return mir2::finite(value)?mir2::clamp(int(mir2::clamp(value,double(low),double(high))),low,high):low; }
enum class TonalKind {hpcp,chroma,tuning,key,clarity,mode};
struct TonalUnit:Unit {double* magnitude;mir2::SpectralPeak peaks[128];double history[120];int fftSize,size;bool ready;double elapsed;};
void tonalDestroy(TonalUnit* unit){release(unit->mWorld,unit->magnitude);}
bool tonalLoad(TonalUnit* unit){
    if(IN0(0)<0)return false;
    SndBuf* buffer=resolveBuffer(unit,0);if(!buffer)return false;
    LOCK_SNDBUF(buffer);return fillMagnitudes(buffer,unit->fftSize,unit->magnitude);
}
template<TonalKind kind>void tonalNext(TonalUnit* unit,int){
    unit->elapsed+=unit->mWorld->mFullRate.mBufLength/unit->mWorld->mSampleRate;
    if(!tonalLoad(unit))return;
    const double binHz=unit->mWorld->mSampleRate/unit->fftSize;
    double current[120]{};
    const int count=mir2::extractSpectralPeaks(unit->magnitude,unit->fftSize/2+1,binHz,
        mir2::sanitize(IN0(3)),mir2::sanitize(IN0(4)),mir2::sanitize(IN0(9)),unit->peaks,128);
    if(kind==TonalKind::tuning){
        const auto r=mir2::tuningFromPeaks(unit->peaks,count);
        ZOUT0(0)=float(r.frequency);ZOUT0(1)=float(r.cents);ZOUT0(2)=float(r.confidence);unit->elapsed=0;return;
    }
    if(kind==TonalKind::chroma&&IN0(5)<=0)
        mir2::fftChromagram(unit->magnitude,unit->fftSize/2+1,binHz,current,unit->size,
            mir2::sanitize(IN0(2)),mir2::sanitize(IN0(3)),mir2::sanitize(IN0(4)),indexInput(IN0(6),0,3));
    else mir2::harmonicPitchClassProfile(unit->peaks,count,current,unit->size,mir2::sanitize(IN0(2)),
        indexInput(IN0(5),1,16),indexInput(IN0(7),0,2),indexInput(IN0(6),0,3),mir2::sanitize(IN0(8)));
    if(kind==TonalKind::hpcp||kind==TonalKind::chroma){for(int i=0;i<unit->size;++i)ZOUT0(i)=float(current[i]);unit->elapsed=0;return;}
    const double decay=std::max(0.0,mir2::sanitize(IN0(11)));
    const double alpha=unit->ready&&decay>0?std::exp(-unit->elapsed/decay):0.0;
    for(int i=0;i<12;++i)unit->history[i]=alpha*unit->history[i]+(1-alpha)*current[i];
    unit->ready=true;unit->elapsed=0;
    // A current silent frame has no key, while the context decays for later frames.
    const auto r=count?mir2::estimateKey(unit->history,indexInput(IN0(12),0,2),mir2::clamp(mir2::sanitize(IN0(13)),0.0,1.0)):mir2::KeyResult{};
    if(kind==TonalKind::clarity)ZOUT0(0)=float(r.strength);
    else if(kind==TonalKind::mode)ZOUT0(0)=float(r.mode);
    else{ZOUT0(0)=float(r.root);ZOUT0(1)=float(r.mode);ZOUT0(2)=float(r.strength);}
}
template<TonalKind kind>void tonalInit(TonalUnit* unit){
    unit->magnitude=nullptr;unit->ready=false;unit->elapsed=0;std::fill(unit->history,unit->history+120,0.0);
    unit->fftSize=indexInput(IN0(10),0,8192);unit->size=indexInput(IN0(1),0,120);
    const int expected=kind==TonalKind::hpcp||kind==TonalKind::chroma?unit->size:(kind==TonalKind::key||kind==TonalKind::tuning?3:1);
    if(mir2::isPowerOfTwo(unit->fftSize)&&unit->fftSize>=256&&unit->size>=12&&unit->size%12==0
        &&int(unit->mNumOutputs)==expected)unit->magnitude=allocate<double>(unit->mWorld,static_cast<std::size_t>(unit->fftSize/2+1));
    if(!unit->magnitude){Print("MIR2 tonal: invalid size or insufficient RT memory\n");SETCALC(ClearUnitOutputs);unit->mDone=true;}
    else SETCALC(tonalNext<kind>);
    ClearUnitOutputs(unit,1);
}
#define TONAL_UNIT(Name,Kind) struct Name:TonalUnit{};void Name##_Ctor(Name* u){tonalInit<TonalKind::Kind>(u);}void Name##_Dtor(Name* u){tonalDestroy(u);}
TONAL_UNIT(HPCP,hpcp) TONAL_UNIT(Chromagram2,chroma) TONAL_UNIT(TuningFrequency,tuning)
TONAL_UNIT(KeyTrack2,key) TONAL_UNIT(KeyClarity2,clarity) TONAL_UNIT(KeyMode2,mode)
#undef TONAL_UNIT

enum class VectorKind {tonnetz,entropy,crest,cens,chord,strength,change};
// Inputs: count,trig,smoothTime,transitionPenalty,threshold,windowSize,downsample,vector...
struct ChromaUnit:Unit{int count,windowSize,writeIndex,filled,downsample,counter;double* ring;double smoothed[12],prior[6],older[6];double elapsed;bool ready;mir2::ChordTracker chord;};
void chromaDestroy(ChromaUnit* unit){release(unit->mWorld,unit->ring);}
template<VectorKind kind>void chromaNext(ChromaUnit* unit,int){
    if(kind==VectorKind::chord)ZOUT0(3)=0;
    if(kind==VectorKind::change)ZOUT0(0)=0;
    unit->elapsed+=unit->mWorld->mFullRate.mBufLength/unit->mWorld->mSampleRate;
    if(!(IN0(1)>0))return;
    double input[120],folded[12];
    for(int i=0;i<unit->count;++i)input[i]=std::max(0.0,mir2::sanitize(IN0(7+i)));
    if(kind==VectorKind::entropy){ZOUT0(0)=float(mir2::chromaEntropy(input,unit->count));unit->elapsed=0;return;}
    if(kind==VectorKind::crest){ZOUT0(0)=float(mir2::chromaCrest(input,unit->count));unit->elapsed=0;return;}
    mir2::foldChroma(input,unit->count,folded);
    if(kind==VectorKind::cens){
        double quantized[12];mir2::quantizeCENS(folded,quantized);
        std::copy(quantized,quantized+12,unit->ring+12*unit->writeIndex);
        unit->writeIndex=(unit->writeIndex+1)%unit->windowSize;unit->filled=std::min(unit->windowSize,unit->filled+1);
        unit->elapsed=0;if(++unit->counter<unit->downsample)return;unit->counter=0;
        double sum[12]{},weight=0;
        for(int i=0;i<unit->filled;++i){
            const double w=unit->filled==1?1.0:0.5-0.5*std::cos(mir2::kTwoPi*(i+1)/(unit->filled+1));
            const int row=(unit->writeIndex-unit->filled+i+unit->windowSize)%unit->windowSize;
            for(int p=0;p<12;++p)sum[p]+=w*unit->ring[row*12+p];
            weight+=w;
        }
        if(weight>0)for(double& x:sum)x/=weight;
        mir2::normalizeChroma(sum,12,3);for(int i=0;i<12;++i)ZOUT0(i)=float(sum[i]);return;
    }
    const double smooth=std::max(0.0,mir2::sanitize(IN0(2)));
    const double alpha=unit->ready&&smooth>0?std::exp(-unit->elapsed/smooth):0;
    for(int i=0;i<12;++i)unit->smoothed[i]=alpha*unit->smoothed[i]+(1-alpha)*folded[i];
    unit->elapsed=0;
    if(kind==VectorKind::tonnetz||kind==VectorKind::change){
        double centroid[6];mir2::tonalCentroid(unit->smoothed,centroid);
        if(kind==VectorKind::tonnetz)for(int i=0;i<6;++i)ZOUT0(i)=float(centroid[i]);
        else{
            double distance=0;
            if(unit->filled>=2)for(int i=0;i<6;++i)distance+=(centroid[i]-unit->older[i])*(centroid[i]-unit->older[i]);
            ZOUT0(0)=float(std::sqrt(distance));std::copy(unit->prior,unit->prior+6,unit->older);
            std::copy(centroid,centroid+6,unit->prior);unit->filled=std::min(2,unit->filled+1);
        }
    }else{
        double energy=0;for(double x:folded)energy+=x;
        double silence[12]{};
        const auto chord=unit->chord.process(energy>1e-20?unit->smoothed:silence,
            mir2::clamp(mir2::sanitize(IN0(4)),0.0,1.0),std::max(0.0,mir2::sanitize(IN0(3))), unit->windowSize > 1);
        if(kind==VectorKind::strength)ZOUT0(0)=float(chord.strength);
        else{ZOUT0(0)=float(chord.root);ZOUT0(1)=float(chord.quality);ZOUT0(2)=float(chord.strength);ZOUT0(3)=chord.changed?1:0;}
    }
    unit->ready=true;
}
template<VectorKind kind>void chromaInit(ChromaUnit* unit){
    unit->ring=nullptr;unit->count=indexInput(IN0(0),0,120);unit->windowSize=indexInput(IN0(5),1,301);
    unit->downsample=indexInput(IN0(6),1,64);unit->counter=unit->writeIndex=unit->filled=0;
    unit->elapsed=0;unit->ready=false;unit->chord={};std::fill(unit->smoothed,unit->smoothed+12,0);
    std::fill(unit->prior,unit->prior+6,0);std::fill(unit->older,unit->older+6,0);
    const int outputs=kind==VectorKind::tonnetz?6:kind==VectorKind::cens?12:kind==VectorKind::chord?4:1;
    bool valid=unit->count>=12&&unit->count%12==0&&int(unit->mNumInputs)==7+unit->count&&int(unit->mNumOutputs)==outputs;
    if(valid&&kind==VectorKind::cens){unit->ring=allocate<double>(unit->mWorld,static_cast<std::size_t>(12*unit->windowSize));valid=unit->ring!=nullptr;}
    if(!valid){Print("MIR2 tonal vector: invalid dimensions or insufficient RT memory\n");SETCALC(ClearUnitOutputs);unit->mDone=true;}
    else SETCALC(chromaNext<kind>);
    ClearUnitOutputs(unit,1);
}
#define VECTOR_UNIT(Name,Kind)struct Name:ChromaUnit{};void Name##_Ctor(Name* u){chromaInit<VectorKind::Kind>(u);}void Name##_Dtor(Name* u){chromaDestroy(u);}
VECTOR_UNIT(Tonnetz,tonnetz) VECTOR_UNIT(HPCPEntropy,entropy) VECTOR_UNIT(HPCPCrest,crest)
VECTOR_UNIT(ChromaCENS,cens) VECTOR_UNIT(ChordTrack,chord) VECTOR_UNIT(ChordStrength,strength) VECTOR_UNIT(HarmonicChange,change)
#undef VECTOR_UNIT
struct ChordStats:Unit{double duration[mir2::kChordStates],elapsed,total,changes;int previous,states,noChord;bool ready;};
void ChordStats_next(ChordStats* unit,int){
    if(IN0(3)>0){std::fill(unit->duration,unit->duration+mir2::kChordStates,0);unit->total=unit->changes=unit->elapsed=0;unit->ready=false;}
    unit->elapsed+=unit->mWorld->mFullRate.mBufLength/unit->mWorld->mSampleRate;
    if(!(IN0(2)>0))return;
    const int root=indexInput(IN0(0),-1,12),quality=indexInput(IN0(1),-1,mir2::kChordQualities);
    const int state=root<0||root>11||quality<0||quality>=unit->noChord/12?unit->noChord:quality*12+root;
    unit->duration[state]+=unit->elapsed;unit->total+=unit->elapsed;unit->elapsed=0;
    if(unit->ready&&state!=unit->previous)unit->changes+=1;
    unit->ready=true;unit->previous=state;
    const double total=std::max(1e-30,unit->total);int dominant=0;
    for(int i=1;i<unit->states;++i)if(unit->duration[i]>unit->duration[dominant])dominant=i;
    ZOUT0(0)=float(mir2::chromaEntropy(unit->duration,unit->states));ZOUT0(1)=float(unit->changes/total);
    ZOUT0(2)=float(unit->duration[unit->noChord]/total);ZOUT0(3)=dominant==unit->noChord?-1:float(dominant%12);
    ZOUT0(4)=dominant==unit->noChord?-1:float(dominant/12);
}
void ChordStats_Ctor(ChordStats* unit){
    std::fill(unit->duration,unit->duration+mir2::kChordStates,0);
    unit->noChord=unit->mNumInputs>4&&IN0(4)>0 ? 12*mir2::kChordQualities : 24;
    unit->states=unit->noChord+1;unit->elapsed=unit->total=unit->changes=0;
    unit->previous=unit->noChord;unit->ready=false;SETCALC(ChordStats_next);ClearUnitOutputs(unit,1);
}
}
namespace mir2plugin{void registerTonalUnits(){
    DefineDtorUnit(HPCP);DefineDtorUnit(Chromagram2);DefineDtorUnit(TuningFrequency);
    DefineDtorUnit(KeyTrack2);DefineDtorUnit(KeyClarity2);DefineDtorUnit(KeyMode2);
    DefineDtorUnit(Tonnetz);DefineDtorUnit(ChromaCENS);DefineDtorUnit(HPCPEntropy);DefineDtorUnit(HPCPCrest);
    DefineDtorUnit(ChordTrack);DefineDtorUnit(ChordStrength);DefineDtorUnit(HarmonicChange);DefineSimpleUnit(ChordStats);
}}
