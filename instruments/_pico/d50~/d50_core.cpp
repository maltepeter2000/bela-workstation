// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Michi71 (PicoFaceD5 bridge this derives from)
// SPDX-FileCopyrightText: 2026 Peter (Pure Data adaptation)
//
// d50_core.cpp -- see d50_core.h. The patch handling follows
// instruments/PicoFaceD5/src/D5_Bridge.cpp of PicoVintageSynthCollection;
// where a comment says "as the bridge", the reasoning is in that file.
#include "d50_core.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "d5_engine/d5_patch.h"
#include "d5_engine/d5_patch_map.h"
#include "d5_presets.h"

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace d50 {

// ---------------------------------------------------------------------------
// Shared data
// ---------------------------------------------------------------------------
namespace {

constexpr size_t kPcmWords = 262144;

struct Store {
    std::vector<int16_t> pcm;
    std::vector<std::array<uint8_t, kPatchBytes>> patches;
    std::vector<std::string> names;
};

Store& store() {
    static Store s;
    return s;
}

// The D-50 panel's character set, as the firmware and the converter use it.
const char kChars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                      "abcdefghijklmnopqrstuvwxyz1234567890-";

bool read_file(const char* path, std::vector<uint8_t>& out, std::string& err) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { err = std::string("cannot open ") + path; return false; }
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) { std::fclose(f); err = "cannot size the file"; return false; }
    out.resize(static_cast<size_t>(n));
    const size_t got = n ? std::fread(out.data(), 1, out.size(), f) : 0;
    std::fclose(f);
    if (got != out.size()) { err = "short read"; return false; }
    return true;
}

}  // namespace

bool load_pcm(const char* path, std::string& err) {
    std::vector<uint8_t> raw;
    if (!read_file(path, raw, err)) return false;
    if (raw.size() != kPcmWords * 2) {
        err = "not a converted D-50 PCM file (expected 524288 bytes; "
              "run tools/make_d50_data.py on the ROM images)";
        return false;
    }
    Store& s = store();
    s.pcm.resize(kPcmWords);
    for (size_t i = 0; i < kPcmWords; ++i) {
        s.pcm[i] = static_cast<int16_t>(
            static_cast<uint16_t>(raw[2 * i] | (raw[2 * i + 1] << 8)));
    }
    return true;
}

bool pcm_loaded() { return store().pcm.size() == kPcmWords; }

// A bulk dump is the machine's own: DT1 messages (F0 41 dev 14 12 aa bb cc
// data checksum F7) from address 02-00-00 upwards, 448 bytes per patch. This
// is tools/d5_extract/d5_syx_to_patches.py in C++, including its range checks
// -- a file that is not a D-50 bank is refused rather than played.
int load_syx(const char* path, std::string& err) {
    std::vector<uint8_t> raw;
    if (!read_file(path, raw, err)) return -1;

    constexpr int kBase = 0x8000, kSlots = 64;
    std::vector<uint8_t> mem(kSlots * kPatchBytes, 0);
    std::vector<uint8_t> have(kSlots * kPatchBytes, 0);
    int messages = 0;
    size_t i = 0;
    while (i < raw.size()) {
        if (raw[i] != 0xF0) { ++i; continue; }
        size_t e = i + 1;
        while (e < raw.size() && raw[e] != 0xF7) ++e;
        if (e >= raw.size()) break;
        const uint8_t* m = raw.data() + i;
        const size_t len = e - i + 1;
        i = e + 1;
        if (len < 10 || m[1] != 0x41 || m[3] != 0x14 || m[4] != 0x12) continue;
        const int addr = m[5] * 16384 + m[6] * 128 + m[7];
        unsigned total = m[5] + m[6] + m[7];
        for (size_t k = 8; k + 2 < len; ++k) total += m[k];
        if (((128 - total % 128) % 128) != m[len - 2]) {
            err = "checksum mismatch in the dump";
            return -1;
        }
        for (size_t k = 8; k + 2 < len; ++k) {
            const long a = static_cast<long>(addr) + static_cast<long>(k - 8) - kBase;
            if (a >= 0 && a < static_cast<long>(mem.size())) {
                mem[a] = m[k] & 0x7F;
                have[a] = 1;
            }
        }
        ++messages;
    }
    if (!messages) { err = "no D-50 data messages in this file"; return -1; }

    int found = 0;
    while (found < kSlots && have[found * kPatchBytes + kPatchBytes - 1]) ++found;
    if (!found) { err = "no patch memory at address 02-00-00"; return -1; }

    // (block, offset, maximum): if the layout were wrong these leave range.
    static const int kChecks[][3] = {
        {2, 10, 6}, {5, 10, 6}, {2, 42, 7}, {6, 18, 8},
        {6, 30, 31}, {0, 7, 99}, {0, 6, 1}, {0, 14, 30}};
    for (int p = 0; p < found; ++p) {
        for (const auto& c : kChecks) {
            if (mem[p * kPatchBytes + c[0] * 64 + c[1]] > c[2]) {
                err = "a parameter is out of range -- not a D-50 patch bank";
                return -1;
            }
        }
    }

    Store& s = store();
    for (int p = 0; p < found; ++p) {
        std::array<uint8_t, kPatchBytes> bytes;
        std::memcpy(bytes.data(), &mem[p * kPatchBytes], kPatchBytes);
        s.patches.push_back(bytes);
        std::string name;
        for (int k = 0; k < 18; ++k) {
            const int v = bytes[6 * 64 + k] & 0x3F;
            name += (v < static_cast<int>(sizeof(kChars)) - 1) ? kChars[v] : ' ';
        }
        while (!name.empty() && name.back() == ' ') name.pop_back();
        s.names.push_back(name);
    }
    return found;
}

