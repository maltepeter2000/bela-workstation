#!/usr/bin/env python3
"""Generates bela/project/ : the Bela Pepper patch for pdoly8~.

  10 pages of 8 pots (PAGES below). Button 1/2: page down/up, LED 1-10 shows the page.
  Button 3/4: factory preset down/up. Pickup: after a page or preset change a pot only
  takes over once it passes the current value.
  CV-Lock: hold button 4 and turn a pot: that input stays on its parameter (for CV) on
  every page; the same again unlocks it. Releasing button 4 without turning = preset up.
  MIDI (USB): notes, pitch bend, program change = factory preset, CC = CCS below.
  All values 0-127 go through pdoly8~'s  n <name> <0-127>  (ranges in pdoly8~.c),
  pickup targets come from  nget <name> <pot>.

Send/receive names
  p8-in       messages to pdoly8~          p8-out      outlet 3 of pdoly8~
  p8-preset   load factory preset N        p8-setpage  go to page N (0-9)
  p8-page     the page changed (0-9)       p8-pagev    [value] with the page
  p8-refresh  pots re-read their targets   p8-tgt-N    pickup target of pot N
  p8-b4       button 4 down (1) / up (0)   p8-b4used   a pot was locked during the hold
  p8-tick     pot clock (10 ms)
usage: python3 tools/gen_bela.py
"""
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "bela", "project")

# page name, 8 pot parameters ("none" = free)
PAGES = [
    ("PLAY",       ["har1", "har2", "env1", "env2", "intv", "detn", "lev1", "lev2"]),
    ("FILTER",     ["cut", "res", "egi", "kbd", "env3", "vcut", "drv", "nois"]),
    ("OSC",        ["oct1", "oct2", "pw1", "pw2", "pwm1", "pwm2", "hmg", "drft"]),
    ("WALSH",      ["wsh1", "wsh2", "wmx1", "wmx2", "wsm", "wse", "lev1", "lev2"]),
    ("SYNC-RING",  ["sync", "swp2", "swe", "swm", "ring", "rmf", "lev1", "lev2"]),
    ("FM",         ["fm1", "fm2", "fmr", "fmhz", "fms", "fmig", "lev1", "lev2"]),
    ("MG-DEG4",    ["mgr", "mgi", "mgd", "mgv", "env4", "trg4", "d4t1", "d4a1"]),
    ("LFO2",       ["l2r", "l2w", "l2k", "l2f", "l2t1", "l2a1", "l2t2", "l2a2"]),
    ("LFO3",       ["l3r", "l3w", "l3k", "l3f", "l3t1", "l3a1", "l3t2", "l3a2"]),
    ("FX-PERFORM", ["dtim", "dfb", "dmf", "dmi", "dlev", "port", "mono", "vamp"]),
]
# MIDI CC -> parameter (any channel)
CCS = {}
for cc0, page in [(20, 0), (40, 1), (48, 2), (12, 9), (102, 7), (110, 8)]:
    for k, nm in enumerate(PAGES[page][1]):
        CCS[cc0 + k] = nm
for k, nm in enumerate(["wsh1", "wsh2", "wmx1", "wmx2", "wsm", "wse"]):
    CCS[56 + k] = nm
for k, nm in enumerate(["sync", "swp2", "swe", "swm", "ring", "rmf", "fm1", "fm2", "fmr", "fmhz", "fms",
                        "fmig", "mgr", "mgi", "mgd", "mgv", "env4", "trg4", "d4t1", "d4a1"]):
    CCS[70 + k] = nm
for cc, nm in [(2, "pmod"), (3, "retr"), (4, "pol"), (5, "trg"), (8, "drvp"), (9, "mgf"), (28, "bnd"), (29, "mrph"),
               (30, "tone"), (31, "leak"), (33, "treb"), (34, "bass"), (35, "sprd"), (36, "d4t2"), (37, "d4a2")]:
    CCS[cc] = nm
