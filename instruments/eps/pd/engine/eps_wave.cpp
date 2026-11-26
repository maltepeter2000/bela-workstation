// eps_wave.cpp — see eps_wave.h. The algorithms follow the descriptions in the
// Musician's Manual (fade zone, scale depth, ranges, the messages); the EPS's own code
// is not known, so results are alike, not bit-identical.
#include "eps_wave.h"
#include "eps_record.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace eps {

namespace {

constexpr int kTail = 8;   // zero samples after the data, as the OS writes them

int16_t clip(double v) { return int16_t(std::lround(std::clamp(v, -32768.0, 32767.0))); }

// Gain curve of a fade: f(0) = 0, f(1) = 1, f(0.5) = -depth dB (6 dB linear, 3 dB equal power).
double curve(double t, int depth) {
    t = std::clamp(t, 0.0, 1.0);
    const double p = scaleDepthDb(depth) / (20.0 * std::log10(2.0));
    return std::pow(t, p);
}

WaveSample *ownerOf(Instrument &in, WaveSample &w) {
    return const_cast<WaveSample *>(in.pcmOwner(w));
}

bool hasCopies(const Instrument &in, int owner) {
    for (const auto &kv : in.waveSamples)
        if (kv.first != owner && kv.second.copyOfWaveSample) {
            const WaveSample *o = in.pcmOwner(kv.second);
            if (o && o->index == owner) return true;
        }
    return false;
}

int freeIndex(const Instrument &in) {
    for (int i = 1; i < 128; ++i) if (!in.waveSamples.count(i)) return i;
    return 0;
}

int layerOfWs(const Instrument &in, int ws) {
    for (size_t l = 0; l < in.layers.size(); ++l)
        for (uint8_t k : in.layers[l].keyMap) if (k == ws) return int(l);
    return 0;
}

void syncPoints(WaveSample &w) {
    w.sampleStart = w.rawStart >> 9;
    w.sampleEnd = w.rawEnd >> 9;
    w.loopStart = w.rawLoopStart >> 9;
    w.loopEnd = (w.rawLoopEnd >> 9) + ((w.rawLoopEnd >> 5) & 0xF);
}

struct Pts { long ss, se, ls, le; };
Pts points(const WaveSample &w) {
    return {long(w.rawStart >> 9), long(w.rawEnd >> 9), long(w.rawLoopStart >> 9), long(w.rawLoopEnd >> 9)};
}

bool range(const WaveSample &w, WaveRange r, long n, long &lo, long &hi, std::string &err) {
    const Pts p = points(w);
    switch (r) {
        case WaveRange::Sample: lo = p.ss; hi = p.se; break;
        case WaveRange::Loop: lo = p.ls; hi = p.le; break;
        case WaveRange::StartToLoop: lo = p.ss; hi = p.ls; break;
        case WaveRange::LoopToEnd: lo = p.le; hi = p.se; break;
    }
    lo = std::clamp(lo, 0L, n);
    hi = std::clamp(hi, 0L, n);
    if (hi <= lo) { err = "DATA RANGE TOO SMALL"; return false; }
    return true;
}

// The loop cross fades. old: the data before; d: written.
bool crossFade(WaveCmd cmd, const WaveSample &w, std::vector<int16_t> &d, const WaveArgs &a, std::string &err) {
    const Pts p = points(w);
    const long n = long(d.size());
    if (p.le <= p.ls + 1 || p.le >= n) { err = "NO LOOP"; return false; }
    const long L = p.le - p.ls;
    const std::vector<int16_t> old = d;
    if (cmd == WaveCmd::BowtieXFade) {
        const long H = std::max(1L, L * std::clamp(a.zone, 1, 50) / 100);
        if (p.le + H > std::min(p.se, n - 1)) { err = "DECREMENT LOOPEND"; return false; }
        if (p.ls - H < p.ss) { err = "INCREMENT LOOPSTART"; return false; }
        // around both points the same blend of the data at loop end and at loop start:
        // before loop end it goes from the end's data towards the start's, after loop
        // start it continues there, so the jump from end to start joins one curve.
        for (long i = -H; i < H; ++i) {
            const double t = double(i + H) / double(2 * H);
            const double m = old[size_t(p.le + i)] * curve(1.0 - t, a.depth) + old[size_t(p.ls + i)] * curve(t, a.depth);
            if (i < 0) d[size_t(p.le + i)] = clip(m);
            else d[size_t(p.ls + i)] = clip(m);
        }
        return true;
    }
    const long F = cmd == WaveCmd::EnsembleXFade ? L : std::max(1L, L * std::clamp(a.zone, 1, 99) / 100);
    if (cmd != WaveCmd::ReverseXFade && p.ls - F < p.ss) { err = "INCREMENT LOOPSTART"; return false; }
    // the end of the loop fades into the data that leads up to loop start (reverse: the
    // data after loop start, backwards), so at loop end it is where loop start is
    for (long i = 0; i <= F; ++i) {
        const double t = double(i) / double(F);
        const long other = cmd == WaveCmd::ReverseXFade ? p.ls + (F - i) : p.ls - F + i;
        const double v = old[size_t(p.le - F + i)] * curve(1.0 - t, a.depth) + old[size_t(other)] * curve(t, a.depth);
        d[size_t(p.le - F + i)] = clip(v);
    }
    return true;
}

} // namespace