bool have_bank() { return !store().patches.empty(); }

int patch_count() {
    return have_bank() ? static_cast<int>(store().patches.size()) : d5::kPresetCount;
}

const char* patch_name(int index) {
    if (index < 0 || index >= patch_count()) return "";
    if (have_bank()) return store().names[index].c_str();
    return d5::preset(index).name;
}

// ---------------------------------------------------------------------------
// Resampler: 32 kHz engine -> host rate. A 64-tap Kaiser-windowed sinc, 256
// phases with linear interpolation between them.
// ---------------------------------------------------------------------------
namespace {

constexpr int kTaps = 64;
constexpr int kPhases = 256;

double bessel_i0(double x) {
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k) {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}

class Resampler {
public:
    void configure(float out_rate) {
        bypass_ = std::fabs(out_rate - kEngineRate) < 0.5f;
        step_ = static_cast<double>(kEngineRate) / out_rate;
        phase_ = 0.0;
        // Cutoff below the lower Nyquist, leaving the window's transition
        // band (about 2.2 kHz at 64 taps) on the stop side of it.
        const double lower = out_rate < kEngineRate ? out_rate / kEngineRate : 1.0;
        const double fc = 0.5 * lower - 0.034;      // cycles per input sample
        const double beta = 7.0;
        const double i0b = bessel_i0(beta);
        const double kPi = 3.14159265358979323846;
        for (int p = 0; p <= kPhases; ++p) {
            const double frac = static_cast<double>(p) / kPhases;
            double sum = 0.0;
            for (int k = 0; k < kTaps; ++k) {
                const double d = frac - kTaps / 2 + k;   // distance in input samples
                const double w = d / (kTaps / 2);
                double win = 0.0;
                if (w > -1.0 && w < 1.0)
                    win = bessel_i0(beta * std::sqrt(1.0 - w * w)) / i0b;
                const double x = 2.0 * kPi * fc * d;
                const double sinc = std::fabs(d) < 1e-9 ? 2.0 * fc
                                                        : std::sin(x) / (kPi * d);
                kernel_[p][k] = static_cast<float>(sinc * win);
                sum += sinc * win;
            }
            for (int k = 0; k < kTaps; ++k)
                kernel_[p][k] = static_cast<float>(kernel_[p][k] / sum);
        }
        clear();
    }

    void clear() {
        std::memset(hl_, 0, sizeof hl_);
        std::memset(hr_, 0, sizeof hr_);
        w_ = 0;
    }

