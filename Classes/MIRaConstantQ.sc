// SPDX-License-Identifier: GPL-3.0-or-later
MIRaConstantQ : MultiOutUGen {
    *validate { |in, fmin, numBins, binsPerOctave, gamma, hop, sparsity|
        if(in.rate != \audio or: { fmin.isNumber.not } or: { fmin <= 0 }
            or: { numBins.isInteger.not } or: { numBins < 2 } or: { numBins > 192 }
            or: { binsPerOctave.isInteger.not } or: { binsPerOctave < 6 } or: { binsPerOctave > 48 }
            or: { gamma.isNumber.not } or: { gamma < 0 }
            or: { hop.isNumber.not } or: { hop < 0.0625 } or: { hop > 1 }
            or: { sparsity.isNumber.not } or: { sparsity < 1e-8 } or: { sparsity > 0.01 }) {
            Error("MIRa constant-Q: invalid audio input or literal transform settings.").throw
        }
    }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
    *ar { Error("MIRa constant-Q analyzers produce control-rate outputs only.").throw }
    *frameSize { |sampleRate = 48000, fmin = 55, binsPerOctave = 12, gamma = 0|
        ^(sampleRate / (((2 ** (1 / binsPerOctave)) - 1) * fmin + gamma)).ceil.max(32).asInteger.nextPowerOfTwo
    }
}
CQT : MIRaConstantQ {
    *kr { |in, fmin = 55, numBins = 84, binsPerOctave = 12, hop = 0.25, sparsity = 1e-4|
        this.validate(in, fmin, numBins, binsPerOctave, 0, hop, sparsity);
        ^this.multiNewList([\control, numBins, in, fmin, numBins, binsPerOctave, 0, hop, sparsity])
    }
}
VQT : MIRaConstantQ {
    *kr { |in, fmin = 55, numBins = 84, binsPerOctave = 12, gamma = 20, hop = 0.25, sparsity = 1e-4|
        this.validate(in, fmin, numBins, binsPerOctave, gamma, hop, sparsity);
        ^this.multiNewList([\control, numBins, in, fmin, numBins, binsPerOctave, gamma, hop, sparsity])
    }
}
CQCC : MIRaConstantQ {
    *kr { |in, numCoeffs = 13, fmin = 55, numBins = 84, binsPerOctave = 12, hop = 0.25,
        uniformBins = 256, include0 = 1, logFloor = 1e-10, sparsity = 1e-4|
        this.validate(in, fmin, numBins, binsPerOctave, 0, hop, sparsity);
        if(numCoeffs.isInteger.not or: { numCoeffs < 1 } or: { numCoeffs > 64 }
            or: { uniformBins.isInteger.not } or: { uniformBins < 16 } or: { uniformBins > 1024 }
            or: { include0.isInteger.not } or: { [0, 1].includes(include0).not }
            or: { numCoeffs + (include0 <= 0).asInteger > uniformBins }) {
            Error("CQCC: invalid coefficient or uniform-bin count.").throw
        };
        ^this.multiNewList([\control, numCoeffs, in, fmin, numBins, binsPerOctave, 0, hop, sparsity,
            0, 0, uniformBins, include0, logFloor])
    }
}

ChromaCQT : MIRaConstantQ {
    *kr { |in, fmin=55, octaves=5, binsPerOctave=12, tuning=440, normalize=\max,
        threshold=0, hop=0.25, sparsity=1e-4|
        var norm=[\none,\max,\sum,\l2].indexOf(normalize), minimum;
        if(octaves.isInteger.not or: {octaves<1} or: {octaves>8}
            or: {[12,24,36,48].includes(binsPerOctave).not}
            or: {tuning.isNumber.not} or: {tuning<300} or: {tuning>500}
            or: {threshold.isNumber.not} or: {threshold<0} or: {threshold>1}
            or: {norm.isNil}) { Error("ChromaCQT: invalid literal chroma settings.").throw };
        minimum=fmin * (tuning / 440);
        this.validate(in,minimum,octaves*binsPerOctave,binsPerOctave,0,hop,sparsity);
        ^this.multiNewList([\control,12,in,minimum,octaves*binsPerOctave,binsPerOctave,
            0,hop,sparsity,tuning,norm,threshold])
    }
}
