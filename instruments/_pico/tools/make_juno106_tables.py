#!/usr/bin/env python3
"""Writes juno~/juno106_tables.h: the Juno-106 model of juno~.

The model is the Juno-60 engine with its sliders remapped: for every value 0-127 of a Juno-106
slider the table holds the Juno-60 slider value (0..1, fractional) at which the engine does what
the Juno-106 does. What the Juno-106 does was measured on Cherry Audio's DCO-106 with
juno~/test/juno106test and juno106test2 (2026-11-20); what the engine does at each slider was
measured by the same methods on a render of juno~ (tools/juno60_curves.py).

Measured quantity per slider:
  cutoff    frequency of the self-oscillating filter (Hz)
  env       octaves the contour adds at sustain 127, on the self-oscillating filter
  vcflfo    frequency swing of the self-oscillating filter, +- octaves
  res       height of the resonant peak at cutoff 64 (dB); above that the self-oscillation
            threshold and level, mapped by hand from the measured points
  attack    note-on to 90 %; decay, release: to -20 dB (s). The engine's contour runs at twice
            its times in this model, so its curve is doubled here
  lfodelay  time until the vibrato reaches half its depth (s), also doubled
  dcolfo    vibrato depth, +- cents
  pwm       duty cycle of the pulse
Two are not remaps: the LFO rate is a table in Hz (the 106 goes down to 0.1 Hz, the 60 to 0.3)
and the VCA level a gain (the 106's slider spans 15 dB where the 60's spans 24).

usage: python3 tools/make_juno106_tables.py
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from juno60_curves import JUNO60   # noqa: E402

OUT = os.path.join(HERE, "..", "juno~", "juno106_tables.h")

# ---- DCO-106 measurements: (slider, value) ------------------------------------------------
DCO106 = {
    "cutoff": [(0, 20), (8, 20), (16, 21), (24, 26), (32, 44), (40, 94), (48, 205), (56, 419),
               (64, 799), (72, 1423), (80, 2396), (88, 3847), (96, 5933), (104, 8843),
               (112, 12801), (120, 18066), (127, 24000)],
    "attack": [(0, 0.001), (32, 0.036), (64, 0.32), (96, 1.21), (127, 3.08)],
    "decay": [(0, 0.009), (32, 0.16), (64, 1.52), (96, 5.86), (112, 9.80), (127, 14.9)],
    "release": [(0, 0.012), (32, 0.16), (64, 1.58), (96, 6.07), (112, 10.15), (127, 15.44)],
    "lfodelay": [(0, 0.0), (64, 1.82), (127, 4.07)],
    "dcolfo": [(0, 0.0), (32, 3.8), (64, 26.4), (127, 193.0)],
    "pwm": [(0, 0.5), (64, 0.255), (127, 0.05)],
    "lforate": [(0, 0.10), (16, 0.321), (32, 1.28), (64, 5.09), (96, 11.44), (127, 20.0)],
    "level_db": [(32, -9.8), (64, -4.8), (100, 0.0), (127, 5.2)],
    # resonance: Juno-106 slider -> Juno-60 slider, from peak heights (64: 9.8 dB, 80: 13.1,
    # 96: 16.4) and the self-oscillation onset (106 at 104, 60 at 93) and level.
    "res_peak": [(64, 9.8), (80, 13.1), (96, 16.4)],
    "res_fixed": [(0, 0.0), (104, 93.0), (112, 95.5), (120, 110.0), (127, 127.0)],
}
ENV_OCT_PER_STEP = 0.0788          # the 106's contour depth is linear: 127 -> 10 octaves
VCF_LFO = (2.76, 3.07)             # +- octaves = 2.76 * (v/127)^3.07
TIME_SCALE = 2.0                   # contour and LFO delay times of the engine in this model
ATTACK_CORR = 0.85                 # see main()


def interp(points, x, log=False):
    xs = [p[0] for p in points]
    ys = [math.log(max(p[1], 1e-9)) if log else p[1] for p in points]
    if x <= xs[0]:
        y = ys[0]
    elif x >= xs[-1]:
        y = ys[-1]
    else:
        i = max(k for k in range(len(xs)) if xs[k] <= x)
        if xs[i] == x:
            y = ys[i]
        else:
            t = (x - xs[i]) / (xs[i + 1] - xs[i])
            y = ys[i] + t * (ys[i + 1] - ys[i])
    return math.exp(y) if log else y


def invert(curve, target, log=False, decreasing=False):
    """Juno-60 slider (fractional, 0..127) at which the monotonic curve reaches target."""
    pts = list(curve)
    if decreasing:
        pts = [(v, -m) for v, m in pts]
        target = -target
    f = (lambda m: math.log(max(m, 1e-9))) if log else (lambda m: m)
    tv = f(target)
    # keep the curve monotonic: stop where it saturates
    clean = [pts[0]]
    for v, m in pts[1:]:
        if f(m) > f(clean[-1][1]):
            clean.append((v, m))
    if tv <= f(clean[0][1]):
        return float(clean[0][0])
    for (v0, m0), (v1, m1) in zip(clean, clean[1:]):
        if f(m0) <= tv <= f(m1):
            return v0 + (tv - f(m0)) / (f(m1) - f(m0)) * (v1 - v0)
    return float(clean[-1][0])


def table(fn):
    return [max(0.0, min(1.0, fn(b) / 127.0)) for b in range(128)]


def main():
    J = JUNO60
    tabs = {}
    tabs["Cutoff"] = table(lambda b: invert(J["cutoff"], interp(DCO106["cutoff"], b, log=True), log=True))
    base = J["env"][0][1]
    env_oct = [(v, math.log2(m / base)) for v, m in J["env"]]
    tabs["Env"] = table(lambda b: 0.0 if b == 0 else invert(env_oct, ENV_OCT_PER_STEP * b))
    tabs["VcfLfo"] = table(lambda b: 0.0 if b == 0 else invert(J["vcflfo"], VCF_LFO[0] * (b / 127.0) ** VCF_LFO[1]))
    for k in ("attack", "decay", "release"):
        # The attack of the 60's curve, doubled, comes out about 15 % long once it runs in the
        # 106 model (measured on the result): the target is shortened by that much.
        corr = ATTACK_CORR if k == "attack" else 1.0
        cur = [(v, m * TIME_SCALE / corr) for v, m in J[k] if m == m and m > 0]
        tabs[k.capitalize()] = table(lambda b, k=k, cur=cur: 0.0 if b == 0 else invert(cur, interp(DCO106[k], b, log=True), log=True))
    cur = [(v, m * TIME_SCALE) for v, m in J["lfodelay"]]
    tabs["LfoDelay"] = table(lambda b: 0.0 if b == 0 else invert(cur, interp(DCO106["lfodelay"], b)))
    # the vibrato depth measurement saturates above 88 on the 60: extend it by its square law
    dc = [(v, m) for v, m in J["dcolfo"] if v <= 88]
    dc += [(v, 89.2 * (v / 64.0) ** 2) for v in range(92, 200, 4)]
    tabs["DcoLfo"] = [min(1.0, x) for x in
                      [0.0] + [invert(dc, interp(DCO106["dcolfo"], b, log=True), log=True) / 127.0 for b in range(1, 128)]]
    tabs["Pwm"] = table(lambda b: invert(J["pwm"], interp(DCO106["pwm"], b), decreasing=True))
    peak = [(v, m) for v, m in J["res"]]
    res_pts = sorted(DCO106["res_fixed"] + [(b, invert(peak, db)) for b, db in DCO106["res_peak"]])
    tabs["Res"] = table(lambda b: interp(res_pts, b))
    lfo_hz = [interp(DCO106["lforate"], b, log=True) for b in range(128)]
    lv = DCO106["level_db"]
    slope = (lv[1][1] - lv[0][1]) / (lv[1][0] - lv[0][0])
    def gain(b):
        if b == 0:
            return 0.0
        db = interp(lv, b) if b >= lv[0][0] else lv[0][1] + slope * (b - lv[0][0])
        return (100 / 127.0) ** 2 * 10 ** (db / 20)
    vca = [gain(b) for b in range(128)]

    L = ["// Generated by tools/make_juno106_tables.py -- do not edit.",
         "// The Juno-106 model: for each Juno-106 slider value 0-127 the Juno-60 slider (0..1) at which",
         "// the engine does what the Juno-106 does, measured on Cherry Audio's DCO-106. LfoHz is the LFO",
         "// rate in Hz, VcaGain the VCA level as a gain.",
         "#ifndef JUNO106_TABLES_H", "#define JUNO106_TABLES_H", ""]
    def emit(name, vals, fmt="{:.5f}f"):
        L.append(f"static const float k106{name}[128] = {{")
        for i in range(0, 128, 8):
            L.append("    " + ", ".join(fmt.format(v) for v in vals[i:i + 8]) + ",")
        L.append("};")
    for name, vals in tabs.items():
        emit(name, vals)
    emit("LfoHz", lfo_hz, "{:.4f}f")
    emit("VcaGain", vca, "{:.5f}f")
    L += ["", "#endif"]
    open(OUT, "w").write("\n".join(L) + "\n")
    print("wrote", os.path.normpath(OUT))
    for name, vals in tabs.items():
        print(f"  {name:9s}", " ".join(f"{b}:{vals[b] * 127:5.1f}" for b in (0, 16, 32, 64, 96, 112, 127)))


if __name__ == "__main__":
    main()
