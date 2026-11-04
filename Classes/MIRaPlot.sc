// SPDX-License-Identifier: GPL-3.0-or-later
/*
    MIRaPlot: Whole-audio-file visual display and interactive MIR inspector.
    Provides standard MIR research visualizations similar to librosa, Essentia, and MIRtoolbox:
    - MIRaPlot.waveshow: Waveform envelope with overlaid onset markers and RMS contour
    - MIRaPlot.spectrogram: 2D time-frequency CQT/spectrogram with perceptual colormaps and dB scale
    - MIRaPlot.chromagram: 12-pitch-class harmonic energy profile with semitone labels
    - MIRaPlot.pitch: Continuous F0 trajectory plotted on musical MIDI piano-roll grid
    - MIRaPlot.dashboard: Synchronized multi-tier MIR Inspector with interactive audio scrubbing
*/

MIRaPlot {
    classvar <playSynth;

    *waveshow { |audioPath, width = 840, height = 280|
        ^this.dashboard(audioPath, tiers: [\wave], width: width, height: height, title: "MIRa Waveform & Onsets")
    }

    *spectrogram { |audioPath, colormap = \magma, width = 840, height = 360|
        ^this.dashboard(audioPath, colormap: colormap, tiers: [\spec], width: width, height: height, title: "MIRa Spectrogram")
    }

    *chromagram { |audioPath, colormap = \viridis, width = 840, height = 320|
        ^this.dashboard(audioPath, colormap: colormap, tiers: [\chroma], width: width, height: height, title: "MIRa Chromagram")
    }

    *pitch { |audioPath, width = 840, height = 320|
        ^this.dashboard(audioPath, tiers: [\pitch], width: width, height: height, title: "MIRa Pitch Contour")
    }

    *dashboard { |audioPath, colormap = \magma, tiers, width = 960, height = 740, title|
        var winTitle = title ? "MIRa Audio File Inspector — %".format(PathName(audioPath).fileName);
        var win, mainView, topBar;
        var playBtn, stopBtn, timeTxt, cmapPopup;
        var soundBuf, isPlaying = false, playStartTime = 0, pausedAt = 0;
        var playRoutine;
        var analysisData, specImg, chromaImg;
        var activeTiers = tiers ? [\wave, \spec, \chroma, \pitch, \dynamics];
        var tierHeights, totalPlotHeight;
        var plotLeft = 68, plotRight = 72; // margins for axis labels & colorbar
        var currentCursorTime = 0.0;
        var colormapName = colormap.asSymbol;

        // Default window
        win = Window(winTitle, Rect(60, 60, width, height)).front;
        win.view.background = Color(0.12, 0.12, 0.14);

        topBar = View(win, Rect(0, 0, width, 42));
        topBar.background = Color(0.18, 0.18, 0.22);

        playBtn = Button(topBar, Rect(12, 8, 80, 26))
            .states_([["▶ Play", Color.white, Color(0.2, 0.6, 0.3)], ["⏸ Pause", Color.white, Color(0.8, 0.5, 0.1)]]);

        stopBtn = Button(topBar, Rect(100, 8, 70, 26))
            .states_([["⏹ Stop", Color.white, Color(0.3, 0.3, 0.35)]]);

        timeTxt = StaticText(topBar, Rect(185, 8, 200, 26))
            .stringColor_(Color.white)
            .font_(Font("Monospace", 12, true))
            .string_("00:00.00 / --:--.--");

        StaticText(topBar, Rect(width - 240, 8, 70, 26))
            .stringColor_(Color.gray(0.75))
            .string_("Colormap:");

        cmapPopup = PopUpMenu(topBar, Rect(width - 165, 8, 140, 26))
            .items_(["magma", "viridis", "plasma", "inferno", "coolwarm"])
            .value_(["magma", "viridis", "plasma", "inferno", "coolwarm"].indexOf(colormapName.asString) ? 0)
            .background_(Color(0.25, 0.25, 0.3))
            .stringColor_(Color.white);

        mainView = View(win, Rect(0, 42, width, height - 42));
        mainView.background = Color(0.12, 0.12, 0.14);

        // Loading message
        StaticText(mainView, Rect((width / 2) - 120, ((height - 42) / 2) - 20, 240, 30))
            .stringColor_(Color.cyan)
            .font_(Font("Sans", 14, true))
            .align_(\center)
            .string_("Analyzing audio file...");

        // Load sound into server for interactive auditioning if server running
        if(Server.default.serverRunning) {
            soundBuf = Buffer.read(Server.default, audioPath);
        };

        // Run full analysis
        MIRaFileAnalyzer.analyze(audioPath, hopHz: 100, numCQT: 72, fmin: 65.4, action: { |dict|
            var colormapListeners = List.new;
            analysisData = dict;
            mainView.removeAll; // clear loading label
            
            // Re-allocate server buffer if needed
            if(soundBuf.isNil and: { Server.default.serverRunning }) {
                soundBuf = Buffer.read(Server.default, dict[\path]);
            };

            // Build Images for 2D matrices
            specImg = MIRaPlot.makeMatrixImage(dict[\cqt], colormapName, dbScale: true);
            chromaImg = MIRaPlot.makeMatrixImage(dict[\chroma], colormapName, dbScale: false);

            timeTxt.string_("00:00.00 / %".format(MIRaPlot.formatTime(dict[\duration])));

            // Calculate heights for requested tiers
            tierHeights = Dictionary.newFrom([
                \wave, 110,
                \spec, 180,
                \chroma, 130,
                \pitch, 130,
                \dynamics, 90
            ]);

            // Build stacked views
            MIRaPlot.buildTiers(
                parent: mainView,
                data: dict,
                activeTiers: activeTiers,
                tierHeights: tierHeights,
                width: width,
                height: height - 42,
                plotLeft: plotLeft,
                plotRight: plotRight,
                specImg: specImg,
                chromaImg: chromaImg,
                colormapName: colormapName,
                onSeek: { |seekTime|
                    pausedAt = seekTime.clip(0, dict[\duration]);
                    currentCursorTime = pausedAt;
                    timeTxt.string_("% / %".format(
                        MIRaPlot.formatTime(currentCursorTime),
                        MIRaPlot.formatTime(dict[\duration])
                    ));
                    if(isPlaying) {
                        MIRaPlot.startPlayback(dict[\path], soundBuf, pausedAt, {
                            playBtn.value = 0;
                            isPlaying = false;
                        });
                        playStartTime = Main.elapsedTime - pausedAt;
                    };
                    mainView.children.do { |ch| if(ch.isKindOf(UserView)) { ch.refresh } };
                },
                getCursorTime: { currentCursorTime },
                colormapListeners: colormapListeners
            );

            // Initial refresh to draw views, images, and playhead cursor immediately
            mainView.children.do { |ch| if(ch.isKindOf(UserView)) { ch.refresh } };

            // Play / Pause handling
            playBtn.action = { |btn|
                if(btn.value == 1) {
                    isPlaying = true;
                    playStartTime = Main.elapsedTime - pausedAt;
                    MIRaPlot.startPlayback(dict[\path], soundBuf, pausedAt, {
                        {
                            playBtn.value = 0;
                            isPlaying = false;
                            pausedAt = 0;
                            currentCursorTime = 0;
                            mainView.children.do { |ch| if(ch.isKindOf(UserView)) { ch.refresh } };
                        }.defer;
                    });
                } {
                    isPlaying = false;
                    pausedAt = currentCursorTime;
                    MIRaPlot.stopPlayback;
                };
            };

            stopBtn.action = {
                isPlaying = false;
                playBtn.value = 0;
                pausedAt = 0;
                currentCursorTime = 0;
                MIRaPlot.stopPlayback;
                timeTxt.string_("00:00.00 / %".format(MIRaPlot.formatTime(dict[\duration])));
                mainView.children.do { |ch| if(ch.isKindOf(UserView)) { ch.refresh } };
            };

            cmapPopup.action = { |pop|
                colormapName = pop.items[pop.value].asSymbol;
                specImg = MIRaPlot.makeMatrixImage(dict[\cqt], colormapName, dbScale: true);
                chromaImg = MIRaPlot.makeMatrixImage(dict[\chroma], colormapName, dbScale: false);
                colormapListeners.do { |listener|
                    listener.value(specImg, chromaImg, colormapName);
                };
                mainView.children.do { |ch|
                    if(ch.isKindOf(UserView)) {
                        ch.refresh;
                    };
                };
            };

            // Keyboard Spacebar play/pause
            win.view.keyDownAction = { |view, char, mod, unicode, keycode|
                if(char == $ ) {
                    playBtn.valueAction = (playBtn.value + 1) % 2;
                };
            };

            // Playhead animation routine
            playRoutine = Routine({
                while { win.isClosed.not } {
                    if(isPlaying) {
                        currentCursorTime = (Main.elapsedTime - playStartTime).clip(0, dict[\duration]);
                        timeTxt.string_("% / %".format(
                            MIRaPlot.formatTime(currentCursorTime),
                            MIRaPlot.formatTime(dict[\duration])
                        ));
                        mainView.children.do { |ch| if(ch.isKindOf(UserView)) { ch.refresh } };
                        if(currentCursorTime >= dict[\duration]) {
                            isPlaying = false;
                            playBtn.value = 0;
                            pausedAt = 0;
                            currentCursorTime = 0;
                            MIRaPlot.stopPlayback;
                            mainView.children.do { |ch| if(ch.isKindOf(UserView)) { ch.refresh } };
                        };
                    };
                    (1 / 30).wait;
                };
            }).play(AppClock);
        });

        win.onClose = {
            isPlaying = false;
            MIRaPlot.stopPlayback;
            if(soundBuf.notNil) { soundBuf.free };
            if(specImg.notNil) { specImg.free };
            if(chromaImg.notNil) { chromaImg.free };
        };

        ^win
    }

    *makeMatrixImage { |matrix, colormapName = \magma, dbScale = true|
        var numFrames, numBins, img, pixels, pixelTable;
        var maxVal = 1e-6;
        if(matrix.isNil or: { matrix.isEmpty }) { ^nil };

        numFrames = matrix.size;
        numBins = matrix[0].size;
        img = Image.new(numFrames, numBins);
        pixels = Int32Array.newClear(numFrames * numBins);
        pixelTable = MIRaColormap.pixelTable(colormapName);

        numFrames.do { |x|
            numBins.do { |b|
                var v = matrix[x][b];
                if(v > maxVal) { maxVal = v };
            };
        };

        numBins.do { |b|
            var y = numBins - 1 - b;
            numFrames.do { |x|
                var raw = matrix[x][b];
                var norm;
                if(dbScale) {
                    var ratio = (raw / maxVal).max(1e-4);
                    var db = 20 * ratio.log10;
                    norm = ((db + 70) / 70).clip(0.0, 1.0);
                } {
                    norm = (raw / maxVal).clip(0.0, 1.0);
                };
                pixels[(y * numFrames) + x] = pixelTable[(norm * 255).round.asInteger];
            };
        };

        img.setPixels(pixels);
        ^img
    }

    *buildTiers { |parent, data, activeTiers, tierHeights, width, height, plotLeft, plotRight, specImg, chromaImg, colormapName, onSeek, getCursorTime, colormapListeners|
        var currentY = 8;
        var plotWidth = width - plotLeft - plotRight;
        var totalAvailable = height - 16;
        var scaleFactor = 1.0;
        var calculatedSum = 0;

        activeTiers.do { |k| calculatedSum = calculatedSum + (tierHeights[k] ? 120) };
        if(calculatedSum > totalAvailable) {
            scaleFactor = totalAvailable / calculatedSum;
        };

        activeTiers.do { |k|
            var h = ((tierHeights[k] ? 120) * scaleFactor).round.asInteger;
            var tierRect = Rect(0, currentY, width, h);
            var uv = UserView(parent, tierRect);
            uv.background = Color(0.14, 0.14, 0.17);

            switch(k,
                \wave, {
                    this.setupWaveTier(uv, data, plotLeft, plotWidth, h, onSeek, getCursorTime);
                },
                \spec, {
                    this.setupSpecTier(uv, data, plotLeft, plotWidth, h, specImg, colormapName, onSeek, getCursorTime, colormapListeners);
                },
                \chroma, {
                    this.setupChromaTier(uv, data, plotLeft, plotWidth, h, chromaImg, colormapName, onSeek, getCursorTime, colormapListeners);
                },
                \pitch, {
                    this.setupPitchTier(uv, data, plotLeft, plotWidth, h, onSeek, getCursorTime);
                },
                \dynamics, {
                    this.setupDynamicsTier(uv, data, plotLeft, plotWidth, h, onSeek, getCursorTime);
                }
            );

            currentY = currentY + h + 4;
        };
    }

    *setupWaveTier { |view, data, plotLeft, plotWidth, h, onSeek, getCursorTime|
        var wave = data[\waveform];
        var onsets = data[\onsets] ? [];
        var rms = data[\rms];
        var duration = data[\duration];
        var maxRms = if(rms.notNil and: { rms.notEmpty }) { rms.maxItem.max(1e-4) } { 1.0 };
        var rmsScale = (0.44 / maxRms.max(0.35)).min(1.5);

        view.drawFunc = {
            var plotRect = Rect(plotLeft, 8, plotWidth, h - 22);
            var midY = plotRect.top + (plotRect.height * 0.5);

            Pen.fillColor = Color(0.08, 0.08, 0.10);
            Pen.fillRect(plotRect);
            Pen.strokeColor = Color.gray(0.3);
            Pen.strokeRect(plotRect);

            Pen.push;
            Pen.addRect(plotRect);
            Pen.clip;

            // Waveform fill
            if(wave.notNil and: { wave.size >= 2 }) {
                var numPairs = (wave.size / 2).asInteger;
                Pen.strokeColor = Color(0.2, 0.55, 0.8, 0.8);
                Pen.width = 1;
                numPairs.do { |i|
                    var x = plotRect.left + (i / numPairs * plotRect.width);
                    var minY = midY - (wave[i * 2] * plotRect.height * 0.45);
                    var maxY = midY - (wave[(i * 2) + 1] * plotRect.height * 0.45);
                    Pen.line(Point(x, minY), Point(x, maxY));
                };
                Pen.stroke;
            };

            // RMS Energy Envelope (symmetric upper and lower contour lines framing waveform core)
            if(rms.notNil and: { rms.size > 1 }) {
                Pen.strokeColor = Color(0.95, 0.82, 0.15, 0.95);
                Pen.width = 1.5;

                // Upper RMS contour
                rms.size.do { |i|
                    var x = plotRect.left + (i / (rms.size - 1) * plotRect.width);
                    var y = midY - (rms[i] * rmsScale * plotRect.height);
                    if(i == 0) { Pen.moveTo(Point(x, y)) } { Pen.lineTo(Point(x, y)) };
                };
                Pen.stroke;

                // Lower RMS contour
                rms.size.do { |i|
                    var x = plotRect.left + (i / (rms.size - 1) * plotRect.width);
                    var y = midY + (rms[i] * rmsScale * plotRect.height);
                    if(i == 0) { Pen.moveTo(Point(x, y)) } { Pen.lineTo(Point(x, y)) };
                };
                Pen.stroke;
            };

            // Onset vertical markers with top pins
            if(onsets.notNil and: { onsets.notEmpty }) {
                Pen.strokeColor = Color(1.0, 0.35, 0.1, 0.85);
                Pen.width = 1.2;
                onsets.do { |t|
                    var x = plotRect.left + ((t / duration).clip(0.0, 1.0) * plotRect.width);
                    Pen.line(Point(x, plotRect.top), Point(x, plotRect.bottom));
                };
                Pen.stroke;

                // Top triangular pins for onsets
                Pen.fillColor = Color(1.0, 0.35, 0.1, 0.95);
                onsets.do { |t|
                    var x = plotRect.left + ((t / duration).clip(0.0, 1.0) * plotRect.width);
                    Pen.beginPath;
                    Pen.moveTo(Point(x - 3, plotRect.top));
                    Pen.lineTo(Point(x + 3, plotRect.top));
                    Pen.lineTo(Point(x, plotRect.top + 6));
                    Pen.lineTo(Point(x - 3, plotRect.top));
                    Pen.fill;
                };
            };

            Pen.pop;

            // Labels
            Pen.stringLeftJustIn("Waveform & Onsets", Rect(8, 8, plotLeft - 10, 16), Font.sansSerif(10, true), Color.white);
            Pen.stringLeftJustIn("RMS", Rect(8, 24, plotLeft - 10, 14), Font.sansSerif(9), Color(0.95, 0.82, 0.15));
            Pen.stringLeftJustIn("Onsets (" ++ onsets.size ++ ")", Rect(8, 38, plotLeft - 10, 14), Font.sansSerif(9), Color(1.0, 0.4, 0.1));

            this.drawTimeAxis(plotRect, duration);
            this.drawCursor(plotRect, duration, getCursorTime.value);
        };

        view.mouseDownAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
        view.mouseMoveAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
    }

    *setupSpecTier { |view, data, plotLeft, plotWidth, h, specImg, colormapName, onSeek, getCursorTime, colormapListeners|
        var duration = data[\duration];
        var img = specImg;
        var cmap = colormapName;

        if(colormapListeners.notNil) {
            colormapListeners.add({ |newSpec, newChroma, newName|
                img = newSpec;
                cmap = newName;
            });
        };

        view.drawFunc = {
            var plotRect = Rect(plotLeft, 6, plotWidth, h - 20);
            var cbRect = Rect(plotRect.right + 12, plotRect.top, 14, plotRect.height);

            if(img.notNil) {
                img.drawInRect(plotRect, img.bounds);
            };

            Pen.strokeColor = Color.gray(0.3);
            Pen.strokeRect(plotRect);

            // Frequency labels
            Pen.stringRightJustIn("2 kHz", Rect(2, plotRect.top + (plotRect.height * 0.05) - 6, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));
            Pen.stringRightJustIn("1 kHz", Rect(2, plotRect.top + (plotRect.height * 0.25) - 6, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));
            Pen.stringRightJustIn("500 Hz", Rect(2, plotRect.top + (plotRect.height * 0.50) - 6, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));
            Pen.stringRightJustIn("200 Hz", Rect(2, plotRect.top + (plotRect.height * 0.75) - 6, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));
            Pen.stringRightJustIn("65 Hz", Rect(2, plotRect.bottom - 14, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));

            this.drawColorbar(cbRect, cmap, "0dB", "-70dB");
            Pen.stringLeftJustIn("Spectrogram (CQT)", Rect(8, 6, plotLeft - 10, 16), Font.sansSerif(10, true), Color.white);
            this.drawTimeAxis(plotRect, duration);
            this.drawCursor(plotRect, duration, getCursorTime.value);
        };

        view.mouseDownAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
        view.mouseMoveAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
    }

    *setupChromaTier { |view, data, plotLeft, plotWidth, h, chromaImg, colormapName, onSeek, getCursorTime, colormapListeners|
        var duration = data[\duration];
        var img = chromaImg;
        var cmap = colormapName;
        var pitchNames = ["B", "A#", "A", "G#", "G", "F#", "F", "E", "D#", "D", "C#", "C"];

        if(colormapListeners.notNil) {
            colormapListeners.add({ |newSpec, newChroma, newName|
                img = newChroma;
                cmap = newName;
            });
        };

        view.drawFunc = {
            var plotRect = Rect(plotLeft, 6, plotWidth, h - 20);
            var cbRect = Rect(plotRect.right + 12, plotRect.top, 14, plotRect.height);
            var binH = plotRect.height / 12;

            if(img.notNil) {
                img.drawInRect(plotRect, img.bounds);
            };

            Pen.strokeColor = Color.gray(0.25);
            Pen.width = 0.5;
            12.do { |i|
                var y = plotRect.top + (i * binH);
                Pen.line(Point(plotRect.left, y), Point(plotRect.right, y));
                Pen.stringRightJustIn(pitchNames[i], Rect(2, y + (binH * 0.5) - 6, plotLeft - 6, 12), Font.sansSerif(9, true), Color.white);
            };
            Pen.stroke;

            Pen.strokeColor = Color.gray(0.3);
            Pen.strokeRect(plotRect);

            this.drawColorbar(cbRect, cmap, "Max", "0");
            Pen.stringLeftJustIn("Chromagram (HPCP)", Rect(8, 6, plotLeft - 10, 16), Font.sansSerif(10, true), Color.white);
            this.drawTimeAxis(plotRect, duration);
            this.drawCursor(plotRect, duration, getCursorTime.value);
        };

        view.mouseDownAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
        view.mouseMoveAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
    }

    *setupPitchTier { |view, data, plotLeft, plotWidth, h, onSeek, getCursorTime|
        var pitchMIDI = data[\pitchMIDI];
        var voicing = data[\voicing];
        var confidence = data[\confidence];
        var duration = data[\duration];
        var validMidi, minNote, maxNote, gridNotes;

        // Auto-scale pitch range to the song's actual voiced range
        validMidi = if(pitchMIDI.notNil and: { voicing.notNil }) {
            pitchMIDI.select { |m, i| (voicing[i] > 0.5) and: { (confidence[i] ? 0) > 0.3 } and: { m > 20 } and: { m < 120 } }
        } { [] };

        if(validMidi.notEmpty) {
            minNote = ((validMidi.minItem / 12).floor * 12 - 12).max(24).asInteger;
            maxNote = ((validMidi.maxItem / 12).ceil * 12 + 12).min(108).asInteger;
        } {
            minNote = 36;
            maxNote = 84;
        };

        if((maxNote - minNote) < 36) {
            maxNote = minNote + 36;
        };

        gridNotes = (minNote, minNote + 12 .. maxNote);

        view.drawFunc = {
            var plotRect = Rect(plotLeft, 6, plotWidth, h - 20);

            Pen.fillColor = Color(0.08, 0.08, 0.10);
            Pen.fillRect(plotRect);

            // Octave grid lines and labels
            gridNotes.do { |midi|
                var normY = (midi - minNote) / (maxNote - minNote);
                var y = plotRect.bottom - (normY * plotRect.height);
                var octave = (midi / 12) - 1;
                Pen.strokeColor = Color.gray(0.25);
                Pen.line(Point(plotRect.left, y), Point(plotRect.right, y));
                Pen.stringRightJustIn("C" ++ octave, Rect(2, y - 6, plotLeft - 6, 12), Font.sansSerif(9, true), Color.gray(0.8));
            };
            Pen.stroke;

            // Plot pitch trajectory inside strictly clipped region
            Pen.push;
            Pen.addRect(plotRect);
            Pen.clip;

            if(pitchMIDI.notNil and: { pitchMIDI.size > 1 }) {
                pitchMIDI.size.do { |i|
                    var note = pitchMIDI[i];
                    var isVoiced = (voicing[i] > 0.5) and: { (confidence[i] ? 0) > 0.3 } and: { note >= minNote } and: { note <= maxNote };
                    if(isVoiced) {
                        var conf = confidence[i].clip(0.3, 1.0);
                        var x = plotRect.left + (i / (pitchMIDI.size - 1) * plotRect.width);
                        var normY = (note - minNote) / (maxNote - minNote);
                        var y = plotRect.bottom - (normY * plotRect.height);
                        Pen.fillColor = Color(0.2, 0.9 * conf, 0.5 * conf + 0.4, conf.max(0.6));
                        Pen.fillOval(Rect(x - 2, y - 2, 4, 4));
                    };
                };
            };
            Pen.pop;

            Pen.strokeColor = Color.gray(0.3);
            Pen.strokeRect(plotRect);

            Pen.stringLeftJustIn("Pitch Track (F0)", Rect(8, 6, plotLeft - 10, 16), Font.sansSerif(10, true), Color.white);
            Pen.stringLeftJustIn("Voiced Pitch", Rect(8, 22, plotLeft - 10, 14), Font.sansSerif(9), Color(0.3, 0.8, 0.5));
            this.drawTimeAxis(plotRect, duration);
            this.drawCursor(plotRect, duration, getCursorTime.value);
        };

        view.mouseDownAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
        view.mouseMoveAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
    }

    *setupDynamicsTier { |view, data, plotLeft, plotWidth, h, onSeek, getCursorTime|
        var centroid = data[\centroid];
        var flatness = data[\flatness];
        var duration = data[\duration];

        view.drawFunc = {
            var plotRect = Rect(plotLeft, 6, plotWidth, h - 20);

            Pen.fillColor = Color(0.08, 0.08, 0.10);
            Pen.fillRect(plotRect);

            Pen.push;
            Pen.addRect(plotRect);
            Pen.clip;

            if(centroid.notNil and: { centroid.size > 1 }) {
                Pen.strokeColor = Color(0.3, 0.7, 1.0, 0.9);
                Pen.width = 1.5;
                centroid.size.do { |i|
                    var x = plotRect.left + (i / (centroid.size - 1) * plotRect.width);
                    var y = plotRect.bottom - ((centroid[i] / 8000).clip(0, 1) * plotRect.height);
                    if(i == 0) { Pen.moveTo(Point(x, y)) } { Pen.lineTo(Point(x, y)) };
                };
                Pen.stroke;
            };

            if(flatness.notNil and: { flatness.size > 1 }) {
                Pen.strokeColor = Color(1.0, 0.5, 0.8, 0.85);
                Pen.width = 1.2;
                flatness.size.do { |i|
                    var x = plotRect.left + (i / (flatness.size - 1) * plotRect.width);
                    var y = plotRect.bottom - (flatness[i].clip(0, 1) * plotRect.height);
                    if(i == 0) { Pen.moveTo(Point(x, y)) } { Pen.lineTo(Point(x, y)) };
                };
                Pen.stroke;
            };

            Pen.pop;

            Pen.strokeColor = Color.gray(0.3);
            Pen.strokeRect(plotRect);

            Pen.stringLeftJustIn("Descriptors", Rect(8, 6, plotLeft - 10, 16), Font.sansSerif(10, true), Color.white);
            Pen.stringLeftJustIn("Centroid", Rect(8, 22, plotLeft - 10, 14), Font.sansSerif(9), Color(0.3, 0.7, 1.0));
            Pen.stringLeftJustIn("Flatness", Rect(8, 36, plotLeft - 10, 14), Font.sansSerif(9), Color(1.0, 0.5, 0.8));

            Pen.stringRightJustIn("8 kHz", Rect(2, plotRect.top - 2, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));
            Pen.stringRightJustIn("0 Hz", Rect(2, plotRect.bottom - 12, plotLeft - 6, 12), Font.sansSerif(9), Color.gray(0.8));

            this.drawTimeAxis(plotRect, duration);
            this.drawCursor(plotRect, duration, getCursorTime.value);
        };

        view.mouseDownAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
        view.mouseMoveAction = { |v, x, y|
            var normX = ((x - plotLeft) / plotWidth).clip(0.0, 1.0);
            onSeek.value(normX * duration);
        };
    }

    *drawCursor { |plotRect, duration, cursorTime|
        var x;
        if(duration.notNil and: { duration > 0 } and: { cursorTime.notNil }) {
            x = plotRect.left + ((cursorTime / duration).clip(0.0, 1.0) * plotRect.width);

            Pen.push;
            // Cursor vertical bar
            Pen.strokeColor = Color(1.0, 0.25, 0.15, 0.95);
            Pen.width = 2.0;
            Pen.line(Point(x, plotRect.top - 2), Point(x, plotRect.bottom + 2));
            Pen.stroke;

            // Cursor top marker / pointer
            Pen.fillColor = Color(1.0, 0.25, 0.15);
            Pen.beginPath;
            Pen.moveTo(Point(x - 5, plotRect.top - 2));
            Pen.lineTo(Point(x + 5, plotRect.top - 2));
            Pen.lineTo(Point(x, plotRect.top + 6));
            Pen.lineTo(Point(x - 5, plotRect.top - 2));
            Pen.fill;

            // Cursor bottom marker / pointer
            Pen.beginPath;
            Pen.moveTo(Point(x - 5, plotRect.bottom + 2));
            Pen.lineTo(Point(x + 5, plotRect.bottom + 2));
            Pen.lineTo(Point(x, plotRect.bottom - 6));
            Pen.lineTo(Point(x - 5, plotRect.bottom + 2));
            Pen.fill;
            Pen.pop;
        };
    }

    *drawTimeAxis { |plotRect, duration|
        var numTicks = 8;
        Pen.strokeColor = Color.gray(0.4);
        Pen.width = 1;
        numTicks.do { |i|
            var tSec = i / (numTicks - 1) * duration;
            var x = plotRect.left + (i / (numTicks - 1) * plotRect.width);
            Pen.line(Point(x, plotRect.bottom), Point(x, plotRect.bottom + 4));
            Pen.stringCenteredIn("%.1fs".format(tSec), Rect(x - 20, plotRect.bottom + 5, 40, 12), Font.sansSerif(9), Color.gray(0.65));
        };
        Pen.stroke;
    }

    *drawColorbar { |rect, colormapName, topLabel, bottomLabel|
        var numSteps = 32;
        var stepH = rect.height / numSteps;
        var table = MIRaColormap.table(colormapName, numSteps);

        numSteps.do { |i|
            var col = table[numSteps - 1 - i];
            var r = Rect(rect.left, rect.top + (i * stepH), rect.width, stepH + 0.5);
            Pen.fillColor = col;
            Pen.fillRect(r);
        };

        Pen.strokeColor = Color.gray(0.3);
        Pen.strokeRect(rect);

        if(topLabel.notNil) {
            Pen.stringLeftJustIn(topLabel, Rect(rect.right + 4, rect.top - 4, 34, 12), Font.sansSerif(8), Color.gray(0.7));
        };
        if(bottomLabel.notNil) {
            Pen.stringLeftJustIn(bottomLabel, Rect(rect.right + 4, rect.bottom - 10, 34, 12), Font.sansSerif(8), Color.gray(0.7));
        };
    }

    *formatTime { |seconds|
        var mins = (seconds / 60).floor.asInteger;
        var secs = seconds % 60;
        ^"%%:%".format(
            if(mins < 10) { "0" } { "" },
            mins,
            if(secs < 10) { "0" } { "" },
            secs.round(0.01)
        )
    }

    *startPlayback { |audioPath, buffer, startTime = 0, onDone|
        this.stopPlayback;
        if(Server.default.serverRunning.not) {
            Server.default.waitForBoot({
                this.startPlayback(audioPath, buffer, startTime, onDone);
            });
            ^this;
        };

        if(buffer.isNil) {
            buffer = Buffer.read(Server.default, audioPath, action: {
                this.startPlayback(audioPath, buffer, startTime, onDone);
            });
            ^this;
        };

        playSynth = {
            var startFrame = (startTime * buffer.sampleRate).asInteger;
            var sig = PlayBuf.ar(buffer.numChannels, buffer.bufnum, BufRateScale.kr(buffer.bufnum),
                startPos: startFrame, loop: 0, doneAction: 2);
            var env = EnvGen.kr(Env.asr(0.01, 1, 0.05), \gate.kr(1), doneAction: 2);
            Out.ar(0, sig * env * 0.4);
        }.play(Server.default);

        NodeWatcher.register(playSynth);
        playSynth.onFree({
            if(onDone.notNil) { onDone.value };
        });
    }

    *stopPlayback {
        if(playSynth.notNil and: { playSynth.isPlaying }) {
            playSynth.set(\gate, 0);
            playSynth = nil;
        };
    }
}
