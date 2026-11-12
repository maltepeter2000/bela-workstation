#!/usr/bin/env python3
"""Generate d50~-help.pd. Never edit the help patch by hand: change this
script and run it again.

    python3 tools/gen_help.py            writes d50~/d50~-help.pd
"""
import os

BS = chr(92)
SEMI, COMMA, DOLLAR, SPACE = BS + ";", BS + ",", BS + "$", BS + " "
M = 20
YEL_X, YEL_Y, YEL_W = 2 * M, M, 700
PINK_X, PINK_W, PINK_TOP = M, M, YEL_Y + 3 * M
BLUE_W = 200
BLUE_X = YEL_X + YEL_W + M - BLUE_W
BLUE_TOP = PINK_TOP + 3 * M
CX = YEL_X + M
LBL = 24
TX = CX + LBL - 2
LINE = 16
MAXCH = 62
GAP = 4
PINK, YELLOW, BLUE = "#ffcfd6", "#fafaf2", "#1e3791"

OBJ = "d50~"
SHORT = "d50"
BUS = f"{DOLLAR}0-{SHORT}"
SUBTITLE = "Roland D-50 linear arithmetic synthesizer"

# (name, min, max, default, description)
CONTROLS = [
    ("preset", 0, 127, "0", "patch number, counted from 0 over all loaded banks."),
    ("volume", 0, 100, "80", "master volume in percent."),
    ("reverb", 0, 100, "patch", "reverb balance, dry to wet."),
    ("chorus", 0, 100, "patch", "chorus balance, dry to wet."),
    ("balance", 0, 100, "patch", "tone balance, lower tone at 0, upper tone at 100. "
                                 "Without effect on patches that use one tone."),
    ("tune", -50, 50, "0", "master tune in cents."),
    ("voices", 1, 8, "8", "polyphony per tone. A patch that uses one tone plays "
                          "twice as many notes."),
    ("bendrange", 0, 12, "patch", "range of the bend message in semitones."),
]

INLETS = [
    "1 float: note number. Velocity is taken from inlet 2",
    "  and velocity 0 is a note off.",
    "1 list: note and velocity.",
    "1 anything: a message, see Messages.",
    "2 float: velocity for the next note, 0 to 127 (default 100).",
    "3 anything: a message, the same set as inlet 1.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: preset names and state, as name and value.",
]

MESSAGES = [
    "preset n: select a patch and report its name.",
    "presets: report every patch as preset, number, name,",
    "  then presets and the count.",
    "info: report the current state, one message per value.",
    "bend f: pitch wheel, -1 to 1, times bendrange.",
    "mod f: modulation lever, 0 to 1.",
    "aftertouch f: channel pressure, 0 to 1.",
    "sustain 0/1: hold pedal.",
    "stop: all notes off.",
    "reset: all notes off and the effect tails emptied.",
]

MIDI = [
    "notein: note and velocity as a list into inlet 1.",
    "bendin: scaled to -1..1 and sent as bend.",
    "ctlin 1: modulation lever, sent as mod.",
    "ctlin 64: hold pedal, sent as sustain.",
    "touchin: channel pressure, sent as aftertouch.",
    "pgmin: program change, minus 1, sent as preset.",
]

DATA = [
    "The object contains no Roland data. It loads two kinds of file from "
    "the folder of the external, or else from the folder of the patch.",
    "d50_pcm.bin: the PCM samples, written by tools/make_d50_data.py from "
    "your own ROM images. Without it the object stays silent and says so "
    "in the Pd window.",
    "Every .syx file beside it is read as a D-50 bulk dump of 64 patches. "
    "The files are loaded in alphabetical order, PND50-00.syx first, and "
    "their patches are numbered through. Without a bank the object plays "
    "eight built-in presets.",
    "The engine runs at 32 kHz, the rate of the original, and is resampled "
    "to the rate of Pd.",
]

PRESETS = [
    "The patches live in the object. Send preset n to select one. The "
    "third outlet answers with the number and the name, which is all a "
    "panel or a headless host needs.",
    "A patch change resets reverb, chorus, equalizer, balance, bendrange "
    "and portamento to the values stored in the patch.",
]

