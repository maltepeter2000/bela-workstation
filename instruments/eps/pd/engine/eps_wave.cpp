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

// In-place radix-2 FFT (n a power of two), inverse with inv.
void fft(std::vector<double> &re, std::vector<double> &im, bool inv) {
    const size_t n = re.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double a = 2.0 * M_PI / double(len) * (inv ? 1.0 : -1.0);
        for (size_t i = 0; i < n; i += len)
            for (size_t k = 0; k < len / 2; ++k) {
                const double cr = std::cos(a * double(k)), ci = std::sin(a * double(k));
                const size_t u = i + k, v = i + k + len / 2;
                const double xr = re[v] * cr - im[v] * ci, xi = re[v] * ci + im[v] * cr;
                re[v] = re[u] - xr; im[v] = im[u] - xi;
                re[u] += xr; im[u] += xi;
            }
    }
    if (inv) for (size_t i = 0; i < n; ++i) { re[i] /= double(n); im[i] /= double(n); }
}

// The commands that change the length of the data: the points of the wavesample move with it.
void shiftAfter(WaveSample &w, long at, long by) {
    auto mv = [at, by](uint32_t &r) {
        if (long(r >> 9) >= at) r = uint32_t(std::max(0L, long(r >> 9) + by)) << 9 | (r & 0x1FF);
    };
    mv(w.rawStart); mv(w.rawEnd); mv(w.rawLoopStart); mv(w.rawLoopEnd);
    syncPoints(w);
}

