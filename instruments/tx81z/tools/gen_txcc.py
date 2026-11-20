#!/usr/bin/env python3
"""Generates pd/txcc.pd, the controller layer of tx81z~ (tools/ctlayer.py with CC_ROWS of
tools/tx81z_params.py).

[txcc <channel>]  (0 = all channels)
  [ctlin] inside: CC pages 20-119 (without 32 38 64-69 84 88 96-101) -> outlet: n <name> <0-127>
  inlet: outlet 3 of tx81z~ (info ...), pickup 0/1, sync
  changes from elsewhere (program change, editor, messages) go back to the controller as CC
  ([ctlout]), pickup avoids jumps.
Also writes pd/txpads.pd (the Launchkey pads, common/tools/ctpads.py) and pd/txnav.pd (the
pad translator for tx81zn~: pgm, the user bank as voicedata, common/tools/ctnav.py).
usage: python3 tools/gen_txcc.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))   # shared: ctlayer, patchwriter
import ctlayer                                   # noqa: E402
import ctnav                                     # noqa: E402
import ctpads                                    # noqa: E402
import tx81z_params as T                         # noqa: E402

# the instrument's own pads: random sounds (39-43 top row, 47-48 bottom row)
OWN_PADS = {39: "random pluck", 40: "random bass", 41: "random lead", 42: "random brass",
            43: "random pad", 47: "random strings", 48: "random total"}

# tx81zn~ keeps its user bank in Pd: [txnav] turns the pad messages into pgm / voicedata
NAV = {"factory_count": 128, "factory_msg": "pgm $1",
       "user_file": "txuser-bank.txt", "user_slots": 64,
       "get_msg": "getvoice", "reply": "voicedata", "recall": "voicedata", "init_msg": "init",
       "name": "voice"}

if __name__ == "__main__":
    out = os.path.join(ROOT, "pd", "txcc.pd")
    m = ctlayer.write(out, "txcc (tx81z~)", T.CC_ROWS)
    print("wrote", out, f"({len(m)} settings on CC)")
    ctpads.write(os.path.join(ROOT, "pd", "txpads.pd"), "txpads (tx81zn~)", ctpads.layout(OWN_PADS))
    ctnav.write(os.path.join(ROOT, "pd", "txnav.pd"), "txnav (tx81zn~)", NAV)
    print("wrote txpads.pd, txnav.pd")
