#!/usr/bin/env python3
"""Generate the help patch of cz4.i~ and cz4~ (one patch, written under both names), in the form
of the skill pd-help-layout: no GUI, one example and the reference as text; the controls row by
row with the titles and labels of the editor (tools/gen_cz4_editor.py). Never edit the help
patch by hand: change this script and run it again.

    python3 tools/gen_cz4_help.py       writes pd/cz4.i~-help.pd and pd/cz4~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, helpdraw, helpkit
import ctlayer                                    # noqa: E402
import cz4_params as T                            # noqa: E402
import gen_cz4_editor as E                        # noqa: E402  (rows and labels of the editor)
import helpdraw as H                              # noqa: E402
import helpkit                                    # noqa: E402

OBJ = "cz4.i~"
FILES = ("cz4.i~-help.pd", "cz4~-help.pd")
TITLE = "cz4.i~ and cz4~"
SUBTITLE = "Casio CZ-1 with CZ tones and SysEx"

CC = ctlayer.cc_map(T.CC_ROWS)


def rng(nm):
    return T.CONTROLS[nm][2], T.CONTROLS[nm][3]


def helptext(nm):
    return T.CONTROLS[nm][6]


OBJECT = [
    "cz4.i~ is the whole instrument in one object: cz4~, the editor, the "
    "controller layer, the Launchkey pads and display, MIDI in and out. Put "
    "it in a patch, connect the two left outlets to dac~ and play. Click it "
    "to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1), argument 2 the channel "
    "of the pads (0 or none = 10, the Drums Channel). Several cz4.i~ do not "
    "get in each other's way, each has its own editor. Page 1 of the "
    "controller is PERFORM, page k is row k of the editor.",
    "cz4~ is the bare engine inside, for patches of your own: inlet 2 takes "
    "the velocity of a note sent as a float, inlet 3 raw MIDI bytes from "
    "midiin (notes with velocity, bend, aftertouch, program change, CC 1, "
    "7, 64 and CZ SysEx), and the floats of outlet 3 (SysEx) go to midiout. "
    "Open cz4.i~ to see the wiring.",
]

EDITOR = [
    "The editor is the window of cz4.i~. Every change shows up in it, from "
    "the controller, a tone change, messages or SysEx. refresh asks for all "
    "values again.",
    "Rows of 8 below the TONE band, titled as in Controls: row k is page k "
    "of the controller layer. Each cell: name and label, a slider and a "
    "number box with the value as from a pot (0 to 127), the box beside it "
    "the value as the CZ shows it. Edits are heard at once.",
    "Line, envelope and tone: the LINE / WAVE, ENVELOPE, RATES and LEVELS "
    "rows edit the line chosen with line (1 or 2) and the envelope chosen "
    "with env (DCO pitch, DCW wave, DCA amplitude), in tone A or, with edit "
    "B, in tone B of a split or mix. The cells follow the selection.",
    "An envelope has 8 steps of rate and level. sus is the step that holds "
    "while the key is down (off: none), end the last step, which goes to "
    "zero. Easy A D S R write a simple envelope into the selected one: "
    "attack to the top, decay to the sustain level, release to zero.",
    "PERFORM and SETUP (beige) are not stored. OPERATION (green) is the "
    "operation memory of the CZ-1: key mode (normal, split, mix), split "
    "point, tone B, volumes, chorus, portamento, glide, bend range, wheel "
    "and aftertouch. It is stored in an operation slot, not in the tone.",
    "TONE band. name: type a new name (16 characters, _ for a space). "
    "factory N: a factory tone, from the CZ banks (.syx) beside cz4~. user "
    "N: a user slot, 0 to 63. reload: the same tone again, edits dropped. "
    "init: a plain sawtooth. compare: hear the stored tone while it is on. "
    "store to N: chooses the user slot, save tone stores the edited tone "
    "there and writes the user bank, see Presets. load bank, write bank: "
    "read or write the bank file without storing. send bank, send tone: as "
    "SysEx to MIDI out.",
    "operation N recalls an operation slot, store to N chooses one, save "
    "operation stores the current operation there and writes the operation "
    "file, init operation goes back to one tone.",
    "MIDI in: the receive channel, 0 = all. out: the channel of the "
    "controller feedback and of SysEx. CZ program, get from CZ, send to CZ: "
    "talk to a real CZ with its handshake, see SysEx.",
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
    "  change, the tones that play, their names, the channels,",
    "  display lines.",
]

PRESETS = [
    "preset selects a factory tone, user a slot of the user bank. They go "
    "to the tone chosen with edit: A, or B of a split or mix. The third "
    "outlet answers with tonea or toneb, the bank, the number and the name.",
    "The user bank (64 tones, cz4user-bank.syx) and the operation memory "
    "(cz4user-op.syx) are files in the folder of your patch, so every "
    "project keeps its own. cz4.i~ reads them at load if they are there, "
    "otherwise the files beside cz4~ are the starting point. save, "
    "userwrite, userread, opwrite and opread without a file name use them: "
    "store to N in the editor chooses the slot, save tone stores and "
    "writes. Save the patch first, an unsaved patch has no folder of its "
    "own.",
    "There is one user bank and one operation memory in Pd, not one per "
    "object: two cz4.i~ in one patch share them.",
    "op recalls an operation slot: key mode, split point, tone B and the "
    "rest of the OPERATION rows. Tone B is loaded from its bank and number.",
]

MESSAGES = [
    "name v: set a control, in the values the CZ shows.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "line 1/2, env 0-2, edit 0/1: what the line, envelope, rate",
    "  and level controls work on (DCO DCW DCA, tone A or B).",
    "preset n: a factory tone, read only.",
    "user n: a slot of the user bank, 0 to 63.",
    "init: a plain sawtooth tone to start from.",
    "reload: the same tone again, edits dropped.",
    "compare 0/1: hear the stored tone instead of the edit.",
    "store n: the edited tone into user slot n, in memory.",
    "  Without n: the user slot played or stored last.",
    "save: store, then write the user bank (the pads' save).",
    "savearm, saveslot n, savecancel: the pads' store with a",
    "  question: show the slot, move it, drop it. save after",
    "  savearm stores into the slot shown.",
    "presetstep [n], userstep [n]: the factory tone or user",
    "  slot n after the one played last, wrapping (the pads).",
    "name text: rename the edited tone (16 characters).",
    "copyline 1 2: line 1 into line 2 (or 2 1).",
    "copyenv 1 DCW 2 DCW: one envelope into another.",
    "userwrite [file], userread [file]: the user bank, see",
    "  Presets. A file name without a path is beside cz4~.",
    "usersend: the user bank as SysEx.",
    "presets, users: list the names.",
    "op n: recall operation slot n, opstore n: store it,",
    "  opinit: back to one tone, opwrite [file], opread [file]:",
    "  the operation memory, see Presets. ops: the used slots.",
    "sysex bytes: one SysEx message from a list. sysexdump:",
    "  the edited tone as SysEx. czget p, czsend p, czmodel 1/101:",
    "  see SysEx.",
    "bend f: pitch wheel, -1 to 1, times bendrange.",
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

SYSEX = [
    "The object speaks the exclusive format of the CZ series. A tone "
    "travels as nibbles, the low one first: 256 for the 128 bytes of a "
    "CZ-101, 1000, 3000 or 5000, 288 for the 144 bytes of a CZ-1. The "
    "bytes arrive with the rest of the MIDI at MIDI in and leave MIDI out.",
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
    "taken at MIDI in or with sysex b b b.",
]

DATA = [
    "The object needs no files. Without them the factory bank holds the "
    "INIT tone.",
    "Every .syx file in the folder of cz4~ is read for CZ tones (CZ-101, "
    "1000, 3000, 5000 with 128 bytes, CZ-1 with 144) and they join the "
    "factory bank in alphabetical order of the files. Casio's own tones are "
    "not part of this project: put your own dumps there.",
    "cz4user-bank.syx beside cz4~ is a user bank: 64 CZ-1 tones as a "
    "librarian keeps them, so other CZ tools read it as well. "
    "cz4user-op.syx holds operation memory: a message of its own per used "
    "slot (F0 7D, the ID for non-commercial use). For cz4.i~ they are the "
    "starting point, see Presets.",
    "For cz4.i~: cz4~ and cz4.i~.pd in a folder on Pd's search path.",
]

CONTROLS_INTRO = [
    "In the order of the editor: one heading per row, the same title as "
    "the row there, page k on the controller is row k. Each line: name "
    "(the label in the editor), range, CC number and what it does. The "
    "line, envelope, rate and level controls act on the selection (line, "
    "env, edit).",
]

CREDITS = ("Credits: uPD933 model by Devin Acker (MAME), CZ engine after melodee by "
           "Kerem Kilic (BSD-3); CZ tone format after Casio's MIDI specifications; "
           "cz4~ by Peter with help by Claude")


def control_line(nm):
    lo, hi = rng(nm)
    cc = f", CC {CC[nm]}" if nm in CC else ""
    return f"{nm} ({E.label(nm)}) {lo}..{hi}{cc}: {helptext(nm)}"


def main():
    rows = [(f"{title}  {E.cc_text(names)}".replace(":", ""), names) for title, names in E.ROWS]
    H.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
            title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
            sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                      ("text", "Launchkey", helpkit.launchkey("cz4~", "cz4cc", "cz4pads", "")),
                      ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                      ("text", "Presets and user bank", PRESETS),
                      ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                      ("text", "SysEx", SYSEX), ("text", "Data files", DATA),
                      ("controls", CONTROLS_INTRO, rows, control_line)])


if __name__ == "__main__":
    main()
