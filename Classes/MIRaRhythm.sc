// SPDX-License-Identifier: GPL-3.0-or-later
MIRaRhythmAnalyzer : MultiOutUGen {
    *validate { |minBPM, maxBPM, numBins, windowTime, hopTime, featureRate, warmup = 0|
        if(minBPM.isNumber.not or: { maxBPM.isNumber.not }
            or: { minBPM < 20 } or: { maxBPM > 600 } or: { maxBPM <= minBPM }
            or: { numBins.isInteger.not } or: { numBins < 8 } or: { numBins > 512 }
            or: { windowTime.isNumber.not } or: { windowTime < 0.5 } or: { windowTime > 16 }
            or: { hopTime.isNumber.not } or: { hopTime < 0.02 } or: { hopTime > 2 }
            or: { featureRate.isNumber.not } or: { featureRate < 20 } or: { featureRate > 200 }
            or: { warmup.isNumber.not } or: { warmup < 0 } or: { warmup > windowTime }) {
            Error("MIRa rhythm: invalid literal BPM range, dimensions, feature rate, or window/hop/warmup times.").throw
        }
    }
    *build { |outputs, in, minBPM, maxBPM, numBins, windowTime, hopTime, featureRate,
        normalize = 1, priorBPM = 120, continuity = 3, tempoSmooth = 0.4,
        phaseSmooth = 0.35, lock = 0, warmup = 0, decay = 2|
        this.validate(minBPM, maxBPM, numBins, windowTime, hopTime, featureRate, warmup);
        ^this.multiNewList([\control, outputs, in, minBPM, maxBPM, numBins, windowTime,
            hopTime, featureRate, normalize, priorBPM, continuity, tempoSmooth,
            phaseSmooth, lock, warmup, decay])
    }
    *kr { Error("MIRaRhythmAnalyzer is abstract.").throw }
    *ar { Error("MIRa rhythm analyzers produce control-rate outputs only.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs {
        if(inputs[0].rate == \audio) { ^"MIRa rhythm strength inputs must be control-rate or scalar." };
        ^this.checkValidInputs
    }
}

Tempogram : MIRaRhythmAnalyzer {
    *kr { |in, minBPM = 30, maxBPM = 300, numBins = 128, windowTime = 6,
        hopTime = 0.1, featureRate = 100, normalize = 1|
        ^this.build(numBins, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, normalize, warmup: 0)
    }
    *bpms { |minBPM = 30, maxBPM = 300, numBins = 128|
        ^Array.interpolation(numBins, minBPM, maxBPM)
    }
}

FourierTempogram : MIRaRhythmAnalyzer {
    *kr { |in, minBPM = 30, maxBPM = 300, numBins = 128, windowTime = 6,
        hopTime = 0.1, featureRate = 100, normalize = 1|
        ^this.build(numBins, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, normalize, warmup: 0)
    }
    *bpms { |minBPM = 30, maxBPM = 300, numBins = 128|
        ^Array.interpolation(numBins, minBPM, maxBPM)
    }
}

BeatHistogram : MIRaRhythmAnalyzer {
    *kr { |in, minBPM = 30, maxBPM = 300, numBins = 128, windowTime = 6,
        hopTime = 0.1, featureRate = 100, normalize = 1, decay = 2|
        ^this.build(numBins, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, normalize, warmup: 0, decay: decay)
    }
    *bpms { |minBPM = 30, maxBPM = 300, numBins = 128|
        ^Array.interpolation(numBins, minBPM, maxBPM)
    }
}

Tempo : MIRaRhythmAnalyzer {
    *kr { |in, minBPM = 30, maxBPM = 300, windowTime = 6, hopTime = 0.1,
        priorBPM = 120, continuity = 3, tempoSmooth = 0.4, warmup = 2,
        featureRate = 100, numBins = 271|
        ^this.build(2, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, priorBPM: priorBPM, continuity: continuity,
            tempoSmooth: tempoSmooth, warmup: warmup)
    }
}

BeatStatistics2 : MIRaRhythmAnalyzer {
    *kr { |in, minBPM = 30, maxBPM = 300, windowTime = 6, hopTime = 0.1,
        priorBPM = 120, continuity = 3, tempoSmooth = 0.4, warmup = 2,
        featureRate = 100, numBins = 271|
        ^this.build(5, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, priorBPM: priorBPM, continuity: continuity,
            tempoSmooth: tempoSmooth, warmup: warmup)
    }
}

PulseClarity : MIRaRhythmAnalyzer {
    *kr { |in, minBPM = 30, maxBPM = 300, windowTime = 6, hopTime = 0.1,
        priorBPM = 120, continuity = 3, tempoSmooth = 0.4, warmup = 2,
        featureRate = 100, numBins = 271|
        ^this.build(1, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, priorBPM: priorBPM, continuity: continuity,
            tempoSmooth: tempoSmooth, warmup: warmup)
    }
}

BeatTrack3 : MIRaRhythmAnalyzer {
    *kr { |chain, minBPM = 30, maxBPM = 300, tempoSmooth = 0.4, phaseSmooth = 0.35,
        lock = 0, windowTime = 6, hopTime = 0.1, warmup = 2, priorBPM = 120,
        continuity = 3, fftSize = 2048, hop = 0.25, method = \superflux,
        featureRate = 100, numBins = 271|
        var onset = Onsets2.kr(chain, method: method, fftSize: fftSize, hop: hop);
        ^this.krOnsets(onset[1], minBPM, maxBPM, tempoSmooth, phaseSmooth, lock,
            windowTime, hopTime, warmup, priorBPM, continuity, featureRate, numBins)
    }
    *krOnsets { |in, minBPM = 30, maxBPM = 300, tempoSmooth = 0.4, phaseSmooth = 0.35,
        lock = 0, windowTime = 6, hopTime = 0.1, warmup = 2, priorBPM = 120,
        continuity = 3, featureRate = 100, numBins = 271|
        ^this.build(5, in, minBPM, maxBPM, numBins, windowTime, hopTime,
            featureRate, priorBPM: priorBPM, continuity: continuity,
            tempoSmooth: tempoSmooth, phaseSmooth: phaseSmooth, lock: lock, warmup: warmup)
    }
}

AutoTrack2 : BeatTrack3 { }

OnsetStatistics2 : MultiOutUGen {
    *kr { |trig, windowTime = 4, minGap = 0|
        if(windowTime.isNumber.not or: { windowTime < 0.05 } or: { windowTime > 60 }) {
            Error("OnsetStatistics2: windowTime must be a literal number from 0.05 through 60 seconds.").throw
        };
        ^this.multiNewList([\control, 4, trig, windowTime, minGap])
    }
    *ar { Error("OnsetStatistics2 is control-rate only.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs {
        if(inputs[0].rate == \audio) { ^"OnsetStatistics2 requires a control-rate trigger." };
        ^this.checkValidInputs
    }
}

OnsetRate : UGen {
    *kr { |trig, windowTime = 4, minGap = 0|
        if(windowTime.isNumber.not or: { windowTime < 0.05 } or: { windowTime > 60 }) {
            Error("OnsetRate: windowTime must be a literal number from 0.05 through 60 seconds.").throw
        };
        ^this.multiNew(\control, trig, windowTime, minGap)
    }
    *ar { Error("OnsetRate is control-rate only.").throw }
    checkInputs {
        if(inputs[0].rate == \audio) { ^"OnsetRate requires a control-rate trigger." };
        ^this.checkValidInputs
    }
}

TempoStability : UGen {
    *kr { |bpm, confidence = 1, windowTime = 4, minConfidence = 0.1|
        if(windowTime.isNumber.not or: { windowTime < 0.05 } or: { windowTime > 60 }) {
            Error("TempoStability: windowTime must be a literal number from 0.05 through 60 seconds.").throw
        };
        ^this.multiNew(\control, bpm, confidence, windowTime, minConfidence)
    }
    *ar { Error("TempoStability is control-rate only.").throw }
    checkInputs {
        if(inputs[0].rate == \audio or: { inputs[1].rate == \audio }) { ^"TempoStability requires control-rate tempo and confidence." };
        ^this.checkValidInputs
    }
}

TempoRubato : UGen {
    *kr { |bpm, confidence = 1, smoothing = 0.3, minConfidence = 0.1|
        ^this.multiNew(\control, bpm, confidence, smoothing, minConfidence)
    }
    *ar { Error("TempoRubato is control-rate only.").throw }
    checkInputs {
        if(inputs[0].rate == \audio or: { inputs[1].rate == \audio }) { ^"TempoRubato requires control-rate tempo and confidence." };
        ^this.checkValidInputs
    }
}

BeatLoudness : MultiOutUGen {
    *kr { |in, beat, maxInterval = 10|
        ^this.multiNewList([\control, 2, in, beat, maxInterval])
    }
    *ar { Error("BeatLoudness produces control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs {
        if(inputs[0].rate != \audio or: { inputs[1].rate == \audio }) { ^"BeatLoudness requires audio input and a control-rate beat trigger." };
        ^this.checkValidInputs
    }
}

BeatBandRatio : MultiOutUGen {
    *kr { |chain, beat, boundaries = #[0, 200, 800, 3200, 20000], fftSize = 1024, maxInterval = 10|
        var numBands = boundaries.size - 1;
        MIRaSpectralBands.validate(boundaries, fftSize, 1);
        ^this.multiNewList([\control, numBands, chain, beat, numBands, fftSize, maxInterval] ++ boundaries)
    }
    *ar { Error("BeatBandRatio produces control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs {
        if(inputs[1].rate == \audio) { ^"BeatBandRatio requires a control-rate beat trigger." };
        ^this.checkValidInputs
    }
}
