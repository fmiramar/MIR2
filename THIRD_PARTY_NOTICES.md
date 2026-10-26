# Third-party notices

The base MIR2 plugin uses independently implemented DSP equations except for the loudness
coefficient data identified below. It does not link Essentia, librosa,
FluCoMa, sc3-plugins, or a neural runtime, and contains no model weights or
datasets. SuperCollider and sc3-plugins were consulted for interfaces,
behavior, and class-name audits.

The optional `neural/` extension is a native PESTO adaptation with its own
[third-party notices](neural/THIRD_PARTY_NOTICES.md), including the upstream
LGPL-3.0 notice. It is built and installed separately and embeds no weights.

## SuperCollider loudness contour data

`include/mir2/Auditory.hpp` retains the 43 band edges and 42-by-11 loudness
contour table from SuperCollider 3.14.1 `server/plugins/Loudness.cpp`.
The original file is Copyright (c) 2002 James McCartney and identifies the
loudness implementation as Nick Collins (2007). Its license is
GPL-2.0-or-later; this project redistributes those data under GPL-3.0-or-later.
The integration, calibration and smoothing code are rewritten.

Source: https://github.com/supercollider/supercollider/blob/Version-3.14.1/server/plugins/Loudness.cpp

This software is provided without warranty, including implied warranties of
merchantability or fitness for a particular purpose. See `LICENSE` for the
GNU General Public License, version 3.

The plugin also builds against SuperCollider's GPL-licensed plugin API.
The optional native onset benchmark compiles an external user-provided
OnsetsDS source file; that source is not redistributed here.
