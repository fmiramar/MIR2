(
var sampleRates = [44100, 48000, 96000];
var blockSizes = [1, 64, 256];
var channelCount = 15;
var duration = 0.15;
var definition = SynthDef(\miraNRT, {
    var source = SinOsc.ar(220, 0, 0.2);
    var chain = FFT(LocalBuf(1024), source, 0.5);
    var onset = Onsets2.kr(chain, fftSize: 1024);
    var yin = PitchYIN.kr(source, 40, 1000, 2048);
    var mpm = PitchMPM.kr(source, 40, 1000, 2048);
    var voice = Voicing.kr(source, 1024, 0.25, 40, 1000);
    var pitch = Pitch2.kr(source, 40, 1000, 2048);
    var values = [
        SpecCentroid2.kr(chain, fftSize: 1024),
        SpecFlatness2.kr(chain, fftSize: 1024),
        SpecPcile2.kr(chain, fftSize: 1024),
        MFCC2.kr(chain, 3, 20, fftSize: 1024),
        FFTFlux2.kr(chain, fftSize: 1024),
        onset[1],
        yin[0], yin[1], mpm[0],
        Periodicity.kr(source, 40, 1000, 2048),
        voice[1], pitch[0], pitch[3]
    ].flat;
    Out.ar(0, K2A.ar(values))
});

Routine({
    var summaries = List.new;
    try {
        sampleRates.do { |sampleRate|
            blockSizes.do { |blockSize|
                var outputPath = Platform.defaultTempDir +/+
                    "mira-nrt-%-%.wav".format(sampleRate, blockSize);
                var options = ServerOptions.new;
                var score;
                var completed = FlowVar.new;
                var soundFile;
                var data;
                var frames;
                var lastFrame;
                var values;
                options.numOutputBusChannels = channelCount;
                options.numInputBusChannels = 0;
                options.numAudioBusChannels = 128;
                options.numWireBufs = 1024;
                options.memSize = 262144;
                options.blockSize = blockSize;
                score = Score([
                    [0.0, [\d_recv, definition.asBytes]],
                    [0.0, [\s_new, \miraNRT, 1000, 0, 0]],
                    [duration, [\n_free, 1000]]
                ]);
                score.recordNRT(
                    outputFilePath: outputPath,
                    sampleRate: sampleRate,
                    headerFormat: "wav",
                    sampleFormat: "float",
                    options: options,
                    duration: duration,
                    action: { completed.value = true }
                );
                completed.value;
                soundFile = SoundFile.openRead(outputPath);
                if(soundFile.isOpen.not or: { soundFile.numChannels != channelCount }) {
                    Error("MIRa NRT output file/channel failure").throw
                };
                frames = soundFile.numFrames;
                data = FloatArray.newClear(frames * channelCount);
                soundFile.readData(data);
                soundFile.close;
                File.delete(outputPath);
                if(data.any({ |value| value.isNaN or: { value.abs > 1e30 } })) {
                    Error("MIRa NRT emitted NaN or Inf").throw
                };
                lastFrame = (sampleRate * 0.12).asInteger.min(frames - 1);
                values = Array.fill(channelCount, { |channel|
                    data[(lastFrame * channelCount) + channel]
                });
                if((values[8] - 220).abs > 3 or: { values[9] < 0.7 }
                    or: { (values[10] - 220).abs > 3 }
                    or: { values[11] < 0.7 }
                    or: { values[12] < 0.5 }
                    or: { (values[13] - 220).abs > 3 }) {
                    values.postln;
                    Error("MIRa NRT pitch bounds failed").throw
                };
                summaries.add([
                    sampleRate, blockSize, values[0].round(0.01),
                    values[8].round(0.01), values[10].round(0.01),
                    values[11].round(0.0001), values[12].round(0.0001)
                ])
            }
        };
        "MIRA_NRT_MATRIX_OK".postln;
        "sampleRate blockSize centroid yinHz mpmHz periodicity voicing".postln;
        summaries.do(_.postln);
        0.exit
    } { |error|
        error.reportError;
        1.exit
    }
}).play(SystemClock)
)
