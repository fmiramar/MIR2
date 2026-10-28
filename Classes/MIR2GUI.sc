// SPDX-License-Identifier: GPL-3.0-or-later
/*
    MIR2GUI: Reusable real-time MIR visualization toolkit for SuperCollider.
    Provides standard graphical monitors for MIR feature extraction:
    - MIR2Scope: Scrolling time-series scalar feature strip
    - MIR2BarView: Multi-band spectrum and cepstral bar monitor
    - MIR2Heatmap: 2D scrolling waterfall spectrogram/tempogram
    - MIR2ChromaWheel: 12-pitch-class circular polar radar
    - MIR2TonnetzView: 2D planar projection of 6D Tonnetz harmonic space
    - MIR2StereoScope: Polar goniometer and stereo correlation meter
    - MIR2PianoRoll: Scrolling pitch and multi-pitch tracker
    - MIR2RhythmWheel: Circular tempo and beat phase dial
    - MIR2TristimulusView: Barycentric ternary triangle for harmonic energy
    - MIR2EnvelopeView: Triggered transient envelope and attack/decay scope
*/

MIR2GUI {
    *soundPath { |filename|
        var devPath = this.filenameSymbol.asString.dirname.dirname +/+ "sounds" +/+ filename;
        if(File.exists(devPath)) { ^devPath };
        devPath = Platform.userExtensionDir +/+ "MIR2/sounds" +/+ filename;
        if(File.exists(devPath)) { ^devPath };
        ^nil
    }

    *scope { |name="Feature", bus, minVal=0.0, maxVal=1.0, units="", width=380, height=200|
        ^MIR2Scope(name, bus, minVal, maxVal, units, width, height)
    }

    *bars { |name="Spectrum", bus, numBars=24, minVal=0.0, maxVal=1.0, labels, width=420, height=220|
        ^MIR2BarView(name, bus, numBars, minVal, maxVal, labels, width, height)
    }

    *heatmap { |name="Time-Frequency", bus, numBins=84, minVal=0.0, maxVal=1.0, width=460, height=240|
        ^MIR2Heatmap(name, bus, numBins, minVal, maxVal, width, height)
    }

    *chroma { |name="Chroma Wheel", bus, tuning=440, width=320, height=340|
        ^MIR2ChromaWheel(name, bus, tuning, width, height)
    }

    *tonnetz { |name="Tonnetz Space", bus, width=340, height=360|
        ^MIR2TonnetzView(name, bus, width, height)
    }

    *stereo { |name="Stereo Scope", bus, width=360, height=360|
        ^MIR2StereoScope(name, bus, width, height)
    }

    *pianoRoll { |name="Piano Roll", bus, minMIDI=36, maxMIDI=84, width=440, height=240|
        ^MIR2PianoRoll(name, bus, minMIDI, maxMIDI, width, height)
    }

    *rhythm { |name="Rhythm Radar", bus, width=320, height=340|
        ^MIR2RhythmWheel(name, bus, width, height)
    }

    *tristimulus { |name="Tristimulus", bus, width=340, height=340|
        ^MIR2TristimulusView(name, bus, width, height)
    }

    *envelope { |name="Transient Envelope", bus, width=380, height=200|
        ^MIR2EnvelopeView(name, bus, width, height)
    }
}

MIR2BaseView {
    var <window, <userView, <skipjack, <bus, <name;
    var <themeBg, <themeFg, <themeGrid, <themeAccent, <themeAccent2;

    initTheme {
        themeBg = Color.fromHexString("#16181d");
        themeFg = Color.fromHexString("#e4e7eb");
        themeGrid = Color.fromHexString("#2b303a");
        themeAccent = Color.fromHexString("#00d2d3");
        themeAccent2 = Color.fromHexString("#ff9f43");
    }

    close {
        if(skipjack.notNil) { skipjack.stop; skipjack = nil };
        if(window.notNil and: { window.isClosed.not }) { window.close };
    }
}

