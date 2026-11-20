#!/usr/bin/env python3
"""Generates d50~/d50cc.pd, the controller layer of d50~ (tools/ctlayer.py with CC_ROWS of
tools/d50_params.py).

[d50cc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages 20-119 (without 32 38 64-69 84 88 96-101) -> outlet: n <name> <0-127>
  inlet: outlet 3 of d50~ (info ..., channel, txchannel), pickup 0/1, sync
  changes from elsewhere (patch change, editor, messages, SysEx) go back to the controller as
  CC ([ctlout]), pickup avoids jumps.
usage: python3 tools/gen_d50cc.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import ctlayer                                   # noqa: E402
import d50_params as T                           # noqa: E402

if __name__ == "__main__":
    out = os.path.join(ROOT, "d50~", "d50cc.pd")
    m = ctlayer.write(out, "d50cc (d50~)", T.CC_ROWS)
    print("wrote", out, f"({len(m)} settings on CC)")
