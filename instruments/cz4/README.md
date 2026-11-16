# CZ1 and CZ4 — Casio CZ for Pure Data

Two Pd externals around one phase-distortion core (the uPD933 chip of the Casio CZ series, as modelled in MAME):

| Object | What it is | Details |
|---|---|---|
| **`cz4~`** (CZ4) | a full Casio CZ-1: 16 polyphonic voices, two lines each, six 8-step envelopes per voice, CZ tones (128 and 144 bytes) with factory and user bank, the CZ-1 operation memory (split, tone mix, portamento, glide), chorus, the CZ SysEx with its handshake both ways, controller layer `[cz4cc]`, editor `[cz4edit]` | this file |
| **`cz1~`** (CZ1) | the oscillator alone, for the Eurorack (Bela Pepper): 4 voices paraphonic, continuous wave morph, vintage layer, no envelopes | [cz1/README.md](cz1/README.md) |

`cz1~` was called `cz4~` until November 2026 (last state under that name: branch `v0.9-cz4-osc`).

## cz4~

Hardware reference is the CZ-1, software reference Arturia's CZ V: everything a CZ tone holds plays as on the CZ and can go to a CZ and back; the simpler access CZ V adds (a performance row, an easy envelope) writes into the same CZ data.

```
[r $0-cz4]    notes, messages            [midiin] -> inlet 3 (raw MIDI, CZ SysEx)
|
[cz4~]  outlets: left, right, events (info ... / SysEx bytes as floats)
```

| | Type | Function |
|---|---|---|
| Inlet 1 | float, list, any | note (velocity from inlet 2), `note velocity [channel]`, messages |
| Inlet 2 | float | velocity of the next note (default 100) |
| Inlet 3 | float, list, any | raw MIDI bytes: notes, bend, aftertouch, program change, CC 1, 7, 64, CZ SysEx; or messages |
| Outlet 1, 2 | signal | left, right |
| Outlet 3 | any, float | `info <name> <0-127> <text>` for every change, `tonea`/`toneb <bank> <n> <name>`, `name`, `op`, channels; SysEx as floats for `[midiout]` |

### Controls

77 controls, all generated from `tools/cz4_params.py` (header, controller layer, editor, help). Values are those the CZ shows. Set them by name (`r3 70`), from a pot or CC (`n r3 <0-127>`), report them with `ninfo`.

| Page (CC) | Controls | Kind |
|---|---|---|
| 1 PERFORM (20–27) | `dcw attack release balance detune vibrato chorus volume` | acts at once, not stored |
| 2 LINE / WAVE | `line linesel wave wave2 window mod octave level` | tone; `line` selects line 1/2 |
| 3 DETUNE / VIBRATO | `dsign doct dnote dfine vwave vdelay vrate vdepth` | tone |
| 4 ENVELOPE | `env sus end kfw kfa velp velw vela` | `env` selects DCO / DCW / DCA |
| 5 RATES | `r1` … `r8` | selected envelope |
| 6 LEVELS | `l1` … `l8` | selected envelope |
| 7 EASY ENV / TOUCH | `eattack edecay esustain erelease edit wheel atvib atamp` | easy envelope writes steps 1–3; `edit` selects tone A/B |
| 8 OPERATION | `keymode split bsrc bnum avol bvol boct bdet` | operation memory |
| 9 OPERATION 2 | `achorus bchorus porta ptime glide gnote gtime solo` | operation memory |
| 10 SETUP | `bend voices tune transpose chip` | `bend` operation memory, the rest instrument |

The CC numbers follow the controller layer of pdoly8~, tx81z~, d50~ and juno~ (pages of 8 from CC 20, see `tools/ctlayer.py`).

### Messages

| Message | Function |
|---|---|
| `preset n`, `user n`, `init`, `reload` | a factory tone, a user slot (0–63), the init tone, the same tone again; into tone A, or B with `edit 1` |
| `compare 0/1` | hear the stored tone instead of the edit |
| `store [n]`, `name <text>` | the edited tone into a user slot; rename it |
| `copyline 1 2`, `copyenv 1 DCW 2 DCW` | copy within the edited tone |
| `userwrite`, `userread [file]`, `usersend`, `presets`, `users` | user bank file `cz4user-bank.syx`, as SysEx, lists |
| `op n`, `opstore [n]`, `opinit`, `opwrite`, `opread [file]`, `ops` | operation memory, file `cz4user-op.syx` |
| `sysex b …`, `sysexdump [prog]` | a CZ tone message in; the edited tone out |
| `czget <prog>`, `czsend [prog]`, `czmodel 1/101` | talk to a real CZ with Casio's handshake (CZ-1: 144 bytes, CZ-101 family: 128) |
| `channel n`, `txchannel n` | receive channel (0 = all), send channel |
| `bend`, `mod`, `at`, `sustain`, `stop`, `reset`, `status`, `dump` | playing and state |

