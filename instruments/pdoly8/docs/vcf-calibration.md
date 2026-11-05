# VCF / DEG3 calibration — status 2026-11-05

Measurements on Peter's Poly-800 Mk II (line out, 48 kHz). Analysed, **not yet built into pdoly8~**. Next step: record F3b–F7 (below), then implement VCF + DEG3 + tracking + noise VCA together and compare against the recordings.

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

## Still to record (F3b–F7)

Base setup: Mode 1; DCO1 octave 2, square, only 8', level 0; noise 15; VCF cutoff 99, resonance 0, KBD 0, polarity 2, EG INT 0, trigger 2; DEG3 and DEG1 A0 D0 BP31 SL0 SU31 R0; chorus/delay off; MG intensities 0; key C3; hold each setting about 3 s.

- **F3b** — cutoff 40, resonance 8. KBD 2: keys C2, C3, C4, C5 single, 2 s each; then KBD 1: same.
- **F4** — cutoff 0, resonance 8, DEG3 A24 D0 BP31 SL0 SU31 R24. (1) EG INT 15, polarity 2: hold 5 s, release, let it fade. (2) EG INT 8: same. (3) cutoff 99, EG INT 15, polarity 1: same.
- **F5** — (1) noise 15, 12, 8, 4, 1, 3 s each. (2) noise 15, DEG3 A24 R24: hold 5 s, release, let it fade.
- **F6** — noise 0, DCO1 level 31 (square 8'), cutoff 20, resonance 8, EG INT 15, DEG3 A16 D20 BP10 SL0 SU10 R16. Trigger 1 (single): C3, E3, G3 overlapping (legato), then three staccato notes. Trigger 2 (multi): same.
- **F7** — noise 0, DCO1 level 31, saw, all four feet, cutoff 60, resonance 15: keys C2, C3, C4, 2 s each.

## Implementation plan

- VCF: zero-delay-feedback 4-pole ladder per output channel, fc from the F1 table (log interpolation) × keyboard tracking × 2^(DEG3·EG INT·depth) × MG, k from the F2 table (clamped 3.95), saturation if F7 shows any.
- DEG3: one global instance of the existing DEG code with single/multi trigger; drives cutoff and the noise VCA.
- Output: 54 Hz high-pass + VCF + one pole at ~10 kHz (replaces the 2-pole 7.5 kHz stage).
- Validate like the feet and DEG1/2: render the recorded protocol and compare spectra/envelopes.
