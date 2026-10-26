#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Export the pinned PESTO mir-1k_g7 model and streaming parity fixtures.

This is an offline research tool, not part of SuperCollider's runtime. It never
fetches source or weights. The supplied checkout and checkpoint are verified
before any upstream Python code or checkpoint is loaded. Generated weights and
fixtures belong in an ignored build directory, not in the source repository.

Raw files contain IEEE-754 float32 values in little-endian, C-order layout.
model.f32 has a 32-float header, flat network weights, 251 five-float kernel
records, and compact convolution kernels. model.wav stores those same values
in a mono IEEE float WAV for explicit Buffer.read in SuperCollider.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import sys


REVISION = "62bc0c9702558f19af4593752947fb9db1eadac9"
CHECKPOINT = "pesto/weights/mir-1k_g7.ckpt"
CHECKPOINT_SHA256 = "16c32e06ddd950e3e4866dfa3c7f8a87c4988f8adf43e57977b189f031f26f3e"
REFERENCE_VERSIONS = {"numpy": "1.26.4", "scipy": "1.13.1", "torch": "2.2.2", "torchaudio": "2.2.2"}
# Pin executable source too: a checkpoint hash alone does not establish that
# the supplied frontend and decoder are the implementations being compared.
SOURCE_SHA256 = {
    "pesto/__init__.py": "d481e92587850d6ff8f6a9def60d37becc9ffd50551354fd40f8cde183601ee2",
    "pesto/core.py": "ca743aa5392b86c1a18508ca8ce670a6f889b03136d333e1acc37dba5a07abd5",
    "pesto/loader.py": "51fd12b871e82a19406570461439a565bf84b33b3ceab70370b6d1170a48985e",
    "pesto/model.py": "b9ee5f15b8f30a0731ca39657e1b953f17dbbbd540bdedd55c7f998388806435",
    "pesto/data.py": "a8dd2b3615622a2fb02b2016a28fcb60aa74d0d640de2f480112ce6783c77bda",
    "pesto/utils/__init__.py": "fc3f9adfd468ca94b6f5a8a929f5db64dadc6332fbe96f06a54782b2321a7a5d",
    "pesto/utils/crop_cqt.py": "cfadc4faa920f4974a35b71a68d5946bc074ab37a8756728130e0301e6bd1cfd",
    "pesto/utils/reduce_activations.py": "59454ce99765060e372f3b79adb0b16c632af4b5010b02d829d11b111d32ddc1",
    "pesto/utils/hcqt.py": "74acd096cdb08ac4dd984985912b96d6765f11ec0890aa9b3b6ee606aac926f2",
    "pesto/utils/cached_conv.py": "86f9d4fdf9293b5694f8b6b40e19b08656476a07ad82e23e9f8c3df7ffdcdc14",
    "pesto/utils/export.py": "99b1ebeeb4e81ff59c2f77bca2922c0c8b85626d5c46e774480491924bad4226",
}
WEIGHT_NAMES = [
    "encoder.layernorm.weight", "encoder.layernorm.bias",
    "encoder.conv1.0.weight", "encoder.conv1.0.bias",
    "encoder.prefilt_layers.0.0.weight", "encoder.prefilt_layers.0.0.bias",
    "encoder.prefilt_layers.1.0.weight", "encoder.prefilt_layers.1.0.bias",
    "encoder.conv_layers.0.weight", "encoder.conv_layers.0.bias",
    "encoder.conv_layers.3.weight", "encoder.conv_layers.3.bias",
    "encoder.conv_layers.6.weight", "encoder.conv_layers.6.bias",
    "encoder.conv_layers.9.weight", "encoder.conv_layers.9.bias",
    "encoder.fc.weight", "confidence.conv.weight", "confidence.conv.bias",
    "confidence.linear.weight", "confidence.linear.bias", "shift",
]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_upstream(root):
    for relative, expected in {**SOURCE_SHA256, CHECKPOINT: CHECKPOINT_SHA256}.items():
        path = root / relative
        if not path.is_file() or sha256(path) != expected:
            raise ValueError("Pinned upstream file missing or changed: " + relative)