### SysEx

As a device `cz4~` answers like a CZ: send request (`F0 44 00 00 7n 10|11 pp`) and receive request (`20|21 pp`) are answered with `F0 44 00 00 7n 30` and the tone follows, so CZ editors and librarians work with it. Program `60` (hex) is tone A, `20`–`5F` the user slots 0–63, `00`–`1F` the first factory tones. As a host it fetches and sends tones with `czget` / `czsend`. Librarian messages (receive request with data in the same message) and `.syx` files are read as well.

### Data files

Nothing is needed. Every `.syx` beside the external is read for CZ tones (CZ-101/1000/3000/5000 with 128 bytes, CZ-1 with 144) and joins the factory bank; without any, the factory bank holds the INIT tone. Casio's tones are not part of this repository. `cz4user-bank.syx` (64 CZ-1 tones, readable by CZ librarians) and `cz4user-op.syx` (operation memory, `F0 7D` messages) are written beside it.

### How close it is

- Waves, windows, wave pairs, ring and noise modulation: the uPD933 as modelled in MAME, computed with the chip's 12-bit tables (`chip 0`: ideal).
- Envelopes: the chip's rate law (`(8 | r&7) << (r>>3)` per chip sample at 40 kHz), its targets and the logarithmic DCA (about 78 dB over 128 levels); sustain and end as Casio documents them; the end step falls to zero.
- Tone data: all 64 tones of a CZ-1 bank decode to panel values and encode back to the same bytes (`tone_test`).
- Estimates until compared with recordings (Arturia CZ V or a real CZ): vibrato clock, velocity and key-follow curves, portamento and glide times, chorus, output level.

## Build

```sh
make PDINCLUDE=/path/to/pd/src      # cz1~ and cz4~ for this machine
make test && ./cz_test && ./tone_test my-cz-bank.syx && ./cz4_render my-cz-bank.syx
make mac-zig                        # universal macOS binaries + patches in dist/ (zig, llvm-lipo)
make generate                       # header, cz4cc, cz4edit, help patches from tools/
make bela PDINCLUDE=/usr/local/include/libpd   # on the Bela board
```

Put `cz4~.pd_darwin` (or `.pd_linux`), `cz4~-help.pd`, `cz4cc.pd` and `cz4edit.pd` in one folder, with your CZ `.syx` files beside them.

## Files

| Path | Content |
|---|---|
| `core/cz_core.*` | phase-distortion core (both objects) |
| `core/cz_tone.*` | CZ tone data: panel values ↔ bytes, SysEx nibbles, `.syx` scanning |
| `cz4/cz4_core.*` | the CZ-1 instrument (voices, envelopes, operation memory, chorus) |
| `cz4/cz4~.cpp` | the Pd external: messages, banks, handshake |
| `cz4/cz4cc.pd`, `cz4/cz4edit.pd`, `cz4/cz4~-help.pd` | controller layer, editor, help (generated) |
| `cz1/` | the oscillator |
| `tools/` | `cz4_params.py` (the one table), generators, `ctlayer.py`, `help_draw.py` |
| `tests/` | `cz_test.c`, `tone_test.c`, `cz4_render.cpp` |
| `third_party/pd/m_pd.h` | Pd's header for the macOS cross build |

## Credits and licence

By Peter with Claude (Anthropic). BSD-3-Clause (`LICENSE`). The CZ engine follows melodee by Kerem Kilic (`firmware/src/cz_native.c`, BSD-3) and the uPD933 model by Devin Acker in MAME (BSD-3, `LICENSE-BSD-3-uPD933.txt`). Tone format after Casio's MIDI specifications as documented in melodee's `docs/CZ1_SYSEX.md`; no GPL code from melodee is used.
