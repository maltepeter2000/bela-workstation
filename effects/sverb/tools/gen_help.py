#!/usr/bin/env python3
"""Generates pd/sverb~-help.pd in the layout of pd-help-layout (do not edit the patch by hand).

usage: python3 tools/gen_help.py      (make gen)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "pd", "sverb~-help.pd")

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

OBJ, SHORT = "sverb~", "sverb"
BUS = f"{DOLLAR}0-{SHORT}"
SUBTITLE = "Signalsmith reverb as a send effect: mono or stereo in"

# (index, name, default 0-127 as the object reports it, text)
CONTROLS = [
    (0, "dry", "0", "dry (0): level of the input in the output, 0 ... 1 ... 4 (64 = 1). "
     "0 for a send effect, the mixer adds the dry signal."),
    (1, "wet", "63.5", "wet (63.5 = 1): level of the reverb, 0 ... 1 ... 4."),
    (2, "room", "49.9 = 80 ms", "room (49.9 = 80 ms): room size 10 ... 100 ... 200 ms "
     "(1 ms is about 1 foot). Sets the spread of all delay lengths, changes are cross-faded."),
    (3, "decay", "41.2 = 1 s", "decay (41.2 = 1 s): RT20, the time to fall by 20 dB, "
     "0.01 ... 2 ... 30 s. To -60 dB it takes three times as long."),
    (4, "early", "87.9 = 1.5", "early (87.9 = 1.5): level of the early reflections, "
     "0 ... 1 ... 2.5."),
    (5, "detune", "34.6 = 2", "detune (34.6 = 2): slow pitch movement inside the feedback "
     "loop, 0 ... 5 ... 50. Breaks up metallic ringing, high values make a chorus tail."),
    (6, "lowcut", "63.5 = 80 Hz", "lowcut (63.5 = 80 Hz): high-pass at the output, "
     "10 ... 80 ... 500 Hz."),
    (7, "lowdamp", "40.6 = 1.5", "lowdamp (40.6 = 1.5): see Damping."),
    (8, "highcut", "106.4 = 12 kHz", "highcut (106.4 = 12 kHz): low-pass at the output, "
     "1 ... 5 ... 20 kHz."),
    (9, "highdamp", "78.2 = 2.5", "highdamp (78.2 = 2.5): see Damping."),
]
MAIN = ["dry", "wet", "room", "decay", "early", "detune", "lowcut", "highcut"]

BLOCKS = [
    ("Damping", [[("lowdamp", 0, 127), ("highdamp", 0, 127)]], [], [
        "The tail loses lows and highs faster than the middle, as in a real room. The shelf "
        "filters sit inside the feedback loop: lows below lowcut + 100 Hz, highs above "
        "highcut / 2.",
        "lowdamp (1.5): how much faster the lows decay than decay says, 1 ... 2 ... 10 "
        "(1 = not faster).",
        "highdamp (2.5): the same for the highs. Higher values give a darker tail.",
    ]),
    ("Values in units and modes", [], [
        "val room 150", "val decay 8", "val highcut 6000",
        "val lowcut 200", "preset ambient", "clear",
        "send", "insert", "mono 1",
        "mono 0", "mono auto", "get decay",
    ], [
        "val <control> <value>: sets a control in its own unit (ms, s, Hz, gain), the same "
        "curve as 0-127 the other way round.",
        "preset ambient: the preset of Signalsmith Basics (long, dark, detuned), dry stays.",
        "clear: empties the reverb, the tail stops at once.",
        "send, insert: dry 0 wet 1 (the default) or dry 1 wet 0.5, the other controls stay. "
        "[sverb~ insert] starts as insert.",
        "mono 1, 0, auto: mono input on, off, or from the connection (default): with nothing "
        "connected to the right inlet the left input goes into both sides of the reverb. The "
        "output is stereo either way.",
    ]),
]

INLETS = [
    "0: signal left (mono)  ,  messages",
    "1: signal right. Not connected: mono (the left input feeds",
    "  both sides of the reverb, the output stays stereo)",
    "2: messages (the same as inlet 0)",
]
OUTLETS = [
    "0: signal left  ,  1: signal right",
    "2: events: value <control> <0-127> <v>  ,  dump sverb ...",
    "  display <title> <value> on every change (lkdisplay, OLED)",
]
MESSAGES = [
    "<control> <0-127>: set a control by name, e.g. decay 80",
    "val <control> <value>: in its unit, e.g. val room 120",
    "ctl <index> <0-127>  ,  params v0 .. v9: set by index",
    "defaults  ,  reset: all controls to the defaults of the mode",
    "send  ,  insert: dry 0 wet 1  /  dry 1 wet 0.5",
    "get <control>: answers value <control> <0-127> <v>",
    "dump: answers dump sverb v0 .. v9, which restores them",
    "preset ambient  ,  clear  ,  mono auto|0|1  ,  print",
]
ALGO = [
    "An FDN (feedback delay network) of 8 channels. The stereo input is spread onto the 8 "
    "channels and goes through two diffusion steps: each a set of 8 delays of different "
    "length followed by a Hadamard matrix, which mixes every channel into every other. One "
    "click becomes 64 echoes.",
    "The feedback loop: 8 delays with lengths spread over about 0.7 to 1.4 x room, a "
    "Householder matrix mixes them, then the low and high shelves (damping) and the decay "
    "gain, set from decay and room so the level falls 20 dB in decay seconds. Three of the 8 "
    "delays are modulated by a slow LFO (detune).",
    "Early reflections: a separate set of delays taps the diffused input once, without "
    "feedback, and adds it to the tail (early).",
    "Two more diffusion steps after the loop, then the 8 channels are folded back to stereo "
    "and filtered (lowcut, highcut). Left and right come from different mixes of the 8 "
    "channels, so even a mono input gives a wide, decorrelated stereo tail.",
    "All delay lengths derive from room with fixed random seeds, a change of room fades "
    "between the old and the new lengths. In bela-workstation the LFO runs as a rotating "
    "phasor (cheaper on the Cortex-A53), the sound is the same.",
]
PRESET_TEXT = [
    "Choose slot 0-15: the current slot is stored, the new one loaded. An empty slot takes "
    "the current sound. The values are kept in [text define -k sverb-slots] and saved with "
    "this patch. The same logic as in skandrums and vrs23~: outlet 2 -> skpreset left, "
    "skpreset left -> sverb~ (here through the bus).",
]
CREDITS = [
    "Credits: Modelled on the reverb of Signalsmith Basics (Geraint",
    "Luff, Signalsmith Audio, MIT) with help by Claude.",
    "skpreset from skandrums-pd (CC0). Reference: vrs23-clap.wav",
]


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

    def obj(self, x, y, t):
        return self.add(f"#X obj {x} {y} {t};")

    def msg(self, x, y, t):
        return self.add(f"#X msg {x} {y} {t};")

    def cnv(self, x, y, w, h, lbl="empty", xo=20, yo=12, fs=30, bg=YELLOW, sel=19):
        return self.add(f"#X obj {x} {y} cnv {sel} {w} {h} empty empty {lbl} {xo} {yo} 0 {fs} {bg} #000000 0;")

    def heading(self, x, y, text, fs):
        return self.cnv(x, y, 10, 10, text.replace(" ", SPACE), LBL, 6, fs, BLUE, 10)

    def comment(self, x, y, lines, para=False):
        if not para:
            assert all(len(l) <= MAXCH for l in lines), [l for l in lines if len(l) > MAXCH]
        return self.add(f"#X text {x} {y} " + (" " + SEMI + " ").join(esc(l) for l in lines)
                        + f", f {MAXCH + 2};")


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
    names = [[n for n, _, _ in g] for g in groups]
    intro, per, cur = [], [[] for _ in groups], None
    for t in paras:
        head = t.split(" (")[0].split(":")[0].split(", ")
        hit = next((gi for gi, ns in enumerate(names) if any(h in ns for h in head)), None)
        if hit is not None:
            cur = hit
        if cur is None:
            intro.append(t)
        else:
            per[cur].append(t)
    if not groups:
        intro = paras
    return intro, per


def draw_block(p, title, groups, examples, paras, y, bus):
    if p:
        p.heading(CX, y, title, 14)
    y += 28
    intro, per = split_text(groups, paras)
    if examples:
        if p:
            sp = p.obj(CX + 400, y, f"s {bus}")
            for i, txt in enumerate(examples):
                r, c = divmod(i, 3)
                m = p.msg(CX + c * 130, y + 34 * r, txt)
                p.conn(m, 0, sp, 0)
        y += 34 * ((len(examples) + 2) // 3)
    if intro:
        if p:
            p.comment(TX, y, intro, para=True)
        y += LINE * nlines(intro) + GAP
    for gi, g in enumerate(groups):
        for row in rows_of([g]):
            if p:
                snd = p.obj(CX + 400, y + 24, f"s {bus}")
                for c, (nm, lo, hi) in enumerate(row):
                    a = p.add(f"#X floatatom {CX + c * 100} {y} 6 {lo} {hi} 0 - {DOLLAR}0-n-{nm} -;")
                    m = p.msg(CX + c * 100, y + 24, f"{nm} {DOLLAR}1")
                    p.conn(a, 0, m, 0)
                    p.conn(m, 0, snd, 0)
            y += 54
        if per[gi]:
            if p:
                p.comment(TX, y, per[gi], para=True)
            y += LINE * nlines(per[gi]) + GAP
    return y + 10


def section(p, y, title, lines, para=False):
    if p:
        p.heading(CX, y, title, 14)
        p.comment(TX, y + 24, lines, para)
    n = nlines(lines) if para else len(lines)
    return y + 24 + LINE * n + 14


def numbers_subpatch(p, x, y):
    """[pd numbers]: shows dumps (from the object and from skpreset) in the number boxes"""
    names = [c[1] for c in CONTROLS]
    sub = ["#N canvas 0 0 700 300 numbers 0;",
           f"#X obj 20 20 r {DOLLAR}0-{SHORT}-out;",
           f"#X obj 200 20 r {DOLLAR}0-{SHORT};",
           "#X obj 20 60 route dump;",
           f"#X obj 20 90 route {SHORT};",
           "#X obj 20 120 unpack " + " ".join("f" for _ in names) + ";"]
    conns = ["#X connect 0 0 2 0;", "#X connect 1 0 2 0;", "#X connect 2 0 3 0;",
             "#X connect 3 0 4 0;"]
    for i, n in enumerate(names):
        sub.append(f"#X msg {20 + i * 65} 160 set {DOLLAR}1;")
        sub.append(f"#X obj {20 + i * 65} 200 s {DOLLAR}0-n-{n};")
        mi, si = 5 + 2 * i, 6 + 2 * i
        conns += [f"#X connect 4 {i} {mi} 0;", f"#X connect {mi} 0 {si} 0;"]
    body = "\n".join(sub + conns)
    return p.add(body + f"\n#X restore {x} {y} pd numbers;")


def build():
    # ---- layout pass ----
    y_title = YEL_Y + 16
    y_demo = 110
    y_main = y_demo + 130
    y_objs = y_main + 2 * 90 + 10
    y_sections = y_objs + 340
    y = y_sections
    plan = []
    for kind, args in [("sec", ("Inlets", INLETS, False)), ("sec", ("Outlets", OUTLETS, False)),
                       ("sec", ("Messages", MESSAGES, False)),
                       ("sec", ("Controls (default)", [c[3] for c in CONTROLS], True))]:
        plan.append((kind, args, y))
        y = section(None, y, *args)
    for b in BLOCKS:
        plan.append(("blk", b, y))
        y = draw_block(None, *b, y, BUS)
    y_presets = y
    y += 28 + 110 + LINE * nlines(PRESET_TEXT) + 14
    plan.append(("sec", ("How the reverb works", ALGO, True), y))
    y = section(None, y, "How the reverb works", ALGO, True)
    y_credits = y
    bottom = y_credits + 24 + LINE * len(CREDITS) + 20

    p = Patch()
    p.cnv(YEL_X, YEL_Y, YEL_W, bottom - YEL_Y)
    p.cnv(PINK_X, PINK_TOP, PINK_W, bottom - PINK_TOP, bg=PINK)
    p.cnv(BLUE_X, BLUE_TOP, BLUE_W, bottom - BLUE_TOP, bg=BLUE)
    p.heading(CX, y_title, OBJ, 22)
    p.heading(CX, y_title + 36, SUBTITLE, 16)

    # ---- demo row (as in vrs23~) ----
    t = p.add(f"#X obj {CX} {y_demo} tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1;")
    me = p.obj(CX, y_demo + 28, "metro 2000")
    hb = p.add(f"#X obj {CX + 6} {y_demo + 56} bng 19 250 50 0 empty empty empty 17 7 0 10 #fcfcfc #000000 #000000;")
    hs = p.obj(CX, y_demo + 86, f"s {DOLLAR}0-clap")
    p.conn(t, 0, me, 0)
    p.conn(me, 0, hb, 0)
    p.conn(hb, 0, hs, 0)
    p.comment(CX + 30, y_demo, ["<- clap every 2 s"])
    p.comment(CX + 30, y_demo + 56, ["<- play the clap"])
    util = ["dump", "defaults", "print", "clear", "preset ambient"]
    us = p.obj(CX + 400, y_demo + 86, f"s {BUS}")
    for i, u in enumerate(util):
        m = p.msg(CX + 160 + (i % 3) * 80, y_demo + 28 + (i // 3) * 30, u)
        p.conn(m, 0, us, 0)

    # ---- main controls ----
    for row in range(2):
        yy = y_main + row * 90
        snd = p.obj(CX + 410, yy + 24, f"s {BUS}")
        for c in range(4):
            nm = MAIN[row * 4 + c]
            a = p.add(f"#X floatatom {CX + c * 105} {yy} 4 0 127 2 {nm} {DOLLAR}0-n-{nm} - 0;")
            m = p.msg(CX + c * 105, yy + 24, f"{nm} {DOLLAR}1")
            p.conn(a, 0, m, 0)
            p.conn(m, 0, snd, 0)

    # ---- object area ----
    yo = y_objs
    # the reference clap, played at its own sample rate: soundfiler -> length in ms -> vline~
    p.comment(CX + 200, yo, ["reference: vrs23-clap.wav"])
    lb = p.obj(CX + 200, yo + 26, "loadbang")
    f0 = p.obj(CX + 280, yo + 26, f"f {DOLLAR}0")
    rd = p.msg(CX + 200, yo + 52, f"read -resize vrs23-clap.wav {DOLLAR}1-clap")
    sf = p.obj(CX + 200, yo + 78, "soundfiler")
    up = p.obj(CX + 280, yo + 104, "unpack f")
    pk = p.obj(CX + 200, yo + 130, "pack f f")
    ex = p.obj(CX + 200, yo + 156, f"expr {DOLLAR}f1 {SEMI} {DOLLAR}f1/{DOLLAR}f2*1000")
    pk2 = p.obj(CX + 200, yo + 182, "pack f f")
    p.obj(CX + 300, yo + 182, f"array define {DOLLAR}0-clap")
    p.conn(lb, 0, f0, 0)
    p.conn(f0, 0, rd, 0)
    p.conn(rd, 0, sf, 0)
    p.conn(sf, 0, pk, 0)
    p.conn(sf, 1, up, 0)
    p.conn(up, 0, pk, 1)
    p.conn(pk, 0, ex, 0)
    p.conn(ex, 0, pk2, 0)
    p.conn(ex, 1, pk2, 1)
    ld = p.msg(CX + 380, yo + 104, "dump")
    lds = p.obj(CX + 380, yo + 130, f"s {BUS}")
    p.conn(lb, 0, ld, 0)
    p.conn(ld, 0, lds, 0)

    rh = p.obj(CX, yo, f"r {DOLLAR}0-clap")
    li = p.obj(CX, yo + 26, "list")
    vm = p.msg(CX, yo + 52, f"0 {COMMA} {DOLLAR}1 {DOLLAR}2")
    vl = p.obj(CX, yo + 78, "vline~")
    tr = p.obj(CX, yo + 104, f"tabread4~ {DOLLAR}0-clap")
    p.conn(rh, 0, li, 0)
    p.conn(pk2, 0, li, 1)
    p.conn(li, 0, vm, 0)
    p.conn(vm, 0, vl, 0)
    p.conn(vl, 0, tr, 0)
    rb = p.obj(CX + 140, yo + 104, f"r {BUS}")
    sv = p.obj(CX, yo + 136, OBJ)
    p.conn(tr, 0, sv, 0)
    p.conn(rb, 0, sv, 2)
    gl = p.obj(CX, yo + 170, "*~ 0.3")
    gr = p.obj(CX + 60, yo + 170, "*~ 0.3")
    dry = p.obj(CX + 120, yo + 170, "*~ 0.3")
    dac = p.obj(CX, yo + 204, "dac~")
    p.conn(sv, 0, gl, 0)
    p.conn(sv, 1, gr, 0)
    p.conn(tr, 0, dry, 0)
    p.conn(gl, 0, dac, 0)
    p.conn(gr, 0, dac, 1)
    p.conn(dry, 0, dac, 0)
    p.conn(dry, 0, dac, 1)
    pr = p.obj(CX + 200, yo + 216, f"print {SHORT}")
    so = p.obj(CX + 200, yo + 242, f"s {BUS}-out")
    p.conn(sv, 2, pr, 0)
    p.conn(sv, 2, so, 0)
    numbers_subpatch(p, CX + 110, yo + 136)
    p.comment(TX, yo + 276, [
        "Right inlet not connected: mono in, stereo out. The clap",
        "also goes dry to the output (right *~), as a mixer does",
        "with a send.",
    ])

    # ---- sections and blocks ----
    for kind, args, yy in plan:
        if kind == "sec":
            section(p, yy, *args)
        else:
            draw_block(p, *args, yy, BUS)

    # ---- presets ----
    p.heading(CX, y_presets, "Presets", 14)
    ys = y_presets + 28
    hr = p.add(f"#X obj {CX} {ys} hradio 19 1 0 16 empty empty empty 0 -8 0 10 #fcfcfc #000000 #000000 0;")
    ro = p.obj(CX + 160, ys + 30, f"r {BUS}-out")
    sk = p.obj(CX, ys + 56, f"skpreset {SHORT}-slots")
    p.obj(CX + 240, ys + 56, f"text define -k {SHORT}-slots")
    sks = p.obj(CX, ys + 82, f"s {BUS}")
    p.conn(hr, 0, sk, 1)
    p.conn(ro, 0, sk, 0)
    p.conn(sk, 0, sks, 0)
    p.comment(TX, ys + 110, PRESET_TEXT, para=True)

    p.heading(CX, y_credits, "Credits", 14)
    p.comment(TX, y_credits + 24, CREDITS)

    head = f"#N canvas 348 20 820 {bottom + M} 12;"
    open(OUT, "w").write("\n".join([head] + p.lines + p.conns) + "\n")
    print("wrote", os.path.normpath(OUT))


if __name__ == "__main__":
    build()
