// SPDX-License-Identifier: BSD-3-Clause
// cz4_core: see cz4_core.h.
#include "cz4_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace cz4 {

// ---- controls -----------------------------------------------------------------

int control_count() { return static_cast<int>(sizeof kControls / sizeof kControls[0]); }
const Control& control(int i) { return kControls[i]; }
const Control* find_control(const char* name) {
    for (const Control& c : kControls)
        if (std::strcmp(c.name, name) == 0) return &c;
    return nullptr;
}

int control_from127(const Control& c, float pot) {
    pot = pot < 0 ? 0 : (pot > 127 ? 127 : pot);
    const int n = c.hi - c.lo + 1;
    if (n <= 16) {
        int z = static_cast<int>(pot * static_cast<float>(n) / 128.0f);
        return c.lo + (z >= n ? n - 1 : z);
    }
    return c.lo + static_cast<int>(std::lround(pot * static_cast<float>(c.hi - c.lo) / 127.0f));
}

int control_to127(const Control& c, int value) {
    const int n = c.hi - c.lo + 1;
    value = value < c.lo ? c.lo : (value > c.hi ? c.hi : value);
    if (n <= 16) {
        const int v = ((value - c.lo) * 2 + 1) * 64 / n;
        return v > 127 ? 127 : v;
    }
    return static_cast<int>(std::lround(static_cast<float>(value - c.lo) * 127.0f / static_cast<float>(c.hi - c.lo)));
}

// ---- banks --------------------------------------------------------------------

namespace {

struct Tone { uint8_t b[kToneBytes]; };

std::vector<Tone> g_factory;
Tone g_user[kUserSlots];
int g_ops[kOpSlots][kOpCount];
bool g_op_used[kOpSlots];
bool g_banks_ready = false;
std::string g_names_scratch;

void banks_init() {
    if (g_banks_ready) return;
    g_banks_ready = true;
    for (Tone& t : g_user) {
        cz_tone_init(t.b);
        cz_name_set(t.b, "EMPTY");
    }
    for (int s = 0; s < kOpSlots; ++s) {
        std::memcpy(g_ops[s], kOpDefaults, sizeof kOpDefaults);
        g_op_used[s] = false;
    }
}

const Tone& init_tone() {
    static Tone t;
    static bool made = false;
    if (!made) { cz_tone_init(t.b); made = true; }
    return t;
}

bool read_file(const char* path, std::vector<uint8_t>& data, std::string& err) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { err = "cannot open"; return false; }
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

struct ScanCtx { std::vector<Tone> tones; std::vector<int> progs; };
void scan_cb(void* ctx, const uint8_t* t, int prog, int) {
    ScanCtx* s = static_cast<ScanCtx*>(ctx);
    Tone x;
    std::memcpy(x.b, t, kToneBytes);
    s->tones.push_back(x);
    s->progs.push_back(prog);
}

char g_namebuf[2][CZ_NAME_LEN + 1];

}  // namespace

int load_syx(const char* path, std::string& err) {
    banks_init();
    std::vector<uint8_t> d;
    if (!read_file(path, d, err)) return -1;
    ScanCtx s;
    const int got = cz_syx_scan(d.data(), static_cast<long>(d.size()), scan_cb, &s);
    if (got <= 0) { err = "no CZ tones in this file"; return -1; }
    g_factory.insert(g_factory.end(), s.tones.begin(), s.tones.end());
    return got;
}

int patch_count() { return g_factory.empty() ? 1 : static_cast<int>(g_factory.size()); }

static const Tone& factory(int i) {
    if (g_factory.empty()) return init_tone();
    return g_factory[static_cast<size_t>(std::clamp(i, 0, patch_count() - 1))];
}

const char* patch_name(int i) {
    cz_name_get(factory(i).b, g_namebuf[0]);
    return g_namebuf[0];
}

const char* user_name(int i) {
    banks_init();
    cz_name_get(g_user[std::clamp(i, 0, kUserSlots - 1)].b, g_namebuf[1]);
    return g_namebuf[1];
}

// The user bank: 64 CZ-1 tones as librarian messages, program 0x20 + slot.
int load_user(const char* path, std::string& err) {
    banks_init();
    std::vector<uint8_t> d;
    if (!read_file(path, d, err)) return -1;
    ScanCtx s;
    const int got = cz_syx_scan(d.data(), static_cast<long>(d.size()), scan_cb, &s);
    if (got <= 0) { err = "no CZ tones in this file"; return -1; }
    int next = 0;
    for (size_t k = 0; k < s.tones.size(); ++k) {
        int slot = s.progs[k] >= 0x20 && s.progs[k] < 0x20 + kUserSlots ? s.progs[k] - 0x20 : next;
        if (slot >= kUserSlots) break;
        g_user[slot] = s.tones[k];
        next = slot + 1;
    }
    return got;
}

bool write_user(const char* path, std::string& err) {
    banks_init();
    FILE* f = std::fopen(path, "wb");
    if (!f) { err = "cannot write"; return false; }
    uint8_t msg[8 + 2 * kToneBytes];
    for (int s = 0; s < kUserSlots; ++s) {
        const int n = cz_syx_tone(g_user[s].b, 0, 0x20 + s, 1, msg);
        std::fwrite(msg, 1, static_cast<size_t>(n), f);
    }
    std::fclose(f);
    return true;
}

