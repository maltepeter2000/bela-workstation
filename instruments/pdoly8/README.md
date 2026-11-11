# pdoly8~

The tone generation of the Korg Poly-800 for Pure Data: 4 voices, one or two DCOs per voice, MIDI control. Built for the Bela Pepper (Eurorack) and running in any Pd (macOS, Linux, Bela). Envelopes (DEG1/2/3) and the VCF are built in, all measured on a Poly-800; chorus is not in this version.

Modelled on the Poly-800 service manual (circuits 19, 22, 27, 28, 31, 32): one master clock, the OKI MSM5232 with its integer divider ROM and binary foot counters, two tone generator groups, the square and saw resistor networks, and LEVEL1/LEVEL2 of the NJM2069. Every edge is band-limited, so the output has the analog edges of the original and no digital aliasing.

## Three versions

All three use the same external (`pdoly8~`), the same factory presets and the same MIDI CC table (`p8cc.pd`); changes to the engine apply to all of them.

| Version | Folder | Control | Display |
|---|---|---|---|
| **Desktop** (macOS, Linux, Windows) | repository root, Mac package in `bin/macos/` | `pdoly8~-help.pd`: number boxes, MIDI keyboard ([notein], [bendin]), MIDI CC / NRPN via [p8cc] | Pd window |
| **Bela Pepper** | `bela/` | 10 pages of 8 pots with pickup and CV-Lock, buttons, LEDs, USB MIDI with [p8cc] | LEDs, console |
| **Bela with OLED** (no Pepper) | `bela-oled/` | MIDI only: notes, bend, program change, CC / NRPN via [p8cc] | OLED via O2O, sent by `pdoly8~` from a thread of its own |

## Object

`[pdoly8~]` — no creation arguments.

| | Type | Function |
|---|---|---|
| Inlet 1 | messages | notes, controls, settings, presets |
| Inlet 2 | signal | external FM modulator (`fms 2`), about −1…1 |
| Outlet 1 | signal | left |
| Outlet 2 | signal | right |
| Outlet 3 | messages | events: `dump pdoly8 <values>`, `value <name> <v>` |

The messages follow the cz4~ / skandrums-pd conventions (`dump`, `params`, `defaults`, `get`).

### Controls (0–127, one per pot or MIDI CC)

| # | Name | Default | Function |
|---|---|---|---|
| 0 | `har1` | 45 | timbre of DCO1: one knob through all 15 distinct original timbres, dark to bright (see One-knob timbre); 45 = saw net, all feet |
| 1 | `har2` | 45 | timbre of DCO2 |
| 2 | `env1` | 0 | envelope of DCO1 (DEG1), one knob: 0 gate, from 1 very short pluck … pluck … piano … long release … swell … pad … long pad (see Envelopes); −1 = own values `a1` … `r1` |
| 3 | `env2` | 0 | envelope of DCO2 (DEG2), the same |
| 4 | `intv` | 0 | interval of DCO2 in semitones 0–12 (not 0–127: the Poly-800 II stores semitones) |
| 5 | `detn` | 0 | detune of DCO2 downwards; the original's steps 0–3 (0, 6.5, 13, 26 cents, Poly-800 II service manual) lie at 0, 42, 85, 127, linear in between |
| 6 | `lev1` | 127 | level of DCO1 |
| 7 | `lev2` | 0 | level of DCO2. Every voice always has both DCOs (the original's DOUBLE, 4 voices); the levels set the mix. When `lev1 + lev2` goes above 127 both are scaled down together |

### One-knob timbre (`har1`, `har2`)

The Poly-800 offers 2 nets × 15 foot combinations, but only 15 timbres are different: a combination without the 16' is the same wave as one with it an octave or more higher (8' 4' 2' = 16' 8' 4' up an octave, 2' alone = 16' three octaves up, in both nets, since the weight ratios stay the same), and 16' alone sounds the same in both nets. `har` puts these 15 on one knob, ordered by brightness (spectral centroid with the measured weights) and with one foot or the net changing per step:

| `har` | 0 | 9 | 18 | 27 | 36 | 45 | 54 | 64 | 73 | 82 | 91 | 100 | 109 | 118 | 127 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| net | – | saw | saw | saw | saw | saw | saw | saw | square | square | square | square | square | square | square |
| feet | 16' | 16' 2' | 16' 4' 2' | 16' 4' | 16' 8' 4' | all | 16' 8' 2' | 16' 8' | 16' 8' | 16' 8' 4' | 16' 4' | 16' 4' 2' | all | 16' 8' 2' | 16' 2' |
| brightness | 2.7 | 2.9 | 3.3 | 3.1 | 3.4 | 3.6 | 3.3 | 3.1 | 3.8 | 5.5 | 5.9 | 9.1 | 8.0 | 7.8 | 9.4 |

The combinations without 16' are reached with `oct1` / `oct2` 2–4: above the chip's HIGH the feet are taken one counter bit higher per octave, which is exactly what leaving out the 16' does (including the chip's clamp of the high feet in the top octave). A fading foot rises linearly in power, and the level changes between the timbres are the original's (up to 8 dB; the saw net is louder).

### Settings (raw values)

