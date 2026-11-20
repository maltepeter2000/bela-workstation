# Engine

The Juno-60 engine of PicoFaceJ6 from
[PicoVintageSynthCollection](https://github.com/Michi71/PicoVintageSynthCollection)
(GPL-3.0-or-later), `instruments/PicoFaceJ6/include/juno` and `src/juno` at
commit 26260b2. Built with `-DJUNO_HOST_BUILD -DJUNO_OVERSAMPLE=2`.

Changes, each marked `juno~ addition` in the source:

- `juno_voice.h`: gain and cutoff offset per note from the key velocity; the
  modulation wheel's share of the vibrato (`modLfo`), taken from the LFO
  before its delay.
- `juno_env.h`: `JunoLfo::raw()`, the triangle before the delay envelope.
- `juno.h`, `juno.cpp`: `setVelocitySens`, `setModWheel`, and `noteOn` passing
  the velocity on.
- Oversampling (`juno_voice.h`, `juno.cpp`): the voice kept only the last of
  its oversampled samples and the decimation filter then ran on that value,
  held. That is decimation without an anti-alias filter followed by a
  low-pass: the aliasing of 1x (worse at the top of the keyboard) and highs
  taken away. The voices now deliver every oversampled sample and the filter
  runs on all of them. At 2x a sawtooth at C7 has its aliasing at -53 dB
  instead of -25 dB, and a sawtooth with the filter open is 3 to 7 dB brighter
  between 4 and 15 kHz. Upstream builds at 1x, where this does not arise.
- Decimation filter (`juno.cpp`, `juno_dsp.h`, new `juno_decim.h` from
  `tools/make_juno_decim.py`): an 8th-order elliptic low-pass instead of the
  6th-order Butterworth at 15 kHz. Flat to 0.42 of the output rate (20.2 kHz
  at 48 kHz), at least 80 dB down where anything would fold back. At 1x there
  is nothing to decimate and the chain is skipped. `JUNO_OVERSAMPLE` must be
  1, 2 or 4.
- Chorus (`juno_fx.h`, `juno_fx.cpp`), after the chorus board in the Juno-60
  service notes (p. 11): ahead of the lines R15/C7 (7.2 kHz) and the two
  Sallen-Key stages around TR19/TR18 (9.69 kHz Q 0.549, 10.38 kHz Q 1.291), the
  same two stages after each line, instead of one pole at 7 kHz and one at
  11 kHz. The mix is that of IC8: direct signal through 47k, its own line
  through 39k, into 100k, so the line comes in 1.205 times the direct signal,
  and the direct signal stays as loud as with the chorus off (upstream mixed
  0.7 : 0.7). Chorus on is therefore louder than off, as on the instrument.

The arpeggiator and the engine's own CC map are present but not used.
- Juno-106 model (`juno.h`, `juno.cpp`, `juno_env.h`, `juno_voice.h`, `juno_dco.h`,
  `juno_filter.h`, `juno_fx.h`, `juno_fx.cpp`, `juno_defs.h`): `Juno::setModel106` switches the
  high-pass to the Juno-106's positions, the chorus to its rate and sweep, doubles the contour
  and LFO delay times and lifts sub and noise; `setLfoRateHz` and `setVcaGain` take the LFO
  rate and the VCA level directly. The sliders themselves are remapped in `juno_core.cpp`.
