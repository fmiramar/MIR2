// Installed numerical checks for the optional research APIs and old SynthDef inputs.
// sclang -D research_contracts.scd /absolute/ignored/output-prefix 48000 scsynth
(
var args = thisProcess.argv;
var prefix = args[0], sampleRate = (args[1] ? "48000").asInteger;
var server = args[2] ? "scsynth";
var definition;
if(prefix.isNil) { "Output prefix is required".postln; 1.exit };
if(server == "supernova") {
    Server.supernova;
    Score.program = Score.program.replace("scsynth", "supernova");
};
if(ChordTrack.label(0, 5) != "C:maj7" or: {ChordTrack.qualityNames(\extended).size != 9}
    or: {ChordTrack.qualityNames.size != 2}) { "CHORD_LABEL_FAILED".postln; 1.exit };
definition = SynthDef(\mir2ResearchContracts, {
    var source = SinOsc.ar(220, 0, 0.2);
    var chroma = ChromaCQT.kr(source, fmin: 110, normalize: \sum);
    var quiet = ChromaCQT.kr(DC.ar(0), fmin: 110, normalize: \l2);
    var bands = [(SinOsc.kr(2, pi/2) * 0.4) + 1, (SinOsc.kr(5, pi/2) * 0.7) + 2];
    var modulation = RhythmTransform.kr(bands, numBins: 13, minHz: 1, maxHz: 7,
        windowTime: 2, phase: 1);
    var pattern = RhythmPattern.krBands(bands, numBins: 13, minHz: 1, maxHz: 7,
        windowTime: 2, normalization: \global);
    var chord = ChordTrack.kr([1,0,0,0,1,0,0,1,0,0,0,1], smooth: 0,
        transitionPenalty: 0, vocabulary: \extended);
    var stats = ChordStats.kr(0, LFPulse.kr(4) * 5, vocabulary: \extended);
    var reconstructed = WaveletDaub2.ar(source, frameSize: 1024, order: 8, boundary: \symmetric);
    // Exact previous SynthDef wire shape: no appended boundary input.
    var legacy = WaveletDaub2.multiNew(\audio, source, 1024, 4, 6, 0);
    var values = [chroma[9], chroma.sum, quiet.sum, modulation[2], modulation[21],
        modulation[28], modulation[47], pattern.squared.sum, chord[0], chord[1],
        chord[2], stats[0], stats[2]].collect { |x| K2A.ar(x) };
    Out.ar(0, values ++ [reconstructed, legacy, source]);
});
Routine({
    var options = ServerOptions.new;
    var done = FlowVar.new, file, data, ok = true;
    var channels = 16, minima = Array.fill(16, inf), maxima = Array.fill(16, -inf);
    var residual = [0.0, 0.0];
    var previousPhase = [nil, nil], phaseError = [0.0, 0.0], phaseUpdates = [0, 0];
    var actualRate, hopFrames, phaseAdvance;
    var expectedEntropy = log(2) / log(109);
    var require = { |condition, message|
        if(condition.not) { ("RESEARCH_CONTRACT_FAILED " ++ message).postln; ok = false };
    };
    options.numOutputBusChannels = channels;
    options.numInputBusChannels = 0;
    options.numAudioBusChannels = 64;
    options.numWireBufs = 2048;
    options.memSize = 262144;
    options.blockSize = if(sampleRate == 44100) {32} {if(sampleRate == 96000) {128} {64}};
    actualRate = sampleRate / (options.blockSize * ((sampleRate / options.blockSize) / 100).round.max(1));
    hopFrames = (0.1 * actualRate).ceil;
    phaseAdvance = [2, 5] * (2 * pi * (hopFrames / actualRate));
    Score([
        [0, [\d_recv, definition.asBytes]],
        [0.1, [\s_new, definition.name, 1000, 0, 0]],
        [4.099, [\n_free, 1000]]
    ]).recordNRT(oscFilePath: prefix ++ ".osc", outputFilePath: prefix ++ ".wav",
        sampleRate: sampleRate, headerFormat: "wav", sampleFormat: "float",
        options: options, duration: 4.1, action: {done.value = true});
    done.value;
    file = SoundFile.openRead(prefix ++ ".wav");
    if(file.isOpen.not or: {file.numChannels != channels}) { "RESEARCH_FILE_FAILED".postln; 1.exit };
    data = FloatArray.newClear(file.numFrames * channels);
    file.readData(data); file.close;
    ((3.1 * sampleRate).asInteger .. (4.08 * sampleRate).asInteger).do { |frame|
        channels.do { |channel|
            var x = data[(frame * channels) + channel];
            if(x.isNaN or: {x.abs > 100}) {ok = false};
            minima[channel] = minima[channel].min(x);
            maxima[channel] = maxima[channel].max(x);
        };
        [13, 14].do { |channel, index|
            var reference = data[((frame - 1024) * channels) + 15];
            residual[index] = residual[index].max((data[(frame * channels) + channel] - reference).abs);
        };
        // K2A interpolates inside a block. At the next block's first sample,
        // its previous control value is exact; inspect that value only.
        if((frame % options.blockSize) == 0) {
            [5, 6].do { |channel, index|
                var phase = data[(frame * channels) + channel], previous = previousPhase[index];
                if(previous.notNil and: {(phase - previous).abs > 1e-5}) {
                    var error = ((phase - previous) - phaseAdvance[index]).wrap(0 - pi, pi).abs;
                    phaseError[index] = phaseError[index].max(error);
                    phaseUpdates[index] = phaseUpdates[index] + 1;
                };
                previousPhase[index] = phase;
            };
        };
    };
    require.value(minima[0] > 0.6, "A-class power");
    require.value((minima[1] - 1).abs < 1e-5 and: {(maxima[1] - 1).abs < 1e-5}, "L1 chroma");
    require.value(maxima[2] == 0, "silent chroma");
    [3, 4].do { |channel, index|
        var target = [0.4, 0.7][index];
        require.value((minima[channel] - target).abs < 0.02 and: {(maxima[channel] - target).abs < 0.02}, "AM amplitude");
    };
    [5, 6].do { |channel, index|
        require.value(minima[channel] >= (0 - pi - 1e-5) and: {maxima[channel] <= (pi + 1e-5)}
            and: {phaseUpdates[index] >= 6} and: {phaseError[index] < 0.003}, "phase matrix advance/range");
    };
    require.value((minima[7] - 1).abs < 1e-5 and: {(maxima[7] - 1).abs < 1e-5}, "pattern global norm");
    require.value(minima[8] == 0 and: {maxima[8] == 0} and: {minima[9] == 5}
        and: {maxima[9] == 5} and: {(minima[10] - 1).abs < 1e-5}, "extended Cmaj7");
    require.value((minima[11] - expectedEntropy).abs < 0.005
        and: {(maxima[11] - expectedEntropy).abs < 0.005} and: {maxima[12] == 0}, "109-state chord statistics");
    residual.do { |error|
        require.value(error < 2e-6, "wavelet reconstruction/delay");
    };
    ["channel minima", minima].postln;
    ["channel maxima", maxima].postln;
    ["reconstruction error", residual, "phase advance error", phaseError, "phase updates", phaseUpdates].postln;
    if(ok) { "MIR2_RESEARCH_CONTRACTS_OK".postln; 0.exit } { 1.exit };
}).play(AppClock);
)
