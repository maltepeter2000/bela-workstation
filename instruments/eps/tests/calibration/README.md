# Calibration data

`measured.json` holds values measured on the original EPS-16 PLUS operating system
(OS 1.30, ROM 1.00F) running in an emulator. `tools/gen_eps_tables.py` turns it into
`pd/engine/eps_tables.h`.

How: copies of an instrument with single parameters patched (one value per wavesample,
twelve wavesamples per run) were loaded by the original OS, played over its MIDI input,
and every ES5505 register write was logged. The tables are the register values the OS
wrote: K1/K2 per cutoff, volume code per level, pan offsets, envelope segment times,
LFO periods, pitch amounts.

Neither the emulator, nor the ROM, nor the OS, nor any Ensoniq instrument is part of this
repository. The measurement scripts are not included either; the method is described
here and in the comments of `pd/engine/eps_sampler.cpp`.
