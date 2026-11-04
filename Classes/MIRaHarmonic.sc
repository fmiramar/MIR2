// SPDX-License-Identifier: GPL-3.0-or-later
MIRaHarmonic : MultiOutUGen {
    *outputs { |maxHarmonics| ^1 }
    *kr { |chain, f0, maxHarmonics=32, tolerance=50, peakThreshold=0.01, fftSize=4096|
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        if(maxHarmonics.isInteger.not or: { maxHarmonics<1 } or: { maxHarmonics>64 }) {
            Error("MIRa harmonic: maxHarmonics must be a literal integer from 1 through 64.").throw
        };
        ^this.multiNewList([\control,this.outputs(maxHarmonics),chain,f0,maxHarmonics,tolerance,peakThreshold,fftSize])
    }
    *fromAudio { |in, minFreq=55, maxFreq=1760, maxHarmonics=32, tolerance=50,
        peakThreshold=0.01, fftSize=4096, hop=0.25|
        var pitch=Pitch2.kr(in,minFreq,maxFreq,fftSize,hop);
        var chain=FFT(LocalBuf(fftSize),in,hop);
        ^this.kr(chain,pitch[0],maxHarmonics,tolerance,peakThreshold,fftSize)
    }
    *ar { Error("MIRa harmonic descriptors produce control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
HarmonicPeaks : MIRaHarmonic {
    *outputs { |maxHarmonics| ^maxHarmonics*2 }
    *kr { |chain, f0, maxHarmonics=32, tolerance=50, peakThreshold=0.01, fftSize=4096|
        var outputs=super.kr(chain,f0,maxHarmonics,tolerance,peakThreshold,fftSize);
        ^[outputs.copyRange(0,maxHarmonics-1),outputs.copyRange(maxHarmonics,maxHarmonics*2-1)]
    }
}
Inharmonicity : MIRaHarmonic {}
Tristimulus : MIRaHarmonic { *outputs { |maxHarmonics| ^3 } }
OddEvenRatio : MIRaHarmonic {}
HarmonicEnergyRatio : MIRaHarmonic {}
HarmonicCentroid : MIRaHarmonic {}
HarmonicDeviation : MIRaHarmonic {}
HarmonicSpread : MIRaHarmonic {}
HarmonicVariation : MIRaHarmonic {}
Noisiness : MIRaHarmonic {}
SensoryDissonance2 : MultiOutUGen {
    *kr { |chain, model=\minimum, maxPeaks=64, normalize=1, peakThreshold=0.01, fftSize=4096|
        var index=[\minimum,\product].indexOf(model);
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        if(index.isNil) { Error("SensoryDissonance2: model must be minimum or product.").throw };
        if(maxPeaks.isInteger.not or: { maxPeaks<2 } or: { maxPeaks>128 }) {
            Error("SensoryDissonance2: maxPeaks must be a literal integer from 2 through 128.").throw
        };
        ^this.multiNewList([\control,1,chain,index,maxPeaks,normalize,peakThreshold,fftSize])
    }
    *ar { Error("SensoryDissonance2 produces control-rate output.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
