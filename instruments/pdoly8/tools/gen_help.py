#!/usr/bin/env python3
"""Generate pdoly8~-help.pd in the layout of the skandrums-pd help patches.

usage: gen_help.py [outfile]   (default: pdoly8~-help.pd)

Same grid, colours and section order as skandrums-pd/tools/gen_help.py:
yellow writing surface, pink strip, blue panel, blue heading squares,
toggle + metro at the top, one [s]/[r] bus, text sections below.
"""
import sys

BS = chr(92)
SEMI, COMMA, DOLLAR, SPACE = BS + ";", BS + ",", BS + "$", BS + " "

# ---- grid (identical to skandrums-pd) -----------------------------------
M = 20
YEL_X, YEL_Y, YEL_W = 2 * M, M, 700
PINK_X, PINK_W = M, M
PINK_TOP = YEL_Y + 3 * M
BLUE_W = 200
BLUE_X = YEL_X + YEL_W + M - BLUE_W
BLUE_TOP = PINK_TOP + 3 * M
CX = YEL_X + M
LBL = 24
TX = CX + LBL - 2
LINE = 16
MAXCH = 62
PINK, YELLOW, BLUE = "#ffcfd6", "#fafaf2", "#1e3791"

OBJ, SHORT = "pdoly8~", "pdoly8"
SUBTITLE = "4-Voice Poly-800 tone generation - MSM5232 foot synthesis"

# controls: name, default, description (index = position)
CONTROLS = [
    ("har1", 45, "Timbre of DCO1, one knob through all 15 distinct original "
                 "timbres, dark to bright: 0 16' / 9-64 saw net "
                 "(16'2', 16'4'2', 16'4', 16'8'4', all at 45, 16'8'2', 16'8') / "
                 "73-127 square net (16'8', 16'8'4', 16'4', 16'4'2', all, "
                 "16'8'2', 16'2'). One foot or the net per step, no jumps. "
                 "Combinations without 16' = oct1 2-4."),
    ("har2", 45, "Timbre of DCO2, same path."),
    ("env1", 0, "Envelope of DCO1 (DEG1), one knob: 0 gate / 1 very short "
                "pluck (no fade from the gate) / 24 pluck / "
                "48 piano / 72 long release / 92 swell / 108 pad / "
                "127 long pad. -1 = own values a1 ... r1."),
    ("env2", 0, "Envelope of DCO2 (DEG2), the same, -1 = own values a2 ... r2."),
    ("intv", 0, "Interval of DCO2 above DCO1 in semitones, 0-12, as stored on the Poly-800 II."),
    ("detn", 0, "Detune of DCO2 downwards: the original's 4 steps "
                "0, 6.5, 13, 26 cents lie at 0, 42, 85, 127."),
    ("lev1", 127, "Level of DCO1."),
    ("lev2", 0, "Level of DCO2 (every voice has both DCOs). Above full scale "
                "(lev1 + lev2 > 127) both are scaled down together."),
]

