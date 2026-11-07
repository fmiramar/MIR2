// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/MS20ESP.hpp"
#include <cassert>
#include <complex>
#include <iostream>
#include <limits>
using namespace mir2::esp;
using C=std::complex<double>;
constexpr double pi=3.14159265358979323846;
static void bounds(FVOutput o) {
    for(double x:{o.cv,o.frequency,o.eventRate,o.valid}) assert(std::isfinite(x)&&x>=0&&x<=1);
}
// Independent nodal solve: matrix [A B; C D] applied to [node_a node_b].
static C solve(C a,C b,C c,C d,C drive) { return -c*drive/(a*d-b*c); }
static C lp(C s,double r,double cf,double cg) {
    return solve(2/r+s*cf,-1/r-s*cf,-1/r,1/r+s*cg,1/r);
}
static C filterReference(double sr,double hz,double low,double high) {
    C s(0,2*sr*std::tan(pi*hz/sr));
    const double r=lowCutResistance(low),rs=680000.0/1680.0,k=1000.0/1680.0;
    const C y=1.0/(rs+1.0/(s*33e-9));
    C h=solve(y+s*33e-9+1/r,-s*33e-9-1/r,-s*33e-9,s*33e-9+1/r,k*y);
    const double lr=std::clamp(1/(2*pi*high*15e-9),2000.0,102000.0);
    return h*lp(s,lr,15e-9,15e-9)*lp(s,lr,150e-9,15e-9);
}
int main() {
    // Isolated fractional pulse: total area exactly equals width, even below one sample.
    for(double sr:{44100.,48000.,96000.}) {
        double dt=1/sr;
        for(double width:{0.2*dt,1.7*dt,0.0002}) {
            CircuitPulse pulse; double area=0;
            for(int i=0;i<100;++i) area+=pulse.process(dt,i==0,0.3*dt,width).coverage*dt;
            assert(std::abs(area-width)<1e-15);
        }
        CircuitPulse overlap;
        double area=overlap.process(dt,true,0,2.5*dt).coverage*dt;
        auto second=overlap.process(dt,true,0.5*dt,2.5*dt);
        assert(second.overlap); area+=second.coverage*dt;
        for(int i=0;i<10;++i) area+=overlap.process(dt,false,0,2.5*dt).coverage*dt;
        assert(std::abs(area-4*dt)<1e-15);
        // Compare transfer magnitude AND phase to an independent analog nodal solve.
        for(double low:{50.,80.,2500.}) for(double high:{100.,2000.,5000.}) {
            for(double hz:{100.,500.,2000.}) {
                CircuitFilters f(sr,low,high); C measured=0;
                for(int i=0;i<static_cast<int>(2*sr);++i) {
                    const double phase=2*pi*hz*i/sr;
                    const double y=f.process(std::cos(phase)); assert(std::isfinite(y));
                    if(i>=sr) measured+=y*std::exp(C(0,-phase))*2.0/sr;
                }
                const C expected=filterReference(sr,hz,low,high);
                assert(std::abs(measured-expected)<1e-8+1e-7*std::abs(expected));
            }
        }
        for(double hz:{110.,220.,440.,880.}) {
            CircuitFVParameters p; p.conditioning.highCut=5000;
            CircuitFreqToVolt f(sr,p); double volts=0,rate=0; int count=0;
            for(int i=0;i<static_cast<int>(2*sr);++i) {
                auto o=f.process(4*std::sin(2*pi*hz*i/sr)); bounds(o);
                if(i>=sr) { volts+=f.internalVolts();rate+=f.internalEventRate();++count;assert(o.valid==1); }
            }
            volts/=count; rate/=count;
            assert(std::abs(rate/(2*hz)-1)<0.003);
            assert(std::abs(volts/(8.4*hz/2000)-1)<0.003);
            std::cout<<"sr="<<sr<<" hz="<<hz<<" meanV="<<volts<<" rate="<<rate<<'\n';
            for(int i=0;i<sr;++i) bounds(f.process(0));
            assert(f.process(0).valid==0 && f.internalVolts()<1e-9);
        }
    }
    // Output smoothing has the analytic critically-damped step response.
    CircuitSection smoothing; smoothing.lowpass(48000,10000,10000,56e-9,56e-9);
    for(int i=0;i<4800;++i) {
        double t=(i+0.5)/48000, wt=t/(10000*56e-9);
        double expected=1-(1+wt)*std::exp(-wt);
        assert(std::abs(smoothing.process(1)-expected)<0.001);
    }
    // Persistent overlapping pulses saturate duty and invalidate inverse calibration.
    CircuitFVParameters overlapping; overlapping.pulseWidth=0.001;overlapping.conditioning.highCut=5000;
    CircuitFreqToVolt saturatedDuty(48000,overlapping); FVOutput overlappingOutput;
    for(int i=0;i<96000;++i) {
        overlappingOutput=saturatedDuty.process(4*std::sin(2*pi*880*i/48000));bounds(overlappingOutput);
    }
    assert(overlappingOutput.valid==0);
    assert(std::abs(saturatedDuty.internalVolts()-2.1)<1e-7);
    // Zero gain, saturation, nonfinite input and discontinuities.
    for(double gain:{0.,100.}) {
        CircuitFVParameters p;p.cvAdjust=gain;
        CircuitFreqToVolt f(48000,p);FVOutput o;
        for(int i=0;i<48000;++i) {o=f.process(4*std::sin(2*pi*220*i/48000));bounds(o);}
        assert(o.valid==0);assert(o.cv==(gain==0?0:1));
    }
    CircuitFreqToVolt clean(48000);
    for(int i=0;i<10000;++i) {
        auto o=clean.process(i%2?std::numeric_limits<double>::infinity():std::numeric_limits<double>::quiet_NaN());
        bounds(o);assert(o.cv==0&&o.valid==0);
    }
    FreqToVoltKernel selected(48000,Mode::circuit);
    bounds(selected.process(0));
    std::cout<<"ESP circuit tests passed\n";
}
