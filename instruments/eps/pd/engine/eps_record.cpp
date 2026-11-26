// eps_record.cpp — see eps_record.h.
#include "eps_record.h"

#include <algorithm>
#include <cmath>

namespace eps {

namespace {

constexpr int kFirst = Layer::kFirstNote;   // key 0 of the key map = A0 (MIDI 21)
constexpr int kKeys = 88;
constexpr int kTail = 8;                    // the OS puts 8 zero samples after a recording

bool inLayer(const Layer &l, int ws) {
    for (uint8_t k : l.keyMap) if (k == ws) return true;
    return false;
}

int freeIndex(const Instrument &in) {
    for (int i = 1; i < 128; ++i) if (!in.waveSamples.count(i)) return i;
    return 0;
}

void setPoints(WaveSample &w, uint32_t n, bool keepLoop) {
    w.rawStart = 0;
    w.rawEnd = n << 9;
    if (!keepLoop || (w.rawLoopEnd >> 9) > n || w.rawLoopStart > w.rawLoopEnd) {
        w.rawLoopStart = 0;
        w.rawLoopEnd = n << 9;
    }
    w.sampleStart = 0;
    w.sampleEnd = n;
    w.loopStart = w.rawLoopStart >> 9;
    w.loopEnd = (w.rawLoopEnd >> 9) + ((w.rawLoopEnd >> 5) & 0xF);
}

} // namespace

std::vector<int16_t> resampleRecording(const std::vector<float> &in, double from, double to) {
    std::vector<int16_t> out;
    if (in.empty() || from <= 0 || to <= 0) return out;
    const double ratio = from / to;                         // input samples per output sample
    const double fc = 0.45 * std::min(1.0, to / from);      // cutoff, cycles per input sample
    const int half = int(std::ceil(16.0 / (2.0 * fc)));     // 16 zero crossings each side
    const size_t n = size_t(std::floor(double(in.size()) / ratio));
    out.resize(n);
    for (size_t j = 0; j < n; ++j) {
        const double c = double(j) * ratio;
        const long k0 = long(std::floor(c));
        double acc = 0, norm = 0;
        for (long k = k0 - half + 1; k <= k0 + half; ++k) {
            const double t = c - double(k);
            const double x = 2.0 * fc * t;
            const double sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
            const double win = 0.42 + 0.5 * std::cos(M_PI * t / half) + 0.08 * std::cos(2 * M_PI * t / half);
            const double h = std::fabs(t) >= half ? 0.0 : sinc * win;
            norm += h;
            if (k >= 0 && k < long(in.size())) acc += h * in[size_t(k)];
        }
        const double v = norm > 0 ? acc / norm : 0.0;
        out[j] = int16_t(std::lround(std::clamp(v, -1.0, 32767.0 / 32768.0) * 32768.0));
    }
    return out;
}

long freeWords(const Instrument &in) {
    long used = long(in.image.size());
    if (used == 0)
        for (const auto &kv : in.waveSamples) used += long(kv.second.pcm.size()) + 144;
    return std::max(0L, kSampleMemoryWords - used);
}

void updateKeyRanges(Instrument &in, int layer) {
    if (layer < 0 || layer >= int(in.layers.size())) return;
    const Layer &l = in.layers[size_t(layer)];
    for (auto &kv : in.waveSamples) {
        int lo = -1, hi = -1;
        for (int k = 0; k < kKeys; ++k)
            if (l.keyMap[k] == kv.first) { if (lo < 0) lo = k; hi = k; }
        if (lo >= 0) { kv.second.keyLow = kFirst + lo; kv.second.keyHigh = kFirst + hi; }
    }
}

void splitAtRoot(Instrument &in, int layer, int ws) {
    if (layer < 0 || layer >= int(in.layers.size())) return;
    auto it = in.waveSamples.find(ws);
    if (it == in.waveSamples.end()) return;
    Layer &l = in.layers[size_t(layer)];
    const int r = it->second.rootNote;
    int below = -1, above = 1000;
    for (const auto &kv : in.waveSamples) {
        if (kv.first == ws || !inLayer(l, kv.first)) continue;
        const int o = kv.second.rootNote;
        if (o < r) below = std::max(below, o);
        if (o > r) above = std::min(above, o);
    }
    const int lo = below < 0 ? kFirst : (below + r) / 2 + 1;
    const int hi = above > 127 ? kFirst + kKeys - 1 : (r + above) / 2;
    for (int k = std::max(lo, kFirst); k <= std::min(hi, kFirst + kKeys - 1); ++k) l.keyMap[k - kFirst] = uint8_t(ws);
    updateKeyRanges(in, layer);
}

void setKeyRange(Instrument &in, int layer, int ws, int lo, int hi) {
    if (layer < 0 || layer >= int(in.layers.size()) || !in.waveSamples.count(ws)) return;
    Layer &l = in.layers[size_t(layer)];
    lo = std::clamp(lo, kFirst, kFirst + kKeys - 1) - kFirst;
    hi = std::clamp(hi, kFirst, kFirst + kKeys - 1) - kFirst;
    if (lo > hi) std::swap(lo, hi);
    int oldLo = -1, oldHi = -1;
    for (int k = 0; k < kKeys; ++k)
        if (l.keyMap[k] == ws) { if (oldLo < 0) oldLo = k; oldHi = k; }
    if (oldLo >= 0) {
        const uint8_t below = oldLo > 0 && l.keyMap[oldLo - 1] != ws ? l.keyMap[oldLo - 1] : 0;
        const uint8_t above = oldHi < kKeys - 1 && l.keyMap[oldHi + 1] != ws ? l.keyMap[oldHi + 1] : 0;
        for (int k = oldLo; k < lo; ++k) if (l.keyMap[k] == ws) l.keyMap[k] = below;
        for (int k = hi + 1; k <= oldHi; ++k) if (l.keyMap[k] == ws) l.keyMap[k] = above;
    }
    for (int k = lo; k <= hi; ++k) l.keyMap[k] = uint8_t(ws);
    updateKeyRanges(in, layer);
}

int placeRecording(Instrument &in, RecordDest dest, int layer, int ws, const std::vector<int16_t> &pcm,
                   int rateIndex, int root, int &layerOut, std::string &err) {
    layerOut = layer;
    if (pcm.empty()) { err = "nothing recorded"; return 0; }
    if (dest == RecordDest::NewInstrument) {
        newInstrument(in);
        in.layers.push_back(newLayer(1));
        layer = 0;
        dest = RecordDest::NewLayer;
        in.layers[0].index = 1;
    } else if (dest == RecordDest::NewLayer) {
        if (in.layers.size() >= 8) { err = "8 layers already"; return 0; }
        in.layers.push_back(newLayer(int(in.layers.size()) + 1));
        layer = int(in.layers.size()) - 1;
        in.patches[0] |= 1 << layer;
        in.keyDownLayers |= 1 << layer;
    }
    if (dest != RecordDest::NewInstrument && dest != RecordDest::NewLayer &&
        (layer < 0 || layer >= int(in.layers.size()))) { err = "no such layer"; return 0; }
    layerOut = layer;
    const uint32_t n = uint32_t(pcm.size());
    if (dest == RecordDest::Replace) {
        auto it = in.waveSamples.find(ws);
        if (it == in.waveSamples.end()) { err = "no wavesample to replace"; return 0; }
        WaveSample &w = it->second;
        w.copyOfWaveSample = 0;
        w.copyOfLayer = 0;
        w.pcm = pcm;
        w.pcm.resize(pcm.size() + kTail, 0);
        w.sampleRateIndex = rateIndex;
        w.sampleRate = sampleRateOfIndex(rateIndex);
        w.rootNote = root;
        setPoints(w, n, true);
        return ws;
    }
    const int idx = freeIndex(in);
    if (!idx) { err = "127 wavesamples already"; return 0; }
    WaveSample w = newWaveSample(idx);
    w.pcm = pcm;
    w.pcm.resize(pcm.size() + kTail, 0);
    w.sampleRateIndex = rateIndex;
    w.sampleRate = sampleRateOfIndex(rateIndex);
    w.rootNote = root;
    setPoints(w, n, false);
    in.waveSamples[idx] = std::move(w);
    splitAtRoot(in, layer, idx);
    return idx;
}

} // namespace eps
