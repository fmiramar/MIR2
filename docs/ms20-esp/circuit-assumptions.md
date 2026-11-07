# Reduced circuit model and implementation decisions

Research date: 2026-11-06. The user explicitly approved proceeding with a documented reduced model after review of the unresolved final LM339 timing connection. This approval resolves the workflow gate; it does not resolve the electrical ambiguity. No measured-hardware accuracy is claimed.

## Evidence and scope

Keep the branch routing, half-wave envelope, active filter orders, opposite-phase event detection, and coupling of LOW CUT to output smoothing. Replace the unresolved timing network with a declared retriggerable monostable. Model the six-inverter CD4069 chain as an even-polarity bounded logic buffer. Do not add speculative semiconductor dynamics or implicit pitch correction.

Use internal volts and seconds. Normalize public outlets only at the boundary. Nominal references are 8.4 V for F/V and 5 V for envelope (S1 p2). A normalized output of one indicates reaching the reference, not proof of tracking accuracy. A global electrical input scale must be fixed and documented before API freeze; normalized audio amplitude is not intrinsically a hardware voltage.

## Linear stages

The preamplifier includes feedback compensation, AC coupling and a second inverting stage. R49/R50 gives a nominal second-stage gain of −22. Opposed 1 µF coupling capacitors have an ideal series capacitance of 0.5 µF. R50 alone then gives a nominal coupling pole of 31.83 Hz; this is an idealized calculation, not a complete loaded preamp response. VR7's control law and saturation need an explicit approximation. Avoid applying both a full physical preamp gain and an undocumented additional normalization gain.

For a unity-gain Sallen–Key low-pass with R_a from input to node a, R_b from a to node b, C_f from a to buffer output, and C_g from b to ground:

```
y = b
H(s) = 1 / [1 + C_g (R_a + R_b) s + R_a R_b C_f C_g s²]
w0 = 1 / sqrt(R_a R_b C_f C_g)
Q = sqrt(R_a R_b C_f C_g) / [C_g (R_a + R_b)]
```

This maps to both HIGH CUT sections and the IC3 smoothing stage, with the respective fixed resistors plus the series pot sections. For equal section resistances, the first HIGH CUT stage has Q=0.5 (C8=C9=15 nF), while the second has Q≈1.581 (C5=150 nF, C7=15 nF). Do not silently turn the second stage into another Q=0.5 section. Input/output loading, finite gain and component tolerance are omitted in this first reduction.

IC3 uses C11=C6=56 nF, with two variable series resistance sections and 10 kΩ fixed resistance in each arm. At equal zero pot resistance its ideal natural frequency is 284.2 Hz; at equal 100 kΩ resistance it is 25.84 Hz. This is a *two-pole* smoothing filter and does not equal a single `smoothTime` exponential. Its control tracks the two high-pass pot sections through VR6. Exact pot taper is UNVERIFIED; choose and disclose a deterministic monotone mapping. Do not claim that a linear resistance approximation reproduces front-panel knob angles.

The input HIGH PASS is second order. Define a between C12 and C10 and b at IC6 pin 3; the buffer output equals b. The feedback arm from a to output is R52 plus its VR6 section (R_a); the b-to-ground arm is R45 plus its VR6 section (R_b). C_a=C12 and C_b=C10. The R55/R56 source network has Thevenin gain k=1000/1680 and resistance R_s=(680*1000)/1680 Ω, assuming a zero-impedance IC7 output. The nodal equations are:

```
(a - k*u)/(R_s + 1/(s*C_a)) + (a-b)*s*C_b + (a-b)/R_a = 0
(b-a)*s*C_b + b/R_b = 0
H(s) = k*s²*C_a*C_b*R_a*R_b /
       [1 + s*(R_a*(C_a+C_b) + C_a*R_s)
          + s²*C_a*C_b*R_a*(R_b+R_s)]
```

