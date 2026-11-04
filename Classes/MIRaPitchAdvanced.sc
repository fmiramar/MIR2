// SPDX-License-Identifier: GPL-3.0-or-later
MIRaAdvancedPitch : MultiOutUGen {
    *methodIndex { |method|
        var names = [\yin, \mpm, \probYin, \harmonic, \swipe];
        var index = names.indexOf(method);
        if(index.isNil) { Error("MIRa advanced pitch: unknown method " ++ method).throw };
        ^index
    }
    *build { |outputs, in, minFreq, maxFreq, frameSize, hop, method, parameter = 0,
        capacity = 1, bufnum = -1, binsPerOctave = 60, tracking = 1|
        MIRaPitchAnalyzer.validate(frameSize, hop, minFreq, maxFreq);
        if(in.rate != \audio) { Error("MIRa advanced pitch requires audio-rate input.").throw };
        if(binsPerOctave.isInteger.not or: { binsPerOctave < 12 } or: { binsPerOctave > 96 }) {
            Error("MIRa pitch: binsPerOctave must be a literal integer from 12 through 96.").throw
        };
        ^this.multiNewList([\control, outputs, in, minFreq, maxFreq, frameSize, hop,
            this.methodIndex(method), parameter, capacity, bufnum, binsPerOctave, tracking])
    }
    *ar { Error("MIRa pitch analyzers return control-rate features.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
    *latency { |sampleRate = 48000, frameSize = 4096| ^frameSize / sampleRate }
}

FundamentalCandidates : MIRaAdvancedPitch {
    *kr { |in, maxCandidates = 5, minFreq = 55, maxFreq = 1760, frameSize = 4096,
        hop = 0.25, method = \probYin, binsPerOctave = 60|
        var outputs;
        if(maxCandidates.isInteger.not or: { maxCandidates < 1 } or: { maxCandidates > 16 }) {
            Error("FundamentalCandidates: maxCandidates must be an integer from 1 through 16.").throw
        };
        outputs = this.build(maxCandidates * 2, in, minFreq, maxFreq, frameSize, hop,
            method, 0, maxCandidates, -1, binsPerOctave);
        ^[outputs.copyRange(0, maxCandidates - 1), outputs.copyRange(maxCandidates, (maxCandidates * 2) - 1)]
    }
}
PitchProb : MIRaAdvancedPitch {
    *kr { |in, minFreq = 55, maxFreq = 1760, frameSize = 4096, hop = 0.25,
        numThresholds = 32, tracking = 1|
        if(numThresholds.isInteger.not or: { numThresholds < 4 } or: { numThresholds > 128 }) {
            Error("PitchProb: numThresholds must be a literal integer from 4 through 128.").throw
        };
        ^this.build(4, in, minFreq, maxFreq, frameSize, hop, \probYin, numThresholds,
            16, -1, 60, tracking)
    }
}
PitchAmbiguity : MIRaAdvancedPitch {
    *kr { |in, minFreq = 55, maxFreq = 1760, frameSize = 4096, hop = 0.25,
        method = \probYin, binsPerOctave = 60|
        ^this.build(3, in, minFreq, maxFreq, frameSize, hop, method, 0, 16, -1, binsPerOctave)
    }
}
PitchSWIPE : MIRaAdvancedPitch {
    *kr { |in, minFreq = 55, maxFreq = 1760, frameSize = 8192, hop = 0.125,
        strengthThreshold = 0.2, binsPerOctave = 60|
        ^this.build(4, in, minFreq, maxFreq, frameSize, hop, \swipe, strengthThreshold,
            16, -1, binsPerOctave)
    }
}
PitchConsensus : MIRaAdvancedPitch {
    *kr { |in, minFreq = 55, maxFreq = 1760, frameSize = 8192, hop = 0.125,
        methods = #[\yin, \mpm, \swipe], binsPerOctave = 60|
        var mask = 0;
        methods.do { |method|
            var index = [\yin, \mpm, \swipe].indexOf(method);
            if(index.isNil) { Error("PitchConsensus: methods must contain yin, mpm, or swipe.").throw };
            mask = mask.bitOr(1 << index)
        };
        if(mask == 0) { Error("PitchConsensus: choose at least one method.").throw };
        ^this.build(4, in, minFreq, maxFreq, frameSize, hop, \swipe, mask, 16, -1, binsPerOctave)
    }
}
PitchSalienceBuf : MIRaAdvancedPitch {
    *kr { |in, bufnum, minFreq = 55, maxFreq = 1760, binsPerOctave = 60,
        frameSize = 8192, hop = 0.125, method = \swipe|
        if([\swipe, \harmonic].includes(method).not) {
            Error("PitchSalienceBuf: method must be swipe or harmonic.").throw
        };
        ^this.build(2, in, minFreq, maxFreq, frameSize, hop, method, 0, 16, bufnum, binsPerOctave)
    }
    *numBins { |minFreq = 55, maxFreq = 1760, binsPerOctave = 60|
        ^(((maxFreq / minFreq).log2 * binsPerOctave).floor.asInteger + 1).clip(2, 1024)
    }
}
PitchSalience : Object {
    *kr { |in, bufnum, minFreq = 55, maxFreq = 1760, binsPerOctave = 60,
        frameSize = 8192, hop = 0.125, method = \swipe|
        ^PitchSalienceBuf.kr(in, bufnum, minFreq, maxFreq, binsPerOctave, frameSize, hop, method)
    }
    *numBins { |minFreq = 55, maxFreq = 1760, binsPerOctave = 60|
        ^PitchSalienceBuf.numBins(minFreq, maxFreq, binsPerOctave)
    }
}
MultiPitch : MIRaAdvancedPitch {
    *kr { |in, maxPolyphony = 4, minFreq = 55, maxFreq = 1760, frameSize = 4096,
        hop = 0.25, method = \klapuri, threshold = 0.15, binsPerOctave = 60|
        var outputs;
        if(maxPolyphony.isInteger.not or: { maxPolyphony < 1 } or: { maxPolyphony > 6 }) {
            Error("MultiPitch: maxPolyphony must be a literal integer from 1 through 6.").throw
        };
        if(method != \klapuri) { Error("MultiPitch: available method is klapuri.").throw };
        outputs = this.build((maxPolyphony * 2) + 1, in, minFreq, maxFreq, frameSize, hop,
            \harmonic, threshold, maxPolyphony, -1, binsPerOctave);
        ^[outputs.copyRange(0, maxPolyphony - 1),
            outputs.copyRange(maxPolyphony, (maxPolyphony * 2) - 1), outputs.last]
    }
}
PredominantPitch : MIRaAdvancedPitch {
    *kr { |in, minFreq = 110, maxFreq = 1760, frameSize = 4096, hop = 0.125,
        minDuration = 0.06, binsPerOctave = 60|
        ^this.build(3, in, minFreq, maxFreq, frameSize, hop, \harmonic,
            minDuration, 16, -1, binsPerOctave)
    }
}
PitchContour : MIRaAdvancedPitch {
    *kr { |candidateFreqs, candidateProbabilities, maxJump = 600, transitionWeight = 1,
        octavePenalty = 1, unvoicedPenalty = 2, trig = 1, lag = 0|
        var frequencies = candidateFreqs.asArray, probabilities = candidateProbabilities.asArray;
        if(frequencies.size < 1 or: { frequencies.size > 16 } or: { probabilities.size != frequencies.size }) {
            Error("PitchContour requires equally sized arrays containing 1 through 16 values.").throw
        };
        if(lag.isInteger.not or: { lag < 0 } or: { lag > 31 }) {
            Error("PitchContour: lag must be a literal integer from 0 through 31.").throw
        };
        ^this.multiNewList([\control, 3, frequencies.size, maxJump, transitionWeight,
            octavePenalty, unvoicedPenalty, trig, lag] ++ frequencies ++ probabilities)
    }
}
PitchStability : MIRaAdvancedPitch {
    *kr { |freq, confidence = 1, windowTime = 0.5|
        if(windowTime.isNumber.not or: { windowTime < 0.05 } or: { windowTime > 5 }) {
            Error("PitchStability: windowTime must be a literal number from 0.05 through 5 seconds.").throw
        };
        ^this.multiNewList([\control, 2, freq, confidence, windowTime])
    }
}
PitchDeviation : UGen {
    *kr { |freq, reference = 440, mode = \nearest12TET|
        var index = [\reference, \nearest12TET, \previous].indexOf(mode);
        if(index.isNil) { Error("PitchDeviation: mode must be reference, nearest12TET, or previous.").throw };
        ^this.multiNew(\control, freq, reference, index)
    }
    *ar { Error("PitchDeviation produces control-rate output.").throw }
}
Vibrato2 : MIRaAdvancedPitch {
    *kr { |freq, pitchConfidence = 1, minRate = 3, maxRate = 12, minExtent = 5, windowTime = 1|
        if(windowTime.isNumber.not or: { windowTime < 0.1 } or: { windowTime > 5 }) {
            Error("Vibrato2: windowTime must be a literal number from 0.1 through 5 seconds.").throw
        };
        ^this.multiNewList([\control, 4, freq, pitchConfidence, minRate, maxRate, minExtent, windowTime])
    }
}
