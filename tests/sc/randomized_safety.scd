(
var received = IdentitySet.new;
var responder;

Server.default = s = Server(\miraSafetyServer, NetAddr("127.0.0.1", 57147));
s.options.numOutputBusChannels = 2;
s.options.numInputBusChannels = 0;

s.waitForBoot({
    responder = OSCdef(\miraSafety, { |message|
        var kind = message[3].asInteger;
        var values = message.copyRange(4, message.size - 1);
        if(values.any({ |value| value.isNaN or: { value.abs > 1e30 } })) {
            Error("MIRa safety test emitted NaN or Inf for kind %".format(kind)).throw
        };
        received.add(kind)
    }, '/mira/safety');

    SynthDef(\miraSafety, { |kind = 0|
        var source = Select.ar(kind, [
            Silent.ar,
            DC.ar(0.2),
            Impulse.ar(3, 0, 0.5),
            WhiteNoise.ar(0.2)
        ]);
        var chain = FFT(LocalBuf(1024), source, 0.5);
        var values = [
            SpecCentroid2.kr(chain, fftSize: 1024),
            SpecFlatness2.kr(chain, fftSize: 1024),
            SpecPcile2.kr(chain, 1, fftSize: 1024),
            MFCC2.kr(chain, 8, 32, fftSize: 1024),
            FFTFlux2.kr(chain, 1, 1, fftSize: 1024),
            Onsets2.kr(chain, 0, \superflux, 0.001, 0, 0.001, 12, 1e-20, 1024, 0.5),
            PitchYIN.kr(source, 50, 5000, 1024, 0.0625, 0.01),
            PitchMPM.kr(source, 50, 5000, 1024, 1, 0.5),
            Periodicity.kr(source, 50, 5000, 1024, 0.0625),
            Voicing.kr(source, 1024, 1, 50, 5000, \hybrid, 0),
            Pitch2.kr(source, 50, 5000, 1024, 0.0625, \yin, 0.8, 0.1, 1)
        ].flat;
        SendReply.kr(Impulse.kr(4), '/mira/safety', [kind] ++ values);
        Silent.ar(2)
    }).add;
    s.sync;
    4.do { |kind| Synth(\miraSafety, [\kind, kind]) };
    Routine({
        4.wait;
        if(received.size != 4) {
            Error("MIRa safety test did not receive all signal cases").throw
        };
        "MIRA_RANDOMIZED_SAFETY_OK".postln;
        responder.free;
        s.quit;
        0.exit
    }).play(SystemClock)
})
)
