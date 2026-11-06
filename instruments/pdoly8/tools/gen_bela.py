#!/usr/bin/env python3
"""Generates bela/project/ : a minimal Bela Pepper patch for pdoly8~.

  MIDI (USB)   notes, pitch bend, CC 20-27 = controls 0-7, program change = factory preset
  Pots 1-8     controls 0-7 (har1 har2 env1 env2 intv detn lev1 lev2). On the Pepper a pot
               sets the value while its CV jack is empty; with a cable in, the pot
               attenuates that CV, so the CV moves the same control
  Button 1/2   factory preset down / up (0-63)
  Start        factory preset 0 (program 11)

Everything inside goes over send/receive:
  p8-in      messages to pdoly8~        p8-out    outlet 3 of pdoly8~ (dumps, values)
  p8-preset  load factory preset N      p8-tick   pot clock (10 ms)
usage: python3 tools/gen_bela.py      (writes bela/project/_main.pd and p8pot.pd)
"""
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "bela", "project")

CTL = ["har1", "har2", "env1", "env2", "intv", "detn", "lev1", "lev2"]
CC0 = 20                           # CC 20-27 = controls 0-7
BTN_DOWN, BTN_UP = 26, 25          # Pepper buttons 1 and 2 (pressed = 0)
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


def p8pot():
    p = P(None, 520, 330)
    p.note("p8pot: deadband for a pot or CV (0-127). Passes a value only when it moved by 2 or more"
           " (or reached 0 / 127), so a resting pot does not override a preset.", 10, 6)
    i = p.add("inlet", 10, 50)
    t = p.add("t f f", 10, 80)
    ex = p.add("expr abs($f1 - $f2) >= 2 || ($f1 != $f2 && ($f1 <= 0 || $f1 >= 127))", 100, 110)
    sp = p.add("spigot", 10, 150)
    t2 = p.add("t f f", 10, 180)
    o = p.add("outlet", 10, 220)
    p.con(i, 0, t, 0); p.con(t, 1, ex, 0); p.con(ex, 0, sp, 1); p.con(t, 0, sp, 0)
    p.con(sp, 0, t2, 0); p.con(t2, 0, o, 0); p.con(t2, 1, ex, 1)
    return p


def sec_init():
    p = P("init", 520, 260)
    p.note("INIT: Pepper buttons 1 / 2 as inputs, then factory preset 0", 10, 6)
    lb = p.add("loadbang", 10, 40)
    m = p.msg(f"in {BTN_DOWN}, in {BTN_UP}", 10, 70)
    sd = p.add("s bela_setDigital", 10, 100)
    d = p.add("del 500", 200, 70)
    z = p.msg("0", 200, 100)
    sp = p.add("s p8-preset", 200, 130)
    p.con(lb, 0, m, 0); p.con(m, 0, sd, 0); p.con(lb, 0, d, 0); p.con(d, 0, z, 0); p.con(z, 0, sp, 0)
    return p


def sec_midi():
    p = P("midi", 820, 520)
    p.note("MIDI (USB): notes and pitch bend to pdoly8~ / CC 20-27 = controls 0-7 / program change = factory preset", 10, 6)
    ni = p.add("notein", 10, 40); pk = p.add("pack f f f", 10, 70); s1 = p.add("s p8-in", 10, 100)
    p.con(ni, 0, pk, 0); p.con(ni, 1, pk, 1); p.con(ni, 2, pk, 2); p.con(pk, 0, s1, 0)
    bi = p.add("bendin", 200, 40); be = p.add("expr ($f1 - 8192) / 8192", 200, 70)
    bm = p.msg("bend $1", 200, 100); s2 = p.add("s p8-in", 200, 130)
    p.con(bi, 0, be, 0); p.con(be, 0, bm, 0); p.con(bm, 0, s2, 0)
    pg = p.add("pgmin", 450, 40); mi = p.add("- 1", 450, 70); s3 = p.add("s p8-preset", 450, 100)
    p.con(pg, 0, mi, 0); p.con(mi, 0, s3, 0)
    ci = p.add("ctlin", 10, 170); cp = p.add("pack f f", 10, 200); sw = p.msg("$2 $1", 10, 230)
    p.con(ci, 0, cp, 0); p.con(ci, 1, cp, 1); p.con(cp, 0, sw, 0)
    rt = p.add("route " + " ".join(str(CC0 + j) for j in range(8)), 10, 260)
    p.con(sw, 0, rt, 0)
    sc = p.add("s p8-in", 10, 460)
    for j in range(8):
        x = 10 + j * 95
        if CTL[j] == "intv":
            q = p.add("expr int($f1 * 12 / 127 + 0.5)", x, 300)
            p.con(rt, j, q, 0); src = q
        else:
            src = None
        m = p.msg(f"ctl {j} $1", x, 400 if src is None else 330)
        p.con(rt if src is None else src, j if src is None else 0, m, 0)
        p.con(m, 0, sc, 0)
    p.note("intv: 0-127 -> 0-12 semitones", 10, 490)
    return p


