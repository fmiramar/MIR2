// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Tonal.hpp"
#include "mir2/Harmonic.hpp"
#include "mir2/FFT.hpp"
#include <vector>
#include <iostream>
#include <chrono>
#include <limits>
namespace {
int failures=0;
void check(bool ok,const char* name){if(!ok){std::cerr<<"FAIL "<<name<<'\n';++failures;}}
bool near(double a,double b,double tolerance=1e-9){return std::abs(a-b)<=tolerance;}
void hpcpTests(){
    double output[120];
    for(int size:{12,24,36,120})for(int pc=0;pc<12;++pc){
        const double hz=440*std::exp2((pc-9.0)/12.0),tuning=440*std::exp2(23.0/1200);
        mir2::SpectralPeak peak{hz*tuning/440,1,1};
        mir2::harmonicPitchClassProfile(&peak,1,output,size,tuning,1);
        const int maximum=int(std::max_element(output,output+size)-output);
        check(maximum==pc*size/12,"HPCP pitch class and tuning covariance");check(near(output[maximum],1),"HPCP max normalization");
    }
    mir2::SpectralPeak peaks[8];for(int h=1;h<=8;++h)peaks[h-1]={220.0*h,1.0/h,h};
    mir2::harmonicPitchClassProfile(peaks,8,output,12);
    check(std::max_element(output,output+12)-output==9,"HPCP harmonic A series");
    mir2::harmonicPitchClassProfile(peaks,8,output,12,440,8,2,2);
    double sum=0;for(int i=0;i<12;++i)sum+=output[i];check(near(sum,1),"HPCP sum normalization");
    for(double cents:{-49.0,-20.0,0.0,31.0,49.0}){
        for(int i=0;i<12;++i)peaks[i%8]={440*std::exp2((i-6)/12.0+cents/1200),1,i};
        auto tuning=mir2::tuningFromPeaks(peaks,8);
        check(near(tuning.cents,cents,1e-8)&&near(tuning.confidence,1),"circular tuning all pitch classes");
    }
    check(mir2::tuningFromPeaks(peaks,0).frequency==0,"tuning silence");
    std::fill(output,output+12,1);check(near(mir2::chromaEntropy(output,12),1)&&near(mir2::chromaCrest(output,12),1),"uniform chroma entropy crest");
    std::fill(output,output+12,0);output[0]=1;check(near(mir2::chromaEntropy(output,12),0)&&near(mir2::chromaCrest(output,12),12),"singleton chroma entropy crest");
    double tonal[6];mir2::tonalCentroid(output,tonal);
    check(near(tonal[0],0)&&near(tonal[1],1)&&near(tonal[2],0)&&near(tonal[3],1)&&near(tonal[4],0)&&near(tonal[5],0.5),"C tonal-centroid analytic coordinates");
    std::fill(output,output+12,1);mir2::tonalCentroid(output,tonal);
    for(double x:tonal)check(near(x,0),"uniform chroma maps to origin");
    const double input[12]{0.41,0.21,0.11,0.06,0.04,0.04,0.03,0.02,0.02,0.02,0.02,0.02};
    // L1 is 1.10; independently derive threshold memberships after normalization.
    double quantized[12];mir2::quantizeCENS(input,quantized);
    double total=0;for(double x:input)total+=x;
    for(int i=0;i<12;++i){double expected=0;for(double boundary:{0.05,0.1,0.2,0.4})if(input[i]/total>boundary)expected+=0.25;check(near(quantized[i],expected),"CENS quantization after L1");}
}
void keyChordTests(){
    for(int profile=0;profile<3;++profile)for(int mode=0;mode<2;++mode)for(int root=0;root<12;++root){
        double chroma[12];const double* p=mir2::keyProfile(profile,mode);
        for(int i=0;i<12;++i)chroma[i]=p[(i-root+12)%12];
        const auto key=mir2::estimateKey(chroma,profile);
        check(key.root==root&&key.mode==mode&&near(key.strength,1),"24 key rotations each profile");
    }
    double chroma[12]{};auto key=mir2::estimateKey(chroma,0);check(key.root==-1&&key.mode==-1,"no-key silence");
    std::fill(chroma,chroma+12,1);check(mir2::estimateKey(chroma,1).root==-1,"uniform has no key");
    for(int quality=0;quality<2;++quality)for(int root=0;root<12;++root){
        std::fill(chroma,chroma+12,0);chroma[root]=chroma[(root+(quality==0?4:3))%12]=chroma[(root+7)%12]=1;
        mir2::ChordTracker tracker;const auto chord=tracker.process(chroma);
        check(chord.root==root&&chord.quality==quality&&near(chord.strength,1),"all major minor chord templates");
        auto held=tracker.process(chroma);check(!held.changed,"same chord no trigger");
        std::fill(chroma,chroma+12,0);auto silence=tracker.process(chroma);check(silence.root==-1&&silence.changed,"silence clears chord and triggers transition");
    }
    std::fill(chroma,chroma+12,1);mir2::ChordTracker tracker;check(tracker.process(chroma).root==-1,"uniform has no chord");
    std::fill(chroma,chroma+12,0);chroma[0]=1;tracker.reset();check(tracker.process(chroma).root==-1,"single pitch is not a triad");
}
void harmonicTests(){
    mir2::SpectralPeak peaks[]{{100,1,10},{200,0.5,20},{300,0.25,30},{400,0.125,40},{500,0.0625,50}};
    auto frame=mir2::associateHarmonics(peaks,5,100,8);auto s=mir2::summarizeHarmonics(frame);
    check(frame.count==5&&near(s.inharmonicity,0),"exact harmonic association");
    check(near(s.tristimulus[0],1/1.9375)&&near(s.tristimulus[1],0.875/1.9375)&&near(s.tristimulus[2],0.0625/1.9375),"tristimulus analytic ratios");
    check(near(s.centroid,(100+100+75+50+31.25)/1.9375),"harmonic amplitude centroid");
    check(near(s.oddEven,(1+0.0625+0.00390625)/(0.25+0.015625)),"odd even power ratio");
    auto missing=mir2::associateHarmonics(peaks+1,4,100,8);check(missing.frequencies[0]==0&&missing.frequencies[1]==200,"missing fundamental preserves indices");
    auto old=frame;for(int i=0;i<5;++i)frame.amplitudes[i]*=2;
    check(near(mir2::summarizeHarmonics(frame,&old).variation,0),"variation invariant under common gain");
    double last=-1;
    for(double stretch:{0.0,0.001,0.003,0.006}){
        mir2::SpectralPeak stretched[5];for(int i=0;i<5;++i)stretched[i]={100*(i+1)*std::sqrt(1+stretch*(i+1)*(i+1)),1,i};
        auto f=mir2::associateHarmonics(stretched,5,100,8,250);auto r=mir2::summarizeHarmonics(f);
        check(r.inharmonicity>=last,"stretched partials increase inharmonicity");last=r.inharmonicity;
    }
    std::vector<double> magnitude(100,0);for(auto& p:peaks)magnitude[p.bin]=p.magnitude;
    check(near(mir2::harmonicEnergyRatio(magnitude.data(),100,old),1),"harmonic energy only");
    magnitude[70]=1;check(mir2::harmonicEnergyRatio(magnitude.data(),100,old)<0.6,"unassociated energy lowers harmonic ratio");
    auto invalid=mir2::associateHarmonics(peaks,5,std::numeric_limits<double>::quiet_NaN(),64);
    check(invalid.count==0,"invalid F0 safe");
    mir2::SpectralPeak pair[]{{440,1,1},{470,0.5,2}};
    const double x=0.24*30/(0.0207*440+18.96);
    const double expected=2.5*(std::exp(-3.51*x)-std::exp(-5.75*x));
    check(near(mir2::sensoryRoughness(pair,2),expected),"Sethares two-sine analytic value");
    pair[1].frequency=440;check(near(mir2::sensoryRoughness(pair,2),0),"unison roughness zero");
}
void spectralIntegration(){
    for(double sr:{44100.,48000.,96000.})for(int n:{2048,4096,8192}){
        std::vector<mir2::Complex> frame(n);std::vector<double> magnitude(n/2+1);
        const double fundamental=sr*20/n;
        for(int i=0;i<n;++i){double value=0;for(int h=1;h<=5;++h)value+=std::sin(mir2::kTwoPi*fundamental*h*i/sr)/h;
            frame[i]={value*(0.5-0.5*std::cos(mir2::kTwoPi*i/n)),0};}
        mir2::fft(frame.data(),n,false);for(int i=0;i<=n/2;++i)magnitude[i]=std::hypot(frame[i].real,frame[i].imag);
        mir2::SpectralPeak peaks[128];const int count=mir2::extractSpectralPeaks(magnitude.data(),n/2+1,sr/n,40,0,0.01,peaks,128);
        auto harmonics=mir2::associateHarmonics(peaks,count,fundamental,16);
        check(harmonics.count==5,"FFT extraction and harmonic association sample/frame matrix");
        check(mir2::summarizeHarmonics(harmonics).inharmonicity<1e-6,"FFT exact harmonic inharmonicity");
    }
}
void benchmark(){
    mir2::SpectralPeak peaks[128];double output[120];for(int i=0;i<128;++i)peaks[i]={50.0+40*i,1.0/(1+i*0.01),i};
    const auto start=std::chrono::steady_clock::now();double sink=0;
    for(int n=0;n<2000;++n){mir2::harmonicPitchClassProfile(peaks,128,output,120);sink+=output[n%120];}
    std::cout<<"HPCP 128 peaks x 120 bins x 8 harmonics: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/2000<<" ms/frame ("<<sink<<")\n";
}
}
int main(){hpcpTests();keyChordTests();harmonicTests();spectralIntegration();benchmark();std::cout<<"Tonal/harmonic failures: "<<failures<<'\n';return failures?1:0;}
