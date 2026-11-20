# juno~ on Bela

`juno/` is a Bela Pd project (made for the Bela Gem, 64-bit ARM): `[juno~]` with MIDI in, the
controller layer, the Launchkey pads, the OLED and an OSC remote. No GUI. `_main.pd` is generated
by `tools/gen_juno_bela.py`, `juno~.pd_linux` is cross-built with `make bela-gem` (in the instrument folder).

| File | |
|---|---|
| `_main.pd` | the patch |
| `juno~.pd_linux` | the external for the Bela Gem (aarch64) |
| `junocc.pd` | controller layer (CC pages from 20, same as on the desktop) |
| `junopads.pd` | the Launchkey pads in Drum mode |
| `junouser-bank.syx` | the user bank, written on the first save (Juno-106 SysEx, same file as on the desktop) |

## Setup

```
scp -r bela/juno root@bela-multi.local:~/Bela/projects/
```

Open the project `juno` in the IDE and click Run. Bela opens its MIDI ports itself (the keyboard on
the USB host port). A user bank from the desktop can be copied in beside the patch:
`scp junouser-bank.syx root@bela-multi.local:~/Bela/projects/juno/`.

An older Bela (32-bit, Cortex-A8) needs the external built on the board: copy the repository there
and run `make` in `instruments/juno`, then copy `juno~.pd_linux` into the project.

## Playing

- Channel 1: notes, bend, mod wheel (vibrato), CC 7 volume, CC 64 hold, program change.
- CC 20 on, channel 1: the controller layer, 5 pages of 8, with feedback to the controller
  (the Launchkey's encoders in Custom 1 with the custom firmware).
- Channel 10 (the Launchkey's Drums Channel), pads in Drum mode:

| Pad | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| top (36–39) | factory + | user + | init (2×) | chorus I → II → I+II → off |
| bottom (44–47) | factory − | user − | save | Juno-106 / 60 |

- Audio on outputs 1 / 2.

## OLED (O2O)

One line (`/display-strings-and-numbers` to 127.0.0.1:7562), juno~'s `display` message: a turned
control (`VCF freq 64`), the patch that plays (`P12 Harpsichord 1`, `U14 ...`), `stored U11`.

## OSC remote (UDP 7563)

`/juno/<message> <args>` goes to juno~ as `<message> <args>`: `/juno/preset 12`,
`/juno/cutoff 64`, `/juno/n res 100`, `/juno/model 106`, `/juno/save`.

## MIDI out

Outlet 3 goes to `[midiout]`, the first MIDI port (the keyboard). MIDI thru and SysEx out are off
at load, so nothing echoes to the keyboard; `/juno/thru 1` and `/juno/sysexout 1` switch them on.
Bela passes no SysEx to Pd, so SysEx from a Juno-106 does not arrive here.

## CPU

About 21–27 % with 6 voices on the Bela Gem at block size 64. Every MIDI byte Pd sends is a
system call in Bela's audio thread, so `[lkdisplay]` sends at most one display message per 40 ms;
the controller feedback and SysEx out (off at load) cost the same per byte.

Built with 2× oversampling (as on the desktop). If the Bela runs short:
`make bela-gem BELA_OS=1` builds it without.
