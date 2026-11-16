#!/usr/bin/env python3
"""The controls of cz4~ in one table: the header cz4/cz4_params.h, the controller layer
[cz4cc], the editor [cz4edit] and the help patch are all generated from it.

A tone is the 144 bytes of a Casio CZ-1 tone. Its values are panel values as the CZ shows
them (core/cz_tone.h converts them to bytes). Kinds:
  T  a value of the tone                         (index: CZP_* of cz_tone.h)
  L  a value of the selected line                (CZP_*)
  E  a value of the selected envelope of that line (CZP_*)
  S  a selection: which line, envelope, tone (A/B) the edit controls work on
  O  operation memory, as on the CZ-1: key mode, split, tone B, chorus, portamento, glide,
     bend, wheel, aftertouch -- stored in an operation slot, not in the tone
  D  easy envelope (as Arturia's DADSR): four values that write the selected envelope
  P  performance: acts at once, also on sounding notes, never stored
  G  setting of the instrument

usage: python3 tools/cz4_params.py
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

WAVES = "saw|square|pulse|null|dsine|sawpuls|reso|pulse2"
WINDOWS = "off|saw|tri|trap|pulse|dsaw|dsaw6|dsaw7"

# name, kind, index, lo, hi, default, label, value names, help
TABLE = [
    # ---- PERFORM
    ("dcw", "P", 0, -64, 63, 0, "DCW offset", "", "brighter or darker: added to the DCW of both lines"),
    ("attack", "P", 1, -64, 63, 0, "Attack", "",
     "faster (-) or slower (+) steps up to the sustain point of every envelope"),
    ("release", "P", 2, -64, 63, 0, "Release", "", "faster (-) or slower (+) steps after the sustain point"),
    ("balance", "P", 3, -64, 63, 0, "Balance", "", "line 1 (-) against line 2 (+)"),
    ("detune", "P", 4, -64, 63, 0, "Detune L2", "", "line 2 against line 1, in cents, on top of the tone"),
    ("vibrato", "P", 5, 0, 99, 0, "Vibrato", "", "extra vibrato depth, as the mod wheel"),
    ("chorus", "P", 6, 0, 127, 64, "Chorus mix", "", "level of the chorus for tones with chorus on"),
    ("volume", "P", 7, 0, 127, 100, "Volume", "", "master volume"),
    # ---- LINE
    ("line", "S", 0, 0, 1, 0, "Edit line", "1|2", "the line that the line and envelope controls edit"),
    ("linesel", "T", "CZP_LINESEL", 0, 3, 0, "Line select", "1|2|1+1'|1+2'",
     "line 1, line 2, line 1 twice (line 2 detuned), both lines"),
    ("wave", "L", "CZP_WAVE", 0, 7, 0, "Wave", WAVES, "first wave of the line"),
    ("wave2", "L", "CZP_WAVE2", 0, 8, 0, "Wave 2", "off|" + WAVES,
     "second wave: the line alternates the two every cycle"),
    ("window", "L", "CZP_WINDOW", 0, 7, 0, "Window", WINDOWS,
     "window of the resonance waves (CZ waves 6, 7, 8 = saw, tri, trap)"),
    ("mod", "T", "CZP_MOD", 0, 2, 0, "Modulation", "off|ring|noise",
     "line 2 ring-modulated by line 1, or its pitch by noise (1+1' and 1+2')"),
    ("octave", "T", "CZP_OCTAVE", -1, 1, 0, "Octave", "-1|0|+1", "octave of the tone"),
    ("level", "L", "CZP_LEVEL", 1, 15, 15, "Line level", "", "level of the line (CZ-1)"),
    # ---- DETUNE / VIBRATO
    ("dsign", "T", "CZP_DSIGN", 0, 1, 0, "Det. sign", "+|-", "detune of line 2 up or down"),
    ("doct", "T", "CZP_DOCT", 0, 3, 0, "Det. octave", "", "detune of line 2 in octaves"),
    ("dnote", "T", "CZP_DNOTE", 0, 11, 0, "Det. note", "", "detune of line 2 in semitones"),
    ("dfine", "T", "CZP_DFINE", 0, 60, 0, "Det. fine", "", "fine detune of line 2, 60 = one semitone"),
    ("vwave", "T", "CZP_VWAVE", 0, 3, 0, "Vib. wave", "tri|saw up|saw dn|square", "vibrato wave"),
    ("vdelay", "T", "CZP_VDELAY", 0, 99, 0, "Vib. delay", "", "time until the vibrato sets in"),
    ("vrate", "T", "CZP_VRATE", 0, 99, 0, "Vib. rate", "", "vibrato speed"),
    ("vdepth", "T", "CZP_VDEPTH", 0, 99, 0, "Vib. depth", "", "vibrato depth"),
    # ---- ENVELOPE
    ("env", "S", 1, 0, 2, 2, "Edit env", "DCO|DCW|DCA",
     "the envelope that the rate, level, sus and end controls edit: pitch, wave, amplitude"),
    ("sus", "E", "CZP_SUS", 0, 8, 0, "Sustain", "off|1|2|3|4|5|6|7|8",
     "step that holds while the key is down, the steps after it are the release"),
    ("end", "E", "CZP_END", 1, 8, 1, "End", "", "last step: it goes to zero"),
    ("kfw", "L", "CZP_KFW", 0, 9, 0, "Key DCW", "", "key follow of the DCW: high notes get less"),
    ("kfa", "L", "CZP_KFA", 0, 9, 0, "Key DCA", "", "key follow of the DCA: high notes get faster"),
    ("velp", "L", "CZP_VELP", 0, 15, 0, "Velo pitch", "", "velocity to the DCO envelope (CZ-1)"),
    ("velw", "L", "CZP_VELW", 0, 15, 0, "Velo DCW", "", "velocity to the DCW envelope (CZ-1)"),
    ("vela", "L", "CZP_VELA", 0, 15, 0, "Velo DCA", "", "velocity to the amplitude (CZ-1)"),
    # ---- RATES, LEVELS
] + [(f"r{j + 1}", "E", f"CZP_RATE1 + {j}", 0, 99, 0, f"Rate {j + 1}", "",
      f"rate of step {j + 1}: 99 fastest") for j in range(8)] + [
    (f"l{j + 1}", "E", f"CZP_LEVEL1 + {j}", 0, 99, 0, f"Level {j + 1}", "",
     f"level of step {j + 1}") for j in range(8)] + [
    # ---- EASY ENVELOPE, TOUCH
    ("eattack", "D", 0, 0, 99, 0, "Easy A", "",
     "easy envelope: attack time. Writes steps 1-3 of the selected envelope"),
    ("edecay", "D", 1, 0, 99, 50, "Easy D", "", "easy envelope: decay time"),
    ("esustain", "D", 2, 0, 99, 99, "Easy S", "", "easy envelope: sustain level"),
    ("erelease", "D", 3, 0, 99, 30, "Easy R", "", "easy envelope: release time"),
    ("edit", "S", 2, 0, 1, 0, "Edit tone", "A|B",
     "the tone that the edit controls work on: A, or B of a split or mix"),
    ("wheel", "O", 0, 0, 99, 50, "Wheel vib.", "", "vibrato depth of the mod wheel"),
    ("atvib", "O", 1, 0, 99, 0, "AT vibrato", "", "aftertouch to vibrato depth"),
    ("atamp", "O", 2, 0, 15, 0, "AT amp", "", "aftertouch to amplitude"),
    # ---- OPERATION
    ("keymode", "O", 3, 0, 2, 0, "Key mode", "normal|split|mix",
     "tone A alone, A above and B below the split point, or A and B on every key"),
    ("split", "O", 4, 0, 127, 60, "Split", "", "split point: the lowest key of tone A"),
    ("bsrc", "O", 5, 0, 1, 0, "B bank", "preset|user", "bank of tone B"),
    ("bnum", "O", 6, 0, 127, 1, "B number", "", "tone B: its number in that bank"),
    ("avol", "O", 7, 0, 99, 99, "A volume", "", "volume of tone A"),
    ("bvol", "O", 8, 0, 99, 99, "B volume", "", "volume of tone B"),
    ("boct", "O", 9, -2, 2, 0, "B octave", "", "octave of tone B"),
    ("bdet", "O", 10, -64, 63, 0, "B detune", "", "detune of tone B in cents (mix)"),
    # ---- OPERATION 2
    ("achorus", "O", 11, 0, 1, 0, "A chorus", "off|on", "chorus for tone A"),
    ("bchorus", "O", 12, 0, 1, 0, "B chorus", "off|on", "chorus for tone B"),
    ("porta", "O", 13, 0, 1, 0, "Portamento", "off|on", "notes slide from the last one"),
    ("ptime", "O", 14, 0, 99, 30, "Porta time", "", "portamento time"),
    ("glide", "O", 15, 0, 1, 0, "Glide", "off|on", "every note starts at gnote and slides in"),
    ("gnote", "O", 16, -24, 24, -12, "Glide note", "", "start of the glide, in semitones from the key"),
    ("gtime", "O", 17, 0, 99, 30, "Glide time", "", "glide time"),
    ("solo", "O", 18, 0, 1, 0, "Solo", "poly|solo", "one voice, the last key. Legato keeps the envelopes"),
    # ---- SETUP
    ("bend", "O", 19, 0, 12, 2, "Bend range", "", "pitch bend range in semitones"),
    ("voices", "G", 0, 1, 16, 8, "Voices", "",
     "notes of a one-line tone (the CZ: 8), a two-line tone plays half as many"),
    ("tune", "G", 1, -64, 63, 0, "Tune", "", "master tune in cents"),
    ("transpose", "G", 2, -24, 24, 0, "Transpose", "", "transposition in semitones"),
    ("chip", "G", 3, 0, 1, 1, "Waves", "clean|chip",
     "chip: the waves as the CZ's chip computes them (12-bit), clean: ideal"),
]

CC_ROWS = [
    ("PERFORM", ["dcw", "attack", "release", "balance", "detune", "vibrato", "chorus", "volume"]),
    ("LINE / WAVE", ["line", "linesel", "wave", "wave2", "window", "mod", "octave", "level"]),
    ("DETUNE / VIBRATO", ["dsign", "doct", "dnote", "dfine", "vwave", "vdelay", "vrate", "vdepth"]),
    ("ENVELOPE", ["env", "sus", "end", "kfw", "kfa", "velp", "velw", "vela"]),
    ("RATES", [f"r{j + 1}" for j in range(8)]),
    ("LEVELS", [f"l{j + 1}" for j in range(8)]),
    ("EASY ENV / TOUCH", ["eattack", "edecay", "esustain", "erelease", "edit", "wheel", "atvib",
                          "atamp"]),
    ("OPERATION", ["keymode", "split", "bsrc", "bnum", "avol", "bvol", "boct", "bdet"]),
    ("OPERATION 2", ["achorus", "bchorus", "porta", "ptime", "glide", "gnote", "gtime", "solo"]),
    ("SETUP", ["bend", "voices", "tune", "transpose", "chip"]),
]
EXTRA_ROWS = []
ALL_ROWS = CC_ROWS + EXTRA_ROWS

# name -> (kind, index, lo, hi, label, names, help), as juno_params.py
CONTROLS = {}
DEFAULTS = {}
for nm, kind, idx, lo, hi, dflt, lbl, names, hlp in TABLE:
    CONTROLS[nm] = (kind, idx, lo, hi, lbl, names, hlp)
    DEFAULTS[nm] = dflt

_listed = [n for _, ns in ALL_ROWS for n in ns]
assert len(_listed) == len(set(_listed)) and set(_listed) == set(CONTROLS), \
    set(CONTROLS) ^ set(_listed)
ORDER = _listed


def value_names(names, lo, hi):
    if not names:
        return None
    v = names.split("|")
    assert len(v) == hi - lo + 1, (names, lo, hi)
    return v


for _nm, _c in CONTROLS.items():
    value_names(_c[5], _c[2], _c[3])
    assert len(_c[4]) <= 12, _c[4]
    assert _c[2] <= DEFAULTS[_nm] <= _c[3], _nm


def by_kind(kind):
    return sorted((c[1], n) for n, c in CONTROLS.items() if c[0] == kind)


def write_header(path):
    L = ["// Generated by tools/cz4_params.py -- do not edit.",
         "// name, kind (T tone, L line, E envelope, S selection, O operation memory,",
         "// D easy envelope, P performance, G setting), index, lowest and highest value,",
         "// default, label, value names",
         "static const Control kControls[] = {"]
    for nm in ORDER:
        kind, i, lo, hi, lbl, names, _ = CONTROLS[nm]
        L.append(f'    {{"{nm}", \'{kind}\', {i}, {lo}, {hi}, {DEFAULTS[nm]}, "{lbl}", "{names}"}},')
    L.append("};")
    L.append("")
    for kind, enum in (("O", "Op"), ("P", "Perf"), ("G", "Set"), ("S", "Sel"), ("D", "Easy")):
        items = by_kind(kind)
        assert [i for i, _ in items] == list(range(len(items))), kind
        L.append(f"enum {enum} {{ " + ", ".join(f"k{enum}_{n}" for _, n in items) +
                 f", k{enum}Count }};")
        L.append(f"static const int k{enum}Defaults[] = {{ " +
                 ", ".join(str(DEFAULTS[n]) for _, n in items) + " };")
    open(path, "w").write("\n".join(L) + "\n")


if __name__ == "__main__":
    out = os.path.join(ROOT, "cz4", "cz4_params.h")
    write_header(out)
    print("wrote", out, f"({len(CONTROLS)} controls, {sum(len(n) for _, n in CC_ROWS)} on CC pages)")