# (title, groups, examples, paragraphs); a group is a list of (name, min, max)
BLOCKS = [
    ("Performance",
     [[("bend", -1, 1), ("mod", 0, 1), ("aftertouch", 0, 1), ("sustain", 0, 1)],
      [("porta", 0, 1), ("portatime", 0, 100)]],
     [],
     ["bend (0): pitch wheel, -1 to 1. The range in semitones is bendrange.",
      "mod (0): modulation lever, 0 to 1. What it does is part of the patch.",
      "aftertouch (0): channel pressure, 0 to 1.",
      "sustain (0): hold pedal. Notes released while it is 1 sound until it returns to 0.",
      "porta (patch): portamento switch.",
      "portatime (patch): portamento time, 0 to 100."]),
    ("Reverb",
     [[("reverb", 0, 100), ("reverbtype", 1, 32)]],
     [],
     ["reverb (patch): reverb balance, 0 is dry.",
      "reverbtype (patch): one of the 32 rooms, halls, delays and gates of "
      "the D-50. A change of type empties the reverb."]),
    ("Chorus",
     [[("chorus", 0, 100), ("chorustype", 1, 8)],
      [("chorusrate", 0, 100), ("chorusdepth", 0, 100)]],
     [],
     ["The D-50 has one chorus per tone. These messages set both tones together.",
      "chorus (patch): chorus balance, 0 is dry.",
      "chorustype (patch): chorus 1 and 2, flanger 1 and 2, feedback chorus, "
      "tremolo, chorus tremolo, dimension.",
      "chorusrate (patch): modulation rate.",
      "chorusdepth (patch): modulation depth."]),
    ("Equalizer",
     [[("eqlowfreq", 0, 15), ("eqlowgain", -12, 12)],
      [("eqhighfreq", 0, 21), ("eqhighq", 0, 8), ("eqhighgain", -12, 12)]],
     [],
     ["eqlowfreq (patch): frequency step of the low shelf.",
      "eqlowgain (patch): gain of the low shelf in dB.",
      "eqhighfreq (patch): frequency step of the high band.",
      "eqhighq (patch): width step of the high band.",
      "eqhighgain (patch): gain of the high band in dB."]),
    ("Additional settings",
     [[("governor", 0, 1)]],
     [],
     ["governor (0): when 1, the object ends the oldest released note "
      "early whenever a block takes longer to compute than it lasts. Held "
      "notes are never ended. Meant for small machines such as Bela."]),
]

CREDITS = ("Credits: Modelled on PicoFaceD5 from PicoVintageSynthCollection by "
           "Michi71; Pure Data adaptation with help by Claude")


def esc(s):
    return s.replace(",", " " + COMMA).replace(";", SEMI).replace("$", DOLLAR)


class Patch:
    def __init__(self):
        self.lines, self.conns = [], []

    def add(self, t):
        self.lines.append(t)
        return len(self.lines) - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def cnv(self, x, y, w, h, lbl="empty", xo=20, yo=12, fs=30, bg=YELLOW, sel=19):
        return self.add(f"#X obj {x} {y} cnv {sel} {w} {h} empty empty {lbl} "
                        f"{xo} {yo} 0 {fs} {bg} #000000 0;")

    def heading(self, x, y, text, fs):
        return self.cnv(x, y, 10, 10, text.replace(" ", SPACE), LBL, 6, fs, BLUE, 10)

    def comment(self, x, y, lines, para=False, semi=False):
        if not para:
            assert all(len(l) <= MAXCH for l in lines), [l for l in lines if len(l) > MAXCH]
        elif not semi:
            assert all(";" not in l for l in lines)
        return self.add(f"#X text {x} {y} " +
                        (" " + SEMI + " ").join(esc(l) for l in lines) +
                        f", f {MAXCH + 2};")

    def obj(self, x, y, text):
        return self.add(f"#X obj {x} {y} {text};")

    def msg(self, x, y, text):
        return self.add(f"#X msg {x} {y} {text};")

    def atom(self, x, y, w, lo, hi):
        return self.add(f"#X floatatom {x} {y} {w} {lo} {hi} 0 - - - 0;")


def wrap(t, w=MAXCH):
    out, cur = [], ""
    for word in t.split():
        if cur and len(cur) + 1 + len(word) > w:
            out.append(cur)
            cur = word
        else:
            cur = (cur + " " + word).strip()
    return out + [cur]


def nlines(paras):
    return sum(len(wrap(t)) for t in paras)


def rows_of(groups):
    return [g[k:k + 4] for g in groups for k in range(0, len(g), 4)]


