#!/usr/bin/env python3
"""Generate the help patch of juno.i~ and juno~ (one patch, written under both names).
Never edit the help patch by hand: change this script and run it again. Names, ranges and
CC pages come from tools/juno_params.py, the drawing helpers from tools/gen_help.py.

The help is built around [juno.i~]: the instrument object holds the editor, the controller
layer, the pads and MIDI in and out, and the editor has every control. So the help has no
GUI: one example ([juno.i~ 1] -> [dac~]) and the reference as text: the object, inlet,
outlets, presets and user bank, messages, the instrument, SysEx, data files, and the controls
row by row with the titles and labels of the editor.

    python3 tools/gen_juno_help.py       writes pd/juno.i~-help.pd and pd/juno~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, helpdraw, helpkit
import ctlayer                                    # noqa: E402
import helpdraw as H                              # noqa: E402
import helpkit                                    # noqa: E402
import juno_params as T                           # noqa: E402
import gen_juno_editor as E                       # noqa: E402  (rows and labels of the editor)

OBJ = "juno.i~"
FILES = ("juno.i~-help.pd", "juno~-help.pd")
SHORT = "juno"
TITLE = "juno.i~ and juno~"
SUBTITLE = "Roland Juno-60 with Juno-106 patches and SysEx"

CC = ctlayer.cc_map(T.CC_ROWS)
MAIN = list(T.CC_ROWS[0][1])


def rng(nm):
    return T.CONTROLS[nm][2], T.CONTROLS[nm][3]


def helptext(nm):
    return T.CONTROLS[nm][6]


OBJECT = [
    "juno.i~ is the whole instrument in one object: juno~, the editor, "
    "the controller layer, the Launchkey pads and display, MIDI in and "
    "out. Put it in a patch, connect the two left outlets to dac~ and play. "
    "Click it to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1), argument 2 the "
    "channel of the pads (0 or none = 10, the Drums Channel). Several "
    "juno.i~ do not get in each other's way, each has its own editor.",
    "juno~ is the bare engine inside, for patches of your own: inlet 2 "
    "takes the velocity of a note sent as a float, inlet 3 raw MIDI bytes "
    "from midiin, and the floats of outlet 3 (SysEx, MIDI thru) go to "
    "midiout. Open juno.i~ to see the wiring.",
]

EDITOR = [
    "The editor is the window of juno.i~. Every change shows up in it, "
    "from the controller, a patch change, messages or SysEx. refresh asks "
    "for all values again.",
    "Rows of 8 below the PATCH band, titled as in Controls: row k is page "
    "k of the controller layer. Each cell: name and label, a slider and a "
    "number box with the value as from a pot (0 to 127), the box beside it "
    "the value as on the panel. Edits are heard at once. The tinted rows "
    "(SETUP, VOICES) belong to the instrument and are not stored in a "
    "patch.",
    "PATCH band. name: the playing patch. factory N, user N: choose a "
    "factory preset or a user slot. reload: the same patch again, edits "
    "dropped. init patch: a plain sawtooth. store to N: chooses the user "
    "slot, save patch stores the playing patch there and writes the user "
    "bank, see Presets. load bank, write bank: read or write the bank file "
    "without storing. send bank, send patch: as SysEx to MIDI out, see "
    "SysEx.",
    "MIDI in: the receive channel, 0 = all. out: the channel of the "
    "controller feedback and of SysEx. SysEx for every change: see SysEx. "
    "MIDI thru: see Messages, thru.",
]

INLETS = [
    "1 float: note number, velocity 100.",
    "1 list: note, velocity and, if you like, the MIDI channel.",
    "1 anything: a message, see Messages.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: info, name, 0-127 and value text for every",
    "  change, the patch that plays, its name, the channels.",
    "3 display title value: two lines for a display: a control",
    "  (VCF freq, 64), the patch that plays (Preset 12,",
    "  Harpsichord 1), a store (Stored, User 11).",
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
    "save: store, then write the user bank (the pads' save).",
    "savearm, saveslot n, savecancel: the pads' store with a",
    "  question: show the slot, move it, drop it. save after",
    "  savearm stores into the slot shown.",
    "presetstep 1 or -1, userstep 1 or -1: the next or previous",
    "  factory preset or user slot, from the one chosen last.",
    "cycle name [-1]: the next (previous) value, round and round.",
    "userwrite [file], userread [file]: the user bank, see",
    "  Presets. A file name without a path is beside juno~.",
    "usersend: the user bank as SysEx.",
    "presets, users: list the names.",
    "bend f: pitch wheel, -1 to 1, times bend.",
    "mod f: modulation wheel, 0 to 1: vibrato.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback and of",
    "  SysEx, 1 to 16.",
    "thru 0/1: MIDI thru, on by default: what arrives at MIDI in",
    "  leaves MIDI out unchanged. Off with several juno.i~ or",
    "  when MIDI out leads back to MIDI in.",
    "sysexout 0/1: SysEx for every change, see SysEx.",
    "param a v: one of the 18 patch bytes, see SysEx.",
    "  param a reports it.",
    "dump: every control as info.",
    "sysexdump: the playing patch as SysEx.",
    "status: report patch, channels and sounding voices.",
    "stop: all notes off.",
    "reset: all notes off at once, chorus emptied.",
]

PRESETS = [
    "preset selects a factory preset, user a slot of the user bank. The "
    "third outlet answers with the number and the name.",
    "The user bank (56 slots, shown as bank and number 11 to 78) is "
    "junouser-bank.syx in the folder of your patch, so every project keeps "
    "its own. juno.i~ reads it at load if it is there, otherwise the bank "
    "beside juno~ is the starting point. save, userwrite and userread "
    "without a file name use this file: store to N in the editor chooses "
    "the slot, save patch stores and writes. Save the patch first, an "
    "unsaved patch has no folder of its own.",
    "There is one user bank in Pd, not one per object: two juno.i~ in one "
    "patch share it.",
    "The controls of the SETUP row, volume to velcutoff, belong to the "
    "instrument. A patch change leaves them alone and they are not stored.",
]

ABOUT = [
    "The engine is a Juno-60: one DCO per voice with sawtooth, pulse, sub "
    "oscillator and noise, the four-position high-pass, the IR3109 "
    "low-pass, one envelope, one LFO and the chorus with its two "
    "bucket-brigade lines. Six voices, handed out in turn as on the "
    "instrument.",
    "A patch is kept in the format of the Juno-106: 16 sliders from 0 to "
    "127 and 8 switches. So Juno-106 patch collections play, and patches "
    "go to a Juno-106 and back. Chorus I+II of the Juno-60 is kept (a "
    "Juno-106 plays it as I), pulse width from the envelope and the "
    "arpeggiator are left out.",
    "Not on the instrument: velocity, off until vellevel or velcutoff is "
    "raised, and the modulation wheel, which adds vibrato as on a "
    "Juno-106. model switches between the behaviour of the Juno-60 and "
    "the Juno-106. The oscillator and the filter run at twice the sample "
    "rate.",
]

SYSEX = [
    "The object speaks the exclusive format of the Juno-106 on MIDI in "
    "and out, on the receive channel. A patch message (F0 41 30) or a "
    "manual message (31) replaces the playing patch, a parameter message "
    "(F0 41 32 channel parameter value F7) changes one of its 18 bytes. "
    "sysexout 1 sends every edit and patch change the same way, like a "
    "Juno-106. send patch and send bank in the editor send without being "
    "asked. A Juno-106 plays what it receives but stores it only from its "
    "own panel.",
    "The 18 bytes: 0 to 15 are the sliders, LFO rate, LFO delay, DCO LFO, "
    "PWM, noise, VCF frequency, resonance, envelope, LFO, keyboard, VCA "
    "level, attack, decay, sustain, release, sub level. 16: bits 0 to 2 "
    "16', 8', 4', bit 3 pulse, bit 4 sawtooth, bit 5 chorus off, bit 6 "
    "chorus I, else II. 17: bit 0 PWM manual, bit 1 envelope negative, "
    "bit 2 VCA gate, bits 3 and 4 the high-pass (3 for position 0 down to "
    "0 for position 3), bit 5 chorus I+II (the object's own).",
]

DATA = [
    "juno~ needs no files: the 56 factory sounds of the Juno-60 are built "
    "in, presets 0 to 55. Every .syx file in the folder of juno~ is read "
    "as a bank of Juno-106 patches and joins the factory presets in "
    "alphabetical order, from preset 56 on, named after the file and the "
    "place a Juno-106 shows (A11 to B88).",
    "For juno.i~ two files are enough: juno~ (the external) and juno.i~.pd "
    "in a folder on Pd's search path.",
]

CREDITS = ("Credits: Modelled on PicoFaceJ6 from PicoVintageSynthCollection by "
           "Michi71; Pure Data adaptation with help by Claude")


CONTROLS_INTRO = [
    "In the order of the editor: one heading per row, the same title as "
    "the row there, page k on the controller is row k. Each line: name "
    "(the label in the editor), range, CC number and what it does.",
]


def control_line(nm):
    lo, hi = rng(nm)
    cc = f", CC {CC[nm]}" if nm in CC else ""
    return f"{nm} ({E.label(nm)}) {lo}..{hi}{cc}: {helptext(nm)}"


def main():
    rows = [(f"{title}  {E.cc_text(names)}".replace(":", ""), names) for title, names in E.ROWS]
    H.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
            title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
            sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                      ("text", "Launchkey", helpkit.launchkey(
                          "juno~", "junocc", "junopads",
                          "39 chorus I, II, I+II, off (round and round), 47 Juno-106 / Juno-60")),
                      ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                      ("text", "Presets and user bank", PRESETS),
                      ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                      ("text", "SysEx", SYSEX), ("text", "Data files", DATA),
                      ("controls", CONTROLS_INTRO, rows, control_line)])


if __name__ == "__main__":
    main()
