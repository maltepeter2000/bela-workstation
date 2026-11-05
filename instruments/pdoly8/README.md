# pdoly8~

The tone generation of the Korg Poly-800 for Pure Data: 4 voices, one or two DCOs per voice, MIDI control. Built for the Bela Pepper (Eurorack) and running in any Pd (macOS, Linux, Bela). Envelopes (DEG1/2/3) and the VCF are built in, all measured on a Poly-800; chorus is not in this version.

Modelled on the Poly-800 service manual (circuits 19, 22, 27, 28, 31, 32): one master clock, the OKI MSM5232 with its integer divider ROM and binary foot counters, two tone generator groups, the square and saw resistor networks, and LEVEL1/LEVEL2 of the NJM2069. Every edge is band-limited, so the output has the analog edges of the original and no digital aliasing.

## Object

`[pdoly8~]` — no creation arguments.

| | Type | Function |
|---|---|---|
| Inlet 1 | messages | notes, controls, settings, presets |
| Outlet 1 | signal | left |
| Outlet 2 | signal | right |
| Outlet 3 | messages | events: `dump pdoly8 <values>`, `value <name> <v>` |

The messages follow the cz4~ / skandrums-pd conventions (`dump`, `params`, `defaults`, `get`), so `[skpreset]` works unchanged.

### Controls (0–127, one per pot or MIDI CC)

| # | Name | Default | Function |
|---|---|---|---|
| 0 | `har1` | 45 | timbre of DCO1: one knob through all 15 distinct original timbres, dark to bright (see One-knob timbre); 45 = saw net, all feet |
| 1 | `har2` | 45 | timbre of DCO2 |
| 2 | `env1` | 0 | envelope of DCO1 (DEG1), one knob: 0 gate, from 1 very short pluck … pluck … piano … long release … swell … pad … long pad (see Envelopes); −1 = own values `a1` … `r1` |
| 3 | `env2` | 0 | envelope of DCO2 (DEG2), the same |
| 4 | `intv` | 0 | interval of DCO2, 0–12 semitones |
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

