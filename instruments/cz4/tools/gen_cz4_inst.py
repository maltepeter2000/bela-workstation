#!/usr/bin/env python3
"""Generates pd/cz4.i~.pd: cz4~ as one instrument object (common/tools/instobj.py), with the
editor cz4edit, the controller layer cz4cc, the pads cz4pads and lkdisplay. Two files beside
the patch: the user bank (tones) and the operation memory.

    [cz4.i~ <channel> <pads channel>] -> [dac~]

Run after the abstractions are generated (make gen does).
usage: python3 tools/gen_cz4_inst.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(os.path.dirname(HERE), "pd")
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import instobj                                   # noqa: E402

if __name__ == "__main__":
    instobj.write(os.path.join(PD, "cz4.i~.pd"),
                  obj="cz4.i~", ext="cz4~", short="cz4",
                  edit=os.path.join(PD, "cz4edit.pd"), edit_title="cz4~ editor",
                  cc=os.path.join(PD, "cz4cc.pd"),
                  pads=os.path.join(PD, "cz4pads.pd"),
                  lkdisplay=os.path.join(PD, "lkdisplay.pd"),
                  banks=[("userwrite", "userread", "cz4user-bank.syx"),
                         ("opwrite", "opread", "cz4user-op.syx")],
                  midi=("raw", 2))
