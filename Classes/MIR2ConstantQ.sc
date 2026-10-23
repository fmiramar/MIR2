// SPDX-License-Identifier: GPL-3.0-or-later
MIR2ConstantQ : MultiOutUGen {
    *validate { |in, fmin, numBins, binsPerOctave, gamma, hop, sparsity|
        if(in.rate != \audio or: { fmin.isNumber.not } or: { fmin <= 0 }
            or: { numBins.isInteger.not } or: { numBins < 2 } or: { numBins > 192 }
            or: { binsPerOctave.isInteger.not } or: { binsPerOctave < 6 } or: { binsPerOctave > 48 }
            or: { gamma.isNumber.not } or: { gamma < 0 }
            or: { hop.isNumber.not } or: { hop < 0.0625 } or: { hop > 1 }
            or: { sparsity.isNumber.not } or: { sparsity < 1e-8 } or: { sparsity > 0.01 }) {
            Error("MIR2 constant-Q: invalid audio input or literal transform settings.").throw
        }
    }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
    *frameSize { |sampleRate = 48000, fmin = 55, binsPerOctave = 12, gamma = 0|
        ^(sampleRate / (((2 ** (1 / binsPerOctave)) - 1) * fmin + gamma)).ceil.max(32).asInteger.nextPowerOfTwo
    }
}
CQT : MIR2ConstantQ {
    *kr { |in, fmin = 55, numBins = 84, binsPerOctave = 12, hop = 0.25, sparsity = 1e-4|
        this.validate(in, fmin, numBins, binsPerOctave, 0, hop, sparsity);
        ^this.multiNewList([\control, numBins, in, fmin, numBins, binsPerOctave, 0, hop, sparsity])
    }
}
VQT : MIR2ConstantQ {
    *kr { |in, fmin = 55, numBins = 84, binsPerOctave = 12, gamma = 20, hop = 0.25, sparsity = 1e-4|
        this.validate(in, fmin, numBins, binsPerOctave, gamma, hop, sparsity);
        ^this.multiNewList([\control, numBins, in, fmin, numBins, binsPerOctave, gamma, hop, sparsity])
    }
}
CQCC : MIR2ConstantQ {
    *kr { |in, numCoeffs = 13, fmin = 55, numBins = 84, binsPerOctave = 12, hop = 0.25,
        uniformBins = 256, include0 = 1, logFloor = 1e-10, sparsity = 1e-4|
        this.validate(in, fmin, numBins, binsPerOctave, 0, hop, sparsity);
        if(numCoeffs.isInteger.not or: { numCoeffs < 1 } or: { numCoeffs > 64 }
            or: { uniformBins.isInteger.not } or: { uniformBins < 16 } or: { uniformBins > 1024 }
            or: { numCoeffs + (include0 <= 0).asInteger > uniformBins }) {
            Error("CQCC: invalid coefficient or uniform-bin count.").throw
        };
        ^this.multiNewList([\control, numCoeffs, in, fmin, numBins, binsPerOctave, 0, hop, sparsity,
            0, 0, uniformBins, include0, logFloor])
    }
}
