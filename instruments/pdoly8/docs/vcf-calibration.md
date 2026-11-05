# VCF / DEG3 calibration — status 2026-11-05 (evening)

Measurements on Peter's Poly-800 Mk II (line out, 48 kHz). F1–F5 are **built into pdoly8~** (see README, VCF and DEG3). Still open: F3b half tracking (both runs came out identical, i.e. full), F6 (single/multi trigger, implemented after the manual) and F7 (saturation).

## Done and analysed

### F1 — cutoff (noise source, resonance 0)

A true 4-pole low-pass (fits with 1.2 dB rms; 3 or 5 poles fit worse). Corner frequency per cutoff value (4 equal real poles):

| Cutoff | 0 | 10 | 20 | 30 | 40 | 50 | 60 | 70 | 80 | 90 | 99 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| fc (Hz) | 66 | 128 | 217 | 363 | 611 | 1059 | 1817 | 3106 | 5192 | 8754 | 12811 |

About 13 steps per octave (log2 fc ≈ 0.0767·C + 6.18); use the table with log interpolation (ends deviate up to 0.15 oct from the line).

Output stage behind the VCF: with the VCF at 12.8 kHz, the earlier open-filter calibration (2 poles at 7.5 kHz as a whole) is matched by VCF + one extra pole at about 10 kHz, plus the 54 Hz high-pass. Replace the current 2-pole 7.5 kHz stage with this when the VCF goes in.

### F2 — resonance (noise source)

Fitted ladder: H = (1+k)/((1+s/ωc)^4 + k) · 1/(1+k), i.e. an uncompensated ladder (bass loss exactly 1/(1+k)), rms about 2.5 dB.

| Resonance | 0 | 4 | 8 | 12 | 15 |
|---|---|---|---|---|---|
| k | 0.12 | 0.53 | 1.84 | 3.07 | 3.88 (→ ~3.95 at cutoff 30 / 70) |
| bass loss | −0.3 dB | −3.6 dB | −8.9 dB | −12.4 dB | −13.7 dB |

- Cutoff does not move with resonance (1031–1059 Hz at cutoff 50).
- No self-oscillation at resonance 15 (F2.3: silent). Clamp k at 3.95.
- No measurable dependence of k on cutoff.

### F3 — keyboard tracking (Full)