    template <typename Source>
    void process(Source&& next, float* l, float* r, int frames) {
        if (bypass_) {
            for (int i = 0; i < frames; ++i) next(l[i], r[i]);
            return;
        }
        for (int i = 0; i < frames; ++i) {
            phase_ += step_;
            while (phase_ >= 1.0) {
                float a, b;
                next(a, b);
                // Written twice so a read never has to wrap.
                hl_[w_] = hl_[w_ + kTaps] = a;
                hr_[w_] = hr_[w_ + kTaps] = b;
                w_ = (w_ + 1) % kTaps;
                phase_ -= 1.0;
            }
            const double pf = phase_ * kPhases;
            const int p = static_cast<int>(pf);
            const float t = static_cast<float>(pf - p);
            const float* k0 = kernel_[p];
            const float* k1 = kernel_[p + 1];
            // Newest sample is at w_ - 1; x[newest - k] pairs with tap k.
            const float* xl = hl_ + w_ + kTaps - 1;
            const float* xr = hr_ + w_ + kTaps - 1;
            float sl = 0.0f, sr = 0.0f;
            for (int k = 0; k < kTaps; ++k) {
                const float c = k0[k] + t * (k1[k] - k0[k]);
                sl += c * xl[-k];
                sr += c * xr[-k];
            }
            l[i] = sl;
            r[i] = sr;
        }
    }

private:
    float kernel_[kPhases + 1][kTaps];
    float hl_[2 * kTaps], hr_[2 * kTaps];
    int w_ = 0;
    double phase_ = 0.0, step_ = 1.0;
    bool bypass_ = true;
};

// Denormals in a decaying reverb tail cost an x86 an order of magnitude; the
// engine was written for a core where they do not.
class NoDenormals {
public:
    NoDenormals() {
#if defined(__SSE__) || defined(__x86_64__)
        saved_ = _mm_getcsr();
        _mm_setcsr(saved_ | 0x8040);   // FTZ | DAZ
#elif defined(__aarch64__)
        __asm__ volatile("mrs %0, fpcr" : "=r"(saved_));
        uint64_t v = saved_ | (1ull << 24);   // FZ
        __asm__ volatile("msr fpcr, %0" : : "r"(v));
#endif
    }
    ~NoDenormals() {
#if defined(__SSE__) || defined(__x86_64__)
        _mm_setcsr(saved_);
#elif defined(__aarch64__)
        __asm__ volatile("msr fpcr, %0" : : "r"(saved_));
#endif
    }
private:
#if defined(__SSE__) || defined(__x86_64__)
    unsigned saved_ = 0;
#elif defined(__aarch64__)
    uint64_t saved_ = 0;
#endif
};

const char* const kStructureNames[7] = {
    "S+S", "S*S ring", "P+S", "P*S ring", "S*P ring", "P+P", "P*P ring"};

int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

}  // namespace

// ---------------------------------------------------------------------------
// Synth
// ---------------------------------------------------------------------------
struct Synth::Impl {
    d5::Patch patch{};
    Resampler rs;
    float out_rate = kEngineRate;

    int patch_index = 0;
    std::string name;
    int structure = 1;
    uint8_t temp[kPatchBytes] = {};

    int volume = 80;            // as the bridge: headroom for sixteen voices
    int tune = 0;
    float base_volume = 1.0f;
    float bend = 0.0f;
    int bend_range = 2;
    int voice_limit = 8;
    bool whole_mode = false;
    bool porta = false;
    int porta_time = 0;
    int reverb = 30, chorus = 50;
    int cho_type = 0, cho_rate = 35, cho_depth = 50;
    int eq_lo_f = 8, eq_lo_g = 12, eq_hi_f = 12, eq_hi_q = 3, eq_hi_g = 12;
    int tone_bal = 50;

    uint8_t held[128] = {};
    uint8_t sustained[128] = {};
    bool sustain = false;
    int active = 0;

    bool governor = false;
    int shed_holdoff = 0;
    int cpu_peak = 0;

    int note_limit() const { return whole_mode ? 2 * voice_limit : voice_limit; }