BUTTONS = [26, 25, 24, 23]                       # Pepper buttons 1-4, pressed = 0
LEDS = [17, 18, 21, 13, 14, 11, 12, 15, 16, 19]  # Pepper LEDs 1-10
# raw adc~ range of the Pepper pots, per pot (measured with beladrumrack/testprojects/potrange:
# maxima 0.907 0.916 0.923 0.924 0.877 0.890 0.917 0.997, here with 1 % margin inwards)
POT_LO = [0.002] * 8
POT_HI = [0.898, 0.907, 0.914, 0.915, 0.868, 0.881, 0.907, 0.987]
FONT = 10


def esc(t):
    return t.replace(",", " \\,").replace(";", " \\;").replace("$", "\\$")


class P:
    def __init__(s, name=None, w=700, h=500):
        s.o, s.c, s.name, s.w, s.h = [], [], name, w, h

    def add(s, txt, x, y, kind="obj"):
        s.o.append((kind, x, y, txt))
        return len(s.o) - 1

    def msg(s, txt, x, y):
        return s.add(txt, x, y, "msg")

    def note(s, txt, x, y):
        return s.add(txt, x, y, "text")

    def sub(s, p, x, y):
        s.o.append(("sub", x, y, p))
        return len(s.o) - 1

    def con(s, a, o, b, i):
        s.c.append((a, o, b, i))

    def lines(s, top=False):
        L = [f"#N canvas 0 50 {s.w} {s.h} {FONT};" if top else f"#N canvas 0 50 {s.w} {s.h} {s.name} 0;"]
        for k, x, y, t in s.o:
            if k == "sub":
                L += t.lines()
                L.append(f"#X restore {x} {y} pd {t.name};")
            elif k == "text":
                L.append(f"#X text {x} {y} {esc(t)}, f {min(max(len(t), 10), 100)};")
            else:
                L.append(f"#X {k} {x} {y} {esc(t)};")
        for a, o, b, i in s.c:
            L.append(f"#X connect {a} {o} {b} {i};")
        return L


