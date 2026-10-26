# MIR2 Neural

MIR2 Neural is an optional native CPU extension for monophonic pitch analysis.
It implements the PESTO research technique inside SuperCollider and remains
separate from MIR2's classical analyzers. The base MIR2 build does not require
this extension, an inference framework, or model files.

`PitchPESTO` estimates fundamental frequency, learned voicing confidence and
spectral energy from a mono audio signal. A variable-Q frontend feeds a residual
convolutional network and a local pitch-distribution decoder, following
[PESTO v2](https://arxiv.org/html/2508.01488v1) and the
[pinned author implementation](https://github.com/SonyCSLParis/pesto/tree/62bc0c9702558f19af4593752947fb9db1eadac9).
The selected checkpoint is `mir-1k_g7`, trained on MIR-1K.

This is an experimental implementation. Matching the original network's output
establishes port correctness; it does not establish general accuracy on voices,
instruments or mixes. The original model can itself return inaccurate pitches:
for example, the reference synthetic 110 Hz sine settles near 112.14 Hz. Use the
confidence output as model evidence rather than a calibrated probability of a
correct pitch. Polyphonic transcription is outside this analyzer's scope.

## Build and install

Run these commands from this directory, using a matching SuperCollider source
checkout:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release -DSTRICT=ON \
  -DMIR2_NEURAL_PLUGIN=ON -DSC_PATH=/path/to/supercollider \
  -DSCSYNTH=ON -DSUPERNOVA=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /path/to/SuperCollider/Extensions
```

Recompile the class library and open `PitchPESTO` help. Its project overview is
`Guides/MIR2`, supplied by the base MIR2 installation. Set
`MIR2_NEURAL_PLUGIN=OFF` to build only the standalone native probe and tests;
that configuration needs no SuperCollider checkout.

The runtime uses native C++ on the CPU. It needs no Python, PyTorch, ONNX, GPU or
background inference service. Model files are loaded explicitly into a
SuperCollider Buffer before creating a Synth; the audio callback never downloads
or reads a model file.

## Prepare a model

Model weights are not included in the source tree or extension package. The
exporter accepts one pinned upstream checkpoint and verifies its SHA-256 and the
upstream source files before loading them. It does not fetch anything. Use the
following POSIX-shell setup for the offline reference/export environment; adapt
virtual-environment executable paths on Windows:

```sh
python3.9 -m venv build/reference-env
build/reference-env/bin/python -m pip install -r tools/requirements-reference.txt
git clone https://github.com/SonyCSLParis/pesto.git build/pesto-source
git -C build/pesto-source checkout --detach 62bc0c9702558f19af4593752947fb9db1eadac9
build/reference-env/bin/python tools/export_pesto.py \
  --upstream build/pesto-source \
  --output build/model-48000 --sample-rate 48000 --model-only
```

The output directory must be new or empty. The export contains `manifest.json`,
`model.f32` for the native tools, and `model.wav` for `Buffer.read`. The WAV is a
float32 model-data container, not audio to play. Preserve its exact samples: do
not normalize, resample, or convert it to integer PCM.

Export separately for each server sample rate: 16000, 44100, 48000 or 96000 Hz.
The UGen rejects a model whose rate differs from the server's rate. For the help
example, create a `models` directory inside the installed `MIR2Neural` extension
and copy the corresponding `model.wav` there as
`mir-1k_g7-48000.wav` (substitute the actual sample rate). These are local generated
assets; installing the plugin does not install or redistribute them.

Keep a model Buffer allocated and unchanged while any Synth uses it. Several
analyzers may share the same read-only model Buffer. Free their Synths, wait for
`s.sync`, and then free the Buffer. Replacing or editing a model requires creating
a new Synth.

## Timing and behavior

The frontend uses 251 variable-Q bins with `gamma=7`, three bins per semitone and
a fixed 10 ms analysis hop. The encoder crops to 219 bins and produces 384 pitch
activations. The implementation preserves the upstream natural-log-power input,
confidence head, pitch-bin shift and argmax-local weighted average.

The selected mode is a trailing, zero-initialized stream (`mirror=0`). Inference
work is spread over the next 10 ms hop, adding one hop of pipeline delay. Results
are held between updates. All outputs remain zero until a complete input history
and its inference are available.

| Server rate | Window | Nominal center lag, including pipeline | Initial warmup |
|---|---:|---:|---:|
| 16 kHz | 4096 samples | 138 ms | 270 ms |
| 44.1 kHz | 8192 samples | 102.88 ms | 200 ms |
| 48 kHz | 8192 samples | 95.33 ms | 190 ms |
| 96 kHz | 16384 samples | 95.33 ms | 190 ms |

Control-block scheduling adds up to one block to those times. Center lag denotes
the temporal position of a window, not a guarantee that pitch changes settle at
that exact time. This mode does not implement the paper's optional refill method
or claim its sub-10-ms latency. Creating an analyzer also validates the model and
initializes its state; prepare analysis Synths before a timing-critical passage.

A small checkpoint does not mean negligible CPU cost. The encoder performs about
28.46 million multiply-accumulates per analysis frame, plus frontend work.
Distributed processing reduces bursts, but available polyphony still depends on
the CPU, server block size and other Synths. Prefer 128-sample blocks at 96 kHz:
the local 64-sample timing run contained one deadline overrun. Native portability,
longer live-server endurance and recording-based accuracy remain separate qualification
work; consult the [implementation report and measured limits](../docs/neural-pitch-implementation.md).

Short live checks at 48 kHz/block 64 also cover two analyzers sharing one model,
independent silence handling and 64 create/free cycles on both scsynth and
supernova. Parallel groups reduce the measured two-instance supernova load on
the test host; available performance still depends on the complete graph.

## Verification tools

Omit `--model-only` when exporting to generate deterministic streaming reference
fixtures, including silence, tones, pitch steps, noise and a chirp. The native
probe accepts `model.f32`, an exported audio fixture and a new output prefix:

```sh
build/pesto_probe build/model-48000/model.f32 \
  build/model-48000/sine440.audio.f32 build/native-sine440
```

The probe writes native features, activations, results and per-frame timings.
No weights or fixture binaries need to enter source control. The pinned offline
packages are only required to export and compare reference results.

## License and provenance

MIR2 Neural is distributed under the parent project's GPL-3.0-or-later license.
Its PESTO adaptation and upstream provenance are documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the upstream LGPL-3.0 text is
preserved in [licenses/LGPL-3.0.md](licenses/LGPL-3.0.md).

The pinned PESTO package metadata explicitly includes `weights/*.ckpt` and labels
the package LGPLv3. No separate checkpoint-specific terms were found in that
revision. This prototype uses explicitly exported local assets; it does not
bundle model weights or represent the training-dataset license as a model
redistribution grant. See the broader [neural source audit](../docs/neural-pitch-research.md).
