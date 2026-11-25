#!/usr/bin/env python3
"""Generates rack/bela/rack4/_main.pd: juno~, d50~, cz4~ and pdoly8~ in one Pd patch on the Bela.

A first, simple rack: all four run at once, each on its own MIDI channel, mixed to outputs 1/2.

  channel 1 juno~   channel 2 d50~   channel 3 cz4~   channel 4 pdoly8~
  (change the Launchkey's Keys Channel to play another one)

- MIDI from Bela comes parsed; the bytes are put back together for juno~, d50~ and cz4~ (raw
  MIDI inlet; each takes only its channel), pdoly8~ gets note <pitch> <vel> <ch> (ch1 4).
- Controller layers: junocc 1, d50cc 2, cz4cc 3, p8cc 4. The encoders (mode Custom 1) send on
  the Keys Channel, so they control the instrument you play; page 1 is the same everywhere.
- Pads (Drum mode, channel 10): the four pad abstractions all listen; only the one of the
  instrument in focus passes on. Focus = the channel of the last note on channels 1-4. A focus
  change shows the instrument on the Launchkey and sends its pad colours.
- Display: the lines of all four go to one [lkdisplay].
- Levels: trims from docs/levels.md (juno +3.9 dB, pdoly8 +2.5 dB; d50, cz4 not measured: 0 dB),
  then the sum x 0.5 (-6 dB headroom for four at once).

Files in the project folder: _main.pd, d50_pcm.bin (from your ROMs, for d50~), optionally the
user banks (made on the first save) and a CZ factory bank for cz4~. The externals and
abstractions come from pd-externals (the branch pd).

usage: python3 rack/gen_rack.py [out.pd]
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "common", "tools"))
import patchwriter as G   # noqa: E402

PAD_CH = 10
# name, object, channel, layer, pads, trim (linear), raw MIDI (else note lists)
INST = [
    ("juno", "juno~", 1, "junocc", "junopads", 1.57, True),
    ("d50", "d50~", 2, "d50cc", "d50pads", 1.0, True),
    ("cz4", "cz4~", 3, "cz4cc", "cz4pads", 1.0, True),
    ("p8", "pdoly8~", 4, "p8cc", "p8pads", 1.33, False),
]
TITLE = {"juno": "Juno", "d50": "D-50", "cz4": "CZ", "p8": "Poly-800"}
MASTER = 0.5


def midi_sub():
    """parsed MIDI -> raw bytes on [s rack-raw] (not channel 10: the pads), notes on [s rack-note]"""
    p = G.P("midi", 900, 420)
    p.note("MIDI from Bela -> raw bytes for juno~ d50~ cz4~ (rack-raw) and note lists (rack-note). "
           f"Channels of a second port (17-32) fold to 1-16. Notes on channel {PAD_CH} belong to "
           "the pads.", 10, 10)
    s = []
    ni = p.add("notein", 10, 60)
    pk = p.add("pack f f f", 10, 90); p.con(ni, 0, pk, 0); p.con(ni, 1, pk, 1); p.con(ni, 2, pk, 2)
    sn = p.add("s rack-note", 200, 90); p.con(pk, 0, sn, 0)
    ex = p.add(f"expr if(($f3 - 1) % 16 == {PAD_CH - 1}, -1, 144 + ($f3 - 1) % 16); $f1; $f2", 10, 120)
    p.con(pk, 0, ex, 0)
    pb = p.add("pack f f f", 10, 150)
    p.con(ex, 0, pb, 0); p.con(ex, 1, pb, 1); p.con(ex, 2, pb, 2)
    rt = p.add("route -1", 10, 180); p.con(pb, 0, rt, 0)
    s.append((rt, 1))
    ci = p.add("ctlin", 330, 60)
    cp = p.add("pack f f f", 330, 90); p.con(ci, 0, cp, 0); p.con(ci, 1, cp, 1); p.con(ci, 2, cp, 2)
    ce = p.add("expr 176 + ($f3 - 1) % 16; $f2; $f1", 330, 120); p.con(cp, 0, ce, 0)
    cb = p.add("pack f f f", 330, 150)
    p.con(ce, 0, cb, 0); p.con(ce, 1, cb, 1); p.con(ce, 2, cb, 2)
    s.append((cb, 0))
    bi = p.add("bendin", 560, 60)
    bp = p.add("pack f f", 560, 90); p.con(bi, 0, bp, 0); p.con(bi, 1, bp, 1)
    sb = p.add("s rack-bend", 700, 90); p.con(bp, 0, sb, 0)
    be = p.add("expr 224 + ($f2 - 1) % 16; $f1 % 128; int($f1 / 128)", 560, 120); p.con(bp, 0, be, 0)
    bb = p.add("pack f f f", 560, 150)
    p.con(be, 0, bb, 0); p.con(be, 1, bb, 1); p.con(be, 2, bb, 2)
    s.append((bb, 0))
    gi = p.add("pgmin", 10, 240)
    gp = p.add("pack f f", 10, 270); p.con(gi, 0, gp, 0); p.con(gi, 1, gp, 1)
    ge = p.add("expr 192 + ($f2 - 1) % 16; $f1 - 1", 10, 300); p.con(gp, 0, ge, 0)
    gb = p.add("pack f f", 10, 330); p.con(ge, 0, gb, 0); p.con(ge, 1, gb, 1)
    s.append((gb, 0))
    out = p.add("s rack-raw", 560, 370)
    for o, k in s:
        p.con(o, k, out, 0)
    return p


def focus_sub():
    """the instrument in focus: the channel of the last note on 1-4 -> rack-focus 1..4"""
    p = G.P("focus", 700, 340)
    p.note("Focus: the channel of the last note on channels 1-4 (the Launchkey's Keys Channel) -> "
           "rack-focus 1-4: only that instrument's pads pass on; a change shows its name and "
           "sends its pad colours.", 10, 10)
    rn = p.add("r rack-note", 10, 60)
    un = p.add("unpack f f f", 10, 90); p.con(rn, 0, un, 0)
    ch = p.add("expr ($f1 - 1) % 16 + 1", 10, 120); p.con(un, 2, ch, 0)
    mo = p.add("moses 5", 10, 150); p.con(ch, 0, mo, 0)
    ch_ = p.add("change", 10, 180); p.con(mo, 0, ch_, 0)
    sf = p.add("s rack-focus", 10, 210); p.con(ch_, 0, sf, 0)
    lb = p.add("loadbang", 200, 60)
    one = p.msg("1", 200, 90); p.con(lb, 0, one, 0); p.con(one, 0, ch_, 0)
    rf = p.add("r rack-focus", 300, 150)
    sel = p.add("sel 1 2 3 4", 300, 180); p.con(rf, 0, sel, 0)
    for k, (nm, obj, chn, layer, pads, trim, raw) in enumerate(INST):
        m = p.msg(f"display Instrument {TITLE[nm]}", 300 + k * 100, 220); p.con(sel, k, m, 0)
        sd = p.add("s rack-display", 300 + k * 100, 250); p.con(m, 0, sd, 0)
        lm = p.msg("leds", 300 + k * 100, 280); p.con(sel, k, lm, 0)
        sl = p.add(f"s rack-{nm}-pads", 300 + k * 100, 310); p.con(lm, 0, sl, 0)
    return p


def inst_sub(nm, obj, chn, layer, pads, trim, raw):
    p = G.P(nm, 760, 400)
    p.note(f"{obj} on channel {chn}: layer {layer} {chn}, pads {pads} {PAD_CH} (when in focus), "
           f"trim x{trim}.", 10, 10)
    lb = p.add("loadbang", 10, 50)
    r = p.add(f"r rack-{nm}", 120, 50)
    if raw:
        st = p.msg(f"channel {chn}" + (", thru 0, sysexout 0" if nm == "juno" else ""), 10, 80)
        p.con(lb, 0, st, 0)
    else:
        st = p.msg(f"ch1 {chn}", 10, 80); p.con(lb, 0, st, 0)
    o = p.add(obj, 10, 170)
    p.con(st, 0, o, 0)
    if raw:
        rr = p.add("r rack-raw", 250, 80); p.con(rr, 0, o, 2)
        p.con(r, 0, o, 0)
    else:
        rn = p.add("r rack-note", 250, 50)
        p.con(rn, 0, o, 0)
        rb = p.add("r rack-bend", 400, 50)
        # bendin gives value, channel: keep channel chn only (unpack: channel first)
        ub = p.add("unpack f f", 400, 80); p.con(rb, 0, ub, 0)
        ec = p.add(f"expr ($f1 - 1) % 16 + 1 == {chn}", 520, 80); p.con(ub, 1, ec, 0)
        gb = p.add("spigot", 400, 110); p.con(ub, 0, gb, 0); p.con(ec, 0, gb, 1)
        bx = p.add("expr ($f1 - 8192) / 8192", 520, 140); p.con(gb, 0, bx, 0)
        bm = p.msg("bend $1", 520, 165); p.con(bx, 0, bm, 0); p.con(bm, 0, o, 0)
        nav = p.add("p8nav", 120, 110); p.con(r, 0, nav, 0); p.con(nav, 0, o, 0)
        p.con(o, 2, nav, 1)
    # audio: trim -> outputs 1/2 (through rack-l / rack-r)
    tl = p.add(f"*~ {trim}", 10, 220); tr = p.add(f"*~ {trim}", 90, 220)
    p.con(o, 0, tl, 0); p.con(o, 1, tr, 0)
    sl = p.add("s~ " + f"rack-{nm}-l", 10, 250); sr = p.add("s~ " + f"rack-{nm}-r", 120, 250)
    p.con(tl, 0, sl, 0); p.con(tr, 0, sr, 0)
    # messages out: layer feedback, display
    ly = p.add(f"{layer} {chn}", 300, 220)
    p.con(o, 2, ly, 0)
    sy = p.add(f"s rack-{nm}", 300, 250); p.con(ly, 0, sy, 0)
    sd = p.add("s rack-display", 450, 250)
    if raw:
        p.con(o, 2, sd, 0)
    else:
        p.con(nav, 1, sd, 0)
    # pads: only in focus
    pa = p.add(f"{pads} {PAD_CH}", 300, 290)
    rl = p.add(f"r rack-{nm}-pads", 300, 260); p.con(rl, 0, pa, 0)
    sp = p.add("spigot", 300, 320); p.con(pa, 0, sp, 0)
    rf = p.add("r rack-focus", 420, 290)
    eq = p.add(f"== {[i[0] for i in INST].index(nm) + 1}", 420, 320)
    p.con(rf, 0, eq, 0); p.con(eq, 0, sp, 1)
    sp2 = p.add(f"s rack-{nm}", 300, 350); p.con(sp, 0, sp2, 0)
    spd = p.add("spigot", 450, 320); p.con(pa, 1, spd, 0); p.con(eq, 0, spd, 1)
    p.con(spd, 0, sd, 0)                       # the pads' own display lines (own pads, init armed)
    return p


def build():
    top = G.P(None, 900, 520)
    top.note("Rack on Bela: juno~ (channel 1), d50~ (2), cz4~ (3), pdoly8~ (4) at once, mixed to "
             "outputs 1/2. Play another one by changing the Launchkey's Keys Channel; the encoders "
             "(Custom 1) follow, the pads (Drum mode, channel 10) go to the instrument of the last "
             "note. Generated by rack/gen_rack.py. Externals and abstractions from pd-externals "
             "(branch pd); here d50_pcm.bin and the user banks.", 10, 10)
    top.add("declare -path ../pd-externals/bela-workstation", 600, 90)
    y = 120
    for k, inst in enumerate(INST):
        top.sub(inst_sub(*inst), 10 + k * 110, y)
    top.sub(midi_sub(), 10, y + 40)
    top.sub(focus_sub(), 120, y + 40)
    # mix
    for k, inst in enumerate(INST):
        top.add(f"r~ rack-{inst[0]}-l", 10 + k * 120, 220)
        top.add(f"r~ rack-{inst[0]}-r", 10 + k * 120, 245)
    # sum: every r~ into the master gains
    gl = top.add(f"*~ {MASTER}", 10, 300)
    gr = top.add(f"*~ {MASTER}", 120, 300)
    d = top.add("dac~ 1 2", 10, 340)
    top.con(gl, 0, d, 0); top.con(gr, 0, d, 1)
    # display: all instruments -> one lkdisplay -> midiout
    rd = top.add("r rack-display", 300, 300)
    lk = top.add("lkdisplay", 300, 330); top.con(rd, 0, lk, 0)
    mo = top.add("midiout", 300, 360); top.con(lk, 0, mo, 0)
    return top


def finish(top):
    # connect every r~ rack-<nm>-l/-r to the master gains
    gains = [i for i, (k, x, y, t) in enumerate(top.o) if k == "obj" and t == f"*~ {MASTER}"]
    assert len(gains) == 2, gains
    gl, gr = gains  # left gain is created first
    for k, x, y, t in list(top.o):
        if k == "obj" and t.startswith("r~ rack-") and t.endswith("-l"):
            top.con(top.o.index((k, x, y, t)), 0, gl, 0)
        if k == "obj" and t.startswith("r~ rack-") and t.endswith("-r"):
            top.con(top.o.index((k, x, y, t)), 0, gr, 0)
    return top


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "bela", "rack4", "_main.pd")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    top = finish(build())
    with open(out, "w") as fh:
        fh.write("\n".join(top.lines(top=True)) + "\n")
    print("wrote", out)