def sec_pots():
    p = P("pots", 900, 460)
    p.note("POTS + CV: adc~ 3-10 every 10 ms, scaled to 0-127 (range of every pot measured on a Pepper) -> controls 0-7."
           " Empty jack: the pot sets the value. Cable in: the pot attenuates the CV.", 10, 6)
    mt = p.add("loadbang", 10, 30); me = p.add("metro 10", 80, 30); st = p.add("s p8-tick", 160, 30)
    p.con(mt, 0, me, 0); p.con(me, 0, st, 0)
    for j in range(8):
        x, y = 10 + j * 110, 70
        p.note(f"pot {j + 1}: {CTL[j]}", x, y)
        rt = p.add("r p8-tick", x + 40, y + 20)
        ad = p.add(f"adc~ {3 + j}", x, y + 45)
        sn = p.add("snapshot~", x, y + 70)
        lo = p.add(f"- {POT_LO[j]:g}", x, y + 95)
        sc = p.add("* %.4f" % (127.0 / (POT_HI[j] - POT_LO[j])), x, y + 120)
        cl = p.add("clip 0 127", x, y + 145)
        it = p.add("i", x, y + 170)
        db = p.add("p8pot", x, y + 195)
        p.con(ad, 0, sn, 0); p.con(rt, 0, sn, 0); p.con(sn, 0, lo, 0); p.con(lo, 0, sc, 0)
        p.con(sc, 0, cl, 0); p.con(cl, 0, it, 0); p.con(it, 0, db, 0)
        src = db
        if CTL[j] == "intv":
            q = p.add("expr int($f1*12/127+0.5)", x, y + 220)
            p.con(db, 0, q, 0); src = q
        m = p.msg(f"ctl {j} $1", x, y + 250)
        s = p.add("s p8-in", x, y + 275)
        p.con(src, 0, m, 0); p.con(m, 0, s, 0)
    return p


def sec_presets():
    p = P("presets", 620, 360)
    p.note("PRESETS: p8-preset N loads factory program N (0-63) / button 1 = down / button 2 = up", 10, 6)
    r = p.add("r p8-preset", 10, 40)
    w = p.add("mod 64", 10, 65)
    t = p.add("t f f f", 10, 90)
    v = p.add("v p8-prog", 200, 120)
    pr = p.add("print preset", 120, 120)
    fa = p.add("p8factory", 10, 150)
    s = p.add("s p8-in", 10, 180)
    p.con(r, 0, w, 0); p.con(w, 0, t, 0); p.con(t, 2, v, 0); p.con(t, 1, pr, 0); p.con(t, 0, fa, 0); p.con(fa, 0, s, 0)
    for k, (pin, d) in enumerate([(BTN_DOWN, 63), (BTN_UP, 1)]):
        x = 10 + k * 200
        rb = p.add(f"r bela_digitalIn{pin}", x, 230)
        ch = p.add("change", x, 255)
        se = p.add("sel 0", x, 280)
        vv = p.add("v p8-prog", x, 305)
        ad = p.add(f"+ {d}", x + 70, 305)
        ss = p.add("s p8-preset", x, 330)
        p.con(rb, 0, ch, 0); p.con(ch, 0, se, 0); p.con(se, 0, vv, 0); p.con(vv, 0, ad, 0); p.con(ad, 0, ss, 0)
    p.note(f"button 1 (pin {BTN_DOWN}) down / button 2 (pin {BTN_UP}) up / pressed = 0", 300, 40)
    return p


def main_patch():
    p = P(None, 640, 420)
    lines = ["pdoly8~ on the Bela Pepper - minimal test patch",
             "MIDI (USB): notes / pitch bend / CC 20-27 = controls 0-7 / program change = factory preset",
             "Pots 1-8: har1 har2 env1 env2 intv detn lev1 lev2. With a cable in a CV jack its pot attenuates the CV.",
             "Button 1 / 2: factory preset down / up. Start: preset 0 (program 11)",
             "A pot only takes over after it moved (deadband) - so a preset stays until you touch a pot."]
    for k, l in enumerate(lines):
        p.note(l, 10, 6 + 15 * k)
    y = 95
    for sec, txt in [(sec_init(), "buttons as inputs / preset 0 at start"), (sec_midi(), "notes / bend / CC / program"),
                     (sec_pots(), "8 pots + CV -> controls 0-7"), (sec_presets(), "factory presets / buttons")]:
        p.sub(sec, 10, y)
        p.note(txt, 120, y)
        y += 25
    r = p.add("r p8-in", 10, y + 20)
    o = p.add("pdoly8~", 10, y + 50)
    d = p.add("dac~ 1 2", 10, y + 85)
    so = p.add("s p8-out", 150, y + 85)
    p.con(r, 0, o, 0); p.con(o, 0, d, 0); p.con(o, 1, d, 1); p.con(o, 2, so, 0)
    return p


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
    open(os.path.join(OUT, "_main.pd"), "w").write("\n".join(main_patch().lines(top=True)) + "\n")
    open(os.path.join(OUT, "p8pot.pd"), "w").write("\n".join(p8pot().lines(top=True)) + "\n")
    txt, n = p8factory_embedded()
    open(os.path.join(OUT, "p8factory.pd"), "w").write(txt)
    shutil.rmtree(os.path.join(OUT, "presets"), ignore_errors=True)
    print(f"bela/project: _main.pd, p8pot.pd, p8factory.pd ({n} programs inside)")
