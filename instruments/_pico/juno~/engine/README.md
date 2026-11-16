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

The arpeggiator and the engine's own CC map are present but not used.
