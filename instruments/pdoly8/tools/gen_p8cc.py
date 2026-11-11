#!/usr/bin/env python3
"""Generates p8cc.pd: MIDI CC and 14-bit NRPN -> "n <name> <0-127>" for pdoly8~.

The CC table (CC_GROUPS / CC_EXTRA in gen_bela.py) is stored inside the abstraction.
Used by all three versions: desktop (help patch), Bela Pepper and Bela with OLED.
  [ctlin] inside (any channel); inlet: "<cc> <value>" lists from elsewhere.
  NRPN: CC 99 = 0, CC 98 = the parameter's CC number, CC 6 = MSB, CC 38 = LSB
  (CC 38 is the LSB only right after a CC 6, otherwise it is its own setting).
  Outlet: n <name> <value> (fractions with NRPN) -> pdoly8~
usage: python3 tools/gen_p8cc.py   (writes p8cc.pd and copies it into bela/project, bela-oled/project)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import gen_bela as G                             # noqa: E402

P = G.P


def p8cc():
    p = P(None, 760, 560)
    p.note("p8cc: MIDI CC and 14-bit NRPN -> n <name> <0-127> for pdoly8~ (table below, stored in this patch)."
           " NRPN: CC 99 = 0 / CC 98 = the parameter's CC number / CC 6 = MSB / CC 38 = LSB (only right after a CC 6)."
           " Inlet: <cc> <value> lists from elsewhere.", 10, 6)
    ci = p.add("ctlin", 10, 60); pk2 = p.add("pack f f", 10, 85); sw = p.msg("$2 $1", 10, 110)
    p.con(ci, 1, pk2, 1); p.con(ci, 0, pk2, 0); p.con(pk2, 0, sw, 0)
    inl = p.add("inlet", 120, 85)
    rt = p.add("route 98 6 38 99", 10, 140); p.con(sw, 0, rt, 0); p.con(inl, 0, rt, 0)
    sc = p.add("s $0-set", 600, 175); p.con(rt, 4, sc, 0)
    num = p.add("f", 10, 360); p.con(rt, 0, num, 1)
    ex = p.add("expr ($f1 * 128 + $f2) * 127 / 16383", 150, 330)
    arm = p.add("f", 420, 290); ms = p.add("f", 300, 290)
    t6 = p.add("t f f b b", 150, 210); o1 = p.msg("1", 300, 235); z = p.msg("0", 250, 260)
    p.con(rt, 1, t6, 0); p.con(t6, 3, o1, 0); p.con(o1, 0, arm, 1); p.con(t6, 2, z, 0); p.con(z, 0, ex, 1)
    p.con(t6, 1, ms, 1); p.con(t6, 0, ex, 0)
    t38 = p.add("t b f", 420, 210); p.con(rt, 2, t38, 0)
    lsbv = p.add("f", 520, 360); pv0 = p.add("f", 640, 360)
    p.con(t38, 1, lsbv, 1); p.con(t38, 1, pv0, 1); p.con(t38, 0, arm, 0)
    ta = p.add("t f b", 420, 315); z2 = p.msg("0", 500, 315); p.con(arm, 0, ta, 0); p.con(ta, 1, z2, 0); p.con(z2, 0, arm, 1)
    sel = p.add("sel 1 0", 420, 340); p.con(ta, 0, sel, 0); p.con(sel, 0, lsbv, 0); p.con(sel, 1, pv0, 0)
    tl = p.add("t b f", 520, 385); p.con(lsbv, 0, tl, 0); p.con(tl, 1, ex, 1); p.con(tl, 0, ms, 0); p.con(ms, 0, ex, 0)
    pl = p.msg("38 $1", 640, 385); s38 = p.add("s $0-set", 640, 410); p.con(pv0, 0, pl, 0); p.con(pl, 0, s38, 0)
    tv = p.add("t b f", 150, 360); p.con(ex, 0, tv, 0)
    pv = p.add("pack f f", 10, 385); p.con(tv, 1, pv, 1); p.con(tv, 0, num, 0); p.con(num, 0, pv, 0)
    s5 = p.add("s $0-set", 10, 410); p.con(pv, 0, s5, 0)
    # number value -> n name value
    r = p.add("r $0-set", 10, 440); up = p.add("unpack f f", 10, 465); ap = p.add("list append", 250, 540)
    p.con(r, 0, up, 0); p.con(up, 1, ap, 1)
    ts = p.add("text search $0-cc", 10, 490); mo = p.add("moses 0", 10, 515)
    tg = p.add("text get $0-cc", 60, 540); spl = p.add("list split 1", 60, 565); sy = p.add("symbol", 60, 590)
    p.con(up, 0, ts, 0); p.con(ts, 0, mo, 0); p.con(mo, 1, tg, 0); p.con(tg, 0, spl, 0); p.con(spl, 1, sy, 0)
    p.con(sy, 0, ap, 0)
    pn = p.add("list prepend n", 250, 565); tr = p.add("list trim", 250, 590); o = p.add("outlet", 250, 620)
    p.con(ap, 0, pn, 0); p.con(pn, 0, tr, 0); p.con(tr, 0, o, 0)
    p.add("text define -k $0-cc", 450, 540)
    return p


def write(path):
    out = []
    for ln in p8cc().lines(top=True):
        out.append(ln)
        if ln.endswith("text define -k \\$0-cc;"):
            out.append(G.table_data([f"{cc} {nm}" for cc, nm in sorted(G.CCS.items())]))
    open(path, "w").write("\n".join(out) + "\n")


if __name__ == "__main__":
    write(os.path.join(ROOT, "p8cc.pd"))
    for d in ("bela/project", "bela-oled/project"):
        if os.path.isdir(os.path.join(ROOT, d)):
            shutil.copy(os.path.join(ROOT, "p8cc.pd"), os.path.join(ROOT, d))
    print("p8cc.pd (+ copies in bela/project, bela-oled/project)")