// Operation memory: one message per used slot, F0 7D 'C' 'Z' '4' 'O' slot
// count v0-lo v1-lo ... F7 (7D: the non-commercial SysEx ID).
int load_ops(const char* path, std::string& err) {
    banks_init();
    std::vector<uint8_t> d;
    if (!read_file(path, d, err)) return -1;
    int got = 0;
    std::vector<const Control*> ops(kOpCount, nullptr);
    for (const Control& c : kControls) if (c.kind == 'O') ops[static_cast<size_t>(c.index)] = &c;
    for (size_t i = 0; i + 8 < d.size(); ++i) {
        if (d[i] != 0xF0 || d[i + 1] != 0x7D || d[i + 2] != 'C' || d[i + 3] != 'Z' || d[i + 4] != '4' ||
            d[i + 5] != 'O')
            continue;
        const int slot = d[i + 6], count = d[i + 7];
        if (slot >= kOpSlots) continue;
        size_t j = i + 8;
        std::memcpy(g_ops[slot], kOpDefaults, sizeof kOpDefaults);
        for (int k = 0; k < count && j < d.size() && d[j] < 0x80; ++k, ++j)
            if (k < kOpCount) g_ops[slot][k] = std::clamp(ops[static_cast<size_t>(k)]->lo + d[j],
                                                          ops[static_cast<size_t>(k)]->lo,
                                                          ops[static_cast<size_t>(k)]->hi);
        g_op_used[slot] = true;
        ++got;
        i = j;
    }
    if (!got) { err = "no cz4 operation memory in this file"; return -1; }
    return got;
}

bool write_ops(const char* path, std::string& err) {
    banks_init();
    FILE* f = std::fopen(path, "wb");
    if (!f) { err = "cannot write"; return false; }
    std::vector<const Control*> ops(kOpCount, nullptr);
    for (const Control& c : kControls) if (c.kind == 'O') ops[static_cast<size_t>(c.index)] = &c;
    for (int s = 0; s < kOpSlots; ++s) {
        if (!g_op_used[s]) continue;
        std::vector<uint8_t> m = {0xF0, 0x7D, 'C', 'Z', '4', 'O', static_cast<uint8_t>(s),
                                  static_cast<uint8_t>(kOpCount)};
        for (int k = 0; k < kOpCount; ++k)
            m.push_back(static_cast<uint8_t>((g_ops[s][k] - ops[static_cast<size_t>(k)]->lo) & 0x7F));
        m.push_back(0xF7);
        std::fwrite(m.data(), 1, m.size(), f);
    }
    std::fclose(f);
    return true;
}

bool op_used(int i) { banks_init(); return i >= 0 && i < kOpSlots && g_op_used[i]; }

// ---- the instrument -------------------------------------------------------------

