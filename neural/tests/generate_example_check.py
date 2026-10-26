#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate an NRT check from PitchPESTO's actual audible help example.

Run the generated script with sclang -D. The first two rendered channels must
both be finite and audible; a third channel monitors the raw UGen outputs for
NaNs/infinities before the example's clipping and smoothing can conceal them.
"""
import argparse
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-script", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, required=True,
                        help="new output prefix for the .osc and .wav files")
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--sample-rate", type=int, choices=(16000, 44100, 48000, 96000), default=48000)
    parser.add_argument("--block-size", type=int, default=64)
    parser.add_argument("--server", choices=("scsynth", "supernova"), default="scsynth")
    args = parser.parse_args()
    if args.block_size < 1 or args.block_size > 8192 or args.block_size & (args.block_size - 1):
        parser.error("--block-size must be a power of two from 1 through 8192")
    output = args.output_script.resolve()
    prefix = args.prefix.resolve()
    model = args.model.resolve()
    if not model.is_file():
        parser.error("--model must be an existing exported model.wav")
    for target in (output, Path(str(prefix) + ".osc"), Path(str(prefix) + ".wav")):
        if target.exists():
            parser.error("Refusing to overwrite existing file: " + str(target))
    root = Path(__file__).resolve().parents[1]
    help_path = root / "HelpSource/Classes/PitchPESTO.schelp"
    source = help_path.read_text()
    match = re.search(r"~mir2PESTOExample\s*=\s*\{(.*?)^\s*\}\.play;", source, re.S | re.M)
    if not match or "PitchPESTO.kr" not in match.group(1) or "PlayBuf.ar" not in match.group(1):
        raise SystemExit("Cannot extract the audible PitchPESTO help graph")
    body = match.group(1)
    server_setup = ""
    if args.server == "supernova":
        server_setup = 'Server.supernova;\nScore.program = Score.program.replace("scsynth", "supernova");\n'
    script = r'''(
var definition;
~mir2Buffer = 0;
~mir2PESTOModel = 1;
__SERVER_SETUP__
definition = SynthDef(\mir2PESTOHelpExample, {
    var sound = {
__BODY__
    }.value;
    var units = UGen.buildSynthDef.children.copy.select { |ugen| ugen.class == PitchPESTO };
    var invalid = units.collect { |ugen|
        ugen.channels.collect { |signal|
            var check = CheckBadValues.ar(K2A.ar(signal), post: 0);
            (check > 0) * (check < 3)
        }
    }.flat;
    Out.ar(0, sound.asArray);
    Out.ar(2, Mix(invalid));
});
definition.asBytes;
"MIR2_NEURAL_EXAMPLE_GRAPH_OK".postln;
Routine({
    var options = ServerOptions.new, completed = FlowVar.new;
    var rate = __RATE__, block = __BLOCK__, start = 0.1, duration = 6.0;
    var path = __AUDIO_PATH__, modelPath = __MODEL_PATH__;
    var file, data, first, last, samples, ok = true;
    var energy = [0.0, 0.0], peak = [0.0, 0.0], rms, badPrimitive = 0.0;
    if(File.exists(modelPath).not) { "NEURAL_EXAMPLE_MODEL_MISSING".postln; 1.exit };
    options.numOutputBusChannels = 3;
    options.numInputBusChannels = 0;
    options.numAudioBusChannels = 32;
    options.numWireBufs = 256;
    options.memSize = 32768;
    options.blockSize = block;
    Score([
        [0.0, [\b_allocRead, 0, ExampleFiles.child]],
        [0.0, [\b_allocRead, 1, modelPath]],
        [0.0, [\d_recv, definition.asBytes]],
        [start, [\s_new, definition.name, 1000, 0, 0]],
        [start + duration, [\n_free, 1000]]
    ]).recordNRT(oscFilePath: __OSC_PATH__, outputFilePath: path,
        sampleRate: rate, headerFormat: "wav", sampleFormat: "float",
        options: options, duration: start + duration + 0.1,
        action: { completed.value = true });
    completed.value;
    file = SoundFile.openRead(path);
    if(file.isNil or: { file.numChannels != 3 }) {
        "NEURAL_EXAMPLE_RENDER_FAILED".postln; 1.exit
    };
    data = FloatArray.newClear(file.numFrames * 3);
    file.readData(data); file.close;
    // Include startup in the finite check; omit it from the audibility check.
    data.do { |value|
        if(value.isNaN or: { value.abs > 100 }) { ok = false };
    };
    first = ((start + 0.8) * rate).asInteger;
    last = ((start + duration - 0.02) * rate).asInteger;
    if((last <= first) or: { ((last + 1) * 3) > data.size }) {
        "NEURAL_EXAMPLE_SHORT_RENDER".postln; 1.exit
    };
    samples = last - first + 1;
    (first .. last).do { |frame|
        2.do { |channel|
            var value = data[(frame * 3) + channel];
            energy[channel] = energy[channel] + (value * value);
            peak[channel] = peak[channel].max(value.abs);
        };
        badPrimitive = badPrimitive.max(data[(frame * 3) + 2]);
    };
    rms = energy.collect { |sum| (sum / samples).sqrt };
    if((rms[0] < 1e-7) or: { rms[1] < 1e-7 } or: { badPrimitive > 0 }) { ok = false };
    ["PitchPESTO help example", "rate", rate, "block", block,
        "rms L/R", rms, "peak L/R", peak, "bad primitive", badPrimitive].postln;
    if(ok) { "MIR2_NEURAL_EXAMPLE_NRT_OK".postln; 0.exit } {
        "MIR2_NEURAL_EXAMPLE_NRT_FAILED".postln; 1.exit
    };
}).play(AppClock);
)
'''
    replacements = {
        "__SERVER_SETUP__": server_setup,
        "__BODY__": body,
        "__RATE__": str(args.sample_rate),
        "__BLOCK__": str(args.block_size),
        "__MODEL_PATH__": json.dumps(str(model)),
        "__AUDIO_PATH__": json.dumps(str(prefix) + ".wav"),
        "__OSC_PATH__": json.dumps(str(prefix) + ".osc"),
    }
    for key, value in replacements.items():
        script = script.replace(key, value)
    output.parent.mkdir(parents=True, exist_ok=True)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x") as stream:
        stream.write(script)
    print("Generated PitchPESTO help example check: " + str(output))


if __name__ == "__main__":
    main()
