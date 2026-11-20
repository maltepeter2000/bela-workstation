#!/usr/bin/env python3
"""Generate juno~-help.pd. Never edit the help patch by hand: change this
script and run it again. Names, ranges and CC pages come from tools/juno_params.py,
the drawing helpers from tools/gen_help.py (the help of d50~).

    python3 tools/gen_juno_help.py       writes juno~/juno~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ctlayer                                    # noqa: E402
import gen_help as G                              # noqa: E402
import juno_params as T                           # noqa: E402

OBJ = "juno~"
SHORT = "juno"
BUS = f"{G.DOLLAR}0-{SHORT}"
G.BUS = BUS                 # draw_block of gen_help sends to its module's bus
SUBTITLE = "Roland Juno-60 with Juno-106 patches and SysEx"
M, CX, TX, LINE, GAP, MAXCH = G.M, G.CX, G.TX, G.LINE, G.GAP, G.MAXCH
DOLLAR, COMMA = G.DOLLAR, G.COMMA
wrap, nlines, section, draw_block = G.wrap, G.nlines, G.section, G.draw_block

CC = ctlayer.cc_map(T.CC_ROWS)
MAIN = list(T.CC_ROWS[0][1])


def rng(nm):
    return T.CONTROLS[nm][2], T.CONTROLS[nm][3]


def default(nm):
    return str(T.DEFAULTS[nm]) if nm in T.DEFAULTS else "patch"


def helptext(nm):
    return T.CONTROLS[nm][6]


def para(nm):
    c = T.CONTROLS[nm]
    names = f" Values: {', '.join(f'{c[2] + i} {v}' for i, v in enumerate(c[5].split('|')))}." if c[5] else ""
    return f"{nm} ({default(nm)}): {helptext(nm)}.{names}"


INLETS = [
    "1 float: note number. Velocity is taken from inlet 2",
    "  and velocity 0 is a note off.",
    "1 list: note, velocity and, if you like, the MIDI channel.",
    "1 anything: a message, see Messages.",
    "2 float: velocity for the next note, 0 to 127 (default 100).",
    "3 float or list: raw MIDI bytes, as midiin gives them.",
    "3 anything: a message, the same set as inlet 1.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: info, name, 0-127 and value text for every",
    "  change, the patch that plays, its name, the channels.",
    "3 display title value: two lines for a display: a control",
    "  set by a message (VCF freq, 64), the patch that plays",
    "  (Preset 12, Harpsichord 1), a store (Stored, User 11).",
    "3 float: SysEx bytes for midiout.",
]

MESSAGES = [
    "name v: set a control, in the values the panel shows.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "preset n: a factory preset, read only.",
    "user n: a slot of the user bank, 0 to 55.",
    "init: a plain sawtooth patch to start from.",
    "reload: the same patch again, edits dropped.",
    "store n: the playing patch into user slot n, in memory.",
    "  Without n: the user slot chosen or stored last.",
    "presetstep 1 or -1, userstep 1 or -1: the next or previous",
    "  factory preset or user slot, from the one chosen last.",
    "save: store, then userwrite (the pads' save).",
    "cycle name [-1]: the next (previous) value, round and round.",
    "userwrite: write the user bank to junouser-bank.syx.",
    "userread: read it again. Both also take a file name.",
    "usersend: the user bank as SysEx on outlet 3.",
    "presets, users: list the names.",
    "bend f: pitch wheel, -1 to 1, times bend.",
    "mod f: modulation wheel, 0 to 1: vibrato.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback and of",
    "  SysEx, 1 to 16.",
    "sysexout 0/1: send SysEx for every change, see SysEx.",
    "thru 0/1: MIDI thru, on by default. MIDI from inlet 3",
    "  leaves outlet 3 unchanged, notes from inlet 1 go out",
    "  on txchannel. SysEx arriving is passed on as well.",
    "status: report patch, channels and sounding voices.",
    "stop: all notes off.",
    "reset: all notes off at once, chorus emptied.",
]

MIDI = [
    "midiin: raw bytes into inlet 3. juno~ takes notes with",
    "  velocity, bend, program change, CC 1 (vibrato), 7",
    "  (volume), 64 (hold) and Juno-106 SysEx.",
    "midiout: the floats of outlet 3, SysEx patches and dumps.",
    "junocc 1: the CC pages from 20 on, on channel 1.",
    "junopads 10: the Launchkey pads in Drum mode, channel 10.",
    "channel 1 at load: juno~ plays channel 1 only, so the pads",
    "  on channel 10 do not play notes.",
    "lkdisplay: the display line as SysEx for the Launchkey's",
    "  display (custom firmware): the control you turn, the",
    "  patch that plays.",
    "Without midiin: send note lists and bend, mod, sustain.",
]

LAYER = [
    "junocc is the controller layer: MIDI CC from 20 on in pages of 8, "
    "the same pages as for pdoly8~, tx81z~ and d50~. Page 1 is filter and "
    "envelope, page k is row k of the editor. It turns a CC into an n "
    "message and sends every change made elsewhere back to the controller "
    "as CC. Its argument is the MIDI channel, and it follows the channels "
    "juno~ reports.",
    "junoedit is the editor: click it in the object area above. All 33 "
    "controls in rows of 8, the factory presets and the user bank, the "
    "MIDI channels and SysEx. It explains itself in its readme.",
    "junopads are the Launchkey's pads in Drum mode as buttons, on the "
    "Drums Channel (10, the argument). With the Launchkey custom firmware "
    "they send notes 36 to 43 on the top row and 44 to 51 on the bottom "
    "row, left to right.",
    "Pads 1 to 4 (top row): factory +, user +, init (press twice within a "
    "second), chorus (I, II, I+II, off, round and round). Pads 9 to 12 (below "
    "them): factory -, user -, save (into the user slot chosen last, and the "
    "bank file), Juno-106 / Juno-60.",
    "All names are listed at the end of this patch with their CC numbers.",
]

ABOUT = [
    "The engine is a Juno-60: one DCO per voice with sawtooth, pulse, sub "
    "oscillator and noise, the four-position high-pass, the IR3109 "
    "low-pass, one envelope, one LFO and the chorus with its two "
    "bucket-brigade lines. Six voices, handed out in turn as on the "
    "instrument.",
    "A patch is kept in the format of the Juno-106, whose panel is nearly "
    "the same and which has MIDI: 16 sliders from 0 to 127 and 8 switches. "
    "So Juno-106 patch collections play, and patches go to a Juno-106 and "
    "back. Of the Juno-60's own extras, chorus I+II is kept (a Juno-106 "
    "plays it as I) and pulse width from the envelope is left out. The "
    "arpeggiator is left out as well.",
    "Not on the instrument: velocity, off until vellevel or velcutoff is "
    "raised, and the modulation wheel, which adds vibrato as on a "
    "Juno-106. The oscillator and the filter run at twice the sample rate.",
]

DATA = [
    "The object needs no files. The 56 factory sounds of the Juno-60 are "
    "built in, presets 0 to 55.",
    "Every .syx file in the folder of the external is read as a bank of "
    "Juno-106 patches and joins the factory presets, in alphabetical "
    "order, from preset 56 on. They are named after the file and the "
    "place a Juno-106 shows, A11 to B88.",
    "junouser-bank.syx in the same folder is the user bank: 56 patch "
    "messages of 24 bytes. Until it has been written the user bank holds "
    "56 init patches. Its slots are shown as on the Juno-60, bank and "
    "number, 11 to 78.",
]

PRESETS = [
    "preset selects a factory preset, user a slot of the user bank. The "
    "third outlet answers with the number and the name.",
    "store 5 followed by userwrite keeps the playing patch in user slot 5 "
    "and on disk. The editor does both with save patch.",
    "The controls of the SETUP row, volume to velcutoff, belong to the "
    "instrument. A patch change leaves them alone and they are not stored.",
]

MEMORY_BLOCK = (
    "By address",
    [],
    ["param 5 60", "param 5", "dump", "sysexdump"],
    ["param a v: sets one of the 18 bytes of the patch, as a Juno-106 "
     "counts them: 0 to 15 are the sliders in the order LFO rate, LFO "
     "delay, DCO LFO, PWM, noise, VCF frequency, resonance, envelope, "
     "LFO, keyboard, VCA level, attack, decay, sustain, release, sub "
     "level. 16 and 17 hold the switches. param a reports the byte.",
     "dump: reports every control as info.",
     "sysexdump: sends the playing patch as SysEx, see below."])

SYSEX = [
    "The object speaks the exclusive format of the Juno-106. The bytes "
    "arrive with the rest of the MIDI at the third inlet and leave the "
    "third outlet as floats. Messages on another channel than the "
    "receive channel are ignored.",
    "A patch message (F0 41 30 channel number, 18 bytes, F7) or a manual "
    "message (31) replaces the playing patch, as on a Juno-106. A "
    "parameter message (F0 41 32 channel parameter value F7) changes one "
    "of its 18 bytes.",
    "sysexdump sends the playing patch and usersend the 56 patches of the "
    "user bank, numbered 0 to 55. A Juno-106 plays what it receives but "
    "does not store it by itself, so after usersend it holds the last "
    "patch. Its memory is written from its own panel.",
    "sysexout 1 makes the object behave like a Juno-106: every edit "
    "leaves as a parameter message and every patch change as a patch "
    "message. What came in as SysEx is not sent back.",
    "Switch bytes. 16: bits 0 to 2 are 16', 8', 4', bit 3 pulse, bit 4 "
    "sawtooth, bit 5 chorus off, bit 6 chorus I, else II. 17: bit 0 PWM "
    "manual, bit 1 envelope negative, bit 2 VCA gate, bits 3 and 4 the "
    "high-pass, 3 for position 0 down to 0 for position 3. Bit 5 of byte "
    "17 is the object's own: chorus I+II.",
]

PLAYING = (
    "Playing",
    [[("bend", -1, 1), ("mod", 0, 1), ("sustain", 0, 1)]],
    [],
    ["bend (0): pitch wheel, -1 to 1. The range in semitones is the control bend.",
     "mod (0): modulation wheel, 0 to 1. Adds vibrato at the rate of the "
     "LFO, without its delay.",
     "sustain (0): hold pedal. Notes released while it is 1 sound until it returns to 0."])

CREDITS = ("Credits: Modelled on PicoFaceJ6 from PicoVintageSynthCollection by "
           "Michi71; Pure Data adaptation with help by Claude")


def blocks():
    out = []
    for title, names in T.CC_ROWS[1:]:
        groups = [[(n,) + rng(n) for n in names[k:k + 4]] for k in range(0, len(names), 4)]
        intro = []
        if title in ("SETUP", "VOICES"):
            intro = ["These belong to the instrument and are not stored in a patch."]
        nice = {"DCO": "DCO", "LFO / VCF / VCA": "LFO VCF VCA", "SETUP": "Setup", "VOICES": "Voices"}[title]
        out.append((nice, groups, [], intro + [para(n) for n in names]))
    return out


def control_lines():
    lines = []
    for title, names in T.CC_ROWS:
        for nm in names:
            lo, hi = rng(nm)
            first, *rest = wrap(f"{nm} {lo}..{hi}, CC {CC[nm]}: {helptext(nm)}")
            lines.append(first)
            for r in wrap(" ".join(rest), MAXCH - 2) if rest else []:
                lines.append("  " + r)
    return lines


def build(p):
    """Draws everything below the backgrounds; returns the bottom y."""
    y = G.YEL_Y + M
    if p:
        p.heading(CX, y, OBJ, 22)
        p.heading(CX, y + 36, SUBTITLE, 16)
    y += 80

    # ---- demo row
    if p:
        tg = p.obj(CX, y, "tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1")
        me = p.obj(CX + 30, y, "metro 500")
        on = p.msg(CX + 110, y, f"57 100 {COMMA} 60 100 {COMMA} 64 100")
        de = p.obj(CX + 30, y + 30, "delay 250")
        off = p.msg(CX + 110, y + 30, f"57 0 {COMMA} 60 0 {COMMA} 64 0")
        s1 = p.obj(CX + 400, y + 30, f"s {BUS}")
        p.conn(tg, 0, me, 0)
        p.conn(me, 0, on, 0)
        p.conn(me, 0, de, 0)
        p.conn(de, 0, off, 0)
        p.conn(on, 0, s1, 0)
        p.conn(off, 0, s1, 0)
    y += 64
    if p:
        s2 = p.obj(CX + 400, y, f"s {BUS}")
        for i, txt in enumerate(["60 100", "60 0", "status", "presets", "stop", "reset"]):
            m = p.msg(CX + i * 62, y, txt)
            p.conn(m, 0, s2, 0)
    y += 40

    # ---- main controls: filter and envelope, two rows of four
    for r in range(2):
        if p:
            snd = p.obj(CX + 400, y + 50, f"s {BUS}")
            for c in range(4):
                nm = MAIN[r * 4 + c]
                lo, hi = rng(nm)
                a = p.atom(CX + c * 112, y, 4, lo, hi)
                m = p.msg(CX + c * 112, y + 24, f"{nm} {DOLLAR}1")
                p.conn(a, 0, m, 0)
                p.conn(m, 0, snd, 0)
        y += 90 if r == 0 else 64

    # ---- object area: MIDI, controller layer, editor
    if p:
        ro = p.obj(CX + 130, y, f"r {BUS}-out")
        cc = p.obj(CX + 130, y + 30, "junocc 1")
        sc = p.obj(CX + 130, y + 60, f"s {BUS}")
        p.conn(ro, 0, cc, 0)
        p.conn(cc, 0, sc, 0)
        p.obj(CX + 250, y + 30, f"junoedit {BUS}")
        pd_ = p.obj(CX + 250, y + 60, "junopads 10")
        ps = p.obj(CX + 250, y + 90, f"s {BUS}")
        p.conn(pd_, 0, ps, 0)
        lb = p.obj(CX, y, "loadbang")
        ch1 = p.msg(CX, y + 30, "channel 1")
        p.conn(lb, 0, ch1, 0)
        mi = p.obj(CX + 400, y + 60, "midiin")
        rc = p.obj(CX, y + 60, f"r {BUS}")
        ob = p.obj(CX, y + 100, OBJ)
        p.conn(rc, 0, ob, 0)
        p.conn(ch1, 0, ob, 0)
        p.conn(mi, 0, ob, 2)
        gl = p.obj(CX, y + 134, "*~ 0.3")
        gr = p.obj(CX + 60, y + 134, "*~ 0.3")
        da = p.obj(CX, y + 164, "dac~")
        rf = p.obj(CX + 130, y + 134, "route float")
        mo = p.obj(CX + 130, y + 224, "midiout")
        lk = p.obj(CX + 220, y + 194, "lkdisplay")
        so = p.obj(CX + 220, y + 164, f"s {BUS}-out")
        pr = p.obj(CX + 330, y + 164, "print JUNO")
        p.conn(ob, 0, gl, 0)
        p.conn(ob, 1, gr, 0)
        p.conn(gl, 0, da, 0)
        p.conn(gr, 0, da, 1)
        p.conn(ob, 2, rf, 0)
        p.conn(rf, 0, mo, 0)
        p.conn(rf, 1, so, 0)
        p.conn(rf, 1, pr, 0)
        p.conn(rf, 1, lk, 0)
        p.conn(lk, 0, mo, 0)
    y += 260

    # ---- text sections
    y = section(p, y, "Inlets", INLETS)
    y = section(p, y, "Outlets", OUTLETS)
    y = section(p, y, "Messages", MESSAGES)
    y = section(p, y, "MIDI", MIDI)
    y = section(p, y, "Controls", [para(nm) for nm in MAIN], para=True)
    y = section(p, y, "Controller layer and editor", LAYER, para=True)
    y = section(p, y, "The instrument", ABOUT, para=True)

    # ---- presets, with live objects
    if p:
        p.heading(CX, y, "Presets", 14)
    y += 28
    if p:
        s = p.obj(CX + 400, y + 24, f"s {BUS}")
        for k, (nm, hi) in enumerate((("preset", 127), ("user", 55))):
            a = p.atom(CX + k * 100, y, 4, 0, hi)
            m = p.msg(CX + k * 100, y + 24, f"{nm} {DOLLAR}1")
            p.conn(a, 0, m, 0)
            p.conn(m, 0, s, 0)
        r = p.obj(CX, y + 60, f"r {BUS}-out")
        ro = p.obj(CX, y + 84, "route preset user")
        p.conn(r, 0, ro, 0)
        for k in range(2):
            un = p.obj(CX + k * 200, y + 110, "unpack f s")
            fa = p.atom(CX + k * 200, y + 138, 4, 0, 0)
            sa = p.add(f"#X symbolatom {CX + 40 + k * 200} {y + 138} 20 0 0 0 - - - 0;")
            p.conn(ro, k, un, 0)
            p.conn(un, 0, fa, 0)
            p.conn(un, 1, sa, 0)
    y += 166
    if p:
        p.comment(TX, y, PRESETS, para=True)
    y += LINE * nlines(PRESETS) + GAP + 10

    # ---- settings blocks: one per controller page after the first
    for title, groups, examples, paras in blocks():
        y = draw_block(p, title, groups, examples, paras, y)
    y = draw_block(p, *PLAYING, y)
    y = draw_block(p, *MEMORY_BLOCK, y)
    y = section(p, y, "SysEx", SYSEX, para=True)
    y = section(p, y, "Data files", DATA, para=True)
    y = section(p, y, "All controls and their CC numbers", control_lines())

    y += 6
    if p:
        p.comment(TX, y, [CREDITS], para=True, semi=True)
    y += LINE * nlines([CREDITS]) + M
    return y


def main():
    bottom = build(None)
    p = G.Patch()
    p.cnv(G.YEL_X, G.YEL_Y, G.YEL_W, bottom - G.YEL_Y)
    p.cnv(G.PINK_X, G.PINK_TOP, G.PINK_W, bottom - G.PINK_TOP, bg=G.PINK)
    p.cnv(G.BLUE_X, G.BLUE_TOP, G.BLUE_W, bottom - G.BLUE_TOP, bg=G.BLUE)
    assert build(p) == bottom
    out = os.path.join(HERE, "..", OBJ, f"{OBJ}-help.pd")
    with open(out, "w") as f:
        f.write(f"#N canvas 348 20 820 {bottom + M} 12;\n")
        f.write("\n".join(p.lines) + "\n")
        f.write("\n".join(p.conns) + "\n")
    print(f"wrote {os.path.normpath(out)} ({len(p.lines)} objects, "
          f"{len(p.conns)} connections, height {bottom + M})")


if __name__ == "__main__":
    main()
