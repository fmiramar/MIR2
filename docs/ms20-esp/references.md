# Sources and provenance

Inspected 2026-11-06. Page references are one-based PDF page indices unless explicitly described as printed page numbers. These documents are primary manufacturer publications hosted by third parties. Research images/PDFs are retained outside the repository under `../../audit/sources` relative to the checkout root; they are not redistributed as plugin assets.

| Key | Source | Inspected material / role |
|---|---|---|
| S1 | [Korg MS-20 service manual, Synfo scan](https://www.synfo.nl/servicemanuals/Korg/MS-20_SERVICE_MANUAL.pdf) | PDF p2 specifications; p5 Circuit Diagram (2), KLM-129 bottom panel; p8 parts list; p10 adjustment procedures. Original schematic visually inspected at enlarged resolution. |
| S2 | [Korg MS-20 service manual, Vintage Synth scan](https://www.vintagesynth.com/sites/default/files/2017-05/Korg_MS-20_Service_Manual.pdf) | PDF p6, printed p5, same KLM-129 drawing. Confirms the ambiguous final comparator marking persists in another scan; not independent circuit evidence. |
| O1 | [Original Korg MS-20 owner's manual](https://www.synthmind.com/MS20_E.pdf) | PDF p13, printed p7, ESP use and setup; p14 continuation/specifications; p16 block diagram in German section. Supports functional routing and operating context, not component constants. |
| D1 | [Texas Instruments LM339 family datasheet](https://www.ti.com/lit/ds/symlink/lm339.pdf) | Revision Z, PDF p4 pin functions: pin 3 positive supply, pin 13 fourth comparator output. Used only for the standard 14-pin device pinout check, not as a historical Korg device measurement. |

The initial plan also lists Arturia's manual and an Electronics Stack Exchange discussion. They were not used to establish component values or silently repair the schematic. Search results were used to locate the alternate manufacturer-manual scan.

## Evidence limits

S1 specifies low-cut range 50–2500 Hz, high-cut 100–5000 Hz, nominal F/V output 0–8.4 V, envelope 0–5 V and active-low trigger. Those ranges do not establish a precise control taper, calibrated volts-per-hertz slope, timing width or clipping law.

The primary-source diagram contains a final LM339 output label/connection ambiguity documented in schematic-map.md. The user approved a reduced software timing model on 2026-11-06 after being informed of its effect on calibration. This approval is an implementation choice, not new circuit evidence.

No instrument measurements, oscilloscope traces, original device model verification or SPICE comparison have been performed. No external implementation code was copied. Equations in circuit-assumptions.md are engineering derivations with stated idealizations.