In the help patch the settings are grouped in blocks: DCO settings (timbre, octave, PWM, Walsh loop, sync, ring), FM, Filter & DEG3, MG, DEG4, LFO2 & LFO3, Delay (with preset messages), DEG1 & DEG2, Portamento, Additional settings.

| Message | Range | Default | Function |
|---|---|---|---|
| `oct1` / `oct2` | −1…4 | 0 | octave of DCO1 / DCO2: −1/0/1 = Low, Mid, High; 2–4 = the feet one, two, three counter bits higher (the original combinations without 16') |
| `nois` | 0–15 | 0 | white noise, sounds while a voice sounds |
| `sprd` | 0–127 | 0 | stereo spread of the 4 voices, continuous |
| `mrph` | ms | 25 | smoothing time of every continuous setting (timbre, levels, cutoff, resonance, FM, ring, drive, noise, DCO2 pitch, detune, vibrato, EG depth …): CC steps become ramps |
| `bnd` | 0–12 | 2 | pitch bend range in semitones (original up to ±7) |
| `mgf` | Hz | 5 | MG rate (also set by `mgr`) |
| `mgi` | 0–200 cents | 0 | vibrato depth (triangle on the master clock, through a measured 14.5 Hz one-pole low-pass, so fast vibrato is shallower as on the original). Original 83: 12.4 cents per step, 186 at 15 |
| `mgr` | 0–15, −1 | −1 | MG frequency in original steps (81, measured: 0 = 0.29 Hz, 3 = 1.26, 6 = 3.39, 8 = 5.22, 9 = 6.49, 12 = 11.1, 15 = 19.1 Hz); sets `mgf`; −1 = use `mgf` |
| `ch1` | 0–16 | 0 | MIDI channel for `p v ch` lists (0 = any) |
| `pw1` / `pw2` | 0–127 | 64 | pulse width of DCO1 / DCO2: 3–97 %, 64 = square; acts on every foot (see PWM) |
| `sync` | 0/1 | 0 | 1: DCO2 restarts at every 16' period of DCO1 (hard sync) |
| `swp2` | 0–72 | 0 | DCO2 pitch offset in semitones, continuous (sync sweeps; also without sync) |
| `swe` | 0–72 | 0 | DEG3 adds up to this many semitones to DCO2 (sync sweep) |
| `swm` | 0–24 | 0 | the MG adds ± this many semitones to DCO2 |
| `hmg` | 0–127 | 0 | the MG moves `har1` and `har2` by ± this many steps (127 = the whole timbre path) |
| `pwm1` / `pwm2` | 0–127 | 0 | MG → pulse width of DCO1 / DCO2, 127 = ±47 % |
| `ring` | 0–127 | 0 | DCO2 times a ±1 wave of DCO1 (XOR ring modulation of square waves) |
| `rmf` | 0–14 | 0 | ring modulator: a step on DCO1's Walsh loop (0 = 16', 7 = 2', 10 = all four feet); steps towards 7 give wider sidebands |
| `fm1` / `fm2` | 0–127 | 0 | linear FM index of DCO1 / DCO2, 0–16 (quadratic; index 1 at about 32, through zero above) |
| `fmr` | 0.125–16 | 1 | ratio of the sine modulator to the DCO's own 16' frequency |
| `fmhz` | −1000…1000 | 0 | fixed offset in Hz added to the sine modulator (inharmonic FM) |
| `fmig` | 0–127 | 64 | gain of the signal at inlet 2 (`fms 2`), 64 = 1 |
| `fms` | 0/1/2 | 0 | FM source (not an on/off switch): 0 internal sine, 1 the other DCO of the voice (cross FM), 2 the signal at inlet 2 |
| `wsh1` / `wsh2` | 0–127 | 0 | Walsh loop position of DCO1 / DCO2 (see Walsh morph) |
| `wmx1` / `wmx2` | 0–127 | 0 | mix from the feet sum (0) to the Walsh loop (127) |
| `wsm` | 0–127 | 0 | the MG moves the Walsh position ± this many steps (127 = the whole loop) |
| `wse` | −127…127 | 0 | DEG3 moves the Walsh position by up to this many steps |
| `leak` | 0/1 | 0 | 1: released voices stay at −55 dB, as measured on the original |
| `drft` | 0–127 | 0 | analogue drift (like the slop / vintage control of newer Sequential instruments): every DCO wanders slowly and independently (low-passed noise, about 0.25 Hz; at 127 about ±3 cents, peaks about 8), each note's envelopes run up to ±20 % faster or slower (DEG3 too), MG rate ±10 %, cutoff ±0.2 octaves |
| `a1 d1 bp1 sl1 su1 r1` | 0–31 | 0 0 31 0 31 0 | DEG1 attack, decay, break point, slope, sustain, release; setting one makes `env1` −1 |
| `a2 d2 bp2 sl2 su2 r2` | 0–31 | 0 0 31 0 31 0 | the same for DEG2 |
| `vamp` | 0–127 | 0 | velocity → level of each voice (both DCOs); 127: level = velocity |
| `port` | 0–127 | 0 | portamento, off … 3 s (quadratic; not on the Poly-800) |
| `pmod` | 0/1/2 | 0 | glide from: 0 the last note played, 1 the voice's own last note, 2 the last note only while another key is held (legato) |
| `mono` | 0/1 | 0 | 0 poly (4 voices); 1 mono: one voice, last key wins (2, from older presets, = mono 1 with `retr 0`) |
| `retr` | 0/1 | 1 | mono: 1 every key restarts the envelopes; 0 legato: only the first key of a phrase restarts them, further keys (and going back to a held one) only change the pitch |
| `cut` | 0–99 | 99 | VCF cutoff (66 Hz … 20 kHz, measured) |
| `res` | 0–15 | 0 | VCF resonance (ladder feedback k 0 … 3.9, no self-oscillation) |
| `drv` | 0–127 | 0 | overdrive (where: `drvp`): soft clipping (rational, as in cz4~), 0–48 dB more gain with level makeup |
| `drvp` | 0/1 | 0 | overdrive 0 post (after the VCF), 1 pre (into the VCF) |
| `vcut` | 0–127 | 0 | velocity → cutoff: at 127 a soft key is up to 5 octaves lower; full velocity plays the set cutoff (one VCF: the last key counts) |
| `kbd` | 0/1/2 | 0 | keyboard tracking off / half / full (highest key; full: 1 oct/oct, nominal at C4; half: 0.5 oct/oct, nominal at the top key C6 — measured) |
| `pol` | 1/2 | 2 | DEG3 polarity: 2 opens the filter, 1 closes it |
| `egi` | 0–15 | 0 | EG INT: DEG3 depth on the cutoff (15 = 7.9 octaves) |
| `trg` | 1/2 | 2 | DEG3 trigger: 1 single (first key only), 2 multi (every key) |
| `env3` | −1…127 | 0 | DEG3 morph position (same loop as `env1`); −1 = own values |
| `a3 d3 bp3 sl3 su3 r3` | 0–31 | 0 0 31 0 31 0 | DEG3 values |
| `tone` | 0/1 | 1 | 1: VCF and output stage (54 Hz high-pass, one pole at 5 kHz); 0: raw tone generators (5 Hz DC block only) |
| `mgd` | 0–15 | 0 | MG delay (82) after the first key of a phrase, measured 0 / 0.47 / 0.95 / 1.55 s at 0 / 5 / 10 / 15; then the MG comes in at full depth (no fade, as measured) |
| `mgv` | 0–15 | 0 | MG to VCF cutoff (84), measured ±0.53 octaves per step, ±7.9 at 15 (the same scale as EG INT) |
| `treb` `bass` | −6…6 | 0 | EQ (76/77), measured: first-order shelves (corners 4.26 kHz / 115 Hz, full boost +3 dB at 1.8 kHz / 316 Hz), about ±4 dB at ±3 and ±10 dB at ±6 |
| `dtim` | 0–99 | 30 | digital delay time (71): exponential, 4.2 ms … 1.08 s (measured: 70 → 216 ms, 99 → 1.08 s) |
| `dfb` | 0–15 | 0 | delay feedback (72): at 15 each echo is 3.1 dB quieter (measured); a gate ends the echo train about 48 dB under full scale, as on the original |
| `dmf` `dmi` | 0–31 | 0 | delay modulation rate (73, measured: 0.05 … 5 Hz, 11 → 0.26 Hz, 21 → 1.15 Hz) and depth (74: the delay time swings 1:5 at 31, as in the manual; the swing falls about 10 dB per 10 steps below that, measured) |
| `dlev` | 0–15 | 0 | delay effect level (75); 0 = off, 15 = as loud as the direct sound. The echo goes to the left output in phase and to the right one inverted (measured: L = dry + echo, R = dry − echo, so a mono sum of both cancels it), band-limited to 7 kHz |
| `dgld` | 0/1 | 1 | 1: delay changes glide (time like a tape, feedback and level smoothed), for CC and modulation; 0: every change mutes the echo for 40 ms, as on the original |

### DEG4, LFO2, LFO3 (modulation)

Not on the Poly-800. Like DEG3 and the MG they exist once for the instrument (the Poly-800 is paraphonic: every voice has its own DEG1/DEG2, but VCF, DEG3 and MG are shared). Each has two target slots: `…t1`/`…t2` = target (number or name, e.g. `d4t1 cut`), `…a1`/`…a2` = amount −127…127 (negative inverts).

| Message | Range | Default | Function |
|---|---|---|---|
| `env4` | −1…127 | 0 | DEG4 morph as `env1`; −1 = own values |
| `a4 d4 bp4 sl4 su4 r4` | 0–31 | 0 0 31 0 31 0 | DEG4 values; DEG4 runs 0…1 (its level, linear) |
| `trg4` | 1/2 | 2 | DEG4 trigger: 1 single, 2 multi |
| `d4t1 d4a1 d4t2 d4a2` | target, −127…127 | 0 | DEG4 targets and amounts |
| `l2r` / `l3r` | 0.01–50 Hz | 2 | LFO rate |
| `l2w` / `l3w` | 0–5 | 0 | wave: triangle, sine, saw up, saw down, square, sample & hold |
| `l2k` / `l3k` | 0/1/2 | 0 | retrigger: free, every key, first key of a phrase |
| `l2f` / `l3f` | 0–10 s | 0 | fade-in after a retrigger |
| `l2t1 l2a1 l2t2 l2a2`, `l3t1 …` | target, −127…127 | 0 | LFO targets and amounts |

Targets (range at amount 127): 0 `off`, 1 `pitch` (±24 semitones), 2 `pitch2` (DCO2 ±48), 3 `detn` (±50 cents), 4/5 `har1` `har2` (±127 steps), 6/7 `wsh1` `wsh2` (±127), 8/9 `wmx1` `wmx2` (±127), 10/11 `pw1` `pw2` (±47 %), 12/13 `fm1` `fm2` (±16 index), 14 `fmr` (×/÷ 4), 15 `ring` (±127), 16 `swe` (±72 semitones), 17/18 `lev1` `lev2` (±127), 19 `amp` (level × 0…2), 20 `pan` (balance), 21 `nois` (±15), 22 `cut` (±8 octaves), 23 `res` (±15), 24 `drv` (±127), 25 `mgr` (×/÷ 8), 26 `mgi` (±200 cents), 27/29 `l2r` `l3r` (LFO rate ×/÷ 8), 28/30 `l2d` `l3d` (LFO depth × 0…2), 31 `dtim` (×/÷ 2), 32 `dlev` (±15). `har`, `wsh`, `wmx`, `lev`, `detn` and `drv` follow per audio block (smoothed by `mrph`), all others per sample.

### Pots, CV and MIDI CC

| Message | Function |
|---|---|
| `n <name> <0-127>` | sets a setting from a 0–127 value (pot, CV, CC), scaled to its range: switches and small ranges in equal zones, −127…127 settings with 0 at 64, `fmr`/`l2r`/`l3r`/`mgf` logarithmic |
| `nget <name> <tag>` | outlet 3 gets `norm <tag> <0-127>`: the setting as a pot position (pickup) |
| `disp <name>` | outlet 3 gets `disp <label> <value>`: two symbols for a display, e.g. `Cutoff` `50.1`, `FM source` `other DCO`, `LFO2 target 1` `cut` |
| `oled <host> <port>` | sends the changed setting (as `disp`) and `oledtext` as OSC `/display-strings-and-numbers` to O2O (an OLED), from a thread of its own: no network call in the audio thread (Linux, macOS) |
| `oledtext <line 1> <line 2>` | two lines for the OLED, e.g. the program |

`n` takes fractions (14-bit controllers); settings with more than 15 values are set continuously. Every continuous setting, the modulation slots (amount, target, LFO edges) and the stereo spread and EQ are smoothed over `mrph` ms (default 25, long enough to bridge the 10–20 ms between the steps of a turned 7-bit knob) inside the engine, so 7-bit CC steps become ramps instead of zipper noise and clicks.

### MIDI CC (`p8cc.pd`)

`[p8cc]` turns MIDI CC (any channel, its own [ctlin]) and 14-bit NRPN into `n <name> <value>` for the object; the table is stored in the abstraction (generated by `tools/gen_p8cc.py` from `tools/gen_bela.py`). Made for a controller with 4 rows of 16 pots: 8 groups of 8, CC 20–83 straight, no setting twice.

| CC | Group | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| 20–27 | PLAY | `har1` | `har2` | `env1` | `env2` | `intv` | `detn` | `lev1` | `lev2` |
| 28–35 | FILTER | `cut` | `res` | `egi` | `env3` | `kbd` | `vcut` | `drv` | `nois` |
| 36–43 | OSC | `oct1` | `oct2` | `pw1` | `pw2` | `pwm1` | `pwm2` | `hmg` | `drft` |
| 44–51 | WALSH-RING | `wsh1` | `wsh2` | `wmx1` | `wmx2` | `wsm` | `wse` | `ring` | `rmf` |
| 52–59 | SYNC-FM | `sync` | `swp2` | `swe` | `swm` | `fm1` | `fm2` | `fmr` | `fmhz` |
| 60–67 | MG-FMSOURCE-DEG3 | `mgr` | `mgi` | `mgd` | `mgv` | `fms` | `fmig` | `pol` | `trg` |
| 68–75 | DELAY-EQ | `dtim` | `dfb` | `dmf` | `dmi` | `dlev` | `dgld` | `treb` | `bass` |
| 76–83 | PERFORM | `port` | `pmod` | `mono` | `retr` | `vamp` | `bnd` | `sprd` | `mrph` |

Without a pot: 84 `env4`, 85 `trg4`, 86 `d4t1`, 87 `d4a1`, 88 `d4t2`, 89 `d4a2`, 90 `drvp`, 91 `tone`, 92 `leak`, 93 `mgf`, 102 `l2r`, 103 `l2w`, 104 `l2k`, 105 `l2f`, 106 `l2t1`, 107 `l2a1`, 108 `l2t2`, 109 `l2a2`, 110 `l3r`, 111 `l3w`, 112 `l3k`, 113 `l3f`, 114 `l3t1`, 115 `l3a1`, 116 `l3t2`, 117 `l3a2`. NRPN: CC 99 = 0, CC 98 = the setting's CC number, CC 6 = MSB, CC 38 = LSB (only right after a CC 6; otherwise CC 38 is `pw1`). Inside the blocks are numbers some keyboards send themselves: 32 (bank select LSB, `kbd`) and 64–67 (pedals: a sustain pedal switches `fms`).

### Other messages

| Message | Function |
|---|---|
| `note p v`, or the list `p v` | note on (v > 0) / off; 4 voices, the oldest is stolen |
| list `p v ch` | from `[notein]` → `[pack f f f]`, filtered by `ch1` |
| `bend f` | pitch bend −1…1 of all voices (on the master clock), times `bnd`; from a MIDI keyboard: `[bendin]` → `[expr ($f1 - 8192) / 8192]` → `bend $1` (as in the help patch) |
| `params v0 v1 …` | set the controls by index |
| `defaults` / `reset` | default values; `reset` also stops all voices |
| `panic` | all voices off |
| `get <name>` | outlet 3 answers `value <name> <v>` |
| `voice i p` / `voiceoff i` | play a voice directly |
| `dump` | outlet 3 sends `dump pdoly8` + the 8 controls + the 60 settings |
| `dump pdoly8 …` | restore such a line; lines of other voices are ignored |
| `print` | voices, values and sample rate to the Pd window |
| `delay <name>` | delay presets: `flanger` (4 ms, slow sweep, feedback), `chorus1` (fast, shallow), `chorus2` (slow, wider), `slap` (120 ms), `long` (0.5 s with repeats), `off` |

Default dump line: see `dump` (all values, new settings are appended at the end, so older dumps still load).

### PWM (`pw1`, `pw2`)

Each foot becomes the AND of its square with a copy shifted by part of its period, a phase offset on the counter: the falling edge stays on the counter step, the rising edge moves, width 3–97 % of each foot's own period. With one foot this is classic PWM; with several feet the pulses stay locked, and on the saw net they form pulse staircases. The pulses are DC free, every edge is band-limited (a 10 % pulse on a 4.2 kHz 2' shows no component above −85 dB), and the width is smoothed like the other controls. At 64 the output is the plain square (the pulse path differs by less than −74 dB).