    void apply_levels() {
        patch.set_volume(base_volume * volume * 0.01f);
        patch.set_reverb_balance(reverb * 0.01f);
        patch.set_chorus_balance(chorus * 0.01f);
        patch.set_master_cents(static_cast<float>(tune));
    }

    void apply_eq() {
        d5::EqSpec e;
        e.low_freq = eq_lo_f;
        e.low_gain_db = static_cast<float>(eq_lo_g) - 12.0f;
        e.high_freq = eq_hi_f;
        e.high_q = eq_hi_q;
        e.high_gain_db = static_cast<float>(eq_hi_g) - 12.0f;
        patch.set_eq(e);
    }

    void apply_patch() {
        Store& s = store();
        const int16_t* blob = s.pcm.data();
        d5::PatchSpec spec;
        if (have_bank()) {
            std::memcpy(temp, s.patches[patch_index].data(), kPatchBytes);
            spec = d5::patch_from_bytes(temp, blob);
            name = s.names[patch_index];
            const uint8_t* pb = d5::patch_block(temp, d5::kBlkPatch);
            const uint8_t* uc = d5::patch_block(temp, d5::kBlkUpperCommon);
            reverb = clamp(pb[31], 0, 100);
            chorus = clamp(uc[45], 0, 100);
            cho_type = clamp(uc[42], 0, 7);
            cho_rate = clamp(uc[43], 0, 100);
            cho_depth = clamp(uc[44], 0, 100);
            eq_lo_f = clamp(uc[37], 0, 15);
            eq_lo_g = clamp(uc[38], 0, 24);
            eq_hi_f = clamp(uc[39], 0, 21);
            eq_hi_q = clamp(uc[40], 0, 8);
            eq_hi_g = clamp(uc[41], 0, 24);
            tone_bal = clamp(pb[33], 0, 100);
        } else {
            d5::Preset pr = d5::preset(patch_index % d5::kPresetCount);
            d5::preset_bind(pr.spec, blob, pr.pcm1, pr.pcm2);
            spec = pr.spec;
            name = pr.name;
            reverb = 30;
            chorus = 50;
        }
        base_volume = spec.volume;
        whole_mode = spec.key_mode == d5::KeyMode::kWhole;
        // A patch change ends the controller overrides, as on the D-50.
        porta = spec.upper.voice.porta_switch;
        porta_time = spec.upper.voice.porta_time;
        bend_range = spec.bend_range;
        structure = spec.upper.voice.structure;

        patch.configure(spec, kEngineRate);
        apply_levels();
        patch.set_bend_semis(bend * bend_range);
    }
};

Synth::Synth() : impl_(nullptr) {
    if (!pcm_loaded()) return;
    // Zeroed storage first: the engine was written for static objects, and
    // a few of its members (delay lines, the pending-note queue) rely on
    // starting at zero rather than being initialised by a constructor.
    void* mem = std::calloc(1, sizeof(Impl));
    if (!mem) return;
    impl_ = new (mem) Impl;
    impl_->rs.configure(kEngineRate);
    impl_->apply_patch();
}

Synth::~Synth() {
    if (!impl_) return;
    impl_->~Impl();
    std::free(impl_);
}

void Synth::set_sample_rate(float sr) {
    if (!impl_ || sr <= 0.0f) return;
    if (std::fabs(sr - impl_->out_rate) < 0.5f) return;
    impl_->out_rate = sr;
    impl_->rs.configure(sr);
}

void Synth::render(float* left, float* right, int frames) {
    if (!impl_) {
        std::memset(left, 0, sizeof(float) * frames);
        std::memset(right, 0, sizeof(float) * frames);
        return;
    }
    Impl& m = *impl_;
    const auto t0 = std::chrono::steady_clock::now();
    {
        NoDenormals guard;
        m.rs.process([&m](float& l, float& r) { m.patch.next_stereo(l, r); },
                     left, right, frames);
    }
    const double us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - t0).count();
    const double budget = frames * 1.0e6 / m.out_rate;
    const int load = budget > 0.0 ? static_cast<int>(us * 100.0 / budget) : 0;
    if (load > m.cpu_peak) m.cpu_peak = load > 999 ? 999 : load;
    else if (m.cpu_peak > 0) --m.cpu_peak;

    // The bridge's CPU governor: retire the longest-ringing released tail
    // when a block runs hot. Held keys are never touched.
    if (m.governor) {
        if (load > 92) {
            m.shed_holdoff = 0;
            m.patch.shed_voice();
        } else if (load > 86) {
            if (++m.shed_holdoff >= 6) { m.shed_holdoff = 0; m.patch.shed_voice(); }
        } else if (m.shed_holdoff > 0) {
            --m.shed_holdoff;
        }
    }
}

