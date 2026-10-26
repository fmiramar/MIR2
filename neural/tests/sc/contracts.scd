// sclang -D contracts.scd output-prefix model.wav sample-rate block-size scsynth|supernova
(
var args = thisProcess.argv;
var prefix = args[0], modelPath = args[1];
var rate = (args[2] ? "48000").asInteger, block = (args[3] ? "64").asInteger;
var definition;
if(prefix.isNil or: {modelPath.isNil}) { "Need output prefix and model path".postln; 1.exit };
if((args[4] ? "scsynth") == "supernova") {
    Server.supernova;
    Score.program = Score.program.replace("scsynth", "supernova");
};
definition = SynthDef(\mir2NeuralContracts, {
    var source = SinOsc.ar(440, 0, 0.2);
    var result = PitchPESTO.kr(source, 0, 0.8);
    var silent = PitchPESTO.kr(DC.ar(0), 0, 0.8);
    var invalid = PitchPESTO.kr(source, 1, 0.8);
    Out.ar(0, (result ++ silent ++ invalid).collect { |x| K2A.ar(x) } ++ [source]);
});
Routine({
    var options = ServerOptions.new, done = FlowVar.new;
    var data, file, ok = true, channels = 13, firstPitch;
    var require = { |condition, message|
        if(condition.not) {
            if(ok) { ("NEURAL_CONTRACT_FAILED " ++ message).postln };
            ok = false
        };
    };
    var window = if(rate == 16000) {4096} {if(rate == 96000) {16384} {8192}};
    var hop = (rate / 100).round.asInteger;
    var warmup = (((window / hop).ceil + 1) * hop) / rate;
    options.numOutputBusChannels = channels;
    options.numInputBusChannels = 0;
    options.numAudioBusChannels = 32;
    options.numWireBufs = 128;
    options.memSize = 32768;
    options.blockSize = block;
    Score([
        [0, [\b_allocRead, 0, modelPath]],
        [0, [\b_alloc, 1, 32, 1]],
        [0, [\d_recv, definition.asBytes]],
        [0.1, [\s_new, definition.name, 1000, 0, 0]],
        [1.5, [\b_free, 0]], // deliberately violate lifetime: fail silent, do not crash
        [1.8, [\n_free, 1000]]
    ]).recordNRT(oscFilePath: prefix ++ ".osc", outputFilePath: prefix ++ ".wav",
        sampleRate: rate, headerFormat: "wav", sampleFormat: "float",
        options: options, duration: 1.9, action: {done.value = true});
    done.value;
    file = SoundFile.openRead(prefix ++ ".wav");
    require.value(file.notNil and: {file.numChannels == channels}, "render format");
    if(file.isNil) {1.exit};
    data = FloatArray.newClear(file.numFrames * channels);
    file.readData(data); file.close;
    data.do { |value| if(value.isNaN or: {value.abs > 1e6}) {ok = false} };
    (0 .. ((1.45 * rate).asInteger)).do { |frame|
        var x = data[frame * channels];
        if(firstPitch.isNil and: {x > 1}) {firstPitch = frame / rate};
    };
    require.value(firstPitch.notNil, "no voiced estimate");
    if(firstPitch.notNil) {
        require.value((firstPitch - (0.1 + warmup)).abs < (3 * block / rate), "warmup timing");
    };
    ((0.7 * rate).asInteger .. (1.4 * rate).asInteger).do { |frame|
        var base = frame * channels;
        require.value((data[base] - 440).abs < 0.02, "440 Hz");
        require.value(data[base + 1] > 0.99 and: {data[base + 2] == 1}, "voicing");
        require.value(data[base + 3] > 0, "energy");
        require.value(data[base + 4] == 0 and: {data[base + 6] == 0}, "silence gating");
        require.value(data[base + 5] < 0.2 and: {data[base + 7] < 1e-10}, "silence evidence");
        (8..11).do { |c| require.value(data[base + c] == 0, "invalid model"); };
    };
    ((1.55 * rate).asInteger .. (1.75 * rate).asInteger).do { |frame|
        12.do { |c| require.value(data[(frame * channels) + c] == 0, "freed model"); };
    };
    ["first pitch", firstPitch, "expected", 0.1 + warmup, "rate/block", rate, block].postln;
    if(ok) {"MIR2_NEURAL_CONTRACTS_OK".postln; 0.exit} {1.exit};
}).play(AppClock);
)
