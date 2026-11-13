#!/usr/bin/env python3
"""Generates p8edit.pd: an editor for pdoly8~ in rows of 8, like the CC rows of the controller.

usage: python3 tools/gen_editor.py [outfile]   (default: p8edit.pd next to the external)

[p8edit <bus>]  e.g. [p8edit $0-pdoly8] in the help patch
  sends    n <name> <0-127>        to [s <bus>]      (the [r <bus>] in front of pdoly8~)
  listens  info <name> <0-127> <t> on [r <bus>-out]  (outlet 3 of pdoly8~ into [s <bus>-out])
  and to everything on <bus>: an n message updates its cell at once, any other setting,
  preset or defaults message refreshes all cells 30 ms later.
Every cell: name and CC number, label, slider and number box 0-127 (as from a pot), the value
with its unit from pdoly8~ (ninfo). Ranges and labels come from the RANGE table in pdoly8~.c,
the rows from the CC map in gen_bela.py, so editor, controller and help always agree.
GUI on top (no wires: send / receive names), the logic in [pd logic].
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import gen_bela as GB                             # noqa: E402  (CC_GROUPS, CC_EXTRA)

BS = chr(92)
D, SP, SEMI, COMMA = BS + "$", BS + " ", BS + ";", BS + ","

src = open(os.path.join(ROOT, "pdoly8~.c")).read()
RANGE = {m[0]: m[4] for m in re.findall(r'\{"(\w+)",\'(\w)\',([^,]+),([^,]+),"([^"]*)","([^"]*)"\}', src)}

CC = {nm: cc for cc, nm in GB.CCS.items()}
SHORT = {"pw1": "PW 1", "pw2": "PW 2", "fmig": "FM in gain", "dgld": "Delay change",
         "dfb": "Delay fback", "dmf": "Del mod rate", "dmi": "Del mod dpth", "drvp": "Drive pos",
         "sprd": "Stereo sprd", "mgf": "MG rate Hz"}


def short(nm):
    if nm in SHORT:
        return SHORT[nm]
    t = re.sub(r"(Env|LFO) ?(\d)", r"\1\2", RANGE[nm])
    t = t.replace("target", "targ").replace("amount", "amt").replace("trigger", "trig")
    assert len(t) <= 12, (nm, t)
    return t


def stages(g):
    return [f"{s}{g}" for s in ("a", "d", "bp", "sl", "su", "r")] + [None, None]


LEFT = [(t, ns) for t, c0, ns in GB.CC_GROUPS]
RIGHT = [("DEG4", ["env4", "trg4", "d4t1", "d4a1", "d4t2", "d4a2", None, None]),
         ("LFO2", ["l2r", "l2w", "l2k", "l2f", "l2t1", "l2a1", "l2t2", "l2a2"]),
         ("LFO3", ["l3r", "l3w", "l3k", "l3f", "l3t1", "l3a1", "l3t2", "l3a2"]),
         ("MORE", ["drvp", "tone", "leak", "mgf", "ch1", None, None, None]),
         ("DEG1 stages (sets env1 to own)", stages(1)),
         ("DEG2 stages (sets env2 to own)", stages(2)),
         ("DEG3 stages (sets env3 to own)", stages(3)),
         ("DEG4 stages (sets env4 to own)", stages(4))]
ALL = [n for _, ns in LEFT + RIGHT for n in ns if n]
assert len(ALL) == len(set(ALL)) and set(ALL) == set(RANGE), set(RANGE) ^ set(ALL)

CW, ROWH, TOP, X0 = 80, 88, 66, 10
X1 = X0 + 8 * CW + 30
BAND = ("#ececec", "#dcdcdc")


def cc_text(ns):
    ccs = [CC[n] for n in ns if n in CC]
    if not ccs:
        return "no CC"
    return f"CC {min(ccs)}-{max(ccs)}" if max(ccs) - min(ccs) + 1 == len(ccs) else "CC " + " ".join(map(str, ccs))


class Canvas:
    def __init__(self):
        self.lines, self.conns = [], []

    def add(self, t):
        self.lines.append(t)
        return len(self.lines) - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")


def gui(c):
    c.add(f"#X text {X0} 6 pdoly8~ editor: rows of 8 as on the controller (left = CC 20-83). Slider and number:"
          f" the value as from a pot (0-127) {COMMA} the box right of it: the real value., f 200;")
    c.add(f"#X text {X0} 22 [p8edit <bus>] sends n <name> <value> to <bus> and reads outlet 3 of pdoly8~ from"
          f" <bus>-out. Changes from MIDI CC {COMMA} presets and messages on the bus show up here., f 200;")
    c.add(f"#X obj {X1 + 8 * CW - 90} 40 bng 17 250 50 0 {D}0-refresh empty refresh 22 9 0 10 #fcfcfc #000000 #000000;")
    for col, (x0, rows) in enumerate(((X0, LEFT), (X1, RIGHT))):
        for r, (title, ns) in enumerate(rows):
            y = TOP + r * ROWH
            lbl = f"{title}  {cc_text(ns)}".replace(" ", SP)
            c.add(f"#X obj {x0 - 4} {y} cnv 15 {8 * CW} {ROWH - 6} empty empty {lbl} 4 8 0 10 {BAND[r % 2]} #404040 0;")
            for k, nm in enumerate(ns):
                if not nm:
                    continue
                x, yc = x0 + k * CW, y + 18
                head = f"{nm} {CC[nm]}" if nm in CC else nm
                c.add(f"#X text {x} {yc} {head};")
                c.add(f"#X text {x} {yc + 12} {short(nm).replace(',', ' ' + COMMA)};")
                c.add(f"#X obj {x} {yc + 30} hsl 70 12 0 127 0 0 {D}0-e-{nm} {D}0-v-{nm} empty -2 -8 0 10"
                      f" #fcfcfc #000000 #000000 0 1;")
                c.add(f"#X floatatom {x} {yc + 46} 3 0 127 0 - {D}0-v-{nm} {D}0-e-{nm};")
                c.add(f"#X symbolatom {x + 26} {yc + 46} 7 0 0 0 - {D}0-t-{nm} -;")


def logic():
    """the [pd logic] subpatch: cells -> bus, bus -> refresh, replies -> cells"""
    c = Canvas()
    c.add("#X text 10 10 cells -> n <name> <value> -> bus (pdoly8~ answers with info on <bus>-out), f 60;")
    for i, nm in enumerate(ALL):
        col, row = i % 8, i // 8
        x, y = 10 + col * 150, 40 + row * 100
        r = c.add(f"#X obj {x} {y} r {D}0-e-{nm};")
        m = c.add(f"#X msg {x} {y + 30} n {nm} {D}1;")
        s = c.add(f"#X obj {x} {y + 60} s {D}1;")
        c.conn(r, 0, m, 0); c.conn(m, 0, s, 0)
    y = 40 + ((len(ALL) + 7) // 8) * 100 + 20
    # bus -> refresh
    rb = c.add(f"#X obj 10 {y} r {D}1;")
    rt = c.add(f"#X obj 10 {y + 30} route n note bend ninfo nget disp info list float;")
    c.conn(rb, 0, rt, 0)
    ls = c.add(f"#X obj 10 {y + 60} list split 1;")      # "cut 30" (selector cut) -> cut
    c.conn(rt, 0, ls, 0)
    sy = c.add(f"#X obj 10 {y + 90} symbol;")
    c.conn(ls, 0, sy, 0)
    pp = c.add(f"#X obj 10 {y + 120} pipe s 0;")         # after pdoly8~ got the n message
    c.conn(sy, 0, pp, 0)
    one = c.add(f"#X msg 10 {y + 150} ninfo {D}1;")
    c.conn(pp, 0, one, 0)
    s1 = c.add(f"#X obj 10 {y + 180} s {D}1;")
    c.conn(one, 0, s1, 0)
    tb = c.add(f"#X obj 400 {y + 60} t b;")
    c.conn(rt, 9, tb, 0)
    lb = c.add(f"#X obj 500 {y} loadbang;")
    rr = c.add(f"#X obj 600 {y} r {D}0-refresh;")
    dl = c.add(f"#X obj 400 {y + 90} del 30;")
    c.conn(tb, 0, dl, 0); c.conn(lb, 0, dl, 0); c.conn(rr, 0, dl, 0)
    allm = c.add(f"#X msg 400 {y + 120} " + f" {COMMA} ".join(f"ninfo {n}" for n in ALL) + ", f 70;")
    c.conn(dl, 0, allm, 0)
    s2 = c.add(f"#X obj 400 {y + 330} s {D}1;")
    c.conn(allm, 0, s2, 0)
    c.add(f"#X text 120 {y + 60} n from anywhere: update that cell at once;")
    c.add(f"#X text 460 {y + 60} anything else (settings / presets / defaults): all cells;")
    # replies -> cells
    y2 = y + 380
    ro = c.add(f"#X obj 10 {y2} r {D}1-out;")
    ri = c.add(f"#X obj 10 {y2 + 30} route info;")
    lp = c.add(f"#X obj 10 {y2 + 60} list prepend {D}0;")   # route info gives the list: name 0-127 text
    st = c.add(f"#X msg 10 {y2 + 90} {SEMI} {D}1-v-{D}2 set {D}3 {SEMI} {D}1-t-{D}2 set {D}4;")
    c.conn(ro, 0, ri, 0); c.conn(ri, 0, lp, 0); c.conn(lp, 0, st, 0)
    c.add(f"#X text 200 {y2 + 30} info <name> <0-127> <value text> -> slider / number and text of the cell, f 50;")
    return c


def build():
    c = Canvas()
    gui(c)
    lg = logic()
    sub = [f"#N canvas 40 40 1260 900 logic 0;"] + lg.lines + lg.conns + [f"#X restore {X0} {TOP + 8 * ROWH + 4} pd logic;"]
    h = TOP + 8 * ROWH + 34
    return "\n".join([f"#N canvas 20 20 {X1 + 8 * CW + 10} {h} 10;"] + c.lines + sub + c.conns) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "p8edit.pd")
    open(out, "w").write(build())
    print("wrote", out, f"({len(ALL)} settings)")