def split_text(groups, paras):
    """Paragraphs go to the group whose parameter name starts them; those
    before the first such paragraph are the block's intro."""
    intro, per = [], [[] for _ in groups]
    cur = None
    for t in paras:
        head = t.split(" (")[0].split(",")[0].strip()
        hit = next((gi for gi, g in enumerate(groups)
                    if any(head == nm for nm, _, _ in g)), None)
        if hit is not None:
            cur = hit
        if cur is None:
            intro.append(t)
        else:
            per[cur].append(t)
    return intro, per


def draw_block(p, title, groups, examples, paras, y):
    """p None = layout only; returns y after the block."""
    if p:
        p.heading(CX, y, title, 14)
    y += 28
    intro, per = split_text(groups, paras)
    if intro:
        if p:
            p.comment(TX, y, intro, para=True)
        y += LINE * nlines(intro) + GAP
    for gi, g in enumerate(groups):
        for row in rows_of([g]):
            if p:
                snd = p.obj(CX + 400, y + 24, f"s {BUS}")
                for c, (nm, lo, hi) in enumerate(row):
                    a = p.atom(CX + c * 100, y, 6, lo, hi)
                    m = p.msg(CX + c * 100, y + 24, f"{nm} {DOLLAR}1")
                    p.conn(a, 0, m, 0)
                    p.conn(m, 0, snd, 0)
            y += 54
        if examples and gi == len(groups) - 1:
            if p:
                sp = p.obj(CX + 400, y, f"s {BUS}")
                for i, txt in enumerate(examples):
                    r, c = divmod(i, 3)
                    m = p.msg(CX + c * 130, y + 34 * r, txt)
                    p.conn(m, 0, sp, 0)
            y += 34 * ((len(examples) + 2) // 3)
        if per[gi]:
            if p:
                p.comment(TX, y, per[gi], para=True)
            y += LINE * nlines(per[gi]) + GAP
    return y + 10


def section(p, y, title, lines, para=False):
    """Heading plus one comment; returns y after it."""
    if p:
        p.heading(CX, y, title, 14)
    y += 28
    if p:
        p.comment(TX, y, lines, para=para)
    y += LINE * (nlines(lines) if para else len(lines)) + GAP
    return y + 10


def build(p):
    """Draws everything below the backgrounds; returns the bottom y."""
    y = YEL_Y + M
    if p:
        p.heading(CX, y, OBJ, 22)
        p.heading(CX, y + 36, SUBTITLE, 16)
    y += 80

    # ---- demo row
    if p:
        tg = p.obj(CX, y, "tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1")
        me = p.obj(CX + 30, y, "metro 500")
        on = p.msg(CX + 110, y, f"57 100 {COMMA} 60 100 {COMMA} 64 100")
        de = p.obj(CX + 30, y + 30, "delay 250")
        off = p.msg(CX + 110, y + 30, f"57 0 {COMMA} 60 0 {COMMA} 64 0")
        s1 = p.obj(CX + 400, y + 30, f"s {BUS}")
        p.conn(tg, 0, me, 0)
        p.conn(me, 0, on, 0)
        p.conn(me, 0, de, 0)
        p.conn(de, 0, off, 0)
        p.conn(on, 0, s1, 0)
        p.conn(off, 0, s1, 0)
    y += 64
    if p:
        s2 = p.obj(CX + 400, y, f"s {BUS}")
        for i, txt in enumerate(["60 100", "60 0", "info", "presets", "stop", "reset"]):
            m = p.msg(CX + i * 62, y, txt)
            p.conn(m, 0, s2, 0)
    y += 40

    # ---- main controls: two rows of four
    for r in range(2):
        if p:
            snd = p.obj(CX + 400, y + 50, f"s {BUS}")
            for c in range(4):
                nm, lo, hi, _, _ = CONTROLS[r * 4 + c]
                a = p.atom(CX + c * 112, y, 4, lo, hi)
                m = p.msg(CX + c * 112, y + 24, f"{nm} {DOLLAR}1")
                p.conn(a, 0, m, 0)
                p.conn(m, 0, snd, 0)
        y += 90 if r == 0 else 64

    # ---- object area
    if p:
        ni = p.obj(CX + 130, y, "notein")
        pk = p.obj(CX + 130, y + 30, "pack f f")
        bi = p.obj(CX + 220, y, "bendin")
        be = p.obj(CX + 220, y + 30, "expr (" + DOLLAR + "f1 - 8192) / 8192")
        bm = p.msg(CX + 220, y + 60, f"bend {DOLLAR}1")
        rc = p.obj(CX, y + 60, f"r {BUS}")
        ob = p.obj(CX, y + 100, OBJ)
        p.conn(ni, 0, pk, 0)
        p.conn(ni, 1, pk, 1)
        p.conn(pk, 0, ob, 0)
        p.conn(bi, 0, be, 0)
        p.conn(be, 0, bm, 0)
        p.conn(bm, 0, ob, 0)
        p.conn(rc, 0, ob, 0)
        gl = p.obj(CX, y + 134, "*~ 0.3")
        gr = p.obj(CX + 60, y + 134, "*~ 0.3")
        da = p.obj(CX, y + 164, "dac~")
        pr = p.obj(CX + 130, y + 134, "print D50")
        so = p.obj(CX + 220, y + 134, f"s {BUS}-out")
        p.conn(ob, 0, gl, 0)
        p.conn(ob, 1, gr, 0)
        p.conn(gl, 0, da, 0)
        p.conn(gr, 0, da, 1)
        p.conn(ob, 2, pr, 0)
        p.conn(ob, 2, so, 0)
    y += 200

    # ---- remaining MIDI, each next to its own send
    if p:
        sm = p.obj(CX + 400, y + 60, f"s {BUS}")
        cols = [("ctlin 1", "/ 127", "mod"), ("ctlin 64", "> 63", "sustain"),
                ("touchin", "/ 127", "aftertouch"), ("pgmin", "- 1", "preset")]
        for c, (src, op, name) in enumerate(cols):
            a = p.obj(CX + c * 110, y, src)
            b = p.obj(CX + c * 110, y + 30, op)
            m = p.msg(CX + c * 110, y + 60, f"{name} {DOLLAR}1")
            p.conn(a, 0, b, 0)
            p.conn(b, 0, m, 0)
            p.conn(m, 0, sm, 0)
    y += 100

    # ---- text sections
    y = section(p, y, "Inlets", INLETS)
    y = section(p, y, "Outlets", OUTLETS)
    y = section(p, y, "Messages", MESSAGES)
    y = section(p, y, "MIDI", MIDI)
    y = section(p, y, "Controls",
                [f"{nm} ({dflt}): {desc}" for nm, _, _, dflt, desc in CONTROLS],
                para=True)
    y = section(p, y, "Data files", DATA, para=True)

    # ---- presets, with live objects
    if p:
        p.heading(CX, y, "Presets", 14)
    y += 28
    if p:
        a = p.atom(CX, y, 4, 0, 127)
        m = p.msg(CX, y + 24, f"preset {DOLLAR}1")
        s = p.obj(CX, y + 50, f"s {BUS}")
        p.conn(a, 0, m, 0)
        p.conn(m, 0, s, 0)
        r = p.obj(CX + 130, y, f"r {BUS}-out")
        ro = p.obj(CX + 130, y + 24, "route preset")
        un = p.obj(CX + 130, y + 50, "unpack f s")
        fa = p.atom(CX + 130, y + 78, 4, 0, 0)
        sa = p.add(f"#X symbolatom {CX + 190} {y + 78} 22 0 0 0 - - - 0;")
        p.conn(r, 0, ro, 0)
        p.conn(ro, 0, un, 0)
        p.conn(un, 0, fa, 0)
        p.conn(un, 1, sa, 0)
    y += 106
    if p:
        p.comment(TX, y, PRESETS, para=True)
    y += LINE * nlines(PRESETS) + GAP + 10

    # ---- settings blocks
    for title, groups, examples, paras in BLOCKS:
        y = draw_block(p, title, groups, examples, paras, y)

    y += 6
    if p:
        p.comment(TX, y, [CREDITS], para=True, semi=True)
    y += LINE * nlines([CREDITS]) + M
    return y


def main():
    bottom = build(None)
    p = Patch()
    p.cnv(YEL_X, YEL_Y, YEL_W, bottom - YEL_Y)
    p.cnv(PINK_X, PINK_TOP, PINK_W, bottom - PINK_TOP, bg=PINK)
    p.cnv(BLUE_X, BLUE_TOP, BLUE_W, bottom - BLUE_TOP, bg=BLUE)
    assert build(p) == bottom
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                       OBJ, f"{OBJ}-help.pd")
    with open(out, "w") as f:
        f.write(f"#N canvas 348 20 820 {bottom + M} 12;\n")
        f.write("\n".join(p.lines) + "\n")
        f.write("\n".join(p.conns) + "\n")
    print(f"wrote {os.path.normpath(out)} ({len(p.lines)} objects, "
          f"{len(p.conns)} connections, height {bottom + M})")


if __name__ == "__main__":
    main()