def p8knob():
    """one pot: $1 pot 0-7, $2 adc~ channel, $3 raw low end, $4 scale to 0-127"""
    p = P(None, 900, 640)
    p.note("p8knob: one Pepper pot (and its CV). Pickup after page / preset changes, CV-Lock with button 4."
           " Args: pot (0-7), adc~ channel, raw low end, scale.", 10, 6)
    rt = p.add("r p8-tick", 60, 40); ad = p.add("adc~ $2", 10, 40); sn = p.add("snapshot~", 10, 65)
    lo = p.add("- $3", 10, 90); sc = p.add("* $4", 10, 115); cl = p.add("clip 0 127", 10, 140)
    it = p.add("i", 10, 165); ch = p.add("change", 10, 190); tv = p.add("t f f f f f f", 10, 220)
    for a_, b_ in [(ad, sn), (rt, sn), (sn, lo), (lo, sc), (sc, cl), (cl, it), (it, ch), (ch, tv)]:
        p.con(a_, 0, b_, 0)
    vs1 = p.add("f", 10, 470); vs2 = p.add("f", 200, 470); vs3 = p.add("f", 560, 300)
    p.con(tv, 5, vs1, 1); p.con(tv, 4, vs2, 1); p.con(tv, 3, vs3, 1)
    # decision: 0 nothing, 1 send to the page parameter, 2 send to the locked parameter
    p.note("decision: locked -> 2 / caught or passing the target -> 1 / else 0", 120, 290)
    dex = p.add("expr if($f2, 2, if($f3 || ($f4 >= 0 && ($f4 - $f5) * ($f1 - $f5) <= 0) || abs($f1 - $f5) <= 1, 1, 0))", 10, 320)
    p.con(tv, 1, dex, 0); p.con(tv, 0, dex, 3)
    ini = p.add("loadbang", 700, 40); m1 = p.msg("-1", 700, 65); p.con(ini, 0, m1, 0); p.con(m1, 0, dex, 3)
    dse = p.add("sel 1 2", 10, 350); p.con(dex, 0, dse, 0)
    tb = p.add("t b b", 10, 380); one = p.msg("1", 60, 410)
    p.con(dse, 0, tb, 0); p.con(tb, 1, one, 0); p.con(one, 0, dex, 2); p.con(tb, 0, vs1, 0)
    p.con(dse, 1, vs2, 0)
    pre1 = p.add("list prepend n none", 10, 500); pre2 = p.add("list prepend n none", 200, 500)
    tr1 = p.add("list trim", 10, 530); tr2 = p.add("list trim", 200, 530)
    so1 = p.add("s p8-in", 10, 560); so2 = p.add("s p8-in", 200, 560)
    p.con(vs1, 0, pre1, 0); p.con(vs2, 0, pre2, 0); p.con(pre1, 0, tr1, 0); p.con(pre2, 0, tr2, 0)
    p.con(tr1, 0, so1, 0); p.con(tr2, 0, so2, 0)
    # pickup target
    rtg = p.add("r p8-tgt-$1", 400, 40); tt = p.add("t f b", 400, 65); z = p.msg("0", 450, 90)
    p.con(rtg, 0, tt, 0); p.con(tt, 1, z, 0); p.con(z, 0, dex, 2); p.con(tt, 0, dex, 4)
    # page: name of this pot on the page, ask pdoly8~ for its value
    rpg = p.add("r p8-page", 400, 130); rrf = p.add("r p8-refresh", 500, 130); vpg = p.add("v p8-pagev", 500, 155)
    p.con(rrf, 0, vpg, 0)
    gate = p.add("spigot 1", 400, 185); p.con(rpg, 0, gate, 0); p.con(vpg, 0, gate, 0)
    tp = p.add("t f b", 400, 210); z2 = p.msg("0", 470, 235); p.con(gate, 0, tp, 0); p.con(tp, 1, z2, 0); p.con(z2, 0, dex, 2)
    tg = p.add("text get p8-map $1 1", 400, 260); p.con(tp, 0, tg, 0)
    tl = p.add("t l l", 400, 285); p.con(tg, 0, tl, 0)
    pn = p.add("list prepend n", 560, 330); hold = p.add("list append", 700, 360)
    p.con(tl, 1, pn, 0); p.con(pn, 0, pre1, 1); p.con(pn, 0, hold, 1)
    rq = p.add("list prepend nget", 400, 330); ap = p.add("list append $1", 400, 355); rqt = p.add("list trim", 400, 380)
    rqs = p.add("s p8-in", 400, 405)
    p.con(tl, 0, rq, 0); p.con(rq, 0, ap, 0); p.con(ap, 0, rqt, 0); p.con(rqt, 0, rqs, 0)
    # CV-Lock: button 4 held and the pot turned by 4 or more
    p.note("CV-Lock: button 4 held + pot turned by 4 -> lock / unlock", 560, 410)
    rb = p.add("r p8-b4", 560, 440); bs = p.add("sel 1 0", 560, 465); tpr = p.add("t b b", 560, 490)
    arm1 = p.msg("1", 600, 515); arm0 = p.msg("0", 640, 490)
    lsp = p.add("spigot", 400, 600); lex = p.add("expr abs($f1 - $f2) >= 4", 400, 625)
    p.con(rb, 0, bs, 0); p.con(bs, 0, tpr, 0); p.con(tpr, 1, arm1, 0); p.con(tpr, 0, vs3, 0); p.con(vs3, 0, lex, 1)
    p.con(arm1, 0, lsp, 1); p.con(bs, 1, arm0, 0); p.con(arm0, 0, lsp, 1)
    p.con(tv, 2, lsp, 0); p.con(lsp, 0, lex, 0)
    lse = p.add("sel 1", 400, 650); p.con(lex, 0, lse, 0)
    tlk = p.add("t b b b", 400, 675); disarm = p.msg("0", 520, 700); used = p.add("s p8-b4used", 580, 700)
    p.con(lse, 0, tlk, 0); p.con(tlk, 2, disarm, 0); p.con(disarm, 0, lsp, 1); p.con(tlk, 1, used, 0)
    lf = p.add("f", 400, 705); leq = p.add("== 0", 400, 730); p.con(tlk, 0, lf, 0); p.con(lf, 0, leq, 0); p.con(leq, 0, lf, 1)
    t3 = p.add("t f f f", 400, 755); p.con(leq, 0, t3, 0)
    p.con(t3, 2, dex, 1)
    inv = p.add("== 0", 470, 780); p.con(t3, 1, inv, 0); p.con(inv, 0, gate, 1)
    ls = p.add("sel 1 0", 400, 805); p.con(t3, 0, ls, 0)
    p.con(ls, 0, hold, 0); p.con(hold, 0, pre2, 1)
    pl = p.add("list prepend locked pot $1:", 400, 860); plt = p.add("list trim", 400, 885); prl = p.add("print pdoly8", 400, 910)
    p.con(hold, 0, pl, 0); p.con(pl, 0, plt, 0); p.con(plt, 0, prl, 0)
    ul = p.add("list prepend unlocked pot $1", 600, 860); ult = p.add("list trim", 600, 885)
    p.con(ls, 1, ul, 0); p.con(ul, 0, ult, 0); p.con(ult, 0, prl, 0)
    p.con(ls, 1, vpg, 0)
    return p