bool moreCommands(Instrument &in, int ws, WaveCmd cmd, const WaveArgs &a, int &selWs, std::string &err) {
    auto it = in.waveSamples.find(ws);
    if (it == in.waveSamples.end()) { err = "NO WAVESAMPLE"; return false; }
    WaveSample &w = it->second;
    WaveSample *own = ownerOf(in, w);
    if (!own) { err = "NO DATA"; return false; }
    std::vector<int16_t> &d = own->pcm;
    const long n = long(d.size());
    const Pts p = points(w);
    const bool changesLength = cmd == WaveCmd::LoopLonger || cmd == WaveCmd::Merge || cmd == WaveCmd::Splice;
    if (changesLength && w.copyOfWaveSample) { err = "WS IS A COPY"; return false; }
    if (changesLength && hasCopies(in, ws)) { err = "WS HAS COPIES"; return false; }
    switch (cmd) {
    case WaveCmd::BidirXFade: {
        if (p.le <= p.ls + 1 || p.le >= n) { err = "NO LOOP"; return false; }
        const long L = p.le - p.ls;
        const long F = std::max(1L, L * std::clamp(a.zone, 1, 50) / 100);
        if (p.le + F >= n) { err = "DECREMENT LOOPEND"; return false; }
        if (p.ls - F < 0) { err = "INCREMENT LOOPSTART"; return false; }
        const std::vector<int16_t> old = d;
        // around each end the data becomes the mean of itself and its mirror image, fully at
        // the end, fading out over the zone: the turn at either end has no corner
        for (long k = 0; k <= F; ++k) {
            const double g = curve(1.0 - double(k) / double(F), a.depth);
            const double e = 0.5 * (old[size_t(p.le - k)] + old[size_t(p.le + k)]);
            d[size_t(p.le - k)] = clip(old[size_t(p.le - k)] * (1.0 - g) + e * g);
            const double s = 0.5 * (old[size_t(p.ls + k)] + old[size_t(p.ls - k)]);
            d[size_t(p.ls + k)] = clip(old[size_t(p.ls + k)] * (1.0 - g) + s * g);
        }
        w.loopMode = 3;
        return true;
    }
    case WaveCmd::LoopLonger: {
        if (p.le <= p.ls + 1 || p.le > n) { err = "NO LOOP"; return false; }
        const long L = p.le - p.ls;
        if (n + L > (1L << 21)) { err = "LOOP TOO LONG"; return false; }
        std::vector<int16_t> loop(d.begin() + p.ls, d.begin() + p.le);
        d.insert(d.begin() + p.le, loop.begin(), loop.end());
        const uint32_t le = w.rawLoopEnd, ls = w.rawLoopStart;
        shiftAfter(w, p.le, L);
        w.rawLoopStart = ls;                         // the start stays, the end moves on
        w.rawLoopEnd = le + (uint32_t(L) << 9);
        syncPoints(w);
        return true;
    }
    case WaveCmd::SynthLoop: {
        if (p.le <= p.ls + 16 || p.le > n) { err = "NO LOOP"; return false; }
        const long L = p.le - p.ls;
        if (L > 65536) { err = "LOOP LENGTH TOO LONG"; return false; }
        size_t N = 1;
        while (N < size_t(L)) N <<= 1;
        std::vector<double> re(N, 0.0), im(N, 0.0);
        double rms = 0;
        for (long i = 0; i < L; ++i) {
            const double v = d[size_t(p.ls + i)];
            rms += v * v;
            re[size_t(i) * N / size_t(L)] += v * (0.5 - 0.5 * std::cos(2.0 * M_PI * (double(i) + 0.5) / double(L)));
        }
        rms = std::sqrt(rms / double(L));
        fft(re, im, false);
        uint32_t seed = 0x2545F491u;
        auto rnd = [&seed]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed / 4294967296.0; };
        re[0] = im[0] = 0.0;                         // no offset
        for (size_t k = 1; k < N / 2; ++k) {
            const double m = std::hypot(re[k], im[k]), ph = 2.0 * M_PI * rnd();
            re[k] = m * std::cos(ph); im[k] = m * std::sin(ph);
            re[N - k] = re[k]; im[N - k] = -im[k];
        }
        re[N / 2] = im[N / 2] = 0.0;
        fft(re, im, true);
        // periodic over N, read back over L (wrapping): periodic over the loop
        double r2 = 0;
        std::vector<double> y(static_cast<size_t>(L));
        for (long i = 0; i < L; ++i) {
            const double pos = double(i) * double(N) / double(L);
            const size_t i0 = size_t(pos) % N, i1 = (i0 + 1) % N;
            const double f = pos - std::floor(pos);
            y[size_t(i)] = re[i0] * (1.0 - f) + re[i1] * f;
            r2 += y[size_t(i)] * y[size_t(i)];
        }
        r2 = std::sqrt(r2 / double(L));
        if (r2 < 1e-9 || rms < 1.0) { err = "NO DATA"; return false; }
        for (long i = 0; i < L; ++i) d[size_t(p.ls + i)] = clip(y[size_t(i)] * rms / r2);
        return true;
    }
    case WaveCmd::CopyData: case WaveCmd::MixData: {
        auto jt = in.waveSamples.find(a.toWs);
        if (jt == in.waveSamples.end() || a.toWs == ws) { err = "NO DESTINATION"; return false; }
        WaveSample *to = ownerOf(in, jt->second);
        if (!to || to == own) { err = "SAME DATA"; return false; }
        long lo = 0, hi = 0;
        if (!range(w, a.range, n, lo, hi, err)) return false;
        const std::vector<int16_t> src(d.begin() + lo, d.begin() + hi);
        std::vector<int16_t> &t = to->pcm;
        const long at = long(jt->second.rawStart >> 9);
        if (at + long(src.size()) + kTail > long(t.size())) t.resize(size_t(at + long(src.size()) + kTail), 0);
        for (size_t i = 0; i < src.size(); ++i) {
            int16_t &o = t[size_t(at) + i];
            o = cmd == WaveCmd::CopyData ? src[i] : clip(double(o) + double(src[i]));
        }
        WaveSample &tw = jt->second;
        if (long(tw.rawEnd >> 9) < at + long(src.size())) tw.rawEnd = uint32_t(at + long(src.size())) << 9;
        syncPoints(tw);
        selWs = a.toWs;
        return true;
    }
    case WaveCmd::AutoLoop: {
        // the loop end nearest to the old one (within the fade zone of the loop length, at most
        // 8192 samples either way) where the data around it is most like the data around loop
        // start: the jump back is as small as it can be, in level and in slope
        if (p.le <= p.ls + 64 || p.le > n) { err = "NO LOOP"; return false; }
        const long L = p.le - p.ls, W = 32;
        const long R = std::min(8192L, std::max(64L, L * std::clamp(a.zone, 1, 99) / 100));
        const long lo = std::max(p.ls + 64, p.le - R), hi = std::min({p.le + R, std::min(p.se, n) - W - 1});
        if (hi <= lo || p.ls < W) { err = "NO ROOM"; return false; }
        double best = 1e300;
        long at = p.le;
        for (long c = lo; c <= hi; ++c) {
            double e = 0;
            for (long k = -W; k < W; ++k) {
                const double dv = double(d[size_t(c + k)]) - double(d[size_t(p.ls + k)]);
                e += dv * dv * (k >= -4 && k < 4 ? 4.0 : 1.0);   // the samples at the joint count most
            }
            e *= 1.0 + 0.5 * double(std::labs(c - p.le)) / double(R);   // near the old end wins a tie
            if (e < best) { best = e; at = c; }
        }
        w.rawLoopEnd = uint32_t(at) << 9;
        syncPoints(w);
        return true;
    }
    case WaveCmd::Merge: case WaveCmd::Splice: {
        auto jt = in.waveSamples.find(a.toWs);
        if (jt == in.waveSamples.end() || a.toWs == ws) { err = "NO DESTINATION"; return false; }
        const WaveSample &b = jt->second;
        const WaveSample *bo = in.pcmOwner(b);
        if (!bo) { err = "NO DATA"; return false; }
        const int ra = own->sampleRate ? own->sampleRate : sampleRateOfIndex(own->sampleRateIndex);
        const int rb = bo->sampleRate ? bo->sampleRate : sampleRateOfIndex(bo->sampleRateIndex);
        if (ra != rb || w.rootNote != b.rootNote || w.fineTune != b.fineTune) { err = "PITCH DIFFERENCE"; return false; }
        const Pts q = points(b);
        const long a0 = std::clamp(p.ss, 0L, n), a1 = std::clamp(p.se, a0, n);
        const long nb = long(bo->pcm.size());
        const long b0 = std::clamp(q.ss, 0L, nb), b1 = std::clamp(q.se, b0, nb);
        const long la = a1 - a0, lb = b1 - b0;
        if (la < 2 || lb < 2) { err = "DATA RANGE TOO SMALL"; return false; }
        const long F = cmd == WaveCmd::Splice ? 0 : std::max(1L, std::min(la, lb) * std::clamp(a.zone, 1, 99) / 100);
        std::vector<int16_t> out(d.begin() + a0, d.begin() + a1 - F);
        for (long i = 0; i < F; ++i) {
            const double t = double(i) / double(F);
            out.push_back(clip(d[size_t(a1 - F + i)] * curve(1.0 - t, a.depth) + bo->pcm[size_t(b0 + i)] * curve(t, a.depth)));
        }
        out.insert(out.end(), bo->pcm.begin() + b0 + F, bo->pcm.begin() + b1);
        const long off = la - F;                     // where the second one starts
        // the loop of the second one, else the first one's
        const bool bLoop = b.loopMode >= 2 && q.le > q.ls;
        w.rawStart = 0;
        w.rawEnd = uint32_t(out.size()) << 9;
        if (bLoop) {
            w.rawLoopStart = uint32_t(std::max(0L, off + q.ls - b0)) << 9;
            w.rawLoopEnd = uint32_t(std::max(0L, off + q.le - b0)) << 9;
            w.loopMode = b.loopMode;
        } else {
            w.rawLoopStart = uint32_t(std::clamp(p.ls - a0, 0L, long(out.size()))) << 9;
            w.rawLoopEnd = uint32_t(std::clamp(p.le - a0, 0L, long(out.size()))) << 9;
        }
        out.resize(out.size() + kTail, 0);
        d.swap(out);
        syncPoints(w);
        return true;
    }
    default:
        err = "UNKNOWN COMMAND";
        return false;
    }
}

} // namespace

