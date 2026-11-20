#!/usr/bin/env python3
"""Generates bela-oled/project/ : pdoly8~ on a Bela with an OLED (no Pepper, no pots).

  MIDI (USB host port hw:1 and the Bela as USB MIDI device hw:0 of the computer):
    notes, pitch bend, program change 1-64 = factory preset 0-63,
    CC -> parameter: the controller layer p8cc (channel MIDI_CH, CC pages of tools/ctlayer.py),
    changes go back to the controller as CC (ctlout), pickup avoids jumps
  Every value goes through pdoly8~'s  n <name> <0-127>;
  pdoly8~ smooths continuous settings (mrph), the delay glides (dgld 1).
  OLED via O2O (OSC to 127.0.0.1:7562, as in tx81z-pd): pdoly8~ sends the changed setting
  and its value from a thread of its own (oled / oledtext), at most every 40 ms. Nothing
  in the patch prints or touches the network, so the audio thread never mode-switches.

Send/receive names
  p8-in      messages to pdoly8~          p8-out     outlet 3 of pdoly8~
  p8-preset  load factory preset N
usage: python3 tools/gen_bela_oled.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import gen_bela as G                             # noqa: E402  (P, CCS, presets)

OUT = os.path.join(ROOT, "bela-oled", "project")
P = G.P
OLED_HOST, OLED_PORT = "127.0.0.1", 7562


def sec_init():
    p = P("init", 700, 260)
    p.note("INIT: open the Bela's own USB MIDI port (hw:0 = computer), connect to O2O, preset 0", 10, 6)
    lb = p.add("loadbang", 10, 40)
    mi = p.msg("; bela_setMidi hw 0 0 0", 10, 70)
    d = p.add("del 500", 200, 70); z = p.msg("0", 200, 100); sp = p.add("s p8-preset", 200, 125)
    hi = p.msg(f"oled {OLED_HOST} {OLED_PORT}, oledtext pdoly8~ Poly-800", 400, 100); so = p.add("s p8-in", 400, 125)
    p.con(lb, 0, mi, 0); p.con(lb, 0, d, 0); p.con(d, 0, z, 0); p.con(z, 0, sp, 0); p.con(lb, 0, hi, 0); p.con(hi, 0, so, 0)
    return p


def sec_presets():
    p = P("presets", 640, 280)
    p.note("PRESETS: p8-preset N loads factory program N (0-63) and shows it (program 11 ... 88)", 10, 6)
    r = p.add("r p8-preset", 10, 40); w = p.add("mod 64", 10, 65); t = p.add("t f f", 10, 90)
    fa = p.add("p8factory", 10, 120); s = p.add("s p8-in", 10, 145)
    ex = p.add("expr (int($f1 / 8) + 1) * 10 + $f1 % 8 + 1", 200, 120)
    mf = p.add("makefilename %d", 200, 145)
    pk = p.msg("oledtext Program $1", 200, 170); so = p.add("s p8-in", 200, 195)
    p.con(r, 0, w, 0); p.con(w, 0, t, 0); p.con(t, 1, fa, 0); p.con(fa, 0, s, 0)
    p.con(t, 0, ex, 0); p.con(ex, 0, mf, 0); p.con(mf, 0, pk, 0); p.con(pk, 0, so, 0)
    return p


def sec_midi():
    p = P("midi", 900, 460)
    p.note("MIDI: notes / pitch bend / program change / CC pages (controller layer p8cc) -> p8-in", 10, 6)
    ni = p.add("notein", 10, 40); pk = p.add("pack f f f", 10, 70); s1 = p.add("s p8-in", 10, 100)
    p.con(ni, 0, pk, 0); p.con(ni, 1, pk, 1); p.con(ni, 2, pk, 2); p.con(pk, 0, s1, 0)
    bi = p.add("bendin", 200, 40); be = p.add("expr ($f1 - 8192) / 8192", 200, 70)
    bm = p.msg("bend $1", 200, 100); s2 = p.add("s p8-in", 200, 125)
    p.con(bi, 0, be, 0); p.con(be, 0, bm, 0); p.con(bm, 0, s2, 0)
    pg = p.add("pgmin", 450, 40); mi = p.add("- 1", 450, 70); s3 = p.add("s p8-preset", 450, 100)
    p.con(pg, 0, mi, 0); p.con(mi, 0, s3, 0)
    rco = p.add("r p8-out", 10, 140)
    cc = p.add(f"p8cc {G.MIDI_CH}", 10, 170); s4 = p.add("s p8-in", 10, 200)
    p.con(rco, 0, cc, 0); p.con(cc, 0, s4, 0)
    p.note(f"p8cc {G.MIDI_CH}: controller layer - CC pages on channel {G.MIDI_CH} -> n <name> <value>,"
           " changes back to the controller (ctlout), pickup (table inside p8cc.pd)", 100, 170)
    return p


def sec_oled():
    p = P("oled", 900, 420)
    p.note("OLED via O2O: disp <line1> <line2> from pdoly8~ and p8-oled <line1> <line2> -> at most every 40 ms"
           " /display-strings-and-numbers <line1> \\n <line2>", 10, 6)
    ro = p.add("r p8-out", 10, 40); rd = p.add("route disp", 10, 65); ri = p.add("r p8-oled", 150, 65)
    p.con(ro, 0, rd, 0)
    l1 = p.add("list", 10, 80); l2 = p.add("list", 150, 90)
    p.con(rd, 0, l1, 0); p.con(ri, 0, l2, 0)
    tl = p.add("t b l", 10, 100); p.con(l1, 0, tl, 0); p.con(l2, 0, tl, 0)
    hold = p.add("list append", 10, 220); p.con(tl, 1, hold, 1)
    gate = p.add("spigot 1", 100, 130); p.con(tl, 0, gate, 0)
    tg = p.add("t b b b", 100, 155); p.con(gate, 0, tg, 0)
    z = p.msg("0", 230, 180); p.con(tg, 2, z, 0); p.con(z, 0, gate, 1)
    d = p.add("del 40", 160, 180); p.con(tg, 1, d, 0)
    one = p.add("t b b", 160, 205); p.con(d, 0, one, 0)
    o1 = p.msg("1", 230, 230); p.con(one, 1, o1, 0); p.con(o1, 0, gate, 1); p.con(one, 0, hold, 0)
    p.con(tg, 0, hold, 0)
    up = p.add("unpack s s", 10, 275); p.con(hold, 0, up, 0)
    pk = p.add("pack s s s", 10, 330); p.con(up, 0, pk, 0); p.con(up, 1, pk, 2)
    lb = p.add("loadbang", 300, 250); tb = p.add("t b b", 300, 275)
    nl = p.msg("92 110", 300, 300); ts = p.add("list tosymbol", 300, 325)
    p.note("= backslash n (line break for O2O)", 400, 300)
    p.con(lb, 0, tb, 0); p.con(tb, 1, nl, 0); p.con(nl, 0, ts, 0); p.con(ts, 0, pk, 1)
    cn = p.msg(f"connect {OLED_HOST} {OLED_PORT}", 450, 330)
    p.con(tb, 0, cn, 0)
    of = p.add("oscformat display-strings-and-numbers", 10, 360); lp = p.add("list prepend send", 10, 385)
    lt = p.add("list trim", 10, 410); ns = p.add("netsend -u -b", 10, 435)
    p.con(pk, 0, of, 0); p.con(of, 0, lp, 0); p.con(lp, 0, lt, 0); p.con(lt, 0, ns, 0); p.con(cn, 0, ns, 0)
    return p


def sec_tables():
    p = P("tables", 600, 120)
    p.note("TABLES: the CC table is inside p8cc.pd", 10, 6)
    p.add("text define -k p8-cc", 10, 40)
    return p


def main_patch():
    p = P(None, 720, 470)
    lines = ["pdoly8~ on Bela with OLED (O2O) - no Pepper",
             "MIDI: notes / pitch bend / program change 1-64 = factory preset / CC pages on channel 1 -> settings",
             "  (controller layer p8cc: table in README, changes go back to the controller, pickup)",
             "pdoly8~ smooths every continuous setting (mrph ms), the delay glides (dgld 1): no zipper or pops.",
             "OLED: the changed setting and its value, the program on preset changes - sent by pdoly8~ from a",
             "  thread of its own (oled 127.0.0.1 7562), never from the audio thread: no network call, no print.",
             "Audio in 1 = external FM source (fms 2, gain fmig)."]
    for k, l in enumerate(lines):
        p.note(l, 10, 6 + 15 * k)
    y = 110
    for sec, txt in [(sec_init(), "MIDI port / O2O / preset 0 at start"),
                     (sec_presets(), "factory presets"), (sec_midi(), "notes / bend / program / CC (p8cc)")]:
        p.sub(sec, 10, y)
        p.note(txt, 140, y)
        y += 25
    r = p.add("r p8-in", 10, y + 20)
    ai = p.add("adc~ 1", 150, y + 20)
    o = p.add("pdoly8~", 10, y + 50)
    d = p.add("dac~ 1 2", 10, y + 85)
    so = p.add("s p8-out", 150, y + 85)
    p.con(r, 0, o, 0); p.con(ai, 0, o, 1); p.con(o, 0, d, 0); p.con(o, 1, d, 1); p.con(o, 2, so, 0)
    return p


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, "_main.pd"), "w").write("\n".join(main_patch().lines(top=True)) + "\n")
    import gen_p8cc
    gen_p8cc.write(os.path.join(OUT, "p8cc.pd"))
    txt, n = G.p8factory_embedded()
    open(os.path.join(OUT, "p8factory.pd"), "w").write(txt)
    print(f"bela-oled/project: _main.pd, p8cc.pd, p8factory.pd ({n} programs inside)")