| Message | Range | Default | Function |
|---|---|---|---|
| `oct1` / `oct2` | −1…4 | 0 | octave of DCO1 / DCO2: −1/0/1 = Low, Mid, High; 2–4 = the feet one, two, three counter bits higher (the original combinations without 16') |
| `nois` | 0–15 | 0 | white noise, sounds while a voice sounds |
| `sprd` | 0–127 | 0 | stereo spread of the 4 voices, continuous |
| `mrph` | ms | 8 | smoothing of `har`, `lev` |
| `bnd` | 0–12 | 2 | pitch bend range in semitones (original up to ±7) |
| `mgf` | Hz | 5 | MG rate (also set by `mgr`) |
| `mgi` | 0–200 cents | 0 | vibrato depth (triangle on the master clock, through a measured 14.5 Hz one-pole low-pass, so fast vibrato is shallower as on the original). Original 83: 12.4 cents per step, 186 at 15 |
| `mgr` | 0–15, −1 | −1 | MG frequency in original steps (81, measured: 0 = 0.29 Hz, 3 = 1.26, 6 = 3.39, 8 = 5.22, 9 = 6.49, 12 = 11.1, 15 = 19.1 Hz); sets `mgf`; −1 = use `mgf` |
| `hsnp` | 0–4 | 1 | plateau around each foot combination, in `har` steps |
| `ch1` | 0–16 | 0 | MIDI channel for `p v ch` lists (0 = any) |
| `pw1` / `pw2` | 0–127 | 64 | pulse width of DCO1 / DCO2: 3–97 %, 64 = square; acts on every foot (see PWM) |
| `sync` | 0/1 | 0 | 1: DCO2 restarts at every 16' period of DCO1 (hard sync) |
| `swp2` | 0–48 | 0 | DCO2 pitch offset in semitones, continuous (sync sweeps; also without sync) |
| `fm` | 0–127 | 0 | linear FM index of DCO1, 0–8 (quadratic; index 1 at about 45, through zero above) |
| `fmr` | 0.125–16 | 1 | ratio of the sine modulator to DCO1's 16' frequency |
| `fms` | 0/1 | 0 | FM source: 0 internal sine, 1 DCO2 (cross FM) |
| `wsh1` / `wsh2` | 0–127 | 0 | Walsh loop position of DCO1 / DCO2 (see Walsh morph) |
| `wmx1` / `wmx2` | 0–127 | 0 | mix from the feet sum (0) to the Walsh loop (127) |
| `leak` | 0/1 | 0 | 1: released voices stay at −55 dB, as measured on the original |
| `a1 d1 bp1 sl1 su1 r1` | 0–31 | 0 0 31 0 31 0 | DEG1 attack, decay, break point, slope, sustain, release; setting one makes `env1` −1 |
| `a2 d2 bp2 sl2 su2 r2` | 0–31 | 0 0 31 0 31 0 | the same for DEG2 |
| `port` | 0–127 | 0 | portamento, off … 3 s (quadratic; not on the Poly-800) |
| `pmod` | 0/1/2 | 0 | glide from: 0 the last note played, 1 the voice's own last note, 2 the last note only while another key is held (legato) |
| `cut` | 0–99 | 99 | VCF cutoff (66 Hz … 20 kHz, measured) |
| `res` | 0–15 | 0 | VCF resonance (ladder feedback k 0 … 3.9, no self-oscillation) |
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
| `dtim` | 0–99 | 30 | digital delay time (71): 4 … 1024 ms (curve provisional: exponential) |
| `dfb` | 0–15 | 0 | delay feedback (72): 0 … 100 % |
| `dmf` `dmi` | 0–31 | 0 | delay modulation rate (73, 0.1 … 10 Hz) and depth (74, up to a 1:5 swing of the time) |
| `dlev` | 0–15 | 0 | delay effect level (75); 0 = off, 15 = as loud as the direct sound. The echo goes to the left output in phase and to the right one inverted (measured: L = dry + echo, R = dry − echo, so a mono sum of both cancels it), band-limited to 7 kHz; changing a delay value mutes the echo briefly, as on the original |

### Other messages

| Message | Function |
|---|---|
| `note p v`, or the list `p v` | note on (v > 0) / off; 4 voices, the oldest is stolen |
| list `p v ch` | from `[notein]` → `[pack f f f]`, filtered by `ch1` |
| `bend f` | pitch bend −1…1 of all voices (on the master clock), times `bnd` |
| `params v0 v1 …` | set the controls by index |
| `defaults` / `reset` | default values; `reset` also stops all voices |
| `panic` | all voices off |
| `get <name>` | outlet 3 answers `value <name> <v>` |
| `voice i p` / `voiceoff i` | play a voice directly |
| `dump` | outlet 3 sends `dump pdoly8` + the 8 controls + the 60 settings |
| `dump pdoly8 …` | restore such a line; lines of other voices are ignored |
| `print` | voices, values and sample rate to the Pd window |

Default dump line: `dump pdoly8 127 127 64 64 0 0 127 0 2 0 0 0 0.5 0.5 8 0 440 0 2 5 0 1 0 1 0 0 1 64 64 0 0 0 1 0 0 0 0 0 0 0 0 0 0 31 0 31 0 0 0 31 0 31 0 0 0 99 0 0 2 0 2 0 0 0 31 0 31 0`.

### PWM (`pw1`, `pw2`)

Each foot becomes the AND of its square with a copy shifted by part of its period, a phase offset on the counter: the falling edge stays on the counter step, the rising edge moves, width 3–97 % of each foot's own period. With one foot this is classic PWM; with several feet the pulses stay locked, and on the saw net they form pulse staircases. The pulses are DC free, every edge is band-limited (a 10 % pulse on a 4.2 kHz 2' shows no component above −85 dB), and the width is smoothed like the other controls. At 64 the output is the plain square (the pulse path differs by less than −74 dB).

### Sync and FM

- **Hard sync (`sync 1`):** DCO2's counter restarts at every 16' period of DCO1, at the exact sub-sample time, with a band-limited step. Sweep DCO2 with `swp2` (continuous, 0–48 semitones) or `intv`/`oct2`; `lev1 0` lets DCO2 be heard alone. Feet, nets and PWM of DCO2 all go through the sync.
- **Linear FM (`fm`, `fmr`, `fms`):** DCO1's counter rate is modulated by a sine at `fmr` times its 16' frequency, or by DCO2 (cross FM). Above index 1 the counter runs backwards for a moment (through-zero FM), so integer ratios stay harmonic; the edges of every foot are found in both directions and stay band-limited.
- Measured: sync (DCO2 at +7.3 semitones) and FM (ratio 1, index 1 and 3) keep every component below 16 kHz harmonic within −75 dB; deep through-zero FM with sync on 4 low voices peaks at 0.72. With `fm 0` and `sync 0` the output is sample-identical to before.

### Walsh morph (`wsh`, `wmx`)

The four feet are Rademacher functions. Their products (the XOR of foot squares) are the 15 Walsh functions: two-level waves on the same counter edges, with their energy around harmonic 1 … 8 of the 16' (the sequency). They come in pairs of equal sequency; `wsh` moves along a closed loop that rises through one of each pair and falls through the other, so neighbours lie one harmonic apart and 127 joins 0. Neighbours fade at equal power (Walsh functions are orthonormal), at the loudness of the square net with all feet, so the level stays constant. Besides the octave squares this reaches energy at harmonics 3, 5, 6 and 7 — formant-like timbres that adding feet cannot make.

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