const char *waveCmdName(WaveCmd c) {
    static const char *const k[] = {"truncate", "crossfade", "reverse_xfade", "ensemble_xfade", "bowtie_xfade",
                                    "convert_rate", "normalize", "fade_in", "fade_out", "volume_smooth",
                                    "scale_data", "reverse_data", "invert_data", "clear_data",
                                    "new_ws", "copy_ws", "delete_ws", "new_layer", "copy_layer",
                                    "delete_layer", "copy_params", "bidir_xfade", "loop_longer",
                                    "synth_loop", "copy_data", "mix_data", "merge", "splice", "auto_loop"};
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

namespace {

// Bit k of a layer mask = layer position k: remove a layer (the bits above move down).
int maskRemove(int m, int pos) {
    const int low = m & ((1 << pos) - 1), high = (m >> (pos + 1)) << pos;
    return (low | high) & 0xFF;
}

WaveSample squareWave(int idx) {
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
    return w;
}

bool layerCommand(Instrument &in, int layer, WaveCmd cmd, const WaveArgs &a, int &selWs, int &selLayer,
                  std::string &err) {
    const int nl = int(in.layers.size());
    if (cmd != WaveCmd::NewLayer && (layer < 0 || layer >= nl)) { err = "NO LAYER"; return false; }
    if (cmd == WaveCmd::DeleteLayer) {
        if (nl <= 1) { err = "ONLY ONE LAYER"; return false; }
        std::vector<int> mine;
        for (uint8_t k : in.layers[size_t(layer)].keyMap)
            if (k && std::find(mine.begin(), mine.end(), int(k)) == mine.end()) mine.push_back(k);
        for (size_t l = 0; l < in.layers.size(); ++l) {
            if (int(l) == layer) continue;
            for (uint8_t k : in.layers[l].keyMap) {
                auto it = in.waveSamples.find(k);
                if (!k || it == in.waveSamples.end()) continue;
                const WaveSample *o = in.pcmOwner(it->second);
                if (o && std::find(mine.begin(), mine.end(), o->index) != mine.end()) { err = "WS HAS COPIES"; return false; }
            }
        }
        for (int w : mine) in.waveSamples.erase(w);
        in.layers.erase(in.layers.begin() + layer);
        for (int &m : in.patches) m = maskRemove(m, layer);
        in.keyDownLayers = maskRemove(in.keyDownLayers, layer) | (in.keyDownLayers & 0x80 ? 0x80 : 0);
        in.keyUpLayers = maskRemove(in.keyUpLayers, layer);
        selLayer = std::max(0, layer - 1);
        selWs = 0;
        for (uint8_t k : in.layers[size_t(selLayer)].keyMap) if (k) { selWs = k; break; }
        return true;
    }
    if (nl >= 8) { err = "8 LAYERS"; return false; }
    const int pos = nl;                                  // the new layer comes last
    Layer l = cmd == WaveCmd::CopyLayer ? in.layers[size_t(layer)] : newLayer(pos + 1);
    l.fileOffset = 0;
    l.index = pos + 1;
    if (cmd == WaveCmd::NewLayer) {
        const int idx = freeIndex(in);
        if (!idx) { err = "127 WAVESAMPLES"; return false; }
        in.waveSamples[idx] = squareWave(idx);
        for (auto &k : l.keyMap) k = uint8_t(idx);
        selWs = idx;
        in.layers.push_back(l);
        in.patches[0] |= 1 << pos;
        in.keyDownLayers |= 1 << pos;
    } else {
        // the wavesamples of the layer, each once, as copies (or with their own data)
        std::vector<std::pair<int, int>> map;            // old -> new
        for (uint8_t &k : l.keyMap) {
            if (!k) continue;
            int to = 0;
            for (auto &m : map) if (m.first == k) to = m.second;
            if (!to) {
                auto it = in.waveSamples.find(k);
                if (it == in.waveSamples.end()) { k = 0; continue; }
                const int idx = freeIndex(in);
                if (!idx) { err = "127 WAVESAMPLES"; return false; }
                WaveSample c = it->second;
                const WaveSample *own = in.pcmOwner(it->second);
                c.index = idx;
                c.fileOffset = 0;
                c.imageWord = 0;
                if (a.withData && own) {
                    c.copyOfWaveSample = 0;
                    c.copyOfLayer = 0;
                    c.pcm = own->pcm;
                } else if (own) {
                    c.copyOfWaveSample = own->index;
                    c.copyOfLayer = layerOfWs(in, own->index);
                    c.pcm.clear();
                }
                in.waveSamples[idx] = std::move(c);
                map.push_back({k, idx});
                to = idx;
            }
            k = uint8_t(to);
        }
        selWs = map.empty() ? 0 : map.front().second;
        in.layers.push_back(l);
        for (int &m : in.patches) if (m & (1 << layer)) m |= 1 << pos;
        if (in.keyDownLayers & (1 << layer)) in.keyDownLayers |= 1 << pos;
        if (in.keyUpLayers & (1 << layer)) in.keyUpLayers |= 1 << pos;
    }
    updateKeyRanges(in, pos);
    selLayer = pos;
    return true;
}

}  // namespace

bool waveCommand(Instrument &in, int layer, int ws, WaveCmd cmd, const WaveArgs &a, int &selWs, std::string &err,
                 int *selLayer) {
    selWs = ws;
    int sl = layer;
    if (selLayer) *selLayer = layer;
    if (cmd == WaveCmd::NewLayer || cmd == WaveCmd::CopyLayer || cmd == WaveCmd::DeleteLayer) {
        const bool ok = layerCommand(in, layer, cmd, a, selWs, sl, err);
        if (ok && selLayer) *selLayer = sl;
        return ok;
    }
    if (cmd >= WaveCmd::BidirXFade && cmd <= WaveCmd::AutoLoop) {
        const bool ok = moreCommands(in, ws, cmd, a, selWs, err);
        if (ok && selLayer) *selLayer = layerOfWs(in, selWs);
        return ok;
    }
    if (cmd == WaveCmd::CopyParams) {
        auto from = in.waveSamples.find(ws);
        auto to = in.waveSamples.find(a.toWs);
        if (from == in.waveSamples.end()) { err = "NO WAVESAMPLE"; return false; }
        if (to == in.waveSamples.end()) { err = "NO DESTINATION"; return false; }
        if (a.toWs == ws) { err = "SAME WAVESAMPLE"; return false; }
        WaveSample keep = std::move(to->second);
        WaveSample c = from->second;
        c.index = keep.index;
        c.name = keep.name;
        c.copyOfWaveSample = keep.copyOfWaveSample;
        c.copyOfLayer = keep.copyOfLayer;
        c.rootNote = keep.rootNote;
        c.fineTune = keep.fineTune;
        c.loopMode = keep.loopMode;
        c.sampleStart = keep.sampleStart; c.sampleEnd = keep.sampleEnd;
        c.loopStart = keep.loopStart; c.loopEnd = keep.loopEnd;
        c.rawStart = keep.rawStart; c.rawEnd = keep.rawEnd;
        c.rawLoopStart = keep.rawLoopStart; c.rawLoopEnd = keep.rawLoopEnd;
        c.sampleRateIndex = keep.sampleRateIndex;
        c.sampleRate = keep.sampleRate;
        c.keyLow = keep.keyLow; c.keyHigh = keep.keyHigh;
        c.pcm = std::move(keep.pcm);
        c.imageWord = keep.imageWord;
        c.fileOffset = keep.fileOffset;
        to->second = std::move(c);
        selWs = a.toWs;
        if (selLayer) *selLayer = layerOfWs(in, a.toWs);
        return true;
    }
    if (cmd == WaveCmd::NewWs) {
        if (layer < 0 || layer >= int(in.layers.size())) { err = "NO LAYER"; return false; }
        const int idx = freeIndex(in);
        if (!idx) { err = "127 WAVESAMPLES"; return false; }
        in.waveSamples[idx] = squareWave(idx);
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
