# pico-vintage-synths

Pure Data externals built on the engines of
[PicoVintageSynthCollection](https://github.com/Michi71/PicoVintageSynthCollection)
by Michi71. The engines are native C++ re-implementations of vintage
synthesizers written for the RP2350; here they run as Pd objects.

| Object | Instrument | State |
|---|---|---|
| `d50~` | Roland D-50 | macOS and Linux; no Bela version yet |
| `juno~` | Roland Juno-60, with Juno-106 patches and SysEx | macOS and Linux; no Bela version yet |

## d50~

A D-50 as one object: sixteen voices, the patch's own chorus, equalizer and
reverb, stereo out. It follows the conventions of `pdoly8~` and `tx81z~`:
the same controller layer, an editor in rows of 8, a generated help patch.

| | Type | Function |
|---|---|---|
| Inlet 1 | float, list, messages | notes (`note velocity [channel]`), controls, patches |
| Inlet 2 | float | velocity for a note sent as a float |
| Inlet 3 | floats, messages | raw MIDI bytes from `[midiin]` (notes, bend, aftertouch, program change, CC 1 5 7 64 65, D-50 SysEx) |
| Outlet 1, 2 | signal | left, right |
| Outlet 3 | messages, floats | `info <name> <0-127> <text>` for every change, patch names, channels; SysEx bytes for `[midiout]` |

### Controls

All 117 controls have one name each, used by messages, the controller layer
and the editor (`tools/d50_params.py` is the single table):

- `<name> <value>` sets a control in the values the D-50 shows, `<name>` reports it.
- `n <name> <0-127>` sets it from a pot or MIDI CC, `ninfo <name>` reports it.
- The D-50 has four partials, two per tone. A partial name (`cutoff`, `level` …)
  acts on the selected partial, a tone name (`structure`, `lfo1rate` …) on the
  tone it belongs to. `part 0…3` selects U1, U2, L1, L2.
- The performance row acts on the whole patch: `cut res att rel chorus reverb
  balance volume`. `cut` moves the cutoff of every synth partial, also of
  notes that are sounding; `res`, `att`, `rel` shift resonance, attack and
  release for the notes that follow. These four are not stored and return to
  0 with a patch change.
- A partial or tone edit is heard from the next note on (a voice copies its
  parameters at note-on). Chorus, equalizer, reverb and the balances follow
  at once.
- By address, without changing the selection: `l2 cutoff 30` (sections `u1 u2
  upper l1 l2 lower patch`), `param <address> <value>` for any of the 448
  patch bytes, `dump`.

### Controller layer and editor

- `[d50cc <channel>]`: MIDI CC 20–119 in pages of 8, feedback to the
  controller, pickup, CC 3 = sync — the layout of `p8cc` and `txcc`
  (`docs/controller-layer.md`). Page 1 is the performance row, pages 2–6 the
  selected partial, pages 7–11 its tone.
- `[d50edit <bus>]`: every control in rows of 8 (rows 1–11 = the CC pages),
  partial select, factory presets and user bank, MIDI channels, SysEx.
- `channel <0-16>` is the receive channel, `txchannel <1-16>` the channel of
  the feedback and the device number of SysEx dumps.

### Patches

- `preset n`: a factory preset, read only. Every `.syx` beside the external
  (except the user bank) is a factory bank; `PND50-00.syx` comes first.
- `user n`: a slot of the user bank (64). `store n` puts the playing patch
  there, `userwrite` writes `d50user-bank.syx`, `userread` reads it. In the
  editor, *store to N* only chooses the slot and *save patch* stores and writes.
- `init`: a plain sawtooth patch. `reload`: the same patch again, edits dropped.
- SysEx, for editors, librarians and a real D-50: data set and request for the
  temporary area (00-00-00, the playing patch) and the internal memory
  (02-00-00, the user bank). `usersend` and `sysexdump` send without being asked.
  The user bank file is a complete D-50 bulk dump: the 64 patches and the
  reverb data a D-50 keeps behind them (the definitions of its reverb types
  17–32, address 03-60-00). The engine has its own reverbs and does not use
  that block; it is taken from the dump it arrived in — the first factory
  bank until the user bank brings its own — and passed on unchanged.

### What you need

The repository contains no Roland data, and the object is silent without it.
You need your own dumps of:

- the two PCM ROMs (IC30 and IC29, or the combined chip of late boards),
- the program EPROM (IC22),
- a D-50 SysEx bulk dump (`.syx`) as factory bank. Without one the object
  plays eight built-in presets, which cannot be edited.

Files are identified by content, so their names do not matter.

```bash
python3 tools/make_d50_data.py /path/to/your/roms
```

writes `d50~/d50_pcm.bin` and copies the `.syx` files into `d50~/`. They are
ignored by git and must stay out of any repository. At run time the object
looks for them in the folder of the external, then in the folder of the patch.

### Build

- **macOS:** the package is in `bin/macos/` (universal, see `LIESMICH.txt`);
  it is cross-built from Linux with `make mac-zig` in `d50~/`.
- **From source:** `cd d50~ && make` (needs Pure Data's `m_pd.h`, see
  `pd-lib-builder/Makefile.pdlibbuilder` for `PDINCLUDEDIR`).
- `make gen` regenerates the parameter header, `d50cc.pd`, `d50edit.pd` and
  the help patch. Do not edit those by hand.
- The workflow in `.github/workflows` builds on real macOS and Linux runners
  when started by hand.

### Notes

- The engine runs at the D-50's 32 kHz and is resampled to Pd's rate.
- `governor 1` ends released notes early when a block takes too long to compute.
- Portamento glides each voice from its own last note, as the engine models
  the hardware; it is heard once the voices have played.

## juno~

A Juno-60 as one object: six voices, the switched high-pass, the IR3109
low-pass, the chorus, stereo out. It needs no data files. Patches are kept in
the format of the Juno-106, whose panel is nearly the same and which, unlike
the Juno-60, has MIDI — so Juno-106 patch collections play, and patches go to
a Juno-106 or an editor for it and back.

Inlets, outlets and messages are those of `d50~`; inlet 3 takes notes with
velocity, bend, program change, CC 1 7 64 and Juno-106 SysEx.

### Controls

30 controls, one name each (`tools/juno_params.py` is the single table):

| Page | CC | Controls |
|---|---|---|
| 1 VCF / ENV | 20–27 | `cutoff res env attack decay sustain release chorus` |
| 2 DCO | 28–31, 33–36 | `range pulse saw sub noise pwm pwmmode dcolfo` |
| 3 LFO / VCF / VCA | 37, 39–45 | `lforate lfodelay vcflfo kybd envpol hpf level vcamode` |
| 4 SETUP | 46–51 | `volume tune bend octave vellevel velcutoff` |

Pages 1 to 3 are the patch: the 16 sliders (0–127) and 8 switches of a
Juno-106. Page 4 belongs to the instrument and is not stored. `<name> <value>`
sets a control, `n <name> <0-127>` sets it from a pot, and every change is
reported as `info <name> <0-127> <text>`. Edits are heard at once.
`[junocc <channel>]` is the controller layer, `[junoedit <bus>]` the editor.

### What differs from the instruments

| | Juno-60 | Juno-106 | `juno~` |
|---|---|---|---|
| Engine | | | Juno-60 (PicoFaceJ6) |
| Patch format, SysEx | none | 18 bytes | Juno-106 |
| Chorus | off, I, II, I+II | off, I, II | all four; I+II is stored in a bit a Juno-106 ignores, which plays it as I |
| Pulse width | LFO, manual, envelope | LFO, manual | LFO, manual |
| Sub oscillator | switch and level | level | level, 0 is off |
| Arpeggiator | yes | no | no |
| Velocity | no | no | optional: `vellevel`, `velcutoff`, 0 by default |
| Modulation wheel | no | vibrato | vibrato |
| Voices | 6 | 6 | 6, handed out in turn |

The chorus follows the chorus board of the Juno-60 service notes: each output
is the direct signal plus its own bucket-brigade line (full stereo width), the
line 1.2 times as loud as the direct signal, with the board's filters ahead of
and after the lines. As on the instrument, switching the chorus on makes the
sound louder.

The nine factory sounds that used pulse width from the envelope have the
fixed width instead, at the value it had while a key is held. The engine's
oscillator and filter run at twice the sample rate (`make OVERSAMPLE=1` for
none).

### Patches

- `preset n`: 0–55 are the factory sounds of the Juno-60, from the chart in
  its owner's manual as transcribed in PicoFaceJ6. Every `.syx` file of
  Juno-106 patches in the folder of the external follows from 56 on.
- `user n`: the user bank, 56 slots as on the Juno-60 (shown as bank and
  number, 11–78). `store n` keeps the playing patch, `userwrite` writes
  `junouser-bank.syx`: 56 Juno-106 patch messages of 24 bytes.
- `usersend` and `sysexdump` send the bank or the playing patch as Juno-106
  SysEx. A Juno-106 plays a patch it receives but stores it only from its own
  panel, so after `usersend` it holds the last one.
- A patch message (`F0 41 30 …`, `31`) replaces the playing patch, a
  parameter message (`F0 41 32 ch param value F7`) changes one byte.
  `sysexout 1` makes the object send those itself for every change, like a
  Juno-106.

### Build

```
cd juno~
make                 # this machine
make mac-zig         # universal macOS binary into ../bin/macos (zig, llvm-lipo)
make gen             # parameter header, controller layer, editor, help
```

## Layout

```
d50~/                 the external, d50cc.pd, d50edit.pd, help patch, Makefile
d50~/engine/          PicoFaceD5 engine headers and the sample table
juno~/                the external, junocc.pd, junoedit.pd, help patch, Makefile
juno~/engine/         PicoFaceJ6 engine, see its README for the changes
bin/macos/            the Mac package (no ROM data)
docs/                 controller-layer.md
tools/                d50_params.py, juno_params.py, generators, ctlayer.py, make_d50_data.py
tools/d5_extract/     ROM converter from PicoVintageSynthCollection
third_party/pd/       m_pd.h for the cross build
pd-lib-builder/       build helper (public domain, Katja Vetter)
```

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). The engine in `d50~/engine/` and
the converter in `tools/d5_extract/` are from PicoVintageSynthCollection
(commit `642e69c`), Copyright (C) 2026 Michi71. `d50_core.cpp` derives from
that project's `D5_Bridge.cpp`.

The engine headers carry three additions for `d50~`, each marked in the
source: a live cutoff offset per voice (`set_perf_cutoff`), LFOs that restart
when their settings are edited, and a master tune per tone so the patch's
own fine tune survives.

Roland and D-50 are trademarks of Roland Corporation. This project is not
affiliated with Roland.
