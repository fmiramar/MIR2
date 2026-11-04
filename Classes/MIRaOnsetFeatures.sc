// SPDX-License-Identifier: GPL-3.0-or-later
FFTComplexDev2 : UGen {
    *kr { |chain, rectify = 0, normalize = 1, floor = 1e-6, fftSize = 1024|
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        ^this.multiNew(\control, chain, rectify, normalize, floor, fftSize)
    }
    checkInputs { ^this.checkValidInputs }
}
FFTPhaseDev2 : UGen {
    *kr { |chain, weighted = 0, normalize = 1, floor = 1e-6, fftSize = 1024|
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        ^this.multiNew(\control, chain, weighted, normalize, floor, fftSize)
    }
    checkInputs { ^this.checkValidInputs }
}
FFTMKL2 : UGen {
    *kr { |chain, epsilon = 1e-6, normalize = 1, fftSize = 1024|
        MIRaFFTAnalyzer.validateFFTSize(fftSize);
        ^this.multiNew(\control, chain, 0, normalize, epsilon, fftSize)
    }
    checkInputs { ^this.checkValidInputs }
}
FFTFluxPos2 {
    *kr { |chain, normalize = 1, spectrumMode = 0, minFreq = 0, maxFreq = 0, fftSize = 1024|
        ^FFTFlux2.kr(chain, normalize, 1, spectrumMode, minFreq, maxFreq, fftSize)
    }
}
