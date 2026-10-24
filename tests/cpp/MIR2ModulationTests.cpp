// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/RhythmModulation.hpp"
#include <cassert>
#include <complex>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

struct Fixture {
    mir2::ModulationConfig c;
    std::vector<double> frame, window, re, im, magnitude, phase, scratch;
    double sum;
    explicit Fixture(mir2::ModulationConfig config) : c(config), frame(c.frames*c.bands),
        window(c.frames), re(c.frames*c.bins), im(re.size()), magnitude(c.bands*c.bins),
        phase(magnitude.size()), scratch(magnitude.size()) {
        assert(mir2::validModulationConfig(c));
        sum = mir2::prepareModulationBasis(c, window.data(), re.data(), im.data());
    }
    void analyze() {
        mir2::modulationSpectrum(frame.data(), c, window.data(), sum, re.data(), im.data(), magnitude.data(), phase.data());
    }
};
static void near(double a, double b, double tolerance=1e-10) { assert(std::abs(a-b) < tolerance); }

// Independent long-double DFT oracle: builds the window and complex exponential
// directly, without using the production frequency grid/basis or magnitude code.
static void directReference() {
    mir2::ModulationConfig c; c.bands=3; c.bins=19; c.frames=127;
    c.featureRate=79.0; c.minHz=0; c.maxHz=15.3; c.phase=true;
    Fixture f(c); std::mt19937 random(314159); std::uniform_real_distribution<double> dist(-1,1);
    for (double& value : f.frame) value=dist(random);
    for (int centered=0; centered<2; ++centered) {
        f.c.removeMean=centered>0; f.analyze();
        for (int band=0; band<c.bands; ++band) {
            long double norm=0, mean=0;
            for (int n=0; n<c.frames; ++n) {
                const long double w=(1-std::cos(2*std::acos(-1.L)*n/(c.frames-1)))/2;
                norm+=w; mean+=w*f.frame[band*c.frames+n];
            }
            mean=centered ? mean/norm : 0;
            for (int k=0; k<c.bins; ++k) {
                const long double hz=c.minHz+(c.maxHz-c.minHz)*k/(c.bins-1);
                std::complex<long double> result=0;
                for (int n=0; n<c.frames; ++n) {
                    const long double w=(1-std::cos(2*std::acos(-1.L)*n/(c.frames-1)))/2;
                    result+=(static_cast<long double>(f.frame[band*c.frames+n])-mean)*w*
                        std::exp(std::complex<long double>(0,-2*std::acos(-1.L)*hz*n/c.featureRate));
                }
                const double amplitude=static_cast<double>(std::abs(result)*(k==0?1:2)/norm);
                near(f.magnitude[band*c.bins+k],amplitude);
                if(amplitude>1e-11) {
                    const double expected=static_cast<double>(std::arg(result)+2*std::acos(-1.L)*hz*(c.frames-1)/c.featureRate);
                    near(std::remainder(f.phase[band*c.bins+k]-expected,mir2::kTwoPi),0);
                }
            }
        }
    }
    std::fill(f.frame.begin(),f.frame.end(),2.0); f.analyze();
    for(double value:f.magnitude) near(value,0);
    f.c.removeMean=false; f.analyze(); near(f.magnitude[0],2);
    std::fill(f.frame.begin(),f.frame.end(),std::numeric_limits<double>::quiet_NaN());
    f.analyze(); for(double value:f.magnitude) near(value,0);
}

static void streamTimingAndAM() {
    for(double rate:{44100.,48000.,96000.}) for(int block:{32,64,128}) {
        mir2::ModulationTiming t;
        assert(mir2::modulationTiming(rate,block,100,2.0,.1,t));
        mir2::ModulationConfig c; c.bands=2;c.bins=13;c.frames=t.frames;
        c.featureRate=t.featureRate;c.minHz=1;c.maxHz=7;c.phase=true;
        Fixture f(c); std::vector<double> ring(c.bands*c.frames);
        mir2::ModulationStream stream; stream.ring=ring.data();stream.frame=f.frame.data();stream.initialize(c,t,0,1);
        int analyses=0, last=0;
        for(int step=1;step<=t.poolBlocks*(t.frames+4*t.hopFrames);++step) {
            const double now=(step-.5)*block/rate;
            const double input[2]={1+.4*std::cos(mir2::kTwoPi*2*now+.2),2+.7*std::cos(mir2::kTwoPi*5*now-.3)};
            if(stream.push(input)) {
                if(analyses==0) assert(step==t.frames*t.poolBlocks);
                else assert(step-last==t.hopFrames*t.poolBlocks);
                last=step; ++analyses; f.analyze();
                for(int band=0;band<2;++band) {
                    const auto begin=f.magnitude.begin()+band*c.bins;
                    const int peak=static_cast<int>(std::max_element(begin,begin+c.bins)-begin);
                    assert(peak==(band==0?2:8));
                    near(f.magnitude[band*c.bins+peak],band==0?.4:.7,.005);
                    const double latestCenter=(step-.5*t.poolBlocks)*block/rate;
                    const double expected=mir2::kTwoPi*(band==0?2:5)*latestCenter+(band==0?.2:-.3);
                    near(std::remainder(f.phase[band*c.bins+peak]-expected,mir2::kTwoPi),0,.002);
                }
            }
        }
        assert(analyses==5);
        // A constant tail completely flushes stale rhythmic energy after a full window.
        for(int i=0;i<t.poolBlocks*(t.frames+t.hopFrames);++i) {
            const double values[2]={2,3}; if(stream.push(values)) f.analyze();
        }
        for(double value:f.magnitude) near(value,0);
    }
}

static void shapeAndBounds() {
    mir2::ModulationConfig c;c.bands=4;c.bins=17;c.frames=200;c.featureRate=100;c.minHz=0;c.maxHz=8;
    Fixture f(c);
    near(mir2::modulationFluctuationWeight(4),1);
    near(mir2::modulationFluctuationWeight(2),mir2::modulationFluctuationWeight(8));
    near(mir2::compressModulationInput(-2,1,3),0);
    near(mir2::compressModulationInput(2,1,3),std::log(7));
    std::fill(f.magnitude.begin(),f.magnitude.end(),1);
    c.smoothBands=1.3;c.smoothBins=2.2;
    mir2::shapeModulationPattern(f.magnitude.data(),f.scratch.data(),c);
    for(double value:f.magnitude) near(value,1);
    c.fluctuation=true;c.normalization=2;
    mir2::shapeModulationPattern(f.magnitude.data(),f.scratch.data(),c);
    for(int band=0;band<c.bands;++band) {
        double sum=0;for(int bin=0;bin<c.bins;++bin) sum+=std::pow(f.magnitude[band*c.bins+bin],2);
        near(sum,1);
    }
    c.normalization=1;mir2::shapeModulationPattern(f.magnitude.data(),f.scratch.data(),c);
    double total=0;for(double value:f.magnitude)total+=value*value;near(total,1);
    c.bands=32;c.bins=128;assert(!mir2::validModulationConfig(c));
    c.bands=1;c.bins=128;c.frames=4097;assert(!mir2::validModulationConfig(c));
    c.frames=200;c.maxHz=41;assert(!mir2::validModulationConfig(c));
    mir2::ModulationTiming timing;
    assert(!mir2::modulationTiming(0,64,100,4,.1,timing));
    assert(!mir2::modulationTiming(48000,64,100,13,.1,timing));
}
int main() {
    directReference();streamTimingAndAM();shapeAndBounds();
    std::cout << "MIR2 modulation: independent DFT, AM/phase, pooling timing, silence, shaping and bounds pass\n";
}
