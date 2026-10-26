// Muted live-server CPU, shared-model and lifecycle checks.
// sclang -D live_runtime.scd model.wav output.csv scsynth|supernova port
(
var args = thisProcess.argv, modelPath = args[0], outputPath = args[1];
var kind = args[2] ? "scsynth", port = (args[3] ? "57181").asInteger;
var server, responder, expected = IdentityDictionary.new, replyCounts = IdentityDictionary.new;
var monitoring = false, badFrames = 0, maximumPitchError = 0.0;
if(modelPath.isNil or: {outputPath.isNil} or: {File.exists(modelPath).not}
    or: {File.exists(outputPath)} or: {["scsynth", "supernova"].includes(kind).not}) {
    "Need an exported 48 kHz model and a new output CSV".postln; 1.exit
};
if(kind == "supernova") {Server.supernova} {Server.scsynth};
server = Server(\mir2NeuralRuntime, NetAddr("127.0.0.1", port));
server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 32;
server.options.numWireBufs = 128;
server.options.blockSize = 64;
server.options.hardwareBufferSize = 256;
server.options.sampleRate = 48000;
server.options.memSize = 32768;
responder = OSCFunc({ |message|
    var target = expected[message[1]], values = message.copyRange(3, 6);
    if(monitoring and: {target.notNil}) {
        replyCounts[message[1]] = (replyCounts[message[1]] ? 0) + 1;
        if(values.size != 4 or: {values.any { |v| v.isNaN or: {v.abs > 1e6} }}) {
            badFrames = badFrames + 1
        } {
            if(target > 0) {
                maximumPitchError = maximumPitchError.max((values[0] - target).abs);
                if((values[0] - target).abs > 0.02 or: {values[1] < 0.99} or: {values[2] != 1}) {
                    badFrames = badFrames + 1
                }
            } {
                if(values[0] != 0 or: {values[2] != 0} or: {values[1] > 0.2}) {
                    badFrames = badFrames + 1
                }
            }
        }
    }
}, '/mir2/neural/runtime', server.addr);
server.waitForBoot({
    Routine({
        var model, group, nodes = List.new, file, phase;
        var ok = true, summaries = List.new, totalReplies = 0;
        try {
            if(server.sampleRate != 48000) {Error("Live check requires actual 48000 Hz").throw};
            file = File(outputPath, "w");
            file.write("phase,seconds,instances,average_cpu_percent,peak_cpu_percent,synths\n");
            model = Buffer.read(server, modelPath);
            SynthDef(\mir2NeuralRuntimeUnit, { |modelBuf, amplitude = 0.2|
                var source = SinOsc.ar(440, 0, amplitude);
                var values = PitchPESTO.kr(source, modelBuf, 0.8);
                SendReply.kr(Impulse.kr(10), '/mir2/neural/runtime', values);
                Out.ar(0, Silent.ar(2));
            }).send(server);
            server.sync;
            group = if(kind == "supernova") {ParGroup.new(server)} {Group.new(server)};
            phase = { |name, seconds|
                var average = List.new, peak = List.new, counts;
                monitoring = false;
                3.wait; // exclude model initialization and window warmup
                replyCounts.clear;
                monitoring = true;
                (seconds * 2).do { |step|
                    0.5.wait;
                    average.add(server.avgCPU);
                    peak.add(server.peakCPU);
                    file.write("%,%,%,%,%,%\n".format(name, (step + 1) * 0.5,
                        nodes.size, server.avgCPU, server.peakCPU, server.numSynths));
                };
                monitoring = false;
                counts = nodes.collect { |node| replyCounts[node.nodeID] ? 0 };
                if(counts.any { |count| count < ((seconds * 10) - 2) }) {ok = false};
                totalReplies = totalReplies + counts.sum;
                if(server.numSynths != nodes.size) {ok = false};
                summaries.add([name, nodes.size, average.mean, peak.maxItem, counts]);
                ["PHASE", name, "instances", nodes.size, "avg CPU", average.mean,
                    "max reported peak", peak.maxItem, "replies", counts].postln;
            };
            phase.value("baseline", 5);
            nodes.add(Synth(\mir2NeuralRuntimeUnit, [\modelBuf, model], group));
            expected[nodes[0].nodeID] = 440;
            phase.value("single", 20);
            nodes.add(Synth(\mir2NeuralRuntimeUnit, [\modelBuf, model], group));
            expected[nodes[1].nodeID] = 440;
            phase.value("shared_two", 20);
            nodes[1].set(\amplitude, 0);
            expected[nodes[1].nodeID] = 0;
            phase.value("shared_independent_silence", 10);
            nodes.do(_.free); nodes.clear; expected.clear;
            server.sync;
            // Repeated construction/destruction tests resource lifetime. This
            // phase is excluded from steady CPU statistics and has muted output.
            64.do {
                var node = Synth(\mir2NeuralRuntimeUnit, [\modelBuf, model, \amplitude, 0], group);
                0.05.wait;
                node.free;
                server.sync;
            };
            phase.value("after_64_lifecycles", 5);
            if(badFrames != 0 or: {totalReplies < 800}) {ok = false};
            ["LIVE_RESULT", "server", kind, "rate", server.sampleRate,
                "block", server.options.blockSize, "bad frames", badFrames,
                "pitch error Hz", maximumPitchError, "replies", totalReplies].postln;
            group.free; server.sync;
            model.free; server.sync;
            file.close;
            responder.free;
            server.quit;
            1.wait;
            if(ok) {"MIR2_NEURAL_LIVE_OK".postln; 0.exit} {"MIR2_NEURAL_LIVE_FAILED".postln; 1.exit};
        } { |error|
            error.reportError;
            if(file.notNil) {file.close};
            responder.free;
            server.quit;
            1.wait;
            1.exit
        }
    }).play(SystemClock)
}, 30, {
    responder.free;
    "MIR2_NEURAL_LIVE_BOOT_FAILED".postln;
    server.quit;
    1.exit
});
)
