# Instrument contract

What every instrument in `instruments/` provides, so the rack patch, the Launchkey and the
editors can treat them alike; where each one stands today; and the order in which they are
aligned. State: 20 Nov 2026.

## The contract

**Object** `[<name>~]`

- Outlets 1, 2: audio left, right.
- Last outlet: messages. Every change is reported as `info <name> <0-127> <text>` in the same
  logical time; also patch names, the channel (`channel <n>`, `txchannel <n>`) and
  `display <title> <value>` for the Launchkey display or an OLED.
- Messages: `n <name> <0-127>` sets a control from a pot or CC, `ninfo <name>` reports it,
  `<name> <value>` sets it in the instrument's own units. Notes, bend, program change and SysEx
  as the original instrument understands them.
- Patches: a factory bank, a user bank in a file beside the patch, `store`/`save` only on an
  explicit save (choosing a slot never writes).
- Pad messages, the same in every instrument: `presetstep [n]` and `userstep [n]` (the
  factory patch / user slot after the one played last, wrapping), `init` (the init patch),
  `save` (the playing sound into the user slot played or stored last, the user bank
  written). Each answers with `display <title> <value>`.
- Two MIDI channels: receive (`channel`, 0 = all) and send (`txchannel`), both reported, so
  the controller layer follows them.

**Files in the instrument folder**

| File | Made by |
|---|---|
| `<x>cc.pd`: controller layer, CC pages from 20 in rows of 8, feedback, pickup, sync | `common/tools/ctlayer.py` |
| `<x>pads.pd`: the Launchkey pads on channel 10 | `common/tools/ctpads.py` |
| `<x>edit.pd`: editor, rows = CC pages, then the controls without a CC | the instrument's editor generator |
| `<name>~-help.pd` | the instrument's help generator |
| `<x>user-bank.*` | the instrument, on save |
| a Bela Gem project or slot | the instrument's Bela generator |

**Page 1 (CC 20-27) is the same in every instrument** (except pdoly8~, which keeps its own order: page 1 PLAY, page 2 FILTER), so a hand finds it at once after
switching instruments:

| Encoder | 1 bright | 2 resonance | 3 attack | 4 release | 5 vibrato | 6 effect | 7 own | 8 volume |
|---|---|---|---|---|---|---|---|---|
| juno~ | cutoff | res | attack | release | dcolfo | chorus | env | volume |
| d50~ | cut | res | att | rel | vib | chorus | balance | volume |
| cz4~ | dcw | detune | attack | release | vibrato | chorus | balance | volume |
| tx81zn~ (fm) | moda | fdbk | atkc | relc | lpmd | dtun | mix | vol |
| eps~ | cut | cut2 (filter 2: no resonance on the EPS) | att | rel | vib | – (no effects yet) | wave (transwave position) | volume |

