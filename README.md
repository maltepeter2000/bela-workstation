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
common/tools/         shared generators: ctlayer.py (controller layer), ctpads.py (pads),
                      lkdisplay.py (Launchkey display), patchwriter.py
launchkey/            the controller: CC pages, pads, display; lkdisplay.pd
docs/                 instrument-contract.md: what every instrument provides, state, next steps
```

Each instrument folder keeps the layout of the repository it came from (`tools/`,
`bin/macos/`, its Bela project, `third_party/`), so its Makefile and generators run
unchanged from inside the folder. Its generators import the shared pieces from
`common/tools`. What is aligned and what is still open is in
[docs/instrument-contract.md](docs/instrument-contract.md).

## Building

Inside an instrument folder, with Pd's `m_pd.h` (one is in `instruments/juno/third_party/pd`):

| Instrument | Desktop / Linux | Mac package | Bela Gem (aarch64), cross build | Output |
|---|---|---|---|---|
| juno | `make -C juno~ PDINCLUDEDIR=…` | `make -C juno~ mac-zig` | `make -C juno~ bela-gem` | `bela/juno` |
| d50 | `make -C d50~ PDINCLUDEDIR=…` | `make -C d50~ mac-zig` | `make -C d50~ bela-gem` | `bela/d50` |
| pdoly8 | `make PDINCLUDE=…` | `make mac-zig` | `make bela-gem` | `bela-gem/` |
| tx81z | `make -C pd` | – | `make -C pd bela-gem` | `bela-pd/tx81zn-pd`, `bela-pd/tx81z-pd` |
| cz4 | `make PDINCLUDE=…` | `make mac-zig` | `make bela-gem` | `bela/cz4` |

The cross builds need zig (`pip install ziglang`). On the board itself a plain `make` works
too (tx81z: `bela-pd/build_on_bela.sh`). The first Bela (armv7) still has its own targets in
pdoly8 and cz4 (`bela`, `bela-zig`).

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
