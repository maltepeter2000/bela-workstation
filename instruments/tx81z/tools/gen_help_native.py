#!/usr/bin/env python3
"""Generate the help patch of tx81zn.i~ and tx81zn~ (native engine; one patch, written under both
names), in the form of the skill pd-help-layout: no GUI, one example and the reference as text;
the FM view row by row as in the editor txfm, then every voice parameter as in txedit. Never
edit the help patch by hand: change this script and run it again.

    python3 tools/gen_help_native.py       writes pd/tx81zn.i~-help.pd and pd/tx81zn~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, helpdraw, helpkit
import ctlayer as CL                             # noqa: E402
import gen_fmedit as F                           # noqa: E402  (pages and labels of txfm)
import gen_help as TXH                           # noqa: E402  (voice parameters, as txedit)
import helpdraw as HD                            # noqa: E402
import helpkit                                   # noqa: E402

OBJ = "tx81zn.i~"
FILES = ("tx81zn.i~-help.pd", "tx81zn~-help.pd")
TITLE = "tx81zn.i~ and tx81zn~"
SUBTITLE = "Yamaha TX81Z sound engine rebuilt natively"
ROM, NVRAM = "tx81z-v1.6.bin", "tx81z.nvram"

# descriptions of some FM controls (the label of txfm otherwise)
FMDESC = {
    "algo": "algorithm 1-8, it sets the roles C / A / B",
    "ratc": "frequency ratio of the carriers C",
    "rata": "frequency ratio of modulator A",
    "ratb": "frequency ratio of the modulators B",
    "moda": "output level of A = brightness",
    "modb": "output level of B",
    "fdbk": "feedback of operator 4, 0-7",
    "mix": "carriers against each other (only with several: algorithms 5-8)",
}

ROLES = "Algorithms (> = modulates) and roles: " + ", ".join(
    f"{k + 1} {g} {c} {a} {b}" for k, (g, c, a, b) in enumerate(F.ALGS)) + "."

FM = ["The FM view gives the four operators roles from the algorithm: C = the carriers (what you "
      "hear), A = the modulator of the main carrier OP1, B = the other modulators. A control of a "
      "role changes all of its operators and shows the value of the first one. In the end everything "
      "is an ordinary TX81Z voice parameter: the voice stays a TX81Z voice (txedit shows every "
      "single value).",
      "Ratio shows the real frequency ratio of coarse and fine. Mix: left the main carrier OP1, right "
      "the other carriers (the louder side keeps its level). Detune spreads all operators around the "
      "centre. ENV C is the amplitude, ENV A and ENV B are the brightness. Changing the algorithm "
      "keeps the operator settings: an operator that becomes a carrier sounds with its output level."]
FM.insert(1, ROLES)

OBJECT = [
    "tx81zn.i~ is the TX81Z in one object: tx81zn~ (the voice routines of "
    "the firmware rebuilt natively), the FM editor txfm, the controller "
    "layer txfmcc, the Launchkey pads and display, MIDI in and out, and "
    "txedit with every single voice parameter. Put it in a patch, connect "
    "the two left outlets to dac~ and play. Click it to open the FM editor, "
    "see Editors.",
    "Argument 1 is the MIDI channel (0 or none = 1), argument 2 the channel "
    "of the pads (0 or none = 10, the Drums Channel). Several tx81zn.i~ do "
    "not get in each other's way, each has its own editors.",
    f"It needs the firmware {ROM} beside tx81zn~ (tables and preset banks A "
    "to D are read from it, inside tx81zn.i~ the patch that holds tx81zn~ is "
    "tx81zn.i~.pd itself), see ROM and battery RAM.",
    "tx81zn~ is the bare engine inside, for patches of your own: raw MIDI "
    "bytes from midiin go into its inlet, its outlet 3 reports every change. "
    "Open tx81zn.i~ to see the wiring.",
]

EDITORS = [
    "The FM editor txfm is the window of tx81zn.i~: the rows of the FM view "
    "(Controls), row k = page k of txfmcc. On top: voice name, bank I A B C "
    "D and patch, the algorithm with its roles, random voices.",
    "txedit (pd txedit below the editor, click it): all 100 voice parameters "
    "in rows of 8 (Voice parameters), voice name, program change, MIDI "
    "channel, the user bank. Both work on the same bus and stay in sync.",
    "Each cell: label, a slider and a number box with the value as from a "
    "pot (0 to 127), the box beside it the value as on the TX81Z (- when "
    "the role has no operator in this algorithm).",
]

INLETS = [
    "1 note, cc, pgm, bend messages, raw MIDI (float, list,",
    "  midi <bytes>) and the messages below.",
]

OUTLETS = [
    "1 signal: left output (~55.9 kHz resampled).",
    "2 signal: right output.",
    "3 anything: info, fm, voice, voicedata, channel, status,",
    "  display <title> <value>, lcd (splash, once).",
]

PRESETS = [
    "bank <I|A|B|C|D> <1-32> selects any voice, bank D included. Program "
    "change 0-127 selects I01-I32 and A01-C32 as on the TX81Z. Bank I comes "
    "from the battery RAM file, without it bank I is empty.",
    "The user bank (64 voices, two TX81Z voice bulk dumps) is "
    "txuser-bank.syx in the folder of your patch, so every project keeps "
    "its own. tx81zn.i~ reads it at load if it is there, otherwise the bank "
    "beside tx81zn~ is the starting point. save, store and userwrite / "
    "userread without a file name use this file. Save the patch first, an "
    "unsaved patch has no folder of its own. tx81z.i~ uses the same file.",
]

MESSAGES = [
    "note <p> <v> [ch], cc <n> <v> [ch], pgm <n> [ch],",
    "  bend <-8192..8191> [ch]",
    "<float> / list, midi <bytes>: raw MIDI bytes ([midiin]),",
    "  SysEx parameter changes and voice dumps included",
    "bank <I|A|B|C|D> <1-32>: select a voice",
    "fm <control> <0-127>, fminfo <control>: FM view",
    "<name> <value>: a voice parameter, TX81Z data value,",
    "  e.g. alg 4, out1 99 (all names: Voice parameters)",
    "n <name> <0-127>: the same from a pot / CC position",
    "ninfo <name>: outlet 3 gets info <name> <0-127> <value>,",
    "  every change is reported this way by itself",
    "vced <p> <d>, aced <p> <d>: raw parameter change",
    "dumpreq: report every value again (info, fm, channel)",
    "getvoice: outlet 3 gets voicedata <name> <93 VCED>",
    "  <23 ACED>, voicedata ...: load that voice",
    "random <kind> [seed]: a new voice,",
    "  kind: pluck bass lead brass pad strings total",
    "presetstep [n], userstep [n]: ROM preset A01-D32 / user",
    "  slot n after the one played last (the Launchkey pads)",
    "init: the TX81Z Init Voice",
    "user <0-63>: recall a slot of the user bank",
    "store [0-63], save: the voice into a slot (save: the last",
    "  one), writes the user bank (see Presets)",
    "savearm, saveslot n, savecancel: the pads' store with a",
    "  question: show the slot, move it, drop it. save after",
    "  savearm stores into the slot shown.",
    "userread [file], userwrite [file]: the user bank. A file",
    "  name without a path is beside tx81zn~.",
    "channel <0-16>: receive channel, 0 = omni",
    "tune <0-127>: master tune (64 = centre)",
    "rom <file>, nvram <file>: load (the nvram is only read)",
    "factoryinit: all notes off, voice A01",
    "gain <f>: output gain (default 1)",
]

ABOUT = [
    "The voice, note, modulation and controller routines of the TX81Z "
    "firmware v1.6 are rebuilt in C++ and drive the YM2414 directly, with "
    "the same register values as the firmware (tested against the firmware "
    "running in tx81z~). A YM2414 core derived from ymfm, bit-identical to "
    "it, renders the sound. About a third to a sixth of the CPU of tx81z~, "
    "and parameter changes take effect at once (no SysEx round trip).",
    "Single mode only: no performance mode, no micro tuning, no front panel. "
    "The display shows the splash screen once (lcd), the voice name comes as "
    "voice <name>.",
]

ROMTEXT = [
    "Arguments: [tx81zn~ <firmware> <battery RAM file>], paths relative to "
    f"the patch that holds tx81zn~ (for tx81zn.i~: beside tx81zn~, it gives "
    f"{ROM} {NVRAM}). The firmware (64 KB, e.g. {ROM} from the MAME set "
    "tx81z) is not included: tx81zn~ reads its tables and the preset banks "
    "A-D from it. The battery RAM file of tx81z~ is optional and only read: "
    "bank I, master tune and receive channel. tx81zn~ never writes it.",
]

FM_INTRO = [
    "The FM view in the order of the editor txfm: one heading per row, the "
    "same title as the row there, page k on the controller is row k. Each "
    "line: name (the label in the editor), range, CC number and what it "
    "does. Set them with fm <control> <0-127>.",
]

CREDITS = ("Credits: Modelled on the Yamaha TX81Z firmware v1.6 and service manual, "
           "YM2414 after ymfm (Aaron Giles) with help by Claude")


def fm_rows():
    rows = []
    for k, title in enumerate(F.PAGES):
        names = [m[0] for m in F.MACROS if m[2] == k]
        rows.append((f"PAGE {k + 1}  {title}  CC {CL.page_text(k)}" if k < len(CL.PAGES) else title, names))
    return rows


LABEL = {m[0]: m[1] for m in F.MACROS}
PAGE = {m[0]: m[2] for m in F.MACROS}


def fm_line(nm):
    k = PAGE[nm]
    idx = [m[0] for m in F.MACROS if m[2] == k].index(nm)
    cc = f", CC {CL.PAGES[k][idx]}" if k < len(CL.PAGES) and idx < len(CL.PAGES[k]) else ""
    text = f": {FMDESC[nm]}" if nm in FMDESC else ""
    return f"{nm} ({LABEL[nm]}) 0..127{cc}{text}"


def main():
    HD.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
             title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
             sections=[("text", OBJ, OBJECT), ("text", "Editors", EDITORS),
                       ("text", "Launchkey", helpkit.launchkey(
                           "tx81zn~", "txfmcc", "txpads", "39-43 random pluck, bass, lead, brass, "
                           "pad, 47 random strings, 48 random total")),
                       ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                       ("text", "Presets and user bank", PRESETS),
                       ("lines", "Messages", MESSAGES), ("text", "FM view", FM),
                       ("text", "The instrument", ABOUT),
                       ("text", "ROM and battery RAM", ROMTEXT),
                       ("controls", FM_INTRO, fm_rows(), fm_line),
                       ("controls", TXH.CONTROLS_INTRO, TXH.param_rows(), TXH.param_line,
                        "Voice parameters (txedit)")])


if __name__ == "__main__":
    main()
