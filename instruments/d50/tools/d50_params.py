#!/usr/bin/env python3
"""The D-50 parameters: one table for the external, the controller layer, the editor and
the help patch.

A patch is 448 bytes: seven blocks of 64 (upper partial 1, upper partial 2, upper tone,
lower partial 1, lower partial 2, lower tone, patch), the layout of the D-50's own dump and
of its temporary area at SysEx address 00-00-00. Only the bytes the engine reads have names;
every byte is reachable with `param <address> <value>`.

Section tables: name, offset inside the block, largest stored value, shift (panel value =
stored value - shift), label, value names ("a|b|c" for the panel values in order; "" = number;
"@note:<n>" = a note name, n the MIDI note of value 0; "@pcm" = the sample's name).

Controls: the flat names of `n <name> <0-127>` / `<name> <value>`. A partial name acts on
the selected partial (control `part`), a tone name on that partial's tone, a patch name on
the patch. The macros of the first row are not patch bytes.

usage: python3 tools/d50_params.py   (writes pd/d50_params.h)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

LFOSEL = "+1|-1|+2|-2|+3|-3"
KF = "-1|-1/2|-1/4|0|1/8|1/4|3/8|1/2|5/8|3/4|7/8|1|5/4|3/2|2|s1|s2"

PARTIAL = [
    ("coarse", 0, 72, 0, "Pitch coarse", "@note:24"),
    ("fine", 1, 100, 50, "Pitch fine", ""),
    ("keyfollow", 2, 16, 0, "Pitch KF", KF),
    ("lfomode", 3, 3, 0, "LFO mode", "off|+|-|A&L"),
    ("penvmode", 4, 2, 0, "P-ENV mode", "off|+|-"),
    ("bendmode", 5, 2, 0, "Bend mode", "off|KF|normal"),
    ("wave", 6, 1, 0, "Waveform", "square|saw"),
    ("pcm", 7, 99, -1, "PCM wave", "@pcm"),
    ("pw", 8, 100, 0, "Pulse width", ""),
    ("pwvelo", 9, 14, 7, "PW velocity", ""),
    ("pwlfo", 10, 5, 0, "PW LFO sel", LFOSEL),
    ("pwlfodepth", 11, 100, 0, "PW LFO", ""),
    ("pwat", 12, 14, 7, "PW aftert.", ""),
    ("cutoff", 13, 100, 0, "Cutoff", ""),
    ("reso", 14, 30, 0, "Resonance", ""),
    ("tvfkf", 15, 14, 0, "Cutoff KF", ""),
    ("tvfbiaspoint", 16, 127, 0, "TVF bias pt", ""),
    ("tvfbiaslevel", 17, 14, 7, "TVF bias lv", ""),
    ("tvfenv", 18, 100, 0, "TVF env", ""),
    ("tvfvelo", 19, 100, 0, "TVF velo", ""),
    ("tvfdepthkf", 20, 4, 0, "Depth KF", ""),
    ("tvftimekf", 21, 4, 0, "Time KF", ""),
    ("tvft1", 22, 100, 0, "TVF T1", ""),
    ("tvft2", 23, 100, 0, "TVF T2", ""),
    ("tvft3", 24, 100, 0, "TVF T3", ""),
    ("tvft4", 25, 100, 0, "TVF T4", ""),
    ("tvft5", 26, 100, 0, "TVF T5", ""),
    ("tvfl1", 27, 100, 0, "TVF L1", ""),
    ("tvfl2", 28, 100, 0, "TVF L2", ""),
    ("tvfl3", 29, 100, 0, "TVF L3", ""),
    ("tvfsustain", 30, 100, 0, "TVF sustain", ""),
    ("tvfend", 31, 1, 0, "TVF end", "0|100"),
    ("tvflfo", 32, 5, 0, "TVF LFO sel", LFOSEL),
    ("tvflfodepth", 33, 100, 0, "TVF LFO", ""),
    ("tvfat", 34, 14, 7, "TVF aftert.", ""),
    ("level", 35, 100, 0, "Level", ""),
    ("velo", 36, 100, 50, "Velocity", ""),
    ("tvabiaspoint", 37, 127, 0, "TVA bias pt", ""),
    ("tvabiaslevel", 38, 12, 12, "TVA bias lv", ""),
    ("tvat1", 39, 100, 0, "TVA T1", ""),
    ("tvat2", 40, 100, 0, "TVA T2", ""),
    ("tvat3", 41, 100, 0, "TVA T3", ""),
    ("tvat4", 42, 100, 0, "TVA T4", ""),
    ("tvat5", 43, 100, 0, "TVA T5", ""),
    ("tval1", 44, 100, 0, "TVA L1", ""),
    ("tval2", 45, 100, 0, "TVA L2", ""),
    ("tval3", 46, 100, 0, "TVA L3", ""),
    ("tvasustain", 47, 100, 0, "TVA sustain", ""),
    ("tvaend", 48, 1, 0, "TVA end", "0|100"),
    ("tvavelofollow", 49, 4, 0, "T1 velocity", ""),
    ("tvatimekf", 50, 4, 0, "Time KF", ""),
    ("tvalfo", 51, 5, 0, "TVA LFO sel", LFOSEL),
    ("tvalfodepth", 52, 100, 0, "TVA LFO", ""),
    ("tvaat", 53, 14, 7, "TVA aftert.", ""),
]

LFOWAVE = "tri|saw|squ|rnd"
TONE = [
    ("structure", 10, 6, -1, "Structure", "S+S|S+S-R|P+S|P+S-R|S+P-R|P+P|P+P-R"),
    ("penvvelo", 11, 2, 0, "P-ENV velo", ""),
    ("penvtimekf", 12, 4, 0, "P-ENV KF", ""),
    ("penvt1", 13, 50, 0, "P-ENV T1", ""),
    ("penvt2", 14, 50, 0, "P-ENV T2", ""),
    ("penvt3", 15, 50, 0, "P-ENV T3", ""),
    ("penvt4", 16, 50, 0, "P-ENV T4", ""),
    ("penvl0", 17, 100, 50, "P-ENV L0", ""),
    ("penvl1", 18, 100, 50, "P-ENV L1", ""),
    ("penvl2", 19, 100, 50, "P-ENV L2", ""),
    ("penvsustain", 20, 100, 50, "P-ENV sus", ""),
    ("penvend", 21, 100, 50, "P-ENV end", ""),
    ("pmodlfo", 22, 100, 0, "Vibrato", ""),
    ("pmodlever", 23, 100, 0, "Vib lever", ""),
    ("pmodat", 24, 100, 0, "Vib aftert.", ""),
    ("lfo1wave", 25, 3, 0, "LFO1 wave", LFOWAVE),
    ("lfo1rate", 26, 100, 0, "LFO1 rate", ""),
    ("lfo1delay", 27, 100, 0, "LFO1 delay", ""),
    ("lfo1sync", 28, 2, 0, "LFO1 sync", "off|on|key"),
    ("lfo2wave", 29, 3, 0, "LFO2 wave", LFOWAVE),
    ("lfo2rate", 30, 100, 0, "LFO2 rate", ""),
    ("lfo2delay", 31, 100, 0, "LFO2 delay", ""),
    ("lfo2sync", 32, 1, 0, "LFO2 sync", "off|on"),
    ("lfo3wave", 33, 3, 0, "LFO3 wave", LFOWAVE),
    ("lfo3rate", 34, 100, 0, "LFO3 rate", ""),
    ("lfo3delay", 35, 100, 0, "LFO3 delay", ""),
    ("lfo3sync", 36, 1, 0, "LFO3 sync", "off|on"),
    ("eqlowfreq", 37, 15, 0, "EQ low f", ""),
    ("eqlowgain", 38, 24, 12, "EQ low dB", ""),
    ("eqhighfreq", 39, 21, 0, "EQ high f", ""),
    ("eqhighq", 40, 8, 0, "EQ high Q", ""),
    ("eqhighgain", 41, 24, 12, "EQ high dB", ""),
    ("chorustype", 42, 7, -1, "Chorus type",
     "Chorus1|Chorus2|Flanger1|Flanger2|FBChorus|Tremolo|ChoTrem|Dimens."),
    ("chorusrate", 43, 100, 0, "Chorus rate", ""),
    ("chorusdepth", 44, 100, 0, "Chorus depth", ""),
    ("tchorus", 45, 100, 0, "Tone chorus", ""),
    ("mute", 46, 3, 0, "Partials on", "none|1|2|1+2"),
    ("pbal", 47, 100, 0, "Partial bal", ""),
]

PATCH = [
    ("keymode", 18, 8, 0, "Key mode",
     "Whole|Dual|Split|Whole-S|Dual-S|Split-US|Split-LS|Separate|Sep-S"),
    ("split", 19, 60, 0, "Split point", "@note:36"),
    ("portamode", 20, 2, 0, "Porta mode", "U|L|UL"),
    ("ukeyshift", 22, 48, 24, "U key shift", ""),
    ("lkeyshift", 23, 48, 24, "L key shift", ""),
    ("ufine", 24, 100, 50, "U fine tune", ""),
    ("lfine", 25, 100, 50, "L fine tune", ""),
    ("bendrange", 26, 12, 0, "Bend range", ""),
    ("atbend", 27, 24, 12, "AT bend", ""),
    ("portatime", 28, 100, 0, "Porta time", ""),
    ("outmode", 29, 3, -1, "Output mode", ""),
    ("reverbtype", 30, 31, -1, "Reverb type", ""),
    ("reverb", 31, 100, 0, "Reverb", ""),
    ("pvol", 32, 100, 0, "Patch level", ""),
    ("balance", 33, 100, 0, "Tone balance", ""),
    ("porta", 41, 1, 0, "Portamento", "off|on"),
]

SECTIONS = [("kPartialParams", PARTIAL), ("kCommonParams", TONE), ("kPatchParams", PATCH)]

# Not patch bytes: name, lo, hi (panel values), label, value names.
MACROS = [
    ("cut", -63, 63, "Cutoff all", ""),
    ("res", -63, 63, "Reso all", ""),
    ("att", -63, 63, "Attack all", ""),
    ("rel", -63, 63, "Release all", ""),
    ("vib", -63, 63, "Vibrato all", ""),
    ("chorus", 0, 100, "Chorus", ""),
    ("volume", 0, 100, "Volume", ""),
    ("part", 0, 3, "Partial", "U1|U2|L1|L2"),
    ("tune", -50, 50, "Master tune", ""),
    ("voices", 1, 8, "Voices", ""),
]
MACRO_HELP = {
    "cut": "cutoff of every synth partial, also of notes that are sounding. 0 leaves the patch as it is",
    "res": "resonance of every synth partial, for the notes that follow",
    "att": "attack (TVA time 1) of all four partials, for the notes that follow",
    "rel": "release (TVA and TVF time 5) of all four partials, for the notes that follow",
    "vib": "vibrato (LFO pitch modulation) of both tones, for the notes that follow",
    "chorus": "chorus balance of both tones",
    "volume": "master volume, not stored in a patch",
    "part": "the partial the partial names act on, and with it the tone (U1 U2: upper, L1 L2: lower)",
    "tune": "master tune in cents, not stored in a patch",
    "voices": "polyphony per tone, not stored in a patch",
}

# Controller pages (row k = CC page k of tools/ctlayer.py): 10 pages of 8, page 11 of 4.
CC_ROWS = [
    # page 1 is the same in every instrument: bright, resonance, attack, release, vibrato,
    # effect, own (here the tone balance), volume (docs/instrument-contract.md)
    ("PERFORMANCE", ["cut", "res", "att", "rel", "vib", "chorus", "balance", "volume"]),
    ("PARTIAL: WG", ["part", "coarse", "fine", "keyfollow", "wave", "pcm", "pw", "pwlfodepth"]),
    ("PARTIAL: TVF", ["cutoff", "reso", "tvfkf", "tvfenv", "tvfvelo", "tvfsustain",
                      "tvflfodepth", "tvfat"]),
    ("PARTIAL: TVF ENV", ["tvft1", "tvft2", "tvft3", "tvft4", "tvft5", "tvfl1", "tvfl2", "tvfl3"]),
    ("PARTIAL: TVA", ["level", "velo", "tvasustain", "tvalfodepth", "tvaat", "tvabiaspoint",
                      "tvabiaslevel", "tvavelofollow"]),
    ("PARTIAL: TVA ENV", ["tvat1", "tvat2", "tvat3", "tvat4", "tvat5", "tval1", "tval2", "tval3"]),
    ("TONE", ["structure", "pbal", "mute", "pmodlfo", "pmodlever", "pmodat", "penvvelo",
              "penvtimekf"]),
    ("TONE: P-ENV", ["penvt1", "penvt2", "penvt3", "penvt4", "penvl0", "penvl1", "penvl2",
                     "penvsustain"]),
    ("TONE: LFO 1 / 2", ["lfo1wave", "lfo1rate", "lfo1delay", "lfo1sync", "lfo2wave", "lfo2rate",
                         "lfo2delay", "lfo2sync"]),
    ("TONE: LFO 3 / EQ", ["lfo3wave", "lfo3rate", "lfo3delay", "lfo3sync", "eqlowfreq",
                          "eqlowgain", "eqhighfreq", "eqhighgain"]),
    ("TONE: CHORUS / REVERB", ["chorustype", "chorusrate", "chorusdepth", "reverb"]),
]
# The rest: in the editor only (mouse).
EXTRA_ROWS = [
    ("PARTIAL: WG MODES", ["lfomode", "penvmode", "bendmode", "pwvelo", "pwlfo", "pwat",
                           "tvaend", "tvatimekf"]),
    ("PARTIAL: TVF / TVA MORE", ["tvfbiaspoint", "tvfbiaslevel", "tvfdepthkf", "tvftimekf",
                                 "tvflfo", "tvfend", "tvalfo", "eqhighq"]),
    ("PATCH", ["keymode", "split", "portamode", "porta", "portatime", "bendrange", "atbend",
               "outmode"]),
    ("PATCH / TONE MORE", ["ukeyshift", "lkeyshift", "ufine", "lfine", "reverbtype", "pvol",
                           "penvend", "tchorus"]),
    ("SETUP", ["tune", "voices"]),
]
ALL_ROWS = CC_ROWS + EXTRA_ROWS

# Flat control table: name -> (kind, index, lo, hi, label, names); kind P/T/G = index into
# the section table, M = macro number.
CONTROLS = {}
for kind, table in (("P", PARTIAL), ("T", TONE), ("G", PATCH)):
    for i, (nm, off, mx, sh, lbl, names) in enumerate(table):
        assert nm not in CONTROLS, nm
        CONTROLS[nm] = (kind, i, -sh, mx - sh, lbl, names)
for i, (nm, lo, hi, lbl, names) in enumerate(MACROS):
    assert nm not in CONTROLS, nm
    CONTROLS[nm] = ("M", i, lo, hi, lbl, names)

_listed = [n for _, ns in ALL_ROWS for n in ns]
assert len(_listed) == len(set(_listed)) and set(_listed) == set(CONTROLS), \
    set(CONTROLS) ^ set(_listed)
ORDER = _listed                      # the order of the editor


def value_names(names, lo, hi):
    if not names or names.startswith("@"):
        return None
    v = names.split("|")
    assert len(v) == hi - lo + 1, (names, lo, hi)
    return v


for _nm, (_k, _i, _lo, _hi, _lbl, _names) in CONTROLS.items():
    value_names(_names, _lo, _hi)
    assert len(_lbl) <= 12, _lbl


def write_header(path):
    L = ["// Generated by tools/d50_params.py -- do not edit.",
         "// name, offset, largest stored value, shift (panel = stored - shift), label, value names",
         ""]
    for cname, table in SECTIONS:
        L.append(f"static const Param {cname}[] = {{")
        for nm, off, mx, sh, lbl, names in table:
            L.append(f'    {{"{nm}", {off}, {mx}, {sh}, "{lbl}", "{names}"}},')
        L.append("};")
        L.append("")
    L.append("// name, kind (P partial, T tone, G patch: index into its table; M macro), index,")
    L.append("// lowest and highest panel value, label, value names")
    L.append("static const Control kControls[] = {")
    for nm in ORDER:
        kind, i, lo, hi, lbl, names = CONTROLS[nm]
        L.append(f'    {{"{nm}", \'{kind}\', {i}, {lo}, {hi}, "{lbl}", "{names}"}},')
    L.append("};")
    L.append("")
    L.append("enum Macro { " + ", ".join(f"kMacro_{m[0]}" for m in MACROS) + ", kMacroCount };")
    open(path, "w").write("\n".join(L) + "\n")


if __name__ == "__main__":
    out = os.path.join(ROOT, "pd", "d50_params.h")
    write_header(out)
    print("wrote", out, f"({len(CONTROLS)} controls, {sum(len(n) for _, n in CC_ROWS)} on CC pages)")
