#!/usr/bin/env python3
"""Texts of the pdoly8~ settings for its help patch (tools/gen_help.py): the eight main controls
and one paragraph per setting, grouped as in the former settings blocks. A paragraph starts with
the names it is about: "name (default): text" or "name1, name2 (default): text"."""

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
      "and metal that follow the key. From a pot, CC or the editor in harmonic steps "
      "(1/8 1/4 1/3 1/2 2/3 3/4 1 5/4 4/3 3/2 2 5/2 3 7/2 4-16), so 1 is always found again. "
      "Inharmonic: fmhz, or any ratio as a message (fmr 1.41).",
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
      "hmg, pwm1/2, wsm and swm (DCO settings).",
      "mgr (-1): frequency in steps 0-15 as parameter 81 (0: 0.29 Hz / 3: 1.26 / 8: 5.2 "
      "/ 15: 19 Hz), sets mgf. -1 = use mgf.",
      "mgf (5): the same in Hz.",
      "mgi (0): depth on the pitch in cents (83: 12.4 cents per step, 186 at 15), a "
      "14.5 Hz low-pass makes fast vibrato shallower, as on the original.",
      "mgd (0): delay 0-15 (82: 0.47 s at 5, 1.55 s at 15) from the first key, then "
      "full depth.",
      "mgv (0): MG to the VCF 0-15 (84: +-7.9 octaves at 15)."]),
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
    ("Performance",
     [[("atk", -63, 63), ("rel", -63, 63)], [("vol", 0, 127)]], [],
     ["Laid over the sound, not stored in it (Launchkey page 9 MORE). A sound recalled sets atk and rel back to 0.",
      "atk, rel (0): attack of DEG1 and DEG2, release of DEG1, DEG2 and DEG3, -63..63 "
      "= -31..+31 DEG steps on top of the sound's own or morphed values.",
      "vol (100): output volume, 100 = 0 dB, 127 = +4 dB, square law. Set up once, "
      "defaults and presets keep it."]),
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
      "ch1 (1): MIDI channel for p v ch lists and for the controller layer, 0 = any. It belongs to "
      "the setup: defaults and presets keep it (set it here or with MIDI ch in the editor).",
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

