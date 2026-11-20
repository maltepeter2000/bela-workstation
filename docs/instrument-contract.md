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

**On Bela**: control channel 10 for the pads, the Launchkey display for names and values,
an OSC remote on UDP, CPU measured at block size 64 on the Bela Gem.

## State per instrument

| | juno | d50 | pdoly8 | tx81z | cz4 |
|---|---|---|---|---|---|
| Object | `juno~` | `d50~` | `pdoly8~` | `tx81zn~` (native); `tx81z~` firmware | `cz4~`; `cz1~` oscillator |
| `n` / `ninfo` / `info` | yes | yes | yes | yes, plus `fm` / `fminfo` for the FM view | yes |
| Controller layer, receive + send channel | common | common | common | common (`txcc`, `txfmcc` with own selectors) | common |
| User bank | `junouser-bank.syx`, 56, Juno-106 SysEx | `d50user-bank.syx`, 64, D-50 SysEx | `p8user-bank.txt` | `txuser-bank.txt`, 64 | `cz4user-bank.syx`, CZ SysEx |
| Pad messages in the object | yes | yes | no: user bank and presets live in Pd (editor, `p8user`, `p8factory`) | no: user bank lives in Pd (editor, Bela patch); no init voice | yes |
| `display` | yes | yes | no (OLED text of its own) | no (OLED text of its own) | yes |
| Pads | `junopads.pd`, common layout | `d50pads.pd`, common layout | – | own layout inside its Bela `_main.pd` | `cz4pads.pd`, common layout |
| Bela project | `bela/juno` (Bela Gem) | – | `bela/` (Pepper), `bela-oled/` (first Bela) | `bela-pd/tx81zn-pd` (Bela Gem) | – |
| Bela Gem build | `make bela-gem` | `make bela-gem` | `make bela-gem` | `make -C pd bela-gem` | `make bela-gem` |
| CPU, Bela Gem | 21–27 %, 6 voices, block 64 | not measured | not measured | not measured | not measured |
| Needs | – | `d50_pcm.bin` from own ROMs | – | `tx81z-v1.6.bin` | – |

## Done

1. **One controller layer** (`common/tools/ctlayer.py`): the send queue and selector option
   from tx81z joined with `txchannel` from cz4/juno/d50; all `<x>cc.pd` regenerated. Every
   object takes and reports `txchannel` (pdoly8~ and tx81zn~: 0 = the receive channel).
2. **One pad layout** (`common/tools/ctpads.py`, see `launchkey/README.md`); juno, d50, cz4
   understand the pad messages and report `display`.
3. **Bela Gem builds for all** (`make bela-gem`, cross-built with zig).

## Next

1. **pdoly8 and tx81zn: pads and display.** Their user bank lives in Pd today, so the pad
   messages have no counterpart in the object, and tx81zn has no init voice. Open: move the
   bank into the object as in juno/d50/cz4, or translate the pad messages in Pd. Both
   should also report `display`.
2. **Measure CPU** of every instrument on the Bela Gem at block size 64.
3. **One Bela generator.** A shared generator writes the instrument part of a Bela patch
   (object, layer, pads, display, OSC) from a small table per instrument, instead of five
   hand-grown `_main.pd` generators.
4. **The rack.** One Pd patch on the Bela Gem: each instrument in a slot with `[switch~]`,
   switched on and off, an output target and two sends per slot, `vrs23` as send effect,
   a mixer. Pd first; a C++ host using Bela's multi-core API only if one core is not enough.
5. **Effects for Bela.** `vrs23~` is heavy (estimated far above one core of the first Bela;
   not measured on the Gem). The minimal reverb + delay from the beladrumrack chat
   (`fx.pd`, `vrsm-*.pd`, delivered as `beladrumrack-patch.zip`, never committed) is the
   candidate for the rack.

## Not moved

`m1~` will be developed here (`instruments/m1`). The old repositories still hold their last
state; a note pointing here is still to be added.
