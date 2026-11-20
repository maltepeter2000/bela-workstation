# m1~ – Korg M1 (planned)

`m1~` will be a Korg M1 for Pure Data, built on the engine PicoFaceM1 of
[PicoVintageSynthCollection](https://github.com/Michi71/PicoVintageSynthCollection) by Michi71,
in the same way as `d50~` and `juno~` (see `instruments/d50`, `instruments/juno`). It is
developed here, not in pico-vintage-synths. No code yet.

## Decided so far (16 Nov 2026)

- Programs only for now, no Combinations.
- A user bank with 100 slots.
- Outputs 3/4 are handled later at the performance level (the rack), not in the object.
- Drum kits only by message.
- The performance row (first CC page) as proposed in the planning chat.
- ROMs: own M1 OS ROMs, revision 19 (IC32 and IC23). As with the D-50, data made from
  them stays out of the repository.

## What it has to meet

The instrument contract in `docs/instrument-contract.md`: `n` / `ninfo` / `info`, the
controller layer from `common/tools/ctlayer.py` (receive and send channel), the pad layout
and messages from `common/tools/ctpads.py` (`presetstep`, `userstep`, `init`, `save`),
`display <title> <value>`, an editor in rows of 8, a generated help patch, and
`make bela-gem`.
