// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/MS20ESP.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace mir2::esp;
static void range(double x) { assert(std::isfinite(x) && x >= 0 && x <= 1); }
static void check(FVOutput o) { range(o.cv); range(o.frequency); range(o.eventRate); range(o.valid); }
static void check(TimingOutput o) { range(o.periodicity); range(o.jitter); range(o.eventConfidence); }
int main() {
    constexpr double pi = 3.14159265358979323846;
    for (double sr : {44100.,48000.,96000.}) {
        for (double hz : {55.,110.,440.,880.}) {
            DigitalFVParameters p;
            p.conditioning.lowCut = p.conditioning.highCut = 0;
            DigitalFreqToVolt fv(sr,p);
            PeriodicityParameters pp; pp.conditioning = p.conditioning;
            Periodicity2Kernel timing(sr,pp);
            double sum = 0; int count = 0;
            for (int i=0;i<static_cast<int>(2*sr);++i) {
                const double x = std::sin(2*pi*hz*i/sr);
                const auto o = fv.process(x); check(o); timing.process(x);
                if (i>sr) { sum += fv.internalEventRate(); ++count; assert(o.valid == 1); }
            }
            const double rate = sum/count;
            assert(std::abs(rate/(2*hz)-1) < 0.003);
            const auto regular = timing.output(); check(regular);
            assert(regular.periodicity > 0.98);
            assert(regular.jitter < 0.01);
            for (int i=0;i<sr;++i) { check(fv.process(0)); timing.process(0); }
            const auto stale = timing.output();
            assert(stale.periodicity == 0 && stale.jitter == 0 && stale.eventConfidence == 0);
            assert(fv.process(0).valid == 0);
            assert(fv.internalEventRate() < 1e-10);
            std::cout << "sr=" << sr << " hz=" << hz << " events/s=" << rate << '\n';
        }
    }
    // Default conditioning preserves a stable crossing rate after filter startup.
    DigitalFreqToVolt conditioned(48000);
    Periodicity2Kernel conditionedTiming(48000);
    for (int i=0;i<96000;++i) {
        const double x = std::sin(2*pi*220*i/48000);
        check(conditioned.process(x)); conditionedTiming.process(x);
    }
    assert(std::abs(conditioned.internalEventRate()/440-1)<0.06);
    assert(conditionedTiming.output().periodicity>0.99);
    // Deterministic irregular event intervals reduce regularity without assuming an F0.
    PeriodicityParameters irregularParameters; irregularParameters.windowTime = 2;
    EventRegularity irregular(irregularParameters); irregular.event(0);
    double irregularTime = 0;
    for (int i=0;i<40;++i) {
        irregularTime += 0.003 + ((i*17)%29)*0.001;
        irregular.event(irregularTime);
    }
    check(irregular.output(irregularTime));
    assert(irregular.output(irregularTime).periodicity < 0.2);
    // Independent event oracle: coefficient of variation for alternating 8/12 ms is 0.2.
    PeriodicityParameters p; p.windowTime = 2; p.timeout = 0.1; p.minIntervals = 4;
    EventRegularity alternating(p); alternating.event(0);
    double t = 0;
    for (int i=0;i<20;++i) { t += i%2 ? 0.012 : 0.008; alternating.event(t); }
    auto o = alternating.output(t); check(o);
    assert(o.jitter == 1);
    assert(std::abs(o.periodicity-std::exp(-0.2/0.15)) < 1e-12);
    // Support gating, duplicate/out-of-order timestamps and fresh restart after a gap.
    EventRegularity regular(p);
    for (int i=0;i<4;++i) regular.event(i*0.01);
    assert(regular.output(0.03).eventConfidence == 0);
    regular.event(0.04);
    assert(std::abs(regular.output(0.04).periodicity-0.5)<1e-12);
    regular.event(0.04); regular.event(0.02);
    assert(std::abs(regular.output(0.04).periodicity-0.5)<1e-12);
    assert(regular.output(0.2).eventConfidence == 0);
    regular.event(0.21); regular.event(0.22);
    assert(regular.output(0.22).eventConfidence == 0);
    // Window is elapsed time, not merely a ring entry count.
    p.windowTime = 0.025;
    EventRegularity shortWindow(p);
    for (int i=0;i<20;++i) shortWindow.event(i*0.01);
    assert(shortWindow.output(0.19).eventConfidence == 0);
    // More events than the fixed capacity remain supported without allocation.
    p.windowTime = 2;
    EventRegularity dense(p);
    for(int i=0;i<1000;++i) dense.event(i*0.001);
    assert(dense.output(0.999).periodicity > 0.999);
    // Duty-cycle asymmetry does not produce alternating intervals with rising-only timing.
    PeriodicityParameters pp; pp.conditioning.lowCut = pp.conditioning.highCut = 0;
    Periodicity2Kernel duty(48000,pp);
    DigitalFVParameters fp; fp.conditioning = pp.conditioning;
    DigitalFreqToVolt fv(48000,fp);
    for(int i=0;i<48000;++i) {
        const double x = i%480 < 48 ? 1 : -1;
        duty.process(x); check(fv.process(x));
    }
    assert(duty.output().periodicity > 0.999);
    // Nonfinite input and malformed parameters remain bounded, and silence emits no events.
    DigitalFVParameters bad; bad.smoothTime = -1; bad.referenceMaxFreq = 0;
    bad.referenceMaxEventRate = std::numeric_limits<double>::quiet_NaN();
    bad.conditioning.inputGain = std::numeric_limits<double>::infinity();
    DigitalFreqToVolt robust(0,bad);
    for (int i=0;i<10000;++i) {
        auto r = robust.process(i%2 ? std::numeric_limits<double>::quiet_NaN() :
                               std::numeric_limits<double>::infinity());
        check(r); assert(r.valid == 0 && r.cv == 0);
    }
    // A signal step clears validity after timeout, while a new frequency settles correctly.
    for(int i=0;i<48000;++i) check(fv.process(std::sin(2*pi*440*i/48000)));
    assert(std::abs(fv.internalEventRate()/880-1)<0.03);
    // Reference saturation stays finite and bounded.
    fp.referenceMaxFreq = fp.referenceMaxEventRate = 1;
    DigitalFreqToVolt clipped(48000,fp);
    for(int i=0;i<48000;++i) check(clipped.process(std::sin(2*pi*440*i/48000)));
    assert(clipped.process(1).cv == 1);
    std::cout << "ESP digital tests passed\n";
}