def sec_init():
    p = P("init", 760, 260)
    p.note("INIT: buttons as inputs, LEDs as outputs, then factory preset 0 and page 0", 10, 6)
    lb = p.add("loadbang", 10, 40)
    m = p.msg(", ".join([f"in {b}" for b in BUTTONS] + [f"out {l}" for l in LEDS]), 10, 70)
    sd = p.add("s bela_setDigital", 10, 100)
    d = p.add("del 500", 10, 140); tb = p.add("t b b", 10, 165)
    z1 = p.msg("0", 10, 195); s1 = p.add("s p8-setpage", 10, 220)
    z2 = p.msg("0", 120, 195); s2 = p.add("s p8-preset", 120, 220)
    p.con(lb, 0, m, 0); p.con(m, 0, sd, 0); p.con(lb, 0, d, 0); p.con(d, 0, tb, 0)
    p.con(tb, 1, z2, 0); p.con(z2, 0, s2, 0); p.con(tb, 0, z1, 0); p.con(z1, 0, s1, 0)
    return p


def sec_pages():
    p = P("pages", 900, 360)
    p.note("PAGES: p8-setpage N -> page N (0-9): the pots read their parameters, LED N+1 lights, the console names the page",
           10, 6)
    r = p.add("r p8-setpage", 10, 40); w = p.add("mod 10", 10, 65); t = p.add("t f f f", 10, 90)
    v = p.add("v p8-pagev", 200, 120); sp = p.add("s p8-page", 100, 120)
    p.con(r, 0, w, 0); p.con(w, 0, t, 0); p.con(t, 2, v, 0); p.con(t, 1, sp, 0)
    tg = p.add("text get p8-map", 10, 150); f8 = p.add("list split 8", 10, 175)
    pr = p.add("list prepend page", 100, 200); prt = p.add("list trim", 100, 225); prn = p.add("print pdoly8", 100, 250)
    p.con(t, 0, tg, 0); p.con(tg, 0, f8, 0); p.con(f8, 1, pr, 0); p.con(pr, 0, prt, 0); p.con(prt, 0, prn, 0)
    for j, pin in enumerate(LEDS):
        x = 10 + j * 85
        e = p.add(f"== {j}", x, 270); so = p.add(f"s bela_digitalOut{pin}", x, 295)
        p.con(t, 2, e, 0); p.con(e, 0, so, 0)
    return p