MIR2Scope : MIR2BaseView {
    var <minVal, <maxVal, <units, <historySize, <history, <currentVal = 0.0;

    *new { |name="Feature", bus, minVal=0.0, maxVal=1.0, units="", width=380, height=200, historySize=200|
        ^super.new.init(name, bus, minVal, maxVal, units, width, height, historySize)
    }

    init { |argName, argBus, argMin, argMax, argUnits, width, height, argHistSize|
        this.initTheme;
        name = argName; bus = argBus; minVal = argMin.asFloat; maxVal = argMax.asFloat;
        units = argUnits.asString; historySize = argHistSize.asInteger;
        history = Array.fill(historySize, minVal);

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.get({ |val|
                        currentVal = val;
                        history = history.drop(1) ++ [val];
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var w = bounds.width;
        var h = bounds.height;
        var padLeft = 45, padRight = 15, padTop = 30, padBottom = 25;
        var plotW = w - padLeft - padRight;
        var plotH = h - padTop - padBottom;
        var range = (maxVal - minVal).max(1e-6);

        // Background & Header
        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, w, h));

        Pen.font = Font.sansSerif(11, true);
        Pen.stringLeftJustified(name, Rect(padLeft, 8, plotW, 18), Font.sansSerif(11, true), themeFg);

        // Numeric Readout
        Pen.stringRightJustified(
            "%.2f %".format(currentVal, units),
            Rect(padLeft, 8, plotW, 18),
            Font.monospace(11, true),
            themeAccent
        );

        // Plot Grid & Labels
        Pen.strokeColor = themeGrid;
        Pen.width = 1;
        3.do { |i|
            var frac = i / 2.0;
            var y = padTop + (plotH * (1.0 - frac));
            var v = minVal + (range * frac);
            Pen.line(Point(padLeft, y), Point(padLeft + plotW, y));
            Pen.stroke;
            Pen.stringRightJustified("%.1f".format(v), Rect(2, y - 6, padLeft - 6, 14), Font.sansSerif(9), Color.gray(0.6));
        };

        // Plot Trace
        if(history.size > 1) {
            var step = plotW / (history.size - 1);
            Pen.beginPath;
            history.do { |val, i|
                var norm = ((val - minVal) / range).clip(0.0, 1.0);
                var x = padLeft + (i * step);
                var y = padTop + (plotH * (1.0 - norm));
                if(i == 0) { Pen.moveTo(Point(x, y)) } { Pen.lineTo(Point(x, y)) };
            };
            Pen.strokeColor = themeAccent;
            Pen.width = 1.8;
            Pen.stroke;
        };
    }
}

MIR2BarView : MIR2BaseView {
    var <numBars, <minVal, <maxVal, <labels, <values, <peaks;

    *new { |name="Spectrum", bus, numBars=24, minVal=0.0, maxVal=1.0, labels, width=420, height=220|
        ^super.new.init(name, bus, numBars, minVal, maxVal, labels, width, height)
    }

    init { |argName, argBus, argNumBars, argMin, argMax, argLabels, width, height|
        this.initTheme;
        name = argName; bus = argBus; numBars = argNumBars.asInteger;
        minVal = argMin.asFloat; maxVal = argMax.asFloat;
        labels = argLabels;
        values = Array.fill(numBars, minVal);
        peaks = Array.fill(numBars, minVal);

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(numBars, { |vals|
                        values = vals;
                        peaks = peaks.collect { |p, i| (p * 0.96).max(vals[i] ? minVal) };
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var w = bounds.width;
        var h = bounds.height;
        var padLeft = 35, padRight = 15, padTop = 30, padBottom = 25;
        var plotW = w - padLeft - padRight;
        var plotH = h - padTop - padBottom;
        var range = (maxVal - minVal).max(1e-6);
        var barW = (plotW / numBars.max(1)).max(2);

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, w, h));

        Pen.stringLeftJustified(name, Rect(padLeft, 8, plotW, 18), Font.sansSerif(11, true), themeFg);

        // Bars
        numBars.do { |i|
            var val = values[i] ? minVal;
            var peak = peaks[i] ? minVal;
            var norm = ((val - minVal) / range).clip(0.0, 1.0);
            var peakNorm = ((peak - minVal) / range).clip(0.0, 1.0);
            var x = padLeft + (i * barW);
            var barH = plotH * norm;
            var y = padTop + plotH - barH;
            var peakY = padTop + plotH - (plotH * peakNorm);

            // Bar fill
            Pen.fillColor = Color.fromHexString("#00d2d3").alpha_(0.8);
            Pen.fillRect(Rect(x + 1, y, barW - 2, barH));

            // Peak tick
            Pen.strokeColor = themeAccent2;
            Pen.width = 1.5;
            Pen.line(Point(x + 1, peakY), Point(x + barW - 1, peakY));
            Pen.stroke;
        };

        // Axis
        Pen.strokeColor = themeGrid;
        Pen.width = 1;
        Pen.line(Point(padLeft, padTop + plotH), Point(padLeft + plotW, padTop + plotH));
        Pen.stroke;
    }
}

