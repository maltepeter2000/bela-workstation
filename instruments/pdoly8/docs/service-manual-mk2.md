# Poly-800 II service manual — check against pdoly8~ (2026-11-05)

| Item | Service manual (Poly-800 II) | pdoly8~ | Status |
|---|---|---|---|
| DEG decay / slope / release | exponential; a rising slope (sustain > break point) is linear | exponential in amplitude; rising slope linear in level | matches (measured on Peter's Mk II) |
| Poly-800 (Mk I) DEG | linear | — | Mk I would need a linear mode (not built) |
| VCF multi trigger | Mk II: a new key restarts the attack from the current level; a key during the attack changes nothing | same | matches (F6 recording) |
| Mk I multi trigger | drops to 0 and restarts | — | not built |
| VCF | 24 dB/oct low-pass (NJM2069, one VCF after the mix, mono) | 4-pole ladder | matches; pdoly8~ filters each output channel (stereo spread is an extension) |
| Cutoff | 0–99 in semitone steps (8.25 oct) | table 66 Hz–20 kHz = 8.24 oct | matches |
| Resonance | 0–15, Q 0–10 | k 0–3.95, no self-oscillation | matches the recordings |
| KBD track | 0 / 50 / 100 % | 0 / 0.5 / 1 oct/oct | matches; reference keys measured |
| EG INT | 0 to ±10 oct (±20 %) | 7.9 oct at 15 (measured) | measured value at the low edge of the tolerance |
| Detune | 0, 6.5, 13, 26 cents | was 0 to 20 cents | **changed** to 0, 6.5, 13, 26 |
| MG → DCO | 0 to ±160 cents | `mgi` 0–160 cents | **changed** (was 0–100) |
| MG → VCF | 0 to ±10 2/3 oct | — | not built yet (param 84) |
| Bend (joystick) | about +7 semitones (+35 cents adjustment) | `bnd` in semitones, free | set `bnd 7` for the original range |
| EQ | treble ±10 dB at 2 kHz, bass ±10 dB at 300 Hz, 13 steps | — | not built (recordings made with EQ 0) |
| DDL | 4–1024 ms, feedback 0–100 %, mod 0.1–10 Hz | — | not built |
| CV system | 8085 CPU, one D/A converter, time-shared S/H CVs, S/H clock 20 kHz; DEG clock on an interrupt | smooth per-sample control | CV stepping not modelled |
| Keyboard | 49 keys C2–C6, MIDI notes 36–84; key C3 at octave MID sounds C3 | MIDI note = sounding pitch at MID | matches |
