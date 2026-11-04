(
var received = false;
var latest;
var responder;

Server.default = s = Server(\miraLiveServer, NetAddr("127.0.0.1", 57146));
s.options.numOutputBusChannels = 2;
s.options.numInputBusChannels = 0;

s.waitForBoot({
    responder = OSCdef(\miraLiveSmoke, { |message|
        latest = message.copyRange(3, message.size - 1);
        received = true
    }, '/mira/live');

    SynthDef(\miraLiveSmoke, {
        var source = SinOsc.ar(220, 0, 0.2);
        var chain = FFT(LocalBuf(1024), source, 0.5);
        var yin = PitchYIN.kr(source, 40, 1000, 2048, 0.25, 0.1);
        var mpm = PitchMPM.kr(source, 40, 1000, 2048, 0.25, 0.93);
        var pitch = Pitch2.kr(source, 40, 1000, 2048, 0.25, \mpm);
        var voice = Voicing.kr(source, 1024, 0.25, 40, 1000);
        var onset = Onsets2.kr(chain, fftSize: 1024);
        var values = [
            SpecCentroid2.kr(chain, fftSize: 1024),
            SpecFlatness2.kr(chain, fftSize: 1024),
            SpecPcile2.kr(chain, fftSize: 1024),
            MFCC2.kr(chain, 3, 20, fftSize: 1024),
            FFTFlux2.kr(chain, fftSize: 1024),
            onset,
            yin,
            mpm,
            Periodicity.kr(source, 40, 1000, 2048),
            voice,
            pitch
        ].flat;
        SendReply.kr(Impulse.kr(5), '/mira/live', values);
        Silent.ar(2)
    }).add;
    s.sync;
    Synth(\miraLiveSmoke);

    Routine({
        4.wait;
        if(received.not) {
            Error("MIRa live smoke received no analysis frames").throw
        };
        if(latest.any({ |value| value.isNaN or: { value.abs > 1e30 } })) {
            Error("MIRa live smoke produced NaN or Inf").throw
        };
        if((latest[9] - 220).abs > 3 or: { latest[10] < 0.7 }
            or: { (latest[12] - 220).abs > 3 }
            or: { latest[13] < 0.7 }
            or: { latest[15] < 0.7 }
            or: { latest[17] < 0.5 }
            or: { (latest[18] - 220).abs > 3 }) {
            latest.postln;
            Error("MIRa pitch/periodicity/voicing smoke bounds failed").throw
        };
        "MIRA_LIVE_SMOKE_OK values=%".format(latest).postln;
        responder.free;
        s.quit;
        0.exit
    }).play(SystemClock)
})
)
