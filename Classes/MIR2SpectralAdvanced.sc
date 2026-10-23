// SPDX-License-Identifier: GPL-3.0-or-later

FFTCrest2 : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("FFTCrest2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

FFTSpread2 : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("FFTSpread2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

FFTSlope2 : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("FFTSlope2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

FFTPower2 : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("FFTPower2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecEntropy : UGen {
    *kr { |chain, mode = \normalized, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        var modeIndex;
        modeIndex = [\normalized, \natural, \bits].indexOf(mode);
        if(modeIndex.isNil) { Error("SpecEntropy: invalid mode.").throw };
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, modeIndex, fftSize)
    }
    *ar { Error("SpecEntropy is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecDecrease : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecDecrease is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecComplexity : UGen {
    *kr { |chain, threshold = 0.1, weighted = 0, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, threshold, weighted, fftSize)
    }
    *ar { Error("SpecComplexity is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecStrongPeak : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecStrongPeak is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecHFC : UGen {
    *kr { |chain, weighting = \linear, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        var modeIndex;
        modeIndex = [\linear, \quadratic].indexOf(weighting);
        if(modeIndex.isNil) { Error("SpecHFC: invalid weighting.").throw };
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, modeIndex, fftSize)
    }
    *ar { Error("SpecHFC is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecBandwidth : UGen {
    *kr { |chain, p = 2, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, p, 0, fftSize)
    }
    *ar { Error("SpecBandwidth is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecIrregularity : UGen {
    *kr { |chain, method = \jensen, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        var modeIndex;
        modeIndex = [\jensen, \krimphoff].indexOf(method);
        if(modeIndex.isNil) { Error("SpecIrregularity: invalid method.").throw };
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, modeIndex, fftSize)
    }
    *ar { Error("SpecIrregularity is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecVariation : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecVariation is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecRMS : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        ^this.multiNew(\control, chain, minFreq, maxFreq, 1, 2, 0, fftSize)
    }
    *ar { Error("SpecRMS is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecEnergy : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecEnergy is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecBandEnergy : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecBandEnergy is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecBandRatio : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecBandRatio is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecSparsity : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, 2, 0, fftSize)
    }
    *ar { Error("SpecSparsity is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

FFTPeak2 : MultiOutUGen {
    *kr { |chain, minFreq = 20, maxFreq = 0, interpolate = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        ^this.multiNewList([\control, 2, chain, minFreq, maxFreq, interpolate, fftSize])
    }
    *ar { Error("FFTPeak2 is control-rate only.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

MIR2SpectralBands : MultiOutUGen {
    *validate { |boundaries, fftSize, spectrumMode|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        if(boundaries.isArray.not or: { boundaries.size < 2 } or: { boundaries.size > 65 }) {
            Error("MIR2 subbands: boundaries must be a literal Array of 2 through 65 frequencies.").throw
        };
        boundaries.do { |value, i|
            if(value.isNumber.not or: { value < 0 } or: { i > 0 and: { value <= boundaries[i-1] } }) {
                Error("MIR2 subbands: boundaries must be nonnegative and strictly increasing.").throw
            }
        }
    }
    *kr { Error("MIR2SpectralBands is abstract.").throw }
    *ar { Error("MIR2 spectral bands are control-rate only.").throw }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

FFTSubbandFlatness2 : MIR2SpectralBands {
    *kr { |chain, boundaries = #[0, 200, 400, 800, 1600, 3200, 6400, 20000],
        floor = 1e-20, spectrumMode = 1, fftSize = 1024|
        var count = boundaries.size - 1;
        this.validate(boundaries, fftSize, spectrumMode);
        ^this.multiNewList([\control, count, chain, count,
            spectrumMode, floor, 0, fftSize] ++ boundaries)
    }
}

FFTSubbandFlux2 : MIR2SpectralBands {
    *kr { |chain, boundaries = #[0, 200, 400, 800, 1600, 3200, 6400, 20000],
        normalize = 1, positive = 1, spectrumMode = 0, fftSize = 1024|
        var count = boundaries.size - 1;
        this.validate(boundaries, fftSize, spectrumMode);
        ^this.multiNewList([\control, count, chain, count,
            spectrumMode, normalize, positive, fftSize] ++ boundaries)
    }
}

FFTSubbandPower2 : MIR2SpectralBands {
    *kr { |chain, boundaries = #[0, 200, 400, 800, 1600, 3200, 6400, 20000],
        spectrumMode = 1, fftSize = 1024|
        var count = boundaries.size - 1;
        this.validate(boundaries, fftSize, spectrumMode);
        ^this.multiNewList([\control, count, chain, count,
            spectrumMode, 0, 0, fftSize] ++ boundaries)
    }
}

SpecContrast : MIR2SpectralBands {
    *kr { |chain, boundaries = #[0, 200, 400, 800, 1600, 3200, 6400, 20000],
        quantile = 0.1, includeValleys = 0, spectrumMode = 0, fftSize = 1024|
        var count = boundaries.size - 1;
        this.validate(boundaries, fftSize, spectrumMode);
        if([0, 1].includes(includeValleys).not) { Error("SpecContrast: includeValleys must be literal 0 or 1.").throw };
        ^this.multiNewList([\control, count * (1 + includeValleys), chain, count,
            spectrumMode, quantile, includeValleys, fftSize] ++ boundaries)
    }
}