def make_signals(np, rate, hop, seconds):
    """Deterministic synthetic fixtures, each starting with a fresh zero cache."""
    frames = max(20, int(round(seconds * rate / hop)))
    samples = frames * hop
    t = np.arange(samples, dtype=np.float64) / rate
    silence = np.zeros(samples, dtype=np.float32)
    yield "silence", silence, {"kind": "silence"}
    for frequency in (110.0, 440.0):
        yield "sine" + str(int(frequency)), (0.2 * np.sin(2 * np.pi * frequency * t)), {
            "kind": "sine", "frequency_hz": frequency, "amplitude": 0.2,
        }
    # The fixture is at least one second to give each pitch a usable steady
    # portion after the finite frontend's history has been replaced.
    step_frames = max(frames, 100)
    step_samples = step_frames * hop
    steps = np.array([110.0, 220.0, 330.0, 165.0])
    step_index = np.minimum(np.arange(step_samples) * 4 // step_samples, 3)
    frequency = steps[step_index]
    phase = np.cumsum(frequency, dtype=np.float64) * (2 * np.pi / rate)
    harmonic = sum(np.sin(h * phase) / h for h in range(1, 9)) * 0.12
    boundaries = [int(np.ceil(i * step_samples / 4)) for i in range(5)]
    yield "harmonic_steps", harmonic, {
        "kind": "harmonic_steps", "frequency_hz": steps.tolist(),
        "sample_boundaries": boundaries, "harmonics": 8,
    }
    rng = np.random.Generator(np.random.PCG64(20261025))
    yield "noise", 0.08 * rng.standard_normal(samples), {
        "kind": "gaussian_noise", "seed": 20261025, "standard_deviation": 0.08,
    }
    low, high = 82.406889, 880.0
    slope = math.log(high / low) / (samples / rate)
    phase = (2 * np.pi * low / slope) * np.expm1(slope * t)
    yield "chirp", 0.2 * np.sin(phase), {
        "kind": "exponential_chirp", "start_hz": low, "end_hz": high,
    }
    yield "quiet_sine", 1e-6 * np.sin(2 * np.pi * 220 * t), {
        "kind": "sine", "frequency_hz": 220.0, "amplitude": 1e-6,
    }
    levels = np.array([0.2, 0.002, 0.0, 0.2])
    amplitudes = levels[np.minimum(np.arange(samples) * 4 // samples, 3)]
    yield "amplitude_steps", amplitudes * np.sin(2 * np.pi * 220 * t), {
        "kind": "amplitude_steps", "frequency_hz": 220.0,
        "levels": levels.tolist(),
        "sample_boundaries": [int(np.ceil(i * samples / 4)) for i in range(5)],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", type=Path, required=True,
                        help="local checkout/archive of the exact pinned upstream revision")
    parser.add_argument("--output", type=Path, required=True,
                        help="new or empty ignored output directory; existing files are never overwritten")
    parser.add_argument("--sample-rate", type=int, choices=(16000, 44100, 48000, 96000), default=48000)
    parser.add_argument("--seconds", type=float, default=0.5,
                        help="duration of most fixtures (0.2..30; harmonic steps are at least 1 second)")
    parser.add_argument("--model-only", action="store_true", help="omit reference audio fixtures")
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 0.2 <= args.seconds <= 30:
        parser.error("--seconds must be finite and between 0.2 and 30")
    root = args.upstream.resolve()
    output = args.output.resolve()
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        parser.error("Output must be new or empty; refusing to overwrite existing contents")
    if root == output or root in output.parents:
        parser.error("Keep generated artifacts outside the upstream checkout")
    verify_upstream(root)

    # Import only after verifying every upstream module executed by pesto's
    # package import. The pinned checkpoint is checked before torch.load below.
    import numpy as np
    import scipy
    from scipy.io import wavfile
    import torch
    import torchaudio
    for module in (np, scipy, torch, torchaudio):
        if module.__version__.split("+")[0] != REFERENCE_VERSIONS[module.__name__]:
            raise RuntimeError("Use requirements-reference.txt; unexpected {} version {}".format(
                module.__name__, module.__version__))
    sys.path.insert(0, str(root))
    import pesto
    from pesto.loader import load_model
    if Path(pesto.__file__).resolve() != root / "pesto/__init__.py":
        raise RuntimeError("Imported pesto does not belong to the verified checkout")
    torch.set_num_threads(1)
    torch.set_num_interop_threads(1)
    torch.manual_seed(20261025)
    torch.use_deterministic_algorithms(True)

    rate = args.sample_rate
    hop = int(0.01 * rate + 0.5)
    model = load_model(str(root / CHECKPOINT), 10.0, sampling_rate=rate,
                       streaming=True, mirror=0.0).cpu().eval()
    frontend = model.preprocessor.hcqt_kernels.cqt_kernels[0]
    state = model.state_dict()
    weights, weight_records, offset = [], [], 0
    for name in WEIGHT_NAMES:
        value = state[name].detach().cpu().numpy().astype("<f4", copy=True)
        if not np.isfinite(value).all():
            raise RuntimeError("Non-finite checkpoint tensor: " + name)
        count = value.size
        weights.append(value.reshape(-1))
        weight_records.append({"name": name, "shape": list(value.shape),
                               "offset": offset, "count": count})
        offset += count
    weights = np.concatenate(weights)
    bins = 251
    width = frontend.kernel_width
    kernels = frontend.conv.weight.detach().cpu().numpy()[:, 0, :]
    if kernels.shape != (2 * bins, width) or frontend.conv.cache.padding != width - hop:
        raise RuntimeError("Unexpected frontend shape or streaming padding")
    lengths = frontend.sqrt_lengths.detach().cpu().numpy().reshape(-1)
    compact, records, descriptors, offset = [], [], [], 0
    for index in range(bins):
        real, imaginary = kernels[index], kernels[index + bins]
        nonzero = np.flatnonzero((real != 0) | (imaginary != 0))
        if not nonzero.size:
            raise RuntimeError("Empty frontend kernel")
        start, end = int(nonzero[0]), int(nonzero[-1] + 1)
        length = end - start
        record = {"start": start, "length": length, "real_offset": offset,
                  "imag_offset": offset + length, "sqrt_length": float(lengths[index])}
        records.append(record)
        descriptors.append(list(record.values()))
        compact.extend([real[start:end], imaginary[start:end]])
        offset += 2 * length
    compact = np.concatenate(compact).astype("<f4", copy=False)
    descriptors = np.asarray(descriptors, dtype="<f4").reshape(-1)
    total = 32 + weights.size + descriptors.size + compact.size
    # Float header integers must be exact; this also bounds export sizes.
    if total >= 2 ** 24:
        raise RuntimeError("Model format exceeds exact float32 integer range")
    header = np.zeros(32, dtype="<f4")
    header[:9] = [734201, 1, rate, hop, width, bins, weights.size, compact.size, total]
    packed = np.concatenate([header, weights, descriptors, compact]).astype("<f4", copy=False)
    if not np.isfinite(packed).all():
        raise RuntimeError("Non-finite exported model")
    output.mkdir(parents=True, exist_ok=True)
    files = {}

    def write_array(name, array):
        value = np.asarray(array, dtype="<f4", order="C")
        if not np.isfinite(value).all():
            raise RuntimeError("Non-finite fixture data: " + name)
        path = output / name
        with path.open("xb") as stream:
            stream.write(value.tobytes(order="C"))
        files[name] = {"sha256": sha256(path), "shape": list(value.shape),
                       "bytes": path.stat().st_size, "dtype": "float32-le"}
        return name

    write_array("weights.f32", weights)
    write_array("kernels.f32", compact)
    write_array("model.f32", packed)
    with (output / "model.wav").open("xb") as stream:
        wavfile.write(stream, rate, packed)
    files["model.wav"] = {"sha256": sha256(output / "model.wav"),
                          "bytes": (output / "model.wav").stat().st_size,
                          "encoding": "WAVE_FORMAT_IEEE_FLOAT", "channels": 1,
                          "sample_rate": rate, "samples": int(total)}
    fixtures = []
    if not args.model_only:
        captured = []

        def before_crop(_module, inputs):
            # This hook sees all 251 bins AFTER PESTO.forward's in-place
            # dB * ln(10)/10 conversion, before its 16:-16 frequency crop.
            captured.append(inputs[0].detach().cpu().numpy().reshape(bins).copy())

        hook = model.crop_cqt.register_forward_pre_hook(before_crop)
        try:
            with torch.inference_mode():
                for name, signal, description in make_signals(np, rate, hop, args.seconds):
                    audio = np.asarray(signal, dtype=np.float32)
                    assert len(audio) % hop == 0
                    frames = len(audio) // hop
                    captured.clear()
                    frontend.conv.cache.pad.zero_()
                    predictions, activations = [], []
                    for begin in range(0, len(audio), hop):
                        chunk = torch.from_numpy(audio[begin:begin + hop])
                        midi, confidence, energy, activation = model(
                            chunk, convert_to_freq=False, return_activations=True)
                        if midi.numel() != 1 or activation.numel() != 384:
                            raise RuntimeError("Expected exactly one prediction per hop")
                        frequency = 440 * torch.pow(2, (midi - 69) / 12)
                        predictions.append([frequency.item(), confidence.item(), energy.item(), midi.item()])
                        activations.append(activation.detach().cpu().numpy().reshape(384).copy())
                    if len(captured) != frames:
                        raise RuntimeError("Reference frontend frame count mismatch")
                    fixture_files = {
                        "audio": write_array(name + ".audio.f32", audio),
                        "logpower": write_array(name + ".logpower.f32", np.asarray(captured)),
                        "activations": write_array(name + ".activations.f32", np.asarray(activations)),
                        "results": write_array(name + ".results.f32", np.asarray(predictions)),
                    }
                    fixtures.append({"name": name, "frames": frames, "samples": len(audio),
                                     "files": fixture_files, "signal": description})
                    print("Exported {}: {} frames".format(name, frames), flush=True)
        finally:
            hook.remove()
    manifest = {
        "format": "mir2-pesto-research", "format_version": 1,
        "source": {"repository": "https://github.com/SonyCSLParis/pesto",
                   "revision": REVISION, "sha256": SOURCE_SHA256},
        "checkpoint": {"path": CHECKPOINT, "sha256": CHECKPOINT_SHA256,
                       "bytes": (root / CHECKPOINT).stat().st_size},
        "environment": {"python": platform.python_version(), "platform": platform.system(),
                        "architecture": platform.machine(), "torch": torch.__version__,
                        "torchaudio": torchaudio.__version__, "numpy": np.__version__,
                        "scipy": scipy.__version__, "torch_threads": 1},
        "frontend": {"sample_rate": rate, "hop_samples": hop, "hop_ms": 10.0,
                     "kernel_width": width, "bins": bins, "streaming": True,
                     "mirror": 0.0, "initial_cache": "zeros", "padding_samples": width - hop,
                     "kernel_sign": "stored real and negative imaginary, used as cross-correlation",
                     "normalization": "multiply each real/imaginary sum by sqrt_length before magnitude",
                     "logpower": "20*log10(max(magnitude,float32_epsilon)) * (ln(10)/10)",
                     "bins_metadata": records, "kernel_float_count": int(compact.size)},
        "network": {"weights": weight_records, "weight_count": int(weights.size),
                    "crop": [16, -16], "input_bins": 219, "output_bins": 384,
                    "activation_roll": -int(torch.round(model.shift * 3).item()),
                    "reduction": "ALWA, offsets -2..2, clamped indices with boundary duplicates"},
        "packed_model": {"header_floats": 32, "magic": 734201, "version": 1,
                         "descriptor_floats_per_bin": 5,
                         "descriptor_fields": ["start", "length", "real_offset", "imag_offset", "sqrt_length"],
                         "kernel_offsets_relative_to": "first packed kernel float",
                         "total_floats": int(total)},
        "reference": {"frame_order": "first frame after first input hop; output includes zero-history warmup",
                      "reset": "fresh zero frontend history for each fixture",
                      "result_columns": ["frequency_hz", "confidence", "energy", "midi"],
                      "silence": "upstream raw predictions, without MIR2 silence or voiced gating"},
        "files": files, "fixtures": fixtures,
        "redistribution": "Local research artifacts; no new license grant for upstream weights is asserted.",
    }
    with (output / "manifest.json").open("x", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print("PESTO_EXPORT_OK rate={} weights={} kernel_floats={} total_floats={}".format(
        rate, weights.size, compact.size, total), flush=True)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError) as error:
        print("export_pesto: " + str(error), file=sys.stderr)
        sys.exit(1)
