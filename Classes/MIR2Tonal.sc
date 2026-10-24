// SPDX-License-Identifier: GPL-3.0-or-later
MIR2Tonal : MultiOutUGen {
    *validateSize { |size|
        if([12,24,36,120].includes(size).not) { Error("MIR2 tonal: size must be 12, 24, 36, or 120.").throw }
    }
    *build { |count, chain, size, tuning, minFreq, maxFreq, harmonics, normalize, weighting, window,
        peakThreshold, fftSize, decay=0, profile=0, threshold=0|
        var norm, weight;
        this.validateSize(size); MIR2FFTAnalyzer.validateFFTSize(fftSize);
        norm = [\none, \max, \sum, \l2].indexOf(normalize);
        weight = [\none, \cosine, \squaredCosine].indexOf(weighting);
        if(norm.isNil or: { weight.isNil }) { Error("MIR2 tonal: invalid normalization or weighting.").throw };
        ^this.multiNewList([\control, count, chain, size, tuning, minFreq, maxFreq, harmonics,
            norm, weight, window, peakThreshold, fftSize, decay, profile, threshold])
    }
    *ar { Error("MIR2 tonal features produce control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
HPCP : MIR2Tonal {
    *kr { |chain, size=12, tuning=440, minFreq=40, maxFreq=5000, harmonics=8,
        normalize=\max, weighting=\squaredCosine, window=1.3333333333, peakThreshold=0.01, fftSize=4096|
        ^this.build(size,chain,size,tuning,minFreq,maxFreq,harmonics,normalize,weighting,window,peakThreshold,fftSize)
    }
}
Chromagram2 : MIR2Tonal {
    *kr { |chain, size=12, tuning=440, minFreq=40, maxFreq=5000, method=\fft,
        harmonics=8, normalize=\max, peakThreshold=0.01, fftSize=4096|
        if([\fft,\hpcp].includes(method).not) { Error("Chromagram2: method must be fft or hpcp.").throw };
        ^this.build(size,chain,size,tuning,minFreq,maxFreq,if(method==\fft){0}{harmonics},
            normalize,\squaredCosine,1.3333333333,peakThreshold,fftSize)
    }
}
TuningFrequency : MIR2Tonal {
    *kr { |chain, minFreq=80, maxFreq=5000, peakThreshold=0.05, fftSize=4096|
        ^this.build(3,chain,12,440,minFreq,maxFreq,1,\max,\none,1,peakThreshold,fftSize)
    }
}
KeyTrack2 : MIR2Tonal {
    *outputs { ^3 }
    *kr { |chain, tuning=440, profile=\temperley, decay=2, minFreq=40, maxFreq=5000,
        threshold=0.3, peakThreshold=0.01, fftSize=4096|
        var index=[\krumhansl,\temperley,\edma].indexOf(profile);
        if(index.isNil) { Error("KeyTrack2: profile must be krumhansl, temperley, or edma.").throw };
        ^this.build(this.outputs,chain,12,tuning,minFreq,maxFreq,8,\sum,\squaredCosine,
            1.3333333333,peakThreshold,fftSize,decay,index,threshold)
    }
}
KeyClarity2 : KeyTrack2 { *outputs { ^1 } }
KeyMode2 : KeyTrack2 { *outputs { ^1 } }
MIR2ChromaVector : MultiOutUGen {
    *build { |outputs, chroma, trig=1, smooth=0, transitionPenalty=0, threshold=0,
        windowSize=1, downsample=1|
        var vector=chroma.asArray;
        MIR2Tonal.validateSize(vector.size);
        ^this.multiNewList([\control,outputs,vector.size,trig,smooth,transitionPenalty,
            threshold,windowSize,downsample]++vector)
    }
    *ar { Error("MIR2 chroma features produce control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
Tonnetz : MIR2ChromaVector {
    *kr { |chroma, trig=1| ^this.build(6,chroma,trig) }
}
HPCPEntropy : MIR2ChromaVector {
    *kr { |chroma, trig=1| ^this.build(1,chroma,trig) }
}
HPCPCrest : MIR2ChromaVector {
    *kr { |chroma, trig=1| ^this.build(1,chroma,trig) }
}
ChromaCENS : MIR2ChromaVector {
    *kr { |chroma, windowSize=21, downsample=1, trig=1|
        if(windowSize.isInteger.not or: { windowSize<1 } or: { windowSize>301 }
            or: { downsample.isInteger.not } or: { downsample<1 } or: { downsample>64 }) {
            Error("ChromaCENS: windowSize must be 1..301, downsample 1..64, both literal integers.").throw
        };
        ^this.build(12,chroma,trig,0,0,0,windowSize,downsample)
    }
}
ChordTrack : MIR2ChromaVector {
    *outputs { ^4 }
    *vocabularyIndex { |vocabulary|
        var index=[\triads,\extended].indexOf(vocabulary);
        if(index.isNil) { Error("ChordTrack: vocabulary must be triads or extended.").throw };
        ^index
    }
    *qualityNames { |vocabulary=\triads|
        ^#[maj,min,dim,aug,7,maj7,min7,sus2,sus4].copyRange(0,
            if(this.vocabularyIndex(vocabulary)==0){1}{8})
    }
    *label { |root,quality|
        if(root.isInteger.not or: {quality.isInteger.not}
            or: {root<0} or: {root>11} or: {quality<0} or: {quality>8}) { ^"N" };
        ^#["C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"][root]
            ++ ":" ++ this.qualityNames(\extended)[quality].asString
    }
    *kr { |chroma, smooth=0.1, transitionPenalty=0.2, threshold=0.65, trig=1, vocabulary=\triads|
        ^this.build(this.outputs,chroma,trig,smooth,transitionPenalty,threshold,
            this.vocabularyIndex(vocabulary)+1)
    }
}
ChordStrength : ChordTrack { *outputs { ^1 } }
HarmonicChange : MIR2ChromaVector {
    *kr { |chroma, smooth=0.1, trig=1| ^this.build(1,chroma,trig,smooth) }
}
ChordStats : MultiOutUGen {
    *kr { |root, quality, trig=1, reset=0, vocabulary=\triads|
        ^this.multiNewList([\control,5,root,quality,trig,reset,ChordTrack.vocabularyIndex(vocabulary)])
    }
    *ar { Error("ChordStats produces control-rate outputs.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs, \control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
