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
    "whole library. reload: the instrument again, edits dropped. init: all controls back to "
    "0, the instrument stays. rescan: read the library folder again after adding files.",
    "folder: where you are, as on the EPS: library is the top (disk images and EFE files), "
    "an image and its directories below (DISK1.img/BASSES). up: one folder up. file: the "
    "selected file with the arrows before and after it, a directory ends in a slash. open: "
    "enter the directory or load the instrument. Loading an instrument shows its folder.",
    "Saving always writes the Ensoniq format, so the EPS reads it. The offsets of the "
    "controls (cutoff, attack ...) go into the instrument first and are set back to 0. "
    "replace: over the file the instrument was loaded from. save copy: a new file in the "
    "folder shown, named after the instrument with a number. new name and save as: a new "
    "file of that name in the folder shown (in an image, or an EFE at the top). new folder: "
    "a directory of that name, inside an image. Names: up to 12 characters, A-Z 0-9 space * "
    "- + (what the EPS lets you type), underscore for a space. See Library and saving.",
    "MIDI in: the receive channel, 0 = all. out: the channel of the controller feedback. "
    "thru: see Messages, thru. size: files in the folder, library size: instruments in all.",
    "The other rows edit the loaded instrument itself (mouse only, see Instrument rows). "
    "SELECT band: the layer and the wavesample the LAYER and WS rows show, with their names "
    "and the key range, root key, rate and length of the wavesample. envelope: which one the "
    "ENV rows show (1 pitch, 2 filter, 3 amp). follow key (on by default): a played key "
    "selects its layer and wavesample, the rows jump to their values. zero crossings (on by "
    "default): sample and loop points move to the nearest rising zero crossing.",
    "Sample and loop points have two cells each: the first sets the position coarsely, in "
    "steps of 1/127 of the sample, the second (fine) moves it by -64 to +63 samples from "
    "there. The box shows the sample number. Edits change the playing instrument at once "
    "(the next note, filter, amp, pan, LFO and wave modulation also sounding notes). They "
    "are kept with replace, save copy or save as, a reload or an instrument change drops "
    "them.",
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
    "  Loading, saving (Saved, PAD_BRIGHT).",
    "3 folder text n, bsel k n kind name: the folder shown,",
    "  its size and the selected file (kind dir or inst).",
    "3 saved: an instrument was written.",
]

