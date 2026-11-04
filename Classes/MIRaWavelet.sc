// SPDX-License-Identifier: GPL-3.0-or-later
MIRaWaveletAnalyzer : MultiOutUGen {
    *resolveOrder { |family, order, boundary|
        if([\periodic, \symmetric].includes(boundary).not) { Error("MIRa wavelets support periodic and symmetric boundaries.").throw };
        if(family == \haar) { ^1 };
        if(family != \db) { Error("MIRa wavelets support the haar and db families.").throw };
        ^order
    }
    *validate { |in, frameSize, order, levels, hop|
        if(in.rate != \audio) { Error("MIRa wavelets require audio-rate input.").throw };
        if(frameSize.isInteger.not or: { [32,64,128,256,512,1024,2048,4096,8192].includes(frameSize).not }
            or: { order.isInteger.not } or: { order < 1 } or: { order > 8 }
            or: { levels.isInteger.not } or: { levels < 1 } or: { levels > frameSize.log2 }
            or: { hop.isNumber.not } or: { hop < 0.0625 } or: { hop > 1 }) {
            Error("MIRa wavelets: invalid literal frameSize, order, levels or hop.").throw
        }
    }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
    *ar { Error("MIRa wavelet descriptors produce control-rate outputs only.").throw }
}
Wavelets2 : MIRaWaveletAnalyzer {
    *kr { |in, frameSize = 1024, order = 4, levels = 6, hop = 0.5, mode = \energy, family = \db, boundary = \periodic|
        var modeIndex = [\energy, \rms, \entropy].indexOf(mode);
        order = this.resolveOrder(family, order, boundary);
        this.validate(in, frameSize, order, levels, hop);
        if(modeIndex.isNil) { Error("Wavelets2: mode must be energy, rms or entropy.").throw };
        ^this.multiNewList([\control, levels + 1, in, frameSize, order, levels, hop, modeIndex, [\periodic, \symmetric].indexOf(boundary)])
    }
}
WaveletBands : Wavelets2 {}
WaveletEntropy : UGen {
    *kr { |in, frameSize = 1024, order = 4, levels = 6, hop = 0.5, family = \db, boundary = \periodic|
        order = MIRaWaveletAnalyzer.resolveOrder(family, order, boundary);
        MIRaWaveletAnalyzer.validate(in, frameSize, order, levels, hop);
        ^this.multiNew(\control, in, frameSize, order, levels, hop, [\periodic, \symmetric].indexOf(boundary))
    }
    checkInputs { ^this.checkValidInputs }
    *ar { Error("MIRa wavelet descriptors produce control-rate outputs only.").throw }
}
WaveletFlux : WaveletEntropy {}
WaveletCentroid : WaveletEntropy {}
WaveletSpread : WaveletEntropy {}
WaveletDaub2 : UGen {
    *ar { |in, frameSize = 512, order = 4, levels = 6, remove = 0, family = \db, boundary = \periodic|
        order = MIRaWaveletAnalyzer.resolveOrder(family, order, boundary);
        MIRaWaveletAnalyzer.validate(in, frameSize, order, levels, 1);
        ^this.multiNew(\audio, in, frameSize, order, levels, remove, [\periodic, \symmetric].indexOf(boundary))
    }
    checkInputs { ^this.checkValidInputs }
}
