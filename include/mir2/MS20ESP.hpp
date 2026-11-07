// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace mir2 { namespace esp {
inline double finite(double x, double fallback = 0.0) noexcept {
    return std::isfinite(x) ? x : fallback;
}
inline double bounded(double x) noexcept { return std::clamp(std::isnan(x) ? 0.0 : x, 0.0, 1.0); }
inline double positive(double x, double fallback) noexcept {
    return std::isfinite(x) && x > 0.0 ? x : fallback;
}
struct Conditioning {
    double lowCut = 80.0, highCut = 2000.0, inputGain = 1.0;
    double threshold = 0.0, hysteresis = 0.01;
};
// Digital one-pole filters. A zero cutoff bypasses the corresponding stage.
class Crossing {
    double dt_, hpA_, lpA_, gain_, threshold_, hysteresis_;
    double low_ = 0, band_ = 0, previous_ = 0;
    bool hp_, lp_, initialized_ = false, high_ = false;
public:
    struct Event { int direction = 0; double offset = 0; };
    Crossing(double sampleRate, Conditioning p = {}) noexcept {
        const double sr = positive(sampleRate, 48000);
        dt_ = 1 / sr;
        hp_ = finite(p.lowCut) > 0; lp_ = finite(p.highCut) > 0;
        hpA_ = std::exp(-2 * 3.14159265358979323846 * std::clamp(finite(p.lowCut), 0.0, sr * 0.45) * dt_);
        lpA_ = std::exp(-2 * 3.14159265358979323846 * std::clamp(finite(p.highCut), 0.0, sr * 0.45) * dt_);
        gain_ = std::clamp(finite(p.inputGain, 1), -1e6, 1e6);
        threshold_ = std::clamp(finite(p.threshold), -1e6, 1e6);
        hysteresis_ = std::clamp(finite(p.hysteresis), 0.0, 1e6);
    }
    Event process(double x) noexcept {
        x = std::clamp(finite(x), -1e6, 1e6) * gain_;
        low_ = hpA_ * low_ + (1-hpA_) * x;
        const double highpass = hp_ ? x-low_ : x;
        band_ = lpA_*band_ + (1-lpA_)*highpass;
        const double y = lp_ ? band_ : highpass;
        if (!initialized_) {
            initialized_ = true; high_ = y >= threshold_; previous_ = y; return {};
        }
        const double level = threshold_ + (high_ ? -hysteresis_ : hysteresis_);
        Event e;
        if ((!high_ && y > level) || (high_ && y < level)) {
            e.direction = high_ ? -1 : 1;
            e.offset = dt_ * std::clamp((level-previous_)/(y-previous_), 0.0, 1.0);
            high_ = !high_;
        }
        previous_ = y;
        return e;
    }
};
struct DigitalFVParameters {
    Conditioning conditioning;
    double smoothTime = 0.02, referenceMaxFreq = 2000, referenceMaxEventRate = 4000;
    double cvAdjust = 1, timeout = 0.1;
};
struct FVOutput { double cv = 0, frequency = 0, eventRate = 0, valid = 0; };
class DigitalFreqToVolt {
    Crossing crossing_;
    double dt_, tau_, decay_, maxFreq_, maxRate_, gain_, timeout_;
    double time_ = 0, last_ = -1, rate_ = 0;
    unsigned events_ = 0;
public:
    DigitalFreqToVolt(double sr, DigitalFVParameters p = {}) noexcept
        : crossing_(sr, p.conditioning) {
        dt_ = 1/positive(sr,48000); tau_ = std::max(positive(p.smoothTime,0.02), dt_);
        decay_ = std::exp(-dt_/tau_);
        maxFreq_ = positive(p.referenceMaxFreq,2000); maxRate_ = positive(p.referenceMaxEventRate,4000);
        gain_ = std::clamp(finite(p.cvAdjust,1),0.0,1e6); timeout_ = positive(p.timeout,0.1);
    }
    FVOutput process(double x) noexcept {
        const auto event = crossing_.process(x);
        rate_ *= decay_;
        if (last_ >= 0 && time_ + dt_ - last_ > timeout_) { events_ = 0; last_ = -1; }
        if (event.direction) {
            last_ = time_ + event.offset; events_ = std::min(events_+1,3u);
            // Unit-area event impulse, with exact exponential fractional-sample decay.
            rate_ += std::exp(-(dt_-event.offset)/tau_)/tau_;
        }
        time_ += dt_;
        const double indicated = rate_*0.5*gain_;
        return { bounded(indicated/maxFreq_), bounded(indicated/maxFreq_),
                 bounded(rate_/maxRate_), events_ >= 3 && gain_ > 0 ? 1.0 : 0.0 };
    }
    double internalEventRate() const noexcept { return rate_; }
    double internalVolts() const noexcept { return 8.4*rate_*0.5*gain_/maxFreq_; }
};
// Bilinear discretization of (n2*s^2+n1*s+n0)/(d2*s^2+d1*s+1).
// No prewarping: tests compare at the bilinear-mapped analog frequency.
class CircuitSection {
    double b0_=1,b1_=0,b2_=0,a1_=0,a2_=0,z1_=0,z2_=0;
public:
    void configure(double sr, double n2, double n1, double n0,
                   double d2, double d1) noexcept {
        const double k = 2*positive(sr,48000), norm = d2*k*k+d1*k+1;
        b0_=(n2*k*k+n1*k+n0)/norm; b1_=(2*n0-2*n2*k*k)/norm;
        b2_=(n2*k*k-n1*k+n0)/norm;
        a1_=(2-2*d2*k*k)/norm; a2_=(d2*k*k-d1*k+1)/norm;
        z1_=z2_=0;
    }
    void lowpass(double sr, double ra, double rb, double cf, double cg) noexcept {
        configure(sr,0,0,1,ra*rb*cf*cg,cg*(ra+rb));
    }
    double process(double x) noexcept {
        const double y = b0_*x+z1_;
        z1_ = b1_*x-a1_*y+z2_; z2_=b2_*x-a2_*y;
        return y;
    }
};
inline double lowCutResistance(double hz) noexcept {
    constexpr double c=33e-9, rs=680000.0/1680.0, pi=3.14159265358979323846;
    const double w=2*pi*std::clamp(positive(hz,80),1.0,20000.0);
    return std::clamp((std::sqrt(rs*rs+4/(c*c*w*w))-rs)/2,2000.0,102000.0);
}
class CircuitFilters {
    CircuitSection hp_,lp1_,lp2_;
public:
    CircuitFilters(double sr, double lowCut, double highCut) noexcept {
        constexpr double c=33e-9, rs=680000.0/1680.0, k=1000.0/1680.0;
        const double r=lowCutResistance(lowCut);
        hp_.configure(sr,k*c*c*r*r,0,0,c*c*r*(r+rs),2*c*r+c*rs);
        const double lpR=std::clamp(1/(2*3.14159265358979323846*
            std::clamp(positive(highCut,2000),1.0,20000.0)*15e-9),2000.0,102000.0);
        lp1_.lowpass(sr,lpR,lpR,15e-9,15e-9);
        lp2_.lowpass(sr,lpR,lpR,150e-9,15e-9);
    }
    double process(double x) noexcept { return lp2_.process(lp1_.process(hp_.process(x))); }
};
// Fractional sample coverage of a retriggerable bounded pulse; units are seconds.
class CircuitPulse {
    double remaining_=0;
public:
    struct Result { double coverage; bool overlap; };
    Result process(double dt, bool event, double offset, double width) noexcept {
        const double old=std::min(remaining_,dt);
        double covered=old;
        bool overlap=false;
        double next=std::max(0.0,remaining_-dt);
        if(event) {
            offset=std::clamp(offset,0.0,dt);
            const double end=offset+width;
            covered += std::max(0.0,std::min(dt,end)-offset);
            covered -= std::max(0.0,std::min(old,end)-offset);
            overlap=remaining_>offset;
            next=std::max(next,end-dt);
        }
        remaining_=std::max(0.0,next);
        return {bounded(covered/dt),overlap};
    }
};
struct CircuitFVParameters {
    Conditioning conditioning;
    double pulseWidth=0.0002, referenceMaxFreq=2000, referenceMaxEventRate=4000;
    double cvAdjust=1, timeout=0.1;
};
class CircuitFreqToVolt {
    CircuitFilters filters_;
    Crossing crossing_;
    CircuitPulse pulse_;
    CircuitSection output_;
    double dt_,width_,maxFreq_,maxRate_,gain_,timeout_,decay_,calibration_;
    double time_=0,last_=-1,lastOverlap_=-1,rate_=0,volts_=0;
    unsigned events_=0;
    static Conditioning comparator(Conditioning p) noexcept {
        p.lowCut=p.highCut=0; p.inputGain=1;
        // Symmetric ideal comparator swing assumption, NOT measured LM339 levels.
        p.hysteresis=std::max(std::max(0.0,finite(p.hysteresis)),15.0/221.0);
        return p;
    }
    double inputGain_;
public:
    CircuitFreqToVolt(double sr, CircuitFVParameters p={}) noexcept
        : filters_(sr,p.conditioning.lowCut,p.conditioning.highCut),
          crossing_(sr,comparator(p.conditioning)) {
        dt_=1/positive(sr,48000); width_=std::clamp(positive(p.pulseWidth,0.0002),1e-7,1.0);
        maxFreq_=positive(p.referenceMaxFreq,2000); maxRate_=positive(p.referenceMaxEventRate,4000);
        gain_=std::clamp(finite(p.cvAdjust,1),0.0,1e6); timeout_=positive(p.timeout,0.1);
        inputGain_=std::clamp(finite(p.conditioning.inputGain,1),-1e6,1e6);
        decay_=std::exp(-dt_/0.02);
        calibration_=8.4/(2*width_*maxFreq_);
        const double r=lowCutResistance(p.conditioning.lowCut)+8000;
        output_.lowpass(sr,r,r,56e-9,56e-9);
    }
    FVOutput process(double x) noexcept {
        // Input is calibrated at the amplified branch: 1 audio unit = 1 model volt.
        const double input=std::clamp(std::clamp(finite(x),-1e6,1e6)*inputGain_,-13.5,13.5);
        const auto e=crossing_.process(filters_.process(input));
        rate_*=decay_;
        if(last_>=0 && time_+dt_-last_>timeout_) { events_=0; last_=-1; }
        if(e.direction) {
            last_=time_+e.offset; events_=std::min(events_+1,3u);
            rate_+=std::exp(-(dt_-e.offset)/0.02)/0.02;
        }
        const auto pulse=pulse_.process(dt_,e.direction!=0,e.offset,width_);
        if(pulse.overlap) lastOverlap_=time_+e.offset;
        const double raw=output_.process(pulse.coverage)*calibration_*gain_;
        volts_=std::clamp(finite(raw),0.0,13.5);
        time_+=dt_;
        const bool inverseValid=events_>=3 && gain_>0 && raw>=0 && raw<8.4 &&
            (lastOverlap_<0 || time_-lastOverlap_>timeout_);
        return {bounded(volts_/8.4),bounded(volts_/8.4),bounded(rate_/maxRate_),inverseValid?1.0:0.0};
    }
    double internalVolts() const noexcept { return volts_; }
    double internalEventRate() const noexcept { return rate_; }
};
enum class Mode { digital, circuit };
// One unit-facing F/V family; mode is fixed at construction, only the selected path runs.
class FreqToVoltKernel {
    Mode mode_;
    DigitalFreqToVolt digital_;
    CircuitFreqToVolt circuit_;
public:
    FreqToVoltKernel(double sr, Mode mode, DigitalFVParameters digital={},
                    CircuitFVParameters circuit={}) noexcept
        : mode_(mode),digital_(sr,digital),circuit_(sr,circuit) {}
    FVOutput process(double x) noexcept {
        return mode_==Mode::circuit ? circuit_.process(x) : digital_.process(x);
    }
};
struct PeriodicityParameters {
    Conditioning conditioning;
    double windowTime = 0.25, timeout = 0.1, jitterScale = 0.15;
    unsigned minIntervals = 4;
};
struct TimingOutput { double periodicity = 0, jitter = 0, eventConfidence = 0; };
class EventRegularity {
    struct Interval { double duration = 0, end = 0; };
    std::array<Interval,128> ring_ {};
    std::size_t head_ = 0, count_ = 0;
    double last_ = -1, window_, timeout_, scale_;
    unsigned minimum_;
    void clear() noexcept { head_ = count_ = 0; last_ = -1; }
public:
    explicit EventRegularity(PeriodicityParameters p = {}) noexcept
        : window_(positive(p.windowTime,0.25)), timeout_(positive(p.timeout,0.1)),
          scale_(positive(p.jitterScale,0.15)), minimum_(std::clamp(p.minIntervals,2u,64u)) {}
    // Timestamp API also supports deterministic offline event-level validation.
    void event(double time) noexcept {
        if (!std::isfinite(time) || time < 0) return;
        if (last_ >= 0 && time <= last_) return;
        if (last_ >= 0 && time-last_ > timeout_) clear();
        if (last_ >= 0) {
            ring_[head_] = {time-last_,time}; head_ = (head_+1)%ring_.size();
            count_ = std::min(count_+1,ring_.size());
        }
        last_ = time;
    }
    TimingOutput output(double time) noexcept {
        if (!std::isfinite(time) || last_ < 0 || time < last_) return {};
        if (time-last_ >= timeout_) { clear(); return {}; }
        // Drop intervals whose start lies outside the supported window.
        while (count_) {
            const auto& oldest = ring_[(head_+ring_.size()-count_)%ring_.size()];
            if (oldest.end-oldest.duration >= time-window_) break;
            --count_;
        }
        if (count_ < minimum_) return {};
        double mean = 0, m2 = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            const double v = ring_[(head_+ring_.size()-count_+i)%ring_.size()].duration;
            const double delta = v-mean;
            mean += delta/static_cast<double>(i+1); m2 += delta*(v-mean);
        }
        const double relative = std::sqrt(std::max(0.0,m2/count_))/mean;
        // Freshness remains full for one expected interval, then falls to zero at timeout.
        const double grace = std::min(mean,timeout_*0.5);
        const double freshness = bounded((timeout_-(time-last_))/(timeout_-grace));
        const double confidence = bounded(static_cast<double>(count_)/(2*minimum_))*freshness;
        return { bounded(confidence*std::exp(-relative/scale_)), bounded(relative/scale_), confidence };
    }
};
class Periodicity2Kernel {
    Crossing crossing_;
    EventRegularity regularity_;
    double dt_, time_ = 0;
public:
    Periodicity2Kernel(double sr, PeriodicityParameters p = {}) noexcept
        : crossing_(sr,p.conditioning), regularity_(p), dt_(1/positive(sr,48000)) {}
    void process(double x) noexcept {
        const auto e = crossing_.process(x);
        if (e.direction > 0) regularity_.event(time_+e.offset);
        time_ += dt_;
    }
    // Compute statistics at the requested output rate, not on every audio sample.
    TimingOutput output() noexcept { return regularity_.output(time_); }
};
struct EnvelopeParameters {
    double inputGain=1, attackTime=0.005, releaseTime=0.08, referenceEnvVolts=5;
};
class EnvelopeKernel {
    Mode mode_;
    double dt_,gain_,attack_,release_,reference_,value_=0;
public:
    EnvelopeKernel(double sr,Mode mode,EnvelopeParameters p={}) noexcept
        : mode_(mode),dt_(1/positive(sr,48000)),
          gain_(std::clamp(finite(p.inputGain,1),-1e6,1e6)),
          attack_(positive(p.attackTime,0.005)),release_(positive(p.releaseTime,0.08)),
          reference_(positive(p.referenceEnvVolts,5)) {}
    double process(double x) noexcept {
        x=std::clamp(std::clamp(finite(x),-1e6,1e6)*gain_,-13.5,13.5);
        if(mode_==Mode::digital) {
            const double target=std::abs(x),tau=target>value_?attack_:release_;
            value_=target+(value_-target)*std::exp(-dt_/tau);
        } else {
            // Exact piecewise solution for a held input and a fixed 0.6 V diode drop.
            constexpr double offTau=0.022,onTau=0.001*22/23;
            const double drive=std::max(0.0,x-0.6);
            double remaining=dt_;
            if(value_>drive) {
                const double crossing=drive>0?offTau*std::log(value_/drive):dt_;
                const double offTime=std::min(dt_,crossing);
                value_*=std::exp(-offTime/offTau);remaining-=offTime;
            }
            if(remaining>0) {
                const double target=drive*22/23;
                value_=target+(value_-target)*std::exp(-remaining/onTau);
            }
        }
        return bounded(value_/reference_);
    }
    double internalVolts() const noexcept { return value_; }
};
struct TriggerOutput { double trigger=0,gate=0,hardwareGate=1; };
class TriggerKernel {
    double threshold_,hysteresis_;
    bool active_=false;
public:
    TriggerKernel(double threshold=0.4,double hysteresis=0.01) noexcept
        : threshold_(bounded(threshold)),hysteresis_(bounded(hysteresis)) {}
    TriggerOutput process(double envelope) noexcept {
        const bool before=active_;
        const double x=bounded(envelope);
        if(!active_ && x>std::min(1.0,threshold_+hysteresis_)) active_=true;
        else if(active_ && x<=std::max(0.0,threshold_-hysteresis_)) active_=false;
        return {active_&&!before?1.0:0.0,active_?1.0:0.0,active_?0.0:1.0};
    }
};
}} // namespace mir2::esp
