#!/usr/bin/env python3
"""Generate cz4~-help.pd in the layout of the skandrums-pd help patches.

usage: gen_help.py [outfile]   (default: cz4~-help.pd)

Same grid, colours and section order as skandrums-pd/tools/gen_help.py:
yellow writing surface, pink strip, blue panel, blue heading squares,
toggle + metro at the top, one [s]/[r] bus, text sections below.
"""
import sys

BS = chr(92)
SEMI, COMMA, DOLLAR, SPACE = BS + ";", BS + ",", BS + "$", BS + " "

# ---- grid (identical to skandrums-pd) -----------------------------------
M = 20
YEL_X, YEL_Y, YEL_W = 2 * M, M, 700
PINK_X, PINK_W = M, M
PINK_TOP = YEL_Y + 3 * M
BLUE_W = 200
BLUE_X = YEL_X + YEL_W + M - BLUE_W
BLUE_TOP = PINK_TOP + 3 * M
CX = YEL_X + M
LBL = 24
TX = CX + LBL - 2
LINE = 16
MAXCH = 62
PINK, YELLOW, BLUE = "#ffcfd6", "#fafaf2", "#1e3791"

OBJ, SHORT = "cz4~", "cz4"
SUBTITLE = "Paraphonic 4-Voice Synth (CZ-1) - phase distortion synthesis"

# controls: name, default, description (index = position)
CONTROLS = [
    ("wav1", 0, "Waveform of line 1. Morphs through the CZ waves: saw, "
                "saw-pulse, square, pulse, resonance with saw, triangle, "
                "cosine, trapezoid, pulse and double-saw window, double sine, "
                "double pulse."),
    ("wav2", 0, "Waveform of line 2, same range."),
    ("dcw1", 6, "DCW of line 1: how far the sine is bent towards the "
                "waveform (0 = sine)."),
    ("dcw2", 6, "DCW of line 2."),
    ("detn", 0, "Detunes line 2 upwards, continuously: 0 = unison, "
                "127 = one octave up. fine adds +-50 cents."),
    ("amp1", 127, "Level of line 1."),
    ("amp2", 0, "Level of line 2. With ring: level of the ring product. "
                "Above full scale (amp1 + amp2 > 127) both are scaled down "
                "together, so two lines never clip."),
    ("mod", 0, "Modulation of line 2: 0-42 off, 43-85 ring, 86-127 noise."),
]

SETTINGS = [
    "pai1, pai2 (-1) second wave of a CZ wave pair 0-11, -1 = off",
    "octv (0) octave -1, 0, 1",
    "sprd (0.5) stereo spread of the 4 voices 0-1",
    "levl (0.5) output level 0-2",
    "drft (0) slow pitch drift per voice 0-1",
    "mrph (8) smoothing of wave changes in ms",
    "hold (1) released voices keep sounding until the next phrase",
    "a4 (440) tuning in Hz",
    "dvel (40) how much velocity adds to the DCW of both lines",
    "trk (1) 1: line 2 follows line 1, 0: line 2 has its own notes",
    "ch1 (0) MIDI channel of line 1, 0 = any",
    "ch2 (2) MIDI channel of line 2 (with trk 0)",
    "pmor (1) 1: morph the bent phase, 0: CZ-style crossfade",
    "sine (1) 1: DCW 0 = sine on every wave, 0: chip behaviour",
    "era (0) vintage character 0-127: chip resolution, chip rate,",
    "DAC and output stage, from clean to crunchy",
    "fine (64) fine detune of line 2, 64 = none, +-50 cents",
]

# number boxes in the bottom panel: name, min, max
ADV = [("pai1", -1, 11), ("pai2", -1, 11), ("octv", -1, 1), ("sprd", 0, 1),
       ("levl", 0, 2), ("drft", 0, 1), ("mrph", 0, 100), ("hold", 0, 1),
       ("a4", 400, 480), ("dvel", 0, 127), ("trk", 0, 1), ("ch1", 0, 16),
       ("ch2", 1, 16), ("pmor", 0, 1), ("sine", 0, 1), ("era", 0, 127),
       ("fine", 0, 127)]