namespace {

constexpr int kCtl = 16;               // samples per control tick
constexpr float kChipRate = 40000.0f;  // the uPD933's sample rate: rates count in its samples

// The DCA law of the uPD933: 128 levels, logarithmic over about 78 dB.
float dca_amp(float level) {
    if (level <= 0.0f) return 0.0f;
    return std::exp2(13.0f * (level * 4.0f / 511.0f) - 13.0f);
}

// An envelope of one line, ready to play: values in its own unit (DCO
// semitones, DCW phase units 0..1016, DCA levels 0..127) and rates per chip
// sample in that unit.
struct EnvDef {
    float level[8];
    float rate[8];
    int sus;        // -1 none
    int end;
};

void env_def(const uint8_t* b, int line, int e, EnvDef& d) {
    cz_env_t r;
    cz_env_decode(b, line, e, &r);
    d.sus = r.sus;
    d.end = r.end;
    for (int j = 0; j < 8; ++j) {
        const int lv = r.level[j];
        const float chip = static_cast<float>((8 | (r.rate[j] & 7)) << (r.rate[j] >> 3));
        switch (e) {
            case CZ_ENV_DCO:
                d.level[j] = static_cast<float>(lv & 63) * ((lv & 64) ? 2.0f : 0.0625f);
                d.rate[j] = chip / 1048576.0f;      // 2^20 per semitone
                break;
            case CZ_ENV_DCW:
                d.level[j] = static_cast<float>(lv) * 8.0f;
                d.rate[j] = chip / 65536.0f;        // 2^16 per phase unit
                break;
            default:
                d.level[j] = static_cast<float>(lv);
                d.rate[j] = chip / 262144.0f;       // 2^18 per level
                break;
        }
    }
}

struct ToneDef {                       // a tone, decoded for playing
    EnvDef env[2][3];                  // [line][DCO DCW DCA]
    int linesel = 0, octave = 0, mod = 0;
    unsigned wave[2][2] = {{0, 0}, {0, 0}}, window[2] = {0, 0};
    float detune = 0;                  // line 2, semitones
    float level[2] = {1, 1};
    float velp[2] = {0, 0}, velw[2] = {0, 0}, vela[2] = {0, 0};
    int kfw[2] = {0, 0}, kfa[2] = {0, 0};
    int vwave = 0;
    float vdelay_s = 0, vrate_hz = 0, vdepth = 0;   // depth in semitones
};

float vib_depth_semis(unsigned machine) { return machine <= 1 ? 0.0f : static_cast<float>(machine) / 256.0f; }

void tone_def(const uint8_t* b, ToneDef& d) {
    d.linesel = cz_get(b, CZP_LINESEL, 0, 0);
    d.octave = cz_get(b, CZP_OCTAVE, 0, 0);
    d.mod = cz_get(b, CZP_MOD, 0, 0);
    for (int l = 0; l < 2; ++l) {
        for (int e = 0; e < 3; ++e) env_def(b, l, e, d.env[l][e]);
        const int w = cz_get(b, CZP_WAVE, l, 0), w2 = cz_get(b, CZP_WAVE2, l, 0);
        d.wave[l][0] = static_cast<unsigned>(w);
        d.wave[l][1] = static_cast<unsigned>(w2 ? w2 - 1 : w);
        d.window[l] = static_cast<unsigned>(cz_get(b, CZP_WINDOW, l, 0));
        d.level[l] = static_cast<float>(cz_get(b, CZP_LEVEL, l, 0)) / 15.0f;
        d.velp[l] = static_cast<float>(cz_get(b, CZP_VELP, l, 0)) / 15.0f;
        d.velw[l] = static_cast<float>(cz_get(b, CZP_VELW, l, 0)) / 15.0f;
        d.vela[l] = static_cast<float>(cz_get(b, CZP_VELA, l, 0)) / 15.0f;
        d.kfw[l] = cz_get(b, CZP_KFW, l, 0);
        d.kfa[l] = cz_get(b, CZP_KFA, l, 0);
    }
    const float fine = static_cast<float>(b[CZB_PDETL] >> 2) / 64.0f;
    d.detune = (static_cast<float>(b[CZB_PDETH]) + fine) * (b[CZB_PDS] & 1 ? -1.0f : 1.0f);
    d.vwave = cz_get(b, CZP_VWAVE, 0, 0);
    // The 16-bit machine values (low byte first) after each panel value. The
    // vibrato clock is not documented: 200 ticks per second (melodee).
    const unsigned delay = b[CZB_PVD + 1] | b[CZB_PVD + 2] << 8;
    const unsigned rate = b[CZB_PVS + 1] | b[CZB_PVS + 2] << 8;
    const unsigned depth = b[CZB_PVV + 1] | b[CZB_PVV + 2] << 8;
    d.vdelay_s = static_cast<float>(delay) / 200.0f;
    d.vrate_hz = static_cast<float>(rate) * 200.0f / 65536.0f;
    d.vdepth = vib_depth_semis(depth);
}

struct Env {
    float cur = 0, target = 0, rate = 0;
    int stage = 0;
};

void env_load(Env& e, const EnvDef& d, int stage) {
    e.stage = stage;
    if (stage > d.end) { e.target = 0; e.rate = 1e9f; return; }
    e.target = stage == d.end ? 0.0f : d.level[stage];
    e.rate = d.rate[stage];
}

// One control tick: `chip` chip samples. k_att / k_rel scale the rates before
// and after the sustain step.
void env_tick(Env& e, const EnvDef& d, bool gate, float chip, float k_att, float k_rel) {
    for (int guard = 0; guard < 9; ++guard) {
        if (e.stage > d.end) { e.cur = 0; return; }
        if (e.cur == e.target) {
            if (gate && e.stage == d.sus) return;
            env_load(e, d, e.stage + 1);
            continue;
        }
        const bool before = d.sus < 0 ? e.stage == 0 : e.stage <= d.sus;
        const float step = e.rate * chip * (before ? k_att : k_rel);
        if (std::fabs(e.target - e.cur) <= step) { e.cur = e.target; chip = 0; continue; }
        e.cur += e.target > e.cur ? step : -step;
        return;
    }
}

void env_release(Env& e, const EnvDef& d) {
    if (d.sus >= 0 && e.stage <= d.sus) env_load(e, d, d.sus + 1);
}

struct Line {
    cz_osc_t osc{0, 0};
    Env env[3];
    float dc = 0, amp = 0, dcw = 0;
    float noise_left = 0;
    bool noise_up = false;
};

struct Voice {
    bool active = false;
    int tone = 0;              // 0 A, 1 B
    int key = 0;               // the key pressed
    float note = 0;            // the note it plays (key + tone offsets)
    float vel = 1;
    bool gate = false, held = false;
    uint32_t age = 0;
    int lines = 1;             // 1 or 2 heard
    Line line[2];
    float slide = 0;           // portamento / glide offset in semitones, runs to 0
    float slide_rate = 0;      // semitones per second
    float vib_t = 0, vib_ph = 0;
    uint32_t rng = 1;
};

uint32_t xorshift(uint32_t& s) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

// Portamento and glide time: 0..99 -> seconds for an octave.
float slide_rate(int v) {
    const float t = 0.01f + 2.5f * static_cast<float>(v * v) / (99.0f * 99.0f);
    return 12.0f / t;
}

// A stereo chorus with two modulated delay taps, as the CZ-1's.
struct Chorus {
    std::vector<float> buf;
    int pos = 0;
    float ph = 0, fs = 44100;
    void init(float rate) {
        fs = rate;
        buf.assign(static_cast<size_t>(rate * 0.04f) + 4, 0.0f);
        pos = 0;
    }
    float tap(float delay_s) const {
        const float d = delay_s * fs;
        const int n = static_cast<int>(buf.size());
        float rp = static_cast<float>(pos) - d;
        while (rp < 0) rp += static_cast<float>(n);
        const int i = static_cast<int>(rp);
        const float f = rp - static_cast<float>(i);
        return buf[static_cast<size_t>(i % n)] * (1 - f) + buf[static_cast<size_t>((i + 1) % n)] * f;
    }
    void run(const float* in, float* l, float* r, int n, float mix) {
        for (int i = 0; i < n; ++i) {
            buf[static_cast<size_t>(pos)] = in[i];
            ph += 0.6f / fs;
            if (ph >= 1) ph -= 1;
            const float tri = ph < 0.5f ? ph * 4 - 1 : 3 - ph * 4;
            const float a = tap(0.0075f + 0.0025f * tri), b = tap(0.0075f - 0.0025f * tri);
            l[i] += in[i] + mix * a;
            r[i] += in[i] + mix * b;
            if (++pos >= static_cast<int>(buf.size())) pos = 0;
        }
    }
};

}  // namespace

