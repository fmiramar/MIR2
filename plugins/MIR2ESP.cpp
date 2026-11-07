// SPDX-License-Identifier: GPL-3.0-or-later
#include "MIR2Plugin.hpp"
#include "mir2/MS20ESP.hpp"
#include <new>
namespace {
using namespace mir2::esp;
// Server allocates Unit memory without running C++ member constructors.
template<class K> struct ESPUnit : Unit {
    alignas(K) unsigned char storage[sizeof(K)];
    K& kernel() noexcept { return *std::launder(reinterpret_cast<K*>(storage)); }
};
struct FreqToVolt : ESPUnit<FreqToVoltKernel> {};
struct Periodicity2 : ESPUnit<Periodicity2Kernel> {};
struct EnvFollow2 : ESPUnit<EnvelopeKernel> {};
struct TriggerDetector : ESPUnit<TriggerKernel> {};
Mode mode(float x) { return x==1 ? Mode::circuit : Mode::digital; }
Conditioning conditioning(Unit* unit) {
    Conditioning p;p.lowCut=IN0(1);p.highCut=IN0(2);p.inputGain=IN0(5);
    p.threshold=IN0(6);p.hysteresis=IN0(7);return p;
}
template<class U,class F> void eachInput(U* unit,F fn) {
    const bool audio=INRATE(0)==calc_FullRate;
    const int n=unit->mWorld->mFullRate.mBufLength;
    const float* in=IN(0);
    for(int i=0;i<n;++i) fn(audio?in[i]:in[0]);
}
void FreqToVolt_next(FreqToVolt* unit,int) {
    FVOutput o;eachInput(unit,[&](double x){o=unit->kernel().process(x);});
    OUT0(0)=o.cv;OUT0(1)=o.frequency;OUT0(2)=o.eventRate;OUT0(3)=o.valid;
}
void FreqToVolt_Ctor(FreqToVolt* unit) {
    DigitalFVParameters d;d.conditioning=conditioning(unit);d.cvAdjust=IN0(3);
    d.smoothTime=IN0(9);d.referenceMaxFreq=IN0(10);d.referenceMaxEventRate=IN0(11);d.timeout=IN0(12);
    CircuitFVParameters c;c.conditioning=d.conditioning;c.cvAdjust=d.cvAdjust;c.pulseWidth=IN0(8);
    c.referenceMaxFreq=d.referenceMaxFreq;c.referenceMaxEventRate=d.referenceMaxEventRate;c.timeout=d.timeout;
    new(unit->storage) FreqToVoltKernel(unit->mWorld->mSampleRate,mode(IN0(4)),d,c);
    SETCALC(FreqToVolt_next);ClearUnitOutputs(unit,1);
}
void Periodicity2_next(Periodicity2* unit,int) {
    eachInput(unit,[&](double x){unit->kernel().process(x);});auto o=unit->kernel().output();
    OUT0(0)=o.periodicity;OUT0(1)=o.jitter;OUT0(2)=o.eventConfidence;
}
void Periodicity2_Ctor(Periodicity2* unit) {
    PeriodicityParameters p;p.conditioning.lowCut=IN0(1);p.conditioning.highCut=IN0(2);
    p.conditioning.threshold=IN0(3);p.conditioning.hysteresis=IN0(4);
    p.windowTime=IN0(5);p.minIntervals=static_cast<unsigned>(std::clamp(finite(IN0(6),4),2.0,64.0));
    p.jitterScale=IN0(7);p.timeout=IN0(8);
    new(unit->storage) Periodicity2Kernel(unit->mWorld->mSampleRate,p);
    SETCALC(Periodicity2_next);ClearUnitOutputs(unit,1);
}
void EnvFollow2_next(EnvFollow2* unit,int) {
    double o=0;eachInput(unit,[&](double x){o=unit->kernel().process(x);});OUT0(0)=o;
}
void EnvFollow2_Ctor(EnvFollow2* unit) {
    EnvelopeParameters p;p.inputGain=IN0(1);p.attackTime=IN0(3);p.releaseTime=IN0(4);p.referenceEnvVolts=IN0(5);
    new(unit->storage) EnvelopeKernel(unit->mWorld->mSampleRate,mode(IN0(2)),p);
    SETCALC(EnvFollow2_next);OUT0(0)=0;
}
void TriggerDetector_next(TriggerDetector* unit,int) {
    // Envelope is control rate; evaluate once so activation lasts one output evaluation.
    auto o=unit->kernel().process(IN0(0));OUT0(0)=o.trigger;OUT0(1)=o.gate;OUT0(2)=o.hardwareGate;
}
void TriggerDetector_Ctor(TriggerDetector* unit) {
    new(unit->storage) TriggerKernel(IN0(1),IN0(2));
    SETCALC(TriggerDetector_next);OUT0(0)=OUT0(1)=0;OUT0(2)=1;
}
}
namespace mir2plugin {
void registerESPUnits() {
    DefineSimpleUnit(FreqToVolt);DefineSimpleUnit(Periodicity2);
    DefineSimpleUnit(EnvFollow2);DefineSimpleUnit(TriggerDetector);
}
}
