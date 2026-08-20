(
var names = [
    \SpecCentroid2, \SpecFlatness2, \SpecPcile2, \MFCC2, \FFTFlux2, \Onsets2,
    \PitchYIN, \PitchMPM, \Periodicity, \Voicing, \Pitch2,
    \MIR2FFTAnalyzer, \MIR2PitchAnalyzer
];
var ok = true;

names.do { |name|
    var class = name.asClass;
    if(class.isNil) {
        ("MISSING_CLASS " ++ name).postln;
        ok = false
    } {
        if(class.filenameSymbol.asString.contains("MIR2").not) {
            "CLASS_COLLISION % at %".format(name, class.filenameSymbol).postln;
            ok = false
        }
    }
};

if(ok) {
    "MIR2_COLLISION_CHECK_OK".postln;
    0.exit
} {
    1.exit
};
)