struct Synth::Impl {
    float fs = 44100;
    Tone tone[2], stored[2];
    ToneDef def[2];
    int src[2] = {kInit, kInit}, idx[2] = {0, 0};
    bool compare = false;
    int op[kOpCount], perf[kPerfCount], set[kSetCount], sel[kSelCount], easy[kEasyCount];
    int op_slot = -1;
    Voice voice[kMaxVoices];
    uint32_t clock = 0;
    float bend = 0, mod = 0, at = 0;
    bool sustain = false;
    std::vector<int> held[2];          // keys down per tone (solo)
    float last_note[2] = {60, 60};
    Chorus chorus;
    float dc_out[2] = {0, 0};
    std::vector<float> dry, wet;

    Impl() {
        banks_init();
        std::memcpy(op, kOpDefaults, sizeof op);
        std::memcpy(perf, kPerfDefaults, sizeof perf);
        std::memcpy(set, kSetDefaults, sizeof set);
        std::memcpy(sel, kSelDefaults, sizeof sel);
        std::memcpy(easy, kEasyDefaults, sizeof easy);
        cz_tables_init();
        for (int t = 0; t < 2; ++t) {
            tone[t] = stored[t] = init_tone();
            redecode(t);
        }
        for (int v = 0; v < kMaxVoices; ++v) voice[v].rng = 0x9E3779B9u * static_cast<uint32_t>(v + 1);
        chorus.init(fs);
    }

    void redecode(int t) { tone_def(playing(t).b, def[t]); }
    const Tone& playing(int t) const { return compare ? stored[t] : tone[t]; }
    int et() const { return sel[kSel_edit]; }

    void set_tone(int t, const Tone& x, int source, int index) {
        tone[t] = stored[t] = x;
        src[t] = source;
        idx[t] = index;
        compare = false;
        redecode(t);
    }

    // Tone B follows the operation memory (bank and number).
    void load_b() {
        if (op[kOp_bsrc] == 1) set_tone(1, g_user[std::clamp(op[kOp_bnum], 0, kUserSlots - 1)], kUser,
                                        std::clamp(op[kOp_bnum], 0, kUserSlots - 1));
        else set_tone(1, factory(op[kOp_bnum]), g_factory.empty() ? kInit : kFactory,
                      std::clamp(op[kOp_bnum], 0, patch_count() - 1));
    }

    int lines_of(int t) const { return def[t].linesel >= 2 ? 2 : 1; }

    int lines_used() const {
        int n = 0;
        for (const Voice& v : voice) if (v.active) n += v.lines;
        return n;
    }

    // A free voice, or the oldest (released ones first) when the line budget
    // of the CZ (8 lines: 8 notes of a one-line tone, 4 of a two-line tone)
    // is used up.
    Voice* allocate(int need) {
        const int budget = std::max(set[kSet_voices], need);
        for (int guard = 0; guard < kMaxVoices && lines_used() + need > budget; ++guard) {
            Voice* old = nullptr;
            for (Voice& v : voice) {
                if (!v.active) continue;
                if (!old || (old->gate && !v.gate) || (old->gate == v.gate && v.age < old->age)) old = &v;
            }
            if (!old) break;
            old->active = false;
        }
        Voice* free = nullptr;
        for (Voice& v : voice) if (!v.active) { free = &v; break; }
        if (!free) {
            for (Voice& v : voice) if (!free || v.age < free->age) free = &v;
        }
        return free;
    }

    float tone_offset(int t) const {
        float n = static_cast<float>(12 * def[t].octave + set[kSet_transpose]) +
                  static_cast<float>(set[kSet_tune]) / 100.0f;
        if (t == 1) n += static_cast<float>(12 * op[kOp_boct]) + static_cast<float>(op[kOp_bdet]) / 100.0f;
        return n;
    }

    void start(Voice& v, int t, int key, int vel, bool retrigger, float from_note) {
        const bool was = v.active;
        v.active = true;
        v.tone = t;
        v.key = key;
        v.note = static_cast<float>(key) + tone_offset(t);
        v.vel = static_cast<float>(vel);
        v.gate = true;
        v.held = false;
        v.age = ++clock;
        v.lines = lines_of(t);
        v.slide = 0;
        if (op[kOp_glide]) {
            v.slide = static_cast<float>(op[kOp_gnote]);
            v.slide_rate = slide_rate(op[kOp_gtime]);
        }
        if (op[kOp_porta] && from_note > -100) {
            v.slide = from_note - v.note;
            v.slide_rate = slide_rate(op[kOp_ptime]);
        }
        if (!retrigger && was) return;
        v.vib_t = 0;
        v.vib_ph = 0;
        // The CZ-1 starts the voices of a tone at slightly different phases.
        const uint32_t ph = static_cast<uint32_t>((v.age & 3u) * 8u) << 21;
        for (int l = 0; l < 2; ++l) {
            Line& ln = v.line[l];
            cz_osc_reset(&ln.osc, ph);
            const int s = source_line(t, l);
            for (int e = 0; e < 3; ++e) {
                // A stolen voice goes on from its current value (as the chip does).
                if (!was) ln.env[e].cur = 0;
                env_load(ln.env[e], def[t].env[s][e], 0);
            }
        }
    }

