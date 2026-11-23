# Rack

`rack/bela/rack4/_main.pd` (made by `gen_rack.py`, `make gen` at the top) runs juno~, d50~,
cz4~ and pdoly8~ in one Pd patch on the Bela Gem, mixed to outputs 1/2:

| Channel | Instrument | Controller layer | Trim |
|---|---|---|---|
| 1 | juno~ | junocc 1 | +3.9 dB |
| 2 | d50~ | d50cc 2 | 0 dB (not measured) |
| 3 | cz4~ | cz4cc 3 | 0 dB (not measured) |
| 4 | pdoly8~ | p8cc 4 (through p8nav) | +2.5 dB |

then the sum -6 dB. Change the Launchkey's Keys Channel to play another instrument; the
encoders (Custom 1) send on that channel too, so they follow. The pads (Drum mode, channel 10)
go to the instrument of the last note (focus); a focus change shows "Instrument <name>" and
sends that instrument's pad colours. All display lines go to one `[lkdisplay]`.

On the Bela: the branch `pd` in `/root/Bela/projects/pd-externals`, then copy
`bela-projects/rack4` to `/root/Bela/projects/` and put `d50_pcm.bin` (from your ROMs) into it;
user banks are written there on the first save.

Checked offline: the patch loads with the Bela folder layout, a note on channels 1, 2, 3, 4
plays juno~, d50~ (silent here without the PCM file), cz4~, pdoly8~. Not yet on the Bela.
