# bela-workstation

A performance system for the Bela Gem Multi (`bela-multi.local`): several instruments and
effects in one rack patch, played from a Novation Launchkey with its display. Every
instrument is a Pure Data object that also runs on the desktop (Mac) with an editor and a
help patch.

## Layout

```
instruments/          one folder per instrument, each with its own history
  juno/               juno~    Roland Juno-60 / Juno-106 (from pico-vintage-synths)
  d50/                d50~     Roland D-50 (from pico-vintage-synths)
  pdoly8/             pdoly8~  Korg Poly-800 (from PDoly800)
  tx81z/              tx81zn~  Yamaha TX81Z, native engine; tx81z~ firmware emulation for
                               comparison (from tx81z-pd, branch native-engine)
  cz4/                cz4~     Casio CZ-1 as a full instrument; cz1~ the oscillator (from CZ4)
  m1/                 m1~      Korg M1, planned (no code yet)
effects/
  vrs23/              vrs23~   Dynacord VRS 23 echo / reverb, Pd vanilla
common/tools/         shared generators: ctlayer.py (controller layer), ctpads.py (pads and
                      their colours), ctnav.py (pad translator), lkdisplay.py (Launchkey
                      display), patchwriter.py
build/bela-gem/       make bela-gem: all instruments cross-built side by side (not in git)
launchkey/            the controller: CC pages, pads, display; lkdisplay.pd
docs/                 instrument-contract.md: what every instrument provides, state, next steps
```

Every instrument folder has the same layout:

```
Makefile      make, make mac-zig, make bela-gem, make gen, make clean
pd/           the Pd package: external source, help patch, controller layer <x>cc.pd, pads
              <x>pads.pd, editor, user bank - the folder to put on Pd's path
src/          engine code outside the Pd package (tx81z, cz4)
bin/macos/    the Mac package (universal, cross-built with make mac-zig)
bela/         Bela projects (_main.pd and its files; the externals are built into them)
tools/        generators (they import the shared pieces from common/tools)
tests/ docs/ third_party/
``` What is aligned and what is still open is in
[docs/instrument-contract.md](docs/instrument-contract.md).

## Building

The same targets in every instrument folder (`instruments/<name>`), or for all of them from the
top (`make <target>` here):

| | |
|---|---|
| `make` | build for this machine into `pd/` (Pd's `m_pd.h`: one is in each `third_party/pd`; juno and d50 take `PDINCLUDEDIR=…`, the others `PDINCLUDE=…`) |
| `make mac-zig` | universal macOS binaries (arm64 + x86_64) and the patches into `bin/macos` |
| `make bela-gem` | cross-built for the Bela Gem (aarch64, glibc 2.31) into `build/bela-gem/`, all instruments side by side (the rack's folder), and into the instrument's own Bela projects (juno: `bela/juno`, tx81z: `bela/tx81zn-pd`, `bela/tx81z-pd`) |
| `make gen` | the generated files: parameter header, controller layer, pads, editor, help, Bela patches |
| `make clean` | |

The cross builds need zig (`pip install ziglang`) and llvm-lipo. On the board itself a plain
`make` works too (tx81z: `bela/build_on_bela.sh`). The first Bela (armv7) still has its own
targets in pdoly8 and cz4 (`bela`, `bela-zig`).

All five build and load in Pd 0.54 on Linux, and the seven Gem binaries build without
errors and need only the board's libc (checked 20 Nov 2026; not yet run on the Gem).

`d50~` needs `d50_pcm.bin` made from your own D-50 ROMs (`tools/make_d50_data.py`),
`tx81zn~` the TX81Z firmware image `tx81z-v1.6.bin`. Neither is in the repository.

## History

The instruments were moved here with their full commit history on 20 Nov 2026:

| Folder | Source | Branch, commit |
|---|---|---|
| `instruments/tx81z` | maltepeter2000/tx81z-pd | native-engine, 220f710 (contains main and controller-layer) |
| `instruments/pdoly8` | maltepeter2000/PDoly800 | main, 8640362 |
| `instruments/cz4` | maltepeter2000/CZ4 | main, 72c9f00 |
| `instruments/juno`, `instruments/d50` | maltepeter2000/pico-vintage-synths | main, 7a916f8 |
| `effects/vrs23` | vrs23-pd.zip, 1 Nov 2026 | (not in a repository before) |

`git log --follow <file>` shows a file's history across the move. `m1~` stays in
pico-vintage-synths.