    int source_line(int t, int l) const { return (def[t].linesel == 2 && l == 1) ? 0 : l; }

    bool tone_plays(int t, int key) const {
        switch (op[kOp_keymode]) {
            case 1: return t == 0 ? key >= op[kOp_split] : key < op[kOp_split];
            case 2: return true;
            default: return t == 0;
        }
    }

    void note_on(int key, int vel) {
        for (int t = 0; t < 2; ++t) {
            if (!tone_plays(t, key)) continue;
            const float from = last_note[t];
            if (op[kOp_solo]) {
                Voice* v = nullptr;
                for (Voice& x : voice) if (x.active && x.tone == t && x.gate) { v = &x; break; }
                const bool legato = v != nullptr;
                if (!v) v = allocate(lines_of(t));
                held[t].push_back(key);
                start(*v, t, key, vel, !legato, op[kOp_porta] ? from : -1000);
                if (legato && op[kOp_porta] == 0) v->slide = 0;
            } else {
                Voice* v = allocate(lines_of(t));
                start(*v, t, key, vel, true, op[kOp_porta] ? from : -1000);
            }
            last_note[t] = static_cast<float>(key) + tone_offset(t);
        }
    }

    void silence_all() {
        for (Voice& v : voice) v.active = false;
        held[0].clear();
        held[1].clear();
    }

    void release(Voice& v) {
        v.gate = false;
        for (int l = 0; l < 2; ++l)
            for (int e = 0; e < 3; ++e) env_release(v.line[l].env[e], def[v.tone].env[source_line(v.tone, l)][e]);
    }

    void note_off(int key) {
        for (int t = 0; t < 2; ++t) {
            auto& h = held[t];
            const bool was_top = !h.empty() && h.back() == key;
            h.erase(std::remove(h.begin(), h.end(), key), h.end());
            if (op[kOp_solo] && was_top && !h.empty()) {
                // back to the key still held (legato)
                for (Voice& v : voice)
                    if (v.active && v.tone == t && v.gate) {
                        const float from = v.note + v.slide;
                        v.key = h.back();
                        v.note = static_cast<float>(h.back()) + tone_offset(t);
                        v.slide = op[kOp_porta] ? from - v.note : 0;
                        v.slide_rate = slide_rate(op[kOp_ptime]);
                    }
                continue;
            }
        }
        for (Voice& v : voice) {
            if (!v.active || !v.gate || v.key != key) continue;
            if (op[kOp_solo] && !held[v.tone].empty()) continue;
            if (sustain) v.held = true;
            else release(v);
        }
    }

    void set_sustain(bool on) {
        sustain = on;
        if (on) return;
        for (Voice& v : voice) if (v.active && v.gate && v.held) release(v);
    }

