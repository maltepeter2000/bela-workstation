# cz4~

Paraphonic 4-voice Casio CZ phase-distortion voice for Pure Data, built for the Bela Pepper (Eurorack) and running in any Pd (macOS, Linux, Bela). Continuous wave morph, two lines per voice with their own wave and DCW, and a switchable vintage layer. Line 2 follows line 1 or plays its own notes from a second MIDI channel. Control is MIDI only; envelopes and VCA live outside the object.

The synthesis core is derived from `firmware/src/cz_native.c` in [keremimo/melodee](https://github.com/keremimo/melodee) (uPD933 model by Devin Acker, MAME), BSD-3-Clause, see `LICENSE` and `LICENSE-BSD-3-uPD933.txt`.

## Object

`[cz4~]` — no creation arguments.

| | Type | Function |
|---|---|---|
| Inlet 1 | messages | notes, controls, settings, presets |
| Outlet 1 | signal | left |
| Outlet 2 | signal | right |
| Outlet 3 | messages | events: `dump cz4 <values>`, `value <name> <v>` |

The messages follow the skandrums-pd conventions (`dump`, `params`, `defaults`, `get`), so `[skpreset]` works with `cz4~` unchanged.

### Controls (0–127, one per pot or MIDI CC)

| # | Name | Default | Function |
|---|---|---|---|
| 0 | `wav1` | 0 | wave of line 1, continuous morph over 11 chip waves |
| 1 | `wav2` | 0 | wave of line 2 |
| 2 | `dcw1` | 6 | DCW of line 1, amount of phase distortion (0 = sine) |
| 3 | `dcw2` | 6 | DCW of line 2 |
| 4 | `detn` | 0 | detune of line 2 upwards, continuous: 0 = unison, 127 = one octave up (about 9.4 cents a step) |
| 5 | `amp1` | 127 | level of line 1 |
| 6 | `amp2` | 0 | level of line 2; with ring: level of the ring product |
| 7 | `mod` | 0 | modulation: 0–42 off, 43–85 ring, 86–127 noise |

Below full scale each amp is a plain level. When `amp1 + amp2` goes above 127, both are scaled down together so the sum stays at full scale: two lines at full level peak no higher than one line alone (as the Cizzle drops its level for chords). Line 2 at level 0 is not computed.

**Ring modulation** (`mod` 43–85): the product of the two lines is normalised by line 1's running level and DC-blocked, so it plays at the level of line 2 alone (within 3 dB over all waves and DCW values; the pulse waves at full DCW, which are nearly silent on their own, stay nearly silent) and `amp2` sets its level directly. Without this the product dropped 3 to 7 dB depending on line 1's wave. A soft saturation keeps its peaks at those of a plain line. At exact unison the product is half DC, which is removed, so it sits 3 dB lower there.

The CZ line modes are covered by these controls: line 1 only = `amp2 0`, line 2 only = `amp1 0`, 1+1' = `wav2` equal to `wav1` plus `detn`/`fine`, 1+2' = two different waves.

Send them by name (`wav1 64`) or by number (`ctl 0 64`, handy for MIDI CC).

### Wave morph

`wav1`/`wav2` 0–127 cover 11 positions, ordered so that neighbours sound close: 0 saw, 1 saw-pulse, 2 square, 3 pulse, 4 resonance I (saw window), 5 resonance II (triangle), 6 resonance III (trapezoid), 7 resonance with pulse window, 8 resonance with double-saw window, 9 double sine, 10 double pulse.

- **Phase morph (`pmor 1`, default):** between two positions the bent phase and the window are interpolated, so every point is one single wave. Where the neighbours differ in cycle count (pulse → resonance, resonance → double sine) the outputs are crossfaded instead, equal-power where they share no harmonics. Level at every midpoint stays within −1.6 to +0.8 dB (the plain crossfade dipped up to −4 dB). `pmor 0` gives the plain CZ-style crossfade.
- **Sine at DCW 0 (`sine 1`, default):** every position starts from the fundamental sine; the resonance windows and the octave of the double waves fade in over the first quarter of the DCW range. `sine 0` keeps the chip's own behaviour (double waves an octave up, resonance windows audible at DCW 0).

### Settings (raw values)

| Message | Range | Default | Function |
|---|---|---|---|
| `pai1` / `pai2` | 0–10, −1 = off | −1 | second wave of a CZ wave pair, alternating every cycle |
| `octv` | −1/0/1 | 0 | octave |
| `sprd` | 0–1 | 0.5 | stereo spread of the 4 voices |
| `levl` | 0–2 | 0.5 | output level |
| `drft` | 0–1 | 0 | slow pitch drift per voice, up to ±8 cents |
| `mrph` | ms | 8 | smoothing of wave changes |
| `hold` | 0/1 | 1 | 1: released voices keep sounding until the next phrase (for an external VCA release); 0: fade out on note-off |
| `a4` | 300–600 Hz | 440 | tuning |
| `dvel` | 0–127 | 40 | how much note velocity adds to the DCW of both lines |
| `trk` | 1/0 | 1 | 1: line 2 follows line 1; 0: line 2 plays its own notes (second allocator, like a second oscillator) |
| `ch1` | 0–16 | 0 | MIDI channel of line 1 for `p v ch` lists (0 = any) |
| `ch2` | 1–16 | 2 | MIDI channel of line 2 for `p v ch` lists when `trk 0` |
| `era` | 0–127 | 0 | vintage layer, clean to crunchy (see below) |
| `fine` | 0–127 | 64 | fine detune of line 2: 64 = none, about 0.8 cent a step, ±50 cents; adds to `detn` |
| `pmor` | 0/1 | 1 | 1: phase morph between waves; 0: CZ-style crossfade |
| `sine` | 0/1 | 1 | 1: DCW 0 is the plain sine on every wave; 0: chip behaviour |

### Other messages

| Message | Function |
|---|---|
| `note p v`, or the list `p v` | note on (v > 0) / off for line 1 (and line 2 with `trk 1`); 4 voices, the oldest is stolen; a note-on resets its voice's phase as on the CZ |
| list `p v ch` | from `[notein]` → `[pack f f f]`: channel `ch2` goes to line 2 when `trk 0`, channel `ch1` (or any) to line 1 |
| `note2 p v` | note for line 2 directly (only with `trk 0`) |
| `params v0 v1 …` | set the controls by index |
| `defaults` / `reset` | default controls and settings; `reset` also stops all voices |
| `panic` | all voices off |
| `phase` | phase reset of all voices |
| `get <name>` | outlet 3 answers `value <name> <v>` (controls and settings) |
| `voice i p` / `voiceoff i` | play a voice directly, for your own allocation |
| `dump` | outlet 3 sends `dump cz4` + the 8 controls + the 17 settings, in the order of the tables above |
| `dump cz4 …` | restore such a line; lines of other voices are ignored |
| `tone b0 … b127` | waves, line levels, modulation, octave and detune from a decoded native CZ-1 tone (detune clamped to 0…+12 semitones); envelopes are ignored |
| `quant` `chiprate` `bits` `dacerr` `hiss` `lpf` `drive` | single vintage stages instead of `era` |
| `print` | voices, values and sample rate to the Pd window |

Default dump line: `dump cz4 0 0 6 6 0 127 0 0 -1 -1 0 0.5 0.5 0 8 1 440 40 1 0 2 1 1 0 64`. Wiring with `[skpreset name]`: outlet 3 → skpreset inlet 0, skpreset outlet 0 → `cz4~` inlet; a slot number 0–15 on skpreset inlet 1 recalls and keeps the values per step.

## Vintage layer (`era`, setting 0–127)

| Stage | Effect | Fades in |
|---|---|---|
| Chip quantisation | 11-bit phase, integer distortion slopes, 12-bit cosine table and window product | 0–0.2 |
| Chip rate | oscillators and DAC sample-and-hold, 44.1 → 16 kHz, no anti-aliasing | from 0.2 |
| DAC | 16 → 8 bit, R-2R bit-weight mismatch, hiss −96 → −66 dBFS | from 0.3 (hiss from 0.05) |
| Output stage | 12 dB low-pass 18 → 5 kHz, soft saturation | from 0.05 (saturation from 0.3) |

## Build

**Bela, on the board itself:**

```sh
git clone https://github.com/maltepeter2000/CZ4.git cz4
scp -r cz4 root@bela.local:~/
ssh root@bela.local
cd cz4 && make bela PDINCLUDE=/usr/local/include/libpd
cp 'cz4~.pd_linux' /root/Bela/projects/pd-externals/
```

Check the path to `m_pd.h` with `find / -name m_pd.h`. Externals load only when the `.pd_linux` file lies flat in `/root/Bela/projects/pd-externals/`, without a subfolder.

**macOS:** `make PDINCLUDE=/Applications/Pd-0.55-2.app/Contents/Resources/src`, then put `cz4~.pd_darwin` and `cz4~-help.pd` next to your patch or in your Pd externals folder.

**Linux:** `make PDINCLUDE=/usr/include/pd`.

**Core test (desktop):** `make test && ./cz_test`; `make demos` renders demo WAVs to `demos/`.

**Help patch:** generated by `tools/gen_help.py` (`make help`) in the layout of the skandrums-pd help files; edit the generator, not the `.pd` file.

## Files

| File | Content |
|---|---|
| `cz4~.c` | the Pd external |
| `cz4~-help.pd` | help and demo patch, skandrums-pd layout (generated by `tools/gen_help.py`) |
| `cz_core.h`, `cz_core.c` | synthesis core, C99, no allocation, no platform dependencies |
| `cz_test.c` | desktop test of the core |

## Tested

- Clean wave function sample-identical to melodee (35.6 million comparisons, max difference 0).
- Wave morph continuous: output step across every integer position below 0.001.
- Morph quality: with `sine 1` DCW 0 gives a pure sine on all 11 positions (overtones below 1 %); midpoint level within ±2 dB on every pair.
- With `pmor 0` and `sine 0`, single positions are still sample-identical to melodee.
- DCW per line: line 1's DCW changes nothing when only line 2 is heard; a split line 2 stays silent until it gets its own note.
- In Pd (libpd 0.56): object loads, notes, voice stealing, controls by name and number, clamping, dump and recall round trip, velocity and `dcw2` on DCW, channel routing with `trk 0`, `hold`, `params`, `get`, `defaults`, foreign dump lines ignored, error on unknown messages; the help patch opens without errors.
- Levels: worst-case peak over all waves and DCW values with 4 loud voices at `levl 0.5` is 0.98 for one line and also 0.98 for both lines at full level. `fine` covers −49.5 to +48.9 cents; `detn` 127 gives +12 semitones, 74 gives +6.99 semitones (+7 with `fine` 65).
- With the real `skpreset.pd` from skandrums-pd: slot store and recall round trip, empty slots keep the current sound.
- Not yet built or measured on the Bela.

## Credits

cz4~ by Peter with Claude (Anthropic). CZ engine derived from melodee by Kerem Kilic and the uPD933 model by Devin Acker (MAME), BSD-3-Clause.
