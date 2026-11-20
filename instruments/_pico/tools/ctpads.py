#!/usr/bin/env python3
"""Control pads: the Launchkey's pads in Drum mode as buttons for an instrument.

With the Launchkey custom firmware (maltepeter2000/launchkey_firmware) the 16 pads send fixed
notes in rows on the Drums Channel (10 at the factory):

    top row     36 37 38 39 40 41 42 43
    bottom row  44 45 46 47 48 49 50 51

write(path, title, table) makes the abstraction [<name> <channel>] (channel 0 or none
= 10): [notein] inside, a pad pressed on that channel -> its message at the outlet (to the
instrument). table: {note: "message"} or {note: ("twice", "message")} for a pad that has to be
pressed twice within a second (for what drops the edits). Inlet: channel <n>.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import patchwriter as G   # noqa: E402

TOP = list(range(36, 44))
BOTTOM = list(range(44, 52))


def write(path, title, table):
    p = G.P(None, 980, 520)
    p.note(f"{title}: control pads - the Launchkey's pads in Drum mode (notes 36-51, top row 36-43) on "
           "channel $1 (0 = 10, the Drums Channel) -> a message to the instrument. Inlet: channel <n>.", 10, 6)
    lb = p.add("loadbang", 10, 40)
    ca = p.add("f $1", 10, 65); p.con(lb, 0, ca, 0)
    cd = p.add("expr if($f1 > 0, $f1, 10)", 10, 90); p.con(ca, 0, cd, 0)
    inl = p.add("inlet", 260, 40)
    rc = p.add("route channel", 260, 65); p.con(inl, 0, rc, 0); p.con(rc, 0, cd, 0)
    ni = p.add("notein", 10, 130)
    pk = p.add("pack f f f", 10, 160)
    p.con(ni, 2, pk, 2); p.con(ni, 1, pk, 1); p.con(ni, 0, pk, 0)
    ok = p.add("expr $f2 > 0 && $f3 == $f4", 10, 190)
    up = p.add("unpack f f f", 10, 215)
    tl = p.add("t l l", 10, 240); p.con(pk, 0, tl, 0)
    p.con(tl, 1, up, 0)
    p.con(up, 0, ok, 0); p.con(up, 2, ok, 2); p.con(up, 1, ok, 1); p.con(cd, 0, ok, 3)
    # the note only when ok: [spigot] on the note
    sp = p.add("spigot", 120, 270); p.con(ok, 0, sp, 1)
    un = p.add("unpack f f f", 120, 245); p.con(tl, 0, un, 0); p.con(un, 0, sp, 0)
    notes = sorted(table)
    sel = p.add("sel " + " ".join(map(str, notes)), 120, 300); p.con(sp, 0, sel, 0)
    out = p.add("outlet", 10, 490)
    for k, nt in enumerate(notes):
        x = 120 + k * 70
        act = table[nt]
        if isinstance(act, tuple):              # twice within a second
            _, msg = act
            t = p.add("t b b", x, 330); p.con(sel, k, t, 0)
            f = p.add("f", x, 360); add = p.add("+ 1", x + 40, 360)
            p.con(t, 1, f, 0); p.con(f, 0, add, 0); p.con(add, 0, f, 1)
            ge = p.add("sel 2", x, 390); p.con(add, 0, ge, 0)
            dl = p.add("del 1000", x + 40, 330); p.con(t, 0, dl, 0)
            z = p.msg("0", x + 40, 420); p.con(dl, 0, z, 0); p.con(ge, 0, z, 0); p.con(z, 0, f, 1)
            m = p.msg(msg, x, 450); p.con(ge, 0, m, 0)
        else:
            m = p.msg(act, x, 330 + (k % 2) * 30); p.con(sel, k, m, 0)
        p.con(m, 0, out, 0)
    with open(path, "w") as fh:
        fh.write("\n".join(p.lines(top=True)) + "\n")
    return {nt: (a[1] + " (2x)" if isinstance(a, tuple) else a) for nt, a in table.items()}
