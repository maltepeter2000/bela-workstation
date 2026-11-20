# Instrument contract

What every instrument in `instruments/` provides, so the rack patch, the Launchkey and the
editors can treat them alike; where each one stands today; and the order in which they are
aligned. State: 20 Nov 2026, right after the move into this repository.

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

**Files in the instrument folder**

| File | Made by |
|---|---|
| `<x>cc.pd`: controller layer, CC pages from 20 in rows of 8, feedback, pickup, sync | `tools/ctlayer.py` |
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
| Controller layer | ctlayer with `txchannel` | ctlayer with `txchannel` | oldest ctlayer (no `txchannel`, no send queue) | ctlayer with send queue (one CC per 2 ms) and own selectors | ctlayer with `txchannel` |
| User bank | `junouser-bank.syx`, 56, Juno-106 SysEx | `d50user-bank.syx`, 64, D-50 SysEx | `p8user-bank.txt` | `txuser-bank.txt`, 64 | `cz4user-bank.syx`, CZ SysEx |
| Bela project | `bela/juno` (Bela Gem) | – | `bela/` (Pepper), `bela-oled/` (first Bela) | `bela-pd/tx81zn-pd` (Bela Gem) | – |
| Bela Gem build | cross build `make bela-gem` | – | – (armv7 only) | on the board | – (armv7 only) |
| Pads, channel 10 | `junopads.pd` | – | – | inside `_main.pd` | – |
| Display | Launchkey (`lkdisplay`) and OLED | – | OLED | OLED | – |
| CPU, Bela Gem | 21–27 %, 6 voices, block 64 | not measured | not measured | not measured | not measured |
| Needs | – | `d50_pcm.bin` from own ROMs | – | `tx81z-v1.6.bin` | – |

## Order of alignment

1. **One `ctlayer.py` for all.** Merge the three versions into `common/tools/ctlayer.py`:
   the send queue and the selector option from tx81z, `txchannel` from cz4/juno/d50. The same
   for `patchwriter.py`. Every generator imports from `common/tools`; regenerate all
   `<x>cc.pd` and check them in Pd. pdoly8 gains `txchannel` and the send queue.
2. **Launchkey pieces for all.** `ctpads.py` and `lkdisplay.py` move to `common/`. One pad
   layout for every instrument, so the hands learn it once. Proposal from what juno and tx81zn
   share today:

   | | 36 | 37 | 38 | 39–43 |
   |---|---|---|---|---|
   | top | factory + | user + | init (twice) | instrument's own |
   | | **44** | **45** | **46** | **47–51** |
   | bottom | factory − | user − | save | instrument's own |

   Every instrument reports `display <title> <value>` for the Launchkey display.
3. **Bela Gem builds for all.** An aarch64 target (`bela-gem`) in each Makefile, cross-built
   like juno; measure CPU per instrument at block size 64.
4. **One Bela generator.** A shared generator writes the instrument part of a Bela patch
   (object, layer, pads, display, OSC) from a small table per instrument, instead of five
   hand-grown `_main.pd` generators.
5. **The rack.** One Pd patch on the Bela Gem: each instrument in a slot with `[switch~]`,
   switched on and off, an output target and two sends per slot, `vrs23` as send effect,
   a mixer. Pd first; a C++ host using Bela's multi-core API only if one core is not enough.
6. **Effects for Bela.** `vrs23~` is heavy (estimated far above one core of the first Bela;
   not measured on the Gem). The minimal reverb + delay from the beladrumrack chat
   (`fx.pd`, `vrsm-*.pd`, delivered as `beladrumrack-patch.zip`, never committed) is the
   candidate for the rack.

## Not moved

`m1~` stays in pico-vintage-synths. The old repositories still hold their last state; a
note pointing here is still to be added.
