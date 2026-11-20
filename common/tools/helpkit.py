#!/usr/bin/env python3
"""Shared text for the help patches of every instrument: the Launchkey section.

Every help patch (made by the instrument's own generator in the layout of pd-help-layout)
has the section "Launchkey" with these paragraphs, so the setup reads the same everywhere.
The generator places the live objects itself, in its object area:

    [<pads> 10] -> ([<nav>] ->) [s <bus>]          the pads in Drum mode, channel 10
    outlet 3 / [r <bus>-out] -> [lkdisplay] -> [midiout]   (with a nav: its right outlet)

launchkey(obj, cc, pads, own, nav=None, cc_channel=1) -> list of paragraphs (para mode).
  obj   the object ("juno~"), cc / pads / nav the abstractions ("junocc", "junopads", "p8nav")
  own   what the instrument's own pads do, e.g. "39 chorus, 47 Juno-106 / 60", or "" for none
"""

PAD_LAYOUT = ("top row 36 factory +, 37 user +, 38 init (press twice within a second), "
              "39-43 own, bottom row 44 factory -, 45 user -, 46 save, 47-51 own")
PAD_COLOURS = ("factory blue, user green, init orange (flashing while the second press is "
               "awaited), save red (white for a moment when pressed), own pads purple, free pads off")


def launchkey(obj, cc, pads, own, nav=None, cc_channel=1):
    paras = [
        "Launchkey: the Novation Launchkey Mini MK4 with the custom firmware "
        "maltepeter2000/launchkey_firmware, set up the same for every instrument of "
        "bela-workstation (launchkey/README.md there).",
        f"{cc} {cc_channel}: the controller layer. Encoders in mode Custom 1 send CC pages from "
        "20 in rows of 8 on the keys channel, every change made elsewhere goes back to them on "
        f"the send channel (txchannel of {obj}), CC 3 asks for all values.",
        f"{pads} 10: the pads in Drum mode (notes 36-51 on channel 10, the Drums Channel): "
        f"{PAD_LAYOUT}. " + (f"Own pads of {obj}: {own}." if own else f"{obj} has no own pads yet."),
        f"Pad colours, sent by {pads} itself with noteout on channel 10 (velocity = colour + 32 x "
        "mode): " + PAD_COLOURS + ". They go out at load, on leds, and when the Launchkey asks "
        "for them (CC 3 = 127 on channel 10, sent when Drum mode is selected).",
    ]
    if nav:
        paras.append(f"{nav}: turns the pad messages into what {obj} understands (its user bank "
                     "is kept in Pd), sends the display lines of patch changes and stores and "
                     f"passes those of {obj} on.")
    paras.append(f"lkdisplay: the display lines (display <title> <value>: the control you turn, "
                 "Preset, User, Init or Stored with the patch name) as SysEx for the Launchkey's "
                 "display -> midiout.")
    return paras
