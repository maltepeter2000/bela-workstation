# CPU on the Bela Gem Multi

Measured with the Bela IDE's CPU display, block size 64, one Bela project per instrument
(`bela-projects/` of the branch `pd`, externals from `pd-externals`), the Launchkey connected.

| Instrument | Idle (no note) | Playing | Notes |
|---|---|---|---|
| tx81zn~ | 14 % | 22-24 % | a few notes held (2026-11-22) |
| juno~ | 19.4 % | 29 % | 6 voices held (2026-11-22) |

To do:

- the base load of an empty project (only `[dac~]`), to tell the share of Pd and the audio
  driver from the instruments' own idle load;
- tx81zn~ with all 8 voices, d50~, cz4~ and pdoly8~;
- the rack (several instruments in one patch) once it exists.

Rough sum for one Pd patch: two of these playing at once are about 50 %, so three to four
instruments fit with headroom if not all of them play their full voice count at the same time.
The other cores of the Gem Multi are not used yet.
