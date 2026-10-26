// SPDX-License-Identifier: GPL-3.0-or-later
PitchPESTO : MultiOutUGen {
    *kr { |in, model, threshold = 0.8|
        ^this.multiNew(\control, in, model, threshold)
    }
    *ar { Error("PitchPESTO has control-rate outputs; its input must be audio-rate.").throw }
    init { |... theInputs|
        inputs = theInputs;
        ^this.initOutputs(4, \control)
    }
    checkInputs {
        if(inputs[0].rate != \audio) { ^"PitchPESTO input must be audio-rate" };
        ^this.checkValidInputs
    }
}
