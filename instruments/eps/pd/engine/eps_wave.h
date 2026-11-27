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
    NewLayer, CopyLayer, DeleteLayer, CopyParams,                                // COMMAND/Layer, Wave
    BidirXFade, LoopLonger, SynthLoop,                                           // COMMAND/Wave
    CopyData, AddData, Replicate, Mix, Merge, Splice,                            // COMMAND/LFO, Amp
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
    int toWs = 0;            // copy parameters, copy / add data, mix, merge, splice: the other one
    int balance = 0;         // mix, merge: -127 all source .. 0 equal .. 127 all destination
    bool clipPrevent = false;  // add data: scale the sum so that it does not clip
};
double scaleDepthDb(int depth);

// Run a command on wavesample ws of layer `layer` (its position). Returns false with
// a message (as the EPS shows it) if it cannot be done; selWs is the wavesample to
// select afterwards (the new one for new / copy, another or 0 after delete), selLayer
// (if given) the layer to select (the new one for new / copy layer, after delete the one before).
//
// The layer commands (COMMAND/Layer of the EPS): new layer (a layer with a small square wave,
// in patch 00 and played on key down), copy layer (into a new layer: its wavesamples as copies,
// with data: their own; in the same patches), delete layer (not the last one, not if a
// wavesample in another layer is a copy of one of it; the patches and key-up / key-down
// layers move with the layers). Copy parameters: all parameters of the wavesample to toWs,
// but not its data, points, loop mode, root key and tuning, rate, keys and name.
//
// More wave commands, after the EPS-16 PLUS External Command Specification (the descriptions
// there; not measured in the OS): bidir_xfade (both loop ends made symmetric over the fade
// zone, at most 50 % of the loop, for the bidirectional loop), loop_longer (Lengthen Loop: the
// loop is followed by itself reversed, the faded-out end and the faded-in reversed copy
// overlap over the fade zone: the loop gets 2L - F long), synth_loop (the loop resynthesized
// from its spectrum with random phases, the spectrum smoothed by `smooth`; at most 65536
// samples), copy_data / add_data (the range of the wavesample written over / added to toWs
// from its sample start; add with clip prevention: the sum scaled to fit), replicate_data (the
// range repeated after itself up to the end of the data), mix (Mix Wavesamples: this one, the
// source, mixed into toWs, balance -127 all source .. 127 all destination), merge (the source
// crossfaded into the start of toWs: source, then toWs, the fade zone % of the shorter one,
// before the loop of toWs) and splice (merge without fade). mix, merge and splice need the
// same rate, root key and tuning (PITCH DIFFERENCE).
// The EPS has no auto loop command: autoLoopSnap is its AUTO-LOOP FINDING, used when a loop
// point is edited.
bool autoLoopSnap(Instrument &in, int ws, bool end, int window = 256);
bool waveCommand(Instrument &in, int layer, int ws, WaveCmd cmd, const WaveArgs &a, int &selWs,
                 std::string &err, int *selLayer = nullptr);

} // namespace eps