# setting blocks: title, groups of number boxes (name, min, max) - each
# group starts a new row -, preset messages, paragraphs (one per parameter,
# each ends with a line break in Pd)
BLOCKS = [
    ("DCO settings",
     [[("har1", 0, 127), ("har2", 0, 127), ("hmg", 0, 127)],
      [("oct1", -1, 4), ("oct2", -1, 4)],
      [("pw1", 0, 127), ("pw2", 0, 127), ("pwm1", 0, 127), ("pwm2", 0, 127)],
      [("wsh1", 0, 127), ("wsh2", 0, 127), ("wmx1", 0, 127), ("wmx2", 0, 127)],
      [("wsm", 0, 127), ("wse", -127, 127)],
      [("sync", 0, 1), ("swp2", 0, 72), ("swe", 0, 72), ("swm", 0, 24)],
      [("ring", 0, 127), ("rmf", 0, 14)]], [],
     ["Timbre, octave and the modulations of the tone generators. Beyond har and oct "
      "nothing here is on the Poly-800. All edges stay band-limited. In this order: "
      "timbre, octave, PWM, Walsh loop, sync, ring.",
      "har1, har2 (45): timbre of DCO1 / DCO2, the same as the controls above (see Timbre).",
      "hmg (0): the MG moves har1 and har2 by +-hmg steps, 127 = through the whole "
      "timbre path (rate mgr / mgf, delay mgd).",
      "oct1, oct2 (0): -1 0 1 = LOW MID HIGH, 2-4: the feet one to three octaves "
      "higher (the combinations without 16').",
      "pw1, pw2 (64): pulse width 0-127 = 3-97 %, 64 = square. Every foot becomes the "
      "AND of itself and a copy shifted by part of its period (pulse staircases on the "
      "saw net).",
      "pwm1, pwm2 (0): the MG moves the width, 127 = +-47 %.",
      "Walsh loop: the four feet are square waves of one counter, an octave apart. "
      "Multiplying them (XOR of the counter bits) gives 15 Walsh functions, square-like "
      "waves with 1 to 8 cycles per 16' period (sequency) and their energy near that "
      "harmonic. They come from the same counter edges, so they are exactly in tune and "
      "band-limited - a wavetable the chip already has. The loop orders them by "
      "sequency: moving along it sweeps the energy from the 16' up to the 8th harmonic "
      "and back, neighbours fading at equal power.",
      "wsh1, wsh2 (0): position on the loop 0-127 (0: 16' / 17: 16'x4' / 59: 2' / "
      "85: all four / 119: 16'x8' / 127 = 0 again).",
      "wmx1, wmx2 (0): mix from the normal feet (0) to the loop (127).",
      "wsm (0): the MG moves the position +-wsm steps (127: the whole loop, a "
      "sweeping, vocal sound - try mgr 2-8).",
      "wse (0): DEG3 moves the position by up to wse steps (-127..127), e.g. a bright "
      "attack falling into the 16'. wsm and wse act where wmx is above 0.",
      "sync (0): 1: DCO2's counter restarts at every 16' period of DCO1, at the exact "
      "sub-sample time.",
      "swp2 (0): DCO2 offset 0-72 semitones.",
      "swe (0): DEG3 adds up to 0-72 semitones to DCO2.",
      "swm (0): the MG adds +-0-24 semitones to DCO2. Screaming sync: sync 1, lev1 "
      "low, lev2 high, swe 40-72, a falling DEG3 (d3, su3 0), res and drv up. The "
      "sweeps also work without sync.",
      "ring (0): 0-127, DCO2 is multiplied by a +-1 wave of DCO1 (XOR, the ring "
      "modulation of square waves). At 127 only sum and difference tones remain, "
      "lev1 0 for the pure ring sound.",
      "rmf (0): the modulator, a step 0-14 on the Walsh loop of DCO1: 0 its 16' "
      "(softest), 1 8', 3 4', 7 2', 10 all four feet, back down to 14 16'x8'. Steps "
      "towards 7 move the sidebands further apart: stronger, more metallic."]),
    ("FM",
     [[("fm1", 0, 127), ("fm2", 0, 127)],
      [("fms", 0, 2), ("fmr", 0.125, 16), ("fmhz", -1000, 1000), ("fmig", 0, 127)]],
     ["fms 0", "fms 1", "fms 2"],
     ["Linear FM of the counter rate, through zero (the counter runs backwards) above "
      "index 1, every edge band-limited. Not on the Poly-800.",
      "fm1, fm2 (0): depth on DCO1 / DCO2, 0-127 = index 0-16 (1 at about 32), 0 = off.",
      "fms (0): chooses the modulator, it switches nothing on or off: 0 a sine per DCO "
      "at fmr x its own 16' frequency plus fmhz, 1 the other DCO of the voice (cross "
      "FM: DCO2 into DCO1 and DCO1 into DCO2), 2 the signal at the right inlet (about "
      "-1..1: osc~, adc~, noise~ ...), here from the osc~ above.",
      "fmr (1): 0.125-16, whole ratios stay harmonic, others (1.41, 3.5) give bells "
      "and metal that follow the key.",
      "fmhz (0): -1000..1000 Hz added to the sine: a fixed, inharmonic offset with the "
      "same beat on every key (fmr 0.125, fmhz 200: almost a fixed modulator).",
      "fmig (64): gain of the signal at the right inlet (fms 2), 64 = 1, 127 = about 2."]),
    ("Filter & DEG3 settings",
     [[("cut", 0, 99), ("res", 0, 15), ("kbd", 0, 2)],
      [("egi", 0, 15), ("pol", 1, 2), ("trg", 1, 2)],
      [("env3", -1, 127)],
      [("a3", 0, 31), ("d3", 0, 31), ("bp3", 0, 31), ("sl3", 0, 31), ("su3", 0, 31), ("r3", 0, 31)],
      [("nois", 0, 15)],
      [("drv", 0, 127), ("drvp", 0, 1)],
      [("vcut", 0, 127)]], [],
     ["One 4-pole VCF for the instrument (NJM2069) and one DEG3 for all voices, "
      "measured on a Poly-800 II.",
      "cut (99): 0-99 = 66 Hz - 20 kHz, 13 steps per octave.",
      "res (0): 0-15, an uncompensated ladder (the bass falls as it rises), no "
      "self-oscillation.",
      "kbd (0): key tracking of the highest key: 0 off, 2 full (1:1, C4 nominal), 1 "
      "half (1:2, C6 nominal).",
      "egi (0): EG INT 0-15, DEG3 moves the cutoff up to 7.9 octaves.",
      "pol (2): 2 up, 1 down.",
      "trg (2): 2 multi: every key restarts DEG3, 1 single: only the first key of a phrase.",
      "env3 (0): DEG3 morph as env1 (see Controls), -1 = own values.",
      "a3 d3 bp3 sl3 su3 r3: attack, decay, break point, slope, sustain, release of "
      "DEG3, 0-31.",
      "nois (0): white noise 0-15, its VCA follows DEG3.",
      "drv (0): overdrive (not on the Poly-800), soft clipping, 0-127 = 0-48 dB more "
      "gain with level makeup.",
      "drvp (0): 0 post: after the VCF, denser with res and chords, 1 pre: into the "
      "VCF, which rounds the distortion off (warmer).",
      "vcut (0): velocity to the cutoff: at 127 a soft key is up to 5 octaves lower, "
      "full velocity plays the cut set (one VCF: the last key counts)."]),
    ("MG settings",
     [[("mgr", -1, 15), ("mgf", 0, 30)], [("mgi", 0, 200), ("mgd", 0, 15)], [("mgv", 0, 15)]], [],
     ["One triangle MG for the instrument, measured on a Poly-800 II. It also drives "
      "hmg, pwm1/2, wsm and swm (DCO settings). More LFOs: LFO2 and LFO3 below.",
      "mgr (-1): frequency in steps 0-15 as parameter 81 (0: 0.29 Hz / 3: 1.26 / 8: 5.2 "
      "/ 15: 19 Hz), sets mgf. -1 = use mgf.",
      "mgf (5): the same in Hz.",
      "mgi (0): depth on the pitch in cents (83: 12.4 cents per step, 186 at 15), a "
      "14.5 Hz low-pass makes fast vibrato shallower, as on the original.",
      "mgd (0): delay 0-15 (82: 0.47 s at 5, 1.55 s at 15) from the first key, then "
      "full depth.",
      "mgv (0): MG to the VCF 0-15 (84: +-7.9 octaves at 15)."]),
    ("DEG4 settings",
     [[("env4", -1, 127), ("trg4", 1, 2)],
      [("a4", 0, 31), ("d4", 0, 31), ("bp4", 0, 31), ("sl4", 0, 31), ("su4", 0, 31), ("r4", 0, 31)],
      [("d4t1", 0, 32), ("d4a1", -127, 127), ("d4t2", 0, 32), ("d4a2", -127, 127)]],
     ["d4t1 cut", "d4t1 swe", "d4t1 fm1"],
     ["A modulation envelope (not on the Poly-800), one for all voices like DEG3. The "
      "Poly-800 is paraphonic: every voice has its own DEG1 / DEG2, but the VCF, DEG3 "
      "and the MG exist once - and so do DEG4, LFO2 and LFO3. Each of them has two "
      "target slots: t = target (number or name, e.g. d4t1 cut), a = amount -127..127 "
      "(negative inverts).",
      "env4 (0): DEG4 morph as env1 (see Controls), -1 = own values.",
      "trg4 (2): 2 multi: every key restarts DEG4, 1 single: only the first key of a "
      "phrase.",
      "a4 d4 bp4 sl4 su4 r4: attack, decay, break point, slope, sustain, release of "
      "DEG4, 0-31. DEG4 runs 0..1 (its level, linear).",
      "d4t1, d4a1, d4t2, d4a2: two targets and amounts. E.g. cut with a pluck (filter "
      "sweep per key), swe with sync 1 (sync sweep), fm1 (FM that decays), l2d (an "
      "LFO2 that comes in).",
      " ",
      "Targets (number name: parameter, range at amount 127):",
      "0 off: no target",
      "1 pitch: pitch, both DCOs +-24 semitones",
      "2 pitch2: pitch DCO2 +-48 semitones",
      "3 detn: detune DCO2 +-50 cents",
      "4 har1: timbre DCO1 +-127 steps",
      "5 har2: timbre DCO2 +-127 steps",
      "6 wsh1: Walsh position DCO1 +-127 steps",
      "7 wsh2: Walsh position DCO2 +-127 steps",
      "8 wmx1: Walsh mix DCO1 +-127",
      "9 wmx2: Walsh mix DCO2 +-127",
      "10 pw1: pulse width DCO1 +-47 %",
      "11 pw2: pulse width DCO2 +-47 %",
      "12 fm1: FM depth DCO1 +-16 index",
      "13 fm2: FM depth DCO2 +-16 index",
      "14 fmr: FM ratio x/: 4",
      "15 ring: ring modulation amount +-127",
      "16 swe: sync sweep depth +-72 semitones",
      "17 lev1: level DCO1 +-127",
      "18 lev2: level DCO2 +-127",
      "19 amp: output level x 0..2 (tremolo)",
      "20 pan: balance left / right",
      "21 nois: noise level +-15",
      "22 cut: cutoff filter +-8 octaves",
      "23 res: resonance filter +-15",
      "24 drv: overdrive +-127",
      "25 mgr: MG rate x/: 8",
      "26 mgi: MG pitch depth +-200 cents",
      "27 l2r: LFO2 rate x/: 8",
      "28 l2d: LFO2 depth x 0..2",
      "29 l3r: LFO3 rate x/: 8",
      "30 l3d: LFO3 depth x 0..2",
      "31 dtim: delay time x/: 2",
      "32 dlev: delay level +-15",
      "har, wsh, wmx, lev, detn and drv follow per audio block (smoothed by mrph), all others per sample."]),
    ("LFO2 & LFO3 settings",
     [[("l2r", 0.01, 50), ("l2w", 0, 5), ("l2k", 0, 2), ("l2f", 0, 10)],
      [("l2t1", 0, 32), ("l2a1", -127, 127), ("l2t2", 0, 32), ("l2a2", -127, 127)],
      [("l3r", 0.01, 50), ("l3w", 0, 5), ("l3k", 0, 2), ("l3f", 0, 10)],
      [("l3t1", 0, 32), ("l3a1", -127, 127), ("l3t2", 0, 32), ("l3a2", -127, 127)]],
     ["l2t1 pitch", "l2t1 har1", "l3t1 cut"],
     ["Two more LFOs (not on the Poly-800), one each for all voices, output -1..1.",
      "l2r, l3r (2): rate 0.01-50 Hz.",
      "l2w, l3w (0): wave 0 triangle, 1 sine, 2 saw up, 3 saw down, 4 square, 5 sample "
      "& hold (a new random value each cycle).",
      "l2k, l3k (0): retrigger: 0 free running, 1 every key restarts the wave, 2 only "
      "the first key of a phrase.",
      "l2f, l3f (0): fade-in after a retrigger, 0-10 s.",
      "l2t1, l2a1, l2t2, l2a2: two targets and amounts of LFO2.",
      "l3t1, l3a1, l3t2, l3a2: the same for LFO3.",
      " ",
      "Targets (number name: parameter, range at amount 127):",
      "0 off: no target",
      "1 pitch: pitch, both DCOs +-24 semitones",
      "2 pitch2: pitch DCO2 +-48 semitones",
      "3 detn: detune DCO2 +-50 cents",
      "4 har1: timbre DCO1 +-127 steps",
      "5 har2: timbre DCO2 +-127 steps",
      "6 wsh1: Walsh position DCO1 +-127 steps",
      "7 wsh2: Walsh position DCO2 +-127 steps",
      "8 wmx1: Walsh mix DCO1 +-127",
      "9 wmx2: Walsh mix DCO2 +-127",
      "10 pw1: pulse width DCO1 +-47 %",
      "11 pw2: pulse width DCO2 +-47 %",
      "12 fm1: FM depth DCO1 +-16 index",
      "13 fm2: FM depth DCO2 +-16 index",
      "14 fmr: FM ratio x/: 4",
      "15 ring: ring modulation amount +-127",
      "16 swe: sync sweep depth +-72 semitones",
      "17 lev1: level DCO1 +-127",
      "18 lev2: level DCO2 +-127",
      "19 amp: output level x 0..2 (tremolo)",
      "20 pan: balance left / right",
      "21 nois: noise level +-15",
      "22 cut: cutoff filter +-8 octaves",
      "23 res: resonance filter +-15",
      "24 drv: overdrive +-127",
      "25 mgr: MG rate x/: 8",
      "26 mgi: MG pitch depth +-200 cents",
      "27 l2r: LFO2 rate x/: 8",
      "28 l2d: LFO2 depth x 0..2",
      "29 l3r: LFO3 rate x/: 8",
      "30 l3d: LFO3 depth x 0..2",
      "31 dtim: delay time x/: 2",
      "32 dlev: delay level +-15",
      "har, wsh, wmx, lev, detn and drv follow per audio block (smoothed by mrph), all others per sample."]),
    ("Delay settings",
     [[("dtim", 0, 99), ("dfb", 0, 15)], [("dmf", 0, 31), ("dmi", 0, 31)], [("dlev", 0, 15), ("dgld", 0, 1)]],
     ["delay flanger", "delay chorus1", "delay chorus2", "delay slap",
      "delay long", "delay off"],
     ["The digital delay of the Poly-800 II (71-75), measured: the echo is in phase on "
      "the left output and inverted on the right (a mono sum cancels it). A gate ends "
      "the echo train, changing a value mutes the echo for a moment, as on the original.",
      "dtim (30): 0-99 = 4.2 ms - 1.08 s (70: 216 ms).",
      "dfb (0): feedback 0-15 (15: -3.1 dB per echo).",
      "dmf (0): mod rate 0-31 = 0.05-5 Hz.",
      "dmi (0): mod depth 0-31 (31: the time swings 1:5, about 10 dB less per 10 steps "
      "below).",
      "dlev (0): echo level 0-15, 0 = off.",
      "dgld (1): 1: changes glide (time like a tape, feedback and level smoothed), for MIDI CC and modulation, 0: every change mutes the echo for 40 ms, as on the original.",
      "Presets: flanger (4 ms, slow sweep, feedback), chorus1 (fast, shallow), chorus2 "
      "(slow, wider), slap (120 ms), long (0.5 s with repeats), off."]),
    ("DEG1 & DEG2 settings",
     [[("a1", 0, 31), ("d1", 0, 31), ("bp1", 0, 31), ("sl1", 0, 31), ("su1", 0, 31), ("r1", 0, 31)],
      [("a2", 0, 31), ("d2", 0, 31), ("bp2", 0, 31), ("sl2", 0, 31), ("su2", 0, 31), ("r2", 0, 31)],
      [("vamp", 0, 127)]], [],
     ["Each voice's DCO1 and DCO2 have their own 6-stage DEG as on the Poly-800. "
      "Measured on a Poly-800 II: level 0-31 is about (L/31)^2, attack rises linearly "
      "(31: about 9 s), decay, slope and release fall at a constant dB per second (31: "
      "about 18 s to -40 dB). env1 / env2 (Controls) morph through: 0 gate / 1 short "
      "pluck / 24 pluck / 48 piano / 72 long release / 92 swell / 108 pad / 127 long pad.",
      "a1 d1 bp1 sl1 su1 r1: attack, decay, break point, slope, sustain, release of "
      "DEG1 (DCO1), 0-31. Setting one makes env1 -1.",
      "a2 d2 bp2 sl2 su2 r2: the same for DEG2 (DCO2), env2 -1.",
      "vamp (0): velocity to the level of each voice (both DCOs, not on the "
      "Poly-800): 0 off, 127 level = velocity."]),
    ("Portamento",
     [[("port", 0, 127), ("pmod", 0, 2)], [("mono", 0, 1), ("retr", 0, 1)]], [],
     ["Not on the Poly-800.",
      "port (0): glide time 0-127 = off to 3 s.",
      "pmod (0): where a glide starts: 0 from the last note played, 1 from the note "
      "this voice played before, 2 from the last note only while another key is still "
      "held (legato glide). pmod is not a mono / poly switch.",
      "mono (0): 0 poly (4 voices), 1 mono: one voice, the last key wins.",
      "retr (1): in mono, 1: every key restarts the envelopes, 0: legato, only the "
      "first key of a phrase restarts them, further keys (and going back to a held "
      "one) only change the pitch."]),
    ("Additional settings",
     [[("bnd", 0, 12), ("ch1", 0, 16)], [("treb", -6, 6), ("bass", -6, 6)],
      [("sprd", 0, 127), ("mrph", 0, 100)], [("tone", 0, 1), ("leak", 0, 1)], [("drft", 0, 127)]], [],
     ["bnd (2): bend range in semitones (original: up to 7). Pitch bend from a MIDI "
      "keyboard comes in through [bendin] at the top.",
      "ch1 (0): MIDI channel for p v ch lists, 0 = any.",
      "treb, bass (0): EQ -6..6 (76/77, measured: about 4 dB at 3, 10 dB at 6, "
      "shelves at 4.3 kHz and 115 Hz).",
      "sprd (0): stereo spread of the 4 voices 0-127.",
      "mrph (25): smoothing time in ms of every continuous setting (timbre, levels, cutoff, resonance, FM, ring, drive, noise, DCO2 pitch, detune, vibrato, EG depth ...), so MIDI CC steps become ramps. 0 = none.",
      "tone (1): 1: output stage measured on a Poly-800 (54 Hz high-pass, VCF, 1 pole "
      "at 5 kHz), 0: raw tone generators.",
      "leak (0): 1: released voices stay at -55 dB, as on the original.",
      "drft (0): analogue drift (not on the Poly-800, whose pitch comes from a crystal "
      "and dividers), like the slop / vintage knob of newer Sequential instruments: "
      "every DCO of every voice wanders slowly and on its own (at 127 about +-3 cents, peaks about 8), "
      "each note's envelopes run a little faster or slower (+-20 %, DEG3 too), the MG "
      "rate wanders +-10 % and the cutoff +-0.2 octaves. 0 = exact."]),
]

