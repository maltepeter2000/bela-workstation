#!/usr/bin/env python3
"""Generate the help patch of d50.i~ and d50~ (one patch, written under both names), in the
form of the skill pd-help-layout: no GUI, one example and the reference as text; the controls
row by row with the titles and labels of the editor (tools/gen_editor.py). Never edit the help
patch by hand: change this script and run it again.

    python3 tools/gen_help.py       writes pd/d50.i~-help.pd and pd/d50~-help.pd
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, helpdraw, helpkit
import ctlayer                                    # noqa: E402
import d50_params as T                            # noqa: E402
import gen_editor as E                            # noqa: E402  (rows and labels of the editor)
import helpdraw as H                              # noqa: E402
import helpkit                                    # noqa: E402

OBJ = "d50.i~"
FILES = ("d50.i~-help.pd", "d50~-help.pd")
TITLE = "d50.i~ and d50~"
SUBTITLE = "Roland D-50 linear arithmetic synthesizer"

CC = ctlayer.cc_map(T.CC_ROWS)
PERF_HELP = dict(T.MACRO_HELP)
PERF_HELP["reverb"] = "reverb balance of the patch"
PERF_HELP["balance"] = "balance of the two tones, lower at 0, upper at 100"


def rng(nm):
    _, _, lo, hi, _, _ = T.CONTROLS[nm]
    return lo, hi


OBJECT = [
    "d50.i~ is the whole instrument in one object: d50~, the editor, the "
    "controller layer, the Launchkey pads and display, MIDI in and out. Put it in a patch, connect the two "
    "left outlets to dac~ and play. Click it to open the editor, see Editor.",
    "Argument 1 is the MIDI channel (0 or none = 1), argument 2 the channel "
    "of the pads (0 or none = 10, the Drums Channel). Several d50.i~ do not "
    "get in each other's way, each has its own editor. Page 1 of the "
    "controller is the performance row, then the selected partial and its "
    "tone: page k is row k of the editor.",
    "d50~ is the bare engine inside, for patches of your own: inlet 2 takes "
    "the velocity of a note sent as a float, inlet 3 raw MIDI bytes from "
    "midiin (notes, bend, aftertouch, program change, CC 1, 5, 7, 64, 65 "
    "and SysEx), and the floats of outlet 3 (SysEx) go to midiout. Open "
    "d50.i~ to see the wiring.",
]

EDITOR = [
    "The editor is the window of d50.i~. Every change shows up in it, from "
    "the controller, a patch change, messages or SysEx. refresh asks for "
    "all values again.",
    "Rows of 8 below the two bands, titled as in Controls: rows 1 to 11 are "
    "the pages of the controller layer, the rows after them have no CC "
    "(mouse only). Each cell: name and label, a slider and a number box "
    "with the value as from a pot (0 to 127), the box beside it the value "
    "as on the D-50.",
    "PARTIAL SELECT: the D-50 has four partials, two per tone. The PARTIAL "
    "rows (blue) show and change the selected one, U1 U2 L1 L2, the TONE "
    "rows (beige) the tone it belongs to, upper for U1 and U2, lower for L1 "
    "and L2. It is the control part, first cell of page 2, so the "
    "controller can switch it too. upper tone, lower tone: their names.",
    "PERFORMANCE (row 1) acts on the whole patch: cut moves the cutoff of "
    "every synth partial, also of notes that are sounding. res, att, rel and "
    "vib shift resonance, attack, release and vibrato of all partials for "
    "the notes that follow. These are not stored and go back to 0 with "
    "every patch change. The other controls of the row are patch values, "
    "volume is the master.",
    "An edit of a partial or tone value is heard from the next note on. "
    "Chorus, equalizer, reverb and the balances follow at once. The "
    "built-in presets cannot be edited: load a bank, or start from init or "
    "a user slot.",
    "PATCH band. name: type a new patch name and press enter, up to 18 "
    "characters (upper and lower tone: 10). factory N: a factory preset, "
    "numbered through all .syx banks beside d50~. user N: a user slot, 0 to "
    "63. reload: the same patch again, edits dropped. init patch: a plain "
    "sawtooth. store to N: chooses the user slot, save patch stores the "
    "playing patch there and writes the user bank, see Presets. load bank, "
    "write bank: read or write the bank file without storing. send bank, "
    "send patch: as SysEx to MIDI out, see SysEx.",
    "MIDI in: the receive channel, 0 = all. out: the channel of the "
    "controller feedback and the device number of SysEx dumps.",
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
    "  change, the patch that plays, its names, the channels.",
]

PRESETS = [
    "preset selects a factory preset, user a slot of the user bank. The "
    "third outlet answers with the number and the name. A patch change "
    "sets cut, res, att, rel and vib back to 0.",
    "The user bank (64 slots) is d50user-bank.syx in the folder of your "
    "patch, so every project keeps its own. d50.i~ reads it at load if it "
    "is there, otherwise the bank beside d50~ is the starting point. save, "
    "userwrite and userread without a file name use this file: store to N "
    "in the editor chooses the slot, save patch stores and writes. Save the "
    "patch first, an unsaved patch has no folder of its own.",
    "There is one user bank in Pd, not one per object: two d50.i~ in one "
    "patch share it.",
    "tune and voices (SETUP) belong to the instrument and are not stored in "
    "a patch.",
]

MESSAGES = [
    "name v: set a control, in the values the D-50 shows.",
    "name: report it as info.",
    "n name v: set it from a pot or a MIDI CC, 0 to 127.",
    "ninfo name: report it as info.",
    "part n: the partial that partial names act on, 0 to 3",
    "  (U1, U2, L1, L2).",
    "preset n: a factory preset, read only.",
    "user n: a slot of the user bank, 0 to 63.",
    "init: a plain sawtooth patch to start from.",
    "reload: the same patch again, edits dropped.",
    "store n: the playing patch into user slot n, in memory.",
    "  Without n: the user slot played or stored last.",
    "save: store, then write the user bank (the pads' save).",
    "presetstep [n], userstep [n]: the factory patch or user",
    "  slot n after the one played last, wrapping (the pads).",
    "userwrite [file], userread [file]: the user bank, see",
    "  Presets. A file name without a path is beside d50~.",
    "usersend: the user bank as SysEx.",
    "presets, users: list the names.",
    "l2 cutoff 30: section, name and value, without changing",
    "  the selection. Sections: u1 u2 upper l1 l2 lower patch.",
    "  Without a value it reports the value.",
    "param a v: one of the 448 patch bytes by address, the",
    "  section times 64 plus the offset. param a reports it.",
    "patch name My Patch: the patch name, up to 18 characters.",
    "  upper name, lower name: up to 10.",
    "dump: every named parameter of every section as info.",
    "sysexdump: the playing patch as SysEx.",
    "sysex bytes: one SysEx message from a list.",
    "bend f: pitch wheel, -1 to 1, times bendrange.",
    "mod f: modulation lever, 0 to 1.",
    "aftertouch f: channel pressure, 0 to 1.",
    "sustain 0/1: hold pedal.",
    "channel n: MIDI receive channel, 0 is all.",
    "txchannel n: channel of the controller feedback and device",
    "  number of SysEx dumps, 1 to 16.",
    "governor 0/1: when 1, the oldest released note ends early",
    "  whenever a block takes too long. Held notes never end.",
    "  For small machines such as Bela. Off by default.",
    "status: report patch, names, channels, voices and CPU.",
    "stop: all notes off.",
    "reset: all notes off and the effect tails emptied.",
]

ABOUT = [
    "A Roland D-50 in one object: linear arithmetic synthesis with two "
    "tones of two partials each. A partial is a synth partial (oscillator, "
    "filter, amplifier) or a PCM sample, structure sets which and whether "
    "they are ring modulated. Each patch has its own chorus, equalizer and "
    "reverb, the output is stereo. Eight voices per tone.",
    "keymode sets how the tones play: whole, dual, split and their solo "
    "variants. The engine runs at 32 kHz, the rate of the original, and is "
    "resampled to the rate of Pd.",
]

SYSEX = [
    "The object speaks the exclusive format of the D-50, so an editor or "
    "librarian made for the D-50 can program it, and it can exchange "
    "patches with a real D-50, over MIDI in and out.",
    "Data set (DT1) to address 00-00-00 edits the playing patch, the same "
    "448 bytes that param reaches. Data set to 02-00-00, the internal "
    "memory of the D-50, goes to the user bank: a bulk dump replaces it in "
    "memory, userwrite keeps it.",
    "A request (RQ1) for either area is answered at MIDI out. sysexdump "
    "sends the playing patch and usersend the user bank without being "
    "asked. The device number of incoming messages is not checked, answers "
    "carry the number of the request.",
]

DATA = [
    "The object contains no Roland data. d50~ loads its files from its own "
    "folder, or else from the folder of the patch that holds it. For "
    "d50.i~ that patch is d50.i~.pd itself, so put the files beside d50~.",
    "d50_pcm.bin: the PCM samples, written by tools/make_d50_data.py from "
    "your own ROM images. Without it the object stays silent and says so in "
    "the Pd window.",
    "Every .syx file beside it is read as a factory bank, a D-50 bulk dump "
    "of 64 patches, in alphabetical order, PND50-00.syx first, numbered "
    "through. Without one the object plays eight built-in presets, which "
    "cannot be edited.",
    "d50user-bank.syx beside d50~: a user bank in the same format, the "
    "starting point for d50.i~ (see Presets). Until it has been written it "
    "holds 64 init patches.",
    "A D-50 dump ends with the data of its reverb types 17 to 32. The "
    "object does not use them, its reverbs are built in, but it keeps the "
    "block of the dump it came with and puts it into every bank it writes "
    "or sends, so a D-50 gets a complete dump back.",
    "For d50.i~: d50~, d50.i~.pd and the data files in a folder on Pd's "
    "search path.",
]

CONTROLS_INTRO = [
    "In the order of the editor: one heading per row, the same title as "
    "the row there, page k on the controller is row k. Each line: name "
    "(the label in the editor), range, CC number and what it does.",
]

CREDITS = ("Credits: Modelled on PicoFaceD5 from PicoVintageSynthCollection by "
           "Michi71; Pure Data adaptation with help by Claude")

PARTIAL_DOC = [
    ("coarse", "pitch in semitones, 0 is C1"),
    ("fine", "fine tune"),
    ("keyfollow", "pitch keyfollow step, 11 is a semitone per key"),
    ("lfomode", "pitch LFO: off, +, -, aftertouch and lever"),
    ("penvmode", "pitch envelope: off, +, -"),
    ("bendmode", "bender: off, keyfollow, normal"),
    ("wave", "synth waveform: square, sawtooth"),
    ("pcm", "PCM sample"),
    ("pw", "pulse width"),
    ("pwvelo", "pulse width by velocity"),
    ("pwlfo", "pulse width LFO: +1, -1, +2, -2, +3, -3"),
    ("pwlfodepth", "pulse width LFO depth"),
    ("pwat", "pulse width by aftertouch"),
    ("cutoff", "filter cutoff"),
    ("reso", "filter resonance"),
    ("tvfkf", "cutoff keyfollow step"),
    ("tvfbiaspoint", "cutoff bias point and direction"),
    ("tvfbiaslevel", "cutoff bias level"),
    ("tvfenv", "filter envelope depth"),
    ("tvfvelo", "filter envelope depth by velocity"),
    ("tvfdepthkf", "filter envelope depth keyfollow"),
    ("tvftimekf", "filter envelope time keyfollow"),
    ("tvft1 tvft2 tvft3 tvft4 tvft5", "filter envelope times"),
    ("tvfl1 tvfl2 tvfl3", "filter envelope levels"),
    ("tvfsustain", "filter envelope sustain level"),
    ("tvfend", "filter envelope end level: 0, 100"),
    ("tvflfo", "filter LFO, as pwlfo"),
    ("tvflfodepth", "filter LFO depth"),
    ("tvfat", "cutoff by aftertouch"),
    ("level", "level of the partial"),
    ("velo", "level by velocity"),
    ("tvabiaspoint", "level bias point and direction"),
    ("tvabiaslevel", "level bias level"),
    ("tvat1 tvat2 tvat3 tvat4 tvat5", "level envelope times"),
    ("tval1 tval2 tval3", "level envelope levels"),
    ("tvasustain", "level envelope sustain level"),
    ("tvaend", "level envelope end level: 0, 100"),
    ("tvavelofollow", "level envelope time 1 by velocity"),
    ("tvatimekf", "level envelope time keyfollow"),
    ("tvalfo", "level LFO, as pwlfo"),
    ("tvalfodepth", "level LFO depth"),
    ("tvaat", "level by aftertouch"),
]

TONE_DOC = [
    ("structure", "S+S, S+S ring, P+S, P+S ring, S+P ring, P+P, P+P ring"),
    ("penvvelo", "pitch envelope by velocity"),
    ("penvtimekf", "pitch envelope time keyfollow"),
    ("penvt1 penvt2 penvt3 penvt4", "pitch envelope times"),
    ("penvl0 penvl1 penvl2", "pitch envelope levels"),
    ("penvsustain", "pitch envelope sustain level"),
    ("penvend", "pitch envelope end level"),
    ("pmodlfo", "pitch LFO depth"),
    ("pmodlever", "pitch LFO depth by the lever"),
    ("pmodat", "pitch LFO depth by aftertouch"),
    ("lfo1wave", "triangle, sawtooth, square, random"),
    ("lfo1rate", "LFO 1 rate"),
    ("lfo1delay", "LFO 1 delay"),
    ("lfo1sync", "off, on, key"),
    ("lfo2wave", "as lfo1wave"),
    ("lfo2rate", "LFO 2 rate"),
    ("lfo2delay", "LFO 2 delay"),
    ("lfo2sync", "off, on"),
    ("lfo3wave", "as lfo1wave"),
    ("lfo3rate", "LFO 3 rate"),
    ("lfo3delay", "LFO 3 delay"),
    ("lfo3sync", "off, on"),
    ("eqlowfreq", "low shelf frequency step"),
    ("eqlowgain", "low shelf gain in dB"),
    ("eqhighfreq", "high band frequency step"),
    ("eqhighq", "high band width step"),
    ("eqhighgain", "high band gain in dB"),
    ("chorustype", "chorus type"),
    ("chorusrate", "chorus rate"),
    ("chorusdepth", "chorus depth"),
    ("tchorus", "chorus balance of this tone"),
    ("mute", "partials sounding: none, first, second, both"),
    ("pbal", "balance of the two partials"),
]

PATCH_DOC = [
    ("keymode", "whole, dual, split, whole solo, dual solo, split "
                "upper solo, split lower solo, separate, separate solo"),
    ("split", "split point, 0 is C2"),
    ("portamode", "portamento on upper, lower, both"),
    ("ukeyshift lkeyshift", "key shift of each tone in semitones"),
    ("ufine lfine", "fine tune of each tone"),
    ("bendrange", "bender range in semitones"),
    ("atbend", "pitch bend by aftertouch in semitones"),
    ("portatime", "portamento time"),
    ("outmode", "how the tones go to reverb and outputs"),
    ("reverbtype", "reverb type"),
    ("reverb", "reverb balance"),
    ("pvol", "level of the patch, volume is the master"),
    ("balance", "balance of the two tones"),
    ("porta", "portamento switch"),
]


TEXT = dict(PERF_HELP)
for _doc in (PARTIAL_DOC, TONE_DOC, PATCH_DOC):
    for _names, _t in _doc:
        for _n in _names.split():
            TEXT[_n] = _t
assert all(n in TEXT for n in E.ALL), [n for n in E.ALL if n not in TEXT]


def control_line(nm):
    lo, hi = rng(nm)
    cc = f", CC {CC[nm]}" if nm in CC else ""
    return f"{nm} ({E.label(nm)}) {lo}..{hi}{cc}: {TEXT[nm]}"


def main():
    rows = [(f"{title}  {E.cc_text(names)}".replace(":", ""), names) for title, names in E.ROWS]
    H.write([os.path.normpath(os.path.join(HERE, "..", "pd", f)) for f in FILES],
            title=TITLE, subtitle=SUBTITLE, obj=OBJ, credits=CREDITS,
            sections=[("text", OBJ, OBJECT), ("text", "Editor", EDITOR),
                      ("text", "Launchkey", helpkit.launchkey("d50~", "d50cc", "d50pads", "")),
                      ("lines", "Inlet", INLETS), ("lines", "Outlets", OUTLETS),
                      ("text", "Presets and user bank", PRESETS),
                      ("lines", "Messages", MESSAGES), ("text", "The instrument", ABOUT),
                      ("text", "SysEx", SYSEX), ("text", "Data files", DATA),
                      ("controls", CONTROLS_INTRO, rows, control_line)])


if __name__ == "__main__":
    main()
