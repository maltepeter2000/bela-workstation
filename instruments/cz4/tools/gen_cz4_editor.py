#!/usr/bin/env python3
"""Generates pd/cz4edit.pd: an editor for cz4~ in rows of 8, row k = CC page k of the
controller layer (pattern of junoedit.pd / d50edit.pd / txedit.pd / p8edit.pd).

usage: python3 tools/gen_cz4_editor.py [outfile]   (default: pd/cz4edit.pd)

[cz4edit <bus>]  e.g. [cz4edit $0-cz4] in the help patch
  sends    n <name> <0-127>, preset / user / store / op ..., ninfo ...   to [s <bus>]
  listens  info / name / tonea / op / channel ...                       on [r <bus>-out]
Ranges, labels and rows come from tools/cz4_params.py.
GUI on top (no wires: send / receive names), the logic in [pd logic].
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import ctlayer                                    # noqa: E402
import cz4_params as T  # noqa: E402
import style as S                             # noqa: E402

BS = chr(92)
D, SP, SEMI, COMMA = BS + "$", BS + " ", BS + ";", BS + ","

CC = ctlayer.cc_map(T.CC_ROWS)                    # name -> CC number
ROWS = [(f"PAGE {k + 1}  {t}", list(ns)) for k, (t, ns) in enumerate(T.CC_ROWS)] + list(T.EXTRA_ROWS)
LEFT, RIGHT = ROWS, []
ALL = [n for _, ns in ROWS for n in ns if n]
assert len(ALL) == len(set(ALL)) == len(T.CONTROLS)

CW, ROWH, X0 = 80, 88, 10
BAND_Y = 44
BAND_H = 128
TOP = BAND_Y + BAND_H + 10
X1 = X0 + 8 * CW + 30
BAND = (S.YELLOW, S.ROW)
# Rows that are not stored in a tone (PERFORM, SETUP) and the operation memory get their own tint.
TINT = {"PERFORM": S.SETUP, "SETUP": S.SETUP, "OPERATION": S.OPERATION}
TITLE = "cz4~ editor"             # tools/gen_cz4_inst.py puts the object's name here


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
    """tone name, factory and user bank, operation memory, SysEx, MIDI channels"""
    y = BAND_Y
    c.add(f"#X obj {X0 - 4} {y} cnv 15 {8 * CW} {BAND_H} empty empty TONE 4 8 0 10 {S.PATCH} {S.BLUE} 0;")
    c.add(f"#X text {X0} {y + 20} name;")
    c.add(f"#X symbolatom {X0 + 40} {y + 20} 17 0 0 0 - {D}0-namev {D}0-name;")
    c.add(f"#X text {X0 + 170} {y + 20} factory;")
    c.add(f"#X floatatom {X0 + 220} {y + 20} 4 0 999 0 - {D}0-presetv {D}0-preset;")
    c.add(f"#X text {X0 + 262} {y + 20} user;")
    c.add(f"#X floatatom {X0 + 294} {y + 20} 3 0 63 0 - {D}0-userv {D}0-urecall;")
    bng(c, X0 + 330, y + 20, "reload", "reload")
    bng(c, X0 + 400, y + 20, "init", "init")
    c.add(f"#X obj {X0 + 450} {y + 20} tgl 17 0 {D}0-cmp {D}0-cmpv compare 21 9 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")
    bng(c, X0 + 8 * CW - 80, y + 20, "refresh", "refresh")
    c.add(f"#X text {X0} {y + 46} store to;")
    c.add(f"#X floatatom {X0 + 56} {y + 46} 3 0 63 0 - - {D}0-uslot;")
    bng(c, X0 + 90, y + 46, "usave", "save tone")
    bng(c, X0 + 180, y + 46, "uload", "load bank")
    bng(c, X0 + 270, y + 46, "uwrite", "write bank")
    bng(c, X0 + 364, y + 46, "usend", "send bank")
    bng(c, X0 + 454, y + 46, "psend", "send tone")
    c.add(f"#X text {X0} {y + 72} operation;")
    c.add(f"#X floatatom {X0 + 66} {y + 72} 3 -1 63 0 - {D}0-opv {D}0-op;")
    c.add(f"#X text {X0 + 100} {y + 72} store to;")
    c.add(f"#X floatatom {X0 + 156} {y + 72} 3 0 63 0 - - {D}0-oslot;")
    bng(c, X0 + 190, y + 72, "osave", "save operation")
    bng(c, X0 + 310, y + 72, "oinit", "init operation")
    c.add(f"#X text {X0} {y + 98} MIDI in;")
    c.add(f"#X floatatom {X0 + 52} {y + 98} 3 0 16 0 - {D}0-chv {D}0-ch;")
    c.add(f"#X text {X0 + 82} {y + 98} 0=all;")
    c.add(f"#X text {X0 + 130} {y + 98} out;")
    c.add(f"#X floatatom {X0 + 156} {y + 98} 3 1 16 0 - {D}0-txv {D}0-tx;")
    c.add(f"#X text {X0 + 200} {y + 98} CZ program;")
    c.add(f"#X floatatom {X0 + 270} {y + 98} 3 0 127 0 - - {D}0-czp;")
    bng(c, X0 + 306, y + 98, "czget", "get from CZ")
    bng(c, X0 + 400, y + 98, "czsend", "send to CZ")


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
    c.add("#X text 10 10 cells -> n <name> <value> -> bus (cz4~ answers with info on <bus>-out), f 60;")
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

    # tone handling
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
    send(1170, yp, "cmp", f"compare {D}1")
    send(10, yp + 100, "op", f"op {D}1")
    send(130, yp + 100, "oinit", "opinit")
    send(250, yp + 100, "name", f"name {D}1")
    yn = yp + 100
    # store to N (only the slot) + save: store N, then write the bank
    ys = yn + 110
    for x, slot, save, text in ((10, "uslot", "usave", f"store {D}1 {COMMA} userwrite"),
                                (400, "oslot", "osave", f"opstore {D}1 {COMMA} opwrite"),
                                (790, "czp", "czget", f"czget {D}1"),
                                (1000, "czp", "czsend", f"czsend {D}1")):
        rsl = c.add(f"#X obj {x + 100} {ys} r {D}0-{slot};")
        rsv = c.add(f"#X obj {x} {ys} r {D}0-{save};")
        fsl = c.add(f"#X obj {x} {ys + 30} f;"); c.conn(rsv, 0, fsl, 0); c.conn(rsl, 0, fsl, 1)
        msv = c.add(f"#X msg {x} {ys + 60} {text};"); c.conn(fsl, 0, msv, 0)
        ssv = c.add(f"#X obj {x} {ys + 90} s {D}1;"); c.conn(msv, 0, ssv, 0)
    c.add(f"#X text 10 {ys + 120} store to N only keeps the slot number \\, save stores and writes the file, f 60;")

    # replies -> cells, names, numbers
    y2 = ys + 150
    ro = c.add(f"#X obj 10 {y2} r {D}1-out;")
    ri = c.add(f"#X obj 10 {y2 + 30} route info name tonea op channel txchannel compare;"); c.conn(ro, 0, ri, 0)
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
    # tonea <preset|user|init|sysex> <n> <name>: the number in its box
    ta = c.add(f"#X obj 640 {y2 + 60} route preset user;"); c.conn(ri, 2, ta, 0)
    for k, dest in enumerate(("presetv", "userv")):
        lst = c.add(f"#X obj {640 + 90 * k} {y2 + 90} list split 1;"); c.conn(ta, k, lst, 0)
        m = c.add(f"#X msg {640 + 90 * k} {y2 + 120} set {D}1;"); c.conn(lst, 0, m, 0)
        s_ = c.add(f"#X obj {640 + 90 * k} {y2 + 150} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
    show(880, 3, f"set {D}1", "opv")
    show(1000, 4, f"set {D}1", "chv")
    show(1120, 5, f"set {D}1", "txv")
    show(1240, 6, f"set {D}1", "cmpv")
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
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "pd", "cz4edit.pd")
    open(out, "w").write(build())
    print("wrote", out, f"({len(ALL)} settings)")
