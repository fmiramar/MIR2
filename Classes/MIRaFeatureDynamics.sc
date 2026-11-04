// SPDX-License-Identifier: GPL-3.0-or-later

MIRaFeatureAnalyzer : UGen {
    *validate { |size, maximum|
        if(size.isInteger.not or: { size < 1 } or: { size > maximum }) {
            Error("MIRa feature: window size must be a literal integer in the documented range.").throw
        }
    }
    *ar { Error("MIRa feature utilities produce control-rate outputs only.").throw }
    checkInputs {
        if(inputs.any { |input| input.rate == \audio }) { ^"MIRa feature utilities accept control or scalar inputs." };
        ^this.checkValidInputs
    }
}

FeatureDelta : MIRaFeatureAnalyzer {
    *kr { |in, radius = 2, trigger = -1, frameTime = 0|
        MIRaFeatureAnalyzer.validate(radius, 32);
        ^this.multiNew(\control, in, radius, trigger, frameTime)
    }
}

FeatureDeltaDelta : MIRaFeatureAnalyzer {
    *kr { |in, radius = 2, trigger = -1, frameTime = 0|
        MIRaFeatureAnalyzer.validate(radius, 32);
        ^this.multiNew(\control, in, radius, trigger, frameTime)
    }
}

FeatureMedian : MIRaFeatureAnalyzer {
    *kr { |in, size = 5, trigger = -1, frameTime = 0|
        MIRaFeatureAnalyzer.validate(size, 127);
        ^this.multiNew(\control, in, size, trigger, frameTime)
    }
}

FeatureSmooth : MIRaFeatureAnalyzer {
    *kr { |in, time = 0.1, trigger = -1, frameTime = 0|
        ^this.multiNew(\control, in, time, trigger, frameTime)
    }
}

