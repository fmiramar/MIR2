#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify the installed optional extension and its actual audible help graph.

All renders/logs go into a new output directory. Model files must already have
been explicitly exported. No installation, downloads or model conversion occur.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sclang", required=True, type=Path)
    parser.add_argument("--exports", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error("Output directory must be new")
    output.mkdir(parents=True)
    tests = Path(__file__).resolve().parent
    sclang = str(args.sclang.resolve(strict=True))

    def run(name, arguments, marker):
        with (output / (name + ".log")).open("x") as log:
            result = subprocess.run([sclang, "-D", *map(str, arguments)],
                                    stdout=log, stderr=subprocess.STDOUT, timeout=120)
        text = (output / (name + ".log")).read_text(errors="replace")
        if result.returncode or marker not in text or re.search(r"\b(?:ERROR|WARNING|FAILURE IN SERVER)\b", text):
            raise RuntimeError(name + " failed; inspect " + str(output / (name + ".log")))
        print(name + " PASS", flush=True)

    profiles = [(16000, 64, "scsynth"), (44100, 32, "scsynth"),
                (48000, 64, "scsynth"), (96000, 128, "scsynth"),
                (48000, 64, "supernova")]
    for rate, block, server in profiles:
        model = args.exports.resolve() / ("export-" + str(rate)) / "model.wav"
        if not model.is_file():
            raise FileNotFoundError(model)
        label = "{}-{}-{}".format(rate, block, server)
        run("contracts-" + label,
            [tests / "sc/contracts.scd", output / ("contracts-" + label), model, rate, block, server],
            "MIR2_NEURAL_CONTRACTS_OK")
        script = output / ("example-" + label + ".scd")
        subprocess.run([sys.executable, str(tests / "generate_example_check.py"),
                        "--output-script", str(script), "--prefix", str(script.with_suffix("")),
                        "--model", str(model), "--sample-rate", str(rate),
                        "--block-size", str(block), "--server", server], check=True)
        run("example-" + label, [script], "MIR2_NEURAL_EXAMPLE_NRT_OK")
    run("scdoc", [tests / "sc/verify_scdoc.scd"], "MIR2_NEURAL_SCDOC_OK")
    print("MIR2_NEURAL_INSTALLED_CHECKS_OK", flush=True)


if __name__ == "__main__":
    main()
