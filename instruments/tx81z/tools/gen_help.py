#!/usr/bin/env python3
"""Generate the help patch of tx81z.i~ and tx81z~ (firmware emulation; one patch, written under
both names), in the form of the skill pd-help-layout: no GUI, one example and the reference as
text; the voice parameters row by row with the titles and labels of the editor txedit
(tools/gen_editor.py). Never edit the help patch by hand: change this script and run it again.
tools/gen_help_native.py (tx81zn~) takes param_lines() and desc() from here.

    python3 tools/gen_help.py       writes pd/tx81z.i~-help.pd and pd/tx81z~-help.pd
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, helpdraw
import ctlayer as CL                             # noqa: E402
import tx81z_params as T                         # noqa: E402
import gen_editor as E                           # noqa: E402  (rows and labels of txedit)
import helpdraw as HD                            # noqa: E402

OBJ = "tx81z.i~"
FILES = ("tx81z.i~-help.pd", "tx81z~-help.pd")
TITLE = "tx81z.i~ and tx81z~"
SUBTITLE = "Yamaha TX81Z emulation running the original firmware"
ROM, NVRAM = "tx81z-v1.6.bin", "tx81z.nvram"
CC = CL.cc_map(T.CC_ROWS)

# longer descriptions for the parameter list (the label of tools/tx81z_params.py otherwise)
DESC = {
    "alg": "algorithm (1-8 on the TX81Z)", "fb": "feedback of operator 4",
    "lfw": "LFO wave: saw square triangle S/H", "trps": "transpose (24 = middle C)",
    "poly": "poly / mono", "pmod": "portamento mode (full time)", "port": "portamento time",
    "pon": "portamento on / off", "sus": "sustain on / off", "chor": "chorus (set 0)",
    "bcpb": "breath control pitch bias (50 = centre)", "bceb": "breath control EG bias",
    "bcp": "breath control pitch", "bca": "breath control amplitude", "mwp": "mod wheel pitch",
    "mwa": "mod wheel amplitude", "fcv": "foot controller volume", "fcp": "foot controller pitch",
    "fca": "foot controller amplitude", "rev": "reverb rate (0 = off, 7 = fast)",
    "pbr": "pitch bend range in semitones", "pms": "pitch modulation sensitivity",
    "ams": "amplitude modulation sensitivity", "sync": "LFO key sync",
    "pmd": "pitch modulation depth", "amd": "amplitude modulation depth",
}
OP_DESC = {
    "ar": "attack rate", "d1r": "decay 1 rate", "d2r": "decay 2 rate", "rr": "release rate",
    "d1l": "decay 1 level", "ls": "level scaling", "rs": "rate scaling", "ebs": "EG bias sensitivity",
    "ame": "amplitude modulation on / off", "kvs": "key velocity sensitivity", "out": "output level",
    "crs": "frequency coarse", "det": "detune (3 = centre)", "fix": "fixed frequency (0 ratio, 1 fixed)",
    "fixr": "fixed range 250 Hz ... 32 kHz", "fin": "frequency fine", "osw": "waveform W1-W8 (0-7)",
    "sh": "EG shift: 0 96 dB, 1 48, 2 24, 3 12",
}


def desc(nm):
    if nm in DESC:
        return DESC[nm]
    m = re.match(r"([a-z]+)([1-4])$", nm)
    if m and m[1] in OP_DESC:
        return f"{OP_DESC[m[1]]}, operator {m[2]}"
    return T.BY_NAME[nm][6].lower()


def param_line(nm):
    p = T.BY_NAME[nm]
    cc = f", CC {CC[nm]}" if nm in CC else ""
    return f"{nm} ({E.label(nm)}) {p[3]}..{p[4]}{cc}: {desc(nm)}"


def param_rows():
    return [(f"{title}  {E.cc_text(names)}".replace(":", ""), names) for title, names in E.ROWS]


OBJECT = [
    "tx81z.i~ is the TX81Z in one object: tx81z~ (the original firmware on "
    "an emulated CPU), the editor txedit, the controller layer txcc, MIDI in "
    "and out. Put it in a patch, connect the two left outlets to dac~ and "
    "play. Click it to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1). The controller layer "
    "follows the receive channel tx81z~ reports. Page k of the controller "
    "is row k of the editor.",
    f"It needs the firmware {ROM} beside tx81z~ (inside tx81z.i~ the patch "
    "that holds tx81z~ is tx81z.i~.pd itself), see Firmware and battery RAM.",
    "tx81z~ is the bare engine inside, for patches of your own: raw MIDI "
    "bytes from midiin go into its inlet, its outlet 3 reports every change. "
    "Open tx81z.i~ to see the wiring. tx81zn.i~ is the same instrument with "
    "the native engine (no CPU emulation).",
]

EDITOR = [
    "The editor is the window of tx81z.i~. Every change shows up in it "
    "(controller, program change, messages, random).",
    "Rows of 8: rows 1 to 11 are the pages of the controller layer, the last "
    "rows have no CC (mouse only), titled as in Controls. Each cell: name, a "
    "slider and a number box with the value as from a pot (0 to 127), the "
    "box beside it the value as on the TX81Z.",
    "VOICE band: voice name, bank and patch (program change I01-C32, in "
    "EDIT mode tx81z~ presses PLAY first, the firmware ignores program "
    "changes while editing), MIDI ch (receive channel, 0 = omni), the user "
    "bank (user N recalls a slot, store to N chooses one, save patch stores "
    "the voice there and writes the file, load bank reads it again), factory "
    "init, random voices. TX81Z LCD: the display of the firmware.",
    "random: a new voice of that kind into the edit buffer: pluck (harp, "
    "piano, guitar), bass, lead, brass, pad, strings, total (anything that "
    "still sounds). Keep a good one with store to and save patch.",
    "All changes go to the edit buffer of the TX81Z. STORE on the TX81Z "
    "(press store) keeps a voice in bank I of the battery RAM.",
]

INLETS = [
    "1 note, cc, pgm, bend messages, raw MIDI (float, list,",
    "  midi <bytes>) and the messages below.",
]

OUTLETS = [
    "1 signal: left output (~55.9 kHz resampled).",
    "2 signal: right output.",
    "3 anything: lcd, led, info, voice, channel, voicedata.",
]

PRESETS = [
    "The user bank (64 voices, two TX81Z voice bulk dumps) is "
    "txuser-bank.syx in the folder of your patch, so every project keeps its "
    "own. tx81z.i~ reads it at load if it is there, otherwise the bank beside "
    "tx81z~ is the starting point. store and userwrite / userread without a "
    "file name use this file. Save the patch first, an unsaved patch has no "
    "folder of its own. tx81zn.i~ uses the same file.",
    "save is tx81z~'s own: it writes the battery RAM, see Firmware and "
    "battery RAM.",
]

MESSAGES = [
    "note <p> <v> [ch], cc <n> <v> [ch], pgm <n> [ch],",
    "  bend <-8192..8191> [ch]: MIDI to the TX81Z",
    "<float> / list, midi <bytes>: raw MIDI bytes ([midiin])",
    "<name> <value>: a voice parameter, TX81Z data value,",
    "  e.g. alg 4, out1 99 (all names: Controls)",
    "n <name> <0-127>: the same from a pot / CC position",
    "ninfo <name>: outlet 3 gets info <name> <0-127> <value>,",
    "  every change is reported this way by itself",
    "vced <p> <d>, aced <p> <d>: raw parameter change",
    "dumpreq: ask the firmware for the edit buffer (also",
    "  done by itself after every program change)",
    "channel <0-16>: receive channel of the TX81Z, 0 = omni.",
    "  note, cc, pgm, bend without [ch] and SysEx follow it",
    "user <0-63>: recall a slot of the user bank",
    "userstep [n]: the user slot n after the one played last",
    "store [0-63]: the voice into a slot, writes the user bank",
    "userread [file], userwrite [file]: the user bank, see",
    "  Presets. A file name without a path is beside tx81z~.",
    "init: the TX81Z Init Voice",
    "getvoice: outlet 3 gets voicedata <name> <93 VCED>",
    "  <23 ACED>, voicedata ...: that voice into the edit",
    "  buffer (bulk dump)",
    "random <kind> [seed]: a new voice into the edit buffer,",
    "  kind: pluck bass lead brass pad strings total",
    "press <button>, button <button> 0/1: front panel",
    "rom <file>, nvram <file>, save, reset: firmware and",
    "  battery RAM (saved on save and when the patch closes)",
    "factoryinit: presets, MIDI setup, program change on",
    "  (runs by itself if the nvram file does not exist yet)",
    "pcswitch 0/1/2, sysparam <p> <d>: system setup",
    "gain <f>: output gain (default 1)",
]

ABOUT = [
    "The original TX81Z firmware v1.6 runs on an emulated HD6303X CPU, the "
    "YM2414 is ymfm's. A parameter change is sent to the firmware as SysEx "
    "(7 bytes, about 2 ms at 31250 baud). tx81z~ sends only the newest value "
    "of each parameter and only while its MIDI input is idle, so notes are "
    "never delayed by a turning knob. The steps themselves are the "
    "firmware's (e.g. 100 output levels): a fast sweep can sound stepped, as "
    "on the real TX81Z.",
]

FIRMWARE = [
    f"Arguments: [tx81z~ <firmware> <battery RAM file>], paths relative to "
    f"the patch that holds tx81z~ (for tx81z.i~: beside tx81z~, it gives "
    f"{ROM} {NVRAM}). The firmware (64 KB, e.g. {ROM} from the MAME set "
    "tx81z) is not included. The battery RAM file keeps voices, "
    "performances and the system setup. If it does not exist, tx81z~ runs "
    "the factory init (about 26 s of DSP time, it runs only while DSP is "
    "on): test mode loads the presets into the internal memory, then the "
    "front panel sets MIDI Control: receive omni, program change, control "
    "change, pitch bend and Exclusive on. Without Exclusive on the firmware "
    "ignores all SysEx, so parameter changes and the editor would not work. "
    "Then it saves the file.",
]

CONTROLS_INTRO = [
    "The voice parameters in the order of the editor: one heading per row, "
    "the same title as the row there, page k on the controller is row k. "
    "Each line: name (the label in the editor), range (TX81Z data value), "
    "CC number and what it does.",
]

CREDITS = ("Credits: Modelled on the Yamaha TX81Z service manual, HD6303X core and "
           "machine wiring after MAME (Aaron Giles, AJR), YM2414 by ymfm (Aaron Giles) "
           "with help by Claude")


def main():
    HD.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
             title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
             sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                       ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                       ("text", "Presets and user bank", PRESETS),
                       ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                       ("text", "Firmware and battery RAM", FIRMWARE),
                       ("controls", CONTROLS_INTRO, param_rows(), param_line)])


if __name__ == "__main__":
    main()
