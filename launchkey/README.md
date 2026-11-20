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
| Pad colours | note on on channel 10 to the Launchkey: note = pad, velocity = colour + 32 × mode (0 static, 1 flashing, 2 pulsing), velocity 0 = the Launchkey's own LED; the firmware asks with CC 3 = 127 on channel 10 when Drum mode is selected | the same |
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
| pdoly8 | `p8pads.pd` → `p8nav.pd` | 39–43 random pluck, bass, lead, brass, pad; 47 strings, 48 total |
| tx81zn | `txpads.pd` (in its Bela patch) | the same random pads |

### Pad colours

The same in every instrument, by what the pad does:

| Pad | Colour |
|---|---|
| factory + / − | blue |
| user + / − | green |
| init | orange, flashing after the first press while the second is awaited |
| save | red, white for a moment when pressed |
| the instrument's own | purple |
| free | off |

`[<x>pads]` sends them itself with `[noteout]` on its channel: at load, on `leds`, on a
channel change and when the Launchkey asks (CC 3 = 127 on the pad channel). The palette and
the protocol are in the firmware's README ("Pad colours from the host"); the colours per
function are `COLORS` in `common/tools/ctpads.py`. Needs launchkey_firmware with pad LEDs
(branch `pad-leds`).

pdoly8~ keeps its user bank in Pd; `[p8nav]` (common/tools/ctnav.py) turns the pad messages
into what it understands, sends the display lines itself and passes the object's own on.

On Bela every MIDI byte that Pd sends is a system call in the audio thread, which is why
feedback and display messages are throttled.