    // ---- rendering ----
    void render_voice(Voice& v, float* out, int n) {
        const ToneDef& d = def[v.tone];
        const float chip = static_cast<float>(n) * kChipRate / fs;
        const float k_att = std::exp2(-static_cast<float>(perf[kPerf_attack]) / 16.0f);   // + slower
        const float k_rel = std::exp2(-static_cast<float>(perf[kPerf_release]) / 16.0f);
        const float kfn = static_cast<float>(std::max(0, v.key - 36));
        const float dt = static_cast<float>(n) / fs;

        // slide (portamento, glide)
        if (v.slide != 0) {
            const float s = v.slide_rate * dt;
            v.slide = std::fabs(v.slide) <= s ? 0 : v.slide - (v.slide > 0 ? s : -s);
        }
        // vibrato
        v.vib_t += dt;
        float vib = 0;
        const float extra = vib_depth_semis(cz_vib_machine(2, op[kOp_wheel])) * mod +
                            vib_depth_semis(cz_vib_machine(2, op[kOp_atvib])) * at +
                            vib_depth_semis(cz_vib_machine(2, perf[kPerf_vibrato]));
        const float depth = (v.vib_t >= d.vdelay_s ? d.vdepth : 0.0f) + extra;
        if (depth > 0) {
            v.vib_ph += d.vrate_hz * dt;
            v.vib_ph -= std::floor(v.vib_ph);
            const float x = v.vib_ph;
            float w;
            switch (d.vwave) {
                case 1: w = 2 * x - 1; break;                         // saw up
                case 2: w = 1 - 2 * x; break;                         // saw down
                case 3: w = x < 0.5f ? 1.0f : -1.0f; break;           // square
                default: w = x < 0.5f ? 4 * x - 1 : 3 - 4 * x; break; // triangle
            }
            vib = w * depth;
        }
        const float velf = (127.0f - v.vel) / 127.0f;
        const float base = v.note + v.slide + vib + bend * static_cast<float>(op[kOp_bendrange]);
        const float vol = static_cast<float>(v.tone ? op[kOp_bvol] : op[kOp_avol]) / 99.0f;
        const float bal = static_cast<float>(perf[kPerf_balance]) / 64.0f;
        const float at_amp = 1.0f + at * static_cast<float>(op[kOp_atamp]) / 15.0f;

        float buf[2][kCtl];
        uint32_t inc[kCtl];
        bool alive = false;
        const int first = d.linesel == 1 ? 1 : 0, last = d.linesel == 0 ? 0 : 1;
        for (int l = first; l <= last; ++l) {
            Line& ln = v.line[l];
            const int s = source_line(v.tone, l);
            const EnvDef* ed = d.env[s];
            const float kfa = 1.0f + static_cast<float>(d.kfa[s]) * kfn / 96.0f;
            env_tick(ln.env[0], ed[0], v.gate, chip, k_att, k_rel);
            env_tick(ln.env[1], ed[1], v.gate, chip, k_att, k_rel);
            env_tick(ln.env[2], ed[2], v.gate, chip * kfa, k_att, k_rel);
            if (ln.env[2].stage <= ed[2].end || ln.env[2].cur > 0) alive = true;

            float note = base + ln.env[0].cur * (1.0f - velf * d.velp[s]);
            if (l == 1 && d.linesel >= 2) note += d.detune + static_cast<float>(perf[kPerf_detune]) / 100.0f;
            const float hz = 440.0f * std::exp2((note - 69.0f) / 12.0f);
            const float fi = std::min(hz, fs * 0.45f) * 4294967296.0f / fs;
            const uint32_t i0 = static_cast<uint32_t>(fi);
            // noise modulation: line 2 jumps 0 or 32 semitones, every 8 chip samples
            if (l == 1 && d.mod == 2 && d.linesel >= 2) {
                const float period = 8.0f * fs / kChipRate;
                const uint32_t ihi = static_cast<uint32_t>(std::min(fi * 6.3496f, 4294967296.0f * 0.45f));
                for (int i = 0; i < n; ++i) {
                    if ((ln.noise_left -= 1.0f) <= 0) {
                        ln.noise_left += period;
                        ln.noise_up = (xorshift(v.rng) >> 31) != 0;
                    }
                    inc[i] = ln.noise_up ? ihi : i0;
                }
            } else {
                for (int i = 0; i < n; ++i) inc[i] = i0;
            }
            // DCW: envelope, velocity, key follow, performance offset; the chip
            // narrows it for high notes
            float dcw = ln.env[1].cur * (1.0f - velf * d.velw[s]);
            if (d.kfw[s] && kfn > 0) dcw *= 96.0f / (96.0f + static_cast<float>(d.kfw[s]) * kfn);
            dcw += static_cast<float>(perf[kPerf_dcw]) * 8.0f;
            const float limit = 1024.0f - std::min(1024.0f, hz * 0.2048f);
            dcw = std::clamp(std::min(dcw, limit), 0.0f, 1023.0f);
            cz_osc_render(&ln.osc, buf[l], static_cast<unsigned>(n), d.wave[s][0], d.wave[s][1], d.window[s],
                          ln.dcw, dcw, inc, set[kSet_chip]);
            ln.dcw = dcw;
            // amplitude: DCA law, velocity, line level, balance, aftertouch, tone volume
            float a = dca_amp(ln.env[2].cur) * (1.0f - velf * d.vela[s]) * d.level[s] * vol * at_amp;
            a *= l == 0 ? std::min(1.0f, 1.0f - bal) : std::min(1.0f, 1.0f + bal);
            const float a0 = ln.amp, da = (a - a0) / static_cast<float>(n);
            for (int i = 0; i < n; ++i) {
                ln.dc += (buf[l][i] - ln.dc) * (1.0f / 1024.0f);
                buf[l][i] = (buf[l][i] - ln.dc) * (a0 + da * static_cast<float>(i + 1));
            }
            ln.amp = a;
        }
        if (!alive) { v.active = false; return; }
        if (first == last) {
            for (int i = 0; i < n; ++i) out[i] += buf[first][i];
        } else if (d.mod == 1) {
            // ring: line 2 times line 1, line 1 heard as well (as the chip does)
            for (int i = 0; i < n; ++i) out[i] += buf[0][i] + buf[1][i] * buf[0][i] * 2.0f;
        } else {
            for (int i = 0; i < n; ++i) out[i] += buf[0][i] + buf[1][i];
        }
    }

    void render(float* l, float* r, int n) {
        if (static_cast<int>(dry.size()) < kCtl) { dry.assign(kCtl, 0); wet.assign(kCtl, 0); }
        const float gain = 0.2f * static_cast<float>(perf[kPerf_volume]) / 127.0f;
        const float mix = static_cast<float>(perf[kPerf_chorus]) / 127.0f;
        for (int at0 = 0; at0 < n; at0 += kCtl) {
            const int m = std::min(kCtl, n - at0);
            std::fill(dry.begin(), dry.begin() + m, 0.0f);
            std::fill(wet.begin(), wet.begin() + m, 0.0f);
            for (Voice& v : voice) {
                if (!v.active) continue;
                const bool ch = v.tone ? op[kOp_bchorus] : op[kOp_achorus];
                render_voice(v, ch ? wet.data() : dry.data(), m);
            }
            float* L = l + at0;
            float* R = r + at0;
            for (int i = 0; i < m; ++i) L[i] = R[i] = dry[i];
            chorus.run(wet.data(), L, R, m, mix);
            for (int i = 0; i < m; ++i) {
                L[i] = std::tanh(L[i] * gain);
                R[i] = std::tanh(R[i] * gain);
            }
        }
    }

