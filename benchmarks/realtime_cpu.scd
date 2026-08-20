(
var cases = [
    [\spectralCore, 32, { |source|
        var chain = FFT(LocalBuf(1024), source, 0.5);
        Mix([
            SpecCentroid2.kr(chain, fftSize: 1024) * 1e-6,
            SpecFlatness2.kr(chain, fftSize: 1024),
            SpecPcile2.kr(chain, fftSize: 1024) * 1e-6,
            FFTFlux2.kr(chain, fftSize: 1024)
        ])
    }],
    [\mfcc13, 16, { |source|
        var chain = FFT(LocalBuf(1024), source, 0.5);
        Mix(MFCC2.kr(chain, 13, 40, fftSize: 1024)) * 1e-4
    }],
    [\onsetsSuperFlux, 16, { |source|
        var chain = FFT(LocalBuf(1024), source, 0.5);
        Mix(Onsets2.kr(chain, fftSize: 1024))
    }],
    [\pitchYIN2048, 8, { |source| Mix(PitchYIN.kr(source, 40, 1000, 2048)) * 1e-4 }],
    [\pitchMPM2048, 8, { |source| Mix(PitchMPM.kr(source, 40, 1000, 2048)) * 1e-4 }],
    [\pitchFamily, 4, { |source|
        Mix([
            Pitch2.kr(source, 40, 1000, 2048),
            Periodicity.kr(source, 40, 1000, 2048),
            Voicing.kr(source, 1024, 0.25, 40, 1000)
        ].flat) * 1e-4
    }]
];

Server.default = s = Server(\mir2BenchmarkServer, NetAddr("127.0.0.1", 57145));
s.options.numOutputBusChannels = 2;
s.options.numInputBusChannels = 0;

s.waitForBoot({
    Routine({
        var bus = Bus.audio(s, 1);
        var baseline;
        2.wait;
        baseline = s.avgCPU;
        "MIR2 benchmark: sampleRate=% blockSize=% baseline=%"
            .format(s.sampleRate, s.options.blockSize, baseline.round(0.001)).postln;
        cases.do { |entry|
            var name = entry[0];
            var count = entry[1];
            var builder = entry[2];
            var definitionName = ("mir2Bench_" ++ name).asSymbol;
            var synths;
            SynthDef(definitionName, {
                var source = SinOsc.ar(220, 0, 0.1) + PinkNoise.ar(0.01);
                Out.ar(bus, K2A.ar(builder.value(source)) * 0)
            }).add;
            s.sync;
            synths = count.collect { Synth(definitionName) };
            3.wait;
            "% instances=% average=% peak=% incrementalPerInstance=%"
                .format(name, count, s.avgCPU.round(0.001), s.peakCPU.round(0.001),
                    ((s.avgCPU - baseline) / count).round(0.0001)).postln;
            synths.do(_.free);
            s.sync;
            1.wait
        };
        bus.free;
        s.sync;
        "MIR2_BENCHMARK_DONE".postln;
        s.quit;
        0.exit
    }).play(SystemClock)
})
)