### Sync and FM

- **Hard sync (`sync 1`):** DCO2's counter restarts at every 16' period of DCO1, at the exact sub-sample time, with a band-limited step. Sweep DCO2 with `swp2` (continuous, 0–48 semitones) or `intv`/`oct2`; `lev1 0` lets DCO2 be heard alone. Feet, nets and PWM of DCO2 all go through the sync.
- **Sweeps (`swe`, `swm`):** DEG3 and the MG move DCO2's pitch (up to +72 and ±24 semitones on top of `swp2`, at most 96 in all). Screaming sync: `sync 1`, `lev1` low, `swe` 40–72 with a falling DEG3, `res` and `drv` up.
- **Ring (`ring`):** DCO2 is multiplied by the ±1 square of DCO1's 16' (the XOR ring modulation of square waves); each edge of DCO1 flips DCO2 with a band-limited step. At 127 only sum and difference tones remain (measured: DCO2 at 98 Hz over a 65.4 Hz 16' → 163 Hz strongest, 98 Hz itself gone except where 3·f1 − f2 lands on it).
- **Linear FM (`fm1`, `fm2`, `fmr`, `fmhz`, `fms`):** each DCO's counter rate is modulated by a sine at `fmr` times its own 16' frequency plus `fmhz` Hz, by the other DCO of the voice (cross FM in both directions), or by the signal at inlet 2. Counter speed is limited to 3 counts per sample (FM, sweeps), which keeps the edges per sample bounded; deep FM on high notes costs the most CPU. Above index 1 the counter runs backwards for a moment (through-zero FM), so integer ratios stay harmonic; the edges of every foot are found in both directions and stay band-limited.
- Measured: sync (DCO2 at +7.3 semitones) and FM (ratio 1, index 1 and 3) keep every component below 16 kHz harmonic within −75 dB; deep through-zero FM with sync on 4 low voices peaks at 0.72. With `fm 0` and `sync 0` the output is sample-identical to before.

### Walsh morph (`wsh`, `wmx`)

The four feet are Rademacher functions. Their products (the XOR of foot squares) are the 15 Walsh functions: two-level waves on the same counter edges, with their energy around harmonic 1 … 8 of the 16' (the sequency). They come in pairs of equal sequency; `wsh` moves along a closed loop that rises through one of each pair and falls through the other, so neighbours lie one harmonic apart and 127 joins 0. Neighbours fade at equal power (Walsh functions are orthonormal), at the loudness of the square net with all feet, so the level stays constant. Besides the octave squares this reaches energy at harmonics 3, 5, 6 and 7 — formant-like timbres that adding feet cannot make.

`wsm` and `wse` move the position with the MG and DEG3 (per audio block, smoothed by `mrph`).

| `wsh` | 0 | 8 | 17 | 25 | 34 | 42 | 51 | 59 | 68 | 76 | 85 | 93 | 102 | 110 | 119 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| feet multiplied | 16' | 8' | 16'·4' | 4' | 16'·4'·2' | 8'·2' | 16'·2' | 2' | 16'·8'·2' | 8'·4'·2' | all | 4'·2' | 16'·8'·4' | 8'·4' | 16'·8' |
| energy at harmonic | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |

`wmx` fades from the feet sum (nets, `har`, PWM) to the Walsh loop. Walsh works with PWM (products of the pulses), sync and FM: foot edges are sorted in time inside each sample, so every product changes at the right moment and stays band-limited.

### Envelopes (DEG1 / DEG2)

Every tone generator has its own 6-stage DEG as on the Poly-800 — attack, decay, break point, slope, sustain, release, 0–31 — acting on the tone generator before the foot buses, so the envelopes are truly polyphonic. Measured on a Poly-800 (one DCO, one key, line out, 35 notes):

- **Level 0–31:** the square root of the amplitude is close to linear in the level (amplitude ≈ (L/31)², a little lower below 12): 28 = −1.7 dB, 24 = −4.4, 20 = −7.5, 16 = −11.6, 12 = −17.0, 8 = −25.1, 4 = −40.9 dB.
- **Attack:** the level rises linearly; time from 0 to level 30: A16 0.45 s, A20 1.0 s, A24 2.2 s, A28 4.8 s, A31 8.7 s (about ×1.22 per step).
- **Decay, slope (down), release:** exponential, a constant dB per second; decay and release use the same table; time to −40 dB: 0 0.06 s, 8 0.14 s, 16 0.63 s, 20 1.4 s, 24 3.3 s, 28 6.9 s, 31 18.5 s (decay 31: 2.2 dB/s). Like the original, the time of a stage grows with the level difference it covers.
- **Break point 31** turns the DEG into a plain ADSR (slope from the peak to sustain); a rising slope (break point below sustain) is a second attack.
- **Release from anywhere:** a key released during attack or decay falls from the current level.
- **Leak:** a released voice does not close completely, it stays at about −55 dB until the voice is used again (`leak 1`).

Against the recording, the model's level curves of all 34 measured notes match within 0.82 dB on average (90 % within 1.6 dB); see `demos/deg_vs_poly800.png`.

**One knob (`env1`, `env2`, `env3`):** 0 is a plain gate; from 1 on a line through DEG settings, starting with a very short pluck (no fade from the gate), interpolated value by value (times therefore move logarithmically). Pluck, piano and long release have the widest stretches; from swell on attack and release only grow:

| `env` | 0 | 1 | 24 | 48 | 72 | 92 | 108 | 127 |
|---|---|---|---|---|---|---|---|---|
| sound | gate | very short pluck | pluck | piano | long release | swell | pad | long pad |
| A D BP SL SU R | 0 0 31 0 31 0 | 0 3 0 0 0 3 | 0 14 0 0 0 12 | 0 10 20 24 0 14 | 2 16 24 0 24 26 | 16 0 31 0 31 18 | 20 22 26 0 26 24 | 30 26 26 0 26 31 |

Measured once more on a Poly-800 Mk II: in both single and multi trigger a new note does not retrigger the held ones; every voice keeps its own envelope (as the manual says, the trigger mode concerns DEG3). Decay 16 on its own reaches −40 dB after 0.64 s, as release 16 (0.63 s).

The own values `a1` … `r2` override the loop (`env` −1). Changing the envelope while a note sounds changes the rates of the running stage, without jumps. The counters keep running on a new note, and a retriggered voice starts its attack from the level it is at.

### Portamento (`port`, `pmod`)

Not on the Poly-800, added here. On a new note the counters jump to the new divider at once, as on the chip; a pitch offset from the old note fades out exponentially on top of the clock (about 95 % after the set time), so every glide ends exactly on the chip's own pitch. The source is the last note played (`pmod 0`, good for lines over the 4 voices), the voice's own last note (`pmod 1`, chords glide voice by voice), or the last note only while another key is held (`pmod 2`, legato). `pmod` is not a mono/poly switch; that is `mono`, with `retr` for retrigger or legato.

### VCF and DEG3

One VCF for the whole instrument, as on the Poly-800 (NJM2069), measured on a real one with noise through the filter (`docs/vcf-calibration.md`):

- **Cutoff:** a 4-pole low-pass, 0–99 = 66, 128, 218, 364, 617, 1083, 1916, 3492, 6384, 11774 Hz at 0, 10 … 90 and about 20 kHz at 99 — about 13 steps per octave. The top value and an output pole at 5 kHz come from the first (open-filter) recording, with which the feet calibration now matches within 0.32 dB.
- **Resonance:** an uncompensated ladder: feedback k = 0, 0.53, 1.84, 3.07, 3.9 at 0, 4, 8, 12, 15, the bass falls by 1/(1+k) (−13.7 dB at 15), the cutoff does not move, no self-oscillation (k held below 4). Zero-delay-feedback ladder, solved exactly per sample.
- **DEG3:** one envelope for the instrument (paraphonic, as the original), same stages, ranges and timing as DEG1/2. Its level moves the cutoff linearly in octaves: EG INT 15 at full level = +7.9 octaves (cutoff 0 opens to about 15.7 kHz), EG INT 8 about half; polarity 1 inverts. It also sets the noise level, linearly (DEG3 sustain 10 ≈ −10 dB). Single trigger: only the first key of a phrase starts DEG3; multi: every key.
- **Noise:** level 0–15 linear in amplitude (12 = −1.8 dB, 8 = −5.3, 4 = −11.5, 1 = −24.7 dB, measured), through the VCF.
- **Keyboard tracking:** full follows the highest key 1:1 (the resonance peak stays on the same harmonic over three octaves), C4 = nominal cutoff; half by 1/2 with the nominal cutoff at the top key C6 (never brighter than the setting), both measured.

The filter sweep of DEG3 against the recording: `demos/vcf_deg3_vs_poly800.png`.

## Presets

Two read-only preset lists send `dump pdoly8 …` messages to the object (connect their outlet to the inlet of `pdoly8~`, or to its `[s]`). Changing a slot never writes anything back; slots start at 0.

- **`[p8factory]`** — the 64 Poly-800 II factory programs, slot 0–63 (0 = program 11, 7 = 18, 8 = 21, … 63 = 88), read on load from `presets/poly800mk2-factory.txt` next to the abstraction (an argument names another file).
- **`[p8user name]`** — your own sounds, in a `[text define -k name]` in your patch (saved with it; `write file` / `read file` on the text define save and load banks). A slot number recalls; only `store N` writes: it asks `pdoly8~` for a dump and stores the answer in slot N, so it needs outlet 2 of `pdoly8~` at its right inlet (e.g. via `[s]`/`[r]`). Empty slots (`-`) and slots past the end send nothing.

A preset line is `pdoly8 v0 v1 …` (the values of a `dump`, controls then settings).

**Sysex converter:** `tools/p8sysex.py` converts a Poly-800 II data dump (`F0 42 21 0C`, all 64 programs) into preset text and back:

    python3 tools/p8sysex.py syx2txt dump.syx bank.txt
    python3 tools/p8sysex.py txt2syx bank.txt dump.syx --template dump.syx
    python3 tools/p8sysex.py show dump.syx

It takes the value order from `pdoly8~.c`, so it follows the object. Waveform and feet become the one-knob timbre plus octave (a combination without 16' is the 16'-rooted one 1–3 octaves up; 16' alone in the square net is the same wave 7.9 dB quieter); levels 0–31 → 0–127; WHOLE → `lev2 0`; DEG1–3 as their own values; noise, VCF, DEG3, MG, delay and EQ step for step. Back to sysex the nearest Poly-800 values are taken and pdoly8~'s own extensions are dropped; `--template` keeps the sequencer, chord memory and MIDI settings of an existing dump. A round trip of the factory dump gives the same 64 programs, except that equivalent settings come back in one form (e.g. 8' 4' 2' at MID as 16' 8' 4' at HIGH, 16' alone square as saw at a lower level). The level law of the DCO levels is assumed linear (not measured).

## How it works

- **Master clock.** One clock for all 8 tone generators. Tune, bend and vibrato act on it, so all voices move together, as on the original.
- **MSM5232 dividers.** A key becomes a pitch code; the chip's 88 × 12 bit ROM gives an integer counter value and a binary divider. The counter steps at `2 × clock / counter`, and the 16', 8', 4', 2' squares are bits n, n−1, n−2, n−3 of a free-running binary counter: locked in phase, with shared edges, exactly 50 % duty. The scale is the chip's, up to 1.3 cents off equal temperament. The clock is about 1.06 MHz (A4 on 8' = code 0x2D): measured on a Poly-800, the 2' foot sticks to the 4' bit from 16' = 1 kHz up, which only this clock explains.
- **Groups.** Group 1 (TG0–3) is DCO1, group 2 (TG4–7) is DCO2 with the interval added to the pitch code. Detune lowers group 2's clock (the original thins out CLK2 pulses).
- **Waveform networks.** Per group the four foot buses are summed by the square net (BR5) or the saw net (BR6); `har` moves between them and sets a gain per foot. Weights measured on a Poly-800: square 1 : 1.057 : 1.083 : 1.098, saw 2.47 × (1 : 0.523 : 0.268 : 0.135) — binary within 1 dB, and the saw net's 16' 7.9 dB above the square net's, so both nets peak about alike.
- **Band-limiting.** Each level step of a tone generator is placed with sub-sample accuracy as a Kaiser-windowed sinc impulse (32 taps, 64 phases) and integrated. Smooth changes (gate ramps, `wav`/`har`/`lev`) go in without an impulse. Output delay 16 samples.
- **Output response (`tone 1`).** Fitted to the recording: one-pole high-pass at 54 Hz, two poles at 7.5 kHz (VCF fully open plus output stage). A moving VCF comes later.
- **Envelopes.** Each tone generator has its own DEG (see above). The counters keep running, so a note-on does not restart the phase.

Not modelled: the Poly-800 (Mk I) chorus, the joystick, and the slight clock jitter that the pulse thinning of the detune adds. MG, EQ and the digital delay are measured on a Poly-800 II.

## Build

A test build for macOS (universal arm64 + x86_64, macOS 11+) is in `bin/macos/`, cross-built with `make mac-zig` (zig cc + llvm-lipo). If macOS blocks it, run `xattr -dr com.apple.quarantine` on the folder.

**Bela, on the board itself:**

```sh
git clone https://github.com/maltepeter2000/PDoly800.git pdoly800
scp -r pdoly800 root@bela.local:~/
ssh root@bela.local
cd pdoly800 && make bela PDINCLUDE=/usr/local/include/libpd
cp 'pdoly8~.pd_linux' /root/Bela/projects/pd-externals/
```

Check the path to `m_pd.h` with `find / -name m_pd.h`. Externals load only when the `.pd_linux` file lies flat in `/root/Bela/projects/pd-externals/`.

**macOS:** `make PDINCLUDE=/Applications/Pd-0.55-2.app/Contents/Resources/src`, then put `pdoly8~.pd_darwin` and `pdoly8~-help.pd` next to your patch or in your Pd externals folder.

**Linux:** `make PDINCLUDE=/usr/include/pd`.

**Core test (desktop):** `make test && ./pdoly8_test`; `make demos` renders demo WAVs to `demos/`.

**Help patch:** generated by `tools/gen_help.py` (`make help`) in the layout of the skandrums-pd help files; edit the generator, not the `.pd` file.

## Files

| File | Content |
|---|---|
| `pdoly8~.c` | the Pd external |
| `pdoly8~-help.pd` | help and demo patch (generated by `tools/gen_help.py`) |
| `pdoly8_core.h`, `pdoly8_core.c` | synthesis core, C99, single precision, no allocation, no platform dependencies |
| `pdoly8_test.c` | desktop test of the core and demo renderer |
| `p8cc.pd` | MIDI CC / NRPN → `n <name> <value>` (generated by `tools/gen_p8cc.py`) |
| `p8factory.pd`, `p8user.pd` | factory presets / own presets (read-only lists) |
| `bela/`, `bela-oled/` | the Bela versions (see their READMEs) |

## Tested

- Against a Poly-800 (one key C over five octaves, two octave settings; square and saw, 16' alone and all feet; line out, VCF open, chorus off): over all 40 notes the harmonics up to −45 dB match within 0.37 dB on average (90 % within 0.63 dB), note levels within 0.5 dB. Below 8 kHz the deviation stays under 0.5 dB, at 12–17 kHz about 1.5 dB.

- ROM reference point: code 0x21 gives 440.00 Hz on 8'. A4 on 8' measures 440.00 Hz; DCO2 with interval 7 and detune −20 cents within 0.01 Hz of the expected value.
- VCF: resonance peak at cutoff 50 / resonance 15 at 1079 Hz (recorded 1.05–1.1 kHz); peaks at cutoff 30/50/70 within 0.5 dB of the recording; cutoff curves of 11 settings within 1.75 dB on average (noise source); DEG3 sweep matches the recorded sweep (best at 7.9 octaves); single/multi trigger; noise VCA linear.
- Portamento: starts exactly on the old pitch (−1200 cents for an octave), 5 % left after the set time, ends at 0.
- DEG: attack times (A16, A24, A31 to level 30) within 1 % of the recording, decay 31 at 2.17 dB/s (measured 2.1–2.2), morph loop closes; level curves of 34 recorded notes within 0.82 dB on average.
- Walsh: on all 15 loop points the zero crossings per 16' period equal the sequency; loudness along the loop constant within 0.0001 dB, the loop closes; a Walsh point on C5 is harmonic within −78 dB below 16 kHz; Walsh with PWM, FM and sync stays finite.
- Sync and FM: off = sample-identical; sync and FM spectra harmonic within −75 dB below 16 kHz; index 4 through-zero FM with sync stays finite and below full scale.
- PWM: measured duty equals the set width (10, 25, 75, 90 %) within 0.1 %; the pulse path at 50 % matches the square path within −74.9 dB; narrow pulses alias-free (−85 dB).
- Timbre path: all 15 points exact (±1 step), one foot or the net per step; octave 2 = the feet one counter bit up. (The core still has the earlier paths 0–2 for the tests.)
- Foot path: all 15 knots exact (also ±1 step: each combination is held as a plateau, so pots and the sysex converter hit the original combinations exactly), neighbours differ in exactly one foot, largest gain change per 0.01 `har` step 0.0014 (no jumps).
- Aliasing: C6 square on 2' (4186 Hz) — strongest non-harmonic component −83 dB.
- No clicks in a 42 s morph of `har` and `wav` (10 ms frame analysis).
- Levels: worst case (4 voices, square, all feet, both DCOs) peaks at 0.92 (fixed output level 0.5); one saw note with all feet at 0.36.
- In Pd (libpd 0.56): object loads, notes, voice stealing, controls by name and number, dump and recall round trip, `get`, `print`, `bend`, `reset`, error on unknown messages; the help patch opens without errors.
- Bela with an OLED and MIDI only (no Pepper): `bela-oled/` (see `bela-oled/README.md`): CC and 14-bit NRPN for every setting, the OLED shows the changed setting.
- Bela Pepper: a prebuilt binary and the Pepper patch (10 pages of 8 pots, pickup, CV-Lock, MIDI CC for every page parameter, audio in 1 as FM input) are in `bela/` (see `bela/README.md`).

## Credits

pdoly8~ by Peter with Claude (Anthropic). Modelled on the Korg Poly-800 service manual. The MSM5232 scale ROM values are the chip data as documented in MAME's `msm5232.cpp` (Jarek Burczynski, Hiromitsu Shioya); no MAME code is used.