A chromatic/scale run over three octaves with a DCO: the resonance peak stays on the same harmonic (4th of 16') for every note → Full tracking is exactly 1 octave per octave of the highest note. Reference key (where cutoff = nominal) not yet known: at the lowest note played the peak was about one octave below nominal. Half tracking not yet recorded.

### DEG3

Same 6 stages and ranges as DEG1/2 (manual), same CPU routine → reuse the measured DEG timing tables. Differences: one global DEG3 (paraphonic VCF and noise), single/multi trigger applies only here, target is the VCF cutoff (via EG INT 0–15, polarity) and the noise VCA — mappings still to be measured.

## F3b, F4, F5 (analysed and built in)

- **F3b:** cutoff 40, resonance 8, keys C2–C5: resonance peak about 290, 540, 1050 Hz for C3, C4, C5 → full tracking 1:1, nominal cutoff (617 Hz, peak ~0.9·fc) at C4. The second run (meant as half) came out identical — half tracking still unconfirmed; modelled as 1/2.
- **F4:** fitted cutoff per 0.1 s frame (ladder fit to the noise spectrum). Attack (A24): cutoff rises linearly in octaves with the DEG level, about 0.25 octave per level step; plateau at EG INT 15 = 7.9 octaves above cutoff 0 (≈ 15.7 kHz); EG INT 8 about half (0.54×). Release (R24): matches an exponential fall of the DEG level (amplitude law (L/31)² → level falls at half the dB rate), the same as DEG1/2. Polarity 1 mirrors it. Model vs recording: best fit at 7.9 octaves.
- **F5:** noise level linear in amplitude (12: −1.8, 8: −5.3, 4: −11.5, 1: −24.7 dB). The DEG3 note (recorded with A16 D20 BP10 SU10 R16) shows the noise VCA linear in the DEG level: sustain 10 → −10 dB, release about 35 dB/s for R16 (DEG1 amplitude 64 dB/s → half).

## Corrections after the build

- The top of the cutoff table: the first recording (open filter, 40 notes) fits much better with 4 poles at about 20 kHz plus one output pole at 5 kHz (0.32 dB) than with 12.8 kHz (0.87 dB); F1 is nearly indifferent to the top value (1.23 vs 1.31 dB), so the table was refitted with 20 kHz at 99: 66, 128, 218, 364, 617, 1083, 1916, 3492, 6384, 11774, 20000 Hz.
- Resonance 0: k = 0 (the fit gave 0.12, within its noise).

## Recordings of 2026-11-05 afternoon (EQ 76/77 = 0)

- **F6 confirmed:** single trigger: C3 + E3 + G3 legato gives one DEG3 sweep only; after all keys are released the next key restarts it. Multi: every new key restarts DEG3 from its current level (no drop to 0). Matches the implementation.
- **F3b:** OFF constant (peak ~480 Hz); FULL C2..C6 peaks 153, 261, 466, 905, 1836 Hz — superseded by Audio_0049 below (the key names of the noise runs were one octave too high).
- **F7 not usable:** the peak sits at ~14 kHz for every note — cutoff apparently still 99 (resonance 15 at the top of the range).
- **F7:** level 31 (Audio_32) and level 8 (Audio_0048 16:32, 23 dB quieter as the (L/31)² law predicts) give the same filter shape: ladder fit fc 1825–1880 Hz (table: 1916), k 3.84–3.95 (table 3.90), rms 2.4–2.7 dB at C2–C4, the same for both levels. No measurable saturation → VCF stays linear. (The note-dependent "bass loss" seen first came from the open reference take, not from drive.) After key-off a ~1.82 kHz ring at about −47 dB lasts ~0.8 s (leak through the resonant filter).
- **Tracking, final (Audio_0049: saw all feet, cutoff 40, resonance 15, octave 2, all five C keys C2–C6; passes 0, 0, 1, 2).** Note: with the 16' foot the lowest audible line is an octave below the key (key C2 → 32.3 Hz); the keys are identified by that. Resonance peak in Hz, Poly-800 / model:

  | KBD | C2 | C3 | C4 | C5 | C6 |
  |---|---|---|---|---|---|
  | 0 | 610 / 617 | 612 / 596 | 615 / 614 | 454* / 457 | 516* / 523 |
  | 1 | 158 / 154 | 224 / 188 | 238 / 228 | 394 / 382 | 516* / 523 |
  | 2 | 158 / 154 | 313 / 307 | 616 / 614 | 1177 / 1227 | 2099* / 2454 |

  (* few harmonics near the peak, coarse.) Full: 1 oct/oct, nominal at key C4 (note 60). Half: 0.5 oct/oct, nominal at the top key C6 (note 84) — half never makes it brighter than the setting. Agrees with the noise run (Audio_27) and Audio_0048. Built in.
- **Correction:** in the F7 and open-filter takes the first (short) note is key C2; earlier comparisons had labelled key C3 as C2. Redone with the correct keys: open filter (Audio_0044, saw all feet, EQ 0) vs model, keys C2–C6 up to 20 kHz: mean 0.56 dB, 90 % within 1.16 dB. The F7 conclusions (no saturation, fc/k) are ratios within the Poly-800 takes and stand.
- **EQ:** the earlier recordings (feet/open filter, F1–F5) may have had treble/bass EQ on. F1/F2/F4 are spectral ratios against an open-filter take of the same session, so EQ cancels there; the output stage (20 kHz top + 5 kHz pole) was checked against an EQ-0 open-filter take: 0.56 dB mean, no tilt — it stands.

## Still to record (Mk II parameter numbers)

Base setup: 18 DCO = 1 (WHOLE); 11 octave = 2; 12 waveform = 1; 13/14/15/16 = 0/1/0/0 (only 8'); 17 level = 0; 37 noise = 15; 31 cutoff = 99, 32 resonance = 0, 33 KBD track = 0, 34 polarity = 2, 35 EG INT = 0, 36 trigger = 2; DEG1 41–46 = 0 0 31 0 31 0; DEG3 61–66 = 0 0 31 0 31 0; 75 delay effect level = 0; 76 treble = 0, 77 bass = 0; 83 MG DCO = 0, 84 MG VCF = 0; joystick centred.

- **F3b again** — 31 = 40, 32 = 8; keys C2, C3, C4, C5, 2 s each, three passes in one take: 33 = 0, 33 = 1, 33 = 2 (the off and full passes are the reference for the half pass).
- **F6** — 37 = 0, 17 = 31; 31 = 20, 32 = 8, 35 = 15; DEG3 61–66 = 16 20 10 0 10 16. 36 = 1: C3, add E3, add G3 (hold all), release all; then C3, E3, G3 staccato. Then 36 = 2: the same.
- **F7** — 37 = 0, 17 = 31, 12 = 2, 13–16 = 1; 31 = 60, 32 = 15; keys C2, C3, C4, 2 s each. Repeat with 17 = 8 (level dependence shows saturation).

## Implementation (built)

- VCF: zero-delay-feedback 4-pole ladder per output channel, feedback solved exactly; fc from the table (log interpolation) × keyboard tracking (highest key; full nominal at C4, half at C6) × 2^(7.9 oct · EG INT/15 · DEG3 level/31); k from the resonance table (held below 4). No saturation yet (F7).
- DEG3: one instance of the DEG code; single trigger = only the first key of a phrase, multi = every key; drives cutoff and noise VCA (both linear in the DEG level).
- Output: 54 Hz high-pass + VCF + one pole at 5 kHz (replaces the 2-pole 7.5 kHz stage).
- Not yet: MG to VCF (param 84).

## MG (Audio_34, 2026-11-05)

Square 8', key C3, line out. Pitch tracked on the 5th harmonic (Hilbert, 7.75 ms smoothing).

- **Rate (81)**, depth 83 = 15: 0 → 0.291 Hz, 3 → 1.262, 6 → 3.39, 8 → 5.22, 9 → 6.493, 12 → 11.09, 15 → 19.08 Hz. Waveform triangle (3rd harmonic −21 dB at slow rates).
- **Depth falls with rate:** ±186, 182, 169, 149, 122, 87 cents at the rates above; the 3rd harmonic of the vibrato drops too → a low-pass on the pitch CV: one pole at 14.5 Hz fits within 1 %. Model after the build: ±179, 169, 150, 124, 89 cents.
- **Depth (83)** at 5.2 Hz: 15 → ±157, 8 → ±89, 4 → ±46, 1 → ±11 cents → linear, 12.4 cents per step at DC (186 at 15).
- **Delay (82):** the vibrato starts abruptly at full depth after 0.47 s (5), 0.95 s (10); 1.55 s (15) in the three M4 notes (the M3 note at 15 came in at about 1.2–1.3 s, unclear). Legato part not recorded.
- **MG → VCF (84)**, saw all feet, cutoff 40, resonance 15: the resonance peak swings ±2.1 octaves at 4, about ±4.2 at 8 → 0.53 octaves per step, ±7.9 at 15 (the same scale as EG INT). The sweep is a sharp triangle (no low-pass on the VCF path).

## EQ (Audio_36)

Noise through the EQ, steady part of each note, spectra relative to the setting 0 (bass takes normalised at 5–16 kHz, treble takes at 60–200 Hz). The bass run has six notes (0, −6, −3, 0, +3, +6), the treble run five (−6, −3, 0, +3, +6).

- Treble +6: +1.4 (500 Hz), +2.8 (1 kHz), +4.5 (2 kHz), +8.3 (5 kHz), +9.9 (12 kHz) dB; −6 mirrored (−9.8 at 16 kHz).
- Bass +6: +9.8 (60 Hz), +7.3 (100), +4.7 (200), +3.2 (300), +0.6 (500) dB; −6: −11.8 at 40 Hz.
- Joint fit, first-order shelves with fixed corners: treble 4256 Hz, bass 115 Hz (full boost reads +3 dB at 1.8 kHz / 316 Hz — the manual's "±3 dB point 2 kHz / 300 Hz"). Gain per step: treble −9.0 / −3.9 / +2.0 / +8.8, bass −11.7 / −6.3 / +4.1 / +9.8 dB at −6 / −3 / +3 / +6 → modelled as 1.10·k + 0.089·k² dB (4.1 at 3, 9.8 at 6). Model within about 1 dB.

## Digital delay, first take (Audio_4, 17:19, stereo L/MONO + R)

- Only two of the plucks carry an echo (at 10.7 s: about 43 ms; at 103.8 s: about 30 ms); in all others L and R are identical (no echo). Which settings belong to which note is still to be clarified.
- Where there is an echo, sum and difference separate it cleanly: (L+R)/2 is the dry pluck alone, (L−R)/2 the echo alone, at about the level of the dry sound (effect level 15). → L = dry + echo, R = dry − echo. Built in.
- After each pluck both outputs carry a noise floor about 10 dB above the idle floor for roughly 1.3 s, then it drops (a gate on the delay path, see the service manual's "gate on / off" noise figures). Not modelled.

## Digital delay (Audio_0023, mono, feedback 15, time 99 … 0)

- Time 99: echoes every 1.06–1.11 s (mean 1.08 s; manual 1024 ms). Time 70: echo period 216 ms. Both fit the exponential curve 4 · 256^(v/99) ms, 6 % longer → built in as 4.24 · 256^(v/99).
- Feedback 15: −3.1 dB per echo (loop gain 0.70, not 100 %); the first echo is 2.6 dB under the note. After about 13 echoes, ~37 dB under the note, the train stops abruptly (gate) → modelled as an echo gate.
- Times 40 … 0: no audible echo on Peter's unit; an amplitude ripple of about 100 ms (≈ 10 Hz) on every note, independent of the time. Unclear (unit fault, gate, or modulation); not modelled.

## Delay modulation (Audio_0024: time 40, feedback 15, depth 31, rate 31 / 21 / 11 / 1)

Echo pitch against the dry note (≈ 2057 Hz), per 10 ms frame:
- Rate 21: the echo pitch swings −523 … +472 cents (2nd/98th percentile, several echoes) with 1.15 Hz; rate 11: 0.26 Hz; rate 1: too slow for the take; rate 31: chaotic.
- A swing of ±3 ms (the first estimate) would give about ±25 cents; the manual's 1:5 time swing gives ±240 cents for the first echo and more for the recirculated ones. Model with 1:5 and rate 0.05 · 100^(v/31) Hz: −603 … +454 cents at 1.13 Hz. Built in (the rate is half the manual's 0.1–10 Hz).
