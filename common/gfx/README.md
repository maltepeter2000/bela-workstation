# Launchkey pictures (common/gfx)

When a setting with a picture is turned, the instruments draw it on the display of the Launchkey Mini MK4
instead of the text line: the value stays readable as a line of text above the picture. This needs the
custom firmware branch `display-gfx` (maltepeter2000/launchkey_firmware, a 128 × 48 picture,
`cfw/lk_gfx.h`). With the stock firmware or `lkgfx 0` the text line comes as before.

| Instrument | Picture | Shown when turning |
|---|---|---|
| pdoly8~ | DEG shape | `env1`–`env3`, `a1` … `r3` |
| | wave of the timbre | `har1`, `har2` |
| | filter curve (24 dB) | `cut`, `res` |
| | MG over time (wave, rate, delay fade-in) | `mgr`, `mgd`, `mgw`, `mgk` |
| cz4~ | 8-step envelope of the selected line and envelope; sustain dotted, turned step marked | `r1`–`r8`, `l1`–`l8`, `sus`, `end`, `env`, `line`, easy envelope |
| | wave at DCW 0 (dotted) and at the DCW envelope's peak, wave 2 beside it | `wave`, `wave2`, `window` |
| tx81zn~ | algorithm (operators, connections, OP4 feedback) | `alg`, `fb`, FM page `algo`, `fdbk` |
| | operator envelope (AR, D1R, D1L, D2R, RR; dotted = key up) | `ar1` … `d1l4`, FM page envelope controls |
| | operator wave W1–W8 | `osw1`–`osw4`, FM page `wavc` `wava` `wavb` |
| juno~ | ADSR | `attack`, `decay`, `sustain`, `release` |
| | filter curve (24 dB, HPF 2/3) | `cutoff`, `res`, `hpf` |
| | DCO wave (saw, pulse with PWM in MAN mode, sub, noise) | `pulse`, `saw`, `sub`, `pwm`, `pwmmode`, `noise` |
| d50~ | TVA / TVF envelope of the selected partial | `tvat1` … `tvaend`, `att`, `rel`, `tvft1` … `tvfend` |
| | pitch envelope | `penvt1` … `penvend` |
| | structure (S / P partials, ring), partials switched off dotted | `structure`, `mute` |
| | filter curve | `cutoff`, `reso`, `cut`, `res` |
| | synth wave (square with pulse width, saw) | `wave`, `pw` |

Every instrument understands `lkgfx 0` (text line only) and `lkgfx 1` (default). The picture goes out on
the instrument's message outlet as `lkgfx <bytes>`, two messages a millisecond, only the 32-column pieces
that changed, at most one picture every 50 ms; `[lkdisplay]` passes it to `[midiout]`.

## Code

- `lkgfx.h`: the bitmap and drawing (5 × 7 capitals, lines, dotted lines, boxes, an envelope of any number
  of segments with a sustain plateau, curves, a filter response), the SysEx pieces. C and C++, header only.
- `lkgfx_pd.h`: the Pd side: throttling, the queue, the outlet, `lkg_pd_show(p, kind, label, value)`.
  The instrument gives a draw function `draw(owner, g, kind)`.
- The TX81Z pictures are in `instruments/tx81z/pd/txgfx.h` (host test `tests/test_txgfx.cpp`).

`common/tools/lkgfx_view.py <dir> <object> -- "msg" ...` runs an external in Pd without a GUI, sends it the
messages and prints the pictures it sends as text (`--png out.png` for a picture).