    // ---- the easy envelope: writes steps 1-3 of the selected envelope ----
    void apply_easy() {
        uint8_t* b = tone[et()].b;
        const int line = sel[kSel_line], e = sel[kSel_env];
        const int peak = 99, sus = easy[kEasy_esustain];
        auto rate = [](int time) { return 99 - time; };
        cz_set(b, CZP_END, line, e, 3);
        cz_set(b, CZP_RATE1, line, e, rate(easy[kEasy_eattack]));
        cz_set(b, CZP_LEVEL1, line, e, peak);
        cz_set(b, CZP_RATE1 + 1, line, e, rate(easy[kEasy_edecay]));
        cz_set(b, CZP_LEVEL1 + 1, line, e, sus);
        cz_set(b, CZP_SUS, line, e, 2);
        cz_set(b, CZP_RATE1 + 2, line, e, rate(easy[kEasy_erelease]));
        cz_set(b, CZP_LEVEL1 + 2, line, e, 0);
        if (e == CZ_ENV_DCO) {                  // a pitch envelope rests at zero
            cz_set(b, CZP_LEVEL1, line, e, 0);
            cz_set(b, CZP_LEVEL1 + 1, line, e, 0);
        }
    }
};

Synth::Synth() : p_(new Impl()) {}
Synth::~Synth() { delete p_; }

void Synth::set_sample_rate(float fs) {
    if (fs <= 0 || fs == p_->fs) return;
    p_->fs = fs;
    p_->chorus.init(fs);
}

int Synth::get_control(const Control& c) const {
    const Impl& p = *p_;
    const uint8_t* b = p.tone[p.et()].b;
    switch (c.kind) {
        case 'T': return cz_get(b, c.index, 0, 0);
        case 'L': return cz_get(b, c.index, p.sel[kSel_line], 0);
        case 'E': return cz_get(b, c.index, p.sel[kSel_line], p.sel[kSel_env]);
        case 'S': return p.sel[c.index];
        case 'O': return p.op[c.index];
        case 'D': return p.easy[c.index];
        case 'P': return p.perf[c.index];
        case 'G': return p.set[c.index];
        default: return 0;
    }
}

void Synth::set_control(const Control& c, int v) {
    Impl& p = *p_;
    v = std::clamp(v, c.lo, c.hi);
    uint8_t* b = p.tone[p.et()].b;
    switch (c.kind) {
        case 'T': cz_set(b, c.index, 0, 0, v); break;
        case 'L': cz_set(b, c.index, p.sel[kSel_line], 0, v); break;
        case 'E': cz_set(b, c.index, p.sel[kSel_line], p.sel[kSel_env], v); break;
        case 'S': p.sel[c.index] = v; return;
        case 'D': p.easy[c.index] = v; p.apply_easy(); break;
        case 'P': p.perf[c.index] = v; return;
        case 'G': p.set[c.index] = v; return;
        case 'O': {
            const int old = p.op[c.index];
            p.op[c.index] = v;
            if ((c.index == kOp_bsrc || c.index == kOp_bnum) && old != v) p.load_b();
            if (c.index == kOp_keymode && old != v) p.silence_all();
            return;
        }
        default: return;
    }
    if (p.compare) p.compare = false;
    p.redecode(p.et());
}