This includes the source loading. Its high-frequency gain is k*R_b/(R_b+R_s). Equal pot resistances are an ideal ganging assumption. At zero pot resistance the natural frequency is approximately 2.20 kHz; at 100 kΩ per section it is approximately 47.2 Hz. These are calculated natural frequencies, not identical to the specified knob endpoints or measured −3 dB corners. Calibrate a software Hz control against this declared definition and acknowledge any range limiting. The nominal 50–2500 Hz manual range does not justify changing transcribed component values to force an endpoint match.

Use a stable discretization, check poles at control extremes, and compare magnitude/phase against the analog equations at representative cutoffs and sample rates. Prewarping choices must be documented; clamping a cutoff near Nyquist changes the modeled response.

## F/V event and timing reduction — approved approximation

IC2's first comparator has positive feedback through 220 kΩ against 1 kΩ to ground. Its ideal reference level for a given output voltage is `Vout / 221`; actual output levels require a supply/output model. The second comparator supplies the opposite phase. C3/C4 and D2/D3 make positive transitions in either phase drive TR2. On a clean sufficiently large sine, use two qualified events per period. For Periodicity2 use only one direction.

R16*C2 = 220 kΩ × 12 nF = 2.64 ms. **That product is not the monostable pulse width.** The timing comparator threshold and feedback alter the charging trajectory. Even for a simple isolated RC recharge, width would depend on start/threshold/supply through a logarithm; this network also includes R23 and switched feedback. Do not use 2.64 ms as a measured pulse width.

Approved replacement:

1. A qualified event starts or retriggers a pulse lasting a fixed model width `w` seconds.
2. Overlapping pulses extend the active interval; they do not add unbounded voltage. This deliberately permits high-rate saturation.
3. Use exact fractional sample pulse coverage, or an equivalently validated sub-sample integrator. Rounding to whole samples would bias DC and violate sample-rate invariance.
4. Drive the output two-pole low-pass with a bounded pulse amplitude `V_p`; apply CV gain after filtering.
5. In the nonoverlapping, steady regime, mean model voltage is `V_p * w * r`, where `r` is event rate. Nominal clean-sine indication uses `r/2`.
6. Declare `w`, `V_p`, gain and reference mapping as software calibration. They are not recovered Korg trim settings. Use the same declared inverse calibration in digital/circuit comparisons.
7. Above the overlap/clipping region, the inverse is no longer reliable. Mark indication invalid when its stated inverse assumptions fail; never claim that saturation is high-confidence F0.

The plan's 0.2 ms pulse width may be retained as a software default only with this label. At 2000 Hz and two events/cycle, that width gives 80% nominal duty, which leaves limited headroom; test the overlap boundary explicitly. Do not infer CD4069 rail amplitude from the 8.4 V public reference.

Digital mode can use additive fixed-area event impulses followed by causal averaging. Its high-rate behavior will differ from the bounded monostable. Document that difference rather than force equality outside the declared linear operating region.

## Envelope derivation

Let `u` be the amplified, unfiltered input voltage, `e` the C1 voltage, and `Vd` an explicitly assumed diode drop. With ideal op-amp input impedance:

```
C de/dt = max((u - Vd - e)/R48, 0) - e/R3
C = 1 µF; R48 = 1 kΩ; R3 = 22 kΩ
```

Diode-off release time is 22 ms. When conducting with constant input, charging time is `(R48 || R3) C = 0.9565 ms` and equilibrium is `(u-Vd)*R3/(R48+R3)`. Solve each region exponentially; handle diode-state changes consistently. This reproduces half-wave storage and ripple, not an absolute-value follower. A fixed diode drop (for example 0.6 V) is a documented approximation, not a transcribed component specification.

IC1 has gain `1 + R4/R2 = 2`. R11/R35 divide this by two at the unloaded ENV OUT. Thus external envelope is nominally `e` before saturation/loading, and internal trigger input is `2e`. Model output swing with an explicit bound if saturation is included. The hardware's nominal 5 V output is a normalization reference, not a demonstrated rail clamp.

