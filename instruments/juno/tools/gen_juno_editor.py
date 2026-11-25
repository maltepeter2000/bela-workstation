#!/usr/bin/env python3
"""Generates pd/junoedit.pd: an editor for juno~ in rows of 8, row k = CC page k of the
controller layer (pattern of d50edit.pd / txedit.pd / p8edit.pd).

usage: python3 tools/gen_juno_editor.py [outfile]   (default: pd/junoedit.pd)

[junoedit <bus>]  e.g. [junoedit $0-juno] in the help patch
  sends    n <name> <0-127>, preset / user / store ..., ninfo ...   to [s <bus>]
  listens  info / name / preset / user / channel ...                on [r <bus>-out]
Ranges, labels and rows come from tools/juno_params.py.
GUI on top (no wires: send / receive names), the logic in [pd logic].
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import ctlayer                                    # noqa: E402
import juno_params as T                            # noqa: E402
import style as S                                  # noqa: E402

BS = chr(92)
D, SP, SEMI, COMMA = BS + "$", BS + " ", BS + ";", BS + ","

CC = ctlayer.cc_map(T.CC_ROWS)                    # name -> CC number
ROWS = [(f"PAGE {k + 1}  {t}", list(ns)) for k, (t, ns) in enumerate(T.CC_ROWS)] + list(T.EXTRA_ROWS)
LEFT, RIGHT = ROWS, []
ALL = [n for _, ns in ROWS for n in ns if n]
assert len(ALL) == len(set(ALL)) == len(T.CONTROLS)

CW, ROWH, X0 = 80, 88, 10
BAND_Y = 44
BAND_H = 100
TOP = BAND_Y + BAND_H + 10
X1 = X0 + 8 * CW + 30
BAND = (S.YELLOW, S.ROW)
# The rows that are not stored in a patch get their own tint.
TINT = {"SETUP": S.SETUP, "VOICES": S.SETUP}
TITLE = "juno~ editor"            # tools/gen_juno_inst.py puts the object's name here


def label(nm):
    return T.CONTROLS[nm][4]


def cc_text(ns):
    ccs = [CC[n] for n in ns if n in CC]
    if not ccs:
        return "no CC (mouse only)"
    return "CC " + " ".join(map(str, ccs))        # every CC, never a range (see the pd-help-layout skill)


class Canvas:
    def __init__(self):
        self.lines, self.conns = [], []

    def add(self, t):
        self.lines.append(t)
        return len(self.lines) - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")


def bng(c, x, y, send, text):
    c.add(f"#X obj {x} {y} bng 17 250 50 0 {D}0-{send} empty {text.replace(' ', SP)} 21 9 0 10 {S.WIDGET} {S.INK} {S.INK};")


def top_band(c):
    """patch name, factory and user bank, MIDI channels"""
    y = BAND_Y
    c.add(f"#X obj {X0 - 4} {y} cnv 15 {8 * CW} {BAND_H} empty empty PATCH 4 8 0 10 {S.PATCH} {S.BLUE} 0;")
    c.add(f"#X text {X0} {y + 20} name;")
    c.add(f"#X symbolatom {X0 + 40} {y + 20} 20 0 0 0 - {D}0-namev -;")
    c.add(f"#X text {X0 + 190} {y + 20} factory;")
    c.add(f"#X floatatom {X0 + 240} {y + 20} 4 0 999 0 - {D}0-presetv {D}0-preset;")
    c.add(f"#X text {X0 + 290} {y + 20} user;")
    c.add(f"#X floatatom {X0 + 324} {y + 20} 3 0 55 0 - {D}0-userv {D}0-urecall;")
    bng(c, X0 + 370, y + 20, "reload", "reload")
    bng(c, X0 + 440, y + 20, "init", "init patch")
    bng(c, X0 + 8 * CW - 110, y + 20, "refresh", "refresh")
    c.add(f"#X text {X0} {y + 46} store to;")
    c.add(f"#X floatatom {X0 + 56} {y + 46} 3 0 55 0 - - {D}0-uslot;")
    bng(c, X0 + 90, y + 46, "usave", "save patch")
    bng(c, X0 + 186, y + 46, "uload", "load bank")
    bng(c, X0 + 276, y + 46, "uwrite", "write bank")
    bng(c, X0 + 370, y + 46, "usend", "send bank")
    bng(c, X0 + 460, y + 46, "psend", "send patch")
    c.add(f"#X text {X0} {y + 74} MIDI in;")
    c.add(f"#X floatatom {X0 + 52} {y + 74} 3 0 16 0 - {D}0-chv {D}0-ch;")
    c.add(f"#X text {X0 + 82} {y + 74} 0=all;")
    c.add(f"#X text {X0 + 130} {y + 74} out;")
    c.add(f"#X floatatom {X0 + 156} {y + 74} 3 1 16 0 - {D}0-txv {D}0-tx;")
    c.add(f"#X obj {X0 + 220} {y + 74} tgl 15 0 {D}0-sxo {D}0-sxov SysEx{SP}for{SP}every{SP}change 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")
    c.add(f"#X obj {X0 + 440} {y + 74} tgl 15 0 {D}0-thru {D}0-thruv MIDI{SP}thru 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")


def tint(title, r):
    for key, colours in TINT.items():
        if key in title:
            return colours[r % 2]
    return BAND[r % 2]


def gui(c):
    # title as in the help: a blue square with the name
    c.add(f"#X obj {X0} 14 cnv 10 10 10 empty empty {TITLE.replace(' ', SP)} 24 6 0 22 {S.BLUE} {S.INK} 0;")
    top_band(c)
    for x0, rows in ((X0, LEFT), (X1, RIGHT)):
        for r, (title, ns) in enumerate(rows):
            y = TOP + r * ROWH
            lbl = f"{title}  {cc_text(ns)}".replace(" ", SP).replace(":", "")
            c.add(f"#X obj {x0 - 4} {y} cnv 15 {8 * CW} {ROWH - 6} empty empty {lbl} 4 8 0 10 {tint(title, r)} {S.BLUE} 0;")
            for k, nm in enumerate(ns):
                if not nm:
                    continue
                x, yc = x0 + k * CW, y + 18
                c.add(f"#X text {x} {yc} {nm};")
                c.add(f"#X text {x} {yc + 12} {label(nm)};")
                c.add(f"#X obj {x} {yc + 30} hsl 70 12 0 127 0 0 {D}0-e-{nm} {D}0-v-{nm} empty -2 -8 0 10"
                      f" {S.WIDGET} {S.INK} {S.INK} 0 1;")
                c.add(f"#X floatatom {x} {yc + 46} 3 0 127 0 - {D}0-v-{nm} {D}0-e-{nm};")
                c.add(f"#X symbolatom {x + 26} {yc + 46} 7 0 0 0 - {D}0-t-{nm} -;")


def logic():
    """the [pd logic] subpatch: cells -> bus, patch handling, replies -> cells"""
    c = Canvas()
    c.add("#X text 10 10 cells -> n <name> <value> -> bus (juno~ answers with info on <bus>-out), f 60;")
    for i, nm in enumerate(ALL):
        col, row = i % 8, i // 8
        x, y = 10 + col * 150, 40 + row * 100
        r = c.add(f"#X obj {x} {y} r {D}0-e-{nm};")
        m = c.add(f"#X msg {x} {y + 30} n {nm} {D}1;")
        s = c.add(f"#X obj {x} {y + 60} s {D}1;")
        c.conn(r, 0, m, 0); c.conn(m, 0, s, 0)
    y = 40 + ((len(ALL) + 7) // 8) * 100 + 20

    def send(x, yy, recv, text):
        """[r $0-recv] -> [text( -> [s bus]"""
        r = c.add(f"#X obj {x} {yy} r {D}0-{recv};")
        m = c.add(f"#X msg {x} {yy + 30} {text};")
        s = c.add(f"#X obj {x} {yy + 60} s {D}1;")
        c.conn(r, 0, m, 0); c.conn(m, 0, s, 0)
        return m

    # start and refresh: the state, then all values
    lb = c.add(f"#X obj 10 {y} loadbang;")
    rr = c.add(f"#X obj 110 {y} r {D}0-refresh;")
    dl = c.add(f"#X obj 10 {y + 30} del 800;")
    c.conn(lb, 0, dl, 0); c.conn(rr, 0, dl, 0)
    allm = c.add(f"#X msg 10 {y + 60} status {COMMA} " + f" {COMMA} ".join(f"ninfo {n}" for n in ALL) + ", f 70;")
    c.conn(dl, 0, allm, 0)
    s2 = c.add(f"#X obj 10 {y + 330} s {D}1;")
    c.conn(allm, 0, s2, 0)

    # patch handling
    yp = y + 400
    send(10, yp, "preset", f"preset {D}1")
    send(130, yp, "urecall", f"user {D}1")
    send(250, yp, "reload", "reload")
    send(350, yp, "init", "init")
    send(450, yp, "uload", "userread")
    send(570, yp, "uwrite", "userwrite")
    send(690, yp, "usend", "usersend")
    send(810, yp, "psend", "sysexdump")
    send(930, yp, "ch", f"channel {D}1")
    send(1050, yp, "tx", f"txchannel {D}1")
    send(1170, yp, "sxo", f"sysexout {D}1")
    send(1290, yp, "thru", f"thru {D}1")
    yn = yp
    # store to N (only the slot) + save patch: store N, then write the bank
    ys = yn + 110
    rsl = c.add(f"#X obj 110 {ys} r {D}0-uslot;")
    rsv = c.add(f"#X obj 10 {ys} r {D}0-usave;")
    fsl = c.add(f"#X obj 10 {ys + 30} f;"); c.conn(rsv, 0, fsl, 0); c.conn(rsl, 0, fsl, 1)
    msv = c.add(f"#X msg 10 {ys + 60} store {D}1 {COMMA} userwrite;"); c.conn(fsl, 0, msv, 0)
    ssv = c.add(f"#X obj 10 {ys + 90} s {D}1;"); c.conn(msv, 0, ssv, 0)
    c.add(f"#X text 200 {ys + 60} store to N only keeps the slot number \\, save patch stores and writes the file, f 60;")

    # replies -> cells, names, numbers
    y2 = ys + 150
    ro = c.add(f"#X obj 10 {y2} r {D}1-out;")
    ri = c.add(f"#X obj 10 {y2 + 30} route info name preset user channel txchannel sysexout thru;"); c.conn(ro, 0, ri, 0)
    lp = c.add(f"#X obj 10 {y2 + 60} list prepend {D}0;")   # route info gives the list: name 0-127 text
    stm = c.add(f"#X msg 10 {y2 + 90} {SEMI} {D}1-v-{D}2 set {D}3 {SEMI} {D}1-t-{D}2 set {D}4;")
    c.conn(ri, 0, lp, 0); c.conn(lp, 0, stm, 0)
    c.add(f"#X text 10 {y2 + 130} info <name> <0-127> <value text> -> slider / number and text of the cell, f 50;")
    def show(x, outlet, text, dest):
        lst = c.add(f"#X obj {x} {y2 + 60} list;"); c.conn(ri, outlet, lst, 0)
        m = c.add(f"#X msg {x} {y2 + 90} {text};"); c.conn(lst, 0, m, 0)
        if dest:
            s = c.add(f"#X obj {x} {y2 + 120} s {D}0-{dest};"); c.conn(m, 0, s, 0)
        return m

    show(520, 1, f"set {D}1", "namev")
    show(640, 2, f"set {D}1", "presetv")
    show(760, 3, f"set {D}1", "userv")
    show(880, 4, f"set {D}1", "chv")
    show(1000, 5, f"set {D}1", "txv")
    show(1120, 6, f"set {D}1", "sxov")
    show(1240, 7, f"set {D}1", "thruv")
    return c


def build():
    c = Canvas()
    gui(c)
    lg = logic()
    rows = len(LEFT)
    sub = ["#N canvas 40 40 1320 900 logic 0;"] + lg.lines + lg.conns + [f"#X restore {X0} {TOP + rows * ROWH + 4} pd logic;"]
    h = TOP + rows * ROWH + 34
    return "\n".join([f"#N canvas 20 20 {X0 + 8 * CW + 10} {h} 10;"] + c.lines + sub + c.conns) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "pd", "junoedit.pd")
    open(out, "w").write(build())
    print("wrote", out, f"({len(ALL)} settings)")
