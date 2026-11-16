#!/usr/bin/env python3
"""Generate cz4~-help.pd. Never edit the help patch by hand: change this
script and run it again. Names, ranges and CC pages come from tools/cz4_params.py,
the drawing helpers from tools/help_draw.py.

    python3 tools/gen_cz4_help.py       writes cz4/cz4~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ctlayer                                    # noqa: E402
import help_draw as G                             # noqa: E402
import cz4_params as T                            # noqa: E402

OBJ = "cz4~"
SHORT = "cz4"
BUS = f"{G.DOLLAR}0-{SHORT}"
G.BUS = BUS                 # draw_block of gen_help sends to its module's bus
SUBTITLE = "Casio CZ-1 with CZ tones and SysEx"
M, CX, TX, LINE, GAP, MAXCH = G.M, G.CX, G.TX, G.LINE, G.GAP, G.MAXCH
DOLLAR, COMMA = G.DOLLAR, G.COMMA
wrap, nlines, section, draw_block = G.wrap, G.nlines, G.section, G.draw_block

CC = ctlayer.cc_map(T.CC_ROWS)
MAIN = list(T.CC_ROWS[0][1])


def rng(nm):
    return T.CONTROLS[nm][2], T.CONTROLS[nm][3]


def default(nm):
    return "tone" if T.CONTROLS[nm][0] in "TLE" else str(T.DEFAULTS[nm])


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
    "  change, the tones that play, their names, the channels.",
    "3 float: SysEx bytes for midiout.",
]

MESSAGES = [
    "name v: set a control, in the values the CZ shows.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "preset n: a factory tone, read only.",
    "user n: a slot of the user bank, 0 to 63.",
    "init: a plain sawtooth tone to start from.",
    "reload: the same tone again, edits dropped.",
    "compare 0/1: hear the stored tone instead of the edit.",
    "store n: the edited tone into user slot n, in memory.",
    "name text: rename the edited tone (16 characters).",
    "copyline 1 2: line 1 into line 2 (or 2 1).",
    "copyenv 1 DCW 2 DCW: one envelope into another.",
    "userwrite: write the user bank to cz4user-bank.syx.",
    "userread: read it again. Both also take a file name.",
    "usersend: the user bank as SysEx on outlet 3.",
    "presets, users: list the names.",
    "op n: recall operation slot n, opstore n: store it,",
    "  opinit: back to one tone, opwrite, opread: the file",
    "  cz4user-op.syx, ops: the used slots.",
    "bend f: pitch wheel, -1 to 1, times bend.",
    "mod f: modulation wheel, 0 to 1: vibrato.",
    "at f: aftertouch, 0 to 1.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback and of",
    "  SysEx, 1 to 16.",
    "status: report tones, operation slot, channels, voices.",
    "stop: all notes off (they release).",
    "reset: silence at once.",
]

MIDI = [
    "midiin: raw bytes into inlet 3. cz4~ takes notes with",
    "  velocity, bend, aftertouch, program change, CC 1",
    "  (vibrato), 7 (volume), 64 (hold) and CZ SysEx.",
    "midiout: the floats of outlet 3, SysEx tones and dumps.",
    "cz4cc 1: the CC pages from 20 on, on channel 1.",
    "Without midiin: send note lists and bend, mod, at, sustain.",
]

LAYER = [
    "cz4cc is the controller layer: MIDI CC from 20 on in pages of 8, "
    "the same pages as for pdoly8~, tx81z~, d50~ and juno~. Page 1 is "
    "PERFORM, page k is row k of the editor. It turns a CC into an n "
    "message and sends every change made elsewhere back to the controller "
    "as CC, also when the selected line or envelope changes. Its argument "
    "is the MIDI channel, and it follows the channels cz4~ reports.",
    "cz4edit is the editor: click it in the object area above. All 77 "
    "controls in rows of 8, the factory and user tones, the operation "
    "memory, the MIDI channels and the CZ handshake. It explains itself "
    "in its readme.",
    "All names are listed at the end of this patch with their CC numbers.",
]

ABOUT = [
    "A tone is a CZ tone: two lines, each with a wave pair and a window, "
    "and three envelopes per line with 8 steps each: DCO (pitch), DCW "
    "(the phase distortion, which does what a filter does on other "
    "synthesizers) and DCA (amplitude). Line select plays line 1, line "
    "2, line 1 twice with line 2 detuned (1+1') or both (1+2'). Line 2 "
    "can be ring-modulated by line 1 or have its pitch thrown about by "
    "noise.",
    "Every voice has its own six envelopes: the instrument is fully "
    "polyphonic. As on the CZ there are 8 lines: 8 notes of a one-line "
    "tone, 4 of a two-line tone (voices raises this up to 16).",
    "From the CZ-1: 144-byte tones with line levels and velocity on each "
    "envelope, names, the operation memory with split and tone mix of two "
    "tones, portamento, glide, bend range, wheel and aftertouch, and the "
    "chorus. The waves are those of the uPD933 chip as MAME models it, "
    "computed with the chip's 12-bit tables (chip 0 gives ideal ones).",
    "From Arturia's CZ V the idea of simpler access: the PERFORM row "
    "changes brightness, attack, release, balance and detune of the "
    "playing tone without touching it, and the easy envelope writes a "
    "whole 8-step envelope from four values. Everything stays a CZ tone, "
    "so it can go to a CZ and back.",
    "Timings the chip does not fix (vibrato clock, velocity and key "
    "follow curves, portamento, glide, chorus) are estimates for now.",
]

DATA = [
    "The object needs no files. Without them the factory bank holds the "
    "INIT tone.",
    "Every .syx file in the folder of the external is read for CZ tones "
    "(CZ-101, 1000, 3000, 5000 with 128 bytes, CZ-1 with 144) and they "
    "join the factory bank in alphabetical order of the files. Casio's "
    "own tones are not part of this project: put your own dumps there.",
    "cz4user-bank.syx in the same folder is the user bank: 64 CZ-1 tones "
    "as a librarian keeps them, so other CZ tools read it as well. "
    "cz4user-op.syx holds the operation memory: a message of its own per "
    "used slot (F0 7D, the ID for non-commercial use).",
]

PRESETS = [
    "preset selects a factory tone, user a slot of the user bank. They go "
    "to the tone chosen with edit: A, or B of a split or mix. The third "
    "outlet answers with tonea or toneb, the bank, the number and the name.",
    "store 5 followed by userwrite keeps the edited tone in user slot 5 "
    "and on disk. The editor does both with save tone.",
    "op recalls an operation slot: key mode, split point, tone B and the "
    "rest of the OPERATION rows. Tone B is loaded from its bank and number.",
]

MEMORY_BLOCK = (
    "Selection",
    [[("line", 0, 1), ("env", 0, 2), ("edit", 0, 1)]],
    ["copyline 1 2", "copyenv 1 DCW 2 DCW", "compare 1", "compare 0"],
    ["line (0 = line 1), env (2 = DCA), edit (0 = tone A): what the line, "
     "envelope, rate and level controls work on. Changing them reports the "
     "values of the new selection, the controller follows.",
     "copyline and copyenv copy within the edited tone, compare plays the "
     "stored tone while it is 1."])

SYSEX = [
    "The object speaks the exclusive format of the CZ series. A tone "
    "travels as nibbles, the low one first: 256 for the 128 bytes of a "
    "CZ-101, 1000, 3000 or 5000, 288 for the 144 bytes of a CZ-1. The "
    "bytes arrive with the rest of the MIDI at the third inlet and leave "
    "the third outlet as floats.",
    "As a device it answers like a CZ, with Casio's handshake: a send "
    "request (F0 44 00 00 7n 10 or 11, program) and a receive request "
    "(20 or 21) are answered with F0 44 00 00 7n 30, and the tone follows. "
    "So CZ editors and librarians work with it. Program 96 (hex 60) is the "
    "tone A that plays, 32 to 95 the user slots 0 to 63, 0 to 31 the first "
    "factory tones.",
    "As a host it talks to a real CZ: czget p asks it for program p, "
    "czsend p sends it the edited tone. czmodel 101 makes both use the "
    "128-byte format of the CZ-101 family, czmodel 1 (default) the CZ-1's.",
    "sysexdump sends the edited tone and usersend the 64 tones of the "
    "user bank as librarian messages (receive request with the data in "
    "the same message). Such messages and .syx files of librarians are "
    "taken at the third inlet or with sysex b b b.",
]

PLAYING = (
    "Playing",
    [[("bend", -1, 1), ("mod", 0, 1), ("at", 0, 1), ("sustain", 0, 1)]],
    [],
    ["bend (0): pitch wheel, -1 to 1. The range in semitones is the control bend.",
     "mod (0): modulation wheel, 0 to 1. Adds vibrato, as deep as wheel says.",
     "at (0): aftertouch, 0 to 1: vibrato (atvib) and level (atamp).",
     "sustain (0): hold pedal. Notes released while it is 1 sound until it returns to 0."])

CREDITS = ("Credits: uPD933 model by Devin Acker (MAME), CZ engine after melodee by "
           "Kerem Kilic (BSD-3); CZ tone format after Casio's MIDI specifications; "
           "cz4~ by Peter with help by Claude")


NICE = {"LINE / WAVE": "Line and wave", "DETUNE / VIBRATO": "Detune and vibrato",
        "ENVELOPE": "Envelope", "RATES": "Rates", "LEVELS": "Levels",
        "EASY ENV / TOUCH": "Easy envelope and touch", "OPERATION": "Operation memory",
        "OPERATION 2": "Operation memory 2", "SETUP": "Setup"}
INTRO = {
    "LINE / WAVE": ["Values of the selected line (line) of the edited tone (edit)."],
    "ENVELOPE": ["sus and end act on the selected envelope (env) of the selected line."],
    "RATES": ["Rates of the 8 steps of the selected envelope: 99 is fastest, 0 takes minutes."],
    "LEVELS": ["Levels of the 8 steps. DCO: pitch above the note (up to 63 in 1/16 "
               "semitones, then in steps of 2 semitones), DCW: wave, DCA: level."],
    "EASY ENV / TOUCH": ["The four easy values write steps 1 to 3 of the selected envelope."],
    "OPERATION": ["Operation memory, stored in an operation slot, not in a tone."],
    "OPERATION 2": ["Operation memory, stored in an operation slot, not in a tone."],
    "SETUP": ["bend belongs to the operation memory, the others to the instrument."],
}


def blocks():
    out = []
    for title, names in T.CC_ROWS[1:]:
        groups = [[(n,) + rng(n) for n in names[k:k + 4]] for k in range(0, len(names), 4)]
        paras = [para(n) for n in names] if title not in ("RATES", "LEVELS") else []
        out.append((NICE[title], groups, [], INTRO.get(title, []) + paras))
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

    # ---- main controls: the PERFORM row, two rows of four
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
        cc = p.obj(CX + 130, y + 30, "cz4cc 1")
        sc = p.obj(CX + 130, y + 60, f"s {BUS}")
        p.conn(ro, 0, cc, 0)
        p.conn(cc, 0, sc, 0)
        p.obj(CX + 250, y + 30, f"cz4edit {BUS}")
        mi = p.obj(CX + 400, y + 60, "midiin")
        rc = p.obj(CX, y + 60, f"r {BUS}")
        ob = p.obj(CX, y + 100, OBJ)
        p.conn(rc, 0, ob, 0)
        p.conn(mi, 0, ob, 2)
        gl = p.obj(CX, y + 134, "*~ 0.3")
        gr = p.obj(CX + 60, y + 134, "*~ 0.3")
        da = p.obj(CX, y + 164, "dac~")
        rf = p.obj(CX + 130, y + 134, "route float")
        mo = p.obj(CX + 130, y + 164, "midiout")
        so = p.obj(CX + 220, y + 164, f"s {BUS}-out")
        pr = p.obj(CX + 330, y + 164, "print CZ4")
        p.conn(ob, 0, gl, 0)
        p.conn(ob, 1, gr, 0)
        p.conn(gl, 0, da, 0)
        p.conn(gr, 0, da, 1)
        p.conn(ob, 2, rf, 0)
        p.conn(rf, 0, mo, 0)
        p.conn(rf, 1, so, 0)
        p.conn(rf, 1, pr, 0)
    y += 200

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
        for k, (nm, hi) in enumerate((("preset", 127), ("user", 63), ("op", 63))):
            a = p.atom(CX + k * 100, y, 4, 0, hi)
            m = p.msg(CX + k * 100, y + 24, f"{nm} {DOLLAR}1")
            p.conn(a, 0, m, 0)
            p.conn(m, 0, s, 0)
        r = p.obj(CX, y + 60, f"r {BUS}-out")
        ro = p.obj(CX, y + 84, "route tonea toneb")
        p.conn(r, 0, ro, 0)
        for k in range(2):
            un = p.obj(CX + k * 200, y + 110, "unpack s f s")
            fa = p.atom(CX + k * 200, y + 138, 4, 0, 0)
            sa = p.add(f"#X symbolatom {CX + 40 + k * 200} {y + 138} 20 0 0 0 - - - 0;")
            p.conn(ro, k, un, 0)
            p.conn(un, 1, fa, 0)
            p.conn(un, 2, sa, 0)
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
    out = os.path.join(HERE, "..", "cz4", f"{OBJ}-help.pd")
    with open(out, "w") as f:
        f.write(f"#N canvas 348 20 820 {bottom + M} 12;\n")
        f.write("\n".join(p.lines) + "\n")
        f.write("\n".join(p.conns) + "\n")
    print(f"wrote {os.path.normpath(out)} ({len(p.lines)} objects, "
          f"{len(p.conns)} connections, height {bottom + M})")


if __name__ == "__main__":
    main()
