// Force index rebuild and render the installed class and shared project guide.
// The runner must also reject every SCDoc warning/error in the captured log.
(
var pages = ["Classes/PitchPESTO", "Guides/MIRa"];
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
if(ok) { "MIR2_NEURAL_SCDOC_OK".postln; 0.exit } { 1.exit };
)
