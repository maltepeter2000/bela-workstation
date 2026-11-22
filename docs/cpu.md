# CPU on the Bela Gem Multi

Measured with the Bela IDE's CPU display, block size 64, one Bela project per instrument
(`bela-projects/` of the branch `pd`, externals from `pd-externals`), the Launchkey connected.

| Project | Idle (no note) | Playing | Notes |
|---|---|---|---|
| empty (`[osc~]` -> `[dac~]`) | 16.9 % | - | the base: Pd, audio driver, IDE (2026-11-22) |
| tx81zn~ | 14 % | 22-24 % | a few notes held (2026-11-22) |
| juno~ | 19.4 % | 29 % | 6 voices held (2026-11-22) |

The base is paid once per Pd patch, not per instrument. Taken off (the readings vary by a few
percent, tx81zn~ idle even reads below the base):

| Instrument | Idle above the base | Playing above the base |
|---|---|---|
| tx81zn~ | about 0 % | about 5-7 % (a few notes) |
| juno~ | about 2.5 % | about 12 % (6 voices, about 1.6 % per voice) |

To do: tx81zn~ with all 8 voices, d50~, cz4~ and pdoly8~; the rack (several instruments in
one patch) once it exists, to see whether the shares add up.

Estimate for one Pd patch: base 17 % plus about 10-15 % per instrument playing at full voice
count, so four to five instruments playing at once fit with headroom, and instruments that do
not play cost almost nothing. The other cores of the Gem Multi are not used yet.
