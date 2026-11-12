# pico-vintage-synths

Pure Data externals built on the engines of
[PicoVintageSynthCollection](https://github.com/Michi71/PicoVintageSynthCollection)
by Michi71. The engines are native C++ re-implementations of vintage
synthesizers written for the RP2350; here they run as Pd objects on desktop
and on Bela.

| Object | Instrument | State |
|---|---|---|
| `d50~` | Roland D-50 | builds and plays on Linux; macOS and Bela untested |

## d50~

A D-50 as one object: sixteen voices, the patch's own chorus, equalizer and
reverb, stereo out. Notes go in as `note velocity` lists or as a float with
the velocity on the second inlet; everything else is a message such as
`preset 62` or `reverb 40`. The third outlet reports preset names and state.
`d50~-help.pd` documents every message.

### What you need

The repository contains no Roland data, and the object is silent without it.
You need your own dumps of:

- the two PCM ROMs (IC30 and IC29, or the combined chip of late boards),
- the program EPROM (IC22),
- optionally one or more D-50 SysEx bulk dumps (`.syx`) as patch banks.
  Without a bank the object plays eight built-in presets.

Files are identified by content, so their names do not matter.

### Build

```bash
python3 tools/make_d50_data.py /path/to/your/roms
cd d50~
make
```

The first command writes `d50~/d50_pcm.bin`, `d50~/generated/d5_pcm_table.h`
and copies any `.syx` files into `d50~/`. All of them are ignored by git and
must stay out of any repository. `make` needs Pure Data's `m_pd.h`; see
`pd-lib-builder/Makefile.pdlibbuilder` for `PDDIR` and the other switches.

At run time the object looks for `d50_pcm.bin` and the `.syx` files in the
folder of the external, then in the folder of the patch. Banks load in
alphabetical order with `PND50-00.syx` first, and presets are numbered
through from 0.

### Notes

- The engine runs at the D-50's 32 kHz and is resampled to Pd's rate.
- `governor 1` ends released notes early when a block takes too long to
  compute. It is off by default and meant for Bela.
- `tools/gen_help.py` generates the help patch; do not edit the patch by hand.

## Layout

```
d50~/                 the external, its Makefile and help patch
d50~/engine/          PicoFaceD5 engine headers, unmodified
tools/d5_extract/     ROM converter from PicoVintageSynthCollection
tools/                make_d50_data.py, gen_help.py
pd-lib-builder/       build helper (public domain, Katja Vetter)
```

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). The engine in `d50~/engine/` and
the converter in `tools/d5_extract/` are from PicoVintageSynthCollection
(commit `642e69c`), Copyright (C) 2026 Michi71. `d50_core.cpp` derives from
that project's `D5_Bridge.cpp`.

Roland and D-50 are trademarks of Roland Corporation. This project is not
affiliated with Roland.
