FreqToVolt : MultiOutUGen {
    *modeIndex { |mode|
        ^case { mode == \digital } { 0 } { mode == \circuit } { 1 }
        { Error("ESP mode must be \\digital or \\circuit.").throw }
    }
    *constants { |values|
        values.do { |x| if(x.isNumber.not) { Error("ESP parameters must be construction-time numbers.").throw } }
    }
    *kr { |in, lowCut=80, highCut=2000, cvAdjust=1, mode=\digital, inputGain=1,
        threshold=0, hysteresis=0.01, pulseWidth=0.0002, smoothTime=0.02,
        referenceMaxFreq=2000, referenceMaxEventRate=4000, timeout=0.1|
        var m=this.modeIndex(mode);
        this.constants([lowCut,highCut,cvAdjust,inputGain,threshold,hysteresis,pulseWidth,
            smoothTime,referenceMaxFreq,referenceMaxEventRate,timeout]);
        ^this.multiNewList([\control,4,in,lowCut,highCut,cvAdjust,m,inputGain,
            threshold,hysteresis,pulseWidth,smoothTime,referenceMaxFreq,referenceMaxEventRate,timeout])
    }
    *ar { Error("FreqToVolt supplies control-rate outputs; use .kr with audio input.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs,\control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
Periodicity2 : MultiOutUGen {
    *kr { |in, lowCut=80, highCut=2000, threshold=0, hysteresis=0.01,
        windowTime=0.25, minIntervals=4, jitterScale=0.15, timeout=0.1|
        FreqToVolt.constants([lowCut,highCut,threshold,hysteresis,windowTime,minIntervals,jitterScale,timeout]);
        ^this.multiNewList([\control,3,in,lowCut,highCut,threshold,hysteresis,
            windowTime,minIntervals,jitterScale,timeout])
    }
    *ar { Error("Periodicity2 is control-rate only.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs,\control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
EnvFollow2 : UGen {
    *kr { |in, inputGain=1, mode=\circuit, attackTime=0.005, releaseTime=0.08, referenceEnvVolts=5|
        var m=FreqToVolt.modeIndex(mode);
        FreqToVolt.constants([inputGain,attackTime,releaseTime,referenceEnvVolts]);
        ^this.multiNew(\control,in,inputGain,m,attackTime,releaseTime,referenceEnvVolts)
    }
    *ar { Error("EnvFollow2 is control-rate only.").throw }
    checkInputs { ^this.checkValidInputs }
}
TriggerDetector : MultiOutUGen {
    *kr { |envelope, threshold=0.4, hysteresis=0.01, mode=\circuit|
        var m=FreqToVolt.modeIndex(mode);
        FreqToVolt.constants([threshold,hysteresis]);
        if(envelope.rate == \audio) { Error("TriggerDetector expects a control-rate normalized envelope.").throw };
        ^this.multiNewList([\control,3,envelope,threshold,hysteresis,m])
    }
    *ar { Error("TriggerDetector is control-rate only.").throw }
    init { |numOutputs ... theInputs| inputs=theInputs; ^this.initOutputs(numOutputs,\control) }
    argNamesInputsOffset { ^2 }
    checkInputs { ^this.checkValidInputs }
}
