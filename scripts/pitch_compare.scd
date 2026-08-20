(
Routine({
    var buffer = Buffer.read(s, ExampleFiles.child);
    s.sync;
    {
        var source = PlayBuf.ar(1, buffer, BufRateScale.kr(buffer), loop: 1);
        // For live input instead: source = SoundIn.ar(0);
        var legacy = Pitch.kr(source);
        var yin = PitchYIN.kr(source, 40, 2000, 2048);
        var mpm = PitchMPM.kr(source, 40, 2000, 2048);
        var modern = Pitch2.kr(source, 40, 2000, 2048);
        [legacy, yin, mpm, modern].flat.poll(5,
            "Pitch / YIN / MPM / Pitch2");
        (source * 0.12) ! 2
    }.play;
}).play(AppClock);
)

