// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/MS20ESP.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace mir2::esp;
int main() {
 for(double sr:{44100.,48000.,96000.}) {
  EnvelopeKernel e(sr,Mode::circuit);
  const double target=(5-0.6)*22/23;
  for(int i=0;i<static_cast<int>(sr/10);++i) {
   double out=e.process(5);
   double expected=target*(1-std::exp(-(i+1)/(sr*(0.001*22/23))));
   assert(std::abs(out*5-expected)<1e-10);
  }
  double initial=e.internalVolts();
  for(int i=0;i<static_cast<int>(sr/10);++i) {
   e.process(0);assert(std::abs(e.internalVolts()-initial*std::exp(-(i+1)/(sr*0.022)))<1e-10);
  }
  // Force a diode-off/on transition exactly halfway through a sample.
  const double start=e.internalVolts(),drive=start*std::exp(-0.5/(sr*0.022));
  e.process(drive+0.6);
  const double onTarget=drive*22/23;
  const double expectedSwitch=onTarget+(drive-onTarget)*std::exp(-0.5/(sr*(0.001*22/23)));
  assert(std::abs(e.internalVolts()-expectedSwitch)<1e-12);
  // Falling input crosses from diode-off decay to conduction within a sample.
  e.process(1);assert(std::isfinite(e.internalVolts()));
  EnvelopeKernel negative(sr,Mode::circuit);
  for(int i=0;i<1000;++i) assert(negative.process(-5)==0);
  EnvelopeKernel digital(sr,Mode::digital);
  for(int i=0;i<static_cast<int>(sr/10);++i) digital.process(-5);
  assert(std::abs(digital.internalVolts()-5)<1e-7);
 }
 TriggerKernel t;
 auto o=t.process(0);assert(o.trigger==0&&o.gate==0&&o.hardwareGate==1);
 o=t.process(0.5);assert(o.trigger==1&&o.gate==1&&o.hardwareGate==0);
 o=t.process(0.4);assert(o.trigger==0&&o.gate==1);
 o=t.process(0.38);assert(o.gate==0&&o.hardwareGate==1);
 o=t.process(0.5);assert(o.trigger==1);
 o=t.process(std::numeric_limits<double>::quiet_NaN());assert(o.gate==0);
 TriggerKernel zero(0,0);zero.process(1);assert(zero.process(0).gate==0);
 EnvelopeKernel safety(48000,Mode::circuit);
 for(double x:{1e300,-1e300,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
  const double y=safety.process(x);assert(std::isfinite(y)&&y>=0&&y<=1);
 }
 std::cout<<"ESP envelope/trigger tests passed\n";
}