std::string Synth::control_text(const Control& c, int v) const {
    if (c.names && *c.names) {
        std::string all = c.names;
        int k = v - c.lo;
        size_t start = 0;
        while (k-- > 0) {
            start = all.find('|', start);
            if (start == std::string::npos) return std::to_string(v);
            ++start;
        }
        const size_t end = all.find('|', start);
        return all.substr(start, end == std::string::npos ? std::string::npos : end - start);
    }
    if (std::strcmp(c.name, "split") == 0 || std::strcmp(c.name, "bnum") == 0) {
        if (c.name[0] == 'b') {
            const int s = p_->op[kOp_bsrc];
            return std::to_string(v) + " " + (s ? user_name(v) : patch_name(v));
        }
        static const char* nn[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        return std::string(nn[v % 12]) + std::to_string(v / 12 - 1);
    }
    return std::to_string(v);
}

bool Synth::select_patch(int i, int t) {
    if (i < 0 || i >= patch_count()) return false;
    if (t < 0) t = p_->et();
    p_->set_tone(t, factory(i), g_factory.empty() ? kInit : kFactory, i);
    return true;
}

bool Synth::select_user(int i, int t) {
    if (i < 0 || i >= kUserSlots) return false;
    if (t < 0) t = p_->et();
    p_->set_tone(t, g_user[i], kUser, i);
    return true;
}

void Synth::select_init(int t) {
    if (t < 0) t = p_->et();
    p_->set_tone(t, init_tone(), kInit, 0);
}

void Synth::reload(int t) {
    if (t < 0) t = p_->et();
    p_->tone[t] = p_->stored[t];
    p_->compare = false;
    p_->redecode(t);
}

void Synth::set_compare(bool on) {
    p_->compare = on;
    p_->redecode(0);
    p_->redecode(1);
}
bool Synth::compare() const { return p_->compare; }

bool Synth::store(int slot) {
    const int t = p_->et();
    if (slot < 0) slot = p_->src[t] == kUser ? p_->idx[t] : -1;
    if (slot < 0 || slot >= kUserSlots) return false;
    g_user[slot] = p_->tone[t];
    p_->stored[t] = p_->tone[t];
    p_->src[t] = kUser;
    p_->idx[t] = slot;
    return true;
}

void Synth::load_tone(const uint8_t* b, int t) {
    if (t < 0) t = p_->et();
    Tone x;
    std::memcpy(x.b, b, kToneBytes);
    p_->set_tone(t, x, kSysex, 0);
}

const uint8_t* Synth::tone(int t) const { return p_->tone[t < 0 ? p_->et() : t].b; }
int Synth::source(int t) const { return p_->src[t < 0 ? p_->et() : t]; }
int Synth::patch(int t) const { return p_->idx[t < 0 ? p_->et() : t]; }
std::string Synth::name(int t) const {
    char n[CZ_NAME_LEN + 1];
    cz_name_get(tone(t), n);
    return n;
}
void Synth::set_name(const char* n) { cz_name_set(p_->tone[p_->et()].b, n); }
bool Synth::edited(int t) const {
    if (t < 0) t = p_->et();
    return std::memcmp(p_->tone[t].b, p_->stored[t].b, kToneBytes) != 0;
}
int Synth::edit_tone() const { return p_->et(); }

void Synth::copy_line(int from, int to) {
    cz_copy_line(p_->tone[p_->et()].b, from, to);
    p_->redecode(p_->et());
}

void Synth::copy_env(int fl, int fe, int tl, int te) {
    uint8_t* b = p_->tone[p_->et()].b;
    const int end = cz_get(b, CZP_END, fl, fe);
    cz_set(b, CZP_END, tl, te, end);
    for (int j = 0; j < 8; ++j) {
        cz_set(b, CZP_RATE1 + j, tl, te, cz_get(b, CZP_RATE1 + j, fl, fe));
        cz_set(b, CZP_LEVEL1 + j, tl, te, cz_get(b, CZP_LEVEL1 + j, fl, fe));
    }
    cz_set(b, CZP_SUS, tl, te, cz_get(b, CZP_SUS, fl, fe));
    p_->redecode(p_->et());
}

bool Synth::select_op(int slot) {
    if (slot < 0 || slot >= kOpSlots) return false;
    std::memcpy(p_->op, g_ops[slot], sizeof p_->op);
    p_->op_slot = slot;
    p_->load_b();
    return true;
}

bool Synth::store_op(int slot) {
    if (slot < 0) slot = p_->op_slot;
    if (slot < 0 || slot >= kOpSlots) return false;
    std::memcpy(g_ops[slot], p_->op, sizeof p_->op);
    g_op_used[slot] = true;
    p_->op_slot = slot;
    return true;
}

void Synth::init_op() {
    std::memcpy(p_->op, kOpDefaults, sizeof p_->op);
    p_->op_slot = -1;
    p_->load_b();
}
int Synth::op_slot() const { return p_->op_slot; }

void Synth::note_on(int key, int velocity) { p_->note_on(key, std::clamp(velocity, 1, 127)); }
void Synth::note_off(int key) { p_->note_off(key); }
void Synth::set_bend(float v) { p_->bend = std::clamp(v, -1.0f, 1.0f); }
void Synth::set_mod(float v) { p_->mod = std::clamp(v, 0.0f, 1.0f); }
void Synth::set_aftertouch(float v) { p_->at = std::clamp(v, 0.0f, 1.0f); }
void Synth::set_sustain(bool on) { p_->set_sustain(on); }
bool Synth::sustain() const { return p_->sustain; }
void Synth::all_notes_off() {
    p_->held[0].clear();
    p_->held[1].clear();
    p_->sustain = false;
    for (Voice& v : p_->voice) if (v.active && v.gate) p_->release(v);
}
void Synth::silence() { p_->silence_all(); }
int Synth::voices_sounding() const {
    int n = 0;
    for (const Voice& v : p_->voice) n += v.active;
    return n;
}

void Synth::render(float* l, float* r, int n) { p_->render(l, r, n); }

void Synth::dump_tone(SysexOut out, void* ctx, int channel, int prog, int t) const {
    uint8_t msg[8 + 2 * kToneBytes];
    const int n = cz_syx_tone(tone(t), channel, prog, 1, msg);
    out(ctx, msg, n);
}

void Synth::dump_user(SysexOut out, void* ctx, int channel) const {
    uint8_t msg[8 + 2 * kToneBytes];
    for (int s = 0; s < kUserSlots; ++s) out(ctx, msg, cz_syx_tone(g_user[s].b, channel, 0x20 + s, 1, msg));
}

// Program numbers of the handshake: 0x60 the playing tone (A), 0x20..0x5F the
// user slots 0..63, 0x00..0x1F the factory tones 0..31.
bool Synth::tone_for_prog(int prog, uint8_t* out) const {
    if (prog == 0x60) { std::memcpy(out, tone(0), kToneBytes); return true; }
    if (prog >= 0x20 && prog < 0x20 + kUserSlots) { std::memcpy(out, g_user[prog - 0x20].b, kToneBytes); return true; }
    if (prog >= 0 && prog < 0x20) { std::memcpy(out, factory(prog).b, kToneBytes); return true; }
    return false;
}

bool Synth::receive_prog(int prog, const uint8_t* b) {
    if (!cz_tone_valid(b)) return false;
    if (prog >= 0x20 && prog < 0x20 + kUserSlots) {
        std::memcpy(g_user[prog - 0x20].b, b, kToneBytes);
        if (p_->src[0] == kUser && p_->idx[0] == prog - 0x20) select_user(prog - 0x20, 0);
        return true;
    }
    load_tone(b, 0);
    return true;
}

}  // namespace cz4
