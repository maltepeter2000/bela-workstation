#!/usr/bin/env python3
"""Generates bela/juno/_main.pd: juno~ as a Pd project on Bela (made for the Bela Gem), no GUI.

usage: python3 tools/gen_juno_bela.py [outfile]

- [juno~] on the bus juno / juno-out, channel 1, audio on outputs 1 / 2
- MIDI from Bela (Bela gives Pd parsed MIDI): notes, CC, bend, program change and pressure are put
  back together as bytes for juno~'s raw MIDI inlet; notes on the pad channel stay out
- [junocc 1]: the controller layer, CC pages from 20, with feedback to the controller
- [junopads 10]: the Launchkey's pads in Drum mode
- outlet 3 floats -> [midiout]: SysEx and MIDI thru (both off at load: the first MIDI port is
  the keyboard)
- OLED via O2O (/display-strings-and-numbers to 127.0.0.1:7562) and the Launchkey's display
  ([lkdisplay], custom firmware): juno~'s display line
- OSC on UDP 7563: /juno/<message> <args> goes to juno~ as <message> <args>
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import patchwriter as G      # noqa: E402

PAD_CH = 10
OSC_PORT = 7563


def midi_sub():
    p = G.P("midi", 900, 420)
    p.note("MIDI from Bela -> raw bytes for juno~ (inlet 3). Bela passes MIDI to Pd parsed, so it is "
           f"put back together here. Channels of a second port (17-32) fold to 1-16. Notes on channel "
           f"{PAD_CH} belong to the pads.", 10, 10)
    s = []
    # notes: note, velocity, channel
    ni = p.add("notein", 10, 60)
    pk = p.add("pack f f f", 10, 90); p.con(ni, 0, pk, 0); p.con(ni, 1, pk, 1); p.con(ni, 2, pk, 2)
    ex = p.add(f"expr if(($f3 - 1) % 16 == {PAD_CH - 1}, -1, 144 + ($f3 - 1) % 16); $f1; $f2", 10, 120)
    p.con(pk, 0, ex, 0)
    pb = p.add("pack f f f", 10, 150)
    p.con(ex, 0, pb, 0); p.con(ex, 1, pb, 1); p.con(ex, 2, pb, 2)
    rt = p.add("route -1", 10, 180); p.con(pb, 0, rt, 0)
    s.append((rt, 1))
    # controllers: value, number, channel
    ci = p.add("ctlin", 330, 60)
    cp = p.add("pack f f f", 330, 90); p.con(ci, 0, cp, 0); p.con(ci, 1, cp, 1); p.con(ci, 2, cp, 2)
    ce = p.add("expr 176 + ($f3 - 1) % 16; $f2; $f1", 330, 120); p.con(cp, 0, ce, 0)
    cb = p.add("pack f f f", 330, 150)
    p.con(ce, 0, cb, 0); p.con(ce, 1, cb, 1); p.con(ce, 2, cb, 2)
    s.append((cb, 0))
    # bend: 0..16383, channel
    bi = p.add("bendin", 560, 60)
    bp = p.add("pack f f", 560, 90); p.con(bi, 0, bp, 0); p.con(bi, 1, bp, 1)
    be = p.add("expr 224 + ($f2 - 1) % 16; $f1 % 128; int($f1 / 128)", 560, 120); p.con(bp, 0, be, 0)
    bb = p.add("pack f f f", 560, 150)
    p.con(be, 0, bb, 0); p.con(be, 1, bb, 1); p.con(be, 2, bb, 2)
    s.append((bb, 0))
    # program change 1..128, channel
    gi = p.add("pgmin", 10, 240)
    gp = p.add("pack f f", 10, 270); p.con(gi, 0, gp, 0); p.con(gi, 1, gp, 1)
    ge = p.add("expr 192 + ($f2 - 1) % 16; $f1 - 1", 10, 300); p.con(gp, 0, ge, 0)
    gb = p.add("pack f f", 10, 330); p.con(ge, 0, gb, 0); p.con(ge, 1, gb, 1)
    s.append((gb, 0))
    # channel pressure
    ti = p.add("touchin", 330, 240)
    tp = p.add("pack f f", 330, 270); p.con(ti, 0, tp, 0); p.con(ti, 1, tp, 1)
    te = p.add("expr 208 + ($f2 - 1) % 16; $f1", 330, 300); p.con(tp, 0, te, 0)
    tb = p.add("pack f f", 330, 330); p.con(te, 0, tb, 0); p.con(te, 1, tb, 1)
    s.append((tb, 0))
    out = p.add("s juno-raw", 560, 370)
    for o, k in s:
        p.con(o, k, out, 0)
    return p


def oled_sub():
    p = G.P("oled", 700, 420)
    p.note("OLED via O2O (127.0.0.1:7562), one line: juno~'s display title and value - a turned "
           "control (VCF freq 64), the patch that plays (Preset 12 Brass), Stored User 11. At most "
           "every 50 ms. The same goes to the Launchkey's display (lkdisplay, main patch).", 10, 10)
    ro = p.add("r juno-out", 10, 60)
    rd = p.add("route display", 10, 90); p.con(ro, 0, rd, 0)
    # title and value -> one line "title value"
    li = p.add("list", 200, 90); p.con(rd, 0, li, 0)      # title arrives as the selector
    tl = p.add("t l l", 10, 115); p.con(li, 0, tl, 0)
    sv = p.add("list split 1", 120, 140); p.con(tl, 1, sv, 0)
    fv = p.add("list fromsymbol", 120, 165); p.con(sv, 1, fv, 0)
    pv = p.add("list prepend 32", 120, 190); p.con(fv, 0, pv, 0)
    st = p.add("list split 1", 10, 140); p.con(tl, 0, st, 0)
    ft = p.add("list fromsymbol", 10, 165); p.con(st, 0, ft, 0)
    ap = p.add("list append", 10, 190); p.con(ft, 0, ap, 0); p.con(pv, 0, ap, 1)
    sy = p.add("list tosymbol", 10, 215); p.con(ap, 0, sy, 0)
    line = p.add("symbol", 10, 240); p.con(sy, 0, line, 1)
    one = p.msg("1", 200, 120); p.con(rd, 0, one, 0)
    lb = p.add("loadbang", 200, 60)
    mt = p.add("metro 50", 300, 90); p.con(lb, 0, mt, 0)
    gate = p.add("spigot", 300, 150); p.con(mt, 0, gate, 0); p.con(one, 0, gate, 1)
    tb = p.add("t b b", 300, 180); p.con(gate, 0, tb, 0)
    zero = p.msg("0", 400, 210); p.con(tb, 1, zero, 0); p.con(zero, 0, gate, 1)
    p.con(tb, 0, line, 0)
    ls = p.add("list", 10, 270); p.con(line, 0, ls, 0)
    of = p.add("oscformat display-strings-and-numbers", 10, 300); p.con(ls, 0, of, 0)
    ps = p.add("list prepend send", 10, 330); p.con(of, 0, ps, 0)
    lt = p.add("list trim", 10, 360); p.con(ps, 0, lt, 0)
    ns = p.add("netsend -u -b", 250, 360); p.con(lt, 0, ns, 0)
    cm = p.msg("connect 127.0.0.1 7562", 450, 300); p.con(lb, 0, cm, 0); p.con(cm, 0, ns, 0)
    init = p.msg("symbol Juno", 450, 120); p.con(lb, 0, init, 0)
    p.con(init, 0, line, 1); p.con(init, 0, one, 0)
    return p


def remote_sub():
    p = G.P("remote", 600, 260)
    p.note(f"OSC on UDP {OSC_PORT}: /juno/<message> <args> -> juno~ (e.g. /juno/preset 12, "
           "/juno/cutoff 64, /juno/n res 100, /juno/thru 1, /juno/sysexout 1).", 10, 10)
    nr = p.add(f"netreceive -u -b {OSC_PORT}", 10, 60)
    op = p.add("oscparse", 10, 90); p.con(nr, 0, op, 0)
    lt = p.add("list trim", 10, 120); p.con(op, 0, lt, 0)
    rt = p.add("route juno", 10, 150); p.con(lt, 0, rt, 0)
    l2 = p.add("list trim", 10, 180); p.con(rt, 0, l2, 0)
    st = p.add("s juno", 10, 210); p.con(l2, 0, st, 0)
    return p


def build():
    top = G.P(None, 820, 460)
    top.note("Juno on Bela: juno~ (Juno-60 engine, Juno-106 patches) - MIDI in, controller layer "
             f"(junocc, CC 20 on, channel 1), pads (junopads, channel {PAD_CH}), OLED, OSC remote "
             f"(UDP {OSC_PORT}). juno~ and its abstractions come from the folder bela-workstation "
             "(branch pd, git pull) in pd-externals; here only junouser-bank.syx (the user bank, made "
             "on the first save).", 10, 10)
    top.add("declare -path ../pd-externals/bela-workstation", 600, 60)
    lb = top.add("loadbang", 300, 60)
    st = top.msg("channel 1, thru 0, sysexout 0", 300, 90); top.con(lb, 0, st, 0)
    r = top.add("r juno", 10, 70)
    rr = top.add("r juno-raw", 150, 70)
    j = top.add("juno~", 10, 130)
    top.con(r, 0, j, 0); top.con(st, 0, j, 0); top.con(rr, 0, j, 2)
    d = top.add("dac~ 1 2", 10, 180); top.con(j, 0, d, 0); top.con(j, 1, d, 1)
    so = top.add("s juno-out", 200, 180); top.con(j, 2, so, 0)
    rf = top.add("route float", 330, 180); top.con(j, 2, rf, 0)
    mo = top.add("midiout", 330, 240); top.con(rf, 0, mo, 0)
    lk = top.add("lkdisplay", 420, 210); top.con(rf, 1, lk, 0); top.con(lk, 0, mo, 0)
    top.note("midiout, the first MIDI port (the Launchkey): its display (lkdisplay), SysEx "
             "(sysexout 1) and MIDI thru (thru 1)", 420, 240)
    ro = top.add("r juno-out", 10, 260)
    cc = top.add("junocc 1", 10, 290); top.con(ro, 0, cc, 0)
    sc = top.add("s juno", 10, 320); top.con(cc, 0, sc, 0)
    pd_ = top.add(f"junopads {PAD_CH}", 150, 290)
    sp = top.add("s juno", 150, 320); top.con(pd_, 0, sp, 0)
    top.sub(midi_sub(), 10, 380)
    top.sub(oled_sub(), 110, 380)
    top.sub(remote_sub(), 210, 380)
    return "\n".join(top.lines(top=True)) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "bela", "juno", "_main.pd")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w").write(build())
    print("wrote", out)
