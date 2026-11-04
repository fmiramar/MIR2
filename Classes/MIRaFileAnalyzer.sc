// SPDX-License-Identifier: GPL-3.0-or-later
/*
    MIRaFileAnalyzer: High-speed whole-audio-file MIR feature extraction engine.
    Extracts multi-tier musical descriptors from any audio file using MIRa's native UGens:
    - Waveform peak and RMS amplitude envelopes
    - Transient onset detection and event timestamps (Onsets2)
    - Fundamental frequency F0, voicing probability, and confidence (PitchYIN)
    - 12-pitch-class harmonic chromagram profile (HPCP)
    - Constant-Q time-frequency transform matrix (CQT)
    - Spectral centroid and spectral flatness (SpecCentroid2, SpecFlatness2)
*/

MIRaFileAnalyzer {
    *analyze { |audioPath, hopHz = 100, numCQT = 72, fmin = 65.4, action|
        var resolvedPath, sf, duration, sampleRate, numChannels;
        var oscPath, outPath, synthDef, score, options;
        var totalChannels, uid, sdefName;

        resolvedPath = audioPath.standardizePath;
        if(File.exists(resolvedPath).not) {
            // Try resolving via MIRaGUI soundPath if a basename was provided
            var alt = MIRaGUI.soundPath(audioPath);
            if(alt.notNil and: { File.exists(alt) }) {
                resolvedPath = alt;
            } {
                Error("MIRaFileAnalyzer: audio file not found: %".format(audioPath)).throw;
            };
        };

        sf = SoundFile.openRead(resolvedPath);
        if(sf.isNil) {
            Error("MIRaFileAnalyzer: unable to open audio file: %".format(resolvedPath)).throw;
        };

        duration = sf.duration;
        sampleRate = sf.sampleRate;
        numChannels = sf.numChannels;
        sf.close;

        // Channel allocation:
        // [0]: RMS
        // [1]: Pitch F0 (Hz)
        // [2]: Voicing flag (0 or 1)
        // [3]: Confidence (0..1)
        // [4]: Spectral Centroid (Hz)
        // [5]: Spectral Flatness (0..1)
        // [6]: Onset trigger
        // [7..18]: 12 HPCP Chroma bins
        // [19 .. 19 + numCQT - 1]: numCQT CQT bins
        uid = UniqueID.next;
        sdefName = (\miraFileExtractor_ ++ uid).asSymbol;

        totalChannels = 7 + 12 + numCQT;

        oscPath = PathName.tmp +/+ "mira_extract_%.osc".format(uid);
        outPath = PathName.tmp +/+ "mira_extract_%.wav".format(uid);

        synthDef = SynthDef(sdefName, { |bufnum|
            var sig = PlayBuf.ar(numChannels, bufnum, BufRateScale.kr(bufnum), loop: 0);
            var mono = if(numChannels > 1) { Mix(sig) * 0.5 } { sig };
            var chain = FFT(LocalBuf(1024), mono);
            var pitch = PitchYIN.kr(mono, minFreq: 50, maxFreq: 1500);
            var hpcp = HPCP.kr(chain, 12, 1024);
            var cqt = CQT.kr(mono, fmin: fmin, numBins: numCQT, binsPerOctave: 12);
            var centroid = SpecCentroid2.kr(chain, 40, 12000, 0, 1024);
            var flatness = SpecFlatness2.kr(chain, 0, 1024);
            var onsets = Onsets2.kr(chain, threshold: 0.3);
            var rms = RunningSum.kr(mono.squared, 512).sqrt;

            var controlSignals = [rms, pitch[0], pitch[1], pitch[2], centroid, flatness, onsets]
                ++ hpcp ++ cqt;

            Out.ar(0, K2A.ar(controlSignals));
        });

        score = Score([
            [0.0, [\b_allocRead, 0, resolvedPath]],
            [0.0, [\d_recv, synthDef.asBytes]],
            [0.0, [\s_new, sdefName, 1000, 0, 0, \bufnum, 0]],
            [duration + 0.08, [\n_free, 1000]]
        ]);

        options = ServerOptions.new;
        options.numOutputBusChannels = totalChannels;
        options.numInputBusChannels = 0;
        options.numWireBufs = 512;
        options.blockSize = 64;
        options.memSize = 65536;

        score.recordNRT(
            oscFilePath: oscPath,
            outputFilePath: outPath,
            sampleRate: sampleRate,
            headerFormat: "wav",
            sampleFormat: "float",
            options: options,
            duration: duration + 0.08,
            action: {
                var resFile = SoundFile.openRead(outPath);
                var totalFrames, rawData;
                var hopSamples, numSampled;
                var rmsArr, pitchArr, pitchMIDIArr, voicingArr, confArr, centroidArr, flatnessArr;
                var onsetList, chromaMat, cqtMat, timeArr;
                var waveData, resultDict;

                if(resFile.isNil) {
                    File.delete(oscPath);
                    Error("MIRaFileAnalyzer: failed to read analysis output").throw;
                };

                totalFrames = resFile.numFrames;
                rawData = FloatArray.newClear(totalFrames * totalChannels);
                resFile.readData(rawData);
                resFile.close;

                File.delete(oscPath);
                File.delete(outPath);

                // Downsample to hopHz
                hopSamples = (sampleRate / hopHz.max(10)).round.asInteger;
                numSampled = (totalFrames / hopSamples).floor.asInteger;

                rmsArr = FloatArray.newClear(numSampled);
                pitchArr = FloatArray.newClear(numSampled);
                pitchMIDIArr = FloatArray.newClear(numSampled);
                voicingArr = FloatArray.newClear(numSampled);
                confArr = FloatArray.newClear(numSampled);
                centroidArr = FloatArray.newClear(numSampled);
                flatnessArr = FloatArray.newClear(numSampled);
                timeArr = FloatArray.newClear(numSampled);
                onsetList = List.new;
                chromaMat = Array.newClear(numSampled);
                cqtMat = Array.newClear(numSampled);

                numSampled.do { |k|
                    var frameIdx = k * hopSamples;
                    var offset = frameIdx * totalChannels;
                    var tSec = frameIdx / sampleRate;
                    var pHz, pMIDI;

                    timeArr[k] = tSec;
                    rmsArr[k] = rawData[offset + 0];
                    pHz = rawData[offset + 1];
                    pitchArr[k] = pHz;

                    pMIDI = if(pHz > 20) { pHz.cpsmidi } { 0.0 };
                    pitchMIDIArr[k] = pMIDI;

                    voicingArr[k] = rawData[offset + 2];
                    confArr[k] = rawData[offset + 3];
                    centroidArr[k] = rawData[offset + 4];
                    flatnessArr[k] = rawData[offset + 5];

                    if(rawData[offset + 6] > 0.5) {
                        onsetList.add(tSec);
                    };

                    chromaMat[k] = rawData[(offset + 7) .. (offset + 18)].asArray;
                    cqtMat[k] = rawData[(offset + 19) .. (offset + 19 + numCQT - 1)].asArray;
                };

                // Read downsampled waveform directly from audio file for display
                waveData = this.readWaveform(resolvedPath, 2000);

                resultDict = Dictionary.newFrom([
                    \path, resolvedPath,
                    \duration, duration,
                    \sampleRate, sampleRate,
                    \numChannels, numChannels,
                    \numFrames, numSampled,
                    \hopHz, hopHz,
                    \time, timeArr,
                    \rms, rmsArr,
                    \pitch, pitchArr,
                    \pitchMIDI, pitchMIDIArr,
                    \voicing, voicingArr,
                    \confidence, confArr,
                    \centroid, centroidArr,
                    \flatness, flatnessArr,
                    \onsets, onsetList.asArray,
                    \chroma, chromaMat,
                    \cqt, cqtMat,
                    \numCQT, numCQT,
                    \fmin, fmin,
                    \waveform, waveData
                ]);

                if(action.notNil) {
                    { action.value(resultDict) }.defer;
                };
            }
        );
    }

    *readWaveform { |audioPath, targetPoints = 2000|
        var sf = SoundFile.openRead(audioPath);
        var totalFrames, numChannels, raw, result;
        if(sf.isNil) { ^FloatArray.new };

        totalFrames = sf.numFrames;
        numChannels = sf.numChannels;
        if(totalFrames <= 0) {
            sf.close;
            ^FloatArray.newClear(targetPoints * 2);
        };

        raw = FloatArray.newClear(totalFrames * numChannels);
        sf.readData(raw);
        sf.close;

        result = FloatArray.newClear(targetPoints * 2);
        targetPoints.do { |i|
            var start = ((i * totalFrames) / targetPoints).floor.asInteger;
            var finish = (((i + 1) * totalFrames) / targetPoints).ceil.asInteger.min(totalFrames);
            var minVal = 0.0, maxVal = 0.0;
            if(start < finish) {
                var first = true;
                (start .. finish - 1).do { |f|
                    var idx = f * numChannels;
                    if(idx < raw.size) {
                        var sample = raw[idx];
                        if(sample.notNil) {
                            if(first) {
                                minVal = sample;
                                maxVal = sample;
                                first = false;
                            } {
                                minVal = minVal.min(sample);
                                maxVal = maxVal.max(sample);
                            };
                        };
                    };
                };
            };
            result[i * 2] = minVal;
            result[(i * 2) + 1] = maxVal;
        };
        ^result
    }

    *extractWaveformPeaks { |audioPath, numPeaks = 1000|
        ^this.readWaveform(audioPath, numPeaks)
    }
}
