// SPDX-License-Identifier: GPL-3.0-or-later
MIRaAuditoryAnalyzer : MultiOutUGen {
    *validateBands { |numBands|
        if(numBands.isInteger.not or: { numBands < 4 } or: { numBands > 128 }) {
            Error("MIRa auditory: numBands must be a literal integer from 4 through 128.").throw
        }
    }
    *validateCoeffs { |numCoeffs, numBands, include0|
        this.validateBands(numBands);
        if(numCoeffs.isInteger.not or: { numCoeffs < 1 } or: { numCoeffs > 64 }
            or: { [0, 1].includes(include0).not } or: { numCoeffs + (1-include0) > numBands }) {
            Error("MIRa auditory: invalid literal numCoeffs/numBands/include0.").throw
        }
    }
    *kr { Error("MIRaAuditoryAnalyzer is abstract.").throw }
    *ar { Error("MIRa auditory analyzers produce control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

BarkBands : MIRaAuditoryAnalyzer {
    *kr { |chain, numBands = 24, minFreq = 20, maxFreq = 0, normalize = 1,
        spectrumMode = 1, fftSize = 1024|
        this.validateBands(numBands);
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        MIRaFFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNewList([\control, numBands, chain, numBands, numBands,
            minFreq, maxFreq, normalize, spectrumMode, 1, 1e-10, 1, 0, fftSize])
    }
}

ERBBands : MIRaAuditoryAnalyzer {
    *kr { |chain, numBands = 24, minFreq = 20, maxFreq = 0, normalize = 1,
        spectrumMode = 1, fftSize = 1024|
        this.validateBands(numBands);
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        MIRaFFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNewList([\control, numBands, chain, numBands, numBands,
            minFreq, maxFreq, normalize, spectrumMode, 1, 1e-10, 1, 0, fftSize])
    }
}

BFCC : MIRaAuditoryAnalyzer {
    *kr { |chain, numCoeffs = 13, numBands = 40, minFreq = 20, maxFreq = 0,
        include0 = 1, normalize = 1, spectrumMode = 1, logFloor = 1e-10,
        dctNorm = 1, lifter = 0, fftSize = 1024|
        this.validateCoeffs(numCoeffs, numBands, include0);
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        MIRaFFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNewList([\control, numCoeffs, chain, numCoeffs, numBands,
            minFreq, maxFreq, normalize, spectrumMode, include0, logFloor, dctNorm, lifter, fftSize])
    }
}

LFCC : MIRaAuditoryAnalyzer {
    *kr { |chain, numCoeffs = 13, numBands = 40, minFreq = 20, maxFreq = 0,
        include0 = 1, normalize = 1, spectrumMode = 1, logFloor = 1e-10,
        dctNorm = 1, lifter = 0, fftSize = 1024|
        this.validateCoeffs(numCoeffs, numBands, include0);
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        MIRaFFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNewList([\control, numCoeffs, chain, numCoeffs, numBands,
            minFreq, maxFreq, normalize, spectrumMode, include0, logFloor, dctNorm, lifter, fftSize])
    }
}

GammatoneBands : MIRaAuditoryAnalyzer {
    *kr { |in, numBands = 32, minFreq = 50, maxFreq = 0, smoothing = 0.01, hop = 0.01|
        this.validateBands(numBands);
        ^this.multiNewList([\control, numBands, in, numBands, numBands,
            minFreq, maxFreq, smoothing, hop, 1, 0, 1e-10])
    }
    checkInputs {
        if(inputs[0].rate != \audio) { ^"GammatoneBands requires an audio-rate input." };
        ^this.checkValidInputs
    }
}

GFCC : MIRaAuditoryAnalyzer {
    *kr { |in, numCoeffs = 13, numBands = 64, minFreq = 50, maxFreq = 0,
        smoothing = 0.01, hop = 0.01, include0 = 1, compression = \cubeRoot, logFloor = 1e-10|
        var compressionIndex = [\cubeRoot, \log].indexOf(compression);
        this.validateCoeffs(numCoeffs, numBands, include0);
        if(compressionIndex.isNil) { Error("GFCC: compression must be \\cubeRoot or \\log.").throw };
        ^this.multiNewList([\control, numCoeffs, in, numCoeffs, numBands,
            minFreq, maxFreq, smoothing, hop, include0, compressionIndex, logFloor])
    }
    checkInputs {
        if(inputs[0].rate != \audio) { ^"GFCC requires an audio-rate input." };
        ^this.checkValidInputs
    }
}

Loudness2 : MIRaAuditoryAnalyzer {
    *kr { |chain, calibration = 100, attack = 0.01, release = 0.1,
        windowPower = 0.375, fftSize = 1024|
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        ^this.multiNewList([\control, 2, chain, calibration, attack, release, windowPower, fftSize])
    }
}
