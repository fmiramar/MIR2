// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/Rhythm.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>
namespace {
void check(bool condition,const char* message){if(!condition){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
void near(double actual,double expected,double tolerance,const char* message){if(!std::isfinite(actual)||std::abs(actual-expected)>tolerance){std::cerr<<"FAIL: "<<message<<" actual="<<actual<<" expected="<<expected<<'\n';std::exit(1);}}
struct Harness {
    std::vector<double> frame,prefix,corr,ac,ft,evidence,phases,ring;
    std::vector<mir2::Complex> fft;
    mir2::RhythmWorkspace work;
    mir2::CausalBeatClock clock;
    mir2::RhythmEstimate estimate {};
    std::vector<double> beats;
    int index=0,filled=0,sample=0;
    explicit Harness(int frames=600,int bins=271,double minBPM=30,double maxBPM=300)
        :frame(static_cast<std::size_t>(frames)),prefix(static_cast<std::size_t>(frames+1)),corr(static_cast<std::size_t>(frames)),
         ac(static_cast<std::size_t>(bins)),ft(static_cast<std::size_t>(bins)),evidence(static_cast<std::size_t>(bins)),phases(static_cast<std::size_t>(bins)),ring(static_cast<std::size_t>(frames)),
         fft(static_cast<std::size_t>(mir2::nextPowerOfTwo(frames*2))),
         work{frame.data(),prefix.data(),fft.data(),corr.data(),ac.data(),ft.data(),evidence.data(),phases.data(),frames,static_cast<int>(fft.size()),bins,100,minBPM,maxBPM}{}
    void process(double value,bool lock=false){
        bool beat,sub;clock.advance(.01,value,.35,lock,beat,sub);
        if(beat)beats.push_back(sample*.01);
        ring[static_cast<std::size_t>(index)]=value;index=(index+1)%work.windowFrames;filled=std::min(work.windowFrames,filled+1);
        if(sample%10==9&&filled>=200){
            for(int i=0;i<filled;++i)frame[static_cast<std::size_t>(i)]=ring[static_cast<std::size_t>((index-filled+work.windowFrames+i)%work.windowFrames)];
            mir2::rhythmEvidence(work,filled,true);
            estimate=mir2::chooseRhythm(work,clock.bpm,120,3);
            clock.observe(estimate,.1,.4,.35,lock);
        }
        ++sample;
    }
};
double pulse(double phase){const double distance=std::min(phase,1-phase);return distance<.08?std::exp(-.5*distance*distance/(.025*.025)):0.;}
void referenceTransforms(){
    Harness h(128,41,60,300);
    for(int i=0;i<128;++i)h.frame[static_cast<std::size_t>(i)]=.3+.4*std::sin(.31*i)+.25*std::cos(.73*i);
    mir2::rhythmEvidence(h.work,128,true);
    const double mean=std::accumulate(h.frame.begin(),h.frame.end(),0.)/128;
    for(int lag=1;lag<64;++lag){double product=0,a=0,b=0;for(int i=lag;i<128;++i){double x=h.frame[static_cast<std::size_t>(i)]-mean,y=h.frame[static_cast<std::size_t>(i-lag)]-mean;product+=x*y;a+=x*x;b+=y*y;}
        near(h.corr[static_cast<std::size_t>(lag)],std::max(0.,product/std::sqrt(a*b)),1e-10,"FFT autocorrelation vs direct finite sum");}
    for(double bpm:{93.,172.,247.}){double r=0,i=0,l1=0;for(int n=0;n<128;++n){double w=.5-.5*std::cos(mir2::kTwoPi*n/127),x=(h.frame[static_cast<std::size_t>(n)]-mean)*w;double angle=mir2::kTwoPi*bpm*n/6000;r+=x*std::cos(angle);i-=x*std::sin(angle);l1+=std::abs(x);}
        const auto f=mir2::rhythmFourier(h.frame.data(),128,100,bpm,mean);near(f.magnitude,std::hypot(r,i),1e-10,"Fourier projection vs direct trig sum");near(f.normalized,std::hypot(r,i)/l1,1e-10,"Fourier L1 normalization");}
    std::fill(h.frame.begin(),h.frame.end(),1.0);mir2::rhythmEvidence(h.work,128,true);for(double v:h.evidence)near(v,0,0,"constant novelty has no tempo");
}
void steadyTempos(){
    for(double bpm:{30.,45.,60.,90.,120.,180.,240.,300.}){
        Harness h;
        for(int n=0;n<1800;++n)h.process(pulse(mir2::wrapUnit(n*.01*bpm/60)));
        std::cout<<"steady "<<bpm<<" -> "<<h.clock.bpm<<" confidence "<<h.clock.confidence<<" clarity "<<h.estimate.clarity<<'\n';
        near(h.clock.bpm,bpm,3.0,"steady tempo including range endpoints");check(h.clock.confidence>.2,"steady confidence");
        int matched=0,total=0;for(double t:h.beats)if(t>8){++total;double cycles=t*bpm/60;double distance=std::abs(cycles-std::round(cycles))*60/bpm;if(distance<.065)++matched;}
        check(total>2&&matched>=static_cast<int>(total*.8),"steady beat timing within 65 ms");
    }
    Harness silent;for(int n=0;n<1000;++n)silent.process(0);near(silent.clock.bpm,0,0,"silent BPM");check(silent.beats.empty(),"no silence beats");
}
void imperfectPatterns(){
    Harness missing;
    for(int n=0;n<2000;++n){const int beat=static_cast<int>(n*.02);double x=pulse(mir2::wrapUnit(n*.02));if(beat%7==6)x=0;missing.process(x);}
    near(missing.clock.bpm,120,3,"missing beats retain tempo");check(missing.clock.confidence>.2,"missing beats confidence");
    Harness accented;
    for(int n=0;n<2000;++n){const int eighth=static_cast<int>(n*.04);double x=pulse(mir2::wrapUnit(n*.04))*(eighth%2==0?1.:.2);accented.process(x);}
    near(accented.clock.bpm,120,3,"weak subdivisions do not force double tempo");
    Harness drift;double phase=0;for(int n=0;n<3000;++n){const double bpm=110+n*.01;drift.process(pulse(phase));phase=mir2::wrapUnit(phase+bpm/6000);}
    near(drift.clock.bpm,140,7,"causal slow tempo drift");
    for(std::size_t i=1;i<drift.beats.size();++i)check(drift.beats[i]-drift.beats[i-1]>.3,"phase correction never duplicates beats");
    for(int n=0;n<800;++n)missing.process(0);near(missing.clock.confidence,0,0,"silence stops clock confidence");
}
void statistics(){
    double times[100];mir2::OnsetWindow onset;onset.times=times;onset.capacity=100;onset.window=4;
    for(int n=0;n<=800;++n)onset.update(n*.01,n%50==0?1:0);
    double rate,mean,sd,cv;onset.statistics(rate,mean,sd,cv);
    near(rate,2,1e-10,"onset rate known pulse count");near(mean,.5,1e-10,"IOI mean");near(sd,0,1e-8,"IOI deviation");
    onset.update(20,0);onset.statistics(rate,mean,sd,cv);near(rate,0,0,"onsets expire without new event");near(mean,0,0,"sparse mean resets");
    onset.update(21,1);onset.update(22,1);onset.statistics(rate,mean,sd,cv);near(rate,.25,0,"high trigger only one edge");
    std::vector<mir2::TempoMoment> memory(100);mir2::TempoWindow stable;stable.values=memory.data();stable.size=100;
    double stability=0;for(int n=0;n<200;++n)stability=stable.update(120,1,.1);near(stability,1,1e-5,"stable constant tempo");
    for(int n=0;n<100;++n)stability=stable.update(n%2?60:180,1,.1);check(stability<.01,"changing tempo low stability");
    for(int n=0;n<100;++n)stability=stable.update(0,0,.1);near(stability,0,0,"invalid tempo not stable");
    mir2::TempoDerivative derivative;double rubato=0;for(int n=0;n<1000;++n)rubato=derivative.update(120*std::exp(.02*n*.01),1,.01,.2,.1);near(rubato,2,1e-8,"known log-tempo acceleration percent per second");
    derivative.update(0,0,.01,0,.1);near(derivative.update(200,1,.01,0,.1),0,0,"invalid gap does not generate derivative spike");
    mir2::BeatEnergy energy;energy.startBlock(1);for(int n=0;n<1000;++n)energy.sample(n%2?.5:-.5,2000);energy.startBlock(0);energy.startBlock(1);near(energy.rms,.5,1e-12,"beat-interval RMS");near(energy.completedPeak,.5,1e-12,"beat-interval peak");
    for(int n=0;n<2002;++n)energy.sample(.5,2000);near(energy.rms,0,0,"missing beat expires RMS");
}
}
int main(){referenceTransforms();steadyTempos();imperfectPatterns();statistics();
    Harness h;for(int n=0;n<600;++n)h.frame[static_cast<std::size_t>(n)]=pulse(mir2::wrapUnit(n*.02));
    const auto start=std::chrono::steady_clock::now();for(int n=0;n<100;++n)mir2::rhythmEvidence(h.work,600,true);
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"Rhythm tests passed; 6 s/100 Hz/271 BPM bins: "<<seconds*1e3/100<<" ms per analysis update\n";
}
