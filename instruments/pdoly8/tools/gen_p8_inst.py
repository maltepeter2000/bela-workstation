#!/usr/bin/env python3
"""Generates pd/pdoly8.i~.pd: pdoly8~ as one instrument object (common/tools/instobj.py), with
the editor p8edit, the controller layer p8cc, the pads p8pads with the pad translator p8nav and
lkdisplay. Notes come from [notein] (pdoly8~ has no raw MIDI inlet), the channel is ch1. The
user bank p8user-bank.txt is kept by the editor and p8nav: inside pdoly8.i~ both use the file
beside the patch (else, as before, the one beside pdoly8~ or the factory programs).

    [pdoly8.i~ <channel> <pads channel>] -> [dac~]

Run after the abstractions are generated (make gen does).
usage: python3 tools/gen_p8_inst.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(os.path.dirname(HERE), "pd")
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import instobj                                   # noqa: E402

if __name__ == "__main__":
    instobj.write(os.path.join(PD, "pdoly8.i~.pd"),
                  obj="pdoly8.i~", ext="pdoly8~", short="pdoly8",
                  edit=os.path.join(PD, "p8edit.pd"), edit_title="pdoly8~ editor",
                  cc=os.path.join(PD, "p8cc.pd"),
                  pads=os.path.join(PD, "p8pads.pd"), nav=os.path.join(PD, "p8nav.pd"),
                  lkdisplay=os.path.join(PD, "lkdisplay.pd"),
                  banks=[], editor_bank="p8user-bank.txt",
                  midi=("notein",), channel_msg="ch1")