Volume is not stored in a sound (100 or the instrument's default = unity). d50's cut, res,
att, rel, vib and pdoly8's atk, rel are offsets over the sound (0 = as stored) and go back to
0 when a sound is recalled; so are all controls of eps~ (offsets over the loaded Ensoniq
instrument, kept with a user slot); the others are the sound's own settings.

**On Bela**: control channel 10 for the pads, the Launchkey display for names and values,
an OSC remote on UDP, CPU measured at block size 64 on the Bela Gem.

## State per instrument

| | juno | d50 | pdoly8 | tx81z | cz4 | eps |
|---|---|---|---|---|---|---|
| Object | `juno~` | `d50~` | `pdoly8~` | `tx81zn~` (native); `tx81z~` firmware | `cz4~`; `cz1~` oscillator | `eps~` |
| `n` / `ninfo` / `info` | yes | yes | yes | yes, plus `fm` / `fminfo` for the FM view | yes | yes |
| Controller layer, receive + send channel | common | common | common | common (`txcc`, `txfmcc` with own selectors) | common | common |
| User bank | `junouser-bank.syx`, 56, Juno-106 SysEx | `d50user-bank.syx`, 64, D-50 SysEx | `p8user-bank.txt`, 64 (text: more settings than a Poly-800 II program; Poly-800 II dumps in and out through the editor) | `txuser-bank.syx`, 64, two TX81Z voice bulk dumps | `cz4user-bank.syx`, CZ SysEx | `epsuser-bank.txt`, 64, text (file, instrument, controls); the instruments themselves are a library: `eps/` beside the patch |
| Pad messages | in the object | in the object | `p8nav.pd` (bank in Pd) | in the object (`tx81zn~`; `tx81z~` has user, userstep, store, init) | in the object | in the object |
| `display` | object: patch, control, store | object: patch, control, store | object: control; `p8nav`: patch, store | object: patch, control, store | object: patch, control, store | object: patch, control, store, loading |
| Pads | `junopads.pd` | `d50pads.pd` | `p8pads.pd` | `txpads.pd`, used in `bela/tx81zn-pd` | `cz4pads.pd` | `epspads.pd` |
| Bela project | `bela/juno` (Bela Gem) | – | `bela/pepper`, `bela/oled` (first Bela) | `bela/tx81zn-pd` (Bela Gem), `bela/tx81z-pd`, `bela/tx81z` (C++) | – | – |
| Builds | `make`, `make mac-zig`, `make bela-gem`, `make gen` in every instrument (into `pd/`, `bin/macos`, `build/bela-gem`) | | | | | |
| CPU, Bela Gem | 21–27 %, 6 voices, block 64 | not measured | not measured | not measured | not measured | not measured |
| Needs | – | `d50_pcm.bin` from own ROMs | – | `tx81z-v1.6.bin` | – | your own Ensoniq instruments or disk images in `eps/` |

## Done

1. **One controller layer** (`common/tools/ctlayer.py`): the send queue and selector option
   from tx81z joined with `txchannel` from cz4/juno/d50; all `<x>cc.pd` regenerated. Every
   object takes and reports `txchannel` (pdoly8~ and tx81zn~: 0 = the receive channel).
2. **One pad layout** (`common/tools/ctpads.py`, see `launchkey/README.md`); juno, d50, cz4
   understand the pad messages and report `display`.
3. **Bela Gem builds for all** (`make bela-gem`, cross-built with zig).
4. **Pads for pdoly8 and tx81zn** through a Pd translator (`common/tools/ctnav.py`), same
   layout, messages and display lines; tx81zn~ got the Init Voice.
5. **Pad LEDs**: one colour code for all instruments, sent by `<x>pads.pd` (factory blue,
   user green, init orange, save red, own pads purple, free pads off; see
   `launchkey/README.md`). Firmware side: launchkey_firmware `main`.
6. **Display of turned controls** in every object (pdoly8~ and tx81zn~ too).
7. **User banks:** SysEx where the original has a format, text otherwise. tx81zn~ (and
   tx81z~) keep 64 voices in `txuser-bank.syx` (two TX81Z voice bulk dumps) and take the
   pad messages themselves; the tx translator is gone. pdoly8~ stays text (its settings go
   beyond a Poly-800 II program). All banks are named `<x>user-bank.*`, so they can lie side
   by side in the rack's folder.
8. **One layout and the same make targets** in every instrument (`pd/`, `bin/macos`, `bela/`,
   `make`, `make mac-zig`, `make bela-gem` into `build/bela-gem`, `make gen`; see README).
9. **Page 1 the same everywhere** (table above). New for it: d50~ `vib`, tx81zn~ FM page PERFORM with `vol`; the other pages moved to make room.
10. **Levels:** `common/tools/measure_levels.py` plays every instrument offline and writes
    `docs/levels.md`: a trim per instrument that brings the median sound to -20 dBFS, for
    the rack (measured here: juno +3.9 dB, pdoly8 +2.5 dB; d50, tx81zn and cz4 need the
    ROMs or a factory bank, so they are measured on a machine that has them).

11. **One object per instrument** (`common/tools/instobj.py`): `[<x>.i~ <channel> <pads
    channel>]` holds the external, its editor (the object's window), controller layer, pads
    (and translator), Launchkey display, MIDI in and out; connect it to `[dac~]` and play.
    `juno.i~`, `d50.i~`, `cz4.i~`, `pdoly8.i~`, `tx81zn.i~` (txfm, txedit inside),
    `tx81z.i~`. User banks (cz4: also the operation memory) lie beside the patch that holds
    the object; until they are there, the bank beside the external is the starting point.
    Inside an `.i~` the external's own folder is "the patch's folder": ROMs and data files
    (d50_pcm.bin, tx81z-v1.6.bin) lie beside the externals.
12. **Help patches without GUI** (`common/tools/helpdraw.py`, skill pd-help-layout): one
    example, the reference as text, the controls row by row as in the editor; one patch
    under both names (`<x>.i~-help.pd`, `<x>~-help.pd`). Editors and help share their
    colours (`common/tools/style.py`); the editors have no readme any more.

## Next

1. **Measure CPU** (started, `docs/cpu.md`: juno~ and tx81zn~ at block size 64; the rest and an empty project still to do) of every instrument on the Bela Gem at block size 64.
2. **One Bela generator.** A shared generator writes the instrument part of a Bela patch
   (object, layer, pads, display, OSC) from a small table per instrument, instead of five
   hand-grown `_main.pd` generators.
3. **The rack.** One Pd patch on the Bela Gem: each instrument in a slot with `[switch~]`,
   switched on and off, an output target and two sends per slot, `vrs23` as send effect,
   a mixer. Pd first; a C++ host using Bela's multi-core API only if one core is not enough.
4. **Effects for Bela.** `vrs23~` is heavy (estimated far above one core of the first Bela;
   not measured on the Gem). The minimal reverb + delay from the beladrumrack chat
   (`fx.pd`, `vrsm-*.pd`, delivered as `beladrumrack-patch.zip`, never committed) is the
   candidate for the rack.

## Not moved

`m1~` will be developed here (`instruments/m1`). The old repositories still hold their last
state; a note pointing here is still to be added.
