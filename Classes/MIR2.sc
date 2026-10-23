// SPDX-License-Identifier: GPL-3.0-or-later

MIR2FFTAnalyzer : MultiOutUGen {
    *validateFFTSize { |fftSize|
        if(fftSize.isInteger.not
            or: { [256, 512, 1024, 2048, 4096, 8192].includes(fftSize).not }) {
            Error("MIR2: fftSize must be a literal power of two from 256 through 8192.").throw
        }
    }

    *validateSpectrumMode { |spectrumMode|
        if(spectrumMode.isInteger.not or: { [0, 1].includes(spectrumMode).not }) {
            Error("MIR2: spectrumMode must be the literal integer 0 (magnitude) or 1 (power).").throw
        }
    }

    *kr { Error("MIR2FFTAnalyzer is abstract.").throw }
    *ar { Error("MIR2 analyzers produce control-rate outputs only.").throw }
}

SpecCentroid2 : UGen {
    *kr { |chain, minFreq = 0, maxFreq = 0, spectrumMode = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(\control, chain, minFreq, maxFreq, spectrumMode, fftSize)
    }
    *ar { Error("SpecCentroid2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecFlatness2 : UGen {
    *kr {
        |chain, minFreq = 0, maxFreq = 0, spectrumMode = 1,
        floor = 1e-20, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(
            \control, chain, minFreq, maxFreq, spectrumMode, floor, fftSize
        )
    }
    *ar { Error("SpecFlatness2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

SpecPcile2 : UGen {
    *kr {
        |chain, fraction = 0.85, minFreq = 0, maxFreq = 0,
        interpolate = 1, spectrumMode = 1, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(
            \control, chain, fraction, minFreq, maxFreq,
            interpolate, spectrumMode, fftSize
        )
    }
    *ar { Error("SpecPcile2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

MFCC2 : MultiOutUGen {
    *kr {
        |chain, numCoeffs = 13, numBands = 40, minFreq = 20,
        maxFreq = 0, include0 = 1, melMode = \slaney,
        spectrumMode = 1, logFloor = 1e-10, dctNorm = 1,
        lifter = 0, fftSize = 1024|
        var melIndex;
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        if(numCoeffs.isInteger.not or: { numCoeffs < 1 } or: { numCoeffs > 64 }
            or: { numBands.isInteger.not } or: { numBands < 4 } or: { numBands > 128 }
            or: { numCoeffs + (include0 <= 0).asInteger > numBands }) {
            Error("MFCC2: invalid literal numCoeffs/numBands/include0 configuration.").throw
        };
        melIndex = case
            { melMode == \slaney } { 0 }
            { melMode == \htk } { 1 }
            { melMode.isNumber } { melMode }
            { Error("MFCC2: melMode must be \\slaney or \\htk.").throw };
        ^this.multiNewList([
            \control, numCoeffs, chain, numCoeffs, numBands, minFreq,
            maxFreq, include0, melIndex, spectrumMode, logFloor,
            dctNorm, lifter, fftSize
        ])
    }

    *ar { Error("MFCC2 is control-rate only.").throw }
    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \control)
    }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

FFTFlux2 : UGen {
    *kr {
        |chain, normalize = 1, positive = 0, spectrumMode = 0,
        minFreq = 0, maxFreq = 0, fftSize = 1024|
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        MIR2FFTAnalyzer.validateSpectrumMode(spectrumMode);
        ^this.multiNew(
            \control, chain, normalize, positive, spectrumMode,
            minFreq, maxFreq, fftSize
        )
    }
    *ar { Error("FFTFlux2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

Onsets2 : MultiOutUGen {
    *kr {
        |chain, threshold = 0.3, method = \superflux,
        relaxTime = 1, minGap = 0.03, medianTime = 0.15,
        maxFilterBins = 3, floor = 1e-5, fftSize = 1024,
        hop = 0.5, whitening = 1, logGain = 100, silenceThreshold = 1e-4,
        diffFrames = 0, noveltyFloor = 0.01, releaseTime = 0.15|
        var methodIndex;
        var methods = #[flux, fluxPos, superflux, power, magsum, complex,
            rcomplex, phase, wphase, mkl, hfc];
        MIR2FFTAnalyzer.validateFFTSize(fftSize);
        if(method == \weightedPhase) { method = \wphase };
        methodIndex = if(method.isNumber) { method } { methods.indexOf(method) };
        if(methodIndex.isInteger.not or: { methodIndex < 0 } or: { methodIndex > 10 }) {
            Error("Onsets2: unknown detection method. See the help page for symbols.").throw
        };
        if(maxFilterBins.isInteger.not or: { maxFilterBins < 0 }
            or: { maxFilterBins > 25 }) {
            Error("Onsets2: maxFilterBins must be a literal integer from 0 through 25.").throw
        };
        if(hop.isNumber.not or: { hop < 0.0625 } or: { hop > 1 }
            or: { medianTime.isNumber.not } or: { medianTime < 0 } or: { medianTime > 10 }
            or: { diffFrames.isInteger.not } or: { diffFrames < 0 } or: { diffFrames > 16 }) {
            Error("Onsets2: invalid literal hop, medianTime, or diffFrames.").throw
        };
        ^this.multiNewList([
            \control, 2, chain, threshold, methodIndex, relaxTime,
            minGap, medianTime, maxFilterBins, floor, fftSize, hop,
            whitening, logGain, silenceThreshold, diffFrames, noveltyFloor, releaseTime
        ])
    }
    *ar { Error("Onsets2 is control-rate only.").throw }
    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \control)
    }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

MIR2PitchAnalyzer : MultiOutUGen {
    *validate { |frameSize, hop, minFreq, maxFreq|
        if(frameSize.isInteger.not
            or: { [256, 512, 1024, 2048, 4096, 8192].includes(frameSize).not }) {
            Error("MIR2 pitch: frameSize must be a literal power of two from 256 through 8192.").throw
        };
        if(hop.isNumber.not or: { hop < 0.0625 } or: { hop > 1 }
            or: { minFreq <= 0 } or: { maxFreq <= minFreq }) {
            Error("MIR2 pitch: invalid literal hop or frequency bounds.").throw
        }
    }

    *kr { Error("MIR2PitchAnalyzer is abstract.").throw }
    *ar { Error("MIR2 pitch analyzers produce control-rate outputs only.").throw }
}

PitchYIN : MultiOutUGen {
    *kr {
        |in, minFreq = 40, maxFreq = 4000, frameSize = 2048,
        hop = 0.25, threshold = 0.1|
        MIR2PitchAnalyzer.validate(frameSize, hop, minFreq, maxFreq);
        ^this.multiNewList([
            \control, 3, in, minFreq, maxFreq, frameSize, hop, threshold
        ])
    }

    *latency { |sampleRate = 48000, frameSize = 2048| ^frameSize / sampleRate }
    *ar { Error("PitchYIN is control-rate only.").throw }
    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \control)
    }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

PitchMPM : MultiOutUGen {
    *kr {
        |in, minFreq = 40, maxFreq = 4000, frameSize = 2048,
        hop = 0.25, cutoff = 0.93|
        MIR2PitchAnalyzer.validate(frameSize, hop, minFreq, maxFreq);
        ^this.multiNewList([
            \control, 3, in, minFreq, maxFreq, frameSize, hop, cutoff
        ])
    }

    *latency { |sampleRate = 48000, frameSize = 2048| ^frameSize / sampleRate }
    *ar { Error("PitchMPM is control-rate only.").throw }
    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \control)
    }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

Periodicity : UGen {
    *kr {
        |in, minFreq = 40, maxFreq = 4000, frameSize = 2048,
        hop = 0.25, method = \nsdf|
        var methodIndex;
        MIR2PitchAnalyzer.validate(frameSize, hop, minFreq, maxFreq);
        methodIndex = case
            { method == \nsdf } { 0 }
            { method == \yin } { 1 }
            { method.isNumber } { method }
            { Error("Periodicity: method must be \\nsdf or \\yin.").throw };
        ^this.multiNew(
            \control, in, minFreq, maxFreq, frameSize, hop, methodIndex
        )
    }

    *latency { |sampleRate = 48000, frameSize = 2048| ^frameSize / sampleRate }
    *ar { Error("Periodicity is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}

Voicing : MultiOutUGen {
    *kr {
        |in, frameSize = 1024, hop = 0.25, minFreq = 40,
        maxFreq = 1000, method = \hybrid, threshold = 0.5|
        var methodIndex;
        MIR2PitchAnalyzer.validate(frameSize, hop, minFreq, maxFreq);
        methodIndex = case
            { method == \periodicity } { 0 }
            { method == \hybrid } { 1 }
            { method.isNumber } { method }
            { Error("Voicing: method must be \\periodicity or \\hybrid.").throw };
        ^this.multiNewList([
            \control, 2, in, frameSize, hop, minFreq, maxFreq,
            methodIndex, threshold
        ])
    }

    *latency { |sampleRate = 48000, frameSize = 1024| ^frameSize / sampleRate }
    *ar { Error("Voicing is control-rate only.").throw }
    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \control)
    }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}

Pitch2 : MultiOutUGen {
    *kr {
        |in, minFreq = 40, maxFreq = 4000, frameSize = 2048,
        hop = 0.25, algorithm = \mpm, threshold = -1,
        smooth = 0, hold = 0|
        var algorithmIndex;
        MIR2PitchAnalyzer.validate(frameSize, hop, minFreq, maxFreq);
        if(algorithm == \swipe) {
            var result = PitchSWIPE.kr(in, minFreq, maxFreq, frameSize, hop,
                if(threshold < 0) { 0.2 } { threshold });
            var frequency = Select.kr(hold > 0, [result[0], Gate.kr(result[0], result[2])]);
            frequency = Lag.kr(frequency.max(1e-12).log, smooth.max(0)).exp * (frequency > 0);
            ^[frequency, result[1], result[2], result[3]]
        };
        algorithmIndex = case
            { algorithm == \yin } { 0 }
            { algorithm == \mpm } { 1 }
            { algorithm.isNumber } { algorithm }
            { Error("Pitch2: algorithm must be \\yin, \\mpm, or \\swipe.").throw };
        ^this.multiNewList([
            \control, 4, in, minFreq, maxFreq, frameSize, hop,
            algorithmIndex, threshold, smooth, hold
        ])
    }

    *latency { |sampleRate = 48000, frameSize = 2048| ^frameSize / sampleRate }
    *ar { Error("Pitch2 is control-rate only.").throw }
    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, \control)
    }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
