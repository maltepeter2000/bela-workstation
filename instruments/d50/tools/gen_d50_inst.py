#!/usr/bin/env python3
"""Generates pd/d50.i~.pd: d50~ as one instrument object (common/tools/instobj.py), with the
editor d50edit, the controller layer d50cc, the pads d50pads and lkdisplay.

    [d50.i~ <channel> <pads channel>] -> [dac~]

Run after the abstractions are generated (make gen does).
usage: python3 tools/gen_d50_inst.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(os.path.dirname(HERE), "pd")
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import instobj                                   # noqa: E402

if __name__ == "__main__":
    instobj.write(os.path.join(PD, "d50.i~.pd"),
                  obj="d50.i~", ext="d50~", short="d50",
                  edit=os.path.join(PD, "d50edit.pd"), edit_title="d50~ editor",
                  cc=os.path.join(PD, "d50cc.pd"),
                  pads=os.path.join(PD, "d50pads.pd"),
                  lkdisplay=os.path.join(PD, "lkdisplay.pd"),
                  banks=[("userwrite", "userread", "d50user-bank.syx")],
                  midi=("raw", 2))