const char *waveCmdName(WaveCmd c) {
    static const char *const k[] = {"truncate", "crossfade", "reverse_xfade", "ensemble_xfade", "bowtie_xfade",
                                    "convert_rate", "normalize", "fade_in", "fade_out", "volume_smooth",
                                    "scale_data", "reverse_data", "invert_data", "clear_data",
                                    "new_ws", "copy_ws", "delete_ws"};
    const int i = int(c);
    return i >= 0 && i < int(WaveCmd::Count) ? k[i] : "?";
}

const char *waveRangeName(WaveRange r) {
    static const char *const k[] = {"sample", "loop", "start-loop", "loop-end"};
    return k[std::clamp(int(r), 0, 3)];
}

double scaleDepthDb(int depth) {
    static const double k[6] = {3.0, 3.5, 4.0, 4.5, 5.5, 6.0};
    return k[std::clamp(depth, 0, 5)];
}

bool waveCommand(Instrument &in, int layer, int ws, WaveCmd cmd, const WaveArgs &a, int &selWs, std::string &err) {
    selWs = ws;
    if (cmd == WaveCmd::NewWs) {
        if (layer < 0 || layer >= int(in.layers.size())) { err = "NO LAYER"; return false; }
        const int idx = freeIndex(in);
        if (!idx) { err = "127 WAVESAMPLES"; return false; }
        // "a small squarewave": one period of C4 at 44.6 kHz, four periods, looped
        WaveSample w = newWaveSample(idx);
        const int period = 171, n = 4 * period;
        for (int i = 0; i < n; ++i) w.pcm.push_back(int16_t((i % period) < period / 2 ? 16000 : -16000));
        w.pcm.resize(size_t(n + kTail), 0);
        w.loopMode = 2;
        w.rawStart = 0;
        w.rawEnd = uint32_t(n) << 9;
        w.rawLoopStart = uint32_t(n - period) << 9;
        w.rawLoopEnd = uint32_t(n) << 9;
        syncPoints(w);
        in.waveSamples[idx] = std::move(w);
        splitAtRoot(in, layer, idx);
        selWs = idx;
        return true;
    }
    auto it = in.waveSamples.find(ws);
    if (it == in.waveSamples.end()) { err = "NO WAVESAMPLE"; return false; }
    WaveSample &w = it->second;
    WaveSample *own = ownerOf(in, w);
    if (!own) { err = "NO DATA"; return false; }
    std::vector<int16_t> &d = own->pcm;
    const long n = long(d.size());

    switch (cmd) {
    case WaveCmd::DeleteWs: {
        if (hasCopies(in, ws)) { err = "WS HAS COPIES"; return false; }
        for (Layer &l : in.layers)
            for (uint8_t &k : l.keyMap) if (k == ws) k = 0;
        in.waveSamples.erase(it);
        selWs = 0;
        if (layer >= 0 && layer < int(in.layers.size()))
            for (uint8_t k : in.layers[size_t(layer)].keyMap) if (k) { selWs = k; break; }
        return true;
    }
    case WaveCmd::CopyWs: {
        if (a.toLayer < 0 || a.toLayer >= int(in.layers.size())) { err = "NO LAYER"; return false; }
        const int idx = freeIndex(in);
        if (!idx) { err = "127 WAVESAMPLES"; return false; }
        WaveSample c = w;
        c.index = idx;
        if (a.withData) {
            c.copyOfWaveSample = 0;
            c.copyOfLayer = 0;
            c.pcm = d;
        } else {
            c.copyOfWaveSample = own->index;
            c.copyOfLayer = layerOfWs(in, own->index);
            c.pcm.clear();
        }
        c.fileOffset = 0;
        c.imageWord = 0;
        // the copy plays the keys the original has in its layer
        Layer &to = in.layers[size_t(a.toLayer)];
        bool any = false;
        if (layer >= 0 && layer < int(in.layers.size()))
            for (int k = 0; k < 88; ++k)
                if (in.layers[size_t(layer)].keyMap[k] == ws) { to.keyMap[k] = uint8_t(idx); any = true; }
        if (!any)
            for (int k = std::max(0, w.keyLow - 21); k <= std::min(87, w.keyHigh - 21); ++k) to.keyMap[k] = uint8_t(idx);
        in.waveSamples[idx] = std::move(c);
        updateKeyRanges(in, a.toLayer);
        if (layer >= 0 && layer < int(in.layers.size())) updateKeyRanges(in, layer);
        selWs = idx;
        return true;
    }
    case WaveCmd::Truncate: {
        if (w.copyOfWaveSample) { err = "WS IS A COPY"; return false; }
        if (hasCopies(in, ws)) { err = "WS HAS COPIES"; return false; }
        const Pts p = points(w);
        const long lo = std::clamp(p.ss, 0L, n), hi = std::clamp(p.se, lo, n);
        if (hi - lo < 2) { err = "DATA RANGE TOO SMALL"; return false; }
        if (lo == 0 && hi + kTail >= n) { err = "NOTHING TO TRUNCATE"; return false; }
        std::vector<int16_t> t(d.begin() + lo, d.begin() + hi);
        t.resize(t.size() + kTail, 0);
        d.swap(t);
        const uint32_t off = uint32_t(lo) << 9;
        auto shift = [off](uint32_t &r) { r = r >= off ? r - off : 0; };
        shift(w.rawStart); shift(w.rawEnd); shift(w.rawLoopStart); shift(w.rawLoopEnd);
        const uint32_t end = uint32_t(hi - lo) << 9;
        w.rawLoopEnd = std::min(w.rawLoopEnd, end);
        w.rawLoopStart = std::min(w.rawLoopStart, w.rawLoopEnd);
        syncPoints(w);
        return true;
    }
    case WaveCmd::ConvertRate: {
        const int to = sampleRateOfIndex(kRecordRateIndex[std::clamp(a.rate, 0, 6)]);
        const int from = own->sampleRate ? own->sampleRate : sampleRateOfIndex(own->sampleRateIndex);
        if (to == from) { err = "SAME SAMPLE RATE"; return false; }
        // the data without the zeros after it, converted, the zeros again after it
        size_t used = d.size();
        while (used > 0 && d.size() - used < size_t(kTail) && d[used - 1] == 0) --used;
        std::vector<float> f(used);
        for (size_t i = 0; i < used; ++i) f[i] = d[i] / 32768.f;
        d = resampleRecording(f, from, to);
        const double r = double(to) / double(from);
        auto scale = [r](uint32_t &x) { x = uint32_t(std::llround(double(x) * r)); };
        for (auto &kv : in.waveSamples) {
            WaveSample &c = kv.second;
            if (in.pcmOwner(c) != own) continue;
            scale(c.rawStart); scale(c.rawEnd); scale(c.rawLoopStart); scale(c.rawLoopEnd);
            const uint32_t last = uint32_t(d.size()) << 9;
            c.rawEnd = std::min(c.rawEnd, last);
            c.rawLoopEnd = std::min(c.rawLoopEnd, last);
            c.rawLoopStart = std::min(c.rawLoopStart, c.rawLoopEnd);
            c.sampleRateIndex = kRecordRateIndex[std::clamp(a.rate, 0, 6)];
            c.sampleRate = to;
            syncPoints(c);
        }
        d.resize(d.size() + kTail, 0);
        return true;
    }
    case WaveCmd::CrossFade: case WaveCmd::ReverseXFade: case WaveCmd::EnsembleXFade: case WaveCmd::BowtieXFade:
        return crossFade(cmd, w, d, a, err);
    case WaveCmd::Normalize: {
        int peak = 0;
        for (int16_t v : d) peak = std::max(peak, std::abs(int(v)));
        if (!peak) { err = "NO DATA"; return false; }
        const double g = 32767.0 / peak;
        for (int16_t &v : d) v = clip(v * g);
        return true;
    }
    default:
        break;
    }
    long lo = 0, hi = 0;
    if (!range(w, a.range, n, lo, hi, err)) return false;
    const double len = double(hi - lo);
    switch (cmd) {
    case WaveCmd::FadeIn:
    case WaveCmd::FadeOut:
        for (long i = lo; i < hi; ++i) {
            const double t = double(i - lo) / len;
            d[size_t(i)] = clip(d[size_t(i)] * curve(cmd == WaveCmd::FadeIn ? t : 1.0 - t, a.depth));
        }
        return true;
    case WaveCmd::Scale: {
        const double s0 = std::clamp(a.scaleStart, 0, 200) / 100.0, s1 = std::clamp(a.scaleEnd, 0, 200) / 100.0;
        for (long i = lo; i < hi; ++i) {
            const double g = s0 + (s1 - s0) * curve(double(i - lo) / len, a.depth);
            d[size_t(i)] = clip(d[size_t(i)] * g);
        }
        return true;
    }
    case WaveCmd::Reverse:
        std::reverse(d.begin() + lo, d.begin() + hi);
        return true;
    case WaveCmd::Invert:
        for (long i = lo; i < hi; ++i) d[size_t(i)] = clip(-double(d[size_t(i)]));
        return true;
    case WaveCmd::Clear:
        std::fill(d.begin() + lo, d.begin() + hi, int16_t(0));
        return true;
    case WaveCmd::Smooth: {
        if (hi - lo < 256) { err = "DATA RANGE TOO SMALL"; return false; }
        // a level that follows the sound over a window (very fine: short), divided out
        static const long kWin[5] = {64, 128, 256, 512, 1024};
        const long W = kWin[std::clamp(a.smooth, 0, 4)];
        std::vector<double> sq(size_t(hi - lo) + 1, 0.0);
        for (long i = lo; i < hi; ++i) sq[size_t(i - lo + 1)] = sq[size_t(i - lo)] + double(d[size_t(i)]) * d[size_t(i)];
        const double target = std::sqrt(sq.back() / len);
        if (target < 1.0) { err = "NO DATA"; return false; }
        std::vector<int16_t> out(d.begin() + lo, d.begin() + hi);
        for (long i = lo; i < hi; ++i) {
            const long a0 = std::max(lo, i - W / 2), a1 = std::min(hi, i + W / 2 + 1);
            const double rms = std::sqrt((sq[size_t(a1 - lo)] - sq[size_t(a0 - lo)]) / double(a1 - a0));
            const double g = std::clamp(target / std::max(rms, target * 0.05), 0.0, 8.0);
            out[size_t(i - lo)] = clip(d[size_t(i)] * g);
        }
        std::copy(out.begin(), out.end(), d.begin() + lo);
        return true;
    }
    default:
        err = "UNKNOWN COMMAND";
        return false;
    }
}

} // namespace eps
