# Levels

Made by `common/tools/measure_levels.py` (run it again after changes, and on a machine with the ROMs for d50 and tx81z). Each instrument plays its first factory sounds with a C major chord at velocity 100; RMS of the held part, dBFS, at the instrument's default volume. **Trim** brings the median to -20 dBFS: the rack puts it in front of the mixer.

| Instrument | Sounds | Median RMS | Range | Peak | Trim dB | Note |
|---|---|---|---|---|---|---|
| juno | 32/32 | -23.9 | -61.6 .. -17.9 | -4.3 | +3.9 | built-in Juno-106 factory patches |
| d50 | - | - | - | - | - | not measured: needs d50_pcm.bin |
| cz4 | - | - | - | - | - | not measured: needs a factory bank (.syx) beside cz4~ |
| pdoly8 | 30/32 | -22.5 | -55.5 .. -14.1 | -0.9 | +2.5 | Poly-800 II factory bank through p8nav |
| tx81z | - | - | - | - | - | not measured: needs tx81z-v1.6.bin (ROM presets A01-) |
