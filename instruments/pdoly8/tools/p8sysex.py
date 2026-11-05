#!/usr/bin/env python3
"""Korg Poly-800 II sysex <-> pdoly8~ preset text.

  p8sysex.py syx2txt dump.syx bank.txt        64 programs -> [text define] bank (lines 32-95)
  p8sysex.py txt2syx bank.txt dump.syx [--template orig.syx]
  p8sysex.py show dump.syx                    the programs as Poly-800 values

The text file has 32 free lines (`-;`, for your own sounds), then one line per
program, `pdoly8 v0 v1 ...;`: line 32 + n = program 11 + 10 * (n // 8) + n % 8
(line 32 = 11, line 95 = 88). --offset N changes the 32. It is the line format
of [skpreset] (skandrums-pd): read it into a [text define -k name] and step
through it with [skpreset name]; a line is a `dump pdoly8 ...` without `dump`.

Sysex format (Poly-800 II service manual, MIDI implementation): F0 42 21 0C,
then the RAM (2688 bytes) as 5376 nibbles, low nibble first, then the 8-bit
sum of the RAM bytes as two nibbles, F7. The 64 programs are 26 bytes each
from 400H; their fields form one bit stream, least significant bit first.

The value order of a line is read from pdoly8~.c (controls, then settings), so
the converter follows the object. Mapping, Poly-800 II -> pdoly8~:
  waveform + feet  -> har (one-knob timbre) and octave: a combination without
                      16' is the same wave higher, so it becomes the 16'-rooted
                      one plus 1-3 octaves (oct 2-4); 16' alone in the square
                      net is 7.9 dB quieter than on the timbre path (saw net),
                      so its level is scaled down
  level 0-31       -> lev 0-127 (linear); WHOLE -> lev2 0 (pdoly8~ always
                      has 4 voices of DCO1 + DCO2)
  DEG1/2/3         -> own values (env -1, a1 ... r3)
  everything else  -> the same steps (cut, res, kbd, MG 81-84, delay, EQ ...)
Back from pdoly8~ the nearest Poly-800 values are taken; settings the Poly-800
does not have (PWM, sync, FM, Walsh, portamento ...) are dropped. Bytes outside
the programs (sequencer, chord memory, MIDI channel) come from --template, or
are empty.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = next((p for p in (os.path.join(HERE, "..", "pdoly8~.c"), os.path.join(HERE, "pdoly8~.c"))
            if os.path.exists(p)), os.path.join(HERE, "..", "pdoly8~.c"))

# ------------------------------------------------------------ the object
def object_layout(path=SRC):
    s = open(path).read()
    def strings(name):
        m = re.search(r"\*" + name + r"\[[A-Z]+\] = \{(.*?)\};", s, re.S)
        return re.findall(r'"([a-z0-9]+)"', m.group(1))
    def floats(name):
        m = re.search(name + r"\[[A-Z]+\] = \{(.*?)\};", s, re.S)
        return [float(v.replace("f", "")) for v in re.findall(r"-?[0-9.]+f?", m.group(1))]
    names = strings("CTL_NAME") + strings("EXT_NAME")
    defs = floats("CTL_DEFAULT") + floats("EXT_DEFAULT")
    assert len(names) == len(defs), "pdoly8~.c: names and defaults differ in length"
    return names, defs

# ------------------------------------------------------------ the sysex
HEAD = bytes([0xF0, 0x42, 0x21, 0x0C])
RAM_SIZE, PROG_BASE, PROG_SIZE, NPROG = 0xA80, 0x400, 26, 64

# (name, byte, lowest bit, bits): one bit stream, LSB first
FIELDS = [
    ("oct1", 0, 0, 2), ("oct2", 0, 2, 2), ("wave1", 0, 4, 2), ("wave2", 0, 6, 2),
    ("feet1", 1, 0, 4), ("feet2", 1, 4, 4),
    ("detune", 2, 0, 2), ("mode", 2, 6, 1),
    ("interval", 3, 0, 4), ("noise", 3, 4, 4),
    ("egint", 4, 0, 4), ("polarity", 4, 4, 1), ("level1", 4, 5, 5), ("level2", 5, 2, 5),
    ("cutoff", 6, 0, 7), ("trigger", 6, 7, 1),
    ("mgfreq", 7, 0, 4), ("mgdelay", 7, 4, 4), ("mgdco", 8, 0, 4), ("mgvcf", 8, 4, 4),
    ("a1", 9, 0, 5), ("d1", 9, 5, 5), ("bp1", 10, 2, 5), ("sl1", 10, 7, 5), ("su1", 11, 4, 5), ("r1", 12, 1, 5),
    ("a2", 12, 6, 5), ("d2", 13, 3, 5), ("bp2", 14, 0, 5), ("sl2", 14, 5, 5), ("su2", 15, 2, 5), ("r2", 15, 7, 5),
    ("a3", 16, 4, 5), ("d3", 17, 1, 5), ("bp3", 17, 6, 5), ("sl3", 18, 3, 5), ("su3", 19, 0, 5), ("r3", 19, 5, 5),
    ("resonance", 20, 2, 4), ("kbd", 20, 6, 2),
    ("ddltime", 21, 0, 7), ("ddlfb", 22, 0, 4), ("ddllevel", 22, 4, 4),
    ("ddlmf", 23, 0, 5), ("ddlmi", 24, 0, 5), ("treble", 25, 0, 4), ("bass", 25, 4, 4),
]


def getbits(b, byte, lo, n):
    v = 0
    for k in range(n):
        p = byte * 8 + lo + k
        v |= ((b[p // 8] >> (p % 8)) & 1) << k
    return v


def setbits(b, byte, lo, n, v):
    for k in range(n):
        p = byte * 8 + lo + k
        if (v >> k) & 1: b[p // 8] |= 1 << (p % 8)
        else: b[p // 8] &= ~(1 << (p % 8)) & 0xFF


def read_syx(path):
    d = open(path, "rb").read()
    i = d.find(HEAD)
    if i < 0: raise SystemExit(f"{path}: no Poly-800 II data dump (F0 42 21 0C)")
    body = d[i + 4: i + 4 + 2 * RAM_SIZE + 2]
    if len(body) < 2 * RAM_SIZE + 2: raise SystemExit(f"{path}: dump too short")
    ram = bytearray((body[2 * k] & 15) | ((body[2 * k + 1] & 15) << 4) for k in range(RAM_SIZE))
    cs = (body[-2] & 15) | ((body[-1] & 15) << 4)
    if sum(ram) & 0xFF != cs: print(f"warning: {path}: check sum differs", file=sys.stderr)
    return ram


def write_syx(path, ram):
    out = bytearray(HEAD)
    for b in ram: out += bytes([b & 15, b >> 4])
    cs = sum(ram) & 0xFF
    out += bytes([cs & 15, cs >> 4, 0xF7])
    open(path, "wb").write(out)


def program(ram, n):
    p = ram[PROG_BASE + PROG_SIZE * n: PROG_BASE + PROG_SIZE * (n + 1)]
    return {name: getbits(p, *f) for name, *f in FIELDS}


def prog_number(n): return 11 + 10 * (n // 8) + n % 8

# --------------------------------------------- timbre path and tables (pdoly8_core.c)
# path 3: (saw net?, feet bits 16' 8' 4' 2' MSB first) at har = i * 127 / 14
TIMB = [0x8, 0x9, 0xB, 0xA, 0xE, 0xF, 0xD, 0xC, 0xC, 0xE, 0xA, 0xB, 0xF, 0xD, 0x9]
TIMB_SAW = [1] * 8 + [0] * 7
SQ16_DB = -7.9                 # 16' alone, square net vs saw net
MG_STEP = [0, 3, 6, 8, 9, 12, 15]; MG_HZ = [0.291, 1.262, 3.39, 5.22, 6.493, 11.09, 19.08]
MG_CENTS = 12.4
MGD = [0.0, 0.47, 0.95, 1.55]
DEG_KPOS = [0, 1, 24, 48, 72, 92, 108, 127]
DEG_KNOT = [[0, 0, 31, 0, 31, 0], [0, 3, 0, 0, 0, 3], [0, 14, 0, 0, 0, 12], [0, 10, 20, 24, 0, 14],
            [2, 16, 24, 0, 24, 26], [16, 0, 31, 0, 31, 18], [20, 22, 26, 0, 26, 24], [30, 26, 26, 0, 26, 31]]


def feet_msb(poly_feet):       # Poly-800 bits b0 16' .. b3 2' -> 16' as MSB
    return sum(((poly_feet >> f) & 1) << (3 - f) for f in range(4))


def feet_poly(msb): return sum(((msb >> (3 - f)) & 1) << f for f in range(4))


def timbre(wave, poly_feet):
    """Poly-800 waveform (0 square, 1 saw) + feet -> (har, extra octaves, level factor)"""
    m = feet_msb(poly_feet)
    if m == 0: return 45, 0, 0.0
    shift = 0
    while not (m & 8): m <<= 1; shift += 1
    m &= 0xF
    if m == 0x8: return 0, shift, (10 ** (SQ16_DB / 20) if wave == 0 else 1.0)
    for i, (k, saw) in enumerate(zip(TIMB, TIMB_SAW)):
        if k == m and saw == wave: return round(i * 127 / 14), shift, 1.0
    raise ValueError("timbre not on the path")


def har_to_poly(har, extra):
    i = max(0, min(14, round(har * 14 / 127)))
    m, wave = TIMB[i], TIMB_SAW[i]
    for _ in range(extra): m >>= 1          # higher feet; feet beyond 2' are lost
    return wave, feet_poly(m)


def env_morph(pos):
    if pos < 0.5: return list(DEG_KNOT[0])
    pos = max(1.0, min(127.0, pos)); a = 1
    while a < len(DEG_KPOS) - 2 and pos > DEG_KPOS[a + 1]: a += 1
    x0, x1 = DEG_KPOS[a], DEG_KPOS[a + 1]
    sn = min(1.0, 0.45 * (x1 - x0))
    lo = x0 if a == 1 else x0 + sn
    hi = x1 if a + 1 == len(DEG_KPOS) - 1 else x1 - sn
    t = 0.0 if pos <= lo else 1.0 if pos >= hi else (pos - lo) / (hi - lo)
    return [DEG_KNOT[a][k] + t * (DEG_KNOT[a + 1][k] - DEG_KNOT[a][k]) for k in range(6)]


def mg_step_from_hz(hz):
    import math
    best = min(range(16), key=lambda s: abs(math.log(max(hz, 1e-3)) - math.log(mg_hz(s))))
    return best


def mg_hz(step):
    a = 0
    while a < 5 and step > MG_STEP[a + 1]: a += 1
    t = (step - MG_STEP[a]) / (MG_STEP[a + 1] - MG_STEP[a])
    return MG_HZ[a] * (MG_HZ[a + 1] / MG_HZ[a]) ** t

# ------------------------------------------------------------ conversion
def to_line(pr, names, defs):
    v = dict(zip(names, defs))
    double = pr["mode"] == 1
    for g in (1, 2):
        har, shift, lf = timbre(pr[f"wave{g}"], pr[f"feet{g}"])
        lev = pr[f"level{g}"] * 127 / 31 * lf
        if g == 2 and not double: lev = 0
        v[f"har{g}"] = har
        v[f"oct{g}"] = min(4, pr[f"oct{g}"] - 1 + shift)
        v[f"lev{g}"] = round(lev)
        v[f"env{g}"] = -1
    v["intv"] = round(pr["interval"] * 127 / 12)
    v["detn"] = [0, 42, 85, 127][pr["detune"]]
    v["nois"] = pr["noise"]
    v["cut"], v["res"], v["kbd"] = pr["cutoff"], pr["resonance"], pr["kbd"]
    v["pol"], v["egi"], v["trg"] = pr["polarity"] + 1, pr["egint"], pr["trigger"] + 1
    v["env3"] = -1
    for g in (1, 2, 3):
        for k in ("a", "d", "bp", "sl", "su", "r"): v[f"{k}{g}"] = pr[f"{k}{g}"]
    v["mgr"] = pr["mgfreq"]; v["mgf"] = round(mg_hz(pr["mgfreq"]), 3)
    v["mgd"] = pr["mgdelay"]; v["mgi"] = round(pr["mgdco"] * MG_CENTS, 1); v["mgv"] = pr["mgvcf"]
    v["dtim"], v["dfb"], v["dmf"], v["dmi"], v["dlev"] = pr["ddltime"], pr["ddlfb"], pr["ddlmf"], pr["ddlmi"], pr["ddllevel"]
    v["treb"], v["bass"] = pr["treble"] - 6, pr["bass"] - 6
    return [v[n] for n in names]


def from_line(vals, names, defs):
    v = dict(zip(names, defs)); v.update(zip(names, vals))
    pr = {}
    for g in (1, 2):
        oc = int(v[f"oct{g}"])
        extra = max(0, oc - 1)
        wave, feet = har_to_poly(v[f"har{g}"], extra)
        pr[f"wave{g}"], pr[f"feet{g}"], pr[f"oct{g}"] = wave, feet, min(oc, 1) + 1
        lev = v[f"lev{g}"]
        if feet == 1 and wave == 0 and feet_msb(feet) == 8: lev /= 10 ** (SQ16_DB / 20)
        pr[f"level{g}"] = max(0, min(31, round(lev * 31 / 127)))
        deg = env_morph(v[f"env{g}"]) if v[f"env{g}"] >= 0 else [v[f"{k}{g}"] for k in ("a", "d", "bp", "sl", "su", "r")]
        for k, x in zip(("a", "d", "bp", "sl", "su", "r"), deg): pr[f"{k}{g}"] = max(0, min(31, round(x)))
    deg3 = env_morph(v["env3"]) if v["env3"] >= 0 else [v[f"{k}3"] for k in ("a", "d", "bp", "sl", "su", "r")]
    for k, x in zip(("a", "d", "bp", "sl", "su", "r"), deg3): pr[f"{k}3"] = max(0, min(31, round(x)))
    pr["mode"] = 1 if v["lev2"] > 0 else 0
    pr["interval"] = max(0, min(12, round(v["intv"] * 12 / 127)))
    pr["detune"] = max(0, min(3, round(v["detn"] * 3 / 127)))
    pr["noise"] = int(v["nois"])
    pr["cutoff"], pr["resonance"], pr["kbd"] = round(v["cut"]), round(v["res"]), int(v["kbd"])
    pr["polarity"], pr["egint"], pr["trigger"] = int(v["pol"]) - 1, round(v["egi"]), int(v["trg"]) - 1
    pr["mgfreq"] = int(v["mgr"]) if v["mgr"] >= 0 else mg_step_from_hz(v["mgf"])
    pr["mgdelay"] = round(v["mgd"]); pr["mgdco"] = max(0, min(15, round(v["mgi"] / MG_CENTS))); pr["mgvcf"] = round(v["mgv"])
    pr["ddltime"], pr["ddlfb"], pr["ddlmf"], pr["ddlmi"], pr["ddllevel"] = (round(v[k]) for k in ("dtim", "dfb", "dmf", "dmi", "dlev"))
    pr["treble"], pr["bass"] = round(v["treb"]) + 6, round(v["bass"]) + 6
    return pr


def fmt(x):
    return str(int(x)) if float(x).is_integer() else f"{x:g}"


OFFSET = 32


def syx2txt(src, dst, offset=OFFSET):
    names, defs = object_layout()
    ram = read_syx(src)
    with open(dst, "w") as f:
        for _ in range(offset): f.write("-;\n")
        for n in range(NPROG):
            f.write("pdoly8 " + " ".join(fmt(x) for x in to_line(program(ram, n), names, defs)) + ";\n")
    print(f"{dst}: {offset} free lines, then {NPROG} programs (line {offset} = 11 ... line {offset + 63} = 88)")


def read_txt(path):
    lines = [l.strip() for l in open(path).read().replace("\n", " ").split(";")]
    out = []
    for l in lines:
        a = l.split()
        if not a: continue
        if a[0] == "dump": a = a[1:]
        out.append(None if not a or a[0] != "pdoly8" else [float(x) for x in a[1:]])
    return out


def txt2syx(src, dst, template=None, offset=OFFSET):
    names, defs = object_layout()
    ram = read_syx(template) if template else bytearray(RAM_SIZE)
    for n, vals in enumerate(read_txt(src)[offset: offset + NPROG]):
        if vals is None: continue
        pr = from_line(vals, names, defs)
        base = PROG_BASE + PROG_SIZE * n
        p = bytearray(ram[base: base + PROG_SIZE])
        for name, *f in FIELDS: setbits(p, *f, pr[name])
        ram[base: base + PROG_SIZE] = p
    write_syx(dst, ram)
    print(f"{dst}: written")


def show(src):
    ram = read_syx(src)
    for n in range(NPROG):
        pr = program(ram, n)
        print(prog_number(n), " ".join(f"{k}={pr[k]}" for k, *_ in FIELDS))


if __name__ == "__main__":
    a = sys.argv[1:]
    off = int(a[a.index("--offset") + 1]) if "--offset" in a else OFFSET
    if len(a) >= 3 and a[0] == "syx2txt": syx2txt(a[1], a[2], off)
    elif len(a) >= 3 and a[0] == "txt2syx":
        t = a[a.index("--template") + 1] if "--template" in a else None
        txt2syx(a[1], a[2], t, off)
    elif len(a) >= 2 and a[0] == "show": show(a[1])
    else: print(__doc__)
