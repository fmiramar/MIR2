// SPDX-License-Identifier: GPL-3.0-or-later
PNCC : MultiOutUGen {
    *kr { |in, numCoeffs = 13, numBands = 40, minFreq = 200, maxFreq = 8000,
        frameTime = 0.0256, hopTime = 0.01, preEmphasis = 0.97, include0 = 1, smoothBands = 4|
        if(in.rate != \audio or: { numCoeffs.isInteger.not } or: { numCoeffs < 1 } or: { numCoeffs > 64 }
            or: { numBands.isInteger.not } or: { numBands < 4 } or: { numBands > 128 }
            or: { include0.isNumber.not } or: { numCoeffs + (include0 <= 0).asInteger > numBands }
            or: { minFreq.isNumber.not } or: { minFreq < 0 } or: { maxFreq.isNumber.not }
            or: { maxFreq <= minFreq }
            or: { frameTime.isNumber.not } or: { frameTime < 0.005 } or: { frameTime > 0.1 }
            or: { hopTime.isNumber.not } or: { hopTime < 0.001 } or: { hopTime > 0.1 }
            or: { smoothBands.isInteger.not } or: { smoothBands < 0 } or: { smoothBands >= numBands }) {
            Error("PNCC: invalid input or literal analysis settings.").throw
        };
        ^this.multiNewList([\control, numCoeffs, in, numBands, minFreq, maxFreq,
            frameTime, hopTime, preEmphasis, include0, smoothBands])
    }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    *ar { Error("PNCC produces control-rate outputs only.").throw }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