Not on the Poly-800, added here. On a new note the counters jump to the new divider at once, as on the chip; a pitch offset from the old note fades out exponentially on top of the clock (about 95 % after the set time), so every glide ends exactly on the chip's own pitch. The source is the last note played (`pmod 0`, good for lines over the 4 voices), the voice's own last note (`pmod 1`, chords glide voice by voice), or the last note only while another key is held (`pmod 2`, legato).

### VCF and DEG3

One VCF for the whole instrument, as on the Poly-800 (NJM2069), measured on a real one with noise through the filter (`docs/vcf-calibration.md`):

- **Cutoff:** a 4-pole low-pass, 0–99 = 66, 128, 218, 364, 617, 1083, 1916, 3492, 6384, 11774 Hz at 0, 10 … 90 and about 20 kHz at 99 — about 13 steps per octave. The top value and an output pole at 5 kHz come from the first (open-filter) recording, with which the feet calibration now matches within 0.32 dB.
- **Resonance:** an uncompensated ladder: feedback k = 0, 0.53, 1.84, 3.07, 3.9 at 0, 4, 8, 12, 15, the bass falls by 1/(1+k) (−13.7 dB at 15), the cutoff does not move, no self-oscillation (k held below 4). Zero-delay-feedback ladder, solved exactly per sample.
- **DEG3:** one envelope for the instrument (paraphonic, as the original), same stages, ranges and timing as DEG1/2. Its level moves the cutoff linearly in octaves: EG INT 15 at full level = +7.9 octaves (cutoff 0 opens to about 15.7 kHz), EG INT 8 about half; polarity 1 inverts. It also sets the noise level, linearly (DEG3 sustain 10 ≈ −10 dB). Single trigger: only the first key of a phrase starts DEG3; multi: every key.
- **Noise:** level 0–15 linear in amplitude (12 = −1.8 dB, 8 = −5.3, 4 = −11.5, 1 = −24.7 dB, measured), through the VCF.
- **Keyboard tracking:** full follows the highest key 1:1 (the resonance peak stays on the same harmonic over three octaves), C4 = nominal cutoff; half by 1/2 with the nominal cutoff at the top key C6 (never brighter than the setting), both measured.

The filter sweep of DEG3 against the recording: `demos/vcf_deg3_vs_poly800.png`.

## Presets

As in skandrums-pd, presets are lines of text: a `[text define -k name]` in your patch holds one line per slot, `pdoly8 v0 v1 …` (the values of a `dump`, controls then settings), and is saved with the patch. `[skpreset name]` (included, from skandrums-pd, CC0) steps through it:

    [pdoly8~] outlet 2  ->  [skpreset name] inlet 0
    [skpreset name] outlet 0  ->  inlet [pdoly8~]
    slot number 0-95  ->  [skpreset name] inlet 1

A new slot number first stores the current sound in the old slot, then loads the new one; an empty line (`-`) keeps the current sound. This copy has 96 slots and does not store on the very first selection, so a bank read into the text is not overwritten in slot 0. `write file` / `read file` on `[text define]` save and load banks.

**Poly-800 II factory bank:** `presets/poly800mk2-factory.txt` keeps slots 0–31 free for your own sounds and holds the 64 factory programs in slots 32–95 (slot 32 = program 11, 39 = 18, 40 = 21, … 95 = 88). The help patch reads it with `[read presets/poly800mk2-factory.txt(`.

**Sysex converter:** `tools/p8sysex.py` converts a Poly-800 II data dump (`F0 42 21 0C`, all 64 programs) into such a bank and back:

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

Not modelled: the Poly-800 (Mk I) chorus, the joystick, and the slight clock jitter that the pulse thinning of the detune adds. MG and EQ are measured on a Poly-800 II; the delay curves are taken from the service manual and still to be calibrated against recordings.

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
- Foot path: all 15 knots exact (also ±1 step with `hsnp 1`), neighbours differ in exactly one foot, largest gain change per 0.01 `har` step 0.0014 (no jumps).
- Aliasing: C6 square on 2' (4186 Hz) — strongest non-harmonic component −83 dB.
- No clicks in a 42 s morph of `har` and `wav` (10 ms frame analysis).
- Levels: worst case (4 voices, square, all feet, both DCOs) peaks at 0.92 (fixed output level 0.5); one saw note with all feet at 0.36.
- In Pd (libpd 0.56): object loads, notes, voice stealing, controls by name and number, dump and recall round trip, `get`, `print`, `bend`, `reset`, error on unknown messages; the help patch opens without errors.
- Not yet built or measured on the Bela.

## Credits

pdoly8~ by Peter with Claude (Anthropic). Modelled on the Korg Poly-800 service manual. The MSM5232 scale ROM values are the chip data as documented in MAME's `msm5232.cpp` (Jarek Burczynski, Hiromitsu Shioya); no MAME code is used.
