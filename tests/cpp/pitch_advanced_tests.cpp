// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/PitchAdvanced.hpp"
#include <vector>
#include <iostream>
#include <random>
#include <chrono>
#include <cstdlib>

namespace {
int failures = 0;
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
struct Work {
    int n;
    double sr;
    std::vector<float> frame;
    std::vector<mir2::Complex> fft;
    std::vector<double> prefix, difference, cmndf, nsdf, frequency, erb, kernels, loudness, salience;
    std::vector<double> magnitude, white, residual;
    mir2::PitchWorkspace time;
    mir2::SwipeWorkspace swipe;
    Work(int size, double rate, double low = 55, double high = 1760)
        : n(size), sr(rate), frame(n), fft(2*n), prefix(n+1), difference(n), cmndf(n), nsdf(n),
          frequency(mir2::pitchGridSize(low,high,60)), erb(int(std::floor(mir2::pitchERB(sr/2)/0.1))),
          kernels(frequency.size()*erb.size()), loudness(6*erb.size()), salience(frequency.size()),
          magnitude(n+1),white(n+1),residual(n+1),
          time{fft.data(),prefix.data(),difference.data(),cmndf.data(),nsdf.data(),n,2*n},
          swipe{fft.data(),frequency.data(),erb.data(),kernels.data(),loudness.data(),salience.data(),
                n,int(frequency.size()),int(erb.size()),0,60,sr} {
        mir2::initializeSwipe(swipe,low);
    }
    void tone(double hz, int first = 1, int last = 1, double gain = 0.5, bool add = false) {
        if (!add) std::fill(frame.begin(),frame.end(),0.0f);
        for(int i=0;i<n;++i) for(int h=first;h<=last;++h)
            if(h*hz<sr/2) frame[i]+=float(gain/h*std::sin(mir2::kTwoPi*h*hz*i/sr));
    }
    mir2::CandidateList swipeCandidates() {
        mir2::analyzeSwipe(frame.data(),swipe);
        return mir2::salienceCandidates(salience.data(),frequency.data(),int(frequency.size()),60);
    }
    mir2::CandidateList multiple(int maximum=6) {
        mir2::pitchMagnitude(frame.data(),n,fft.data(),magnitude.data());
        mir2::whitenPitchSpectrum(magnitude.data(),white.data(),n+1,sr/(2*n));
        return mir2::multiPitch(white.data(),residual.data(),n+1,sr/(2*n),frequency.data(),
            int(frequency.size()),60,salience.data(),maximum);
    }
};
bool contains(const mir2::CandidateList& c,double hz,double cents=35) {
    for(int i=0;i<c.count;++i) if(mir2::centsDistance(c.values[i].frequencyHz,hz)<cents) return true;
    return false;
}
void candidateTests() {
    require(std::abs(mir2::pitchThresholdCDF(0.0))<1e-12 && std::abs(mir2::pitchThresholdCDF(1)-1)<1e-12,"Beta CDF endpoints");
    // Numerical integration of the independently expressed beta density.
    double integrated=0; const int steps=100000;
    for(int i=0;i<steps;++i) { double x=(i+0.5)*0.2/steps; integrated+=342*x*std::pow(1-x,17)*0.2/steps; }
    require(std::abs(integrated-mir2::pitchThresholdCDF(0.2))<1e-8,"Beta CDF agrees with density integral");
    for(double sr : {44100.0,48000.0,96000.0}) {
        Work w(4096,sr); w.tone(220,2,8);
        double rms=0; require(mir2::preparePitchEvidence(w.frame.data(),w.time,rms),"time evidence preparation");
        auto c=mir2::timePitchCandidates(w.time,sr,55,1760,2,64);
        require(c.count>0 && mir2::centsDistance(c.values[0].frequencyHz,220)<10,"probYin missing fundamental");
        require(c.mass()<=1.0 && c.mass()>0.9,"probYin probability conservation");
        std::mt19937 rng(42); std::normal_distribution<double> noise(0,0.2);
        for(auto& v:w.frame) v=float(noise(rng));
        mir2::preparePitchEvidence(w.frame.data(),w.time,rms);
        c=mir2::timePitchCandidates(w.time,sr,55,1760,2,64);
        require(c.mass()<0.05,"probYin noise leaves unvoiced mass");
    }
    mir2::CandidateList c; c.add({220,0.5,0.5,0,0,0}); c.add({440,0.5,0.5,0,0,0});
    auto u=mir2::pitchUncertainty(c);
    require(std::abs(u.entropy-1)<1e-12 && std::abs(u.octave-1)<1e-12,"equal octave competitors have maximum ambiguity");
}
void swipeTests() {
    require(mir2::primeHarmonic(1) && mir2::primeHarmonic(2) && mir2::primeHarmonic(17)
        && !mir2::primeHarmonic(4) && !mir2::primeHarmonic(9),"prime harmonic set");
    require(std::abs(mir2::swipePrimeKernel(400,100))<1e-10,"SWIPE prime excludes composite harmonic centre");
    require(mir2::swipePrimeKernel(200,100)>0 && mir2::swipePrimeKernel(150,100)<0,"SWIPE signed lobes");
    for(double sr : {44100.0,48000.0,96000.0}) for(int n : {4096,8192}) {
        Work w(n,sr);
        for(double hz : {110.0,220.0,440.0,880.0}) {
            w.tone(hz,1,8); auto c=w.swipeCandidates();
            if(!c.count || mir2::centsDistance(c.values[0].frequencyHz,hz)>35)
                std::cerr<<"SWIPE sr="<<sr<<" n="<<n<<" hz="<<hz<<" result="<<(c.count?c.values[0].frequencyHz:0)<<'\n';
            require(c.count && mir2::centsDistance(c.values[0].frequencyHz,hz)<35,"SWIPE harmonic tone pitch");
        }
        w.tone(220,2,8); auto c=w.swipeCandidates();
        require(c.count && mir2::centsDistance(c.values[0].frequencyHz,220)<35,"SWIPE missing fundamental");
        std::fill(w.frame.begin(),w.frame.end(),0); c=w.swipeCandidates(); require(c.count==0,"SWIPE silence");
    }
    Work w(8192,48000); w.tone(440,1,12);
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<30;++i) w.swipeCandidates();
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30;
    std::cout<<"SWIPE 8192 / 60 bins per octave: "<<elapsed<<" ms/frame; initial frame 170.667 ms, hop 21.333 ms\n";
}
void polyphonyTests() {
    Work w(8192,48000);
    for(const auto& notes : {std::vector<double>{220},std::vector<double>{220,329.6276},
                            std::vector<double>{220,277.1826,329.6276}}) {
        std::fill(w.frame.begin(),w.frame.end(),0);
        for(double f:notes) w.tone(f,1,8,0.25,true);
        auto c=w.multiple();
        std::cout<<"MultiPitch expected "<<notes.size()<<" got "<<c.count<<":";
        for(int i=0;i<c.count;++i) std::cout<<' '<<c.values[i].frequencyHz;
        std::cout<<'\n';
        for(double f:notes) require(contains(c,f),"MultiPitch isolated/dyad/triad fundamentals");
        require(c.count==int(notes.size()),"MultiPitch polyphony selection");
    }
    auto start=std::chrono::steady_clock::now();
    for(int i=0;i<30;++i) w.multiple();
    std::cout<<"MultiPitch 8192: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30<<" ms/frame\n";
}
void contourTests() {
    mir2::PitchTracker tracker;
    mir2::CandidateList c; c.add({220,0.9,0.9,0,0,0});
    for(int i=0;i<10;++i) tracker.process(c);
    c.clear(); c.add({440,0.55,0.55,0,0,0}); c.add({220,0.4,0.4,0,0,0});
    require(mir2::centsDistance(tracker.process(c).frequency,220)<1,"DP rejects one-frame octave glitch");
    c.clear(); require(tracker.process(c).frequency==0,"DP silence does not hold stale frequency");
    tracker.reset(); c.add({330,1,1,0,0,0});
    require(tracker.process(c,600,1,1,2,2).frequency==0,"bounded traceback waits for lag");
    tracker.process(c,600,1,1,2,2);
    require(tracker.process(c,600,1,1,2,2).frequency==330,"bounded traceback emits after lag");
    double f[]{220,220,440},p[]{1,1,1}; auto result=mir2::pitchConsensus(f,p,3);
    require(result.frequency>219.99 && result.frequency<220.01 && std::abs(result.agreement-2.0/3)<1e-12
        && std::abs(result.octaveConflict-1.0/3)<1e-12,"consensus preserves octave disagreement");
    mir2::MelodyTracker melody; c.clear(); c.add({330,1,0.7,0,1,0}); c.add({110,0.4,0.3,0,0.4,0});
    for(int i=0;i<10;++i) result.frequency=melody.process(c,0.02).frequency;
    require(result.frequency==330,"melody contour selects persistent salient track");
    c.clear(); require(melody.process(c,0.02).frequency==0,"melody gaps output undefined pitch");
}
void descriptorTests() {
    require(std::abs(mir2::pitchDeviation(880,440,0)-1200)<1e-12,"reference cents");
    require(std::abs(mir2::pitchDeviation(440*std::exp2(27.0/1200),440,1)-27)<1e-9,"12TET cents");
    double cents[]{-50,50}, weights[]{1,1}; auto s=mir2::pitchStability(cents,weights,2);
    require(std::abs(s.deviation-50)<1e-12 && std::abs(s.stability-std::exp(-1))<1e-12,"weighted pitch variance");
    std::vector<double> contour(200), confidence(200,1);
    for(int i=0;i<200;++i) contour[i]=1000+3*i+30*std::sin(mir2::kTwoPi*5.5*i/100.0);
    auto v=mir2::analyzeVibrato(contour.data(),confidence.data(),200,100,3,12,5);
    require(v.present==1 && std::abs(v.rate-5.5)<0.1 && std::abs(v.extent-60)<2,"vibrato rate/extent on glissando");
    for(int i=0;i<200;++i) contour[i]=1000+3*i;
    v=mir2::analyzeVibrato(contour.data(),confidence.data(),200,100,3,12,5);
    require(v.present==0,"pure glissando is not vibrato");
    for(int i=0;i<100;++i) confidence[i]=0;
    v=mir2::analyzeVibrato(contour.data(),confidence.data(),200,100,3,12,5);
    require(v.present==0,"vibrato rejects insufficient voiced coverage");
}
}
int main() {
    candidateTests(); swipeTests(); polyphonyTests(); contourTests(); descriptorTests();
    std::cout<<"Advanced pitch failures: "<<failures<<'\n'; return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
