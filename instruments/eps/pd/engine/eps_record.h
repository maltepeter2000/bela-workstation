// eps_record — the wave recorder of eps~: a recording becomes a wavesample of the
// instrument, as on the EPS-16 PLUS (Musician's Manual, section 5: Sampling).
//
//   new wavesample   in the selected layer; it gets the keys from the midpoint to the
//                    root key of the next wavesample below to the midpoint to the next
//                    one above (the whole keyboard when it is the first)
//   replace          the selected wavesample; its keys and parameters stay, its loop
//                    points too when they fit into the new sample
//   new layer        a new layer (up to 8) with the recording on every key
//   new instrument   UNNAMED INST, as the OS creates it, with the recording on every key
//
// Not real-time safe (allocates); call from Pd's scheduler, not from the audio thread.
#pragma once

#include <string>
#include <vector>

#include "eps_loader.h"

namespace eps {

enum class RecordDest { NewWaveSample = 0, Replace = 1, NewLayer = 2, NewInstrument = 3 };

// The recording at an EPS sample rate: band-limited (windowed sinc, cutoff 0.45 of the
// lower rate, as the EPS's input filter follows the sample rate) and 16 bit.
std::vector<int16_t> resampleRecording(const std::vector<float> &in, double fromHz, double toHz);

// Put a recording (at the rate of rateIndex, see kRecordRateIndex) into the instrument.
// layer: position in Instrument::layers, ws: the wavesample to replace. Returns the
// number of the wavesample, 0 on error; layerOut = its layer's position.
int placeRecording(Instrument &in, RecordDest dest, int layer, int ws, const std::vector<int16_t> &pcm,
                   int rateIndex, int root, int &layerOut, std::string &err);

// Root key of a new wavesample: the keys of the layer between the midpoints to the
// neighbours' root keys (as the OS does after PLAY ROOT KEY).
void splitAtRoot(Instrument &in, int layer, int ws);

// The keys lo..hi of the layer play the wavesample. Keys it had below (above) the new
// range go to the wavesample of the key below (above) its old range, as when the split
// point moves on the EPS; none if there is no such key.
void setKeyRange(Instrument &in, int layer, int ws, int lo, int hi);

// WaveSample::keyLow / keyHigh of the layer's wavesamples from its key map.
void updateKeyRanges(Instrument &in, int layer);

// Free sample memory (words) beside the instrument: the EPS-16 PLUS has 1024K words.
constexpr long kSampleMemoryWords = 1024L * 1024L;
long freeWords(const Instrument &in);

} // namespace eps