VINT = [("quant", 0, 1), ("chiprate", 0, 44100), ("bits", 4, 16), ("dacerr", 0, 1),
        ("hiss", -120, -40), ("lpf", 0, 20000), ("drive", 0, 1)]


def esc(s):
    return s.replace(",", " " + COMMA).replace(";", SEMI).replace("$", DOLLAR)


def label(s):
    return s.replace(" ", SPACE)


class Patch:
    def __init__(self):
        self.lines, self.conns, self.n = [], [], 0

    def add(self, text):
        self.lines.append(text)
        self.n += 1
        return self.n - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def cnv(self, x, y, w, h, lbl="empty", xo=20, yo=12, fs=30, bg=YELLOW, sel=19):
        return self.add(f"#X obj {x} {y} cnv {sel} {w} {h} empty empty {lbl} {xo} {yo} 0 {fs} {bg} #000000 0;")

    def heading(self, x, y, text, fs):
        return self.cnv(x, y, 10, 10, label(text), LBL, 6, fs, BLUE, 10)

    def comment(self, x, y, lines, width=MAXCH + 2):
        for ln in lines:
            assert len(ln) <= MAXCH, f"line too long ({len(ln)}): {ln}"
        body = (" " + SEMI + " ").join(esc(l) for l in lines)
        return self.add(f"#X text {x} {y} {body}, f {width};")


def wrap(text, width=MAXCH):
    out, cur = [], ""
    for w in text.split():
        if cur and len(cur) + 1 + len(w) > width:
            out.append(cur); cur = w
        else:
            cur = (cur + " " + w).strip()
    return out + [cur]