def sec_buttons():
    p = P("buttons", 900, 380)
    p.note("BUTTONS (pressed = 0): 1 page down / 2 page up / 3 preset down / 4 preset up on release,"
           " unless a pot was locked while it was held (CV-Lock)", 10, 6)
    for k, (pin, var, d, dst) in enumerate([(BUTTONS[0], "p8-pagev", 9, "p8-setpage"), (BUTTONS[1], "p8-pagev", 1, "p8-setpage"),
                                            (BUTTONS[2], "p8-prog", 63, "p8-preset")]):
        x = 10 + k * 200
        rb = p.add(f"r bela_digitalIn{pin}", x, 40); ch = p.add("change 1", x, 65); se = p.add("sel 0", x, 90)
        vv = p.add(f"v {var}", x, 115); ad = p.add(f"+ {d}", x, 140); ss = p.add(f"s {dst}", x, 165)
        for a_, b_ in [(rb, ch), (ch, se), (se, vv), (vv, ad), (ad, ss)]:
            p.con(a_, 0, b_, 0)
    x = 620
    rb = p.add(f"r bela_digitalIn{BUTTONS[3]}", x, 40); ch = p.add("change 1", x, 65); eq = p.add("== 0", x, 90)
    t = p.add("t f f", x, 115); sb = p.add("s p8-b4", x + 60, 140); se = p.add("sel 1 0", x, 165)
    z = p.msg("0", x, 195); uf = p.add("f", x + 60, 220); s0 = p.add("sel 0", x + 60, 245)
    vv = p.add("v p8-prog", x + 60, 270); ad = p.add("+ 1", x + 60, 295); ss = p.add("s p8-preset", x + 60, 320)
    ru = p.add("r p8-b4used", x + 150, 165); one = p.msg("1", x + 150, 190)
    for a_, o, b_, i in [(rb, 0, ch, 0), (ch, 0, eq, 0), (eq, 0, t, 0), (t, 1, sb, 0), (t, 0, se, 0), (se, 0, z, 0),
                         (z, 0, uf, 1), (se, 1, uf, 0), (uf, 0, s0, 0), (s0, 0, vv, 0), (vv, 0, ad, 0), (ad, 0, ss, 0),
                         (ru, 0, one, 0), (one, 0, uf, 1)]:
        p.con(a_, o, b_, i)
    return p


def sec_presets():
    p = P("presets", 620, 260)
    p.note("PRESETS: p8-preset N loads factory program N (0-63), then the pots re-read their targets", 10, 6)
    r = p.add("r p8-preset", 10, 40); w = p.add("mod 64", 10, 65); t = p.add("t b f f f", 10, 90)
    v = p.add("v p8-prog", 300, 120); pr = p.add("print preset", 200, 120)
    fa = p.add("p8factory", 100, 150); s = p.add("s p8-in", 100, 175); rf = p.add("s p8-refresh", 10, 210)
    p.con(r, 0, w, 0); p.con(w, 0, t, 0); p.con(t, 3, v, 0); p.con(t, 2, pr, 0); p.con(t, 1, fa, 0)
    p.con(fa, 0, s, 0); p.con(t, 0, rf, 0)
    return p


