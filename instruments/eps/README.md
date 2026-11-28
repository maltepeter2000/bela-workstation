# eps~ – Ensoniq EPS / EPS-16 PLUS / ASR-10

Plays the instruments of the Ensoniq samplers from their own files and disk images on a model of
the ES5505 ("OTIS") voice chip of the EPS-16 PLUS. No Ensoniq data is part of this folder: put
your instruments into a folder `eps/` beside your patch.

```
[eps.i~ 1]        library: eps/ beside the patch (disk images, EFE)
|      |          saving: into the images (Ensoniq format) or as EFE
[dac~   ]         audio to sample: Pd's input 1 (SAMPLE band: input) and inlet 2
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
  select, loop modes 0–4 (4: loop until release), three envelopes (pitch, filter, amp; soft /
  hard levels by velocity, attack velocity, keyboard time scaling, two-stage release, finite
  mode), the LFO (7 waves, speed, depth, delay, reset / humanize, depth and rate modulation),
  random pitch, modulation of pitch, both filters, amp and pan from the 16 sources (LFO,
  ENV1–3, VEL, VEL2, PR+VL, VEL3, KBD, KBD2, WHEEL, PEDAL, XCTRL, PRESS, WHL+PR, PITCH),
  crossfades, layer glide (mono), all 7 wave modulation types (loop position, sample
  start, both, loop start, loop end, loop start-X, transwave), bend, pressure (channel and
  poly), pedal, sustain.
- **Loads in the background**: a file is read and the instrument built in a thread of its own;
  the scheduler swaps it in when it is ready, so the audio goes on.

Not yet there: pitch tables (format unknown, no instrument with one yet).

**Effects** (ES5510 / ESP) run as on the EPS-16 PLUS, with the user's ROM and OS (not part of
eps~): the two ROM dumps (`*.u27` lower = odd bytes, `*.u28` upper = even bytes, or one
interleaved `eps16plus-rom.bin`) and the OS 1.30 file (`eps16plus-os.efe` or any EFE of file type 27) in the library. A 68000
(Musashi, MIT, `third_party/musashi`, without its FPU and MMU) executes the effect block's
driver code together with the ROM and OS helpers (program loader, parameter bytecode,
real-time routine at 77 Hz); `engine/es5510` (from MAME, BSD-3) runs the ESP program at the
effect's rate (byte $66: 29 762 / 44 643 / 78 125 Hz). The OS parts eps~ does itself: the order
of the load, the fade-in of GPR 127, the OS flags the drivers read, the mod-source table
($FFDF4C). Output buses: 0 / 1 / 2 / 3 play on chip channel 0 / 2 / 3 / 1 (measured); channels
0, 2, 3 go to SER0, SER2, SER3, the output is SER1, channel 1 (aux) stays dry. Checked with
CLASSIC PAD, DECAY BASS and some 45 Waveboy effects; `build/fxprobe ROM OS file.efe` lists an
effect's parameters and runs noise through it. Messages `fx 0|1`, `fx`, `fxparam k v`,
`fxpage ±1|0`, `fxcell k v127`, `fxvar ±1`, `fxmode off|inst|bank|rom n|step ±1`, `fxlist`,
`fxsel ±1|cancel`, `fxload [k]`, `fxcopy bank|inst`, `fxsave [name]`.
The effects follow the EPS-16 PLUS manual (section 4, COMMAND/Effects, section 11): FX=INST plays
the instrument's effect (saved with it), FX=BANK the bank effect (effect files load into it; SAVE
BANK EFFECT writes it as a type 24 file), FX=ROM 1-13 the ROM effects (global, not saved; edited,
one becomes the bank effect), FX=OFF dry. COPY CURRENT EFFECT: TO BANK (what plays), TO INST (the
bank effect into the instrument; the file is then laid out anew by `buildInstrument`). The ROM
effects are whole effect blocks in the ROM; `findRomEffects` reads the OS's table of 13 pointers
(at $C03E48 in ROM 1.00F) and names them as the manual does. Editor band EFFECT: FX= < >, name,
variation, page; load effect < > load, copy to bank, copy to inst, save bank fx. Launchkey E20
(INST again): O/N FX=, SEL + O/N LOAD EFFECT, CMD COPY TO BANK? / COPY TO INST? / SAVE BANK
EFFECT? / DELETE INST EFFECT? (SEL + CMD: next, CMD: do it; `fxdelete`); VAR is the first encoder. The descriptors: flags $80 start
a group, $C0 continue its name, $E0 repeat it unnamed; lists are [lo][names][width][last].
FX mix (P2 encoder 5, `fxmix`) blends the effect output with the voices that went into it.

### Instrument locations 1-8

Like the EPS's INSTRUMENT buttons: `inst k` selects, `inststep ±1`, `instch k ch` (location k
plays on its channel; at the start k on k; the selected one also on the base channel),
`instdelete [k]`; outlet `inst k name ch`, `instsel k`, `instcur name ch`. Each location has
its own sampler (chip); all go into the one effect (FX=INST: the selected one's). The selected
location lives in t_eps (sampler, entry, controls, editor), the others in `Part`; selecting
swaps them, so loading, editing, sampling and saving work on the selected one unchanged.
Launchkey PLAY: SEL + a key C..G = location 1-8. Editor: PATCH band, row instrument.
Not yet: a common voice pool (each location has its own voices), program change on the other
channels.
Not yet: the X-IN effects (audio input into
the ESP).

**Calibrated against the original operating system** (OS 1.30, ROM 1.00F, running in an
emulator; register traces of test instruments, see `tests/calibration/`): the cutoff scale of
both filters (low-pass and high-pass), filter envelope, key tracking (relative to the root key)
and modulation amounts, the volume / envelope level law, pan law and boost, envelope times and
shapes (the amp envelope runs in the chip's volume code domain), attack velocity, keyboard
time scaling, the two-stage release (T5 to L5, then T6), the finite mode, LFO speeds, depth and
delay, pitch envelope (relative to its sustain level), pitch modulation and random amounts,
the wave modulation (word 3 of the five is the type: 0 off, 1 loop position, 2 sample start,
3 start + loop position, 4 loop start, 5 loop end, 6 loop start-X, 7 transwave; word 2 the
range, 2^(21 − n) samples, 6 = 32K; transwave moves by whole waves and ignores the range;
loop start-X lets the loop grow backwards, 2^(5.25 × value) times, and raises the pitch with
the loop length, which sounds like hard sync),
the behaviour of the mod sources 0–11 (4 and 6 linear velocity, 5 and 7 velocity curves, 8
and 9 the key, 10 wheel, 11 pedal). The display names of 4–9 and 12–15 still need confirming.
Compared with the original on DECAY BASS and MOOG FUNK 1, pitch, filter and volume registers
agree within a few percent. Still approximations: the soft / hard velocity levels (the
emulator showed no velocity response there), amp and pan modulation amounts, crossfade curves,
glide (every mode is mono with glide).

## Controls

Performance offsets over the loaded instrument (0 = as stored); saving writes `transpose tune
cut cut2 fenv att dec rel fatt fdec frel` into the instrument and sets them back to 0. `volume`
and `voices` are settings. One table: `tools/eps_params.py`.

| Page | CC | Controls |
|---|---|---|
| 1 PERFORM (P1) | 20–27 | `cut cut2 fatt fdec frel att dec rel` (filter envelope ENV2, amp envelope ENV3) |
| 2 PLAY (P2) | 28–31 | `volume transpose wave fenv` |

`patch tune bend vib vibrate voices`: editor only (row MORE); patch select is on the pads.

## Launchkey surface

eps.i~ drives the Launchkey Mini MK4 (custom firmware, pads in Drum mode) itself, in two layers
(`tools/eps_pages.py`, `pd/engine/eps_surface.*`, `pd/eps_lk.inc`): fixed pad meanings, the
display says what they do. Pad 51 (bottom right) is MODE in both layers: PLAY <-> EDIT, blue in PLAY, purple in EDIT.

| Layer | Top pads 36–43 | Bottom pads 44–51 | Encoders |
|---|---|---|---|
| PLAY | > – INIT(2x) PAT> – SMPL OPEN – | < – SAVE(2x) PAT< SEL – EXIT MODE | CC page 1 = P1, page 2 = P2 |
| EDIT | INST LAYER WAVE PITCH FILTER AMP LFO ENV (again: next page) | SEL LAY> WS> ENV# O/N CMD SMPL MODE | encoder n = cell n of the page shown (E1–E20) |

Display: a small line (page, layer, wavesample), two big lines of 10 characters with the chosen
option underlined, or a picture; the encoder names after a page change. The values of the page
go back to the encoders as CC. The bare `eps~` has the surface off (`lk 1`); `epscc` and
`epspads` stay for the Bela patch.

## Library and saving

- **Library**: argument 3 of `eps.i~` (relative to the patch or `~/...`), else the folder chosen
  with *choose* in the editor (kept in `eps-library.txt` beside the patch), else `eps/` beside
  the patch (`[eps~]` alone takes `eps/` beside the external): disk images (floppy, ZuluSCSI / BlueSCSI hard disk
  images) and EFE files. Folders as on the EPS: `browse`, `bstep`, `benter`, `bup`; `preset n`,
  `presetstep`, program change go through all instruments. `rescan` after adding files.
- **Saving** (`pd/engine/eps_fs.*`) always in the Ensoniq format, so the EPS reads it:
  `saveas <name>` (a new file in the folder shown: into the image, or `<name>.efe` at the top),
  `replace` (over the file it came from), `save` (a free name, the pads), `mkdir <name>`.
  The file is the instrument as loaded with every parsed field written back
  (`encodeInstrument`: byte for byte the same when nothing changed). The file system is written
  as the original OS 1.30 writes it (checked in the emulator: CREATE DIRECTORY gives the same
  blocks, the OS lists and loads instruments eps~ saved): directory entry with size = files in
  it and 2 contiguous blocks, a parent entry (type 8) first in each directory, FAT chain, free
  count in block 2, 38 files per directory, names of 12 characters A-Z 0-9 space `* - +` (the
  characters the OS offers when you type a name).
- **Safety**: before writing, eps~ checks the image (free count = FAT, directory sizes = file
  counts) and refuses otherwise. Every block it changes is copied to `<image>.undo` first;
  `build/eps_fs_tool undo <image>` restores the image and deletes the journal.

## Sampling

The wave recorder (`pd/engine/eps_record.*`, the SAMPLE band of the editor, `rec ...`
messages) works as SAMPLE on the EPS-16 PLUS, without a waveform display:

- **Rate**: the seven EPS rates, 625 kHz / 14, 21 … 56 (44.6, 29.8, 22.3, 17.9, 14.9, 12.8,
  11.2 kHz). Recorded at Pd's rate, then converted (windowed sinc, cutoff 0.45 of the lower
  rate, as the EPS's input filter follows the rate).
- **Threshold** 0–19 (0 = at once, else −54 to 0 dB), **pre-trigger** 0–127 ms, a level display
  (`recmeter`: bars, `*` = threshold), the free sample memory (1024K words with the instrument).
- **Into**: a new wavesample in the selected layer, the selected wavesample (keys, parameters
  and the loop if it fits stay), a new layer, a new instrument (UNNAMED INST).
- **PLAY ROOT KEY** after stop: the next key is the root key, then PLAY LOW KEY and PLAY HIGH
  KEY (a Launchkey pad or `rec learn 0` keeps the automatic range). A new wavesample gets the keys
  between the midpoints to its neighbours' root keys, as on the EPS; `rec learn root|lo|hi`
  (root key / low key / high key in the editor) and the cells `w_keylo` / `w_keyhi` change the
  key range in the layer's key map.

The instrument file is then built anew from the structs (`buildInstrument`: header, effect,
layers each followed by their wavesamples, pitch tables; known blocks keep their bytes, new ones
start as the OS creates them: UNNAMED INST / UNNAMEDLAYER / UNNAMED WS, measured on OS 1.30).
Checked with the original OS in the emulator: it loads an instrument recorded by eps~ (no
effect block) and plays it at the right pitch, and CLASSIC PAD with a wavesample added. The
recording is in memory until saved (`replace`, `saveas`, `save`).

## Wavesample commands

The commands of the COMMAND/Wave, LFO and Amp pages of the EPS (`pd/engine/eps_wave.*`, the
COMMAND band of the editor, `cmd ...` messages) on the selected wavesample, with KEEP=OLD NEW:
the result plays at once, `cmd old` / `cmd new` compare, `cmd keep` / `cmd cancel` decide.
truncate, crossfade, reverse / ensemble / bowtie crossfade (fade zone, scale depth 3–6 dB),
convert sample rate, normalize, fade in / out, volume smoothing, scale / reverse / invert /
clear data (range: sample, loop, start to loop, loop to end), new / copy / delete wavesample,
new / copy / delete layer (patches and key-up / key-down layers follow), copy parameters (all
but data, points, loop mode, root key, tuning, rate and keys) to another wavesample,
bidirectional crossfade, make loop longer, synthesized loop, copy / add / replicate data, mix,
merge and splice wavesamples (after the descriptions in the External Command Specification,
not measured). `alf`: AUTO-LOOP FINDING as on the EPS, a moved loop point goes where the loop
joins best.
`xcc` (row MORE, E1 XCTL) chooses the MIDI controller of the XCTRL mod source (2 by default).
The algorithms follow the manual's descriptions. Checked with the original OS: a wavesample
converted to 12.8 kHz, truncated and crossfaded plays at its pitch; a params-only copy in
another layer plays the shared data (copy-of layer = position of the original's layer). A new layer, a copied layer (params only and with
data) and the layer left after deleting the first one play on the original OS. Written blocks
start on 16 bytes: the OS ignores the low four bits of their offsets.

## Build

```
make            # into pd/ (PDINCLUDEDIR=../third_party/pd if Pd's headers are elsewhere)
make mac-zig    # bin/macos, universal
make bela-gem   # ../../build/bela-gem
make gen        # parameter header, controller layer, pads, editor, help, eps.i~
make test       # engine tests on synthetic disk images (ASan/UBSan, fuzzing)
make tools      # build/eps_dump, build/demo_real, build/read_all_files, build/eps_fs_tool
```

`build/eps_dump disk.img` lists a disk, `build/eps_dump PAD.EFE 00 wavs/` shows an instrument's
parameters and writes its wavesamples as WAV, `build/demo_real PAD.EFE pad.wav --sweep` renders
a demo with a mod wheel sweep.

## Sources and licences

- `pd/engine/es5505.*`: after MAME `src/devices/sound/es5506.cpp` (Aaron Giles), BSD-3-Clause.
- `pd/engine/eps_loader.*`: the Ensoniq file format after ConvertWithMoss
  (`format/ensoniq/epsasr`, Jürgen Moßgraber), LGPLv3.
- The rest: GPL-3.0-or-later, as the other instruments.
