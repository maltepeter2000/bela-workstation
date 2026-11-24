#!/usr/bin/env python3
"""Generates the instrument objects of tx81z (common/tools/instobj.py):

    pd/tx81zn.i~.pd   tx81zn~ (native engine) with the FM editor txfm as its window, txedit (all
                      voice parameters) on the same bus, the controller layer txfmcc, the pads
                      txpads and lkdisplay
    pd/tx81z.i~.pd    tx81z~ (firmware emulation) with txedit and the controller layer txcc

    [tx81zn.i~ <channel> <pads channel>] -> [dac~]      [tx81z.i~ <channel>] -> [dac~]

Both read tx81z-v1.6.bin (and tx81z.nvram if it is there) beside the externals, and keep the
user bank txuser-bank.syx beside the patch that holds them. store writes it there as well;
tx81z~'s own save (the battery RAM) goes to the object unchanged.
Run after the abstractions are generated (make gen does).
usage: python3 tools/gen_tx_inst.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PD = os.path.join(os.path.dirname(HERE), "pd")
sys.path.insert(1, os.path.join(HERE, "..", "..", "..", "common", "tools"))
import instobj                                   # noqa: E402

BANK = [("userwrite", "userread", "txuser-bank.syx")]
ARGS = "tx81z-v1.6.bin tx81z.nvram"

if __name__ == "__main__":
    instobj.write(os.path.join(PD, "tx81zn.i~.pd"),
                  obj="tx81zn.i~", ext=f"tx81zn~ {ARGS}", short="tx81zn",
                  edit=os.path.join(PD, "txfm.pd"), edit_title="tx81zn~ FM editor",
                  cc=os.path.join(PD, "txfmcc.pd"),
                  pads=os.path.join(PD, "txpads.pd"),
                  lkdisplay=os.path.join(PD, "lkdisplay.pd"),
                  more_edits=[(os.path.join(PD, "txedit.pd"), "txedit")],
                  banks=BANK, save=True, store_writes=True, midi=("raw", 0))
    instobj.write(os.path.join(PD, "tx81z.i~.pd"),
                  obj="tx81z.i~", ext=f"tx81z~ {ARGS}", short="tx81z",
                  edit=os.path.join(PD, "txedit.pd"), edit_title="tx81z~ editor",
                  cc=os.path.join(PD, "txcc.pd"),
                  banks=BANK, save=False, store_writes=True, midi=("raw", 0))
