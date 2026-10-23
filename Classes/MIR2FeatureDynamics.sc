// SPDX-License-Identifier: GPL-3.0-or-later

MIR2FeatureAnalyzer : UGen {
    *validate { |size, maximum|
        if(size.isInteger.not or: { size < 1 } or: { size > maximum }) {
            Error("MIR2 feature: window size must be a literal integer in the documented range.").throw
        }
    }
    *ar { Error("MIR2 feature utilities produce control-rate outputs only.").throw }
    checkInputs {
        if(inputs.any { |input| input.rate == \audio }) { ^"MIR2 feature utilities accept control or scalar inputs." };
        ^this.checkValidInputs
    }
}

FeatureDelta : MIR2FeatureAnalyzer {
    *kr { |in, radius = 2, trigger = -1, frameTime = 0|
        MIR2FeatureAnalyzer.validate(radius, 32);
        ^this.multiNew(\control, in, radius, trigger, frameTime)
    }
}

FeatureDeltaDelta : MIR2FeatureAnalyzer {
    *kr { |in, radius = 2, trigger = -1, frameTime = 0|
        MIR2FeatureAnalyzer.validate(radius, 32);
        ^this.multiNew(\control, in, radius, trigger, frameTime)
    }
}

FeatureMedian : MIR2FeatureAnalyzer {
    *kr { |in, size = 5, trigger = -1, frameTime = 0|
        MIR2FeatureAnalyzer.validate(size, 127);
        ^this.multiNew(\control, in, size, trigger, frameTime)
    }
}

FeatureSmooth : MIR2FeatureAnalyzer {
    *kr { |in, time = 0.1, trigger = -1, frameTime = 0|
        ^this.multiNew(\control, in, time, trigger, frameTime)
    }
}