The public `attackTime`/`releaseTime` proposal must distinguish digital customization from circuit-derived defaults. Arbitrary values cannot simultaneously be called the original RC behavior.

## Trigger derivation and public input

Keep the proposed normalized-envelope input. Map `envelope` to external nominal volts as `5 * envelope`, then to internal comparator level as `10 * envelope` before comparing. This is exact only within the simplified unloaded envelope mapping and before its public clipping. If preservation of above-reference internal envelope is needed, the trigger must instead own its frontend; do not silently change the input meaning.

For pot position p, obtain the unloaded threshold wiper voltage Vt and Thevenin resistance Rt from the +15 V → R20 → VR3 → R1 → ground chain. Let Rref=R21+Rt. Positive comparator input is:

```
Vplus = (Vt/Rref + Vout/R19) / (1/Rref + 1/R19)
```

Use current high/low output state in this expression. Switch low when the internal envelope exceeds the high-state threshold and high when it falls below the low-state threshold. The switching polarity is established by IC1 pin 6 receiving the envelope. The actual output swing must be assumed or measured; it is not the nominal external 5 V trigger level. D1/R9/R34 shape the external electrical high state.

For a simple public normalized threshold/hysteresis API, expose a declared digitalized Schmitt approximation in both modes and document that the knob law is not a measured VR3 law. `hardwareGate = 1 - gate`, and `trigger` emits one control evaluation pulse on activation. Tests must include threshold equality, chatter, silence, initialization and held input. Do not re-trigger from F/V events.

## One bounded electrical study

Only one targeted SPICE study is recommended: TR2/C2 and the last two IC2 sections, with named capacitor/threshold/output nodes, finite output sinks and explicit assumed rail connections. First obtain a corrected netlist or clearly enumerate competing wiring interpretations. SPICE can compare those hypotheses; it cannot prove which wiring is in the instrument. Sweep isolated events, paired edges, interval, pulse drive and VR2; record capacitor trajectories, pulse width, DC gain and retrigger behavior. No SPICE run or independent electrical-reference match has yet been completed.

## Remaining decisions before implementation

- Freeze audio-to-volts scale, software pulse width/amplitude, control mapping and saturation rules in the API decision record.
- Validate the derived high-pass and low-pass responses numerically; the ideal source/buffer assumptions above remain explicit.
- Keep all new public signals finite and in [0,1]; distinguish validity from pitch confidence.
- Preserve existing Periodicity and tests. No hardware-equality claims without measurements.

## Phase 2 digital API decisions

The first implementation is an isolated C++ kernel in `include/mir2/MS20ESP.hpp`. SC wrappers and public mode selection are deferred to integration; the planned single FreqToVolt class still has exactly `digital` and `circuit` modes. Circuit mode is not implemented in Phase 2.

Digital conditioning uses a one-pole high-pass followed by a one-pole low-pass, each bypassable with cutoff zero. Coefficients use the audio sample rate; positive cutoffs are limited to 0.45 times that rate. Input gain is a fixed dimensionless multiplier; digital input amplitude is in audio units and does not imply electrical volts. Hysteresis is the comparator half-width around threshold, in conditioned audio units. The first sample initializes comparator state without emitting an event. Subsequent crossing timestamps interpolate the relevant Schmitt boundary between adjacent samples.

Digital F/V counts both directions. Each crossing contributes a unit-area event impulse to a causal exponential with time constant `smoothTime` (minimum one sample). Fractional crossing position determines its exact exponential decay to the current sample boundary. This is an impulse-density model; no finite pulse-width parameter is needed in this mode. For an event at t_e, its contribution at t>=t_e is `exp(-(t-t_e)/tau)/tau`, in events/second. Sampling that continuous response produces a small residual discrete integration bias that converges with sample rate; cross-rate tests bound the observed mean-rate error. Circuit mode's finite retriggerable pulse will have different overlap behavior.