def sec_midi():
    p = P("midi", 820, 420)
    p.note("MIDI (USB): notes / pitch bend / program change = factory preset / CC -> parameter (table p8-cc) on any channel",
           10, 6)
    ni = p.add("notein", 10, 40); pk = p.add("pack f f f", 10, 70); s1 = p.add("s p8-in", 10, 100)
    p.con(ni, 0, pk, 0); p.con(ni, 1, pk, 1); p.con(ni, 2, pk, 2); p.con(pk, 0, s1, 0)
    bi = p.add("bendin", 200, 40); be = p.add("expr ($f1 - 8192) / 8192", 200, 70)
    bm = p.msg("bend $1", 200, 100); s2 = p.add("s p8-in", 200, 130)
    p.con(bi, 0, be, 0); p.con(be, 0, bm, 0); p.con(bm, 0, s2, 0)
    pg = p.add("pgmin", 450, 40); mi = p.add("- 1", 450, 70); s3 = p.add("s p8-preset", 450, 100)
    p.con(pg, 0, mi, 0); p.con(mi, 0, s3, 0)
    ci = p.add("ctlin", 10, 170); pk2 = p.add("pack f f", 10, 195); up = p.add("unpack f f", 10, 220)
    p.con(ci, 1, pk2, 1); p.con(ci, 0, pk2, 0); p.con(pk2, 0, up, 0)
    p.note("CC number first: find its parameter (or none), then the value", 200, 170)
    ts = p.add("text search p8-cc", 100, 250); mo = p.add("moses 0", 100, 275)
    no = p.msg("none", 100, 300); tg = p.add("text get p8-cc", 200, 300); sp = p.add("list split 1", 200, 325)
    pn = p.add("list prepend n", 200, 350); hold = p.add("list prepend n none", 10, 380)
    p.con(up, 1, ts, 0); p.con(ts, 0, mo, 0); p.con(mo, 0, no, 0); p.con(no, 0, pn, 0); p.con(mo, 1, tg, 0); p.con(tg, 0, sp, 0)
    p.con(sp, 1, pn, 0); p.con(pn, 0, hold, 1)
    t = p.add("t b f", 10, 250); p.con(up, 0, t, 0); p.con(t, 1, hold, 0)
    tr = p.add("list trim", 10, 405); s4 = p.add("s p8-in", 10, 430); rf = p.add("s p8-refresh", 120, 430)
    p.con(hold, 0, tr, 0); p.con(tr, 0, s4, 0); p.con(t, 0, rf, 0)
    return p


