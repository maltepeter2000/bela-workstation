#!/usr/bin/env python3
"""Generates cz4/cz4cc.pd, the controller layer of cz4~ (tools/ctlayer.py with CC_ROWS of
tools/cz4_params.py).

[cz4cc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages from 20 on -> outlet: n <name> <0-127>
  inlet: outlet 3 of cz4~ (info ..., channel, txchannel), pickup 0/1, sync
  changes from elsewhere (tone change, line or envelope selection, editor, SysEx) go back to
  the controller as CC ([ctlout]); pickup avoids jumps.
usage: python3 tools/gen_cz4cc.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import ctlayer                                   # noqa: E402
import cz4_params as T                           # noqa: E402

if __name__ == "__main__":
    out = os.path.join(ROOT, "cz4", "cz4cc.pd")
    m = ctlayer.write(out, "cz4cc (cz4~)", T.CC_ROWS)
    print("wrote", out, f"({len(m)} settings on CC)")