Let r be smoothed internal event rate. The software calibration is:

```
f_indicated = cvAdjust * r / 2
V_model = 8.4 * f_indicated / referenceMaxFreq
cv = clip(V_model / 8.4, 0, 1)
frequency = clip(f_indicated / referenceMaxFreq, 0, 1)
eventRate = clip(r / referenceMaxEventRate, 0, 1)
```

`cv` and `frequency` currently coincide numerically under this linear calibration but retain distinct semantics for later circuit modeling. Changing `referenceMaxFreq` changes the volts-like software calibration as well as output scaling; it is not a measured hardware gain. `cvAdjust` scales the indication; zero gain makes it invalid. Internal electrical quantities are only diagnostic methods, never planned SC outlets. Defaults: referenceMaxFreq=2000 Hz, referenceMaxEventRate=4000 events/s, cvAdjust=1, smoothTime=20 ms, timeout=100 ms. These are fixed construction settings for this kernel revision. `valid` requires three recent events (two intervals) and positive gain; it does not promise unique acoustic F0 or unclipped outputs. Timeout resets support and leaves the voltage/rate estimate to decay. Reference clipping loses invertibility even while event evidence is valid.

Periodicity2 uses only rising crossings. It retains at most 128 complete intervals whose **starts** are within windowTime. This is an upper time window with a capacity limit, not a guarantee of keeping the full window at high event rates. minIntervals is clamped to [2,64], allowing count confidence to reach one at twice the minimum. A gap exceeding timeout starts a new history. Duplicate/nonincreasing event times are ignored. Statistics use population standard deviation and mean of the retained intervals, with Welford accumulation.

For enough recent intervals, count support is `min(count/(2*minIntervals),1)`. Freshness is one until age reaches `min(meanInterval, timeout/2)`, then decreases linearly to zero at timeout. eventConfidence is count support times freshness. jitter is `clip((std/mean)/jitterScale,0,1)`; periodicity is `eventConfidence*exp(-(std/mean)/jitterScale)`. Insufficient or stale history returns all zeros. Statistics run only when output is requested; per-sample event processing uses fixed storage and does not allocate or lock. Runtime per statistics call is bounded by 128 entries.

Nonfinite input becomes zero; extreme finite input/gain is bounded before filtering. Invalid positive reference/time settings use documented defaults. Clipping maps positive overflow to one and NaN to zero. These fallback rules preserve the public bounds; they do not turn invalid settings into physical evidence.

## Phase 3 circuit implementation decisions

`FreqToVoltKernel` is the single unit-facing family with construction-time `Mode::digital` or `Mode::circuit`; only the selected path processes samples. Internal implementation classes are not additional public UGens. Both internal states are constructed without allocation so later server integration can use fixed in-unit storage.

The circuit kernel starts at the **amplified branch**, with one input audio unit equal to one model volt after inputGain. It does not recreate the microphone-level preamp, VR7 taper or auto-pad. The branch voltage is limited to ±13.5 V as an explicit software headroom assumption. Digital amplitude units remain separate from this model-voltage convention. This is a deliberate frontend simplification, not a measured conversion from sound-card voltage to MS-20 input voltage.

All analog second-order sections use the bilinear transform with s=2*sampleRate*(1-z^-1)/(1+z^-1), without prewarping. Frequency-response oracles therefore evaluate the analog equations at omega=2*sampleRate*tan(pi*f/sampleRate). This preserves their stable analog poles but warps upper-band frequencies. Tests cover the derived filter orders and response, not an independent SPICE/hardware reference.