def sec_pots():
    p = P("pots", 900, 200)
    p.note("POTS + CV: adc~ 3-10 every 10 ms -> p8knob (range of every pot measured on a Pepper)."
           " Empty jack: the pot sets the value. Cable in: the pot attenuates the CV.", 10, 6)
    lb = p.add("loadbang", 10, 40); me = p.add("metro 10", 90, 40); st = p.add("s p8-tick", 170, 40)
    p.con(lb, 0, me, 0); p.con(me, 0, st, 0)
    for j in range(8):
        p.add("p8knob %d %d %g %.4f" % (j, 3 + j, POT_LO[j], 127.0 / (POT_HI[j] - POT_LO[j])), 10 + (j % 4) * 220, 80 + (j // 4) * 40)
    return p


def sec_tables():
    p = P("tables", 900, 200)
    p.note("TABLES: p8-map = 8 pot parameters + name per page (line = page), p8-cc = MIDI CC -> parameter", 10, 6)
    p.add("text define -k p8-map", 10, 40)
    p.add("text define -k p8-cc", 250, 40)
    return p


def sec_out():
    p = P("from-pdoly8", 600, 160)
    p.note("FROM PDOLY8~: norm <pot> <0-127> -> pickup target of that pot", 10, 6)
    r = p.add("r p8-out", 10, 40); ro = p.add("route norm", 10, 65); rp = p.add("route 0 1 2 3 4 5 6 7", 10, 90)
    p.con(r, 0, ro, 0); p.con(ro, 0, rp, 0)
    for j in range(8):
        s_ = p.add(f"s p8-tgt-{j}", 10 + j * 70, 125); p.con(rp, j, s_, 0)
    return p


def main_patch():
    p = P(None, 700, 470)
    lines = ["pdoly8~ on the Bela Pepper",
             "Button 1 / 2: page down / up (LED 1-10). Button 3 / 4: factory preset down / up.",
             "Pots 1-8: the parameters of the page (pickup: a pot takes over once it passes the current value).",
             "CV-Lock: hold button 4 and turn a pot - that input stays on its parameter on every page (for CV).",
             "  Again to unlock. The console names pages, presets, locks and modulation targets.",
             "MIDI (USB): notes / pitch bend / program change = preset / CC: see the table in README.",
             "Audio in 1 = external FM source (page FM: fms 2, pot 6 = input gain)."]
    for k, l in enumerate(lines):
        p.note(l, 10, 6 + 15 * k)
    y = 110
    for sec, txt in [(sec_init(), "pins / preset 0 / page 0 at start"), (sec_tables(), "pages and CC tables"),
                     (sec_pages(), "page change / LEDs"), (sec_buttons(), "buttons 1-4"),
                     (sec_presets(), "factory presets"), (sec_midi(), "notes / bend / program / CC"),
                     (sec_pots(), "8 pots + CV"), (sec_out(), "pickup targets from pdoly8~")]:
        p.sub(sec, 10, y)
        p.note(txt, 140, y)
        y += 25
    r = p.add("r p8-in", 10, y + 20)
    ai = p.add("adc~ 1", 150, y + 20)
    p.note("audio in 1 -> FM input (fms 2, gain fmig)", 220, y + 20)
    o = p.add("pdoly8~", 10, y + 50)
    d = p.add("dac~ 1 2", 10, y + 85)
    so = p.add("s p8-out", 150, y + 85)
    p.con(r, 0, o, 0); p.con(ai, 0, o, 1); p.con(o, 0, d, 0); p.con(o, 1, d, 1); p.con(o, 2, so, 0)
    return p


def table_data(rows):
    return "#A set " + " \\; ".join(rows) + " \\;;"


def p8factory_embedded():
    """p8factory with the 64 programs stored inside the patch ([text define -k]):
    no file is read at run time, so it does not depend on Pd's search path."""
    rows = []
    for line in open(os.path.join(ROOT, "presets", "poly800mk2-factory.txt")):
        line = line.strip().rstrip(";").strip()
        if line:
            rows.append(line)
    data = " \\; ".join(rows) + " \\;"
    L = ["#N canvas 120 120 640 420 12;",
         "#X text 20 10 p8factory (Bela): the 64 Poly-800 II factory programs \\, stored inside this patch."
         " A slot number 0-63 sends that program as dump pdoly8 ... to the outlet., f 70;",
         "#X obj 20 90 inlet;",
         "#X obj 20 330 text define -k $0-bank;",
         "#A set " + data + ";",
         "#X obj 20 120 moses 0;",
         "#X obj 70 150 t f b;",
         "#X obj 130 180 text size $0-bank;",
         "#X obj 70 210 moses;",
         "#X obj 70 240 text get $0-bank;",
         "#X obj 70 270 list trim;",
         "#X obj 70 300 route pdoly8;",
         "#X obj 250 300 list prepend dump pdoly8;",
         "#X obj 250 330 list trim;",
         "#X obj 250 370 outlet;",
         "#X connect 1 0 3 0;", "#X connect 3 1 4 0;", "#X connect 4 1 5 0;", "#X connect 5 0 6 1;",
         "#X connect 4 0 6 0;", "#X connect 6 0 7 0;", "#X connect 7 0 8 0;", "#X connect 8 0 9 0;",
         "#X connect 9 0 10 0;", "#X connect 10 0 11 0;", "#X connect 11 0 12 0;"]
    return "\n".join(L) + "\n", len(rows)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    L = main_patch().lines(top=True)
    out = []
    for ln in L:
        out.append(ln)
        if ln.endswith("text define -k p8-map;"):
            out.append(table_data([" ".join(n) + " " + t for t, n in PAGES]))
        if ln.endswith("text define -k p8-cc;"):
            out.append(table_data([f"{cc} {nm}" for cc, nm in sorted(CCS.items())]))
    open(os.path.join(OUT, "_main.pd"), "w").write("\n".join(out) + "\n")
    open(os.path.join(OUT, "p8knob.pd"), "w").write("\n".join(p8knob().lines(top=True)) + "\n")
    for old in ("p8pot.pd",):
        if os.path.exists(os.path.join(OUT, old)): os.remove(os.path.join(OUT, old))
    txt, n = p8factory_embedded()
    open(os.path.join(OUT, "p8factory.pd"), "w").write(txt)
    shutil.rmtree(os.path.join(OUT, "presets"), ignore_errors=True)
    print(f"bela/project: _main.pd, p8pot.pd, p8factory.pd ({n} programs inside)")
