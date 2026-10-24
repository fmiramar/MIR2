# Benchmarks

`realtime_cpu.scd` measures warmed server CPU for representative FFT and pitch
analyzers. Results are local comparisons, not portable percentages. Record the
operating system, CPU architecture, SuperCollider version, sample rate, audio
block size, server block size, frame size, hop, and instance count.

Benchmark 1, 8, and 32 instances where stable. Pitch cost must be compared at
the same frame and hop configuration; changing either materially changes the
amount of analysis work.


`chroma_gate.cpp` and `chroma-gate-summary.csv` compare folded constant-Q,
HPCP and FFT chromagrams on reproducible synthetic signals at three sample
rates. The method, latency tradeoffs and deliberately limited inclusion decision
are documented in [the ChromaCQT evaluation](../docs/chroma-cqt-evaluation.md).

`modulation_cpu.cpp` measures bounded per-band projection and shaping cost.
The configuration, results and per-update burst limits are documented in
[the rhythm modulation report](../docs/rhythm-modulation-implementation.md).
