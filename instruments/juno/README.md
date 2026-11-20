# juno~ – Roland Juno-60 / Juno-106

Pure Data external built on the engines of
[PicoVintageSynthCollection](https://github.com/Michi71/PicoVintageSynthCollection)
by Michi71 (native C++ re-implementations of vintage synthesizers for the RP2350).
Moved here with its history from `maltepeter2000/pico-vintage-synths`.

## juno~

A Juno-60 as one object: six voices, the switched high-pass, the IR3109
low-pass, the chorus, stereo out. It needs no data files. Patches are kept in
the format of the Juno-106, whose panel is nearly the same and which, unlike
the Juno-60, has MIDI — so Juno-106 patch collections play, and patches go to
a Juno-106 or an editor for it and back.

Inlets, outlets and messages are those of `d50~`; inlet 3 takes notes with
velocity, bend, program change, CC 1 7 64 and Juno-106 SysEx.

### Controls

33 controls, one name each (`tools/juno_params.py` is the single table):

| Page | CC | Controls |
|---|---|---|
| 1 VCF / ENV | 20–27 | `cutoff res env attack decay sustain release chorus` |
| 2 DCO | 28–31, 33–36 | `range pulse saw sub noise pwm pwmmode dcolfo` |
| 3 LFO / VCF / VCA | 37, 39–45 | `lforate lfodelay vcflfo kybd envpol hpf level vcamode` |
| 4 SETUP | 46–52 | `volume tune bend octave vellevel velcutoff model` |
| 5 VOICES | 53–54 | `calibrate chorusnoise` |

Pages 1 to 3 are the patch: the 16 sliders (0–127) and 8 switches of a
Juno-106. Pages 4 and 5 belong to the instrument and are not stored: `calibrate`
lets the six voices drift apart in tuning, cutoff, resonance and level as on an
instrument out of calibration, `chorusnoise` adds the hiss of the chorus lines. `<name> <value>`
sets a control, `n <name> <0-127>` sets it from a pot, and every change is
reported as `info <name> <0-127> <text>`. Edits are heard at once.
`[junocc <channel>]` is the controller layer, `[junoedit <bus>]` the editor.

### Juno-60 or Juno-106

`model` switches the instrument the patch plays on. `60` is the engine as it is: a Juno-60.
`106` is the Juno-106 as measured on Cherry Audio's DCO-106 with the kit in `juno~/test`
(`juno106test`, `juno106test2`): the cutoff curve and resonance (self-oscillation only from
about 104), the contour's depth (linear, 10 octaves) and times (decay and release up to about
15 s), LFO rate (0.1 to 20 Hz), delay and depth, the VCF LFO, the pulse width (down to 5 %), the
VCA level, the high-pass (position 0 a small bass lift, 1 flat, 2 and 3 at 226 and 720 Hz, as in
the Juno-106 service notes), the chorus (twice the rate over half the sweep), and the sub and
noise levels. In this model the contour and the LFO move in the steps of the Juno-106's
D/A cycle (4.2 ms). The model is the Juno-60 engine with remapped sliders (`juno~/juno106_tables.h`,
from `tools/make_juno106_tables.py`) and a few switches in the engine. Like the other setup
controls it is not stored in a patch.

### What differs from the instruments

| | Juno-60 | Juno-106 | `juno~` |
|---|---|---|---|
| Engine | | | Juno-60 (PicoFaceJ6) |
| Patch format, SysEx | none | 18 bytes | Juno-106 |
| Chorus | off, I, II, I+II | off, I, II | all four; I+II is stored in a bit a Juno-106 ignores, which plays it as I |
| Pulse width | LFO, manual, envelope | LFO, manual | LFO, manual |
| Sub oscillator | switch and level | level | level, 0 is off |
| Arpeggiator | yes | no | no |
| Velocity | no | no | optional: `vellevel`, `velcutoff`, 0 by default |
| Modulation wheel | no | vibrato | vibrato |
| Voices | 6 | 6 | 6, handed out in turn |