def build():
    p = Patch()
    n = len(CONTROLS)
    rows = (n + 3) // 4
    top_y = YEL_Y + 3 * M + 24 + 28
    util_y = top_y + 66
    ctl_y0 = util_y + 56
    ROW = 90
    last_s = ctl_y0 + ROW * (rows - 1) + 58
    r_y = last_s + 18 + 20
    obj_y = r_y + 36
    mul_y, dac_y = obj_y + 36, obj_y + 72
    y = dac_y + 18 + 36

    sections = []
    sections.append(("Inlets", ["0: messages (see Messages below)"]))
    sections.append(("Outlets", ["0: signal left", "1: signal right",
                                 "2: events: value, dump"]))
    sections.append(("Messages", [
        "<pitch> <velocity>, note <p> <v>: note on/off (0 = off)",
        "<p> <v> <channel>: from [notein], routed by ch1 / ch2",
        "note2 <p> <v>: a note for line 2 alone (with trk 0)",
        "<control> <0-127>: set a control by name, e.g. wav1 64",
        "ctl <index> <0-127>, params v0 v1 ...: set by index",
        "<setting> <value>: e.g. trk 0 (see Settings below)",
        "defaults, reset: default values, reset also stops all voices",
        "panic: all voices off, phase: restart all phases",
        "get <name>: reply 'value <name> <v>' on outlet 2",
        "print: voices, values and sample rate to the Pd window",
        "dump: send all values to outlet 2 as 'dump " + SHORT + " v0 ..'",
        "dump " + SHORT + " v0 v1 ...: restore such a dump (this voice only)",
        "voice <i> <p>, voiceoff <i>: play one voice directly",
        "tone b0 ... b127: waves, levels and detune from a CZ-1 tone",
        "quant, chiprate, bits, dacerr, hiss, lpf, drive: single",
        "vintage stages instead of era"]))
    clines = []
    for i, (cn, dv, txt) in enumerate(CONTROLS):
        clines += wrap(f"{i} {cn} ({dv}) {txt}")
    sections.append(("Controls (index name default)", clines))
    sections.append(("Settings (name default)", SETTINGS))
    sections.append(("Wave morph", [
        "wav moves through 12 waves. Neighbours are ordered so",
        "they sound close, and with pmor 1 the bent phase itself is",
        "interpolated: one single wave at every point, no level dip.",
        "With sine 1 every wave starts from the plain sine at DCW 0,",
        "so a DCW envelope always opens from the same sine.",
        "pmor 0 and sine 0 give the CZ chip's own behaviour.",
        "The cosine window (position 6) comes from Casio's patent: a",
        "resonance without the buzz of the chip's windows."]))
    sections.append(("Two lines", [
        "Every voice has two lines, each with its own wave and DCW.",
        "With trk 1 line 2 plays the same notes, detuned by detn.",
        "With trk 0 it gets its own 4-voice allocation and notes from",
        "MIDI channel ch2, like a second oscillator: two sequencer",
        "tracks on one voice, levelled by amp1 and amp2, joined by",
        "ring or noise. detn and fine tune line 2 against line 1."]))
    sections.append(("Envelopes", [
        "cz4~ has no envelopes. A note-on restarts its voice's phase,",
        "velocity opens the DCW (dvel), and the level is shaped by an",
        "external VCA. With hold 1 a released chord keeps sounding",
        "until the next phrase, so the VCA's release stays intact."]))
    sections.append(("Presets and steps", [
        "Per-step values: connect the voice to [skpreset name]",
        "(outlet 2 -> inlet 0, outlet 0 -> voice inlet). A slot",
        "number 0-15 then recalls and keeps the values for a step.",
        "More in skpreset-help.pd (click to open)."], 110))

    placed = []
    for sec in sections:
        title, lines = sec[0], sec[1]
        extra = sec[2] if len(sec) > 2 else 0
        placed.append((title, lines, y, extra))
        y += 24 + LINE * len(lines) + 28 + extra
    # bottom panel: number boxes for the settings and the vintage stages,
    # each row into its own [s $0-cz4] (the [r] above the object picks it up)
    ROWA = 76
    adv_y = y
    adv_rows = (len(ADV) + 3) // 4
    vint_y = adv_y + 24 + 28 + ROWA * adv_rows + 20
    vint_rows = (len(VINT) + 3) // 4
    y = vint_y + 24 + 28 + ROWA * vint_rows + 20
    credits_y = y
    yel_bottom = credits_y + 3 * LINE + M
    bar_bottom = yel_bottom + M

    p.cnv(YEL_X, YEL_Y, YEL_W, yel_bottom - YEL_Y, bg=YELLOW)
    p.cnv(PINK_X, PINK_TOP, PINK_W, bar_bottom - PINK_TOP, bg=PINK)
    p.cnv(BLUE_X, BLUE_TOP, BLUE_W, bar_bottom - BLUE_TOP, bg=BLUE)

    p.heading(CX, YEL_Y + M, OBJ, 22)
    p.heading(CX, YEL_Y + 3 * M, SUBTITLE, 16)

    bus = DOLLAR + "0-" + SHORT
    o = p.add(f"#X obj {CX} {obj_y} {OBJ};")
    rcv = p.add(f"#X obj {CX} {r_y} r {bus};")
    p.conn(rcv, 0, o, 0)
    snd0 = p.add(f"#X obj {CX} {util_y + 30} s {bus};")
    # demo: a chord every 500 ms, released after 250 ms
    tgl = p.add(f"#X obj {CX} {top_y} tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1;")
    met = p.add(f"#X obj {CX} {top_y + 28} metro 500;")
    p.conn(tgl, 0, met, 0)
    tbb = p.add(f"#X obj {CX + 100} {top_y} t b b;")
    p.conn(met, 0, tbb, 0)
    on = p.add(f"#X msg {CX + 160} {top_y} note 48 100 {COMMA} note 55 90 {COMMA} note 60 80 {COMMA} note 64 70;")
    dly = p.add(f"#X obj {CX + 100} {top_y + 28} delay 250;")
    off = p.add(f"#X msg {CX + 190} {top_y + 28} note 48 0 {COMMA} note 55 0 {COMMA} note 60 0 {COMMA} note 64 0;")
    p.conn(tbb, 1, on, 0); p.conn(tbb, 0, dly, 0); p.conn(dly, 0, off, 0)
    p.conn(on, 0, snd0, 0); p.conn(off, 0, snd0, 0)
    x = CX
    for text in ("60 100", "60 0", "dump", "defaults", "panic", "print"):
        m = p.add(f"#X msg {x} {util_y} {text};")
        p.conn(m, 0, snd0, 0)
        x += 7 * len(text) + 4 + 16
    snds = {}
    for i, (cname, _, _) in enumerate(CONTROLS):
        r, c = divmod(i, 4)
        cx = CX + c * 112
        ay = ctl_y0 + ROW * r
        if r not in snds:
            snds[r] = p.add(f"#X obj {CX} {ay + 58} s {bus};")
        a = p.add(f"#X floatatom {cx} {ay} 4 0 127 0 - - -;")
        m = p.add(f"#X msg {cx} {ay + 30} {cname} {DOLLAR}1;")
        p.conn(a, 0, m, 0)
        p.conn(m, 0, snds[r], 0)
    # MIDI keyboard straight into the object
    nin = p.add(f"#X obj {CX + 200} {r_y - 36} notein;")
    pck = p.add(f"#X obj {CX + 200} {r_y} pack f f f;")
    p.conn(nin, 0, pck, 0); p.conn(nin, 1, pck, 1); p.conn(nin, 2, pck, 2)
    p.conn(pck, 0, o, 0)
    mull = p.add(f"#X obj {CX} {mul_y} *~ 0.3;")
    mulr = p.add(f"#X obj {CX + 100} {mul_y} *~ 0.3;")
    dac = p.add(f"#X obj {CX} {dac_y} dac~;")
    pr = p.add(f"#X obj {CX + 330} {mul_y} print {SHORT.upper()};")
    p.conn(o, 0, mull, 0); p.conn(o, 1, mulr, 0)
    p.conn(mull, 0, dac, 0); p.conn(mulr, 0, dac, 1)
    p.conn(o, 2, pr, 0)

    for title, lines, sy, extra in placed:
        p.heading(CX, sy, title, 14)
        p.comment(TX, sy + 24, lines)
        if extra:
            ly = sy + 24 + LINE * len(lines) + 10
            m1 = p.add(f"#X msg {TX} {ly} dir;")
            pc = p.add(f"#X obj {TX} {ly + 26} pdcontrol;")
            lp = p.add(f"#X obj {TX} {ly + 52} list prepend skpreset-help.pd;")
            m2 = p.add(f"#X msg {TX} {ly + 78} {SEMI} pd open {DOLLAR}1 {DOLLAR}2;")
            p.conn(m1, 0, pc, 0); p.conn(pc, 0, lp, 0); p.conn(lp, 0, m2, 0)
            p.add(f"#X text {TX + 50} {ly} <- click to open skpreset-help.pd, f 40;")
    def panel(y0, title, note, items):
        p.heading(CX, y0, title, 14)
        p.comment(TX, y0 + 24, [note])
        snd = {}
        for i, (nm, lo, hi) in enumerate(items):
            r, c = divmod(i, 4)
            x = CX + c * 112
            ay = y0 + 24 + 28 + ROWA * r
            if r not in snd:
                snd[r] = p.add(f"#X obj {CX} {ay + 52} s {bus};")
            a = p.add(f"#X floatatom {x} {ay} 6 {lo} {hi} 0 - - -;")
            m = p.add(f"#X msg {x} {ay + 26} {nm} {DOLLAR}1;")
            p.conn(a, 0, m, 0)
            p.conn(m, 0, snd[r], 0)
    panel(adv_y, "Advanced settings", "The settings above as number boxes, sent via the s/r bus.", ADV)
    panel(vint_y, "Vintage stages", "Single stages instead of era (era sets them all again).", VINT)
    p.comment(TX, credits_y, ["Credits: CZ engine from melodee by Kerem Kilic and the",
                              "uPD933 model by Devin Acker (MAME), BSD-3-Clause",
                              "Pd wrapper cz4~, built with Claude (Anthropic)"])

    head = f"#N canvas 348 20 {YEL_X + YEL_W + 2 * M + 20} {bar_bottom + M} 12;"
    return head + "\n" + "\n".join(p.lines) + "\n" + "\n".join(p.conns) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else OBJ + "-help.pd"
    with open(out, "w") as f:
        f.write(build())
    print("wrote", out)
