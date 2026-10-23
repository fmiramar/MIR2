(
var sourceRoot = Pitch2.filenameSymbol.asString.dirname.dirname +/+ "HelpSource";
var pages = ["Classes", "Guides"].collect { |folder|
    PathName(sourceRoot +/+ folder).files.select { |p| p.extension == "schelp" }
        .collect { |p| folder +/+ p.fileNameWithoutExtension }
}.flat;
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

