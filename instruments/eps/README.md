# eps~ – Ensoniq EPS / EPS-16 PLUS / ASR-10

Plays the instruments of the Ensoniq samplers from their own files and disk images on a model of
the ES5505 ("OTIS") voice chip of the EPS-16 PLUS. No Ensoniq data is part of this folder: put
your instruments into a folder `eps/` beside your patch.

```
[eps.i~ 1]        library: eps/ beside the patch (EFE, IMG, EDE, EDA, GKH)
|      |          user bank: epsuser-bank.txt beside the patch
[dac~   ]
```

## What it does

- **Reads** Ensoniq disk images and files (`pd/engine/eps_loader.*`): `.efe`, `.img` (floppy, also
  hard disk and SD card images, read block by block), `.ede`/`.eda` (Giebler), `.gkh`, raw
  devices. Directories and the FAT are followed. Per instrument: layers (key and velocity
  ranges, patch select, key-down / key-up), wavesamples with their 16-bit samples, sample and
  loop points, loop modes, three envelopes, filters, pan, volume, LFO and modulation settings and
  the wave modulation (transwave). HFE floppy images: convert to IMG first.
- **Plays** them (`pd/engine/es5505.*`, `eps_sampler.*`): the ES5505 after MAME's
  `es5506.cpp` (linear interpolation, the 4-pole filter in its four arrangements, 8-bit log
  volume, forward / backward / bidirectional loops, 20.9 addresses, 2/1024 pitch steps) and a
  rebuilt operating system on top: voice allocation (20 voices, up to 32), layers and patch
  select, loop modes 0–4 (4: loop until release), amp and filter envelopes (soft / hard levels
  by velocity), cutoff with key tracking, pan, transwave and loop-position modulation (mod
  wheel), bend, sustain.
- **Loads in the background**: a file is read and the instrument built in a thread of its own;
  the scheduler swaps it in when it is ready, so the audio goes on.

Not yet there: the instrument's LFOs, the pitch envelope, most modulation routings, glide,
crossfades, the ES5510 effects. The OS side (envelope and volume curves, cutoff scale) is a first
approximation, to be measured against the original (register traces from an EPS-16 PLUS
emulator). Values checked on a real EPS-16 PLUS display (CLASSIC PAD): fine tune in 1/128
semitone, wave mod source / amount / type / range.

## Controls

Performance offsets over the loaded instrument (0 = as stored), kept with a user slot;
`volume` and `voices` are settings and not stored. One table: `tools/eps_params.py`.

| Page | CC | Controls |
|---|---|---|
| 1 PERFORM | 20–27 | `cut cut2 att rel vib` – `wave volume` |
| 2 PLAY | 28–31, 33–36 | `patch transpose tune bend dec fenv vibrate voices` |

Page 1 follows the other instruments (bright, resonance, attack, release, vibrato, effect, own,
volume). The EPS has no resonance: `cut2` (filter 2) takes its place. The effect place stays free
until eps~ has its effects; own is `wave`, the transwave position. Pads 39 / 47: patch select
+ / −.

## Presets and user bank

- **Library**: every instrument in every file of `eps/` beside the patch (`eps.i~` sends
  `library <dir>/eps` at load; `[eps~]` alone takes `eps/` beside the external), in the order of
  the file names and of the instruments inside a disk image. `preset n`, `presetstep`, program
  change. `rescan` after adding files.
- **User bank**: `epsuser-bank.txt`, 64 slots, text (no Ensoniq format covers it): per slot the
  file, the instrument number in it, the name and the controls.

## Build

```
make            # into pd/ (PDINCLUDEDIR=../third_party/pd if Pd's headers are elsewhere)
make mac-zig    # bin/macos, universal
make bela-gem   # ../../build/bela-gem
make gen        # parameter header, controller layer, pads, editor, help, eps.i~
make test       # engine tests on synthetic disk images (ASan/UBSan, fuzzing)
make tools      # build/eps_dump, build/demo_real, build/read_all_files
```

`build/eps_dump disk.img` lists a disk, `build/eps_dump PAD.EFE 00 wavs/` shows an instrument's
parameters and writes its wavesamples as WAV, `build/demo_real PAD.EFE pad.wav --sweep` renders
a demo with a mod wheel sweep.

## Sources and licences

- `pd/engine/es5505.*`: after MAME `src/devices/sound/es5506.cpp` (Aaron Giles), BSD-3-Clause.
- `pd/engine/eps_loader.*`: the Ensoniq file format after ConvertWithMoss
  (`format/ensoniq/epsasr`, Jürgen Moßgraber), LGPLv3.
- The rest: GPL-3.0-or-later, as the other instruments.
