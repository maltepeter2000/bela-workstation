#define EPS_EDIT_FIELDS
#include "eps_edit.h"
#include "eps_record.h"
#include "eps_sampler.h"   // modSourceName

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace eps {

namespace {

const char *noteText(int n, char *b) {
    static const char *k[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    std::snprintf(b, 16, "%s%d", k[((n % 12) + 12) % 12], (n / 12 - 1) % 100);
    return b;
}

// The same mapping as the controls: up to 16 values in zones of equal width, else linear.
int to127(int lo, int hi, int v) {
    const int n = hi - lo + 1;
    v = std::clamp(v, lo, hi);
    if (n <= 16) return std::min(((v - lo) * 2 + 1) * 64 / n, 127);
    return static_cast<int>(std::lround(double(v - lo) * 127.0 / double(hi - lo)));
}
int from127(int lo, int hi, double x) {
    x = std::clamp(x, 0.0, 127.0);
    const int n = hi - lo + 1;
    if (n <= 16) return lo + std::min(static_cast<int>(x * n / 128.0), n - 1);
    return lo + static_cast<int>(std::lround(x * (hi - lo) / 127.0));
}

const std::vector<int16_t> &pcmOf(const Instrument &in, const WaveSample &w) {
    static const std::vector<int16_t> none;
    const WaveSample *o = in.pcmOwner(w);
    return o ? o->pcm : none;
}

} // namespace

int Editor::find(const char *name) {
    for (int i = 0; i < kEditParamCount; ++i)
        if (!std::strcmp(kEditParams[i].name, name)) return i;
    return -1;
}

std::vector<int> Editor::layerWaveSamples(const Instrument &in) const {
    std::vector<int> v;
    if (layer < 0 || layer >= int(in.layers.size())) return v;
    for (uint8_t w : in.layers[size_t(layer)].keyMap)
        if (w && in.waveSamples.count(w) && std::find(v.begin(), v.end(), int(w)) == v.end()) v.push_back(w);
    return v;
}

int Editor::wsPosition(const Instrument &in) const {
    const std::vector<int> v = layerWaveSamples(in);
    const auto it = std::find(v.begin(), v.end(), ws);
    return it == v.end() ? 0 : int(it - v.begin()) + 1;
}

void Editor::selectWs(const Instrument &in, int pos) {
    const std::vector<int> v = layerWaveSamples(in);
    if (!v.empty()) ws = v[size_t(std::clamp(pos, 1, int(v.size())) - 1)];
    fit(in);
}

void Editor::stepWs(const Instrument &in, int d) {
    const std::vector<int> v = layerWaveSamples(in);
    if (v.empty()) return;
    const int n = int(v.size()), p = std::max(wsPosition(in), 1) - 1;
    selectWs(in, ((p + d) % n + n) % n + 1);
}

void Editor::fit(const Instrument &in) {
    if (layer < 0 || layer >= int(in.layers.size())) layer = 0;
    if (!wsPosition(in)) {
        const std::vector<int> v = layerWaveSamples(in);
        if (!v.empty()) ws = v.front();
    }
    if (!in.waveSamples.count(ws)) ws = in.waveSamples.empty() ? 1 : in.waveSamples.begin()->first;
    env = std::clamp(env, 0, 2);
    for (int i = 0; i < 5; ++i) anchor_[i] = 0;
    if (const auto it = in.waveSamples.find(ws); it != in.waveSamples.end())
        for (int i = 0; i < 5; ++i) anchor_[i] = point(it->second, i);
}

bool Editor::followKey(const Instrument &in, int key, int patchMask) {
    // the selected layer first, if it plays the key (several layers on one key: choose the
    // layer by hand, the wavesample follows); else the first layer that plays it
    if (layer >= 0 && layer < int(in.layers.size()) && (patchMask & (1 << layer))) {
        const int w = in.layers[size_t(layer)].waveSampleForNote(key);
        if (w && in.waveSamples.count(w)) {
            if (w == ws) return false;
            ws = w;
            fit(in);
            return true;
        }
    }
    for (size_t l = 0; l < in.layers.size() && l < 8; ++l) {
        if (!(patchMask & (1 << l))) continue;
        const int w = in.layers[l].waveSampleForNote(key);
        if (!w || !in.waveSamples.count(w)) continue;
        if (int(l) == layer && w == ws) return false;
        layer = int(l);
        ws = w;
        fit(in);
        return true;
    }
    return false;
}

Layer *Editor::layerP(Instrument &in) const {
    return (layer >= 0 && layer < int(in.layers.size())) ? &in.layers[size_t(layer)] : nullptr;
}
WaveSample *Editor::wsP(Instrument &in) const {
    auto it = in.waveSamples.find(ws);
    return it == in.waveSamples.end() ? nullptr : &it->second;
}
Envelope *Editor::envP(Instrument &in) const {
    WaveSample *w = wsP(in);
    if (!w) return nullptr;
    return env == 0 ? &w->pitchEnv : (env == 1 ? &w->filterEnv : &w->ampEnv);
}

uint32_t Editor::point(const WaveSample &w, int i) const {
    switch (i) {
        case 0: return w.sampleStart;
        case 1: return w.sampleEnd;
        case 2: return w.loopStart;
        case 3: return w.loopEnd;
        default: return w.loopStart;   // 4: loop position, shown as the loop start (as on the EPS)
    }
}

// Sample and loop points: inside the sample, start <= loop start < loop end <= end,
// optionally on the nearest rising zero crossing (within 4096 samples).
void Editor::setPoint(Instrument &in, WaveSample &w, int i, int64_t pos) {
    const std::vector<int16_t> &pcm = pcmOf(in, w);
    if (pcm.size() < 4) return;
    const int64_t last = int64_t(pcm.size()) - 1;
    pos = std::clamp<int64_t>(pos, 0, last);
    if (zeroCross) {
        auto rising = [&](int64_t p) { return p > 0 && p <= last && pcm[size_t(p - 1)] < 0 && pcm[size_t(p)] >= 0; };
        for (int64_t d = 0; d < 4096; ++d) {
            if (rising(pos - d)) { pos -= d; break; }
            if (rising(pos + d)) { pos += d; break; }
        }
    }
    int64_t s = w.sampleStart, e = w.sampleEnd, ls = w.loopStart, le = w.loopEnd;
    if (i == 4) {
        // loop position: loop start and end move together, the loop keeps its length (and
        // the fine part of its end), so its pitch stays
        const int64_t len = int64_t(w.rawLoopEnd >> 9) - int64_t(w.rawLoopStart >> 9);
        const int64_t to = std::clamp<int64_t>(pos, s, std::max<int64_t>(s, e - len));
        const int64_t d = to - int64_t(w.rawLoopStart >> 9);
        w.rawLoopStart = uint32_t(int64_t(w.rawLoopStart) + d * 512);
        w.rawLoopEnd = uint32_t(int64_t(w.rawLoopEnd) + d * 512);
        w.loopStart = w.rawLoopStart >> 9;
        w.loopEnd = (w.rawLoopEnd >> 9) + ((w.rawLoopEnd >> 5) & 0xF);
        return;
    }
    switch (i) {
        case 0: s = std::min(pos, ls); break;
        case 1: e = std::max(pos, le); break;
        case 2: ls = std::clamp<int64_t>(pos, s, le - 2); break;
        default: le = std::clamp<int64_t>(pos, ls + 2, e); break;
    }
    w.sampleStart = uint32_t(s);
    w.sampleEnd = uint32_t(e);
    w.loopStart = uint32_t(ls);
    w.loopEnd = uint32_t(le);
    w.rawStart = w.sampleStart << 9;
    w.rawEnd = w.sampleEnd << 9;
    w.rawLoopStart = w.loopStart << 9;
    w.rawLoopEnd = w.loopEnd << 9;
}

int Editor::value(Instrument &in, int id) const {
    const EditParam &p = kEditParams[id];
    if (p.scope == 'm') {
        if (p.index < 4) return (in.patches[p.index] >> layer) & 1;
        return (in.keyUpLayers >> layer) & 1;
    }
    if (p.scope == 'p' || p.scope == 'f') {
        const WaveSample *w = wsP(in);
        if (!w) return 0;
        const uint32_t pos = point(*w, p.index);
        if (p.scope == 'f') return std::clamp(int(int64_t(pos) - int64_t(anchor_[p.index])) + 64, 0, 127);
        const size_t n = pcmOf(in, *w).size();
        return n > 1 ? int(std::lround(double(pos) * 127.0 / double(n - 1))) : 0;
    }
    const int *f = editField(id, in, layerP(in), wsP(in), envP(in));
    if (!f) return p.lo;
    if (!std::strcmp(p.kind, "s8")) return int(int8_t(*f & 0xFF));
    return *f;
}

void Editor::setValue(Instrument &in, int id, int v) {
    const EditParam &p = kEditParams[id];
    if (p.scope == 'm') {
        int &mask = p.index < 4 ? in.patches[p.index] : in.keyUpLayers;
        mask = v ? (mask | (1 << layer)) : (mask & ~(1 << layer));
        return;
    }
    int *f = editField(id, in, layerP(in), wsP(in), envP(in));
    if (!f) return;
    *f = !std::strcmp(p.kind, "s8") ? (v & 0xFF) : v;
    // The key range of a wavesample is its keys in the layer's key map (what plays).
    if (!std::strcmp(p.name, "w_keylo") || !std::strcmp(p.name, "w_keyhi")) {
        const WaveSample &w = *wsP(in);
        setKeyRange(in, layer, ws, w.keyLow, w.keyHigh);
    }
}

int Editor::get127(Instrument &in, int id) const {
    const EditParam &p = kEditParams[id];
    if (p.scope == 'p' || p.scope == 'f') return value(in, id);
    return to127(p.lo, p.hi, value(in, id));
}

void Editor::set127(Instrument &in, int id, double x) {
    const EditParam &p = kEditParams[id];
    if (p.scope == 'p' || p.scope == 'f') {
        WaveSample *w = wsP(in);
        if (!w) return;
        const size_t n = pcmOf(in, *w).size();
        if (n < 4) return;
        const int i = p.index;
        if (p.scope == 'p') {
            anchor_[i] = uint32_t(std::lround(std::clamp(x, 0.0, 127.0) / 127.0 * double(n - 1)));
            setPoint(in, *w, i, anchor_[i]);
        } else {
            setPoint(in, *w, i, int64_t(anchor_[i]) + int64_t(std::lround(x)) - 64);
        }
        // the other points may have moved with it (loop position, limits): their fine
        // cells count from where they are now
        for (int j = 0; j < 5; ++j)
            if (j != i) anchor_[j] = point(*w, j);
        return;
    }
    setValue(in, id, from127(p.lo, p.hi, x));
}

std::string Editor::text(Instrument &in, int id) const {
    const EditParam &p = kEditParams[id];
    char b[48];
    if (p.scope == 'p' || p.scope == 'f') {
        const WaveSample *w = wsP(in);
        std::snprintf(b, sizeof b, "%u", w ? point(*w, p.index) : 0u);
        return b;
    }
    const int v = value(in, id);
    const char *k = p.kind;
    if (std::strchr(k, '|')) {
        const char *s = k;
        for (int i = p.lo; i < v && s; ++i) {
            s = std::strchr(s, '|');
            if (s) ++s;
        }
        if (s) {
            const char *e = std::strchr(s, '|');
            return std::string(s, e ? size_t(e - s) : std::strlen(s));
        }
        return std::to_string(v);
    }
    if (!std::strcmp(k, "note")) { char nb[16]; return noteText(v, nb); }
    if (!std::strcmp(k, "src")) return modSourceName(v);
    if (!std::strcmp(k, "cents")) { std::snprintf(b, sizeof b, "%+dct", int(std::lround(v * 100.0 / 128.0))); return b; }
    if (!std::strcmp(k, "bend")) { if (v > 12) return "global"; std::snprintf(b, sizeof b, "%d", v); return b; }
    if (!std::strcmp(k, "range")) {
        const uint32_t r = 1u << (21 - std::min(v, 21));
        if (r >= 1024) std::snprintf(b, sizeof b, "%uK", r / 1024);
        else std::snprintf(b, sizeof b, "%u", r);
        return b;
    }
    if (!std::strcmp(k, "s") || !std::strcmp(k, "s8")) { std::snprintf(b, sizeof b, "%+d", v); return b; }
    std::snprintf(b, sizeof b, "%d", v);
    return b;
}

std::string Editor::selectionInfo(const Instrument &in) const {
    auto it = in.waveSamples.find(ws);
    if (it == in.waveSamples.end()) return "-";
    const WaveSample &w = it->second;
    char a[16], c[16], r[16], b[128];
    const WaveSample *o = in.pcmOwner(w);
    std::snprintf(b, sizeof b, "keys_%s-%s_root_%s_%dHz_%zu_samples%s", noteText(w.keyLow, a), noteText(w.keyHigh, c),
                  noteText(w.rootNote, r), o ? o->sampleRate : w.sampleRate, o ? o->pcm.size() : size_t(0),
                  w.copyOfWaveSample ? ("_copy_of_" + std::to_string(w.copyOfWaveSample)).c_str() : "");
    return b;
}

} // namespace eps
