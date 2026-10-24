// SPDX-License-Identifier: GPL-3.0-or-later
MIR2ModulationAnalyzer : MultiOutUGen {
    *build { |bands, numBins, minHz, maxHz, windowTime, hopTime, featureRate,
        normalization, removeMean, phase, compression, gain, fluctuation, smoothBands, smoothBins|
        var norm = [\none, \global, \band].indexOf(normalization);
        var comp = [\none, \logPower].indexOf(compression);
        var count;
        if(bands.isArray.not or: { bands.size < 1 } or: { bands.size > 32 }
            or: { numBins.isInteger.not } or: { numBins < 2 } or: { numBins > 128 }
            or: { minHz.isNumber.not } or: { maxHz.isNumber.not }
            or: { minHz < 0 } or: { maxHz <= minHz }
            or: { windowTime.isNumber.not } or: { windowTime < 0.25 } or: { windowTime > 12 }
            or: { hopTime.isNumber.not } or: { hopTime < 0.02 } or: { hopTime > 2 }
            or: { featureRate.isNumber.not } or: { featureRate < 20 } or: { featureRate > 200 }
            or: { maxHz > (featureRate * 0.4) }
            or: { norm.isNil } or: { comp.isNil }
            or: { [0, 1].includes(removeMean).not } or: { [0, 1].includes(phase).not }
            or: { [0, 1].includes(fluctuation).not }
            or: { gain.isNumber.not } or: { gain < 0 } or: { gain > 1e6 }
            or: { smoothBands.isNumber.not } or: { smoothBands < 0 } or: { smoothBands > 3 }
            or: { smoothBins.isNumber.not } or: { smoothBins < 0 } or: { smoothBins > 3 }) {
            Error("MIR2 modulation: invalid literal dimensions, timing, modes or shaping controls.").throw
        };
        count = bands.size * numBins * (phase + 1);
        if(count > 1024) { Error("MIR2 modulation: at most 1024 outputs are supported.").throw };
        ^this.multiNewList([\control, count, bands.size, numBins, minHz, maxHz,
            windowTime, hopTime, featureRate, norm, removeMean, phase, comp,
            gain, fluctuation, smoothBands, smoothBins] ++ bands)
    }
    *kr { Error("MIR2ModulationAnalyzer is abstract.").throw }
    *ar { Error("MIR2 modulation analyzers produce control-rate outputs only.").throw }
    *frequencies { |minHz = 0.5, maxHz = 8, numBins = 16|
        ^Array.interpolation(numBins, minHz, maxHz)
    }
    init { |numOutputs ... theInputs| inputs = theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs {
        if(inputs.drop(15).any({ |item| item.rate == \audio })) {
            ^"MIR2 modulation requires an Array of control-rate or scalar band envelopes."
        };
        ^this.checkValidInputs
    }
}

RhythmTransform : MIR2ModulationAnalyzer {
    *kr { |bands, numBins = 16, minHz = 0.5, maxHz = 8, windowTime = 4,
        hopTime = 0.1, featureRate = 100, normalization = \none,
        removeMean = 1, phase = 0|
        ^this.build(bands, numBins, minHz, maxHz, windowTime, hopTime,
            featureRate, normalization, removeMean, phase, \none, 1, 0, 0, 0)
    }
}

RhythmPattern : MIR2ModulationAnalyzer {
    *kr { |chain, numBands = 24, numBins = 16, minHz = 0.5, maxHz = 8,
        windowTime = 4, hopTime = 0.1, featureRate = 100, gain = 1,
        normalization = \global, fluctuation = 1, smoothBands = 0.5,
        smoothBins = 0.5, minFreq = 20, maxFreq = 0, fftSize = 1024|
        var bands;
        if(numBands.isInteger.not or: { numBands < 4 } or: { numBands > 32 }) {
            Error("RhythmPattern: numBands must be a literal integer from 4 through 32.").throw
        };
        bands = BarkBands.kr(chain, numBands, minFreq, maxFreq, 1, 1, fftSize);
        ^this.krBands(bands, numBins, minHz, maxHz, windowTime, hopTime,
            featureRate, \logPower, gain, normalization, fluctuation, smoothBands, smoothBins)
    }
    *krBands { |bands, numBins = 16, minHz = 0.5, maxHz = 8, windowTime = 4,
        hopTime = 0.1, featureRate = 100, compression = \none, gain = 1,
        normalization = \global, fluctuation = 1, smoothBands = 0.5, smoothBins = 0.5|
        ^this.build(bands, numBins, minHz, maxHz, windowTime, hopTime,
            featureRate, normalization, 1, 0, compression, gain, fluctuation, smoothBands, smoothBins)
    }
}