The chorus follows the chorus board of the Juno-60 service notes: each output
is the direct signal plus its own bucket-brigade line (full stereo width), the
line 1.2 times as loud as the direct signal, with the board's filters ahead of
and after the lines. As on the instrument, switching the chorus on makes the
sound louder.

The nine factory sounds that used pulse width from the envelope have the
fixed width instead, at the value it had while a key is held. The engine's
oscillator and filter run at twice the sample rate (`make OVERSAMPLE=1` for
none).

### Control pads

`[junopads <channel>]` (default 10) turns the Launchkey's pads in Drum mode into buttons
(`tools/ctpads.py`). With the Launchkey custom firmware the
pads send notes 36–43 (top row) and 44–51 (bottom row) on the Drums Channel:

| Pad | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| top (36–39) | factory + | user + | init (2×) | chorus I → II → I+II → off |
| bottom (44–47) | factory − | user − | save | Juno-106 / 60 |

`save` stores into the user slot chosen last and writes the bank file; `init` needs two presses
within a second. The messages behind them work on their own too: `presetstep 1|-1`,
`userstep 1|-1`, `save`, `cycle chorus`, `cycle model`. `cycle <name> [-1]` steps any control round and round.
Outlet 3 also says `display <title> <value>`: two lines for a display (a control set by a
message: `VCF freq` `64`, the patch that plays: `Preset 12` `Harpsichord 1`, `Stored` `User 11`).
`[lkdisplay]` (`tools/lkdisplay.py`) turns it into SysEx `F0 7D 4C 4B 01 <title> 00 <value> F7`
for `[midiout]`: the Launchkey custom firmware shows the two lines in its popup, so the display
names the parameter of the encoder you turn. The help patch and the Bela project use it. Set `juno~` to a channel other than the pads' (the help patch sends
`channel 1`), or the pads play notes.

### Patches

- `preset n`: 0–55 are the factory sounds of the Juno-60, from the chart in
  its owner's manual as transcribed in PicoFaceJ6. Every `.syx` file of
  Juno-106 patches in the folder of the external follows from 56 on.
- `user n`: the user bank, 56 slots as on the Juno-60 (shown as bank and
  number, 11–78). `store n` keeps the playing patch, `userwrite` writes
  `junouser-bank.syx`: 56 Juno-106 patch messages of 24 bytes.
- `usersend` and `sysexdump` send the bank or the playing patch as Juno-106
  SysEx. A Juno-106 plays a patch it receives but stores it only from its own
  panel, so after `usersend` it holds the last one.
- MIDI thru (`thru 1`, the default): what arrives at inlet 3 — notes, controllers, program
  changes, bend, pressure, clock and SysEx — leaves outlet 3 unchanged, and notes sent to
  inlet 1 go out on `txchannel`. Together with `sysexout 1` a Juno-106 (or an emulation) on
  the MIDI output plays along with `juno~`: same notes, same patch, every edit. `thru 0`
  when the output leads back to the input.
- A patch message (`F0 41 30 …`, `31`) replaces the playing patch, a
  parameter message (`F0 41 32 ch param value F7`) changes one byte.
  `sysexout 1` makes the object send those itself for every change, like a
  Juno-106.

### Build

```
cd juno~
make                 # this machine
make mac-zig         # universal macOS binary into ../bin/macos (zig, llvm-lipo)
make gen             # parameter header, controller layer, editor, help
```

## Layout

```
juno~/                the external, junocc.pd, junoedit.pd, junopads.pd, lkdisplay.pd, help patch, Makefile
juno~/engine/         PicoFaceJ6 engine, see its README for the changes
juno~/test/           Juno-106 comparison patches
bela/juno/            the Bela Gem project
bin/macos/            the Mac package
tools/                juno_params.py, generators, ctlayer.py, ctpads.py, lkdisplay.py
                      (gen_help.py and d50_params.py: shared with d50, used by gen_juno_help.py)
third_party/pd/       m_pd.h for the cross build
pd-lib-builder/       build helper (public domain, Katja Vetter)
```

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). The engines are from PicoVintageSynthCollection
(commit `642e69c`), Copyright (C) 2026 Michi71.

Roland, Juno-60 and Juno-106 are trademarks of Roland Corporation. This project is not affiliated with Roland.
