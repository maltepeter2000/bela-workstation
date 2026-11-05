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

## Still to record (Mk II parameter numbers)

Base setup: 18 DCO = 1 (WHOLE); 11 octave = 2; 12 waveform = 1; 13/14/15/16 = 0/1/0/0 (only 8'); 17 level = 0; 37 noise = 15; 31 cutoff = 99, 32 resonance = 0, 33 KBD track = 0, 34 polarity = 2, 35 EG INT = 0, 36 trigger = 2; DEG1 41–46 = 0 0 31 0 31 0; DEG3 61–66 = 0 0 31 0 31 0; 75 delay effect level = 0; 76 treble = 0, 77 bass = 0; 83 MG DCO = 0, 84 MG VCF = 0; joystick centred.

- **F3b again** — 31 = 40, 32 = 8; keys C2, C3, C4, C5, 2 s each, three passes in one take: 33 = 0, 33 = 1, 33 = 2 (the off and full passes are the reference for the half pass).
- **F6** — 37 = 0, 17 = 31; 31 = 20, 32 = 8, 35 = 15; DEG3 61–66 = 16 20 10 0 10 16. 36 = 1: C3, add E3, add G3 (hold all), release all; then C3, E3, G3 staccato. Then 36 = 2: the same.
- **F7** — 37 = 0, 17 = 31, 12 = 2, 13–16 = 1; 31 = 60, 32 = 15; keys C2, C3, C4, 2 s each. Repeat with 17 = 8 (level dependence shows saturation).

## Implementation (built)

- VCF: zero-delay-feedback 4-pole ladder per output channel, feedback solved exactly; fc from the table (log interpolation) × keyboard tracking (highest key, C4 nominal) × 2^(7.9 oct · EG INT/15 · DEG3 level/31); k from the resonance table (held below 4). No saturation yet (F7).
- DEG3: one instance of the DEG code; single trigger = only the first key of a phrase, multi = every key; drives cutoff and noise VCA (both linear in the DEG level).
- Output: 54 Hz high-pass + VCF + one pole at 5 kHz (replaces the 2-pole 7.5 kHz stage).
- Not yet: MG to VCF (param 84).
