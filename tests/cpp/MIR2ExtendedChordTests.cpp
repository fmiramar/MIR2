// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Tonal.hpp"
#include <cmath>
#include <iostream>
#include <chrono>
namespace {
int failures=0;
void require(bool ok,const char* message){if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
// Independent musical pitch-class masks; not derived through the implementation.
constexpr int masks[9] {0x091,0x089,0x049,0x111,0x491,0x891,0x489,0x085,0x0a1};
int rotatedMask(int root,int quality){const int mask=masks[quality];return ((mask<<root)|(mask>>(12-root)))&0xfff;}
void vectorFor(int root,int quality,double* chroma){const int mask=rotatedMask(root,quality);for(int i=0;i<12;++i)chroma[i]=(mask&(1<<i))?1.0:0.0;}
void exactAndAmbiguous(){
    for(int quality=0;quality<9;++quality)for(int root=0;root<12;++root){
        double chroma[12];vectorFor(root,quality,chroma);mir2::ChordTracker tracker;
        const auto r=tracker.process(chroma,0.65,0.2,true);
        require(r.root>=0&&r.quality>=0,"complete chord must be recognized");
        require(rotatedMask(r.root,r.quality)==rotatedMask(root,quality),"selected label must describe identical pitch-class set");
        require(std::abs(r.strength-1.0)<1e-10,"exact chord cosine similarity");
        // Chroma cannot distinguish augmented inversions or sus2/sus4 aliases.
        if(quality!=3&&quality!=7&&quality!=8)require(r.root==root&&r.quality==quality,"unique complete chord label");
    }
    double chroma[12];vectorFor(8,3,chroma);mir2::ChordTracker tracker;
    auto r=tracker.process(chroma,0.65,0.2,true);require(r.root==0&&r.quality==3,"augmented equivalence deterministic new-path tie");
    vectorFor(7,8,chroma);tracker.reset();r=tracker.process(chroma,0.65,0.2,true);
    require(r.root==0&&r.quality==7,"Gsus4 equals Csus2 in chroma");
}
void incompleteAndSilence(){
    double chroma[12];vectorFor(0,5,chroma);chroma[11]=0;mir2::ChordTracker tracker;
    auto r=tracker.process(chroma,0.65,0.0,true);require(r.root==0&&r.quality==0,"missing seventh falls back to complete major triad");
    vectorFor(0,4,chroma);chroma[4]=0;tracker.reset();r=tracker.process(chroma,0.65,0.0,true);
    require(r.root==-1,"root fifth seventh without defining third is no-chord");
    for(int i=0;i<12;++i)chroma[i]=1;tracker.reset();r=tracker.process(chroma,0.65,0.0,true);
    require(r.root==-1,"uniform chroma is no-chord with expanded vocabulary");
    for(int i=0;i<12;++i)chroma[i]=0;tracker.reset();r=tracker.process(chroma,0.0,0.0,true);
    require(r.root==-1&&r.quality==-1&&r.strength==0,"silence even with zero threshold");
    chroma[0]=1;tracker.reset();r=tracker.process(chroma,0.65,0.0,true);require(r.root==-1,"single pitch no-chord");
}
void compatibilityAndTransitions(){
    double chroma[12];
    for(int quality=0;quality<2;++quality)for(int root=0;root<12;++root){
        vectorFor(root,quality,chroma);mir2::ChordTracker oldDefault,explicitTriads;
        const auto a=oldDefault.process(chroma),b=explicitTriads.process(chroma,0.65,0.2,false);
        require(a.root==root&&a.quality==quality&&a.root==b.root&&a.quality==b.quality&&a.strength==b.strength,"default vocabulary compatibility");
    }
    vectorFor(0,0,chroma);mir2::ChordTracker tracker;tracker.process(chroma,0.65,2.0,true);
    vectorFor(0,5,chroma);auto r=tracker.process(chroma,0.65,2.0,true);
    require(r.quality==0&&!r.changed,"transition penalty delays new seventh label");
    for(int i=0;i<40;++i)r=tracker.process(chroma,0.65,2.0,true);
    require(r.quality==5,"persistent seventh evidence eventually changes label");
    for(int i=0;i<12;++i)chroma[i]=0;r=tracker.process(chroma,0.65,100.0,true);
    require(r.root==-1&&r.changed,"silence overrides transition penalty");
    vectorFor(0,0,chroma);r=tracker.process(chroma,0.65,0.2,false);require(r.root==0&&r.quality==0,"vocabulary switch resets dimension safely");
}
}
int main(){exactAndAmbiguous();incompleteAndSilence();compatibilityAndTransitions();
    double chroma[12];vectorFor(0,4,chroma);mir2::ChordTracker tracker;
    const auto begin=std::chrono::steady_clock::now();double sink=0;
    for(int i=0;i<10000;++i)sink+=tracker.process(chroma,0.65,0.2,true).strength;
    std::cout<<"109-state chord engine: "<<std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/10000<<" us/update; "<<sink<<'\n';
    std::cout<<"Extended chord failures: "<<failures<<'\n';return failures?1:0;
}
