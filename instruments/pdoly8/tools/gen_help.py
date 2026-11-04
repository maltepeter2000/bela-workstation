#!/usr/bin/env python3
"""Generate pdoly8~-help.pd in the layout of the skandrums-pd help patches.

usage: gen_help.py [outfile]   (default: pdoly8~-help.pd)

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

OBJ, SHORT = "pdoly8~", "pdoly8"
SUBTITLE = "4-Voice Poly-800 tone generation - MSM5232 foot synthesis"

# controls: name, default, description (index = position)
CONTROLS = [
    ("wav1", 127, "Waveform of DCO1: 0 = square (feet added 1:1:1:1), "
                  "127 = saw (1:1/2:1/4:1/8, a step wave). In between the "
                  "weights move continuously."),
    ("wav2", 127, "Waveform of DCO2, same range."),
    ("har1", 64, "Foot combination of DCO1. 15 original combinations at "
                 "0, 9, 18 ... 127, from one to the next a single foot "
                 "fades in or out: 16' alone, all feet at 64, 2' alone."),
    ("har2", 64, "Foot combination of DCO2, same path."),
    ("intv", 0, "Interval of DCO2 above DCO1, 0-12 semitones."),
    ("detn", 0, "Detune of DCO2, 0 to -20 cents (the original's 4 steps "
                "lie at 0, 42, 85, 127)."),
    ("lev1", 127, "Level of DCO1."),
    ("lev2", 0, "Level of DCO2 (DOUBLE only). Above full scale "
                "(lev1 + lev2 > 127) both are scaled down together."),
]

SETTINGS = [
    "mode (2) 1 = WHOLE: one DCO per voice, 2 = DOUBLE: two DCOs",
    "oct1, oct2 (0) octave of DCO1 / DCO2 -1, 0, 1",
    "nois (0) white noise level 0-15, sounds while a voice sounds",
    "sprd (0.5) stereo spread of the 4 voices 0-1",
    "levl (0.5) output level 0-2",
    "mrph (8) smoothing of wav, har and lev changes in ms",
    "hold (1) released voices keep sounding until the next phrase",
    "a4 (440) tuning in Hz",
    "tune (0) master tune +-50 cents",
    "bnd (2) pitch bend range in semitones (original: up to 7)",
    "mgf (5) vibrato rate in Hz",
    "mgi (0) vibrato depth in cents (triangle on the master clock)",
    "hsnp (1) plateau around each foot combination, in har steps",
    "ch1 (0) MIDI channel for p v ch lists, 0 = any",
]

# number boxes in the bottom panel: name, min, max
ADV = [("mode", 1, 2), ("oct1", -1, 1), ("oct2", -1, 1), ("nois", 0, 15),
       ("sprd", 0, 1), ("levl", 0, 2), ("mrph", 0, 100), ("hold", 0, 1),
       ("a4", 400, 480), ("tune", -50, 50), ("bnd", 0, 12), ("mgf", 0, 30),
       ("mgi", 0, 100), ("hsnp", 0, 4), ("ch1", 0, 16), ("bend", -1, 1)]
VINT = []


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
        "<p> <v> <channel>: from [notein], filtered by ch1",
        "<control> <0-127>: set a control by name, e.g. har1 64",
        "ctl <index> <0-127>, params v0 v1 ...: set by index",
        "<setting> <value>: e.g. mode 1 (see Settings below)",
        "bend <-1..1>: pitch bend of all voices (master clock)",
        "defaults, reset: default values, reset also stops all voices",
        "panic: all voices off",
        "get <name>: reply 'value <name> <v>' on outlet 2",
        "print: voices, values and sample rate to the Pd window",
        "dump: send all values to outlet 2 as 'dump " + SHORT + " v0 ..'",
        "dump " + SHORT + " v0 v1 ...: restore such a dump (this voice only)",
        "voice <i> <p>, voiceoff <i>: play one voice directly"]))
    clines = []
    for i, (cn, dv, txt) in enumerate(CONTROLS):
        clines += wrap(f"{i} {cn} ({dv}) {txt}")
    sections.append(("Controls (index name default)", clines))
    sections.append(("Settings (name default)", SETTINGS))
    sections.append(("How the Poly-800 makes its sound", [
        "One master clock feeds the MSM5232: 8 tone generators in two",
        "groups (TG0-3 = DCO1, TG4-7 = DCO2). Each key becomes an",
        "integer divider from the chip's ROM, then a binary counter",
        "whose bits are the 16', 8', 4' and 2' squares, locked in",
        "phase. The four feet of a group are summed to a square or a",
        "step saw. Tune, bend and vibrato move the master clock, so",
        "all voices move together. Detune thins out DCO2's clock.",
        "Every edge is band-limited: analog edges, no aliasing."]))
    sections.append(("Foot path (har)", [
        "0: 16' / 9: 16'8' / 18: 16'8'4' / 27: 16'4' / 36: 16'4'2'",
        "45: 16'2' / 54: 16'8'2' / 64: all feet / 73: 8'4'2'",
        "82: 8'2' / 91: 8' / 100: 8'4' / 109: 4' / 118: 4'2' / 127: 2'",
        "On these values the sound is exactly the original. In",
        "between one foot fades, so har can be modulated smoothly."]))
    sections.append(("Envelopes", [
        "pdoly8~ has no envelopes and no filter yet. A key gates its",
        "voice with a short de-click ramp; shape the level with an",
        "external VCA. With hold 1 a released chord keeps sounding",
        "until the next phrase. The chip's counters run freely, so",
        "a note-on does not restart the phase (as on the original)."]))
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
    # each row into its own [s $0-pdoly8] (the [r] above the object picks it up)
    ROWA = 76
    adv_y = y
    adv_rows = (len(ADV) + 3) // 4
    y = adv_y + 24 + 28 + ROWA * adv_rows + 20
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
    p.comment(TX, credits_y, ["Credits: pdoly8~ by Peter with Claude (Anthropic).",
                              "Modelled on the Korg Poly-800 service manual; MSM5232",
                              "scale ROM data as documented in MAME's msm5232.cpp"])

    head = f"#N canvas 348 20 {YEL_X + YEL_W + 2 * M + 20} {bar_bottom + M} 12;"
    return head + "\n" + "\n".join(p.lines) + "\n" + "\n".join(p.conns) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else OBJ + "-help.pd"
    with open(out, "w") as f:
        f.write(build())
    print("wrote", out)
