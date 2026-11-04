(
var root = Pitch2.filenameSymbol.asString.dirname.dirname;
var names = PathName(root +/+ "HelpSource/Classes").files.select { |p| p.extension == "schelp" }.collect { |p| p.fileNameWithoutExtension.asSymbol };
var ok = true;
names.do { |name|
    var class = name.asClass;
    if(class.isNil or: { class.filenameSymbol.asString.beginsWith(root).not }) {
        ("MISSING_OR_COLLIDING_CLASS " ++ name).postln;
        ok = false;
    };
};
if(ok) { "MIRA_COLLISION_CHECK_OK".postln; 0.exit } { 1.exit };
)
