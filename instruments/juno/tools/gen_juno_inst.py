#!/usr/bin/env python3
"""Generates pd/juno.i~.pd: juno~ as one instrument object (common/tools/instobj.py), with the
editor junoedit, the controller layer junocc, the pads junopads and lkdisplay.

    [juno.i~ <channel> <pads channel>] -> [dac~]

Run after the abstractions are generated (make gen does).
usage: python3 tools/gen_juno_inst.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(os.path.dirname(HERE), "pd")
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import instobj                                   # noqa: E402

if __name__ == "__main__":
    instobj.write(os.path.join(PD, "juno.i~.pd"),
                  obj="juno.i~", ext="juno~", short="juno",
                  edit=os.path.join(PD, "junoedit.pd"), edit_title="juno~ editor",
                  cc=os.path.join(PD, "junocc.pd"),
                  pads=os.path.join(PD, "junopads.pd"),
                  lkdisplay=os.path.join(PD, "lkdisplay.pd"),
                  banks=[("userwrite", "userread", "junouser-bank.syx")],
                  midi=("raw", 2))
