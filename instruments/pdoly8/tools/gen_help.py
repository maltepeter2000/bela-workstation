#!/usr/bin/env python3
"""Generate the help patch of pdoly8.i~ and pdoly8~ (one patch, written under both names), in the
form of the skill pd-help-layout: no GUI, one example and the reference as text; the settings row
by row with the titles and labels of the editor (tools/gen_editor.py), each with its text from
tools/p8_texts.py. Never edit the help patch by hand: change this script and run it again.

    python3 tools/gen_help.py       writes pd/pdoly8.i~-help.pd and pd/pdoly8~-help.pd
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: helpdraw, helpkit
import gen_editor as E                           # noqa: E402  (rows and labels of p8edit)
import helpdraw as HD                            # noqa: E402
import helpkit                                   # noqa: E402
import p8_texts as X                             # noqa: E402

OBJ = "pdoly8.i~"
FILES = ("pdoly8.i~-help.pd", "pdoly8~-help.pd")
TITLE = "pdoly8.i~ and pdoly8~"
SUBTITLE = "4-voice Poly-800 tone generation - MSM5232 foot synthesis"

SRC = open(os.path.join(HERE, "..", "pd", "pdoly8~.c")).read()
RANGE = {m[0]: (m[2].strip(), m[3].strip()) for m in
         re.findall(r'\{"(\w+)",\'(\w)\',([^,]+),([^,]+),"([^"]*)","([^"]*)"\}', SRC)}


def texts():
    """name -> 'text' from the paragraphs 'name1, name2 (default): text' of p8_texts"""
    out = {}
    for nm, dv, t in X.CONTROLS:
        out[nm] = f"{t} Default {dv}."
    for _, _, _, paras in X.BLOCKS:
        for para in paras:
            m = re.match(r"^([\w]+(?:, [\w]+)*)(?: \(([^)]*)\))?: (.*)$", para)
            if not m:
                continue
            for nm in m[1].split(", "):
                if nm in RANGE and nm not in out:
                    out[nm] = m[3] + (f" Default {m[2]}." if m[2] else "")
    return out


TEXT = texts()


def line(nm):
    lo, hi = RANGE[nm]
    cc = f", CC {E.CC[nm]}" if nm in E.CC else ""
    text = TEXT.get(nm, E.RANGE[nm].lower())
    return f"{nm} ({E.short(nm)}) {lo}..{hi}{cc}: {text}"


OBJECT = [
    "pdoly8.i~ is the whole instrument in one object: pdoly8~, the editor, "
    "the controller layer, the Launchkey pads with their translator p8nav "
    "and the display, MIDI in and out. Put it in a patch, connect the two "
    "left outlets to dac~ and play. Click it to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1, the setting ch1), "
    "argument 2 the channel of the pads (0 or none = 10, the Drums Channel). "
    "Notes and pitch bend come from notein and bendin inside, the controller "
    "layer follows ch1. Several pdoly8.i~ do not get in each other's way, "
    "each has its own editor.",
    "pdoly8~ is the bare engine inside, for patches of your own: notes as "
    "lists from notein, inlet 2 a signal for FM (extfm), outlet 3 its "
    "events. Open pdoly8.i~ to see the wiring.",
]

EDITOR = [
    "The editor is the window of pdoly8.i~. Every change shows up in it "
    "(controller, presets, messages, random).",
    "Rows of 8: rows 1 to 11 are the pages of the controller layer (page k "
    "on the Launchkey = row k), then the rows without CC, titled as in "
    "Controls. Each cell: name and CC number, label, a slider and a number "
    "box with the value as from a pot (0 to 127), the box beside it the real "
    "value.",
    "PRESETS DELAY RANDOM band: factory N (factory program 0 to 63, = "
    "program 11 to 88 of the Poly-800 II), MIDI ch (ch1, 0 = omni: notes and "
    "the controller layer follow it, presets and defaults keep it), the user "
    "bank (user N loads a slot, store to N only chooses one, save patch "
    "stores the sound there and writes the file, load bank reads it again, "
    "factory init fills the user bank with the 64 factory programs: the file "
    "only changes with save patch), the delay presets of the Poly-800 II and "
    "random sounds: pluck, bass, lead, brass, pad, strings, total (keep a "
    "good one with store to and save patch).",
    "POLY-800 II SYSEX band (the user bank): send bank sends it as a data "
    "dump to MIDI out (the Poly-800's MIDI channel must match), write .syx "
    "saves it as a file. A dump from the synth (start it there) and read "
    ".syx replace the user bank, save patch keeps it. The memory outside the "
    "programs (sequencer, chord memory) comes from the factory dump, loaded "
    "at start. The Launchkey does not pass MIDI from the computer to its "
    "MIDI out: use a MIDI interface.",
]

INLETS = [
    "1 <pitch> <velocity> and the messages below.",
]

OUTLETS = [
    "1 signal: left output.",
    "2 signal: right output.",
    "3 anything: value, dump, norm, disp, info, channel,",
    "  display lines.",
]

PRESETS = [
    "The user bank (64 sounds, text, p8user-bank.txt) is kept by the editor "
    "and the pad translator. In pdoly8.i~ it is the file in the folder of "
    "your patch, so every project keeps its own. Until it is there the file "
    "beside pdoly8~ is read, or else the 64 factory programs. save patch in "
    "the editor and the pads' save write the file beside your patch. Save "
    "the patch first, an unsaved patch has no folder of its own.",
    "The factory programs come from presets/poly800mk2-factory.txt beside "
    "pdoly8~. tools/p8sysex.py converts Poly-800 II SysEx dumps to preset "
    "text and back. For lists of your own: p8factory and p8user name (a "
    "[text define -k name] saved with your patch) send 'dump pdoly8 ...' "
    "messages, store N writes.",
]

MESSAGES = [
    "<pitch> <velocity>, note <p> <v>: note on/off (0 = off)",
    "<p> <v> <channel>: from [notein], filtered by ch1",
    "<setting> <value>: e.g. cut 60 (all names: Controls)",
    "ctl <index> <0-127>, params v0 v1 ...: set by index",
    "delay flanger / chorus1 / chorus2 / slap / long / off",
    "bend <-1..1>: pitch bend of all voices, times bnd semitones",
    "n <name> <0-127>: a setting from a pot, CV or MIDI CC,",
    "  scaled to its range (switches in zones, 64 = 0 if bipolar)",
    "nget <name> <tag>: outlet 3 gets norm <tag> <0-127>,",
    "  the setting as a pot position (for pickup)",
    "ninfo <name>: outlet 3 gets info <name> <0-127> <value>,",
    "  pot position and value with unit. Every change is also",
    "  reported this way by itself (editor, controller layer)",
    "disp <name>: outlet 3 gets disp <label> <value>,",
    "  two symbols for a display (OLED)",
    "oled <host> <port>: changed settings to O2O (an OLED) as",
    "  OSC, sent from a thread of its own",
    "oledtext <line 1> <line 2>: two lines for the OLED",
    "defaults, reset: default values, reset also stops all voices",
    "panic: all voices off",
    "get <name>: reply 'value <name> <v>' on outlet 3",
    "print: voices, values and sample rate to the Pd window",
    "dump: send all values to outlet 3 as 'dump pdoly8 v0 ..'",
    "dump pdoly8 v0 v1 ...: restore such a dump",
    "voice <i> <p>, voiceoff <i>: play one voice directly",
    "midiclock: one MIDI clock tick (0xF8) for the delay sync",
    "  (dsyn), from [midirealtimein] -> [sel 248]",
    "bpm <x>: the tempo for dsyn directly, 0 = none",
    "lkgfx 0/1 (1): pictures on the Launchkey display for env1-3,",
    "  the DEG stages and har1/har2 (outlet 3: lkgfx <SysEx bytes>,",
    "  firmware branch display-gfx), 0 = the text line as before",
    "random <kind> [seed]: a new sound, kind = pluck, bass, lead,",
    "  brass, pad, strings or total (the MIDI channel stays)",
    "syxin <byte>: bytes from [sysexin], a whole data dump gives",
    "  bank <n> pdoly8 ... for the 64 programs on outlet 3",
    "syxread <file>, syxwrite <file>: a .syx file",
    "syxbank <n> pdoly8 ...: a preset into program n (0-63)",
    "syxsend: the dump as syxbyte <b> on outlet 3 (-> [midiout])",
    "  needs a dump received or read first (sequencer, chord",
    "  memory and MIDI channel are kept from it)",
]

TIMBRE = [
    "The 15 distinct timbres of the original, dark to bright:",
    "0: 16' / saw net 9: 16'2' / 18: 16'4'2' / 27: 16'4' /",
    "36: 16'8'4' / 45: all / 54: 16'8'2' / 64: 16'8' /",
    "square net 73: 16'8' / 82: 16'8'4' / 91: 16'4' /",
    "100: 16'4'2' / 109: all / 118: 16'8'2' / 127: 16'2'.",
    "One foot or the net per step. The combinations without 16'",
    "are the same waves higher: use oct1 / oct2 2-4.",
    "A fading foot rises in power, so the morph has no dips.",
]

CONTROLS_INTRO = [
    "In the order of the editor: one heading per row, the same title as the "
    "row there, page k on the controller is row k. Each line: name (the "
    "label in the editor), range, CC number and what it does.",
]

# the intros of the former settings blocks (what a group of settings is about)
SETTINGS = [f"{title}: {paras[0]}" for title, _, _, paras in X.BLOCKS
            if paras and not re.match(r"^\w+(, \w+)*( \([^)]*\))?: ", paras[0])]

CREDITS = ("Credits: Modelled on the Korg Poly-800 service manual, MSM5232, scale ROM data "
           "as documented in MAME's msm5232.cpp with help by Claude")


def main():
    rows = [(f"{title}  {E.cc_text(names)}", names) for title, names in E.LEFT + E.RIGHT]
    missing = [n for _, ns in rows for n in ns if n and n not in TEXT]
    if missing:
        print("note: without a text of their own (the label is used):", " ".join(missing))
    HD.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
             title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
             sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                       ("text", "Launchkey", helpkit.launchkey(
                           "pdoly8~", "p8cc", "p8pads", "39-43 random pluck, bass, lead, brass, "
                           "pad, 47 random strings, 48 random total", nav="p8nav")),
                       ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                       ("text", "Presets and user bank", PRESETS),
                       ("lines", "Messages", MESSAGES), ("lines", "Timbre (har1 / har2)", TIMBRE),
                       ("text", "Settings", SETTINGS),
                       ("controls", CONTROLS_INTRO, rows, line)])


if __name__ == "__main__":
    main()
