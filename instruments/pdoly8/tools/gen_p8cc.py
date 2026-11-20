#!/usr/bin/env python3
"""Generates p8cc.pd, the controller layer of pdoly8~ (tools/ctlayer.py with CC_ROWS of gen_bela.py).

[p8cc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages 20-119 (without 32 38 64-69 84 88 96-101) -> outlet: n <name> <0-127>
  inlet: outlet 3 of pdoly8~ (info ...), pickup 0/1, sync
  changes from elsewhere go back to the controller as CC ([ctlout]), pickup avoids jumps.
Used by all three versions: desktop (help patch), Bela Pepper and Bela with OLED.
usage: python3 tools/gen_p8cc.py   (writes p8cc.pd and copies it into bela/project, bela-oled/project)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import gen_bela as G                             # noqa: E402

import ctlayer                                   # noqa: E402


def write(path):
    return ctlayer.write(path, "p8cc (pdoly8~)", G.CC_ROWS)


if __name__ == "__main__":
    write(os.path.join(ROOT, "p8cc.pd"))
    for d in ("bela/project", "bela-oled/project"):
        if os.path.isdir(os.path.join(ROOT, d)):
            shutil.copy(os.path.join(ROOT, "p8cc.pd"), os.path.join(ROOT, d))
    print("p8cc.pd (+ copies in bela/project, bela-oled/project)")
