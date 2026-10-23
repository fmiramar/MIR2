// SPDX-License-Identifier: GPL-3.0-or-later

MIR2EnvelopeAnalyzer : UGen {
    *validate { |windowTime, frameTime|
        if(windowTime.isNumber.not or: { frameTime.isNumber.not }
            or: { frameTime < 0.001 } or: { frameTime > 0.1 }
            or: { windowTime < (2 * frameTime) } or: { windowTime > 10 }) {
            Error("MIR2 envelope: literal frameTime 0.001..0.1 and windowTime 2*frameTime..10 are required.").throw
        }
    }
    *ar { Error("MIR2 envelope analyzers produce control-rate outputs only.").throw }
    checkInputs {
        if(inputs[0].rate != \audio) { ^"MIR2 envelope input must be audio rate." };
        ^this.checkValidInputs
    }
}

TemporalCentroid : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

EnvelopeSpread : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

EnvelopeSkewness : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

EnvelopeKurtosis : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

EffectiveDuration : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

DynamicComplexity : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

StrongDecay : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

TemporalDecrease : MIR2EnvelopeAnalyzer {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, windowTime, frameTime, floorDB)
    }
}

SilenceRate : MIR2EnvelopeAnalyzer {
    *kr { |in, thresholdDB = -60, windowTime = 0.5, frameTime = 0.01|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNew(\control, in, thresholdDB, windowTime, frameTime)
    }
}

FadeDetection : MultiOutUGen {
    *kr { |in, windowTime = 0.5, frameTime = 0.01, slopeThreshold = 3, minFit = 0.6, floorDB = -60|
        MIR2EnvelopeAnalyzer.validate(windowTime, frameTime);
        ^this.multiNewList([\control, in, windowTime, frameTime, slopeThreshold, minFit, floorDB])
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(3, \control) }
    *ar { Error("FadeDetection is control-rate only.").throw }
    checkInputs {
        if(inputs[0].rate != \audio) { ^"FadeDetection requires an audio input." };
        ^this.checkValidInputs
    }
}

AttackTime : MIR2EnvelopeAnalyzer {
    *kr { |in, trigger = 0, maxTime = 0.5, frameTime = 0.005, lowRatio = 0.1, highRatio = 0.9|
        MIR2EnvelopeAnalyzer.validate(maxTime, frameTime);
        ^this.multiNew(\control, in, trigger, maxTime, frameTime, lowRatio, highRatio)
    }
}

AttackSlope2 : MIR2EnvelopeAnalyzer {
    *kr { |in, trigger = 0, maxTime = 0.5, frameTime = 0.005, lowRatio = 0.1, highRatio = 0.9|
        MIR2EnvelopeAnalyzer.validate(maxTime, frameTime);
        ^this.multiNew(\control, in, trigger, maxTime, frameTime, lowRatio, highRatio)
    }
}

DecayTime : MIR2EnvelopeAnalyzer {
    *kr { |in, trigger = 0, maxTime = 0.5, frameTime = 0.005, lowRatio = 0.1, highRatio = 0.9|
        MIR2EnvelopeAnalyzer.validate(maxTime, frameTime);
        ^this.multiNew(\control, in, trigger, maxTime, frameTime, lowRatio, highRatio)
    }
}

LogAttackTime : MultiOutUGen {
    *kr { |in, trigger = 0, maxTime = 0.5, frameTime = 0.005, lowRatio = 0.1, highRatio = 0.9|
        MIR2EnvelopeAnalyzer.validate(maxTime, frameTime);
        ^this.multiNewList([\control, in, trigger, maxTime, frameTime, lowRatio, highRatio])
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, \control) }
    *ar { Error("LogAttackTime is control-rate only.").throw }
    checkInputs {
        if(inputs[0].rate != \audio) { ^"LogAttackTime requires an audio input." };
        ^this.checkValidInputs
    }
}

