#!/usr/bin/env python3
"""Generates juno~/junocc.pd, the controller layer of juno~ (tools/ctlayer.py with CC_ROWS of
tools/juno_params.py).

[junocc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages from 20 on -> outlet: n <name> <0-127>
  inlet: outlet 3 of juno~ (info ..., channel, txchannel), pickup 0/1, sync
  changes from elsewhere (patch change, editor, messages, SysEx) go back to the controller as
  CC ([ctlout]), pickup avoids jumps.
usage: python3 tools/gen_junocc.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ctlayer                                   # noqa: E402
import juno_params as T                          # noqa: E402

if __name__ == "__main__":
    out = os.path.join(ROOT, "juno~", "junocc.pd")
    m = ctlayer.write(out, "junocc (juno~)", T.CC_ROWS)
    print("wrote", out, f"({len(m)} settings on CC)")
