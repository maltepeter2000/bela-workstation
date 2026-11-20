# Launchkey

The Novation Launchkey Mini MK4 with the custom firmware
[maltepeter2000/launchkey_firmware](https://github.com/maltepeter2000/launchkey_firmware)
is the controller of the workstation. It stays set up the same for every instrument; the
instruments adapt to it. The firmware lives in its own repository.

| Part | What it does | Made by |
|---|---|---|
| Keys | notes on the instrument's MIDI channel (one channel per instrument) | – |
| Encoders, Custom 1 | CC pages from 20 in rows of 8; the layer sends every outside change back as CC on the instrument's send channel (`txchannel`), one CC per 2 ms, and the firmware takes it as the encoder's value | `common/tools/ctlayer.py` → `<x>cc.pd` |
| CC 3 = 127 | sync request: the layer sends all values of the channel back | the same |
| Pads, Drum mode | notes 36–51 on channel 10, the same layout for every instrument (below) | `common/tools/ctpads.py` → `<x>pads.pd` |
| Display | SysEx `F0 7D 4C 4B 01 <title> 00 <value> F7` shows two lines in the popup; at most one message per 40 ms | `common/tools/lkdisplay.py` → `lkdisplay.pd` (here) |

## Pads

| | 36 | 37 | 38 | 39–43 |
|---|---|---|---|---|
| **top row** | factory + | user + | init (press twice within 1 s) | the instrument's own |
| | **44** | **45** | **46** | **47–51** |
| **bottom row** | factory − | user − | save | the instrument's own |

The pads send the same messages to every instrument: `presetstep 1` / `presetstep -1`,
`userstep 1` / `userstep -1`, `init`, `save`. The instrument answers with
`display <title> <value>` (`Preset 12` + name, `User 11` + name, `Stored` + slot), which
`lkdisplay` puts on the Launchkey.

| Instrument | Pads | Own pads |
|---|---|---|
| juno | `junopads.pd` | 39 chorus I > II > I+II > off, 47 Juno-106 / 60 |
| d50 | `d50pads.pd` | – |
| cz4 | `cz4pads.pd` | – |
| pdoly8 | open, see `docs/instrument-contract.md` | |
| tx81zn | open, still its own layout inside its Bela patch | |

On Bela every MIDI byte that Pd sends is a system call in the audio thread, which is why
feedback and display messages are throttled.
