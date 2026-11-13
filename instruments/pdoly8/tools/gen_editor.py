#!/usr/bin/env python3
"""Generates p8edit.pd: an editor for pdoly8~ in rows of 8, row k = CC page k of the controller layer.

usage: python3 tools/gen_editor.py [outfile]   (default: p8edit.pd next to the external)

[p8edit <bus>]  e.g. [p8edit $0-pdoly8] in the help patch
  sends    n <name> <0-127>        to [s <bus>]      (the [r <bus>] in front of pdoly8~)
  listens  info <name> <0-127> <t> on [r <bus>-out]  (outlet 3 of pdoly8~ into [s <bus>-out]):
  pdoly8~ reports every change itself (Launchkey, presets, messages); at load and with the
  refresh button the editor asks for all values (ninfo).
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


# rows 1-11 = the CC pages of the Launchkey (CC_ROWS in gen_bela.py), then rows without CC
ROWS = [(f"PAGE {k + 1}  {t}", list(ns)) for k, (t, ns) in enumerate(GB.CC_ROWS)]
ROWS[10] = (ROWS[10][0], ROWS[10][1] + ["l3k", "l3f", "l3t2", "l3a2"])   # page 11 has 4 CCs: the rest by mouse
LEFT = ROWS[:8]
RIGHT = ROWS[8:] + [
         ("MORE", ["leak", "mgf", "ch1", None, None, None, None, None]),
         ("DEG1 stages (sets env1 to own)", stages(1)),
         ("DEG2 stages (sets env2 to own)", stages(2)),
         ("DEG3 stages (sets env3 to own)", stages(3)),
         ("DEG4 stages (sets env4 to own)", stages(4))]
assert len(RIGHT) == 8
ALL = [n for _, ns in LEFT + RIGHT for n in ns if n]
assert len(ALL) == len(set(ALL)) and set(ALL) == set(RANGE), set(RANGE) ^ set(ALL)

CW, ROWH, TOP, X0 = 80, 88, 166, 10
BAND_Y = 44                                      # presets / delay / sysex band
X1 = X0 + 8 * CW + 30
BAND = ("#ececec", "#dcdcdc")


def cc_text(ns):
    ccs = [CC[n] for n in ns if n in CC]
    if not ccs:
        return "no CC"
    if len(ccs) < len([n for n in ns if n]):
        return "CC " + " ".join(map(str, ccs)) + " (the rest by mouse)"
    return f"CC {min(ccs)}-{max(ccs)}" if max(ccs) - min(ccs) + 1 == len(ccs) else "CC " + " ".join(map(str, ccs))


class Canvas:
    def __init__(self):
        self.lines, self.conns = [], []

    def add(self, t):
        self.lines.append(t)
        return len(self.lines) - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")


def bng(c, x, y, send, label):
    c.add(f"#X obj {x} {y} bng 17 250 50 0 {D}0-{send} empty {label.replace(' ', SP)} 21 9 0 10 #fcfcfc #000000 #000000;")


DELAYS = [("flanger", "flanger"), ("chorus1", "chorus small"), ("chorus2", "chorus big"),
          ("slap", "slapback"), ("long", "long delay"), ("off", "off")]


def top_band(c):
    """presets (factory, user bank), delay presets, Poly-800 II sysex: send names into [pd logic]"""
    y = BAND_Y
    c.add(f"#X obj {X0 - 4} {y} cnv 15 {8 * CW} 112 empty empty PRESETS{SP}AND{SP}DELAY 4 8 0 10 #e4ecf4 #404040 0;")
    c.add(f"#X text {X0} {y + 20} factory;")
    c.add(f"#X floatatom {X0 + 52} {y + 20} 3 0 63 0 - - {D}0-fac;")
    c.add(f"#X text {X0 + 82} {y + 20} = program;")
    c.add(f"#X floatatom {X0 + 150} {y + 20} 2 0 0 0 - {D}0-prog -;")
    c.add(f"#X text {X0 + 200} {y + 20} user;")
    c.add(f"#X floatatom {X0 + 234} {y + 20} 3 0 63 0 - - {D}0-usr;")
    c.add(f"#X text {X0 + 270} {y + 20} store to;")
    c.add(f"#X floatatom {X0 + 330} {y + 20} 3 0 63 0 - - {D}0-sto;")
    bng(c, X0 + 380, y + 20, "save", "save bank")
    bng(c, X0 + 480, y + 20, "load", "load bank")
    c.add(f"#X text {X0} {y + 50} delay;")
    for k, (msg, label) in enumerate(DELAYS):
        bng(c, X0 + 52 + k * 106, y + 50, f"dl-{msg}", label)
    c.add(f"#X text {X0} {y + 74} user bank: p8user-bank.txt next to the editor (save bank writes it). "
          f"store to N: the current sound into slot N., f 90;")
    x1 = X1
    c.add(f"#X obj {x1 - 4} {y} cnv 15 {8 * CW} 112 empty empty POLY-800{SP}II{SP}SYSEX{SP}(the{SP}user{SP}bank)"
          f" 4 8 0 10 #f4ece4 #404040 0;")
    bng(c, x1, y + 20, "syxsend", "send bank")
    bng(c, x1 + 110, y + 20, "syxread", "read .syx")
    bng(c, x1 + 220, y + 20, "syxwrite", "write .syx")
    c.add(f"#X text {x1 + 330} {y + 20} MIDI out port;")
    c.add(f"#X floatatom {x1 + 430} {y + 20} 2 1 16 0 - {D}0-portv {D}0-port;")
    bng(c, x1 + 8 * CW - 100, y + 20, "refresh", "refresh")
    c.add(f"#X text {x1} {y + 50} status;")
    c.add(f"#X symbolatom {x1 + 50} {y + 50} 36 0 0 0 - {D}0-syxst -;")
    c.add(f"#X text {x1 + 380} {y + 50} Poly-800 MIDI channel;")
    c.add(f"#X floatatom {x1 + 540} {y + 50} 2 1 16 0 - {D}0-chv {D}0-syxch;")
    c.add(f"#X text {x1} {y + 74} receive: start the data dump on the Poly-800 II {COMMA} the 64 programs land in the "
          f"user bank. send: needs a dump received or read first (sequencer {COMMA} MIDI channel come from it) {COMMA} the channel must be the Poly-800's., f 90;")


def gui(c):
    c.add(f"#X text {X0} 6 pdoly8~ editor: rows of 8 as the pages of the controller (MIDI channel of [p8cc]). Slider and number:"
          f" the value as from a pot (0-127) {COMMA} the box right of it: the real value., f 200;")
    c.add(f"#X text {X0} 22 [p8edit <bus>] sends n <name> <value> to <bus> and reads outlet 3 of pdoly8~ from"
          f" <bus>-out: every change (Launchkey {COMMA} presets {COMMA} messages) shows up here. Page k on the Launchkey = row k., f 200;")
    top_band(c)
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
    # start and refresh button: ask for all values (later changes are reported by pdoly8~ itself)
    lb = c.add(f"#X obj 400 {y} loadbang;")
    rr = c.add(f"#X obj 500 {y} r {D}0-refresh;")
    dl = c.add(f"#X obj 400 {y + 30} del 30;")
    c.conn(lb, 0, dl, 0); c.conn(rr, 0, dl, 0)
    allm = c.add(f"#X msg 400 {y + 60} " + f" {COMMA} ".join(f"ninfo {n}" for n in ALL) + ", f 70;")
    c.conn(dl, 0, allm, 0)
    s2 = c.add(f"#X obj 400 {y + 270} s {D}1;")
    c.conn(allm, 0, s2, 0)
    # presets: factory, user bank (p8user-bank.txt next to the editor), delay presets
    yb = y + 330
    rf = c.add(f"#X obj 10 {yb} r {D}0-fac;")
    tf = c.add(f"#X obj 10 {yb + 30} t f f;"); c.conn(rf, 0, tf, 0)
    pf = c.add(f"#X obj 10 {yb + 60} p8factory;"); c.conn(tf, 0, pf, 0)
    sf = c.add(f"#X obj 10 {yb + 90} s {D}1;"); c.conn(pf, 0, sf, 0)
    ex = c.add(f"#X obj 120 {yb + 60} expr (int($f1 / 8) + 1) * 10 + $f1 % 8 + 1;"); c.conn(tf, 1, ex, 0)
    mp = c.add(f"#X msg 120 {yb + 90} set {D}1;"); c.conn(ex, 0, mp, 0)
    sp = c.add(f"#X obj 120 {yb + 120} s {D}0-prog;"); c.conn(mp, 0, sp, 0)
    ru = c.add(f"#X obj 400 {yb} r {D}0-usr;")
    rs = c.add(f"#X obj 480 {yb} r {D}0-sto;")
    ms = c.add(f"#X msg 480 {yb + 30} store {D}1;"); c.conn(rs, 0, ms, 0)
    ro2 = c.add(f"#X obj 560 {yb + 30} r {D}1-out;")
    pu = c.add(f"#X obj 400 {yb + 60} p8user {D}0-user;")
    c.conn(ru, 0, pu, 0); c.conn(ms, 0, pu, 0); c.conn(ro2, 0, pu, 1)
    su = c.add(f"#X obj 400 {yb + 90} s {D}1;"); c.conn(pu, 0, su, 0)
    td = c.add(f"#X obj 700 {yb + 90} text define {D}0-user;")
    lbf = c.add(f"#X obj 700 {yb} loadbang;")
    rl = c.add(f"#X obj 780 {yb} r {D}0-load;")
    rd = c.add(f"#X msg 700 {yb + 30} read p8user-bank.txt;"); c.conn(lbf, 0, rd, 0); c.conn(rl, 0, rd, 0)
    rsv = c.add(f"#X obj 900 {yb} r {D}0-save;")
    wr = c.add(f"#X msg 900 {yb + 30} write p8user-bank.txt;"); c.conn(rsv, 0, wr, 0)
    c.conn(rd, 0, td, 0); c.conn(wr, 0, td, 0)
    for k, (msg, _) in enumerate(DELAYS):
        r = c.add(f"#X obj {10 + 110 * k} {yb + 170} r {D}0-dl-{msg};")
        m = c.add(f"#X msg {10 + 110 * k} {yb + 200} delay {msg};"); c.conn(r, 0, m, 0)
        sd = c.add(f"#X obj {10 + 110 * k} {yb + 230} s {D}1;"); c.conn(m, 0, sd, 0)
    # Poly-800 II sysex: in from [sysexin], bank in / out through pdoly8~ (syxin, bank, syxbank, syxsend)
    ys = yb + 290
    si = c.add(f"#X obj 10 {ys} sysexin;")
    mi = c.add(f"#X msg 10 {ys + 30} syxin {D}1;"); c.conn(si, 0, mi, 0)
    ssi = c.add(f"#X obj 10 {ys + 60} s {D}1;"); c.conn(mi, 0, ssi, 0)
    ro3 = c.add(f"#X obj 200 {ys} r {D}1-out;")
    rb = c.add(f"#X obj 200 {ys + 30} route bank syxbyte syxstatus syxch;"); c.conn(ro3, 0, rb, 0)
    mch = c.add(f"#X msg 420 {ys + 120} set {D}1;"); c.conn(rb, 3, mch, 0)
    sch = c.add(f"#X obj 420 {ys + 150} s {D}0-chv;"); c.conn(mch, 0, sch, 0)
    rch = c.add(f"#X obj 1200 {ys} r {D}0-syxch;")
    mch2 = c.add(f"#X msg 1200 {ys + 30} syxch {D}1;"); c.conn(rch, 0, mch2, 0)
    sch2 = c.add(f"#X obj 1200 {ys + 60} s {D}1;"); c.conn(mch2, 0, sch2, 0)
    lbc = c.add(f"#X obj 1200 {ys + 90} loadbang;")
    m1c = c.add(f"#X msg 1200 {ys + 120} set 1;"); c.conn(lbc, 0, m1c, 0)
    s1c = c.add(f"#X obj 1200 {ys + 150} s {D}0-chv;"); c.conn(m1c, 0, s1c, 0)
    tl = c.add(f"#X obj 200 {ys + 60} t l l;"); c.conn(rb, 0, tl, 0)
    ls1 = c.add(f"#X obj 260 {ys + 90} list split 1;"); c.conn(tl, 1, ls1, 0)
    ls2 = c.add(f"#X obj 200 {ys + 120} list split 1;"); c.conn(tl, 0, ls2, 0)
    tset = c.add(f"#X obj 200 {ys + 150} text set {D}0-user;"); c.conn(ls1, 0, tset, 1); c.conn(ls2, 1, tset, 0)
    mo = c.add(f"#X obj 420 {ys + 60} midiout;"); c.conn(rb, 1, mo, 0)
    rpt = c.add(f"#X obj 500 {ys} r {D}0-port;")
    c.conn(rpt, 0, mo, 1)
    lbp = c.add(f"#X obj 580 {ys} loadbang;")
    mpv = c.add(f"#X msg 580 {ys + 30} set 1;"); c.conn(lbp, 0, mpv, 0)
    spv = c.add(f"#X obj 580 {ys + 60} s {D}0-portv;"); c.conn(mpv, 0, spv, 0)
    lb1 = c.add(f"#X msg 660 {ys + 30} 1;"); c.conn(lbp, 0, lb1, 0); c.conn(lb1, 0, mo, 1)
    lst = c.add(f"#X obj 300 {ys + 60} list;"); c.conn(rb, 2, lst, 0)   # status symbol (route gives it as a selector)
    mst = c.add(f"#X msg 300 {ys + 90} set {D}1;"); c.conn(lst, 0, mst, 0)
    sst = c.add(f"#X obj 300 {ys + 120} s {D}0-syxst;"); c.conn(mst, 0, sst, 0)
    # send bank: every line of the user bank -> syxbank n pdoly8 ..., then syxsend
    rsx = c.add(f"#X obj 700 {ys} r {D}0-syxsend;")
    tbb = c.add(f"#X obj 700 {ys + 30} t b b;"); c.conn(rsx, 0, tbb, 0)
    # the user bank into the Poly-800 memory of pdoly8~ (before send bank and write .syx)
    push = c.add(f"#X obj 760 {ys + 30} t b b;"); c.conn(tbb, 1, push, 0)
    n64 = c.add(f"#X msg 740 {ys + 60} 64;"); c.conn(push, 0, n64, 0)
    z = c.add(f"#X msg 800 {ys + 60} 0;"); c.conn(push, 1, z, 0)
    un = c.add(f"#X obj 740 {ys + 90} until;"); c.conn(n64, 0, un, 0)
    cnt = c.add(f"#X obj 740 {ys + 120} f;"); c.conn(un, 0, cnt, 0); c.conn(z, 0, cnt, 1)
    inc = c.add(f"#X obj 800 {ys + 150} + 1;"); 
    tff = c.add(f"#X obj 740 {ys + 150} t f f f;"); c.conn(cnt, 0, tff, 0); c.conn(tff, 2, inc, 0); c.conn(inc, 0, cnt, 1)
    lpp = c.add(f"#X obj 740 {ys + 210} list prepend;"); c.conn(tff, 1, lpp, 1)
    tg = c.add(f"#X obj 740 {ys + 180} text get {D}0-user;"); c.conn(tff, 0, tg, 0); c.conn(tg, 0, lpp, 0)
    lps = c.add(f"#X obj 740 {ys + 240} list prepend syxbank;"); c.conn(lpp, 0, lps, 0)
    ltr = c.add(f"#X obj 740 {ys + 270} list trim;"); c.conn(lps, 0, ltr, 0)
    ss2 = c.add(f"#X obj 740 {ys + 300} s {D}1;"); c.conn(ltr, 0, ss2, 0)
    msd = c.add(f"#X msg 700 {ys + 330} syxsend;"); c.conn(tbb, 0, msd, 0)
    ss3 = c.add(f"#X obj 700 {ys + 360} s {D}1;"); c.conn(msd, 0, ss3, 0)
    # read / write a .syx file
    rrd = c.add(f"#X obj 950 {ys} r {D}0-syxread;")
    op = c.add(f"#X obj 950 {ys + 30} openpanel;"); c.conn(rrd, 0, op, 0)
    mrd = c.add(f"#X msg 950 {ys + 60} syxread {D}1;"); c.conn(op, 0, mrd, 0)
    rwr = c.add(f"#X obj 1080 {ys} r {D}0-syxwrite;")
    sv = c.add(f"#X obj 1080 {ys + 30} savepanel;"); c.conn(rwr, 0, sv, 0)
    twr = c.add(f"#X obj 1080 {ys + 60} t a b;"); c.conn(sv, 0, twr, 0); c.conn(twr, 1, push, 0)
    mwr = c.add(f"#X msg 1080 {ys + 90} syxwrite {D}1;"); c.conn(twr, 0, mwr, 0)
    ss4 = c.add(f"#X obj 950 {ys + 120} s {D}1;"); c.conn(mrd, 0, ss4, 0); c.conn(mwr, 0, ss4, 0)
    y = ys + 330
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
