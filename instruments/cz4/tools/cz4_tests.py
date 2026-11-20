#!/usr/bin/env python3
"""Comparison tests cz4~ <-> Arturia CZ V (or a real CZ): one table for the test tones,
the note sequences, the Pd test rig and the analysis (tools/cz_compare.py).

Each test is a CZ tone (written into cz4/test/cz4test-bank.syx and one .syx per tone, so
CZ V can import it) and a sequence of MIDI events. The Pd patch cz4/test/cz4test.pd plays
the sequence to CZ V over MIDI and to cz4~ at the same time and records both.

usage: python3 tools/cz4_tests.py      writes the bank, the single tones and the sequences
"""
import ctypes
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "cz4", "test")

DCO, DCW, DCA = 0, 1, 2

# ---- the tests --------------------------------------------------------------------
# tone: a list of settings ("param", line, env, value) on top of BASE
# czv:  what to set by hand in CZ V (and the same as cz4~ messages in "cz4")
# events: (ms, kind, a, b): note pitch vel | cc num val | bend value (-8192..8191) | at val
# measure: what tools/cz_compare.py looks at

def env(line, e, steps, sus=0):
    """steps: [(rate, level), ...]; the last one is END (its level goes to zero)."""
    out = [("END", line, e, len(steps))]
    for j, (r, lv) in enumerate(steps):
        out.append((f"RATE{j + 1}", line, e, r))
        out.append((f"LEVEL{j + 1}", line, e, 0 if j == len(steps) - 1 else lv))
    out.append(("SUS", line, e, sus))
    return out


def organ(line=0):
    """flat pitch, DCW 0 (a sine), fast up, sustain, fast release"""
    return (env(line, DCO, [(99, 0), (99, 0)], 1) + env(line, DCW, [(99, 0), (99, 0)], 1) +
            env(line, DCA, [(99, 99), (70, 0)], 1))


BASE = [("LINESEL", 0, 0, 0), ("WAVE", 0, 0, 0), ("WAVE", 1, 0, 0)] + organ(0) + organ(1)


def held(pitch, on, off, vel=100):
    return [(on, "note", pitch, vel), (off, "note", pitch, 0)]


TESTS = [
    dict(name="T01 DCA RATES", length=10000, measure="dca",
         about="DCA envelope: rates 60 up, 40, 30 down, 50 release; levels 99, 70, 30. "
               "Gives the slope of each step in dB/s and the DCA level law.",
         tone=env(0, DCA, [(60, 99), (40, 70), (30, 30), (50, 0)], 3),
         events=held(57, 0, 6000, 127)),
    dict(name="T02 DCW SWEEP", length=8000, measure="dcw",
         about="DCW envelope from 0 up to 99 at rate 30, sawtooth: brightness over time.",
         tone=env(0, DCW, [(30, 99), (60, 0)], 1),
         events=held(45, 0, 5500, 127)),
    dict(name="T03 PITCH ENV", length=8000, measure="dco",
         about="DCO envelope: rate 50 up to level 64, held; release rate 40 to 20, then 30 "
               "to 0 (slow DCA release, so the fall is heard). Gives pitch envelope units and rates.",
         tone=env(0, DCO, [(50, 64), (40, 20), (30, 0)], 1) + env(0, DCA, [(99, 99), (25, 0)], 1),
         events=held(57, 0, 3000, 127)),
    dict(name="T04 VIBRATO", length=11000, measure="vibrato",
         about="Vibrato triangle, delay 30, rate 40, depth 60. Then mod wheel full (CC 1) "
               "and aftertouch full on a second note.",
         tone=[("VWAVE", 0, 0, 0), ("VDELAY", 0, 0, 30), ("VRATE", 0, 0, 40), ("VDEPTH", 0, 0, 60)],
         events=held(57, 0, 4000, 100) +
         [(5000, "cc", 1, 127), (5000, "note", 57, 100), (7500, "note", 57, 0), (7600, "cc", 1, 0),
          (8000, "note", 57, 100), (8000, "at", 127, 0), (10500, "note", 57, 0), (10600, "at", 0, 0)]),
    dict(name="T05 VELOCITY", length=9000, measure="velocity",
         about="CZ-1 velocity on the DCA (15) and DCW (8), DCW static at 40: the same note "
               "at velocities 127, 100, 80, 60, 40, 20, 1.",
         tone=[("VELA", 0, 0, 15), ("VELW", 0, 0, 8)] + env(0, DCW, [(99, 40), (99, 0)], 1),
         events=[e for k, v in enumerate((127, 100, 80, 60, 40, 20, 1))
                 for e in held(57, k * 1200, k * 1200 + 800, v)]),
    dict(name="T06 KEY FOLLOW", length=16000, measure="keyfollow",
         about="Key follow DCA 9 and DCW 9: a decaying tone (rate 35, no sustain) with DCW "
               "40 on C2, C3, C4, C5, C6.",
         tone=[("KFA", 0, 0, 9), ("KFW", 0, 0, 9)] + env(0, DCA, [(99, 99), (35, 0)], 0) +
         env(0, DCW, [(99, 40), (99, 0)], 1),
         events=[e for k, p in enumerate((36, 48, 60, 72, 84)) for e in held(p, k * 3000, k * 3000 + 2500)]),
    dict(name="T07 PORTA BEND", length=9000, measure="porta",
         about="Portamento from C3 to C4 and back, then pitch bend full up, full down.",
         czv="Portamento on, time 50 (and in poly mode); bend range 2.",
         cz4=["porta 1", "ptime 50", "bendrange 2"],
         tone=[],
         events=held(48, 0, 1000) + held(60, 1000, 2000) + held(48, 2000, 3000) +
         [(4000, "note", 57, 100), (5000, "bend", 8191, 0), (6000, "bend", -8192, 0),
          (7000, "bend", 0, 0), (8000, "note", 57, 0)]),
    dict(name="T08 RING MOD", length=5000, measure="ring",
         about="Lines 1+2', both sines, line 2 a fifth up, ring modulation: is line 1 heard "
               "next to the product, and how loud is the product?",
         tone=[("LINESEL", 0, 0, 3), ("MOD", 0, 0, 1), ("DNOTE", 0, 0, 7)],
         events=held(57, 0, 3000, 127)),
    dict(name="T09 NOISE MOD", length=5000, measure="noise",
         about="Lines 1+2', both sines, noise modulation on line 2.",
         tone=[("LINESEL", 0, 0, 3), ("MOD", 0, 0, 2)],
         events=held(57, 0, 3000, 127)),
    dict(name="T10 DETUNE", length=6000, measure="detune",
         about="Lines 1+1', sines, line 2 detuned by fine 30: the beat rate gives the unit "
               "of the fine detune.",
         tone=[("LINESEL", 0, 0, 2), ("DFINE", 0, 0, 30)],
         events=held(57, 0, 4500, 127)),
    dict(name="T11 RESONANCE", length=7000, measure="spectrum",
         about="Resonance wave with triangle window (CZ wave 7), DCW sweep rate 40 up to 99: "
               "the shape of the resonance against CZ V.",
         tone=[("WAVE", 0, 0, 6), ("WINDOW", 0, 0, 2)] + env(0, DCW, [(40, 99), (60, 0)], 1),
         events=held(45, 0, 4500, 127)),
    dict(name="T12 CHORD LEVEL", length=6000, measure="level",
         about="One note, then a chord of 4 and of 8: output level and how it adds up.",
         tone=env(0, DCW, [(99, 50), (99, 0)], 1),
         events=held(48, 0, 1500) +
         [e for p in (48, 55, 60, 64) for e in held(p, 2000, 3500)] +
         [e for p in (36, 43, 48, 52, 55, 60, 64, 67) for e in held(p, 4000, 5500)]),
]

