#!/usr/bin/env python3
"""The controls of juno~ in one table: the header pd/juno_params.h, the controller layer
[junocc], the editor [junoedit] and the help patch are all generated from it.

A patch is the 18 bytes of a Juno-106 patch dump: 16 sliders 0..127 and two switch bytes.
  slider   byte 0..15, in the order of the Juno-106
  switch   a field of byte 16 or 17, decoded in juno_core.cpp
Settings belong to the instrument and are not stored in a patch.

usage: python3 tools/juno_params.py
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# name, byte, label, help
SLIDERS = [
    ("lforate", 0, "LFO rate", "LFO rate, 0.3 to 20 Hz"),
    ("lfodelay", 1, "LFO delay", "time until the LFO sets in after the first key of a phrase"),
    ("dcolfo", 2, "DCO LFO", "LFO to pitch (vibrato)"),
    ("pwm", 3, "PWM", "pulse width: the depth of the LFO, or the width itself with pwmmode MAN"),
    ("noise", 4, "Noise", "noise level"),
    ("cutoff", 5, "VCF freq", "cutoff of the low-pass filter"),
    ("res", 6, "VCF res", "resonance, up to self-oscillation"),
    ("env", 7, "VCF env", "envelope to cutoff"),
    ("vcflfo", 8, "VCF LFO", "LFO to cutoff"),
    ("kybd", 9, "VCF kybd", "keyboard follow of the cutoff"),
    ("level", 10, "VCA level", "level of the patch, in front of the chorus"),
    ("attack", 11, "Attack", "envelope attack"),
    ("decay", 12, "Decay", "envelope decay"),
    ("sustain", 13, "Sustain", "envelope sustain level"),
    ("release", 14, "Release", "envelope release"),
    ("sub", 15, "Sub level", "level of the sub oscillator, one octave down"),
]
# name, hi, value names, label, help
SWITCHES = [
    ("range", 2, "16'|8'|4'", "DCO range", "octave of the oscillator"),
    ("pulse", 1, "off|on", "Pulse", "pulse wave"),
    ("saw", 1, "off|on", "Sawtooth", "sawtooth wave"),
    ("chorus", 3, "off|I|II|I+II", "Chorus",
     "chorus. I+II is the Juno-60's third mode, a Juno-106 plays it as I"),
    ("pwmmode", 1, "LFO|MAN", "PWM mode", "pulse width from the LFO or fixed"),
    ("envpol", 1, "pos|neg", "VCF env pol", "polarity of the envelope on the cutoff"),
    ("vcamode", 1, "ENV|GATE", "VCA mode", "the amplifier follows the envelope or the gate"),
    ("hpf", 3, "0|1|2|3", "HPF", "high-pass filter, four positions"),
]
# name, lo, hi, default, label, value names, help
SETTINGS = [
    ("volume", 0, 127, 127, "Volume", "", "master volume, after the chorus"),
    ("tune", -50, 50, 0, "Tune", "", "master tune in cents"),
    ("bend", 0, 7, 2, "Bend range", "", "bender range in semitones"),
    ("octave", -2, 2, 0, "Octave", "", "octave transposition of the keyboard"),
    ("vellevel", 0, 127, 0, "Velo level", "",
     "velocity to level. 0 is the instrument, which has no velocity"),
    ("velcutoff", 0, 127, 0, "Velo cutoff", "", "velocity to cutoff, a soft key is darker"),
    ("model", 0, 1, 0, "Model", "60|106",
     "which Juno the patch plays on: the Juno-60, or the Juno-106 as measured on Cherry Audio's "
     "DCO-106 (cutoff and resonance, contour times and depth, LFO, high-pass, chorus, sub and "
     "noise levels, pulse width, VCA level)"),
    ("calibrate", 0, 127, 0, "Calibrate", "",
     "how far the six voices drift apart, as on an instrument out of calibration: each voice "
     "its own tuning, cutoff, resonance and level. 0 is six identical voices"),
    ("chorusnoise", 0, 127, 0, "Chorus hiss", "",
     "hiss of the chorus lines, heard while the chorus is on. 0 is none"),
]

# Controller pages (row k = CC page k of tools/ctlayer.py).
CC_ROWS = [
    # page 1 is the same in every instrument: bright, resonance, attack, release, vibrato,
    # effect, own (here the VCF envelope amount), volume (docs/instrument-contract.md)
    ("PERFORM", ["cutoff", "res", "attack", "release", "dcolfo", "chorus", "env", "volume"]),
    ("DCO", ["range", "pulse", "saw", "sub", "noise", "pwm", "pwmmode", "hpf"]),
    ("ENV / LFO / VCF / VCA", ["decay", "sustain", "lforate", "lfodelay", "vcflfo", "kybd",
                               "envpol", "level"]),
    ("VCA", ["vcamode"]),
    ("SETUP", ["tune", "bend", "octave", "vellevel", "velcutoff", "model"]),
    ("VOICES", ["calibrate", "chorusnoise"]),
]
EXTRA_ROWS = []

# Control pads ([junopads], tools/ctpads.py): Launchkey pads in Drum mode, notes on the Drums
# Channel, top row 36-43 (pads 1-8), bottom row 44-51 (pads 9-16). Each column one job: up on
# top, down / the counterpart below.
# Control pads: 36-38 and 44-46 are the same in every instrument (common/tools/ctpads.py);
# these are juno's own.
OWN_PADS = {39: "cycle chorus", 47: "cycle model"}
OWN_PAD_TEXT = {39: "chorus I > II > I+II > off", 47: "Juno-106 / 60"}
ALL_ROWS = CC_ROWS + EXTRA_ROWS

# name -> (kind, index, lo, hi, label, names, help); kind P: slider byte, W: switch number,
# S: setting number
CONTROLS = {}
DEFAULTS = {}
for nm, byte, lbl, hlp in SLIDERS:
    CONTROLS[nm] = ("P", byte, 0, 127, lbl, "", hlp)
for i, (nm, hi, names, lbl, hlp) in enumerate(SWITCHES):
    CONTROLS[nm] = ("W", i, 0, hi, lbl, names, hlp)
for i, (nm, lo, hi, dflt, lbl, names, hlp) in enumerate(SETTINGS):
    CONTROLS[nm] = ("S", i, lo, hi, lbl, names, hlp)
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


def write_header(path):
    L = ["// Generated by tools/juno_params.py -- do not edit.",
         "// name, kind (P slider byte, W switch, S setting), index, lowest and highest value,",
         "// label, value names",
         "static const Control kControls[] = {"]
    for nm in ORDER:
        kind, i, lo, hi, lbl, names, _ = CONTROLS[nm]
        L.append(f'    {{"{nm}", \'{kind}\', {i}, {lo}, {hi}, "{lbl}", "{names}"}},')
    L.append("};")
    L.append("")
    L.append("enum Switch { " + ", ".join(f"kSw_{s[0]}" for s in SWITCHES) + ", kSwitchCount };")
    L.append("enum Setting { " + ", ".join(f"kSet_{s[0]}" for s in SETTINGS) + ", kSettingCount };")
    L.append("static const int kSettingDefaults[] = { " + ", ".join(str(s[3]) for s in SETTINGS) + " };")
    open(path, "w").write("\n".join(L) + "\n")


if __name__ == "__main__":
    out = os.path.join(ROOT, "pd", "juno_params.h")
    write_header(out)
    print("wrote", out, f"({len(CONTROLS)} controls, {sum(len(n) for _, n in CC_ROWS)} on CC pages)")
