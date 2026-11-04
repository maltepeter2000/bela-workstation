# pdoly8~

The tone generation of the Korg Poly-800 for Pure Data: 4 voices, one or two DCOs per voice, MIDI control. Built for the Bela Pepper (Eurorack) and running in any Pd (macOS, Linux, Bela). Envelopes, filter and chorus are not in this version; the level is shaped outside the object.

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
| 0 | `wav1` | 127 | waveform of DCO1: 0 = square (feet added 1:1:1:1), 127 = saw (1:½:¼:⅛, a step wave); continuous in between |
| 1 | `wav2` | 127 | waveform of DCO2 |
| 2 | `har1` | 64 | foot combination of DCO1, continuous path through the 15 original combinations (see below) |
| 3 | `har2` | 64 | foot combination of DCO2 |
| 4 | `intv` | 0 | interval of DCO2, 0–12 semitones |
| 5 | `detn` | 0 | detune of DCO2, 0 to −20 cents; the original's steps 0–3 lie at 0, 42, 85, 127 |
| 6 | `lev1` | 127 | level of DCO1 |
| 7 | `lev2` | 0 | level of DCO2 (DOUBLE only). When `lev1 + lev2` goes above 127 both are scaled down together |

### Foot path (`har`)

On these values the sound is exactly the original foot combination. From one value to the next a single foot fades in or out, so `har` can be modulated without jumps. The lowest foot only ever moves up, and the order keeps the timbre changes small:

| `har` | feet | `har` | feet | `har` | feet |
|---|---|---|---|---|---|
| 0 | 16' | 45 | 16' 2' | 91 | 8' |
| 9 | 16' 8' | 54 | 16' 8' 2' | 100 | 8' 4' |
| 18 | 16' 8' 4' | 64 | all four | 109 | 4' |
| 27 | 16' 4' | 73 | 8' 4' 2' | 118 | 4' 2' |
| 36 | 16' 4' 2' | 82 | 8' 2' | 127 | 2' |

`hsnp` (default 1) holds the exact combination for ±1 step around each value, so a pot or CC lands on it reliably. `wav` and `har` are independent: at the ends of `wav` and on the `har` values every point is an original patch.

### Settings (raw values)

| Message | Range | Default | Function |
|---|---|---|---|
| `mode` | 1/2 | 2 | 1 = WHOLE: one DCO per voice (group 2 is not computed); 2 = DOUBLE: DCO1 on TG n, DCO2 on TG n+4. Both 4 voices |
| `oct1` / `oct2` | −1/0/1 | 0 | octave of DCO1 / DCO2 (Low, Mid, High) |
| `nois` | 0–15 | 0 | white noise, sounds while a voice sounds |
| `sprd` | 0–1 | 0.5 | stereo spread of the 4 voices |
| `levl` | 0–2 | 0.5 | output level |
| `mrph` | ms | 8 | smoothing of `wav`, `har`, `lev` |
| `hold` | 0/1 | 1 | 1: released voices keep sounding until the next phrase (for an external VCA release); 0: off on note-off |
| `a4` | 300–600 Hz | 440 | tuning |
| `tune` | ±50 cents | 0 | master tune |
| `bnd` | 0–12 | 2 | pitch bend range in semitones (original up to ±7) |
| `mgf` | Hz | 5 | vibrato rate |
| `mgi` | 0–100 cents | 0 | vibrato depth, triangle on the master clock |
| `hsnp` | 0–4 | 1 | plateau around each foot combination, in `har` steps |
| `ch1` | 0–16 | 0 | MIDI channel for `p v ch` lists (0 = any) |

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
| `dump` | outlet 3 sends `dump pdoly8` + the 8 controls + the 15 settings |
| `dump pdoly8 …` | restore such a line; lines of other voices are ignored |
| `print` | voices, values and sample rate to the Pd window |

Default dump line: `dump pdoly8 127 127 64 64 0 0 127 0 2 0 0 0 0.5 0.5 8 1 440 0 2 5 0 1 0`.

## How it works

- **Master clock.** One clock for all 8 tone generators. Tune, bend and vibrato act on it, so all voices move together, as on the original.
- **MSM5232 dividers.** A key becomes a pitch code; the chip's 88 × 12 bit ROM gives an integer counter value and a binary divider. The counter steps at `2 × clock / counter`, and the 16', 8', 4', 2' squares are bits n, n−1, n−2, n−3 of a free-running binary counter: locked in phase, with shared edges. The scale is the chip's, up to 1.3 cents off equal temperament. In the top octaves the high feet stick at bit 0, as on the chip.
- **Groups.** Group 1 (TG0–3) is DCO1, group 2 (TG4–7) is DCO2 with the interval added to the pitch code. Detune lowers group 2's clock (the original thins out CLK2 pulses).
- **Waveform networks.** Per group the four foot buses are summed with weights 1:1:1:1 (square, BR5) or 1:½:¼:⅛ (saw, BR6); `wav` moves between them, `har` sets a gain per foot.
- **Band-limiting.** Each level step of a tone generator is placed with sub-sample accuracy as a Kaiser-windowed sinc impulse (32 taps, 64 phases) and integrated. Smooth changes (gate ramps, `wav`/`har`/`lev`) go in without an impulse. Output delay 16 samples. A 5 Hz high-pass stands for the coupling capacitors.
- **Gate.** Without envelopes a key gates its tone generators with a 1.5 ms ramp. The counters keep running, so a note-on does not restart the phase.

Not modelled yet: DEG1/2/3 (on the original they act per tone generator on the chip's CAP pins, before the foot buses), VCF, chorus, and the slight clock jitter that the pulse thinning of the detune adds.

## Build

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

- ROM reference point: code 0x21 gives 440.00 Hz on 8'. A4 on 8' measures 440.00 Hz; DCO2 with interval 7 and detune −20 cents within 0.01 Hz of the expected value.
- Foot path: all 15 knots exact (also ±1 step with `hsnp 1`), neighbours differ in exactly one foot, largest gain change per 0.01 `har` step 0.0014 (no jumps).
- Aliasing: C6 square on 2' (4186 Hz) — strongest non-harmonic component −83 dB.
- No clicks in a 42 s morph of `har` and `wav` (10 ms frame analysis).
- Levels: worst case (4 voices, square, all feet, both DCOs) peaks at 0.92 with `levl 0.5`; one saw note with all feet at 0.36.
- WHOLE: group 2 stays silent even with `lev2` up.
- In Pd (libpd 0.56): object loads, notes, voice stealing, controls by name and number, mode switch, dump and recall round trip, `get`, `print`, `hold`, `bend`, `reset`, error on unknown messages; the help patch opens without errors.
- Not yet built or measured on the Bela.

## Credits

pdoly8~ by Peter with Claude (Anthropic). Modelled on the Korg Poly-800 service manual. The MSM5232 scale ROM values are the chip data as documented in MAME's `msm5232.cpp` (Jarek Burczynski, Hiromitsu Shioya); no MAME code is used.
