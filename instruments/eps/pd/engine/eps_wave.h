// eps_wave — the wavesample commands of the EPS-16 PLUS (Musician's Manual, section 6:
// COMMAND/Wave, COMMAND/LFO and COMMAND/Amp pages) for eps~: they change the data and
// points of the selected wavesample. The caller keeps the old instrument for the
// KEEP=OLD NEW audition.
//
// Data commands work on the data the wavesample plays (that of the wavesample it is a
// copy of, if it is one): copies change with it, as on the EPS. Their range is one of
// sample start..end, loop start..end, sample start..loop start, loop end..sample end.
//
// Not real-time safe (allocates); for the loader thread or Pd's scheduler.
#pragma once

#include <string>

#include "eps_loader.h"

namespace eps {

enum class WaveCmd {
    Truncate, CrossFade, ReverseXFade, EnsembleXFade, BowtieXFade, ConvertRate,   // COMMAND/Wave
    Normalize, FadeIn, FadeOut, Smooth,                                          // COMMAND/Amp
    Scale, Reverse, Invert, Clear,                                               // COMMAND/LFO
    NewWs, CopyWs, DeleteWs,                                                     // COMMAND/Wave
    Count
};
const char *waveCmdName(WaveCmd c);              // "truncate", "crossfade" ...

enum class WaveRange { Sample = 0, Loop = 1, StartToLoop = 2, LoopToEnd = 3 };
const char *waveRangeName(WaveRange r);

struct WaveArgs {
    int depth = 0;           // scale depth 0..5: 3.0 3.5 4.0 4.5 5.5 6.0 dB (default 3 dB)
    int zone = 50;           // fade zone, % of the loop (1..99; bowtie 1..50)
    WaveRange range = WaveRange::Sample;
    int rate = 1;            // convert to kRecordRateIndex[rate]
    int scaleStart = 100;    // scale data: multipliers at the start and end, % (0..200)
    int scaleEnd = 100;
    int smooth = 2;          // volume smoothing 0 very fine .. 4 very coarse
    int toLayer = 0;         // copy: the layer (position in Instrument::layers)
    bool withData = false;   // copy: params & data (else params only)
};
double scaleDepthDb(int depth);

// Run a command on wavesample ws of layer `layer` (its position). Returns false with
// a message (as the EPS shows it) if it cannot be done; selWs is the wavesample to
// select afterwards (the new one for new / copy, another or 0 after delete).
bool waveCommand(Instrument &in, int layer, int ws, WaveCmd cmd, const WaveArgs &a, int &selWs,
                 std::string &err);

} // namespace eps
