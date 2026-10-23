// SPDX-License-Identifier: GPL-3.0-or-later

MIR2StereoAnalyzer : UGen {
    *validate { |windowTime|
        if(windowTime.isNumber.not or: { windowTime < 0.001 } or: { windowTime > 2 }) {
            Error("MIR2 stereo: windowTime must be a literal number from 0.001 through 2 seconds.").throw
        }
    }
    *ar { Error("MIR2 stereo analyzers produce control-rate outputs only.").throw }
    checkInputs {
        if(inputs[0].rate != \audio or: { inputs[1].rate != \audio }) {
            ^"MIR2 stereo analyzers require two audio inputs."
        };
        ^this.checkValidInputs
    }
}

StereoBalance : MIR2StereoAnalyzer {
    *kr { |left, right, windowTime = 0.05|
        MIR2StereoAnalyzer.validate(windowTime);
        ^this.multiNew(\control, left, right, windowTime)
    }
}

StereoWidth : MIR2StereoAnalyzer {
    *kr { |left, right, windowTime = 0.05|
        MIR2StereoAnalyzer.validate(windowTime);
        ^this.multiNew(\control, left, right, windowTime)
    }
}

StereoCorrelation : MIR2StereoAnalyzer {
    *kr { |left, right, windowTime = 0.05|
        MIR2StereoAnalyzer.validate(windowTime);
        ^this.multiNew(\control, left, right, windowTime)
    }
}

MidSideRatio : MIR2StereoAnalyzer {
    *kr { |left, right, windowTime = 0.05|
        MIR2StereoAnalyzer.validate(windowTime);
        ^this.multiNew(\control, left, right, windowTime)
    }
}

StereoCoherence : MIR2StereoAnalyzer {
    *kr { |left, right, frameSize = 1024, hop = 0.5, averageTime = 0.2, minFreq = 0, maxFreq = 0|
        MIR2FFTAnalyzer.validateFFTSize(frameSize);
        if(hop.isNumber.not or: { hop < 0.0625 } or: { hop > 1 }) {
            Error("MIR2 stereo: hop must be a literal frame fraction from 0.0625 through 1.").throw
        };
        ^this.multiNew(\control, left, right, frameSize, hop, averageTime, minFreq, maxFreq)
    }
}

StereoPanning : MIR2StereoAnalyzer {
    *kr { |left, right, frameSize = 1024, hop = 0.5, averageTime = 0.2, minFreq = 0, maxFreq = 0|
        MIR2FFTAnalyzer.validateFFTSize(frameSize);
        if(hop.isNumber.not or: { hop < 0.0625 } or: { hop > 1 }) {
            Error("MIR2 stereo: hop must be a literal frame fraction from 0.0625 through 1.").throw
        };
        ^this.multiNew(\control, left, right, frameSize, hop, averageTime, minFreq, maxFreq)
    }
}

InterchannelPhase : MultiOutUGen {
    *kr { |left, right, frameSize = 1024, hop = 0.5, averageTime = 0.2, minFreq = 0, maxFreq = 0|
        MIR2FFTAnalyzer.validateFFTSize(frameSize);
        if(hop.isNumber.not or: { hop < 0.0625 } or: { hop > 1 }) {
            Error("MIR2 stereo: hop must be a literal frame fraction from 0.0625 through 1.").throw
        };
        ^this.multiNewList([\control, left, right, frameSize, hop, averageTime, minFreq, maxFreq])
    }
    init { |... theInputs| inputs = theInputs; ^this.initOutputs(2, \control) }
    *ar { Error("InterchannelPhase is control-rate only.").throw }
    checkInputs {
        if(inputs[0].rate != \audio or: { inputs[1].rate != \audio }) { ^"InterchannelPhase requires two audio inputs." };
        ^this.checkValidInputs
    }
}