LOW CUT in Hz specifies the input high-pass's analog natural frequency. Its equal arm resistance solves R*(R+Rs)=1/((2*pi*f)^2*C^2), then is limited to 2–102 kΩ. The two output smoothing arms use R+8 kΩ, giving 10–110 kΩ. This preserves ideal equal ganging while leaving the unknown physical taper out of the software Hz interface. HIGH CUT specifies the first low-pass section's analog natural frequency; its equal arm resistance is 1/(2*pi*f*15 nF), also limited to 2–102 kΩ. The second stage uses that same resistance and its transcribed unequal capacitors. Circuit cutoffs cannot bypass the physical stages with zero; invalid/nonpositive values use 80/2000 Hz defaults. Requests outside the component-derived range saturate at the resistance limits.

The comparator receives the filtered model voltage, with threshold in those voltage units and symmetric half-hysteresis at least 15/221 V. The ±15 V switching level used to derive that minimum is an explicit idealization; an actual LM339 sink plus pull-up is asymmetric and loaded. No transistor/diode edge-coupler dynamics are simulated. A caller-supplied larger hysteresis can suppress small signals. This sensitivity is expected; no pitch detector repairs lost/extra crossings.

The approved timing replacement uses a 0.2 ms default retriggerable pulse. pulseWidth is limited to 0.1 µs–1 s. Within each sample the exact union of old and newly triggered pulse intervals sets a bounded duty drive. The low-pass receives this sample-average duty, which preserves pulse area but approximates its within-sample waveform. The six-inverter even-polarity chain is collapsed into this ideal 0–1 internal drive; its actual supply swing and delay are not simulated.

Software pulse calibration multiplies filtered duty by `8.4/(2*pulseWidth*referenceMaxFreq)` and cvAdjust. It therefore reproduces the Phase 2 average mapping in the nonoverlapping regime. Internal output voltage is limited to [0,13.5] model volts, then public cv/frequency are normalized against 8.4 V. Those voltage bounds and gain are software assumptions, not recovered CD4069/TL081 measurements. `cv` and `frequency` remain numerically equal in this calibrated reduced model.

The separate eventRate outlet uses a fixed 20 ms exponential event average. Circuit output smoothing itself follows LOW CUT and does not use digital smoothTime. Validity requires three recent events, positive gain, an unclipped nonnegative instantaneous voltage indication below 8.4 V, and no pulse overlap within the last timeout interval. Overlap invalidity has hysteresis in time: one timeout must elapse after the last overlap. Neither validity nor a high timing score guarantees true acoustic F0. Silence lets both filters and diagnostic rate decay and clears event support.

## Envelope, trigger and server integration decisions

EnvFollow2 defaults to circuit mode with 5 model volts per normalized unit. It processes audio input at audio sample rate and applies the exact piecewise held-input diode/RC solution, including decay-to-conduction transitions within a sample. The diode drop is fixed at 0.6 V. A ±13.5 V amplified-input limit is assumed; the original first two preamp stages, IC1 output swing/loading and LED behavior are not simulated. Circuit attack/release follow component values; the public attackTime/releaseTime parameters apply only to the digital absolute-value follower.

TriggerDetector receives only a normalized control-rate envelope. Both mode names select the approved reduced normalized Schmitt law: activate above threshold+hysteresis, deactivate at or below threshold−hysteresis, with boundaries limited to [0,1]. Strict activation avoids a spurious silence trigger at threshold zero; inclusive deactivation releases a gate at exactly zero. This uses externally specified normalized thresholds rather than a reconstructed VR3 knob law. It emits one evaluation pulse on activation; no periodic retrigger parameter is exposed. hardwareGate is exactly the inverse logical gate. The current implementation evaluates this law directly in normalized units, algebraically equivalent to scaling both input and thresholds into the simplified internal envelope domain.

The four public classes are control-rate only. F/V, envelope and timing readers consume the full audio input block internally; control/scalar input is held across that block. TriggerDetector rejects audio input, preventing accidental sampling of raw audio as an envelope. All other numeric arguments must be construction-time numbers; mode must be a literal digital/circuit Symbol. Constructor outputs are initialized, kernels use in-unit placement construction and fixed storage, and no runtime allocations or locks occur in their sample callbacks. No user extensions are installed by these build checks.
