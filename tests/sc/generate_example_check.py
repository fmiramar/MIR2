#!/usr/bin/env python3
"""Generate a check of installed help graph construction, SCDoc and NRT audio.

Usage: python3 tests/sc/generate_example_check.py /path/to/check.scd [--nrt]
Run the generated script with sclang. Generated scores/audio live beside it.
"""
from pathlib import Path
import argparse,re,json
p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--nrt',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[2]
output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
files=sorted((root/'HelpSource/Classes').glob('*.schelp'))
lines=['(', 'var ok = true, definitions = List.new, names = List.new;', 'var buffer = 0, salience = 1;', '~mir2Buffer = 0;', 'SCDoc.indexAllDocuments(true);']
for path in files:
 text=path.read_text();match=re.search(r'~mir2\w*Example\s*=\s*\{(.*?)^\s*\}.play;',text,re.S|re.M)
 if not match:raise SystemExit('No audible example graph: '+path.name)
 name=path.stem;body=match.group(1)
 lines+=['try {',f'    var definition = SynthDef(\\example{name}, {{', '        var sound = {',body, '        }.value;', '        Out.ar(0, sound.asArray);', '    });', f'    var doc = SCDoc.documents["Classes/{name}"];', '    definition.asBytes;', '    if(doc.isNil) { Error("Missing help page").throw };', '    SCDoc.renderer.renderToFile(doc.destPath, doc, SCDoc.parseDoc(doc));','    definitions.add(definition);',f'    names.add("{name}");',f'    "EXAMPLE_GRAPH_OK {name}".postln;', f'}} {{ |error| "EXAMPLE_FAILED {name}".postln; error.reportError; ok = false }};']
lines+=['if(ok.not) { 1.exit };', f'"MIR2_EXAMPLE_GRAPHS_OK {len(files)}".postln;']
if not args.nrt:lines+=['0.exit;']
else:
 audio=json.dumps(str(output.with_suffix('.wav')))
 lines+=['''Routine({
    var options = ServerOptions.new;
    var events = List.new;
    var completed = FlowVar.new;
    var duration = 3.0;
    var start = 0.1;
    var file, data;
    options.numOutputBusChannels = 2;
    options.numInputBusChannels = 0;
    options.numAudioBusChannels = 128;
    options.numWireBufs = 2048;
    options.memSize = 262144;
    options.blockSize = 64;
    events.add([0.0, [\\b_allocRead, 0, ExampleFiles.child]]);
    events.add([0.0, [\\b_alloc, 1, 2048, 1]]);
    definitions.do { |definition, i|
        events.add([0.0, [\\d_recv, definition.asBytes]]);
        events.add([start + (i * duration), [\\s_new, definition.name, 1000 + i, 0, 0]]);
        events.add([start + ((i+1) * duration) - 0.001, [\\n_free, 1000 + i]]);
    };
    Score(events.asArray).recordNRT(
        outputFilePath: '''+audio+''',
        sampleRate: 48000, headerFormat: "wav", sampleFormat: "float",
        options: options, duration: start + (definitions.size * duration),
        action: { completed.value = true }
    );
    completed.value;
    file = SoundFile.openRead('''+audio+''');
    if(file.isOpen.not or: { file.numChannels != 2 }) { "NRT_FILE_FAILED".postln; 1.exit };
    data = FloatArray.newClear(file.numFrames * 2);
    file.readData(data); file.close;
    names.do { |name, i|
        var first = ((start + (i * duration) + 0.8) * 48000).asInteger * 2;
        var last = ((start + ((i+1) * duration) - 0.1) * 48000).asInteger * 2;
        var peak = 0.0, invalid = false;
        (first .. last).do { |index|
            var value = data[index];
            if(value.isNaN or: { value.abs > 100 }) { invalid = true };
            peak = peak.max(value.abs);
        };
        if(invalid or: { peak < 1e-7 }) {
            "NRT_EXAMPLE_FAILED % peak %".format(name, peak).postln; ok = false
        } { "NRT_EXAMPLE_OK % peak %".format(name, peak).postln };
    };
    if(ok) { "MIR2_EXAMPLE_NRT_OK".postln; 0.exit } { 1.exit };
}).play(AppClock);''']
lines+=[')'];output.write_text('\n'.join(lines)+'\n')
print(f'Generated {len(files)} example checks: {output}')