MIR2Heatmap : MIR2BaseView {
    var <numBins, <minVal, <maxVal, <historyFrames, <grid;

    *new { |name="Heatmap", bus, numBins=84, minVal=0.0, maxVal=1.0, width=460, height=240, historyFrames=80|
        ^super.new.init(name, bus, numBins, minVal, maxVal, width, height, historyFrames)
    }

    init { |argName, argBus, argBins, argMin, argMax, width, height, argHist|
        this.initTheme;
        name = argName; bus = argBus; numBins = argBins.asInteger;
        minVal = argMin.asFloat; maxVal = argMax.asFloat;
        historyFrames = argHist.asInteger;
        grid = Array.fill(historyFrames, { Array.fill(numBins, minVal) });

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(numBins, { |vals|
                        grid = grid.drop(1) ++ [vals];
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.04, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var w = bounds.width;
        var h = bounds.height;
        var padLeft = 40, padRight = 15, padTop = 30, padBottom = 20;
        var plotW = w - padLeft - padRight;
        var plotH = h - padTop - padBottom;
        var range = (maxVal - minVal).max(1e-6);
        var cellW = (plotW / historyFrames).max(1);
        var cellH = (plotH / numBins).max(1);

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, w, h));
        Pen.stringLeftJustified(name, Rect(padLeft, 8, plotW, 18), Font.sansSerif(11, true), themeFg);

        grid.do { |frame, xIdx|
            var x = padLeft + (xIdx * cellW);
            frame.do { |val, yIdx|
                var norm = ((val - minVal) / range).clip(0.0, 1.0);
                var y = padTop + plotH - ((yIdx + 1) * cellH);
                // Thermal map: dark blue -> purple -> yellow -> white
                var r = (norm * 1.5).clip(0.0, 1.0);
                var g = ((norm - 0.4) * 2.0).clip(0.0, 1.0);
                var b = (1.0 - (norm * 1.2)).clip(0.1, 0.8);
                Pen.fillColor = Color(r, g, b, 0.95);
                Pen.fillRect(Rect(x, y, cellW + 0.5, cellH + 0.5));
            };
        };
    }
}

MIR2ChromaWheel : MIR2BaseView {
    var <tuning, <chromaValues;
    classvar <noteNames;

    *initClass {
        noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
    }

    *new { |name="Chroma Wheel", bus, tuning=440, width=320, height=340|
        ^super.new.init(name, bus, tuning, width, height)
    }

    init { |argName, argBus, argTuning, width, height|
        this.initTheme;
        name = argName; bus = argBus; tuning = argTuning;
        chromaValues = Array.fill(12, 0.0);

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(12, { |vals|
                        chromaValues = vals;
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var center = Point(bounds.width * 0.5, (bounds.height * 0.5) + 10);
        var maxRadius = (bounds.width.min(bounds.height) * 0.42).max(20);
        var angleStep = 2pi / 12;

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, bounds.width, bounds.height));
        Pen.stringCenterJustified(name, Rect(10, 8, bounds.width - 20, 18), Font.sansSerif(11, true), themeFg);

        // Reference Circles
        Pen.strokeColor = themeGrid;
        Pen.width = 1;
        3.do { |i|
            var r = maxRadius * (i + 1) / 3.0;
            Pen.addArc(center, r, 0, 2pi);
            Pen.stroke;
        };

        // 12 Pitch-Class Wedges
        12.do { |i|
            var norm = (chromaValues[i] ? 0.0).clip(0.0, 1.0);
            var angle = (-0.5pi) + (i * angleStep);
            var wedgeR = maxRadius * norm;
            var labelPt = Point(
                center.x + ((maxRadius + 14) * cos(angle)),
                center.y + ((maxRadius + 14) * sin(angle))
            );

            // Sector Arc
            if(wedgeR > 1) {
                Pen.beginPath;
                Pen.moveTo(center);
                Pen.addArc(center, wedgeR, angle - (angleStep * 0.45), angleStep * 0.9);
                Pen.lineTo(center);
                Pen.fillColor = Color.fromHexString("#00d2d3").alpha_(0.25 + (norm * 0.65));
                Pen.fill;
            };

            // Pitch label
            Pen.font = Font.sansSerif(10, true);
            Pen.stringCenterJustified(
                noteNames[i],
                Rect(labelPt.x - 12, labelPt.y - 7, 24, 14),
                Font.sansSerif(10, true),
                if(norm > 0.5) { themeAccent2 } { themeFg }
            );
        };
    }
}

