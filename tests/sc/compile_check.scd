(
var required = [
    SpecCentroid2, SpecFlatness2, SpecPcile2, MFCC2, FFTFlux2, Onsets2,
    PitchYIN, PitchMPM, Periodicity, Voicing, Pitch2
];
if(required.any(_.isNil)) {
    Error("MIR2 class compilation failed").throw
};
"MIR2_COMPILE_OK".postln;
0.exit
)

