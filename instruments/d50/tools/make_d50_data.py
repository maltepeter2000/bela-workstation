#!/usr/bin/env python3
"""Prepare the D-50 data that d50~ needs, from your own ROM images.

    python3 tools/make_d50_data.py <romdir>

<romdir> holds the two PCM ROMs (IC30, IC29, or the combined chip of late
boards) and a program EPROM (IC22). Files are identified by content, so their
names do not matter. Any D-50 SysEx bulk dumps (*.syx) found there are copied
along as patch banks.

Writes, inside d50~/:

    d50_pcm.bin   the decoded samples, loaded by the external at run time
    *.syx         the patch banks, loaded at run time

Neither belongs in a repository: they are derived from Roland's ROMs.
Building the external does not need them, only playing it.
The conversion itself is tools/d5_extract/ of PicoVintageSynthCollection.
"""
import glob
import os
import shutil
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    romdir = os.path.abspath(sys.argv[1])
    here = os.path.dirname(os.path.abspath(__file__))
    dest = os.path.join(here, "..", "d50~")
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([sys.executable,
                        os.path.join(here, "d5_extract", "d5_make_blob.py"),
                        romdir, tmp], check=True)
        shutil.copy(os.path.join(tmp, "d5_pcm.bin"),
                    os.path.join(dest, "d50_pcm.bin"))
    banks = sorted(glob.glob(os.path.join(romdir, "*.syx")) +
                   glob.glob(os.path.join(romdir, "*.SYX")))
    for b in banks:
        shutil.copy(b, os.path.join(dest, os.path.basename(b)))
    print(f"d50~/d50_pcm.bin written; "
          f"{len(banks)} patch bank(s) copied")
    if not banks:
        print("no *.syx in the ROM folder: d50~ will play its eight built-in "
              "presets until a D-50 bulk dump sits beside d50_pcm.bin")


if __name__ == "__main__":
    main()
