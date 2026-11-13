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

The patch itself can be edited while it plays, in two ways:

- by name: `u1 cutoff 60`, `upper structure 3`, `patch keymode 1`. The
  sections are `u1 u2 upper l1 l2 lower patch`; the help patch lists the
  109 names. `param <address> <value>` reaches any of the 448 patch bytes.
- by D-50 SysEx, for existing editors and librarians: `[sysexin]` into the
  third inlet, the floats of the third outlet into `[midiout]`. Data set to
  the temporary area (00-00-00) and to internal memory (02-00-00) and
  requests for both are handled.

`dump` reports every parameter, `store` keeps an edit in its bank slot until
Pd closes. Writing a bank back to a `.syx` file is not implemented yet.

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
cd d50~
make
python3 ../tools/make_d50_data.py /path/to/your/roms
```

`make` needs Pure Data's `m_pd.h` and no ROM data; see
`pd-lib-builder/Makefile.pdlibbuilder` for `PDINCLUDEDIR` and the other
switches. Every push also builds a universal macOS binary and a Linux one
(Actions tab, artifacts of the latest run).

The last command writes `d50~/d50_pcm.bin` and copies any `.syx` files into
`d50~/`. They are ignored by git and must stay out of any repository.

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
d50~/engine/          PicoFaceD5 engine headers, unmodified, and the sample
                      table generated from upstream's d5_sample_table.json
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
