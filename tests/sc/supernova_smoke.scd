(
var server = Server(\miraSupernova, NetAddr("127.0.0.1", 57148));

Server.supernova;
server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.numAudioBusChannels = 128;
server.options.blockSize = 64;
server.options.sampleRate = 48000;
server.options.memSize = 262144;

server.waitForBoot({
    Routine({
        var recordBuffer;
        var data;
        var condition;
        try {
            recordBuffer = Buffer.alloc(server, 16384, 1);
            server.sync;
            SynthDef(\miraSupernovaSmoke, { |recordBuf|
                var source = SinOsc.ar(220, 0, 0.2);
                var chain = FFT(LocalBuf(1024), source, 0.5);
                var values = [
                    SpecCentroid2.kr(chain, fftSize: 1024) / 1000,
                    SpecFlatness2.kr(chain, fftSize: 1024),
                    SpecPcile2.kr(chain, fftSize: 1024) / 1000,
                    MFCC2.kr(chain, 3, 20, fftSize: 1024) * 0.01,
                    FFTFlux2.kr(chain, fftSize: 1024),
                    Onsets2.kr(chain, fftSize: 1024),
                    PitchYIN.kr(source, 40, 1000, 2048)[0] / 1000,
                    PitchMPM.kr(source, 40, 1000, 2048)[0] / 1000,
                    Periodicity.kr(source, 40, 1000, 2048),
                    Voicing.kr(source, 1024, 0.25, 40, 1000),
                    Pitch2.kr(source, 40, 1000, 2048)[0] / 1000
                ].flat;
                RecordBuf.ar(K2A.ar(Mix(values) * 0.02), recordBuf,
                    loop: 0, doneAction: Done.freeSelf)
            }).add;
            server.sync;
            Synth(\miraSupernovaSmoke, [\recordBuf, recordBuffer], server);
            0.4.wait;
            server.sync;
            condition = Condition({ data.notNil });
            recordBuffer.loadToFloatArray(action: { |array|
                data = array;
                condition.signal
            });
            condition.wait;
            if(data.any({ |value| value.isNaN or: { value.abs > 1e30 } })) {
                Error("MIRa supernova emitted NaN or Inf").throw
            };
            if(data.abs.maxItem < 1e-6) {
                Error("MIRa supernova analysis stayed silent").throw
            };
            "MIRA_SUPERNOVA_OK peak=%".format(data.abs.maxItem.round(0.0001)).postln;
            recordBuffer.free;
            server.sync;
            server.quit;
            Server.scsynth;
            0.exit
        } { |error|
            error.reportError;
            if(recordBuffer.notNil) { recordBuffer.free };
            server.quit;
            Server.scsynth;
            1.exit
        }
    }).play(SystemClock)
}, 20, {
    "MIRa supernova server failed to boot".error;
    Server.scsynth;
    1.exit
})
)
