#!/usr/bin/env python3
"""Generates pd/eps.i~.pd: eps~ as one instrument object (common/tools/instobj.py), with the
editor epsedit, the controller layer epscc, the pads epspads and lkdisplay.

    [adc~ 1]
    |
    [eps.i~ <channel> <pads channel>] -> [dac~]      (inlet 2: the audio to sample)

The library (the folder eps/: Ensoniq disk images and EFE files) lies beside the patch that
holds the object; eps~ saves into it. Run after the abstractions are generated (make gen does).
usage: python3 tools/gen_eps_inst.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(os.path.dirname(HERE), "pd")
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import instobj                                   # noqa: E402

if __name__ == "__main__":
    instobj.write(os.path.join(PD, "eps.i~.pd"),
                  obj="eps.i~", ext="eps~", short="eps",
                  edit=os.path.join(PD, "epsedit.pd"), edit_title="eps~ editor",
                  cc=os.path.join(PD, "epscc.pd"),
                  pads=os.path.join(PD, "epspads.pd"),
                  lkdisplay=os.path.join(PD, "lkdisplay.pd"),
                  banks=[],
                  dir_msgs=[("library", "eps")],
                  midi=("raw", 2, "sysex"),
                  sig_in=3, adc_in=True)
