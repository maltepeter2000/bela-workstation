#!/usr/bin/env python3
"""[lkdisplay]: an instrument's display line -> the Launchkey's display.

The Launchkey custom firmware (maltepeter2000/launchkey_firmware, cfw/lk_text.c) shows two lines
in its popup when this SysEx arrives on its MIDI port:

    F0 7D 4C 4B 01 <title> 00 <value> F7     (7D = non-commercial ID, "LK", 01 = popup)

Inlet: display <title> <value> (what juno~ says on outlet 3; everything else is ignored).
Outlet: the bytes, one float each, for [midiout]. Characters outside ASCII become '?'.
At most one message per THROTTLE ms: the first at once, then the newest one at the end of the
interval. A display needs no more, and on Bela every MIDI byte is a system call in the audio
thread (one message per encoder tick caused dropouts at small block sizes).

usage: python3 tools/lkdisplay.py <out.pd>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import patchwriter as G   # noqa: E402

HEADER = [0xF0, 0x7D, 0x4C, 0x4B, 0x01]
THROTTLE = 40


def build():
    p = G.P(None, 760, 560)
    p.note("lkdisplay: display <title> <value> -> SysEx F0 7D 4C 4B 01 title 00 value F7 for [midiout]: "
           "the Launchkey custom firmware shows the two lines in its popup.", 10, 6)
    inl = p.add("inlet", 10, 50)
    rd = p.add("route display", 10, 75); p.con(inl, 0, rd, 0)
    # throttle: keep the newest line; send it now when the gate is open, else when it opens
    keep = p.add("list", 420, 75); p.con(rd, 0, keep, 1)          # newest line (cold)
    tb = p.add("t b b", 420, 100); p.con(rd, 0, tb, 0)
    pend = p.msg("1", 520, 125); p.con(tb, 1, pend, 0)
    gate = p.add("spigot 1", 420, 150); p.con(tb, 0, gate, 0)     # open = no message for THROTTLE ms
    fire = p.add("t b b b", 420, 180); p.con(gate, 0, fire, 0)
    close = p.msg("0", 560, 210); p.con(fire, 1, close, 0); p.con(close, 0, gate, 1)
    clr = p.msg("0", 600, 180); p.con(fire, 0, clr, 0)
    p.con(fire, 2, keep, 0)
    dl = p.add(f"del {THROTTLE}", 600, 240); p.con(fire, 0, dl, 0)
    td = p.add("t b b", 600, 265); p.con(dl, 0, td, 0)           # open first, then look
    opn = p.msg("1", 560, 295); p.con(td, 1, opn, 0); p.con(opn, 0, gate, 1)
    chk = p.add("f", 660, 295); p.con(td, 0, chk, 0)              # 1 = a line is waiting
    p.con(pend, 0, chk, 1); p.con(clr, 0, chk, 1)
    sel = p.add("sel 1", 660, 325); p.con(chk, 0, sel, 0); p.con(sel, 0, gate, 0)
    ls = p.add("list", 10, 100); p.con(keep, 0, ls, 0)
    tl = p.add("t l l", 10, 125); p.con(ls, 0, tl, 0)
    # value (first): 00 value F7
    sv = p.add("list split 1", 200, 155); p.con(tl, 1, sv, 0)
    rv = p.add("route float", 200, 185); p.con(sv, 1, rv, 0)       # a number as text
    mv = p.add("makefilename %g", 200, 210); p.con(rv, 0, mv, 0)
    fv = p.add("list fromsymbol", 280, 185); p.con(rv, 1, fv, 0); p.con(mv, 0, fv, 0)
    pv = p.add("list prepend 0", 280, 210); p.con(fv, 0, pv, 0)
    av = p.add("list append 247", 280, 235); p.con(pv, 0, av, 0)
    # title: header title, then the value part
    st = p.add("list split 1", 10, 155); p.con(tl, 0, st, 0)
    ft = p.add("list fromsymbol", 10, 185); p.con(st, 0, ft, 0)
    pt = p.add("list prepend " + " ".join(map(str, HEADER)), 10, 210); p.con(ft, 0, pt, 0)
    ap = p.add("list append", 10, 265); p.con(pt, 0, ap, 0); p.con(av, 0, ap, 1)
    # one byte after the other; bytes > 127 inside the text -> '?'
    t2 = p.add("t l l", 10, 295); p.con(ap, 0, t2, 0)
    stor = p.add("list store", 200, 420); p.con(t2, 1, stor, 1)
    ln = p.add("list length", 10, 325); p.con(t2, 0, ln, 0)
    tfb = p.add("t f f b", 10, 350); p.con(ln, 0, tfb, 0)
    last = p.add("- 1", 120, 380); p.con(tfb, 1, last, 0)
    zero = p.msg("0", 200, 350); p.con(tfb, 2, zero, 0)
    un = p.add("until", 10, 380); p.con(tfb, 0, un, 0)
    cnt = p.add("f", 10, 410); p.con(un, 0, cnt, 0); p.con(zero, 0, cnt, 1)
    inc = p.add("+ 1", 50, 410); p.con(cnt, 0, inc, 0); p.con(inc, 0, cnt, 1)
    tff = p.add("t f f", 10, 440); p.con(cnt, 0, tff, 0)
    get = p.msg("get $1 1", 10, 470); p.con(tff, 0, get, 0); p.con(get, 0, stor, 0)
    ex = p.add("expr if($f1 > 127 && $f2 > 0 && $f2 < $f3, 63, $f1)", 200, 470)
    p.con(stor, 0, ex, 0); p.con(tff, 1, ex, 1); p.con(last, 0, ex, 2)
    out = p.add("outlet", 200, 510); p.con(ex, 0, out, 0)
    return "\n".join(p.lines(top=True)) + "\n"


if __name__ == "__main__":
    open(sys.argv[1], "w").write(build())
    print("wrote", sys.argv[1])
