#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check native PESTO against independently exported upstream streaming fixtures.

Run the reference exporter first. This tool verifies its pinned manifest and
artifact hashes, executes the native probe sequentially, and records numerical
errors and warmed per-hop timings. Timing misses are reported, not hidden by the
numerical pass flag. A standalone process benchmark is not an audio-server
real-time qualification. Output must be a new directory; no files are replaced.
"""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys

import numpy as np

from export_pesto import CHECKPOINT_SHA256, REFERENCE_VERSIONS, REVISION, SOURCE_SHA256


# Explicit float32 comparison bounds. Near-zero VQT bins may have appreciable
# log-domain differences from cancellation; compare their linear power relative
# to the frame's peak power, while still reporting the raw log-domain maximum.
LIMITS = {
    "pitch_cents_max": 0.1,
    "confidence_absolute_max": 1e-4,
    # The encoder alone stays below 1e-4 on identical input features. End-to-end
    # float32 frontend cancellation perturbs low-energy bins and raises the
    # measured posterior error to 0.00153, while pitch stays within 0.052 cents.
    "posterior_absolute_max": 0.002,
    "energy_relative_max": 1e-4,
    "frontend_power_peak_relative_max": 1e-4,
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def checked_path(root, filename):
    path = root / filename
    if path.resolve().parent != root.resolve():
        raise ValueError("Manifest file must be directly inside export directory: " + filename)
    return path


def load_manifest(root, rate):
    path = root / "manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("format") != "mir2-pesto-research" or manifest.get("format_version") != 1:
        raise ValueError("Unsupported reference manifest")
    if manifest["source"]["revision"] != REVISION or manifest["source"]["sha256"] != SOURCE_SHA256:
        raise ValueError("Reference source hashes are not the pinned PESTO revision")
    if manifest["checkpoint"]["sha256"] != CHECKPOINT_SHA256:
        raise ValueError("Reference checkpoint is not pinned mir-1k_g7")
    if any(manifest["environment"][name].split("+")[0] != version
           for name, version in REFERENCE_VERSIONS.items()):
        raise ValueError("Unexpected reference framework versions")
    frontend = manifest["frontend"]
    if (frontend["sample_rate"] != rate or frontend["hop_ms"] != 10.0
            or frontend["hop_samples"] != round(rate / 100)
            or frontend["bins"] != 251 or frontend["mirror"] != 0
            or frontend["streaming"] is not True or frontend["initial_cache"] != "zeros"):
        raise ValueError("Unexpected reference frontend configuration")
    if manifest["network"]["weight_count"] != 130575 or manifest["network"]["activation_roll"] != -20:
        raise ValueError("Unexpected reference network configuration")
    if not manifest["fixtures"]:
        raise ValueError("No reference fixtures")
    for name, record in manifest["files"].items():
        artifact = checked_path(root, name)
        if artifact.stat().st_size != record["bytes"] or digest(artifact) != record["sha256"]:
            raise ValueError("Reference artifact size or hash differs: " + name)
    return manifest


def array(path, shape):
    data = np.fromfile(path, dtype="<f4")
    if data.size != int(np.prod(shape)) or not data.size or not np.isfinite(data).all():
        raise ValueError("Nonfinite, empty or incorrectly sized artifact: " + str(path))
    return data.reshape(shape).astype(np.float64)


def stats(values, hop_us, block_us):
    values = np.asarray(values, dtype=np.float64)
    if not values.size or not np.isfinite(values).all() or np.any(values < 0):
        raise ValueError("Invalid timing samples")
    return {
        "count": int(values.size),
        "median_us": float(np.quantile(values, 0.5)),
        "p95_us": float(np.quantile(values, 0.95)),
        "p99_us": float(np.quantile(values, 0.99)),
        "max_us": float(values.max()),
        "analysis_hop_deadline_us": hop_us,
        "analysis_hop_misses": int(np.count_nonzero(values > hop_us)),
        "audio_block_deadline_us": block_us,
        "audio_block_misses_if_unsliced": int(np.count_nonzero(values > block_us)),
    }


def compare(root, prefix, fixture):
    frames = fixture["frames"]
    if frames <= 0:
        raise ValueError("Empty reference fixture")
    files = fixture["files"]
    expected = array(checked_path(root, files["results"]), (frames, 4))
    actual = array(Path(str(prefix) + ".results.f32"), (frames, 4))
    expected_p = array(checked_path(root, files["activations"]), (frames, 384))
    actual_p = array(Path(str(prefix) + ".activations.f32"), (frames, 384))
    expected_log = array(checked_path(root, files["logpower"]), (frames, 251))
    actual_log = array(Path(str(prefix) + ".logpower.f32"), (frames, 251))
    if np.any(actual[:, 0] <= 0) or np.any(expected[:, 0] <= 0):
        raise ValueError("Reference parity expects ungated positive frequencies")
    if np.any(actual_p < 0) or not np.allclose(actual_p.sum(axis=1), 1.0, rtol=0, atol=2e-5):
        raise ValueError("Invalid native posterior normalization")
    expected_power = np.exp(expected_log)
    actual_power = np.exp(actual_log)
    peak = np.maximum(expected_power.max(axis=1, keepdims=True), 1e-30)
    metrics = {
        "pitch_cents_max": float(np.max(np.abs(1200 * np.log2(actual[:, 0] / expected[:, 0])))),
        "midi_cents_max": float(np.max(np.abs(actual[:, 3] - expected[:, 3])) * 100),
        "confidence_absolute_max": float(np.max(np.abs(actual[:, 1] - expected[:, 1]))),
        "posterior_absolute_max": float(np.max(np.abs(actual_p - expected_p))),
        "energy_relative_max": float(np.max(np.abs(actual[:, 2] - expected[:, 2])
                                             / np.maximum(np.abs(expected[:, 2]), 1e-30))),
        "frontend_power_peak_relative_max": float(np.max(np.abs(actual_power - expected_power) / peak)),
        "frontend_logpower_absolute_max": float(np.max(np.abs(actual_log - expected_log))),
    }
    failed = {key: {"actual": metrics[key], "limit": limit}
              for key, limit in LIMITS.items() if metrics[key] > limit}
    return metrics, failed


def run_stream(probe, root, fixture, prefix, full_prefix, rate, block, warmup, window):
    completed = subprocess.run([str(probe), str(root / "model.f32"),
                                str(checked_path(root, fixture["files"]["audio"])),
                                str(block), str(prefix)], check=True, text=True, capture_output=True)
    summary = json.loads(completed.stdout)
    if summary["frames"] != fixture["frames"]:
        raise ValueError("Stream output frame count differs")
    actual = array(Path(str(prefix) + ".results.f32"), (fixture["frames"], 4))
    expected = array(Path(str(full_prefix) + ".results.f32"), (fixture["frames"], 4))
    identical = Path(str(prefix) + ".results.f32").read_bytes() == Path(str(full_prefix) + ".results.f32").read_bytes()
    timing = np.genfromtxt(str(prefix) + ".timing.csv", delimiter=",", names=True)
    hop = round(rate / 100)
    expected_blocks = (fixture["samples"] + hop + block - 1) // block
    if timing.shape != (expected_blocks,) or not np.array_equal(timing["block"], np.arange(expected_blocks)):
        raise ValueError("Incorrect stream timing count/order")
    published = timing[timing["published"] == 1]
    source_samples = np.arange(1, fixture["frames"] + 1) * hop
    if published.size != fixture["frames"] or not np.array_equal(published["source_end_sample"], source_samples):
        raise ValueError("Incorrect stream result timestamp/order")
    delays = published["end_sample"] - source_samples
    if np.any(delays < hop) or np.any(delays >= hop + block):
        raise ValueError("Stream delay exceeds one hop plus block quantization")
    if not np.array_equal(timing["ready"], (timing["source_end_sample"] >= window).astype(int)):
        raise ValueError("Stream full-history warmup gate differs")
    warmed = timing["total_us"][(timing["end_sample"] - block) >= (warmup + 1) * hop]
    measured = stats(warmed, 10000.0, block * 1e6 / rate)
    measured["audio_block_misses"] = measured.pop("audio_block_misses_if_unsliced")
    return {"block_size": block, "frames": fixture["frames"], "bit_identical_to_unsliced": identical,
            "result_absolute_max": float(np.max(np.abs(actual - expected))),
            "publication_delay_samples_min": int(delays.min()),
            "publication_delay_samples_max": int(delays.max()),
            "full_history_ready_at_sample": int(timing["end_sample"][timing["ready"] == 1][0])
                if np.any(timing["ready"] == 1) else None,
            "probe": summary, "warmed_total": measured}, warmed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--exports", type=Path, required=True, help="Parent of export-<sample-rate> folders")
    parser.add_argument("--output", type=Path, required=True, help="New directory for results and report.json")
    parser.add_argument("--rates", type=int, nargs="+", default=[16000, 44100, 48000, 96000])
    parser.add_argument("--block-size", type=int, default=64)
    parser.add_argument("--warmup-frames", type=int, default=10)
    parser.add_argument("--stream-probe", type=Path, help="Also verify the incremental server-block engine")
    parser.add_argument("--stream-block-sizes", type=int, nargs="+", default=[32, 64, 128])
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError("Refusing to replace an existing output directory")
    if args.block_size <= 0 or args.warmup_frames < 0:
        raise ValueError("Invalid block size or warmup frame count")
    if any(rate not in (16000, 44100, 48000, 96000) for rate in args.rates):
        raise ValueError("Unsupported sample rate")
    if any(block <= 0 or block > min(args.rates) / 100 for block in args.stream_block_sizes):
        raise ValueError("Stream block sizes must fit within the smallest hop")
    manifests = {rate: load_manifest(args.exports / ("export-" + str(rate)), rate) for rate in args.rates}
    probe = args.probe.resolve(strict=True)
    stream_probe = args.stream_probe.resolve(strict=True) if args.stream_probe else None
    args.output.mkdir(parents=True)
    neural = Path(__file__).resolve().parents[1]
    report = {
        "format": "mir2-pesto-native-parity", "version": 1,
        "source_revision": REVISION, "checkpoint_sha256": CHECKPOINT_SHA256,
        "probe_sha256": digest(probe),
        "stream_probe_sha256": digest(stream_probe) if stream_probe else None,
        "native_source_sha256": {str(path.relative_to(neural)): digest(path)
                                 for path in sorted((neural / "include").rglob("*.hpp"))},
        "host": {"system": platform.system(), "release": platform.release(),
                 "architecture": platform.machine(), "python": platform.python_version(),
                 "numpy": np.__version__},
        "limits": LIMITS, "warmup_frames_per_fixture": args.warmup_frames,
        "timing_note": "Sequential standalone whole-hop inference; misses are measured observations, not hard realtime guarantees. Server scheduling must be measured separately.",
        "rates": [], "passed": True,
    }
    for rate in args.rates:
        root = args.exports / ("export-" + str(rate))
        manifest = manifests[rate]
        rate_dir = args.output / str(rate)
        rate_dir.mkdir()
        rate_report = {"sample_rate": rate, "manifest_sha256": digest(root / "manifest.json"),
                       "model_sha256": manifest["files"]["model.f32"]["sha256"],
                       "kernel_width": manifest["frontend"]["kernel_width"],
                       "nominal_frame_center_lag_ms": 500 * manifest["frontend"]["kernel_width"] / rate,
                       "fixtures": []}
        timings = []
        stream_timings = {block: [] for block in args.stream_block_sizes} if stream_probe else {}
        for fixture in manifest["fixtures"]:
            name = fixture["name"]
            if Path(name).name != name or name in (".", ".."):
                raise ValueError("Invalid fixture name")
            prefix = rate_dir / name
            completed = subprocess.run([str(probe), str(root / "model.f32"),
                                        str(checked_path(root, fixture["files"]["audio"])), str(prefix)],
                                       check=True, text=True, capture_output=True)
            probe_report = json.loads(completed.stdout)
            if probe_report["frames"] != fixture["frames"]:
                raise ValueError("Native frame count differs")
            metrics, failed = compare(root, prefix, fixture)
            timing = np.genfromtxt(str(prefix) + ".timing.csv", delimiter=",", names=True)
            if timing.shape != (fixture["frames"],) or not np.array_equal(timing["frame"], np.arange(fixture["frames"])):
                raise ValueError("Invalid native timing frame count/order")
            warmed = timing["total_us"][args.warmup_frames:]
            timing_stats = stats(warmed, 10000.0, args.block_size * 1e6 / rate)
            timings.extend(warmed.tolist())
            item = {"name": name, "frames": fixture["frames"], "metrics": metrics,
                    "failed_limits": failed, "passed": not failed,
                    "probe": probe_report, "warmed_total": timing_stats}
            if stream_probe:
                item["stream"] = []
                for block in args.stream_block_sizes:
                    stream_prefix = rate_dir / (name + "-block" + str(block))
                    stream_report, stream_warmed = run_stream(
                        stream_probe, root, fixture, stream_prefix, prefix, rate, block, args.warmup_frames,
                        manifest["frontend"]["kernel_width"])
                    item["stream"].append(stream_report)
                    stream_timings[block].extend(stream_warmed.tolist())
                    report["passed"] = report["passed"] and stream_report["bit_identical_to_unsliced"]
            rate_report["fixtures"].append(item)
            report["passed"] = report["passed"] and not failed
            print("rate={} {} frames={} cents={:.6g} posterior={:.6g} {}".format(
                rate, name, fixture["frames"], metrics["pitch_cents_max"],
                metrics["posterior_absolute_max"], "PASS" if not failed else "FAIL " + str(failed)), flush=True)
        rate_report["warmed_total"] = stats(timings, 10000.0, args.block_size * 1e6 / rate)
        rate_report["maximum_errors"] = {key: max(item["metrics"][key] for item in rate_report["fixtures"])
                                         for key in rate_report["fixtures"][0]["metrics"]}
        rate_report["stream_warmed_total"] = {}
        for block, samples in stream_timings.items():
            measured = stats(samples, 10000.0, block * 1e6 / rate)
            measured["audio_block_misses"] = measured.pop("audio_block_misses_if_unsliced")
            rate_report["stream_warmed_total"][str(block)] = measured
        report["rates"].append(rate_report)
    with (args.output / "report.json").open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print("PESTO_PARITY_" + ("OK" if report["passed"] else "FAILED"), flush=True)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