MIR2TonnetzView : MIR2BaseView {
    var <tonnetzCoords;

    *new { |name="Tonnetz Space", bus, width=340, height=360|
        ^super.new.init(name, bus, width, height)
    }

    init { |argName, argBus, width, height|
        this.initTheme;
        name = argName; bus = argBus;
        tonnetzCoords = Array.fill(6, 0.0);

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(6, { |vals|
                        tonnetzCoords = vals;
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var center = Point(bounds.width * 0.5, (bounds.height * 0.5) + 10);
        var r = (bounds.width.min(bounds.height) * 0.38).max(20);

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, bounds.width, bounds.height));
        Pen.stringCenterJustified(name, Rect(10, 8, bounds.width - 20, 18), Font.sansSerif(11, true), themeFg);

        // Three Tonnetz Circles: Fifths (r), Minor 3rds (0.75r), Major 3rds (0.5r)
        Pen.strokeColor = themeGrid;
        Pen.width = 1;
        Pen.addArc(center, r, 0, 2pi);
        Pen.addArc(center, r * 0.75, 0, 2pi);
        Pen.addArc(center, r * 0.5, 0, 2pi);
        Pen.stroke;

        // Vector 1 (Fifths)
        Pen.strokeColor = Color.fromHexString("#00d2d3");
        Pen.width = 2;
        Pen.line(center, Point(center.x + (tonnetzCoords[0] * r), center.y - (tonnetzCoords[1] * r)));
        Pen.stroke;

        // Vector 2 (Minor Thirds)
        Pen.strokeColor = Color.fromHexString("#ff9f43");
        Pen.width = 1.8;
        Pen.line(center, Point(center.x + (tonnetzCoords[2] * r * 0.75), center.y - (tonnetzCoords[3] * r * 0.75)));
        Pen.stroke;

        // Centroid Bead
        Pen.fillColor = Color.white;
        Pen.addArc(Point(center.x + (tonnetzCoords[0] * r), center.y - (tonnetzCoords[1] * r)), 4, 0, 2pi);
        Pen.fill;
    }
}

MIR2StereoScope : MIR2BaseView {
    var <balance = 0.0, <widthVal = 1.0, <corr = 1.0;

    *new { |name="Stereo Scope", bus, width=360, height=360|
        ^super.new.init(name, bus, width, height)
    }

    init { |argName, argBus, width, height|
        this.initTheme;
        name = argName; bus = argBus;

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(3, { |vals|
                        balance = vals[0] ? 0.0;
                        widthVal = vals[1] ? 1.0;
                        corr = vals[2] ? 1.0;
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var center = Point(bounds.width * 0.5, (bounds.height * 0.5) + 5);
        var r = (bounds.width.min(bounds.height) * 0.36).max(20);

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, bounds.width, bounds.height));
        Pen.stringCenterJustified(name, Rect(10, 8, bounds.width - 20, 18), Font.sansSerif(11, true), themeFg);

        // Circular perimeter & crosshairs
        Pen.strokeColor = themeGrid;
        Pen.width = 1;
        Pen.addArc(center, r, 0, 2pi);
        Pen.line(Point(center.x - r, center.y), Point(center.x + r, center.y));
        Pen.line(Point(center.x, center.y - r), Point(center.x, center.y + r));
        Pen.stroke;

        // Stereo Width Ellipse
        Pen.strokeColor = themeAccent;
        Pen.width = 2;
        Pen.addArc(center, (r * widthVal.clip(0.1, 1.8) * 0.5), 0, 2pi);
        Pen.stroke;

        // Balance Needle
        Pen.strokeColor = themeAccent2;
        Pen.width = 2.5;
        Pen.line(center, Point(center.x + (balance.clip(-1.0, 1.0) * r), center.y - (r * 0.8)));
        Pen.stroke;

        // Correlation readout
        Pen.stringCenterJustified("Corr: %.2f | Bal: %.2f".format(corr, balance), Rect(10, bounds.height - 24, bounds.width - 20, 16), Font.monospace(10), themeFg);
    }
}

