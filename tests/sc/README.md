# Installed server and help checks

Install MIR2 into the SuperCollider Extensions directory before running these
scripts. `compile_check.scd` and `collision_check.scd` discover the public classes
from the installed help catalog; `verify_scdoc.scd` forces indexing and renders
all class pages and the guide. Any SCDoc warning or error fails verification.

The generated example check constructs the actual runnable graph from every
class help page, renders help, and optionally renders four seconds of audio per
class. A third WAV channel monitors all native MIR2 primitive outputs before
example clipping or smoothing can conceal NaN/Inf. Audio must be finite,
bounded and non-silent. Silent-playback mode preserves audio rate with DC.ar(0)
and requires finite output without requiring audible sound. Some examples also
contain deliberately synthesized probes or voices, so this is specifically a
silent-playback test, not a requirement that every example produce silence.

```sh
python3 tests/sc/generate_example_check.py build/examples.scd --nrt
sclang -D build/examples.scd
python3 tests/sc/generate_example_check.py build/examples-96k.scd \
  --nrt --sample-rate 96000 --block-size 128 --skip-docs
sclang -D build/examples-96k.scd
python3 tests/sc/generate_example_check.py build/silence.scd \
  --nrt --silence --skip-docs
sclang -D build/silence.scd
python3 tests/sc/generate_example_check.py build/supernova.scd \
  --nrt --server supernova --skip-docs
sclang -D build/supernova.scd
```

`--classes ClassA ClassB` restricts checks after a localized change. Generated
scores, WAV files and scripts stay beside the specified output path. Each run
uses an explicit unique score filename; SuperCollider's default temporary name
is only unique within one sclang process and can collide across concurrent runs.
The script batches graph construction to avoid the language's selector-table
limit. Preserve logs and require both the success marker and absence of reported
errors. The current examples use ExampleFiles.child, which ships with SC.

These checks establish construction, buffer synchronization, rendering and
numerical/audio stability. They do not replace listening or annotated-corpus
recognition tests. The older dedicated pitch/FFT smoke scripts remain useful
for focused diagnostics; they are not the complete current class catalog.
