#!/usr/bin/env python3
"""Generates pd/epscc.pd, the controller layer of eps~ (common/tools/ctlayer.py with CC_ROWS of
tools/eps_params.py), and pd/epspads.pd, the Launchkey pads (common/tools/ctpads.py).

[epscc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages from 20 on -> outlet: n <name> <0-127>
  inlet: outlet 3 of eps~ (info ..., channel, txchannel), pickup 0/1, sync
usage: python3 tools/gen_epscc.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import ctlayer                                   # noqa: E402
import eps_params as T                           # noqa: E402

if __name__ == "__main__":
    out = os.path.join(ROOT, "pd", "epscc.pd")
    m = ctlayer.write(out, "epscc (eps~)", T.CC_ROWS)
    print("wrote", out, f"({len(m)} settings on CC)")
    import ctpads
    out = os.path.join(ROOT, "pd", "epspads.pd")
    pads = ctpads.layout(T.OWN_PADS)
    ctpads.write(out, "epspads (eps~)", pads)
    print("wrote", out, f"({len(pads)} pads)")
