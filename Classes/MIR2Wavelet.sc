// SPDX-License-Identifier: GPL-3.0-or-later
MIR2WaveletAnalyzer : MultiOutUGen {
    *validate { |in, frameSize, order, levels, hop|
        if(in.rate != \audio) { Error("MIR2 wavelets require audio-rate input.").throw };
        if(frameSize.isInteger.not or: { [32,64,128,256,512,1024,2048,4096,8192].includes(frameSize).not }
            or: { order.isInteger.not } or: { order < 1 } or: { order > 4 }
            or: { levels.isInteger.not } or: { levels < 1 } or: { levels > frameSize.log2 }
            or: { hop.isNumber.not } or: { hop < 0.0625 } or: { hop > 1 }) {
            Error("MIR2 wavelets: invalid literal frameSize, order, levels or hop.").throw
        }
    }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
Wavelets2 : MIR2WaveletAnalyzer {
    *kr { |in, frameSize = 1024, order = 4, levels = 6, hop = 0.5, mode = \energy|
        var modeIndex = [\energy, \rms, \entropy].indexOf(mode);
        this.validate(in, frameSize, order, levels, hop);
        if(modeIndex.isNil) { Error("Wavelets2: mode must be energy, rms or entropy.").throw };
        ^this.multiNewList([\control, levels + 1, in, frameSize, order, levels, hop, modeIndex])
    }
}
WaveletBands : Wavelets2 {}
WaveletEntropy : UGen {
    *kr { |in, frameSize = 1024, order = 4, levels = 6, hop = 0.5|
        MIR2WaveletAnalyzer.validate(in, frameSize, order, levels, hop);
        ^this.multiNew(\control, in, frameSize, order, levels, hop)
    }
    checkInputs { ^this.checkValidInputs }
}
WaveletFlux : WaveletEntropy {}
WaveletCentroid : WaveletEntropy {}
WaveletSpread : WaveletEntropy {}
WaveletDaub2 : UGen {
    *ar { |in, frameSize = 512, order = 4, levels = 6, remove = 8|
        MIR2WaveletAnalyzer.validate(in, frameSize, order, levels, 1);
        ^this.multiNew(\audio, in, frameSize, order, levels, remove)
    }
    checkInputs { ^this.checkValidInputs }
}
