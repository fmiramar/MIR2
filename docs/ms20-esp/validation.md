# ESP validation record

## Baseline

Checkout baseline: 1f1c6d8a796958e795bec9eddac8c40ed060909f, branch feature/ms20-esp. Release scsynth plugin and all 16 original C++ tests passed before implementation. Runtime namespace audit with the checkout's class path confirmed existing Periodicity and sc3-plugins EnvFollow load, while the four proposed names are free. Baseline records and test SHA-256 manifest are in the workspace's audit directory outside the checkout.

## Phase 2 — isolated digital kernels

New files: include/mir2/MS20ESP.hpp and tests/cpp/test_esp_digital.cpp. Existing CMake glob discovers the test without harness edits. No circuit path, plugin registration or SC classes have been added yet.

The focused test covers:

- Clean sine inputs at 55, 110, 440 and 880 Hz at 44100, 48000 and 96000 Hz sample rates. Average event rate over the settled second is within 0.3% of 2f. Periodicity exceeds 0.98 and normalized jitter remains below 0.01.
- Default conditioning at 220 Hz, startup support, silence decay/timeout and a step to 440 Hz.
- Independent 8/12 ms alternating interval oracle: coefficient of variation 0.2, normalized jitter one, regularity exp(-0.2/0.15).
- Irregular intervals, minimum-count gating, duplicates and reversed timestamps, short elapsed-time window, dense events exceeding buffer capacity, stale history and restart.
- Asymmetric 10% duty-cycle waveform with regular rising-edge intervals.
- NaN/Inf input, malformed parameters and reference saturation; all public values remain finite in [0,1].

Commands: `cmake --build build -j 6` and `ctest --test-dir build -R esp_digital --output-on-failure`. Also compile/run the new test with AppleClang C++17, -Wall -Wextra -Wpedantic and address/undefined-behavior sanitizers. Exact numerical output and command logs are in audit/phase-2-*.log outside the checkout.

Implementation inspection confirms fixed-capacity storage, no allocation/locks in process methods, and bounded statistics work. This is not a plugin CPU benchmark or a sample-accurate public trigger guarantee. Full existing-suite recheck follows integration; original test hashes are checked independently after Phase 2.

## Remaining validation

Circuit filter/reference comparisons, reduced monostable calibration, envelope/trigger tests, SC wrapper/output ordering, server loading, examples and CPU measurements remain pending. Supernova configuration currently lacks nova-tt in the local external SC source tree. No SPICE comparison or hardware measurements have been performed.

## Phase 3 — reduced circuit F/V kernel

Added the analog filter sections, coupled resistance mapping, idealized hysteretic crossing frontend, fractional retriggerable pulse, output smoothing/calibration and construction-time mode family in MS20ESP.hpp. Added only tests/cpp/test_esp_circuit.cpp for this phase; no build-harness or old test edits.

The circuit test independently solves the high-pass/low-pass nodal equations in complex arithmetic and compares the implemented cascade's magnitude and phase. Coverage spans low-cut requests 50/80/2500 Hz, high-cut requests 100/2000/5000 Hz, probes 100/500/2000 Hz and sample rates 44.1/48/96 kHz, including resistance limits. The response is compared at the bilinear-mapped analog frequency, with absolute complex-response tolerance 1e-8 plus 1e-7 of reference magnitude.

Further checks cover isolated sub-sample pulses and exact area, retrigger overlap union area, the critically damped output-filter step against its closed-form response, clean-sine mean voltage and event rates at 110/220/440/880 Hz across all three sample rates (0.3% tolerance), persistent overlap/duty saturation invalidity, silence decay, zero gain, clipping and nonfinite input. Both ESP tests pass; the circuit test also passes address/undefined-behavior sanitizers with warning flags enabled. Numerical output is in audit/phase-3-numerical.log outside the checkout.

This establishes consistency with the declared analog equations and software timing replacement. It does not establish the disputed original timing wiring, a SPICE match, exact device behavior or measured hardware fidelity. Server integration and remaining two UGens are still pending.

## Phases 4–6 — envelope, trigger, integration and documentation

EnvFollow2 and TriggerDetector are implemented in MS20ESP.hpp and registered with FreqToVolt and Periodicity2 in plugins/MIR2ESP.cpp. All four SC classes are in Classes/MIR2ESP.sc. Existing-file code edits are limited to one CMake source entry, one registration declaration and one registration call. README/changelog/provenance additions describe the new family. There are exactly three new focused C++ test files and one new SC smoke test; old tests and the discovery harness are unchanged.

The envelope/trigger test checks closed-form capacitor charging and release at 44.1/48/96 kHz, an exact diode-state transition halfway through a sample, circuit half-wave behavior, digital negative-input rectification, trigger activation/hold/release/reactivation, silence at zero threshold, polarity, clipping and nonfinite inputs. It passes address/undefined-behavior sanitizers. The earlier digital and circuit sanitizer checks also passed.

Final Release build succeeded. All **19 CTest tests passed** (16 original plus 3 new), in 21.20 seconds on this run. All original tracked test SHA-256 values match the baseline manifest. `git diff --check` passes. The original Periodicity source, class and help are unchanged. Namespace compilation with normal sc3-plugins confirms that its EnvFollow still resolves normally and all four new classes resolve to this checkout.

`tests/sc/esp_smoke.scd` rendered a 15-channel float WAV through scsynth at 48 kHz for block sizes 16 and 64, then inspected every sample for finite [0,1] bounds. It verified output ordering, both F/V calibrations and validity, rising-event timing, envelope, activation pulse and complementary gate polarity. The block-64 case uses 880 Hz audio (above the control-rate Nyquist of 375 Hz), demonstrating that event detection consumes audio samples. Settled normalized F/V values were approximately 0.4353 digital and 0.43993 circuit against the 0.44 reference. Block 16 used 220 Hz. These point samples include expected smoothing ripple; numerical kernel tests separately validate settled averages. No physical audio device or audible monitoring was needed.

All four new class help pages and the new guide parsed successfully with SCDoc; all seven code examples compiled. Class-library inspection plus the documented catalog gives 137 UGen subclasses and two analyzer convenience wrappers, for **139 documented public analyzers**. Internal/helper subclasses are not counted as public catalog entries.

A standalone optimized AppleClang benchmark on Apple M5 processed 100 seconds of generated 48 kHz input through F/V, envelope, timing and trigger kernels, with statistics requested each 64 samples. Digital suite cost was approximately 31.0 ns/sample (0.149% of one core at real time); circuit suite approximately 37.8 ns/sample (0.181%). This includes sine generation, excludes server/OSC/GUI overhead, is one local measurement rather than a scheduling guarantee, and is not an existing-analyzer CPU comparison. Exact output is in audit/esp-benchmark.log.

The arm64 scsynth plugin and install tree were staged with CMake under /private/tmp/mir2-esp-stage. No global installation was performed. Development branch: feature/ms20-esp.

## Remaining limits for release claims

Supernova was not built/tested because the external source tree lacks nova-tt. The approved monostable, comparator levels, input scale and calibration remain software reductions. No independent SPICE or hardware comparison establishes original MS-20 fidelity. No listening evaluation or measured existing-analyzer CPU comparison is claimed. These limits do not prevent using the documented reduced model and normalized interfaces.

## Updated FreqToVolt comparison example

The class help now loops SuperCollider’s bundled a11wlk01.wav and selects vanilla Pitch, MIR2 PitchMPM or FreqToVolt to control LFSaw frequency. The documentation parsed, its code blocks compiled, and a headless render produced finite, nonzero audio for all three selections. The updated help was installed and rendered locally.
