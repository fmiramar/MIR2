(
var pages = [
    "Classes/SpecCentroid2", "Classes/SpecFlatness2", "Classes/SpecPcile2",
    "Classes/MFCC2", "Classes/FFTFlux2", "Classes/Onsets2",
    "Classes/PitchYIN", "Classes/PitchMPM", "Classes/Periodicity",
    "Classes/Voicing", "Classes/Pitch2",
    "Guides/MIR2", "Guides/MIR2-legacy-migration", "Guides/MIR2-and-FluCoMa"
];
var ok = true;

SCDoc.verbosity = 2;
SCDoc.indexAllDocuments(true);
pages.do { |path|
    var document = SCDoc.documents[path];
    if(document.isNil) {
        ("MISSING_DOC " ++ path).postln;
        ok = false
    } {
        var root = SCDoc.parseDoc(document);
        if(root.isNil) {
            ("PARSE_FAILED " ++ path).postln;
            ok = false
        } {
            SCDoc.renderer.renderToFile(document.destPath, document, root);
            ("RENDERED " ++ path).postln
        }
    }
};

if(ok) {
    "MIR2_SCDOC_OK".postln;
    0.exit
} {
    1.exit
};
)

