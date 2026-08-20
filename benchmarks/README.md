# Benchmarks

`realtime_cpu.scd` measures warmed server CPU for representative FFT and pitch
analyzers. Results are local comparisons, not portable percentages. Record the
operating system, CPU architecture, SuperCollider version, sample rate, audio
block size, server block size, frame size, hop, and instance count.

Benchmark 1, 8, and 32 instances where stable. Pitch cost must be compared at
the same frame and hop configuration; changing either materially changes the
amount of analysis work.