void Synth::note_on(int note, float velocity) {
    if (!impl_ || note < 0 || note > 127) return;
    Impl& m = *impl_;
    if (velocity > 1.0f) velocity = 1.0f;
    // As the bridge: only a genuinely new note may push out the oldest one.
    if (m.active >= m.note_limit() && !m.held[note]) {
        for (int n = 0; n < 128; ++n) {
            if (m.held[n]) {
                m.patch.note_off(n);
                m.held[n] = 0;
                m.sustained[n] = 0;
                --m.active;
                break;
            }
        }
    }
    m.patch.note_on(note, velocity);
    if (!m.held[note]) ++m.active;
    m.held[note] = 1;
    m.sustained[note] = 0;
}

void Synth::note_off(int note) {
    if (!impl_ || note < 0 || note > 127) return;
    Impl& m = *impl_;
    if (m.sustain && m.held[note]) { m.sustained[note] = 1; return; }
    m.patch.note_off(note);
    if (m.held[note]) { m.held[note] = 0; if (m.active > 0) --m.active; }
}

void Synth::all_notes_off() {
    if (!impl_) return;
    Impl& m = *impl_;
    m.sustain = false;
    for (int n = 0; n < 128; ++n) {
        m.sustained[n] = 0;
        if (m.held[n]) { m.patch.note_off(n); m.held[n] = 0; }
    }
    m.active = 0;
}

void Synth::silence() {
    if (!impl_) return;
    all_notes_off();
    impl_->patch.silence();
    impl_->rs.clear();
}

bool Synth::select_patch(int index) {
    if (!impl_ || index < 0 || index >= patch_count()) return false;
    all_notes_off();
    impl_->patch_index = index;
    impl_->apply_patch();
    return true;
}

int Synth::patch() const { return impl_ ? impl_->patch_index : 0; }
const char* Synth::name() const { return impl_ ? impl_->name.c_str() : ""; }

const char* Synth::structure_name() const {
    if (!impl_) return "";
    const int s = impl_->structure;
    return kStructureNames[(s < 1 || s > 7) ? 0 : s - 1];
}

void Synth::set_volume(int percent) {
    if (!impl_) return;
    impl_->volume = clamp(percent, 0, 100);
    impl_->apply_levels();
}

void Synth::set_tune(int cents) {
    if (!impl_) return;
    impl_->tune = clamp(cents, -50, 50);
    impl_->apply_levels();
}

void Synth::set_bend(float normalized) {
    if (!impl_) return;
    if (normalized < -1.0f) normalized = -1.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    impl_->bend = normalized;
    impl_->patch.set_bend_semis(normalized * impl_->bend_range);
}

void Synth::set_bend_range(int semis) {
    if (!impl_) return;
    impl_->bend_range = clamp(semis, 0, 12);
    impl_->patch.set_bend_semis(impl_->bend * impl_->bend_range);
}

void Synth::set_mod(float w) {
    if (impl_) impl_->patch.set_mod_wheel(w < 0 ? 0 : (w > 1 ? 1 : w));
}

void Synth::set_aftertouch(float a) {
    if (impl_) impl_->patch.set_aftertouch(a < 0 ? 0 : (a > 1 ? 1 : a));
}

void Synth::set_sustain(bool on) {
    if (!impl_) return;
    Impl& m = *impl_;
    if (m.sustain == on) return;
    m.sustain = on;
    if (on) return;
    for (int n = 0; n < 128; ++n) {
        if (!m.sustained[n]) continue;
        m.sustained[n] = 0;
        m.patch.note_off(n);
        if (m.held[n]) { m.held[n] = 0; if (m.active > 0) --m.active; }
    }
}