MIR2PianoRoll : MIR2BaseView {
    var <minMIDI, <maxMIDI, <pitchHistory, <currentMIDI = 60.0;

    *new { |name="Piano Roll", bus, minMIDI=36, maxMIDI=84, width=440, height=240, historySize=120|
        ^super.new.init(name, bus, minMIDI, maxMIDI, width, height, historySize)
    }

    init { |argName, argBus, argMin, argMax, width, height, historySize|
        this.initTheme;
        name = argName; bus = argBus; minMIDI = argMin.asFloat; maxMIDI = argMax.asFloat;
        pitchHistory = Array.fill(historySize, minMIDI);

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.get({ |val|
                        currentMIDI = if(val > 20) { val.cpsmidi } { 0 };
                        pitchHistory = pitchHistory.drop(1) ++ [currentMIDI];
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var w = bounds.width;
        var h = bounds.height;
        var padLeft = 40, padRight = 15, padTop = 30, padBottom = 20;
        var plotW = w - padLeft - padRight;
        var plotH = h - padTop - padBottom;
        var numSemitones = (maxMIDI - minMIDI).max(12);
        var noteH = plotH / numSemitones;

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, w, h));
        Pen.stringLeftJustified(name, Rect(padLeft, 8, plotW, 18), Font.sansSerif(11, true), themeFg);

        // Key stripes
        numSemitones.do { |i|
            var midi = minMIDI + i;
            var isBlack = [1, 3, 6, 8, 10].includes(midi.asInteger % 12);
            var y = padTop + plotH - ((i + 1) * noteH);
            Pen.fillColor = if(isBlack) { Color.fromHexString("#1b1e24") } { Color.fromHexString("#21252d") };
            Pen.fillRect(Rect(padLeft, y, plotW, noteH));
        };

        // Pitch Trace
        if(pitchHistory.size > 1) {
            var step = plotW / (pitchHistory.size - 1);
            pitchHistory.do { |midi, i|
                if(midi >= minMIDI and: { midi <= maxMIDI }) {
                    var norm = (midi - minMIDI) / numSemitones;
                    var x = padLeft + (i * step);
                    var y = padTop + plotH - (norm * plotH);
                    Pen.fillColor = themeAccent;
                    Pen.addArc(Point(x, y), 2.5, 0, 2pi);
                    Pen.fill;
                };
            };
        };
    }
}

MIR2RhythmWheel : MIR2BaseView {
    var <bpm = 120.0, <confidence = 1.0, <phase = 0.0;

    *new { |name="Rhythm Radar", bus, width=320, height=340|
        ^super.new.init(name, bus, width, height)
    }

    init { |argName, argBus, width, height|
        this.initTheme;
        name = argName; bus = argBus;

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(3, { |vals|
                        bpm = vals[0] ? 120.0;
                        confidence = vals[1] ? 1.0;
                        phase = (phase + (bpm / 60.0 * 0.033)).wrap(0.0, 1.0);
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var center = Point(bounds.width * 0.5, (bounds.height * 0.5) + 10);
        var r = (bounds.width.min(bounds.height) * 0.38).max(20);
        var handAngle = (-0.5pi) + (phase * 2pi);

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, bounds.width, bounds.height));
        Pen.stringCenterJustified(name, Rect(10, 8, bounds.width - 20, 18), Font.sansSerif(11, true), themeFg);

        // Dial Circles
        Pen.strokeColor = themeGrid;
        Pen.width = 1.5;
        Pen.addArc(center, r, 0, 2pi);
        Pen.stroke;

        // 4 Beat Division Ticks
        4.do { |i|
            var angle = (-0.5pi) + (i * 0.5pi);
            var p1 = Point(center.x + ((r - 8) * cos(angle)), center.y + ((r - 8) * sin(angle)));
            var p2 = Point(center.x + (r * cos(angle)), center.y + (r * sin(angle)));
            Pen.strokeColor = themeAccent2;
            Pen.width = 2;
            Pen.line(p1, p2);
            Pen.stroke;
        };

        // Sweep Hand
        Pen.strokeColor = themeAccent;
        Pen.width = 2.5;
        Pen.line(center, Point(center.x + (r * cos(handAngle)), center.y + (r * sin(handAngle))));
        Pen.stroke;

        // Center BPM Readout
        Pen.stringCenterJustified("%.1f BPM".format(bpm), Rect(center.x - 50, center.y - 10, 100, 20), Font.sansSerif(13, true), themeFg);
    }
}