for _t in TESTS:
    _t.setdefault("czv", "")
    _t.setdefault("cz4", [])
    assert len(_t["name"]) <= 16


# ---- tone data through the C code of the instrument (core/cz_tone.c) ---------------

def cz_lib():
    src = os.path.join(ROOT, "core", "cz_tone.c")
    so = os.path.join(tempfile.gettempdir(), "cz_tone_py.so")
    subprocess.check_call(["cc", "-O1", "-std=c99", "-shared", "-fPIC", "-o", so, src,
                           "-I" + os.path.join(ROOT, "core")])
    return ctypes.CDLL(so)


def czp_ids():
    """the CZP_* numbers from cz_tone.h"""
    h = open(os.path.join(ROOT, "core", "cz_tone.h")).read()
    body = re.search(r"enum \{\s*(CZP_LINESEL.*?)\};", h, re.S).group(1)
    ids, n = {}, 0
    for item in re.sub(r"/\*.*?\*/", "", body, flags=re.S).replace("\n", " ").split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            name, expr = (s.strip() for s in item.split("="))
            n = eval(expr, {}, {k: v for k, v in ids.items()})
        else:
            name = item
        ids[name] = n
        n += 1
    for j in range(8):
        ids[f"CZP_RATE{j + 1}"] = ids["CZP_RATE1"] + j
        ids[f"CZP_LEVEL{j + 1}"] = ids["CZP_LEVEL1"] + j
    return ids


def make_tone(lib, ids, settings, name):
    b = (ctypes.c_uint8 * 144)()
    lib.cz_tone_init(b)
    for p, line, e, v in BASE + settings:
        lib.cz_set(b, ids["CZP_" + p], line, e, v)
    lib.cz_name_set(b, name.encode())
    return b


def syx(lib, tone, prog):
    out = (ctypes.c_uint8 * 400)()
    n = lib.cz_syx_tone(tone, 0, prog, 1, out)
    return bytes(out[:n])


def sequence_text(t):
    """a [qlist] file: '<delay> <receiver> <args>;' -- receivers of cz4test.pd"""
    ev = sorted(t["events"], key=lambda e: e[0])
    lines, now = [], 0
    for c in t["cz4"]:
        lines.append(f"0 czt-cz4 {c};")
    for ms, kind, a, b in ev:
        d = ms - now
        now = ms
        if kind == "note":
            lines.append(f"{d} czt-note {a} {b};")
        elif kind == "cc":
            lines.append(f"{d} czt-cc {a} {b};")
        elif kind == "bend":
            lines.append(f"{d} czt-bend {a};")
        elif kind == "at":
            lines.append(f"{d} czt-at {a};")
    lines.append(f"{t['length'] - now} czt-end bang;")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    os.makedirs(os.path.join(OUT, "seq"), exist_ok=True)
    os.makedirs(os.path.join(OUT, "tones"), exist_ok=True)
    lib, ids = cz_lib(), czp_ids()
    bank = b""
    for k, t in enumerate(TESTS):
        tone = make_tone(lib, ids, t["tone"], t["name"])
        # bank: program 0x20 + k = user slot k of cz4~
        bank += syx(lib, tone, 0x20 + k)
        open(os.path.join(OUT, "tones", f"test{k + 1:02d}.syx"), "wb").write(syx(lib, tone, 0x60))
        open(os.path.join(OUT, "seq", f"test{k + 1:02d}.txt"), "w").write(sequence_text(t))
    open(os.path.join(OUT, "cz4test-bank.syx"), "wb").write(bank)
    print(f"wrote {len(TESTS)} test tones and sequences to {OUT}")