VINT = []


def esc(s):
    return s.replace(",", " " + COMMA).replace(";", SEMI).replace("$", DOLLAR)


def label(s):
    return s.replace(" ", SPACE)


class Patch:
    def __init__(self):
        self.lines, self.conns, self.n = [], [], 0

    def add(self, text):
        self.lines.append(text)
        self.n += 1
        return self.n - 1

    def conn(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def cnv(self, x, y, w, h, lbl="empty", xo=20, yo=12, fs=30, bg=YELLOW, sel=19):
        return self.add(f"#X obj {x} {y} cnv {sel} {w} {h} empty empty {lbl} {xo} {yo} 0 {fs} {bg} #000000 0;")

    def heading(self, x, y, text, fs):
        return self.cnv(x, y, 10, 10, label(text), LBL, 6, fs, BLUE, 10)

    def comment(self, x, y, lines, width=MAXCH + 2, para=False):
        if not para:
            for ln in lines:
                assert len(ln) <= MAXCH, f"line too long ({len(ln)}): {ln}"
        body = (" " + SEMI + " ").join(esc(l) for l in lines)
        return self.add(f"#X text {x} {y} {body}, f {width};")


def nlines(paras, width=MAXCH):
    return sum(len(wrap(t, width)) for t in paras)


def rows_of(groups):
    out = []
    for g in groups:
        for k in range(0, len(g), 4):
            out.append(g[k:k + 4])
    return out


def wrap(text, width=MAXCH):
    out, cur = [], ""
    for w in text.split():
        if cur and len(cur) + 1 + len(w) > width:
            out.append(cur); cur = w
        else:
            cur = (cur + " " + w).strip()
    return out + [cur]


def split_text(groups, paras):
    """intro paragraphs, and the paragraphs of each group (by the names they start with)"""
    import re
    def owner(t):
        m = re.match(r"^([a-z0-9][a-z0-9 ,/]*?)( \(|:)", t)
        if not m:
            return None
        names = [n for n in re.split(r"[ ,/]+", m.group(1)) if n]
        for gi, g in enumerate(groups):
            if any(n == it[0] for it in g for n in names):
                return gi
        return None
    intro, per, pending, seen = [], [[] for _ in groups], [], False
    for t in paras:
        gi = owner(t)
        if gi is None:
            (pending if seen else intro).append(t)
        else:
            seen = True
            per[gi] += pending + [t]; pending = []
    per[-1] += pending
    return intro, per


ROWA = 62
GAP = 4


def draw_block(p, title, groups, presets, paras, y, bus):
    """draws a settings block at y (p None: layout only), returns the y after it"""
    if p: p.heading(CX, y, title, 14)
    y += 28
    intro, per = split_text(groups, paras)
    if intro:
        if p: p.comment(TX, y, intro, para=True)
        y += LINE * nlines(intro) + GAP
    for gi, g in enumerate(groups):
        for row in rows_of([g]):
            if p:
                snd = p.add(f"#X obj {CX + 400} {y + 24} s {bus};")
                for c, (nm, lo, hi) in enumerate(row):
                    a = p.add(f"#X floatatom {CX + c * 100} {y} 6 {lo} {hi} 0 - - -;")
                    m = p.add(f"#X msg {CX + c * 100} {y + 24} {nm} {DOLLAR}1;")
                    p.conn(a, 0, m, 0); p.conn(m, 0, snd, 0)
            y += 54
        last = gi == len(groups) - 1
        if presets and (title == "FM" and gi == 1 or title != "FM" and last):
            if p:
                sp = p.add(f"#X obj {CX + 400} {y} s {bus};")
                for i, txt in enumerate(presets):
                    r, c = divmod(i, 3)
                    m = p.add(f"#X msg {CX + c * 130} {y + 34 * r} {txt};")
                    p.conn(m, 0, sp, 0)
            y += 34 * ((len(presets) + 2) // 3)
        if title == "FM" and gi == 1:                       # external modulator -> right inlet
            if p:
                fa = p.add(f"#X floatatom {CX} {y} 6 0 0 0 - - -;")
                osc = p.add(f"#X obj {CX} {y + 28} osc~ 110;")
                ss = p.add(f"#X obj {CX} {y + 56} s~ {DOLLAR}0-fmin;")
                p.add(f"#X text {CX + 90} {y + 28} external modulator (fms 2);")
                p.conn(fa, 0, osc, 0); p.conn(osc, 0, ss, 0)
            y += 86
        if per[gi]:
            if p: p.comment(TX, y, per[gi], para=True)
            y += LINE * nlines(per[gi]) + GAP
    return y + 10


def build():
    p = Patch()
    n = len(CONTROLS)
    rows = (n + 3) // 4
    top_y = YEL_Y + 3 * M + 24 + 28
    util_y = top_y + 66
    ctl_y0 = util_y + 56
    ROW = 90
    last_s = ctl_y0 + ROW * (rows - 1) + 58
    r_y = last_s + 18 + 20
    obj_y = r_y + 36
    mul_y, dac_y = obj_y + 36, obj_y + 72
    y = dac_y + 18 + 36

    sections = []
    sections.append(("Inlets", ["0: messages (see Messages below)",
                                 "1: signal, external FM modulator (fms 2, see FM)"]))
    sections.append(("Outlets", ["0: signal left", "1: signal right",
                                 "2: events: value, dump"]))
    sections.append(("Messages", [
        "<pitch> <velocity>, note <p> <v>: note on/off (0 = off)",
        "<p> <v> <channel>: from [notein], filtered by ch1",
        "<control> <0-127>: set a control by name, e.g. har1 64",
        "ctl <index> <0-127>, params v0 v1 ...: set by index",
        "<setting> <value>: e.g. cut 60 (see the settings blocks below)",
        "delay flanger / chorus1 / chorus2 / slap / long / off",
        "bend <-1..1>: pitch bend of all voices, times bnd semitones",
        "n <name> <0-127>: a setting from a pot, CV or MIDI CC,",
        "  scaled to its range (switches in zones, 64 = 0 if bipolar)",
        "nget <name> <tag>: outlet 3 gets norm <tag> <0-127>,",
        "  the setting as a pot position (for pickup)",
        "disp <name>: outlet 3 gets disp <label> <value>,",
        "  two symbols for a display (OLED)",
        "oled <host> <port>: changed settings to O2O (an OLED) as",
        "  OSC, sent from a thread of its own",
        "oledtext <line 1> <line 2>: two lines for the OLED",
        "defaults, reset: default values, reset also stops all voices",
        "panic: all voices off",
        "get <name>: reply 'value <name> <v>' on outlet 2",
        "print: voices, values and sample rate to the Pd window",
        "dump: send all values to outlet 2 as 'dump " + SHORT + " v0 ..'",
        "dump " + SHORT + " v0 v1 ...: restore such a dump (this voice only)",
        "voice <i> <p>, voiceoff <i>: play one voice directly"]))
    clines = [f"{i} {cn} ({dv}): {txt}" for i, (cn, dv, txt) in enumerate(CONTROLS)]
    sections.append(("Controls (index name default)", clines, 0, True))
    sections.append(("Timbre (har1 / har2)", [
        "The 15 distinct timbres of the original, dark to bright:",
        "0: 16' / saw net 9: 16'2' / 18: 16'4'2' / 27: 16'4' /",
        "36: 16'8'4' / 45: all / 54: 16'8'2' / 64: 16'8' /",
        "square net 73: 16'8' / 82: 16'8'4' / 91: 16'4' /",
        "100: 16'4'2' / 109: all / 118: 16'8'2' / 127: 16'2'.",
        "One foot or the net per step. The combinations without 16'",
        "are the same waves higher: use oct1 / oct2 2-4.",
        "A fading foot rises in power, so the morph has no dips."]))
    sections.append(("Presets", [
        "Two read-only lists send presets to the object, changing",
        "slots never writes anything back. Both send 'dump pdoly8 ...'",
        "messages (here via [s], nothing is wired across the patch).",
        "[p8factory]: the 64 Poly-800 II factory programs, slot 0-63",
        "(0 = program 11, 7 = 18, 8 = 21 ... 63 = 88), read from",
        "presets/poly800mk2-factory.txt next to the abstraction.",
        "[p8user name]: your own sounds in a [text define -k name]",
        "(saved with the patch). A slot number recalls; only",
        "'store N' writes: it stores the current sound in slot N",
        "(it needs outlet 2 of pdoly8~ at its right inlet, here",
        "via [s]/[r]). 'write file' / 'read file' on the text",
        "define save and load banks. tools/p8sysex.py converts",
        "Poly-800 II sysex dumps to preset text and back."], 200))

    placed = []
    for sec in sections:
        title, lines = sec[0], sec[1]
        extra = sec[2] if len(sec) > 2 else 0
        para = sec[3] if len(sec) > 3 else False
        placed.append((title, lines, y, extra, para))
        y += 24 + LINE * (nlines(lines) if para else len(lines)) + 28 + extra
    # setting blocks: number boxes (each row into its own [s $0-pdoly8]),
    # preset messages, then the explanation
    ROWA = 62
    placed_b = []
    for title, items, presets, text in BLOCKS:
        placed_b.append((title, items, presets, text, y))
        y = draw_block(None, title, items, presets, text, y, None)
    credits_y = y
    yel_bottom = credits_y + 3 * LINE + M
    bar_bottom = yel_bottom + M

    p.cnv(YEL_X, YEL_Y, YEL_W, yel_bottom - YEL_Y, bg=YELLOW)
    p.cnv(PINK_X, PINK_TOP, PINK_W, bar_bottom - PINK_TOP, bg=PINK)
    p.cnv(BLUE_X, BLUE_TOP, BLUE_W, bar_bottom - BLUE_TOP, bg=BLUE)

    p.heading(CX, YEL_Y + M, OBJ, 22)
    p.heading(CX, YEL_Y + 3 * M, SUBTITLE, 16)

    bus = DOLLAR + "0-" + SHORT
    o = p.add(f"#X obj {CX} {obj_y} {OBJ};")
    rcv = p.add(f"#X obj {CX} {r_y} r {bus};")
    p.conn(rcv, 0, o, 0)
    rfm = p.add(f"#X obj {CX + 100} {r_y} r~ {DOLLAR}0-fmin;")
    p.conn(rfm, 0, o, 1)
    snd0 = p.add(f"#X obj {CX} {util_y + 30} s {bus};")
    # demo: a chord every 500 ms, released after 250 ms
    tgl = p.add(f"#X obj {CX} {top_y} tgl 19 0 empty empty empty 0 -10 0 12 #fcfcfc #000000 #000000 0 1;")
    met = p.add(f"#X obj {CX} {top_y + 28} metro 500;")
    p.conn(tgl, 0, met, 0)
    tbb = p.add(f"#X obj {CX + 100} {top_y} t b b;")
    p.conn(met, 0, tbb, 0)
    on = p.add(f"#X msg {CX + 160} {top_y} note 48 100 {COMMA} note 55 90 {COMMA} note 60 80 {COMMA} note 64 70;")
    dly = p.add(f"#X obj {CX + 100} {top_y + 28} delay 250;")
    off = p.add(f"#X msg {CX + 190} {top_y + 28} note 48 0 {COMMA} note 55 0 {COMMA} note 60 0 {COMMA} note 64 0;")
    p.conn(tbb, 1, on, 0); p.conn(tbb, 0, dly, 0); p.conn(dly, 0, off, 0)
    p.conn(on, 0, snd0, 0); p.conn(off, 0, snd0, 0)
    x = CX
    for text in ("60 100", "60 0", "dump", "defaults", "panic", "print"):
        m = p.add(f"#X msg {x} {util_y} {text};")
        p.conn(m, 0, snd0, 0)
        x += 7 * len(text) + 4 + 16
    snds = {}
    for i, (cname, _, _) in enumerate(CONTROLS):
        r, c = divmod(i, 4)
        cx = CX + c * 112
        ay = ctl_y0 + ROW * r
        if r not in snds:
            snds[r] = p.add(f"#X obj {CX} {ay + 58} s {bus};")
        hi = 12 if cname == "intv" else 127
        a = p.add(f"#X floatatom {cx} {ay} 4 0 {hi} 0 - - -;")
        m = p.add(f"#X msg {cx} {ay + 30} {cname} {DOLLAR}1;")
        p.conn(a, 0, m, 0)
        p.conn(m, 0, snds[r], 0)
    # MIDI keyboard straight into the object
    nin = p.add(f"#X obj {CX + 200} {r_y - 36} notein;")
    pck = p.add(f"#X obj {CX + 200} {r_y} pack f f f;")
    p.conn(nin, 0, pck, 0); p.conn(nin, 1, pck, 1); p.conn(nin, 2, pck, 2)
    p.conn(pck, 0, o, 0)
    bin_ = p.add(f"#X obj {CX + 330} {r_y - 36} bendin;")
    bex = p.add(f"#X obj {CX + 330} {r_y} expr ($f1 - 8192) / 8192;")
    bms = p.add(f"#X msg {CX + 330} {obj_y} bend {DOLLAR}1;")
    p.conn(bin_, 0, bex, 0); p.conn(bex, 0, bms, 0); p.conn(bms, 0, o, 0)
    mull = p.add(f"#X obj {CX} {mul_y} *~ 0.3;")
    mulr = p.add(f"#X obj {CX + 100} {mul_y} *~ 0.3;")
    dac = p.add(f"#X obj {CX} {dac_y} dac~;")
    pr = p.add(f"#X obj {CX + 330} {mul_y} print {SHORT.upper()};")
    p.conn(o, 0, mull, 0); p.conn(o, 1, mulr, 0)
    p.conn(mull, 0, dac, 0); p.conn(mulr, 0, dac, 1)
    p.conn(o, 2, pr, 0)
    sev = p.add(f"#X obj {CX + 330} {mul_y + 30} s {DOLLAR}0-{SHORT}-out;")
    p.conn(o, 2, sev, 0)

    for title, lines, sy, extra, para in placed:
        p.heading(CX, sy, title, 14)
        p.comment(TX, sy + 24, lines, para=para)
        if extra:
            ly = sy + 24 + LINE * len(lines) + 10
            if title.startswith("Presets"):
                ev = DOLLAR + "0-" + SHORT + "-out"
                f1 = p.add(f"#X floatatom {TX} {ly} 4 0 63 0 factory - -;")
                pf = p.add(f"#X obj {TX} {ly + 30} p8factory;")
                s1 = p.add(f"#X obj {TX} {ly + 60} s {bus};")
                p.conn(f1, 0, pf, 0); p.conn(pf, 0, s1, 0)
                f2 = p.add(f"#X floatatom {TX + 170} {ly} 4 0 0 0 user - -;")
                f3 = p.add(f"#X floatatom {TX + 250} {ly} 4 0 0 0 store - -;")
                m3 = p.add(f"#X msg {TX + 250} {ly + 26} store {DOLLAR}1;")
                p.conn(f3, 0, m3, 0)
                rv = p.add(f"#X obj {TX + 340} {ly + 26} r {ev};")
                pu = p.add(f"#X obj {TX + 170} {ly + 56} p8user {DOLLAR}0-mine;")
                p.conn(f2, 0, pu, 0); p.conn(m3, 0, pu, 0); p.conn(rv, 0, pu, 1)
                s2 = p.add(f"#X obj {TX + 170} {ly + 86} s {bus};")
                p.conn(pu, 0, s2, 0)
                p.add(f"#X obj {TX + 170} {ly + 116} text define -k {DOLLAR}0-mine;")
                continue
            m1 = p.add(f"#X msg {TX} {ly} dir;")
            pc = p.add(f"#X obj {TX} {ly + 26} pdcontrol;")
            lp = p.add(f"#X obj {TX} {ly + 52} list prepend skpreset-help.pd;")
            m2 = p.add(f"#X msg {TX} {ly + 78} {SEMI} pd open {DOLLAR}1 {DOLLAR}2;")
            p.conn(m1, 0, pc, 0); p.conn(pc, 0, lp, 0); p.conn(lp, 0, m2, 0)
            p.add(f"#X text {TX + 50} {ly} <- click to open skpreset-help.pd, f 40;")
    for title, items, presets, text, y0 in placed_b:
        draw_block(p, title, items, presets, text, y0, bus)
    p.comment(TX, credits_y, ["Credits: Modelled on the Korg Poly-800 service manual;",
                              "MSM5232; scale ROM data as documented in MAME's",
                              "msm5232.cpp with help by Claude"])

    head = f"#N canvas 348 20 {YEL_X + YEL_W + 2 * M + 20} {bar_bottom + M} 12;"
    return head + "\n" + "\n".join(p.lines) + "\n" + "\n".join(p.conns) + "\n"


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else OBJ + "-help.pd"
    with open(out, "w") as f:
        f.write(build())
    print("wrote", out)
