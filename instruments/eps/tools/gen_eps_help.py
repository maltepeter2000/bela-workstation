#!/usr/bin/env python3
"""Generate the help patch of eps.i~ and eps~ (one patch, written under both names).
Never edit the help patch by hand: change this script and run it again. Names, ranges and
CC pages come from tools/eps_params.py, the drawing helpers from common/tools/helpdraw.py.

    python3 tools/gen_eps_help.py       writes pd/eps.i~-help.pd and pd/eps~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import ctlayer                                    # noqa: E402
import helpdraw as H                              # noqa: E402
import helpkit                                    # noqa: E402
import eps_params as T                            # noqa: E402
import gen_eps_editor as E                        # noqa: E402  (rows and labels of the editor)

OBJ = "eps.i~"
FILES = ("eps.i~-help.pd", "eps~-help.pd")
TITLE = "eps.i~ and eps~"
SUBTITLE = "Ensoniq EPS / EPS-16 PLUS / ASR-10 instruments"

CC = ctlayer.cc_map(T.CC_ROWS)


def rng(nm):
    return T.CONTROLS[nm][2], T.CONTROLS[nm][3]


OBJECT = [
    "eps.i~ is the whole instrument in one object: eps~, the editor, the controller layer, "
    "the Launchkey pads and display, MIDI in and out. Put it in a patch, put your Ensoniq "
    "instruments into a folder eps beside the patch, connect the two left outlets to dac~ "
    "and play. Click it to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1), argument 2 the channel of the pads "
    "(0 or none = 10, the Drums Channel). Several eps.i~ do not get in each other's way, "
    "each has its own editor and plays its own instrument.",
    "eps~ is the bare engine inside, for patches of your own: inlet 2 takes the velocity of "
    "a note sent as a float, inlet 3 raw MIDI bytes from midiin, and the floats of outlet 3 "
    "(MIDI thru) go to midiout. eps~ alone takes the library beside the external, or the "
    "folder given with library. Open eps.i~ to see the wiring.",
]

EDITOR = [
    "The editor is the window of eps.i~. Every change shows up in it, from the controller, "
    "an instrument change or messages. refresh asks for all values again.",
    "Rows of 8 below the PATCH band, titled as in Controls: row k is page k of the "
    "controller layer. Each cell: name and label, a slider and a number box with the value as "
    "from a pot (0 to 127), the box beside it the value itself.",
    "PATCH band. name: the instrument that plays. library N: load instrument N of the "
    "library. user N: a user slot. reload: the instrument or slot again, edits dropped. init: "
    "all controls back to 0, the instrument stays. store to N: chooses the user slot, save "
    "patch stores there and writes the user bank, see Presets. load bank, write bank: read or "
    "write the bank file without storing. rescan library: read the folder again after adding "
    "files.",
    "MIDI in: the receive channel, 0 = all. out: the channel of the controller feedback. "
    "MIDI thru: see Messages, thru. library size: how many instruments the folder holds.",
]

INLETS = [
    "1 float: note number, velocity 100.",
    "1 list: note, velocity and, if you like, the MIDI channel.",
    "  MIDI in eps.i~: notes, bend, pressure (channel and poly),",
    "  program change, CC 1 wheel, 2 XCTRL, 4 pedal, 7 volume,",
    "  64 sustain.",
    "1 anything: a message, see Messages.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: info, name, 0-127 and value text for every",
    "  change, the instrument that plays, its name, the channels.",
    "3 loaded model layers wavesamples: an instrument is in.",
    "3 display title value: two lines for a display: a control",
    "  (Cutoff, -12), the instrument (Preset 3, CLASSIC_PAD),",
    "  Loading, a store (Stored, User 11).",
]

MESSAGES = [
    "name v: set a control, see Controls.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "cycle name [-1]: the next (previous) value, round and round.",
    "dump: every control as info.",
    "library [dir]: use this folder as the library and read",
    "  its list. Without dir: eps beside the external.",
    "rescan: read the library folder again.",
    "preset n: load library instrument n (from 0), controls",
    "  back to 0.",
    "presetstep 1 or -1: the next or previous instrument.",
    "open file [k]: load instrument k (from 0) of any file,",
    "  without the library. A name without a path is beside",
    "  the external.",
    "presets: list the library as preset n name.",
    "user n: a user slot, 0 to 63: its instrument and controls.",
    "userstep 1 or -1: the next or previous used user slot.",
    "init: every stored control back to 0, the instrument stays.",
    "reload: the instrument or slot again, edits dropped.",
    "store n: the playing instrument and its controls into",
    "  user slot n, in memory. Without n: the slot chosen or",
    "  stored last.",
    "save: store, then write the user bank (the pads' save).",
    "savearm, saveslot n, savecancel: the pads' store with a",
    "  question: show the slot, move it, drop it.",
    "userwrite [file], userread [file]: the user bank, see",
    "  Presets.",
    "users: list the used user slots.",
    "bend f: pitch wheel, -1 to 1, times bend.",
    "mod f: modulation wheel, 0 to 1 (EPS source WHEEL).",
    "pressure f: channel pressure, 0 to 1 (PRESS).",
    "polypressure note f: pressure of one key (PRESS).",
    "pedal f, xctrl f: the sources PEDAL and XCTRL, 0 to 1.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback, 1 to 16.",
    "thru 0/1: MIDI thru, on by default.",
    "status: report instrument, channels, library size and",
    "  sounding voices.",
    "stop, reset: all notes off.",
]

PRESETS = [
    "The library is every instrument in every file of the folder eps beside the patch "
    "(EFE, IMG, EDE, EDA, GKH, also images of hard disks and SD cards in Ensoniq format, HDA, "
    "should work too but are not tested yet), in "
    "the order of the file names and of the instruments in each disk image. preset N, "
    "presetstep and program changes load from it. An instrument is read in the background: "
    "notes go on sounding until it is in, then it takes over. Your Ensoniq files are not part "
    "of eps~ and are never in git.",
    "The user bank (64 slots) is epsuser-bank.txt in the folder of your patch, a text file: "
    "per slot the file, the instrument and the controls. eps.i~ reads it at load if it is "
    "there, otherwise the bank beside eps~ is the starting point. save, userwrite and "
    "userread without a file name use this file: store to N in the editor chooses the slot, "
    "save patch stores and writes. Save the patch first, an unsaved patch has no folder of "
    "its own.",
    "There is one user bank and one library in Pd, not one per object: two eps.i~ in one "
    "patch share them.",
    "volume and voices belong to the instrument object. An instrument change leaves them "
    "alone and they are not stored.",
]

ABOUT = [
    "eps~ reads the instruments of the Ensoniq EPS, EPS-16 PLUS and ASR-10 from their own "
    "files: layers, wavesamples with their samples, loops and loop modes, envelopes, filters, "
    "pan, patch select and the wave modulation (transwave). It plays them on a model of the "
    "ES5505 voice chip of the EPS-16 PLUS, taken from MAME: interpolation, the 4-pole filter "
    "in its four arrangements, the 8-bit logarithmic volume, forward, backward and "
    "bidirectional loops, the 2/1024 pitch steps.",
    "What the EPS operating system does is rebuilt on top: voice allocation, layers and patch "
    "select, the three envelopes (pitch, filter, amp) with the two-stage release, attack "
    "velocity, keyboard time scaling and the finite mode, the LFO with its seven waves, delay, "
    "reset and humanize, random pitch, the modulation of pitch, both filters, amp and pan from "
    "the 16 sources (LFO, ENV1-3, three velocity curves, PR+VL, KBD, KBD2, WHEEL, PEDAL, XCTRL, "
    "PRESS, WHL+PR, PITCH), crossfades, layer glide and all seven wave modulation types "
    "(loop position, sample start, both, loop start, loop end, loop start-X, transwave).",
    "Calibrated against the original operating system (OS 1.30 in an emulator, register "
    "traces of test instruments): cutoff scale of both filters, volume and envelope level law, "
    "pan law, boost, envelope times and shapes, release, LFO speeds and depth, wave "
    "modulation types and range, the pitch, "
    "filter and key amounts, the mod sources 0-11. Still approximations: the soft/hard "
    "velocity levels, amp and pan modulation amounts, crossfade curves, glide (every mode is "
    "mono with glide). Not there: pitch tables and the effects of the ES5510. The controls of "
    "eps~ are offsets over the instrument, 0 plays it as stored.",
    "The output runs at Pd's sample rate, not at the 29.76 kHz of the EPS-16 PLUS. Up to 32 "
    "voices, 20 by default.",
]

DATA = [
    "Building needs no data. To play, put Ensoniq instruments into the folder eps beside your "
    "patch: single files (.efe from EpsLin, EnDiskEx or Chicken Translator) or whole disk "
    "images (.img from a floppy, an SD card of a ZuluSCSI or BlueSCSI, .ede and .eda from "
    "Giebler's tools, .gkh). HFE floppy images are converted to IMG first. The EPS, the "
    "EPS-16 PLUS and the ASR-10 share the format, tested so far with EPS-16 PLUS instruments.",
    "For eps.i~ two files are enough: eps~ (the external) and eps.i~.pd in a folder on Pd's "
    "search path.",
]

CREDITS = ("Credits: Modelled on the Ensoniq EPS-16 PLUS; ES5505 after MAME's es5506.cpp by "
           "Aaron Giles; file format after ConvertWithMoss by Juergen Mossgraber with help by "
           "Claude")

CONTROLS_INTRO = [
    "In the order of the editor: one heading per row, the same title as the row there, page k "
    "on the controller is row k. Each line: name (the label in the editor), range, CC number "
    "and what it does. Page 1 follows the other instruments: bright, resonance, attack, "
    "release, vibrato, effect (free for now), own (the wave position) and volume.",
]

HELP = {nm: T.CONTROLS[nm][6] for nm in T.CONTROLS}


def control_line(nm):
    lo, hi = rng(nm)
    cc = f", CC {CC[nm]}" if nm in CC else ""
    return f"{nm} ({E.label(nm)}) {lo}..{hi}{cc}: {HELP[nm]}"


def main():
    rows = [(f"{title}  {E.cc_text([n for n in names if n])}".replace(":", ""), [n for n in names if n])
            for title, names in E.ROWS]
    H.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
            title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
            sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                      ("text", "Launchkey", helpkit.launchkey(
                          "eps~", "epscc", "epspads", "39 patch select +, 47 patch select -")),
                      ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                      ("text", "Presets and user bank", PRESETS),
                      ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                      ("text", "Data files", DATA),
                      ("controls", CONTROLS_INTRO, rows, control_line)])


if __name__ == "__main__":
    main()
