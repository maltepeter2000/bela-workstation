# Launchkey

The Novation Launchkey Mini MK4 with the custom firmware
[maltepeter2000/launchkey_firmware](https://github.com/maltepeter2000/launchkey_firmware)
is the controller of the workstation. It stays set up the same for every instrument; the
instruments adapt to it. The firmware lives in its own repository.

| Part | What it does | Today |
|---|---|---|
| Keys | notes on the instrument's MIDI channel (one channel per instrument) | all instruments |
| Encoders, Custom 1 | CC pages from 20 in rows of 8 (`docs/controller-layer.md` in each instrument); the layer sends every outside change back as CC, the firmware takes it as the encoder's value | all; three versions of the layer, see `docs/instrument-contract.md` |
| CC 3 = 127 | sync request: the layer sends all values of the channel back | all |
| Pads, Drum mode | notes 36–43 (top row) and 44–51 (bottom row) on channel 10: patch, user bank, store, init | juno (`junopads.pd`), tx81zn (in its `_main.pd`); layouts differ |
| Display | SysEx `F0 7D 4C 4B 01 <title> 00 <value> F7` shows two lines in the popup | juno (`lkdisplay.pd`, at most one message per 40 ms) |

The generators for pads and display are in `instruments/juno/tools/ctpads.py` and
`instruments/juno/tools/lkdisplay.py` for now; they become shared pieces for all
instruments (step 2 in `docs/instrument-contract.md`).

On Bela every MIDI byte that Pd sends is a system call in the audio thread: feedback and
display messages are throttled (tx81z's layer: one CC per 2 ms; lkdisplay: one popup per
40 ms).
