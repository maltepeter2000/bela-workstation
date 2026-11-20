#!/usr/bin/env python3
"""Generates p8cc.pd, the controller layer of pdoly8~ (tools/ctlayer.py with CC_ROWS of gen_bela.py).

[p8cc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages 20-119 (without 32 38 64-69 84 88 96-101) -> outlet: n <name> <0-127>
  inlet: outlet 3 of pdoly8~ (info ...), pickup 0/1, sync
  changes from elsewhere go back to the controller as CC ([ctlout]), pickup avoids jumps.
Used by all three versions: desktop (help patch), Bela Pepper and Bela with OLED.
Also writes p8pads.pd (the Launchkey pads, common/tools/ctpads.py) and p8nav.pd (the pad
translator: factory presets and user bank as dumps, common/tools/ctnav.py).
usage: python3 tools/gen_p8cc.py   (writes p8cc.pd and copies it into bela/project, bela-oled/project)
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import gen_bela as G                             # noqa: E402

import ctlayer                                   # noqa: E402
import ctnav                                     # noqa: E402
import ctpads                                    # noqa: E402

# the instrument's own pads: random sounds (39-43 top row, 47-48 bottom row)
OWN_PADS = {39: "random pluck", 40: "random bass", 41: "random lead", 42: "random brass",
            43: "random pad", 47: "random strings", 48: "random total"}

# pdoly8~ keeps presets and user bank in Pd: [p8nav] turns the pad messages into dumps
NAV = {"factory_count": 64, "factory_file": "presets/poly800mk2-factory.txt",
       "user_file": "p8user-bank.txt", "user_slots": 64,
       "get_msg": "dump", "reply": "dump", "recall": "dump", "init_msg": "defaults", "name": None}


def write(path):
    return ctlayer.write(path, "p8cc (pdoly8~)", G.CC_ROWS)


if __name__ == "__main__":
    write(os.path.join(ROOT, "p8cc.pd"))
    ctpads.write(os.path.join(ROOT, "p8pads.pd"), "p8pads (pdoly8~)", ctpads.layout(OWN_PADS))
    ctnav.write(os.path.join(ROOT, "p8nav.pd"), "p8nav (pdoly8~)", NAV)
    print("p8pads.pd, p8nav.pd")
    for d in ("bela/project", "bela-oled/project"):
        if os.path.isdir(os.path.join(ROOT, d)):
            shutil.copy(os.path.join(ROOT, "p8cc.pd"), os.path.join(ROOT, d))
    print("p8cc.pd (+ copies in bela/project, bela-oled/project)")