void Synth::set_porta(bool on) {
    if (!impl_) return;
    impl_->porta = on;
    impl_->patch.set_porta(on, impl_->porta_time);
}

void Synth::set_porta_time(int percent) {
    if (!impl_) return;
    impl_->porta_time = clamp(percent, 0, 100);
    impl_->patch.set_porta(impl_->porta, impl_->porta_time);
}

void Synth::set_voice_limit(int voices) {
    if (impl_) impl_->voice_limit = clamp(voices, 1, 8);
}

void Synth::set_reverb(int balance) {
    if (!impl_) return;
    impl_->reverb = clamp(balance, 0, 100);
    impl_->apply_levels();
}

void Synth::set_reverb_type(int type) {
    if (impl_) impl_->patch.set_reverb_type(clamp(type, 1, 32) - 1);
}

void Synth::set_chorus(int balance) {
    if (!impl_) return;
    impl_->chorus = clamp(balance, 0, 100);
    impl_->apply_levels();
}

void Synth::set_chorus_type(int type) {
    if (!impl_) return;
    impl_->cho_type = clamp(type, 1, 8) - 1;
    impl_->patch.set_chorus_type(impl_->cho_type);
}

void Synth::set_chorus_rate(int v) {
    if (!impl_) return;
    impl_->cho_rate = clamp(v, 0, 100);
    impl_->patch.set_chorus_rate(impl_->cho_rate * 0.01f);
}

void Synth::set_chorus_depth(int v) {
    if (!impl_) return;
    impl_->cho_depth = clamp(v, 0, 100);
    impl_->patch.set_chorus_depth(impl_->cho_depth / 100.0f);
}

void Synth::set_eq_low_freq(int v) {
    if (!impl_) return;
    impl_->eq_lo_f = clamp(v, 0, 15);
    impl_->apply_eq();
}

void Synth::set_eq_low_gain(int db) {
    if (!impl_) return;
    impl_->eq_lo_g = clamp(db, -12, 12) + 12;
    impl_->apply_eq();
}

void Synth::set_eq_high_freq(int v) {
    if (!impl_) return;
    impl_->eq_hi_f = clamp(v, 0, 21);
    impl_->apply_eq();
}

void Synth::set_eq_high_q(int v) {
    if (!impl_) return;
    impl_->eq_hi_q = clamp(v, 0, 8);
    impl_->apply_eq();
}

void Synth::set_eq_high_gain(int db) {
    if (!impl_) return;
    impl_->eq_hi_g = clamp(db, -12, 12) + 12;
    impl_->apply_eq();
}

void Synth::set_tone_balance(int v) {
    if (!impl_) return;
    impl_->tone_bal = clamp(v, 0, 100);
    impl_->patch.set_tone_balance(impl_->tone_bal * 0.01f);
}

void Synth::set_governor(bool on) {
    if (impl_) { impl_->governor = on; impl_->shed_holdoff = 0; }
}

Synth::State Synth::state() const {
    State s{};
    if (!impl_) return s;
    const Impl& m = *impl_;
    s.patch = m.patch_index;
    s.volume = m.volume;
    s.tune = m.tune;
    s.bend_range = m.bend_range;
    s.voice_limit = m.voice_limit;
    s.reverb = m.reverb;
    s.reverb_type = m.patch.reverb_type() + 1;
    s.chorus = m.chorus;
    s.chorus_type = m.cho_type + 1;
    s.chorus_rate = m.cho_rate;
    s.chorus_depth = m.cho_depth;
    s.eq_low_freq = m.eq_lo_f;
    s.eq_low_gain = m.eq_lo_g - 12;
    s.eq_high_freq = m.eq_hi_f;
    s.eq_high_q = m.eq_hi_q;
    s.eq_high_gain = m.eq_hi_g - 12;
    s.tone_balance = m.tone_bal;
    s.notes_held = m.active;
    s.voices_sounding = m.patch.voices_sounding();
    s.cpu_peak = m.cpu_peak;
    s.porta = m.porta;
    s.porta_time = m.porta_time;
    s.sustain = m.sustain;
    s.governor = m.governor;
    return s;
}

}  // namespace d50