MESSAGES = [
    "name v: set a control, see Controls.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "cycle name [-1]: the next (previous) value, round and round.",
    "dump: every control as info.",
    "e name v: set an instrument parameter (see Instrument",
    "  rows) from 0 to 127. einfo name: report it as info.",
    "edump: the selection (sel) and every instrument parameter.",
    "sel layer N, sel ws N, sel env N (1-3): select what the",
    "  LAYER, WS and ENV rows edit; sel layerstep / wsstep +-1.",
    "  ws N counts within the layer (WS 1..n of it, as on the EPS).",
    "  Answers sel <layer> <ws> <env> <layers> <layer name>",
    "  <ws name> <info> <wavesamples in the layer>.",
    "follow 0/1: a played key selects its layer and wavesample.",
    "zerox 0/1: points snap to zero crossings.",
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
    "browse: report the folder shown (folder, bsel).",
    "bstep 1 or -1, bselect k: move the selection in the folder.",
    "benter: enter the selected directory or load the selected",
    "  instrument. bup: the folder above.",
    "userstep 1 or -1: the next or previous instrument in the",
    "  folder of the one playing (the pads' user + and -).",
    "init: every control back to 0, the instrument stays.",
    "reload: the instrument again, edits dropped.",
    "saveas name: write the instrument as a new file into the",
    "  folder shown: into the image, or name.efe at the top.",
    "  Refused if the name is there.",
    "replace: write it over the file it was loaded from.",
    "save: saveas with a free name, the instrument's and a",
    "  number (the pads' save).",
    "mkdir name: a new directory in the folder shown (images).",
    "SysEx with a real EPS-16 PLUS (MIDI both ways, SYS-EX ON in",
    "  its EDIT / System-MIDI page):",
    "epsch n: the EPS's MIDI base channel, 1-16 (default 1).",
    "fetch i: instrument i (1-8) from the EPS's memory into the",
    "  folder shown as a new file, then loaded. The effect does",
    "  not come along (SysEx cannot carry it).",
    "fetchws i l w: wavesample w of layer l as name.wav into the",
    "  library folder.",
    "send i, sendws i l w file.wav: into the EPS (not finished).",
    "sxcancel: stop a transfer. Outlet 3: sysex step percent,",
    "  fetched, sent.",
    "  All three write the offsets into the instrument first.",
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
    "The library is the folder eps beside the patch: Ensoniq disk images (IMG from a floppy, "
    "HDA or IMG images of the hard disk on the SD card of a ZuluSCSI or BlueSCSI, EDE, EDA, "
    "GKH) and EFE files. The editor shows it as on the EPS: each image is a folder with its "
    "directories, the EFE files lie at the top. preset N, presetstep and program changes "
    "go through all instruments, in the order of the file names and the directories. An "
    "instrument is read in the background: notes go on sounding until it is in.",
    "Saving keeps the Ensoniq format: into an image the file goes as the EPS writes it "
    "(checked against the original operating system: it lists and loads what eps~ saved), "
    "at the top level as an EFE. So your library can go back onto the SD card of the "
    "ZuluSCSI as it is. A directory holds 38 files, names have 12 characters.",
    "Before eps~ writes into an image it checks it: the free block count must match the "
    "FAT and every directory its file count, otherwise it refuses and changes nothing. Every "
    "block it changes is first copied to <image>.undo beside the image: build/eps_fs_tool "
    "undo <image> puts the image back as it was before the first change (and deletes the "
    "journal). Delete the .undo file once you are happy, and keep a copy of your SD card "
    "anyway.",
    "There is one library in Pd, not one per object: two eps.i~ in one patch share it. "
    "Save the patch first, an unsaved patch has no folder of its own.",
    "The controls of eps~ are offsets over the instrument while you play. Saving writes "
    "transpose, tune, cutoff, filter 2, filter env, attack, decay and release into the "
    "instrument (into every wavesample) and sets them back to 0. vibrato, wave position, "
    "bend range, patch select and vibrato rate are playing controls, the EPS has no place "
    "for them in an instrument: they stay. volume and voices belong to the instrument "
    "object.",
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
    "the 15 sources (LFO, RANDM, ENV1, ENV2, PR+VL, VEL, VEL1, VEL2, KBD, PITCH, WHEEL, PEDAL, "
    "XCTRL, PRESS, WL+PR, numbered as in the External Command Specification), crossfades, layer glide and all seven wave modulation types "
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


import eps_edit_params as EP                       # noqa: E402

EDIT_INTRO = [
    "The rows below the controls edit the instrument (mouse only, message e). Each line: "
    "name (label) and range of the value as stored, what it is.",
]


def edit_line(nm):
    _, lbl, scope, _, lo, hi, kind, hlp = EP.P[nm]
    rng_ = "0..127" if scope in "pf" else f"{lo}..{hi}"
    return f"{nm} ({lbl}) {rng_}: {hlp}"


def main():
    rows = [(f"{title}  {E.cc_text([n for n in names if n])}".replace(":", ""), [n for n in names if n])
            for title, names in E.ROWS]
    H.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
            title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
            sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                      ("text", "Launchkey", helpkit.launchkey(
                          "eps~", "epscc", "epspads", "39 patch select +, 47 patch select -")),
                      ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                      ("text", "Library and saving", PRESETS),
                      ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                      ("text", "Data files", DATA),
                      ("controls", CONTROLS_INTRO, rows, control_line),
                      ("controls", EDIT_INTRO, [(t, [n for n in ns if n]) for t, ns in EP.ROWS_LEFT + EP.ROWS_RIGHT],
                       edit_line, "Instrument rows")])


if __name__ == "__main__":
    main()
