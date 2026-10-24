#!/usr/bin/env python3
"""Generate installed help graph, SCDoc and instrumented NRT audio checks.

Run the generated script with sclang. The third audio channel records invalid
MIR2 primitive outputs before example clipping/smoothing can conceal them.
"""
from pathlib import Path
import argparse
import json
import re

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
parser.add_argument('--classes', nargs='+', help='Restrict verification to named public classes')
parser.add_argument('--server', choices=['scsynth', 'supernova'], default='scsynth')
parser.add_argument('--nrt', action='store_true')
parser.add_argument('--sample-rate', type=int, default=48000)
parser.add_argument('--block-size', type=int, default=64)
parser.add_argument('--duration', type=float, default=4.0)
parser.add_argument('--silence', action='store_true', help='Silence each PlayBuf source; check finite outputs, not audibility')
parser.add_argument('--skip-docs', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
output = args.output.resolve()
output.parent.mkdir(parents=True, exist_ok=True)
files = sorted((root / 'HelpSource/Classes').glob('*.schelp'))
if args.classes:
    files = [p for p in files if p.stem in args.classes]
    if len(files) != len(args.classes):
        raise SystemExit('Requested example class not found')
lines = ['(', 'var ok = true, definitions = List.new, names = List.new;',
         'var buffer = 0, salience = 1;', '~mir2Buffer = 0;']
if args.server == 'supernova':
    lines += ['Server.supernova;', 'Score.program = Score.program.replace("scsynth", "supernova");']
if not args.skip_docs:
    lines.append('SCDoc.indexAllDocuments(true);')


def silence_playback(body):
    """Wrap balanced PlayBuf calls, including nested rate/buffer expressions."""
    start = 0
    while True:
        first = body.find('PlayBuf.ar(', start)
        if first < 0:
            return body
        cursor = first + len('PlayBuf.ar(')
        depth = 1
        while depth:
            if body[cursor] == '(':
                depth += 1
            elif body[cursor] == ')':
                depth -= 1
            cursor += 1
        body = body[:first] + 'DC.ar(0)' + body[cursor:]
        start = first + len('DC.ar(0)')


for index, path in enumerate(files):
    if index % 16 == 0:
        lines += ['{']
    text = path.read_text()
    match = re.search(r'~mir2\w*Example\s*=\s*\{(.*?)^\s*\}.play;', text, re.S | re.M)
    if not match:
        raise SystemExit('No audible example graph: ' + path.name)
    name, body = path.stem, match.group(1)
    if args.silence:
        body = silence_playback(body)
    lines += ['try {', f'    var definition = SynthDef(\\example{name}, {{',
              '        var sound = {', body, '        }.value;',
              '''        var units = UGen.buildSynthDef.children.copy.select { |ugen|
            ugen.class.filenameSymbol.asString.find("/MIR2/Classes/").notNil
        };
        var invalid = units.collect { |ugen|
            var signals = if(ugen.isKindOf(MultiOutUGen)) { ugen.channels } { [ugen] };
            signals.collect { |signal|
                var audio = if(signal.rate == \\audio) { signal } { K2A.ar(signal) };
                var check = CheckBadValues.ar(audio, post: 0);
                (check > 0) * (check < 3)
            }
        }.flat;
        Out.ar(0, sound.asArray);
        Out.ar(2, Mix(invalid));
    });''']
    if not args.skip_docs:
        lines += [f'    var doc = SCDoc.documents["Classes/{name}"];']
    lines += ['    definition.asBytes;']
    if not args.skip_docs:
        lines += ['    if(doc.isNil) { Error("Missing help page").throw };',
                  '    SCDoc.renderer.renderToFile(doc.destPath, doc, SCDoc.parseDoc(doc));']
    lines += ['    definitions.add(definition);', f'    names.add("{name}");',
              f'    "EXAMPLE_GRAPH_OK {name}".postln;',
              f'}} {{ |error| "EXAMPLE_FAILED {name}".postln; error.reportError; ok = false }};']
    if index % 16 == 15 or index == len(files) - 1:
        lines += ['}.value;']
lines += ['if(ok.not) { 1.exit };', 'if(ok) {', f'"MIR2_EXAMPLE_GRAPHS_OK {len(files)}".postln;']
if not args.nrt:
    lines += ['0.exit;']
else:
    audio = json.dumps(str(output.with_suffix('.wav')))
    lines += ['''Routine({
    var options = ServerOptions.new;
    var events = List.new;
    var completed = FlowVar.new;
    var duration = DURATION;
    var sampleRate = SAMPLE_RATE;
    var start = 0.1;
    var file, data;
    options.numOutputBusChannels = 3;
    options.numInputBusChannels = 0;
    options.numAudioBusChannels = 128;
    options.numWireBufs = 4096;
    options.memSize = 262144;
    options.blockSize = BLOCK_SIZE;
    events.add([0.0, [\\b_allocRead, 0, ExampleFiles.child]]);
    events.add([0.0, [\\b_alloc, 1, 2048, 1]]);
    definitions.do { |definition, i|
        events.add([0.0, [\\d_recv, definition.asBytes]]);
        events.add([start + (i * duration), [\\s_new, definition.name, 1000 + i, 0, 0]]);
        events.add([start + ((i+1) * duration) - 0.001, [\\n_free, 1000 + i]]);
    };
    Score(events.asArray).recordNRT(
        oscFilePath: OSC_PATH,
        outputFilePath: AUDIO_PATH,
        sampleRate: sampleRate, headerFormat: "wav", sampleFormat: "float",
        options: options, duration: start + (definitions.size * duration),
        action: { completed.value = true }
    );
    completed.value;
    file = SoundFile.openRead(AUDIO_PATH);
    if(file.isOpen.not or: { file.numChannels != 3 }) { "NRT_FILE_FAILED".postln; 1.exit };
    data = FloatArray.newClear(file.numFrames * 3);
    file.readData(data); file.close;
    names.do { |name, i|
        var first = ((start + (i * duration)) * sampleRate).asInteger;
        var last = ((start + ((i+1) * duration) - 0.01) * sampleRate).asInteger;
        var peak = 0.0, invalid = false, badPrimitive = 0.0;
        (first .. last).do { |frame|
            2.do { |channel|
                var value = data[(frame * 3) + channel];
                if(value.isNaN or: { value.abs > 100 }) { invalid = true };
                if(frame > (first + (sampleRate * 0.8))) { peak = peak.max(value.abs) };
            };
            badPrimitive = badPrimitive.max(data[(frame * 3) + 2]);
        };
        if(invalid or: { badPrimitive > 0 } or: { REQUIRE_SOUND and: { peak < 1e-7 } }) {
            "NRT_EXAMPLE_FAILED % peak % badPrimitive %".format(name, peak, badPrimitive).postln; ok = false
        } { "NRT_EXAMPLE_OK % peak % badPrimitive %".format(name, peak, badPrimitive).postln };
    };
    if(ok) { "MIR2_EXAMPLE_NRT_OK".postln; 0.exit } { 1.exit };
}).play(AppClock);'''.replace('DURATION', str(args.duration)).replace('SAMPLE_RATE', str(args.sample_rate))
        .replace('BLOCK_SIZE', str(args.block_size)).replace('AUDIO_PATH', audio)
        .replace('OSC_PATH', json.dumps(str(output.with_suffix('.osc'))))
        .replace('REQUIRE_SOUND', 'false' if args.silence else 'true')]
lines += ['};', ')']
output.write_text('\n'.join(lines) + '\n')
print(f'Generated {len(files)} example checks: {output}')
