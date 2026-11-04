// SPDX-License-Identifier: GPL-3.0-or-later
/*
    MIRaColormap: Perceptually uniform color lookup tables for MIR visualization.
    Provides standard research colormaps:
    - \magma: Dark violet -> crimson -> orange -> golden yellow
    - \viridis: Deep blue -> teal -> emerald -> bright yellow
    - \plasma: Dark purple -> violet -> magenta -> red-orange -> yellow
    - \inferno: Black -> deep violet -> reddish purple -> orange -> yellow
    - \coolwarm: Symmetrical diverging blue-white-red
*/

MIRaColormap {
    classvar <cache;
    classvar <pixelCache;

    *initClass {
        cache = IdentityDictionary.new;
        pixelCache = IdentityDictionary.new;
    }

    *nodes { |name=\magma|
        switch(name.asSymbol,
            \magma, {
                ^[
                    [0.00, 0.001, 0.001, 0.010],
                    [0.12, 0.100, 0.050, 0.250],
                    [0.25, 0.280, 0.070, 0.450],
                    [0.38, 0.500, 0.130, 0.520],
                    [0.50, 0.710, 0.210, 0.480],
                    [0.62, 0.880, 0.350, 0.400],
                    [0.75, 0.980, 0.550, 0.380],
                    [0.88, 0.990, 0.780, 0.530],
                    [1.00, 0.995, 0.995, 0.750]
                ]
            },
            \viridis, {
                ^[
                    [0.00, 0.267, 0.004, 0.329],
                    [0.12, 0.282, 0.165, 0.470],
                    [0.25, 0.231, 0.322, 0.545],
                    [0.38, 0.176, 0.447, 0.557],
                    [0.50, 0.129, 0.569, 0.553],
                    [0.62, 0.153, 0.686, 0.502],
                    [0.75, 0.369, 0.788, 0.380],
                    [0.88, 0.678, 0.863, 0.188],
                    [1.00, 0.992, 0.906, 0.145]
                ]
            },
            \plasma, {
                ^[
                    [0.00, 0.051, 0.027, 0.530],
                    [0.15, 0.280, 0.010, 0.620],
                    [0.30, 0.520, 0.030, 0.650],
                    [0.45, 0.730, 0.210, 0.530],
                    [0.60, 0.870, 0.380, 0.380],
                    [0.75, 0.960, 0.590, 0.250],
                    [0.90, 0.980, 0.810, 0.160],
                    [1.00, 0.940, 0.976, 0.129]
                ]
            },
            \inferno, {
                ^[
                    [0.00, 0.001, 0.001, 0.020],
                    [0.15, 0.150, 0.040, 0.320],
                    [0.30, 0.380, 0.070, 0.440],
                    [0.45, 0.620, 0.160, 0.340],
                    [0.60, 0.820, 0.320, 0.180],
                    [0.75, 0.940, 0.540, 0.050],
                    [0.90, 0.980, 0.780, 0.220],
                    [1.00, 0.990, 0.990, 0.650]
                ]
            },
            \coolwarm, {
                ^[
                    [0.00, 0.230, 0.299, 0.754],
                    [0.25, 0.480, 0.640, 0.890],
                    [0.50, 0.865, 0.865, 0.865],
                    [0.75, 0.920, 0.570, 0.450],
                    [1.00, 0.706, 0.016, 0.150]
                ]
            },
            {
                // Fallback to magma
                ^this.nodes(\magma)
            }
        )
    }

    *colorAt { |val = 0.5, name = \magma|
        var tbl = this.table(name);
        var idx = (val.clip(0.0, 1.0) * (tbl.size - 1)).round.asInteger;
        ^tbl[idx]
    }

    *pixelAt { |val = 0.5, name = \magma|
        var tbl = this.pixelTable(name);
        var idx = (val.clip(0.0, 1.0) * (tbl.size - 1)).round.asInteger;
        ^tbl[idx]
    }

    *table { |name = \magma, size = 256|
        var sym = name.asSymbol;
        var existing = cache[sym];
        if(existing.notNil and: { existing.size == size }) { ^existing };

        existing = this.makeColorTable(this.nodes(sym), size);
        cache[sym] = existing;
        ^existing
    }

    *pixelTable { |name = \magma, size = 256|
        var sym = name.asSymbol;
        var existing = pixelCache[sym];
        if(existing.notNil and: { existing.size == size }) { ^existing };

        existing = this.makePixelTable(this.nodes(sym), size);
        pixelCache[sym] = existing;
        ^existing
    }

    *makeColorTable { |nodes, size = 256|
        var arr = Array.newClear(size);
        size.do { |i|
            var v = i / (size - 1);
            var idx = 0, n0, n1, t, r, g, b;
            (nodes.size - 1).do { |k|
                if(nodes[k][0] <= v) { idx = k };
            };
            idx = idx.min(nodes.size - 2);
            n0 = nodes[idx];
            n1 = nodes[idx + 1];
            t = (v - n0[0]) / ((n1[0] - n0[0]).max(1e-6));
            r = (n0[1] + (t * (n1[1] - n0[1]))).clip(0, 1);
            g = (n0[2] + (t * (n1[2] - n0[2]))).clip(0, 1);
            b = (n0[3] + (t * (n1[3] - n0[3]))).clip(0, 1);
            arr[i] = Color(r, g, b);
        };
        ^arr
    }

    *makePixelTable { |nodes, size = 256|
        var arr = Int32Array.newClear(size);
        size.do { |i|
            var v = i / (size - 1);
            var idx = 0, n0, n1, t, r, g, b, ri, gi, bi, pixel;
            (nodes.size - 1).do { |k|
                if(nodes[k][0] <= v) { idx = k };
            };
            idx = idx.min(nodes.size - 2);
            n0 = nodes[idx];
            n1 = nodes[idx + 1];
            t = (v - n0[0]) / ((n1[0] - n0[0]).max(1e-6));
            r = (n0[1] + (t * (n1[1] - n0[1]))).clip(0, 1);
            g = (n0[2] + (t * (n1[2] - n0[2]))).clip(0, 1);
            b = (n0[3] + (t * (n1[3] - n0[3]))).clip(0, 1);
            ri = (r * 255).round.asInteger;
            gi = (g * 255).round.asInteger;
            bi = (b * 255).round.asInteger;
            // 0xAARRGGBB packed with alpha 255
            pixel = (255 << 24) | (ri << 16) | (gi << 8) | bi;
            arr[i] = pixel;
        };
        ^arr
    }

    *names {
        ^[\magma, \viridis, \plasma, \inferno, \coolwarm]
    }

    *drawColorbar { |rect, name = \magma, vertical = true|
        var steps = 64;
        var table = this.table(name, steps);
        if(vertical) {
            var stepH = rect.height / steps;
            steps.do { |i|
                Pen.fillColor = table[steps - 1 - i];
                Pen.fillRect(Rect(rect.left, rect.top + (i * stepH), rect.width, stepH + 0.5));
            };
        } {
            var stepW = rect.width / steps;
            steps.do { |i|
                Pen.fillColor = table[i];
                Pen.fillRect(Rect(rect.left + (i * stepW), rect.top, stepW + 0.5, rect.height));
            };
        };
        Pen.strokeColor = Color.gray(0.3);
        Pen.strokeRect(rect);
    }
}
