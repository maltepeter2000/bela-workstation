#!/usr/bin/env python3
"""Generates pd/epsedit.pd: an editor for eps~ in rows of 8, row k = CC page k of the
controller layer (pattern of junoedit.pd / d50edit.pd).

usage: python3 tools/gen_eps_editor.py [outfile]   (default: pd/epsedit.pd)

[epsedit <bus>]  e.g. [epsedit $0-eps]
  sends    n <name> <0-127>, preset / user / store ..., ninfo ...   to [s <bus>]
  listens  info / name / preset / user / channel ...                on [r <bus>-out]
Ranges, labels and rows come from tools/eps_params.py.
GUI on top (no wires: send / receive names), the logic in [pd logic].
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import ctlayer                                    # noqa: E402
import eps_params as T                             # noqa: E402
import eps_edit_params as EP                       # noqa: E402
import style as S                                  # noqa: E402

BS = chr(92)
D, SP, SEMI, COMMA = BS + "$", BS + " ", BS + ";", BS + ","

CC = ctlayer.cc_map(T.CC_ROWS)                    # name -> CC number
# ROWS: the controls of eps~ (offsets, CC pages); the help lists these. The instrument
# rows of eps_edit_params.py follow: left the instrument, layer and wavesample points,
# right the rest of the wavesample and its envelope.
ROWS = [(f"PAGE {k + 1}  {t}", list(ns)) for k, (t, ns) in enumerate(T.CC_ROWS)] + list(T.EXTRA_ROWS)
LEFT = ROWS + list(EP.ROWS_LEFT)
RIGHT = list(EP.ROWS_RIGHT)
ALL = [n for _, ns in ROWS for n in ns if n]
EDIT = [n for _, ns in EP.ROWS_LEFT + EP.ROWS_RIGHT for n in ns if n]
assert len(ALL) == len(set(ALL)) == len(T.CONTROLS)
assert not set(ALL) & set(EDIT)

CW, ROWH, X0 = 80, 88, 10
BAND_Y = 44
BAND_H = 124
TOP = BAND_Y + BAND_H + 10
X1 = X0 + 8 * CW + 30
BAND = (S.YELLOW, S.ROW)
TINT = {"INSTRUMENT": S.SETUP, "LAYER": S.FOLLOW, "WS ": S.FOLLOW, "ENV": S.FOLLOW}
TITLE = "eps~ editor"             # tools/gen_eps_inst.py puts the object's name here


def label(nm):
    return T.CONTROLS[nm][4] if nm in T.CONTROLS else EP.label(nm)


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
    lbl = text.replace(' ', SP) if text else "empty"
    c.add(f"#X obj {x} {y} bng 17 250 50 0 {D}0-{send} empty {lbl} 21 9 0 10 {S.WIDGET} {S.INK} {S.INK};")


def top_band(c):
    """instrument name, library folders, saving, MIDI channels"""
    y = BAND_Y
    c.add(f"#X obj {X0 - 4} {y} cnv 15 {8 * CW} {BAND_H} empty empty PATCH 4 8 0 10 {S.PATCH} {S.BLUE} 0;")
    c.add(f"#X text {X0} {y + 20} name;")
    c.add(f"#X symbolatom {X0 + 40} {y + 20} 20 0 0 0 - {D}0-namev -;")
    c.add(f"#X text {X0 + 190} {y + 20} library;")
    c.add(f"#X floatatom {X0 + 240} {y + 20} 4 0 9999 0 - {D}0-presetv {D}0-preset;")
    bng(c, X0 + 290, y + 20, "reload", "reload")
    bng(c, X0 + 360, y + 20, "init", "init")
    bng(c, X0 + 420, y + 20, "rescan", "rescan")
    bng(c, X0 + 566, y + 46, "libchoose", "choose")
    bng(c, X0 + 8 * CW - 110, y + 20, "refresh", "refresh")
    c.add(f"#X text {X0} {y + 46} folder;")
    c.add(f"#X symbolatom {X0 + 46} {y + 46} 40 0 0 0 - {D}0-folderv -;")
    bng(c, X0 + 300, y + 46, "bup", "up")
    c.add(f"#X text {X0 + 360} {y + 46} size;")
    c.add(f"#X floatatom {X0 + 390} {y + 46} 3 0 0 0 - {D}0-fcountv -;")
    c.add(f"#X text {X0 + 430} {y + 46} library size;")
    c.add(f"#X floatatom {X0 + 510} {y + 46} 5 0 0 0 - {D}0-countv -;")
    c.add(f"#X text {X0} {y + 72} file;")
    bng(c, X0 + 40, y + 72, "bprev", "")
    c.add(f"#X symbolatom {X0 + 62} {y + 72} 16 0 0 0 - {D}0-bselv -;")
    bng(c, X0 + 170, y + 72, "bnext", "")
    bng(c, X0 + 196, y + 72, "benter", "open")
    bng(c, X0 + 270, y + 72, "replace", "replace")
    bng(c, X0 + 360, y + 72, "savecopy", "save copy")
    c.add(f"#X text {X0} {y + 98} MIDI in;")
    c.add(f"#X floatatom {X0 + 52} {y + 98} 3 0 16 0 - {D}0-chv {D}0-ch;")
    c.add(f"#X text {X0 + 82} {y + 98} 0=all;")
    c.add(f"#X text {X0 + 124} {y + 98} out;")
    c.add(f"#X floatatom {X0 + 148} {y + 98} 3 1 16 0 - {D}0-txv {D}0-tx;")
    c.add(f"#X obj {X0 + 190} {y + 98} tgl 15 0 {D}0-thru {D}0-thruv thru 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")
    c.add(f"#X text {X0 + 270} {y + 98} new name;")
    c.add(f"#X symbolatom {X0 + 330} {y + 98} 14 0 0 0 - - {D}0-newname;")
    bng(c, X0 + 432, y + 98, "saveas", "save as")
    bng(c, X0 + 520, y + 98, "mkdir", "new folder")
    select_band(c)


def select_band(c):
    """which layer, wavesample and envelope the LAYER / WS / ENV rows show"""
    x1, y = X1, BAND_Y
    c.add(f"#X obj {x1 - 4} {y} cnv 15 {8 * CW} {BAND_H} empty empty SELECT 4 8 0 10 {S.SELECT} {S.BLUE} 0;")
    c.add(f"#X text {x1} {y + 20} layer;")
    bng(c, x1 + 40, y + 20, "lprev", "")
    c.add(f"#X floatatom {x1 + 62} {y + 20} 2 1 8 0 - {D}0-lselv {D}0-lsel;")
    bng(c, x1 + 86, y + 20, "lnext", "")
    c.add(f"#X text {x1 + 108} {y + 20} of;")
    c.add(f"#X floatatom {x1 + 124} {y + 20} 2 0 0 0 - {D}0-lcountv -;")
    c.add(f"#X symbolatom {x1 + 150} {y + 20} 14 0 0 0 - {D}0-lnamev -;")
    c.add(f"#X text {x1 + 300} {y + 20} wavesample;")
    bng(c, x1 + 372, y + 20, "wprev", "")
    c.add(f"#X floatatom {x1 + 394} {y + 20} 2 1 127 0 - {D}0-wselv {D}0-wsel;")
    bng(c, x1 + 418, y + 20, "wnext", "")
    c.add(f"#X text {x1 + 440} {y + 20} of;")
    c.add(f"#X floatatom {x1 + 456} {y + 20} 2 0 0 0 - {D}0-wcountv -;")
    c.add(f"#X symbolatom {x1 + 482} {y + 20} 14 0 0 0 - {D}0-wnamev -;")
    c.add(f"#X symbolatom {x1} {y + 46} 70 0 0 0 - {D}0-winfov -;")
    c.add(f"#X text {x1} {y + 74} envelope;")
    c.add(f"#X obj {x1 + 60} {y + 74} hradio 15 1 0 3 {D}0-esel {D}0-eselv empty 0 -8 0 10 {S.WIDGET} {S.INK} {S.INK} 2;")
    c.add(f"#X text {x1 + 112} {y + 74} 1 pitch 2 filter 3 amp;")
    c.add(f"#X obj {x1 + 290} {y + 74} tgl 15 0 {D}0-follow {D}0-followv follow{SP}key 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 1 1;")
    c.add(f"#X obj {x1 + 400} {y + 74} tgl 15 0 {D}0-zerox {D}0-zeroxv zero{SP}crossings 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 1 1;")


SAMPLE_H = 100
DESTS = "new WS / replace WS / new layer / new instrument"


def sample_band(c, y):
    """the wave recorder: rate, threshold, pre-trigger, where the recording goes, the keys"""
    x0 = X0
    c.add(f"#X obj {x0 - 4} {y} cnv 15 {8 * CW} {SAMPLE_H - 6} empty empty SAMPLE{SP}(audio{SP}into{SP}inlet{SP}2) 4 8 0 10 {S.SELECT} {S.BLUE} 0;")
    c.add(f"#X text {x0} {y + 20} rate;")
    bng(c, x0 + 32, y + 20, "rprev", "")
    c.add(f"#X symbolatom {x0 + 54} {y + 20} 7 0 0 0 - {D}0-ratev -;")
    bng(c, x0 + 112, y + 20, "rnext", "")
    c.add(f"#X text {x0 + 140} {y + 20} threshold;")
    c.add(f"#X floatatom {x0 + 200} {y + 20} 2 0 19 0 - {D}0-threshv {D}0-rthresh;")
    c.add(f"#X text {x0 + 226} {y + 20} pre ms;")
    c.add(f"#X floatatom {x0 + 270} {y + 20} 3 0 127 0 - {D}0-prev {D}0-rpre;")
    c.add(f"#X text {x0 + 306} {y + 20} into;")
    c.add(f"#X obj {x0 + 336} {y + 20} hradio 15 1 0 4 {D}0-rdest {D}0-destv empty 0 -8 0 10 {S.WIDGET} {S.INK} {S.INK} 0;")
    c.add(f"#X symbolatom {x0 + 400} {y + 20} 11 0 0 0 - {D}0-destnv -;")
    c.add(f"#X text {x0 + 480} {y + 20} input;")
    c.add(f"#X obj {x0 + 514} {y + 20} hradio 15 1 0 4 {D}0-rinput {D}0-inputv empty 0 -8 0 10 {S.WIDGET} {S.INK} {S.INK} 1;")
    c.add(f"#X symbolatom {x0 + 576} {y + 20} 6 0 0 0 - {D}0-inputnv -;")
    c.add(f"#X obj {x0} {y + 46} tgl 15 0 {D}0-rlevel {D}0-rlevelv level 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")
    bng(c, x0 + 60, y + 46, "rstart", "rec")
    bng(c, x0 + 110, y + 46, "rstop", "stop")
    c.add(f"#X symbolatom {x0 + 164} {y + 46} 20 0 0 0 - {D}0-meterv -;")
    c.add(f"#X symbolatom {x0 + 300} {y + 46} 9 0 0 0 - {D}0-statev -;")
    c.add(f"#X symbolatom {x0 + 366} {y + 46} 30 0 0 0 - {D}0-recinfov -;")
    c.add(f"#X text {x0} {y + 72} play a key for the selected wavesample:;")
    bng(c, x0 + 256, y + 72, "lroot", "root key")
    bng(c, x0 + 346, y + 72, "llo", "low key")
    bng(c, x0 + 430, y + 72, "lhi", "high key")
    c.add(f"#X obj {x0 + 530} {y + 72} tgl 15 0 {D}0-rmon {D}0-rmonv monitor 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")


def command_band(c, y):
    """the wavesample commands of the EPS (COMMAND/Wave, LFO, Amp) with KEEP=OLD NEW"""
    x0 = X1
    c.add(f"#X obj {x0 - 4} {y} cnv 15 {8 * CW} {SAMPLE_H - 6} empty empty COMMAND{SP}(selected{SP}wavesample) 4 8 0 10 {S.SELECT} {S.BLUE} 0;")
    c.add(f"#X text {x0} {y + 20} command;")
    bng(c, x0 + 50, y + 20, "cprev", "")
    c.add(f"#X symbolatom {x0 + 72} {y + 20} 14 0 0 0 - {D}0-cmdv -;")
    bng(c, x0 + 166, y + 20, "cnext", "")
    c.add(f"#X text {x0 + 196} {y + 20} range;")
    c.add(f"#X obj {x0 + 232} {y + 20} hradio 15 1 0 4 {D}0-crange {D}0-crangev empty 0 -8 0 10 {S.WIDGET} {S.INK} {S.INK} 0;")
    c.add(f"#X symbolatom {x0 + 296} {y + 20} 10 0 0 0 - {D}0-crangenv -;")
    bng(c, x0 + 380, y + 20, "cdo", "do")
    bng(c, x0 + 420, y + 20, "cold", "old")
    bng(c, x0 + 466, y + 20, "cnew", "new")
    bng(c, x0 + 512, y + 20, "ckeep", "keep")
    bng(c, x0 + 562, y + 20, "ccancel", "cancel")
    c.add(f"#X text {x0} {y + 46} depth;")
    c.add(f"#X floatatom {x0 + 34} {y + 46} 1 0 5 0 - {D}0-cdepthv {D}0-cdepth;")
    c.add(f"#X symbolatom {x0 + 50} {y + 46} 5 0 0 0 - {D}0-cdepthnv -;")
    c.add(f"#X text {x0 + 92} {y + 46} zone %;")
    c.add(f"#X floatatom {x0 + 134} {y + 46} 2 1 99 0 - {D}0-czonev {D}0-czone;")
    c.add(f"#X text {x0 + 164} {y + 46} scale %;")
    c.add(f"#X floatatom {x0 + 212} {y + 46} 3 0 200 0 - {D}0-csstartv {D}0-csstart;")
    c.add(f"#X floatatom {x0 + 240} {y + 46} 3 0 200 0 - {D}0-csendv {D}0-csend;")
    c.add(f"#X text {x0 + 276} {y + 46} rate;")
    c.add(f"#X floatatom {x0 + 304} {y + 46} 1 0 6 0 - {D}0-cratev {D}0-crate;")
    c.add(f"#X symbolatom {x0 + 320} {y + 46} 7 0 0 0 - {D}0-cratenv -;")
    c.add(f"#X text {x0 + 376} {y + 46} smooth;")
    c.add(f"#X floatatom {x0 + 418} {y + 46} 1 0 4 0 - {D}0-csmoothv {D}0-csmooth;")
    c.add(f"#X symbolatom {x0 + 434} {y + 46} 11 0 0 0 - {D}0-csmoothnv -;")
    c.add(f"#X text {x0 + 520} {y + 46} balance;")
    c.add(f"#X floatatom {x0 + 572} {y + 46} 4 -127 127 0 - {D}0-cbalancev {D}0-cbalance;")
    c.add(f"#X text {x0} {y + 72} copy to layer;")
    c.add(f"#X floatatom {x0 + 84} {y + 72} 1 1 8 0 - {D}0-clayerv {D}0-clayer;")
    c.add(f"#X obj {x0 + 104} {y + 72} tgl 15 0 {D}0-cdata {D}0-cdatav with{SP}data 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")
    c.add(f"#X text {x0 + 196} {y + 72} to WS;")
    c.add(f"#X floatatom {x0 + 232} {y + 72} 3 0 127 0 - {D}0-cwsv {D}0-cws;")
    c.add(f"#X symbolatom {x0 + 264} {y + 72} 5 0 0 0 - {D}0-cstatev -;")
    c.add(f"#X symbolatom {x0 + 304} {y + 72} 28 0 0 0 - {D}0-cinfov -;")
    c.add(f"#X obj {x0 + 520} {y + 72} tgl 15 0 {D}0-cclip {D}0-cclipv clip{SP}prev. 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")


FX_H = 110


def fx_band(c, y):
    """the effect of the instrument: name, on, variation, pages of 8 parameters (named by it)"""
    x0 = X0
    c.add(f"#X obj {x0 - 4} {y} cnv 15 {8 * CW} {FX_H - 6} empty empty EFFECT{SP}(ROM{SP}and{SP}OS{SP}of{SP}the{SP}EPS{SP}in{SP}the{SP}library){SP}{SP}<{SP}>{SP}choose{SP}an{SP}effect{SP}then{SP}load 4 8 0 10 {S.SETUP[0]} {S.BLUE} 0;")
    # the effect: < name > to choose (ROM, effect files), load; on; variation; page
    bng(c, x0, y + 20, "fxsprev", "")
    c.add(f"#X symbolatom {x0 + 20} {y + 20} 14 0 0 0 - {D}0-fxnamev -;")
    bng(c, x0 + 124, y + 20, "fxsnext", "")
    bng(c, x0 + 146, y + 20, "fxsload", "load")
    c.add(f"#X obj {x0 + 200} {y + 21} tgl 15 0 {D}0-fxon {D}0-fxonv on 19 8 0 10 {S.WIDGET} {S.INK} {S.INK} 1 1;")
    c.add(f"#X text {x0 + 240} {y + 20} var;")
    bng(c, x0 + 262, y + 20, "fxvprev", "")
    c.add(f"#X symbolatom {x0 + 282} {y + 20} 16 0 0 0 - {D}0-fxvarv -;")
    bng(c, x0 + 398, y + 20, "fxvnext", "")
    c.add(f"#X text {x0 + 424} {y + 20} page;")
    bng(c, x0 + 456, y + 20, "fxpprev", "")
    c.add(f"#X symbolatom {x0 + 476} {y + 20} 4 0 0 0 - {D}0-fxpagev -;")
    bng(c, x0 + 514, y + 20, "fxpnext", "")
    for k in range(8):
        x = x0 + k * CW
        c.add(f"#X symbolatom {x} {y + 46} 10 0 0 0 - {D}0-fxn{k} -;")
        c.add(f"#X obj {x} {y + 66} hsl 70 12 0 127 0 0 {D}0-fxe{k} {D}0-fxv{k} empty -2 -8 0 10 {S.WIDGET} {S.INK} {S.INK} 0 1;")
        c.add(f"#X symbolatom {x} {y + 82} 10 0 0 0 - {D}0-fxt{k} -;")


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
    sample_band(c, TOP + len(LEFT) * ROWH)
    command_band(c, TOP + len(RIGHT) * ROWH)
    fx_band(c, TOP + max(len(LEFT), len(RIGHT)) * ROWH + SAMPLE_H)


def logic():
    """the [pd logic] subpatch: cells -> bus, patch handling, replies -> cells"""
    c = Canvas()
    c.add("#X text 10 10 cells -> n <name> <value> -> bus (eps~ answers with info on <bus>-out), f 60;")
    for i, nm in enumerate(ALL):
        col, row = i % 8, i // 8
        x, y = 10 + col * 150, 40 + row * 100
        r = c.add(f"#X obj {x} {y} r {D}0-e-{nm};")
        m = c.add(f"#X msg {x} {y + 30} n {nm} {D}1;")
        s = c.add(f"#X obj {x} {y + 60} s {D}1;")
        c.conn(r, 0, m, 0); c.conn(m, 0, s, 0)
    for i, nm in enumerate(EDIT):
        col, row = i % 8, i // 8 + (len(ALL) + 7) // 8
        x, y = 10 + col * 150, 40 + row * 100
        r = c.add(f"#X obj {x} {y} r {D}0-e-{nm};")
        m = c.add(f"#X msg {x} {y + 30} e {nm} {D}1;")
        s = c.add(f"#X obj {x} {y + 60} s {D}1;")
        c.conn(r, 0, m, 0); c.conn(m, 0, s, 0)
    y = 40 + ((len(ALL) + 7) // 8 + (len(EDIT) + 7) // 8) * 100 + 20

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
    allm = c.add(f"#X msg 10 {y + 60} status {COMMA} " + f" {COMMA} ".join(f"ninfo {n}" for n in ALL) + f" {COMMA} edump {COMMA} browse {COMMA} rec status {COMMA} cmd status {COMMA} fx, f 70;")
    c.conn(dl, 0, allm, 0)
    s2 = c.add(f"#X obj 10 {y + 330} s {D}1;")
    c.conn(allm, 0, s2, 0)

    # patch handling
    yp = y + 400
    send(10, yp, "preset", f"preset {D}1")
    send(130, yp, "bup", "bup")
    send(250, yp, "reload", "reload")
    send(350, yp, "init", "init")
    send(450, yp, "bprev", "bstep -1")
    send(570, yp, "bnext", "bstep 1")
    send(690, yp, "rescan", "rescan")
    send(930, yp, "ch", f"channel {D}1")
    send(1050, yp, "tx", f"txchannel {D}1")
    send(1290, yp, "thru", f"thru {D}1")
    # selection
    ye = yp + 110
    send(10, ye, "lsel", f"sel layer {D}1")
    send(130, ye, "lprev", "sel layerstep -1")
    send(250, ye, "lnext", "sel layerstep 1")
    send(370, ye, "wsel", f"sel ws {D}1")
    send(490, ye, "wprev", "sel wsstep -1")
    send(610, ye, "wnext", "sel wsstep 1")
    re_ = c.add(f"#X obj 730 {ye} r {D}0-esel;")
    pe = c.add(f"#X obj 730 {ye + 30} + 1;"); c.conn(re_, 0, pe, 0)
    me = c.add(f"#X msg 730 {ye + 60} sel env {D}1;"); c.conn(pe, 0, me, 0)
    se = c.add(f"#X obj 730 {ye + 90} s {D}1;"); c.conn(me, 0, se, 0)
    send(850, ye, "follow", f"follow {D}1")
    send(970, ye, "zerox", f"zerox {D}1")
    # the wave recorder
    yr = ye + 110
    for k, (recv, text) in enumerate([("rprev", "rec ratestep -1"), ("rnext", "rec ratestep 1"),
                                      ("rthresh", f"rec thresh {D}1"), ("rpre", f"rec pre {D}1"),
                                      ("rdest", f"rec dest {D}1"), ("rlevel", f"rec level {D}1"),
                                      ("rinput", f"rec input {D}1"), ("rmon", f"rec monitor {D}1"),
                                      ("rstart", "rec start"), ("rstop", "rec stop"),
                                      ("lroot", "rec learn root"), ("llo", "rec learn lo"),
                                      ("lhi", "rec learn hi")]):
        send(10 + 120 * k, yr, recv, text)
    # the wavesample commands
    yc = yr + 110
    for k, (recv, text) in enumerate([("cprev", "cmd step -1"), ("cnext", "cmd step 1"),
                                      ("crange", f"cmd range {D}1"), ("cdepth", f"cmd depth {D}1"),
                                      ("czone", f"cmd zone {D}1"), ("csstart", f"cmd sstart {D}1"),
                                      ("csend", f"cmd send {D}1"), ("crate", f"cmd rate {D}1"),
                                      ("csmooth", f"cmd smooth {D}1"), ("clayer", f"cmd layer {D}1"),
                                      ("cdata", f"cmd data {D}1"), ("cdo", "cmd do"), ("cold", "cmd old"),
                                      ("cnew", "cmd new"), ("ckeep", "cmd keep"), ("ccancel", "cmd cancel"),
                                      ("cws", f"cmd ws {D}1"), ("cbalance", f"cmd balance {D}1"),
                                      ("cclip", f"cmd clip {D}1")]):
        send(10 + 120 * (k % 11), yc + 110 * (k // 11), recv, text)
    yp = yc + 110
    yn = yp
    # saving: replace, save copy (a free name), save as / new folder with the name typed
    ys = yn + 110
    send(10, ys, "benter", "benter")
    send(130, ys, "replace", "replace")
    send(250, ys, "savecopy", "save")
    rnn = c.add(f"#X obj 400 {ys} r {D}0-newname;")
    for k, (recv, msg) in enumerate([("saveas", "saveas"), ("mkdir", "mkdir")]):
        x = 520 + 120 * k
        rb = c.add(f"#X obj {x} {ys} r {D}0-{recv};")
        sy = c.add(f"#X obj {x} {ys + 30} symbol;"); c.conn(rb, 0, sy, 0); c.conn(rnn, 0, sy, 1)
        mm = c.add(f"#X msg {x} {ys + 60} {msg} {D}1;"); c.conn(sy, 0, mm, 0)
        ss = c.add(f"#X obj {x} {ys + 90} s {D}1;"); c.conn(mm, 0, ss, 0)
    c.add(f"#X text 760 {ys + 30} new name: '_' = space \\, 12 characters A-Z 0-9 * - +, f 40;")
    # the library folder: a folder dialog -> libset (eps.i~ keeps it in eps-library.txt)
    rlc = c.add(f"#X obj 10 {ys + 110} r {D}0-libchoose;")
    opl = c.add(f"#X obj 10 {ys + 140} openpanel 1;"); c.conn(rlc, 0, opl, 0)
    mls = c.add(f"#X msg 10 {ys + 170} libset {D}1;"); c.conn(opl, 0, mls, 0)
    sls = c.add(f"#X obj 10 {ys + 200} s {D}1;"); c.conn(mls, 0, sls, 0)
    c.add(f"#X text 130 {ys + 140} choose: the library folder \\, kept beside the patch in eps-library.txt, f 40;")

    # replies -> cells, names, numbers
    y2 = ys + 240
    ro = c.add(f"#X obj 10 {y2} r {D}1-out;")
    ri = c.add(f"#X obj 10 {y2 + 30} route info name preset folder channel txchannel thru presets sel editmode bsel;"); c.conn(ro, 0, ri, 0)
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
    show(760, 3, f"set {D}1", "folderv")
    lfc = c.add(f"#X obj 760 {y2 + 150} list;"); c.conn(ri, 3, lfc, 0)
    mfc = c.add(f"#X msg 760 {y2 + 180} set {D}2;"); c.conn(lfc, 0, mfc, 0)
    sfc = c.add(f"#X obj 760 {y2 + 210} s {D}0-fcountv;"); c.conn(mfc, 0, sfc, 0)
    show(880, 4, f"set {D}1", "chv")
    show(1000, 5, f"set {D}1", "txv")
    show(1120, 6, f"set {D}1", "thruv")
    show(1240, 7, f"set {D}1", "countv")
    # sel <layer> <ws> <env> <layers> <layer name> <ws name> <info> -> the SELECT bar
    ys2 = y2 + 180
    lsl = c.add(f"#X obj 10 {ys2} list;"); c.conn(ri, 8, lsl, 0)
    usl = c.add(f"#X obj 10 {ys2 + 30} unpack f f f f s s s f;"); c.conn(lsl, 0, usl, 0)
    for k, dest in [(0, "lselv"), (1, "wselv"), (3, "lcountv"), (4, "lnamev"), (5, "wnamev"), (6, "winfov"), (7, "wcountv")]:
        x = 10 + 70 * k
        m = c.add(f"#X msg {x} {ys2 + 60} set {D}1;"); c.conn(usl, k, m, 0)
        s = c.add(f"#X obj {x} {ys2 + 90 + 15 * (k % 2)} s {D}0-{dest};"); c.conn(m, 0, s, 0)
    ue = c.add(f"#X obj 500 {ys2 + 30} unpack f f f;"); c.conn(lsl, 0, ue, 0)
    ee = c.add(f"#X obj 640 {ys2 + 60} - 1;"); c.conn(ue, 2, ee, 0)
    me = c.add(f"#X msg 640 {ys2 + 90} set {D}1;"); c.conn(ee, 0, me, 0)
    se = c.add(f"#X obj 640 {ys2 + 120} s {D}0-eselv;"); c.conn(me, 0, se, 0)
    # the recorder's replies -> the SAMPLE band
    yq = ys2 + 150
    rq = c.add(f"#X obj 10 {yq} route recset recstate recinfo recmeter;"); c.conn(ro, 0, rq, 0)
    rs = c.add(f"#X obj 10 {yq + 30} route rate thresh pre dest input monitor;"); c.conn(rq, 0, rs, 0)
    for k, (outlet, text, dest) in enumerate([(0, f"set {D}2", "ratev"), (1, f"set {D}1", "threshv"),
                                              (2, f"set {D}1", "prev"), (3, f"set {D}1", "destv"),
                                              (3, f"set {D}2", "destnv"), (4, f"set {D}1", "inputv"),
                                              (4, f"set {D}2", "inputnv"), (5, f"set {D}1", "rmonv")]):
        x = 10 + 110 * k
        ll = c.add(f"#X obj {x} {yq + 60} list;"); c.conn(rs, outlet, ll, 0)
        m = c.add(f"#X msg {x} {yq + 90} {text};"); c.conn(ll, 0, m, 0)
        s_ = c.add(f"#X obj {x} {yq + 120} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
    lists = {}
    for k, (outlet, dest) in enumerate([(1, "statev"), (2, "recinfov"), (3, "meterv")]):
        x = 600 + 110 * k
        ll = c.add(f"#X obj {x} {yq + 30} list;"); c.conn(rq, outlet, ll, 0)   # a word -> symbol
        lists[outlet] = ll
        m = c.add(f"#X msg {x} {yq + 60} set {D}1;"); c.conn(ll, 0, m, 0)
        s_ = c.add(f"#X obj {x} {yq + 90} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
    sl = c.add(f"#X obj 930 {yq + 30} sel level;"); c.conn(lists[1], 0, sl, 0)
    m1 = c.add(f"#X msg 930 {yq + 60} set 1;"); c.conn(sl, 0, m1, 0)
    m0 = c.add(f"#X msg 1000 {yq + 60} set 0;"); c.conn(sl, 1, m0, 0)
    sv = c.add(f"#X obj 930 {yq + 90} s {D}0-rlevelv;"); c.conn(m1, 0, sv, 0); c.conn(m0, 0, sv, 0)
    # the command replies -> the COMMAND band
    yk = yq + 180
    rk = c.add(f"#X obj 10 {yk} route cmdset cmdstate cmdinfo;"); c.conn(ro, 0, rk, 0)
    keys = ["cmd", "range", "depth", "zone", "sstart", "send", "rate", "smooth", "layer", "data", "ws",
            "balance", "clip"]
    rks = c.add(f"#X obj 10 {yk + 30} route {' '.join(keys)};"); c.conn(rk, 0, rks, 0)
    dests = [("cmd", [(f"set {D}2", "cmdv")]), ("range", [(f"set {D}1", "crangev"), (f"set {D}2", "crangenv")]),
             ("depth", [(f"set {D}1", "cdepthv"), (f"set {D}2", "cdepthnv")]), ("zone", [(f"set {D}1", "czonev")]),
             ("sstart", [(f"set {D}1", "csstartv")]), ("send", [(f"set {D}1", "csendv")]),
             ("rate", [(f"set {D}1", "cratev"), (f"set {D}2", "cratenv")]),
             ("smooth", [(f"set {D}1", "csmoothv"), (f"set {D}2", "csmoothnv")]),
             ("layer", [(f"set {D}1", "clayerv")]), ("data", [(f"set {D}1", "cdatav")]),
             ("ws", [(f"set {D}1", "cwsv")]), ("balance", [(f"set {D}1", "cbalancev")]),
             ("clip", [(f"set {D}1", "cclipv")])]
    col = 0
    for k, (_, outs) in enumerate(dests):
        ll = c.add(f"#X obj {10 + 110 * col} {yk + 60} list;"); c.conn(rks, k, ll, 0)
        for text, dest in outs:
            x = 10 + 110 * col
            m = c.add(f"#X msg {x} {yk + 90} {text};"); c.conn(ll, 0, m, 0)
            s_ = c.add(f"#X obj {x} {yk + 120} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
            col += 1
    for k, (outlet, dest) in enumerate([(1, "cstatev"), (2, "cinfov")]):
        x = 10 + 110 * (col + k)
        ll = c.add(f"#X obj {x} {yk + 30} list;"); c.conn(rk, outlet, ll, 0)
        m = c.add(f"#X msg {x} {yk + 60} set {D}1;"); c.conn(ll, 0, m, 0)
        s_ = c.add(f"#X obj {x} {yk + 90} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
    # the effect: editor -> eps~, and fxpage / fxcell replies -> the EFFECT band
    yf = yk + 160
    for k, (recv, text) in enumerate([("fxon", f"fx {D}1"), ("fxvprev", "fxvar -1"), ("fxvnext", "fxvar 1"),
                                      ("fxpprev", "fxpage -1"), ("fxpnext", "fxpage 1"),
                                      ("fxsprev", "fxsel -1"), ("fxsnext", "fxsel 1"), ("fxsload", "fxload")]):
        send(10 + 120 * k, yf, recv, text)
    for k in range(8):
        send(10 + 120 * k, yf + 100, f"fxe{k}", f"fxcell {k} {D}1")
    yf2 = yf + 200
    rf = c.add(f"#X obj 10 {yf2} route fxpage fxcell fxpick;"); c.conn(ro, 0, rf, 0)
    upk = c.add(f"#X obj 1300 {yf2 + 30} unpack s s;"); c.conn(rf, 2, upk, 0)
    mpk = c.add(f"#X msg 1300 {yf2 + 60} set {D}1;"); c.conn(upk, 0, mpk, 0)
    spk = c.add(f"#X obj 1300 {yf2 + 90} s {D}0-fxnamev;"); c.conn(mpk, 0, spk, 0)
    upf = c.add(f"#X obj 10 {yf2 + 30} unpack s s s f;"); c.conn(rf, 0, upf, 0)
    for k, dest in enumerate(["fxnamev", "fxpagev", "fxvarv", "fxonv"]):
        m = c.add(f"#X msg {10 + 110 * k} {yf2 + 60} set {D}1;"); c.conn(upf, k, m, 0)
        s_ = c.add(f"#X obj {10 + 110 * k} {yf2 + 90} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
    rc = c.add(f"#X obj 500 {yf2 + 30} route 0 1 2 3 4 5 6 7;"); c.conn(rf, 1, rc, 0)
    for k in range(8):
        x = 500 + 100 * k
        u = c.add(f"#X obj {x} {yf2 + 60} unpack s f s;"); c.conn(rc, k, u, 0)
        for j, dest in enumerate([f"fxn{k}", f"fxv{k}", f"fxt{k}"]):
            m = c.add(f"#X msg {x} {yf2 + 90 + 30 * j} set {D}1;"); c.conn(u, j, m, 0)
            s_ = c.add(f"#X obj {x + 40} {yf2 + 105 + 30 * j} s {D}0-{dest};"); c.conn(m, 0, s_, 0)
    lem = c.add(f"#X obj 800 {ys2} list;"); c.conn(ri, 9, lem, 0)
    lbs = c.add(f"#X obj 1000 {ys2} list;"); c.conn(ri, 10, lbs, 0)
    mbs = c.add(f"#X msg 1000 {ys2 + 30} set {D}4;"); c.conn(lbs, 0, mbs, 0)
    sbs = c.add(f"#X obj 1000 {ys2 + 60} s {D}0-bselv;"); c.conn(mbs, 0, sbs, 0)
    uem = c.add(f"#X obj 800 {ys2 + 30} unpack f f;"); c.conn(lem, 0, uem, 0)
    for k, dest in [(0, "followv"), (1, "zeroxv")]:
        m = c.add(f"#X msg {800 + 80 * k} {ys2 + 60} set {D}1;"); c.conn(uem, k, m, 0)
        s = c.add(f"#X obj {800 + 80 * k} {ys2 + 90} s {D}0-{dest};"); c.conn(m, 0, s, 0)
    return c


def build():
    c = Canvas()
    gui(c)
    lg = logic()
    rows = max(len(LEFT), len(RIGHT))
    yl = TOP + rows * ROWH + SAMPLE_H + FX_H
    sub = ["#N canvas 40 40 1320 900 logic 0;"] + lg.lines + lg.conns + [f"#X restore {X0} {yl} pd logic;"]
    h = yl + 30
    return "\n".join([f"#N canvas 20 20 {X1 + 8 * CW + 10} {h} 10;"] + c.lines + sub + c.conns) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "pd", "epsedit.pd")
    open(out, "w").write(build())
    print("wrote", out, f"({len(ALL)} controls, {len(EDIT)} instrument parameters)")
