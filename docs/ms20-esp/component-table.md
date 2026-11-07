# KLM-129 component transcription

Direct visual transcription, S1 PDF p5 bottom panel. S2 PDF p6 provides a second scan of the same drawing, not independent design evidence. Bare decimal capacitor labels are interpreted as microfarads; `P` as pF. These unit interpretations must remain explicit. Values describe nominal components, not measured tolerances. No hardware constants have been added to source.

| Schematic reference | Nominal value | Net(s) / stage | Evidence | Confidence / model parameter |
|---|---|---|---|---|
| R51; VR7 | 2.2 kΩ; 1 MΩ B | J1 / preamp gain | S1 p5 upper left of KLM-129 | High reading; taper law unverified |
| R53; C17 | 1 MΩ; 100 pF | First IC7 feedback | S1 p5 | High |
| C15, C16 | 1 µF / 50 V each | Opposed input coupling caps | S1 p5 | High; ideal series equivalent 0.5 µF |
| R50; R49 | 10 kΩ; 220 kΩ | Second IC7 inverting stage | S1 p5 | High; nominal gain −22 |
| R55; R56 | 680 Ω; 1 kΩ | High-pass drive divider | S1 p5 | High |
| C12, C10 | 0.033 µF each | High-pass capacitors | S1 p5 | High |
| R52; R45 | 2 kΩ each | High-pass resistors | S1 p5 | High |
| VR6 | 100 kΩ C ×4 | Two HP sections, two output-filter sections | S1 p5 dashed ganging; S1 p8 four-gang pot listing | High ganging/value; resistance-vs-angle UNVERIFIED |
| R44, R42, R43, R41 | 2 kΩ each | Two low-pass stages | S1 p5 | High |
| C8; C9 | 0.015 µF; 0.015 µF | First low-pass feedback/shunt | S1 p5 | High |
| C5; C7 | 0.15 µF; 0.015 µF | Second low-pass feedback/shunt | S1 p5 | High reading; preserve unequal values, don't replace C5 with C7 |
| VR5 | 100 kΩ C ×4 | Four low-pass resistor sections | S1 p5 | High value; taper law UNVERIFIED |
| R47; R46 | 22 kΩ; 2.7 kΩ | J9 amplifier output divider | S1 p5 | High |
| R39; R40 | 10 kΩ; 2.7 kΩ | J17 filtered output divider | S1 p5 | High |
| R33 | 10 kΩ | IC2 input series resistor | S1 p5 | High |
| R28; R31 | 10 kΩ each | Comparator pull-ups | S1 p5 | High |
| R29; R30 | 220 kΩ; 1 kΩ | First comparator positive feedback | S1 p5 | High |
| C3, C4 | 220 pF each | Opposite-phase edge coupling | S1 p5 | High |
| R13, R15 | 22 kΩ each | Edge-cap return | S1 p5, S2 p6 | High |
| R12, R14 | 47 kΩ each | Diode to TR2 base | S1 p5 | High |
| R10 | 10 kΩ | TR2 base return | S1 p5 | High |
| D2, D3; TR2 | Diodes; C1685 | Edge rectification / reset switch | S1 p5 | Device label high; exact device dynamic model UNVERIFIED |
| C2; R16 | 12000 pF; 220 kΩ | Timing capacitor / charging branch | S1 p5, S2 p6 | High; RC alone is not pulse width |
| R17; R23 | 2.2 MΩ; 22 kΩ | Timing threshold / feedback | S1 p5 | High values |
| R24; R25; VR2 | 100 kΩ; 100 kΩ; 100 kΩ B | Timing feedback / trim | S1 p5 | High values; circuit operation UNVERIFIED |
| R27; R32; R26 | 10 kΩ; 4.7 kΩ; 15 kΩ | Timing output/reference network | S1 p5 | High values; exact node interpretation UNVERIFIED |
| IC2 | LM339-family quad comparator | F/V conditioning and timing | S1 p5; D1 pinout | Output `3` on final section conflicts with pinout |
| IC4 | CD4069 | Six cascaded inverters | S1 p5 | High chain topology; supplies/transfer curve UNVERIFIED |
| R37; R54 | 10 kΩ each | Output low-pass | S1 p5 | High |
| C11; C6 | 0.056 µF each | Output low-pass feedback/shunt | S1 p5 | High |
| IC3; VR1; R22 | TL081; 10 kΩ B; 47 kΩ | Output buffer / offset trim | S1 p5 | High values; offset setting unverified |
| R36; VR4; R38 | 470 Ω; 10 kΩ A; 1 kΩ | CV output scaling | S1 p5 | High values; taper law unverified |
| D4; R48; C1 | Diode; 1 kΩ; 1 µF / 50 V | Envelope rectifier / storage | S1 p5 | High |
| R3 | 22 kΩ | Envelope capacitor discharge to ground | S1 p5 enlarged envelope crop | High |
| R4; R2 | 100 kΩ each | Envelope amplifier feedback / ground leg | S1 p5 | High; noninverting gain 2 |
| R11; R35 | 2.7 kΩ each | Envelope output divider | S1 p5 | High; unloaded gain 1/2 |
| R19; R21 | 100 kΩ; 3.3 kΩ | Trigger positive feedback / threshold feed | S1 p5 | High |
| R20; VR3; R1 | 680 Ω; 10 kΩ; 820 Ω | Threshold reference divider | S1 p5 | High; VR3 taper unspecified |
| D1; R9; R34 | Diode; 4.3 kΩ; 2.7 kΩ | Active-low trigger output | S1 p5 | High values; actual high level depends on output swing and diode |
| R8 | 4.7 kΩ | Trigger indicator LED load | S1 p5 | High |
| R5; R6; R7; TR5 | 100 kΩ; 150 kΩ; 2.7 kΩ; C1685 | Envelope peak indicator | S1 p5 | High; omit LED optics from DSP |
| C13; C14 | 100 µF / 16 V each | ±15 V supply decoupling | S1 p5 right | High; omit power-ripple simulation initially |

Component identifiers are local to KLM-129; do not mix identically numbered KLM-128 parts into the model.
