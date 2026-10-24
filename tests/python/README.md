# Optional Python references

These scripts are development oracles only. They are not installed as a MIR2
runtime dependency and are not needed to build or use the SuperCollider
extension.

`reference_descriptors.py` uses only the Python standard library to print
independently calculated values for simple spectra. A separate environment
with NumPy and librosa may be used for larger optional comparisons.

`pncc_reference.py` regenerates the tracked stage-by-stage PNCC fixture from
independent offline equations. `summarize_onsets.py` aggregates the deterministic
native/old-MIR2/revised onset sweep; the selected thresholds are diagnostics on
those same fixtures, not held-out accuracy estimates. Both use only the Python
standard library. See the PNCC and onset implementation reports for commands
and limitations.