MIR2TristimulusView : MIR2BaseView {
    var <t1 = 0.33, <t2 = 0.33, <t3 = 0.34;

    *new { |name="Tristimulus", bus, width=340, height=340|
        ^super.new.init(name, bus, width, height)
    }

    init { |argName, argBus, width, height|
        this.initTheme;
        name = argName; bus = argBus;

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(3, { |vals|
                        t1 = vals[0] ? 0.33;
                        t2 = vals[1] ? 0.33;
                        t3 = vals[2] ? 0.34;
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var side = bounds.width.min(bounds.height) * 0.75;
        var hTri = side * (3.sqrt / 2);
        var center = Point(bounds.width * 0.5, (bounds.height * 0.5) + 12);
        var v1 = Point(center.x, center.y - (hTri * (2/3)));
        var v2 = Point(center.x - (side * 0.5), center.y + (hTri * (1/3)));
        var v3 = Point(center.x + (side * 0.5), center.y + (hTri * (1/3)));

        var sum = (t1 + t2 + t3).max(1e-6);
        var n1 = t1 / sum, n2 = t2 / sum, n3 = t3 / sum;
        var pt = Point((n1 * v1.x) + (n2 * v2.x) + (n3 * v3.x), (n1 * v1.y) + (n2 * v2.y) + (n3 * v3.y));

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, bounds.width, bounds.height));
        Pen.stringCenterJustified(name, Rect(10, 8, bounds.width - 20, 18), Font.sansSerif(11, true), themeFg);

        // Triangle Outline
        Pen.strokeColor = themeGrid;
        Pen.width = 1.5;
        Pen.moveTo(v1); Pen.lineTo(v2); Pen.lineTo(v3); Pen.lineTo(v1);
        Pen.stroke;

        // Vertex Labels
        Pen.stringCenterJustified("T1 (f0)", Rect(v1.x - 30, v1.y - 18, 60, 16), Font.sansSerif(10, true), themeAccent);
        Pen.stringCenterJustified("T2 (2-4)", Rect(v2.x - 35, v2.y + 4, 70, 16), Font.sansSerif(10, true), themeAccent2);
        Pen.stringCenterJustified("T3 (5+)", Rect(v3.x - 35, v3.y + 4, 70, 16), Font.sansSerif(10, true), Color.fromHexString("#a29bfe"));

        // Position Bead
        Pen.fillColor = Color.white;
        Pen.addArc(pt, 5, 0, 2pi);
        Pen.fill;
    }
}

MIR2EnvelopeView : MIR2BaseView {
    var <attackVal = 0.05, <decayVal = 0.5;

    *new { |name="Transient Envelope", bus, width=380, height=200|
        ^super.new.init(name, bus, width, height)
    }

    init { |argName, argBus, width, height|
        this.initTheme;
        name = argName; bus = argBus;

        if(GUI.current.isNil) { ^this };
        {
            window = Window(name, Rect(200, 200, width, height)).background_(themeBg);
            userView = UserView(window, Rect(0, 0, width, height));
            userView.drawFunc = { |view| this.draw(view) };

            skipjack = SkipJack({
                if(bus.notNil and: { bus.server.serverRunning }) {
                    bus.getn(2, { |vals|
                        attackVal = vals[0] ? 0.05;
                        decayVal = vals[1] ? 0.5;
                        { if(userView.notNil and: { userView.isClosed.not }) { userView.refresh } }.defer;
                    });
                };
            }, 0.033, { window.isClosed }, name);

            window.onClose = { this.close };
            window.front;
        }.defer;
    }

    draw { |view|
        var bounds = view.bounds;
        var w = bounds.width;
        var h = bounds.height;
        var padLeft = 40, padRight = 20, padTop = 30, padBottom = 25;
        var plotW = w - padLeft - padRight;
        var plotH = h - padTop - padBottom;
        var peakX = padLeft + (plotW * (attackVal.clip(0.001, 1.0) / 1.0));
        var decayX = (peakX + (plotW * (decayVal.clip(0.01, 2.0) / 2.0))).min(padLeft + plotW);

        Pen.fillColor = themeBg;
        Pen.fillRect(Rect(0, 0, w, h));
        Pen.stringLeftJustified(name, Rect(padLeft, 8, plotW, 18), Font.sansSerif(11, true), themeFg);

        // Envelope shape
        Pen.beginPath;
        Pen.moveTo(Point(padLeft, padTop + plotH));
        Pen.lineTo(Point(peakX, padTop));
        Pen.lineTo(Point(decayX, padTop + plotH));
        Pen.strokeColor = themeAccent;
        Pen.width = 2.5;
        Pen.stroke;

        // Readout
        Pen.stringRightJustified("Atk: %.3fs | Dec: %.2fs".format(attackVal, decayVal), Rect(padLeft, 8, plotW, 18), Font.monospace(10), themeAccent2);
    }
}
