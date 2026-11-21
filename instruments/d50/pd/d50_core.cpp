// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Michi71 (PicoFaceD5 bridge this derives from)
// SPDX-FileCopyrightText: 2026 Peter (Pure Data adaptation)
//
// d50_core.cpp -- see d50_core.h. The patch handling follows
// instruments/PicoFaceD5/src/D5_Bridge.cpp of PicoVintageSynthCollection;
// where a comment says "as the bridge", the reasoning is in that file.
// The engine's headers are large inline functions by design, and Pd's method
// tables are casts by design; the build helper's warnings for both are noise.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include "d50_core.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
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

typedef std::array<uint8_t, kPatchBytes> PatchBytes;

constexpr int kBankBytes = kUserSlots * kPatchBytes;   // 02-00-00 up to 03-60-00
constexpr int kReverbBytes = 16 * 376;                 // sixteen reverb types

void init_patch(uint8_t* b);
std::string decode_name(const uint8_t* bytes, int len);

struct Store {
    std::vector<int16_t> pcm;
    std::vector<PatchBytes> patches;          // factory presets
    std::vector<std::string> names;
    PatchBytes user[kUserSlots];
    std::string user_names[kUserSlots];
    // What a D-50 keeps behind its 64 patches: the definitions of the reverb
    // types 17 to 32 (address 03-60-00). The engine has its own reverbs and
    // does not read this; it is carried along so that a bank written or sent
    // from here is a complete dump. Empty until a dump has brought one.
    std::vector<uint8_t> reverb;

    Store() {
        for (int i = 0; i < kUserSlots; ++i) {
            init_patch(user[i].data());
            user_names[i] = decode_name(user[i].data() + 6 * 64, 18);
        }
    }
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
namespace {
int read_syx(const char* path, std::vector<PatchBytes>& out, std::vector<uint8_t>& reverb,
             std::string& err) {
    std::vector<uint8_t> raw;
    if (!read_file(path, raw, err)) return -1;

    constexpr int kBase = 0x8000, kSlots = 64;
    std::vector<uint8_t> mem(kBankBytes + kReverbBytes, 0);
    std::vector<uint8_t> have(kBankBytes + kReverbBytes, 0);
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

    for (int p = 0; p < found; ++p) {
        PatchBytes bytes;
        std::memcpy(bytes.data(), &mem[p * kPatchBytes], kPatchBytes);
        out.push_back(bytes);
    }
    // The reverb block counts only when all of it is there.
    reverb.clear();
    bool whole = true;
    for (int k = 0; k < kReverbBytes && whole; ++k) whole = have[kBankBytes + k] != 0;
    if (whole) reverb.assign(mem.begin() + kBankBytes, mem.end());
    return found;
}
}  // namespace

int load_syx(const char* path, std::string& err) {
    std::vector<PatchBytes> got;
    std::vector<uint8_t> reverb;
    const int n = read_syx(path, got, reverb, err);
    if (n < 0) return n;
    Store& s = store();
    // Until the user bank brings its own, it carries the reverb data of the
    // first factory bank -- what a D-50 with that card holds.
    if (s.reverb.empty() && !reverb.empty()) s.reverb = reverb;
    for (const PatchBytes& b : got) {
        s.patches.push_back(b);
        s.names.push_back(decode_name(b.data() + 6 * 64, 18));
    }
    return n;
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

int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

#include "d50_params.h"

constexpr int kControlCount = static_cast<int>(sizeof kControls / sizeof kControls[0]);

const char* const kSectionNames[kSectionCount] = {
    "u1", "u2", "upper", "l1", "l2", "lower", "patch"};

// Largest value each of the 448 bytes may hold. Names are six bits.
struct Limits {
    uint8_t max[kPatchBytes];
    Limits() {
        std::memset(max, 127, sizeof max);
        for (int s = 0; s < kSectionCount; ++s) {
            int n = 0;
            const Param* p = section_params(s, n);
            for (int i = 0; i < n; ++i) max[s * 64 + p[i].offset] = p[i].max;
        }
        for (int k = 0; k < 10; ++k) max[kUpper * 64 + k] = max[kLower * 64 + k] = 63;
        for (int k = 0; k < 18; ++k) max[kPatch * 64 + k] = 63;
    }
};

const Limits& limits() {
    static const Limits l;
    return l;
}

void sanitize(uint8_t* b) {
    const Limits& lim = limits();
    for (int k = 0; k < kPatchBytes; ++k)
        if (b[k] > lim.max[k]) b[k] = lim.max[k];
}

void encode_name(uint8_t* dst, int len, const char* text) {
    bool end = false;
    for (int k = 0; k < len; ++k) {
        if (!end && text[k] == 0) end = true;
        const char* at = end ? nullptr : std::strchr(kChars, text[k]);
        dst[k] = at ? static_cast<uint8_t>(at - kChars) : 0;
    }
}

uint8_t roland_checksum(const uint8_t* body, int len) {
    unsigned sum = 0;
    for (int i = 0; i < len; ++i) sum += body[i] & 0x7F;
    return static_cast<uint8_t>((128 - sum % 128) % 128);
}

constexpr int kTempBase = 0x000000;
constexpr int kIntBase = 0x008000;   // 02-00-00

// 256 data bytes per message, the size of the machine's own dumps.
void send_dt1(SysexOut out, void* ctx, int dev, int addr, const uint8_t* data, int len) {
    while (len > 0) {
        const int n = len > 256 ? 256 : len;
        uint8_t msg[10 + 256];
        msg[0] = 0xF0; msg[1] = 0x41; msg[2] = dev & 0x7F; msg[3] = 0x14; msg[4] = 0x12;
        msg[5] = (addr >> 14) & 0x7F;
        msg[6] = (addr >> 7) & 0x7F;
        msg[7] = addr & 0x7F;
        for (int i = 0; i < n; ++i) msg[8 + i] = data[i] & 0x7F;
        msg[8 + n] = roland_checksum(msg + 5, 3 + n);
        msg[9 + n] = 0xF7;
        out(ctx, msg, 10 + n);
        data += n; addr += n; len -= n;
    }
}

// The whole user bank as one run of bytes: its messages step 256 bytes at a
// time across the patch boundaries, exactly as the machine's own dump does.
void user_run(std::vector<uint8_t>& run, int from, int size) {
    Store& s = store();
    while (size > 0 && from < kBankBytes) {
        const int slot = from / kPatchBytes, in = from % kPatchBytes;
        int m = kPatchBytes - in;
        if (m > size) m = size;
        const uint8_t* src = s.user[slot].data() + in;
        run.insert(run.end(), src, src + m);
        from += m; size -= m;
    }
    // ... and on into the reverb data, if a dump has brought any.
    const int rv = static_cast<int>(s.reverb.size());
    if (size > 0 && from - kBankBytes < rv) {
        int m = rv - (from - kBankBytes);
        if (m > size) m = size;
        run.insert(run.end(), s.reverb.begin() + (from - kBankBytes),
                   s.reverb.begin() + (from - kBankBytes) + m);
    }
}

int user_bytes() { return kBankBytes + static_cast<int>(store().reverb.size()); }

struct Collect {
    std::vector<uint8_t> bytes;
    static void put(void* ctx, const uint8_t* msg, int len) {
        Collect* c = static_cast<Collect*>(ctx);
        c->bytes.insert(c->bytes.end(), msg, msg + len);
    }
};

const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B"};

// A plain, neutral starting point: one sawtooth partial, filter open, no
// modulation, no effects. Written for d50~, not taken from any Roland bank.
void init_patch(uint8_t* b) {
    std::memset(b, 0, kPatchBytes);
    for (int s : {kU1, kU2, kL1, kL2}) {
        uint8_t* p = b + s * 64;
        p[0] = 36;  p[1] = 50;  p[2] = 11;          // C4, no detune, pitch follows the keys
        p[5] = 2;   p[6] = 1;                       // bender normal, sawtooth
        p[9] = 7;   p[12] = 7;                      // pulse width: no velocity, no aftertouch
        p[13] = 100; p[15] = 11; p[17] = 7;         // cutoff open, follows the keys, no bias
        p[27] = p[28] = p[29] = p[30] = 100;        // TVF envelope flat
        p[34] = 7;
        p[35] = 100; p[36] = 50; p[38] = 12;        // level, no velocity, no bias
        p[43] = 20;                                 // a short release
        p[44] = p[45] = p[46] = p[47] = 100;        // TVA envelope: organ
        p[53] = 7;
    }
    for (int s : {kUpper, kLower}) {
        uint8_t* c = b + s * 64;
        encode_name(c, 10, "Init");
        for (int k = 17; k <= 21; ++k) c[k] = 50;   // pitch envelope flat
        c[26] = c[30] = c[34] = 60;                 // LFO rates
        c[38] = 12; c[41] = 12;                     // equalizer flat
        c[43] = 50; c[44] = 50;                     // chorus rate and depth, balance 0
        c[46] = 1;  c[47] = 50;                     // first partial only
    }
    uint8_t* pb = b + kPatch * 64;
    encode_name(pb, 18, "Init Patch");
    pb[19] = 24;                                    // split at C4
    pb[22] = pb[23] = 24; pb[24] = pb[25] = 50;     // no key shift, no fine tune
    pb[26] = 2; pb[27] = 12;                        // bender 2 semitones, no aftertouch bend
    pb[32] = 100; pb[33] = 50;                      // level, tone balance
}

std::string decode_name(const uint8_t* bytes, int len) {
    std::string name;
    for (int k = 0; k < len; ++k) {
        const int v = bytes[k] & 0x3F;
        name += (v < static_cast<int>(sizeof(kChars)) - 1) ? kChars[v] : ' ';
    }
    while (!name.empty() && name.back() == ' ') name.pop_back();
    return name;
}

}  // namespace

const char* section_name(int section) {
    return (section >= 0 && section < kSectionCount) ? kSectionNames[section] : "";
}

int section_from_name(const char* name) {
    for (int s = 0; s < kSectionCount; ++s)
        if (std::strcmp(name, kSectionNames[s]) == 0) return s;
    return -1;
}

const Param* section_params(int section, int& count) {
    switch (section) {
        case kU1: case kU2: case kL1: case kL2:
            count = static_cast<int>(sizeof kPartialParams / sizeof kPartialParams[0]);
            return kPartialParams;
        case kUpper: case kLower:
            count = static_cast<int>(sizeof kCommonParams / sizeof kCommonParams[0]);
            return kCommonParams;
        case kPatch:
            count = static_cast<int>(sizeof kPatchParams / sizeof kPatchParams[0]);
            return kPatchParams;
        default:
            count = 0;
            return nullptr;
    }
}

const Param* find_param(int section, const char* name) {
    int n = 0;
    const Param* p = section_params(section, n);
    for (int i = 0; i < n; ++i)
        if (std::strcmp(p[i].name, name) == 0) return &p[i];
    return nullptr;
}

int control_count() { return kControlCount; }
const Control& control(int i) { return kControls[i]; }

const Control* find_control(const char* name) {
    for (int i = 0; i < kControlCount; ++i)
        if (std::strcmp(kControls[i].name, name) == 0) return &kControls[i];
    return nullptr;
}

int control_from127(const Control& c, float pot) {
    if (pot < 0.0f) pot = 0.0f;
    if (pot > 127.0f) pot = 127.0f;
    const int n = c.hi - c.lo + 1;
    if (n <= 16) {
        const int k = static_cast<int>(std::floor(pot * n / 128.0f));
        return c.lo + (k > n - 1 ? n - 1 : k);
    }
    return c.lo + static_cast<int>(std::lround(pot * (c.hi - c.lo) / 127.0f));
}

int control_to127(const Control& c, int v) {
    const int n = c.hi - c.lo + 1;
    const float pot = n <= 16 ? (v - c.lo + 0.5f) * 128.0f / n
                              : (v - c.lo) * 127.0f / (c.hi - c.lo);
    return clamp(static_cast<int>(std::lround(pot)), 0, 127);
}

int load_user(const char* path, std::string& err) {
    std::vector<PatchBytes> got;
    std::vector<uint8_t> reverb;
    const int n = read_syx(path, got, reverb, err);
    if (n < 0) return n;
    Store& s = store();
    if (!reverb.empty()) s.reverb = reverb;     // a file without the block keeps the one held
    for (int i = 0; i < n && i < kUserSlots; ++i) {
        s.user[i] = got[i];
        sanitize(s.user[i].data());
        s.user_names[i] = decode_name(s.user[i].data() + kPatch * 64, 18);
    }
    return n < kUserSlots ? n : kUserSlots;
}

bool write_user(const char* path, std::string& err) {
    std::vector<uint8_t> run;
    user_run(run, 0, user_bytes());
    Collect c;
    send_dt1(Collect::put, &c, 0, kIntBase, run.data(), static_cast<int>(run.size()));
    FILE* f = std::fopen(path, "wb");
    if (!f) { err = std::string("cannot write ") + path; return false; }
    const size_t put = std::fwrite(c.bytes.data(), 1, c.bytes.size(), f);
    std::fclose(f);
    if (put != c.bytes.size()) { err = "short write"; return false; }
    return true;
}

const char* user_name(int slot) {
    return (slot >= 0 && slot < kUserSlots) ? store().user_names[slot].c_str() : "";
}

// ---------------------------------------------------------------------------
// Synth
// ---------------------------------------------------------------------------
struct Synth::Impl {
    d5::Patch patch{};
    Resampler rs;
    float out_rate = kEngineRate;

    Source source = kFactory;
    int index = 0;
    uint8_t temp[kPatchBytes] = {};
    bool temp_valid = false;       // false: a built-in preset, no bytes behind it
    bool dirty = false;            // temp[] or a macro changed since the engine saw it
    std::string builtin_name;

    // Not patch bytes.
    int volume = 80;               // as the bridge: headroom for sixteen voices
    int tune = 0;
    int voice_limit = 8;
    int part = 0;                  // 0..3 = u1 u2 l1 l2
    int cut = 0, res = 0, att = 0, rel = 0, vib = 0;
    int builtin_chorus = 50;

    float base_volume = 1.0f;
    float fine_upper = 0.0f, fine_lower = 0.0f;   // the patch's own fine tune per tone
    float bend = 0.0f;
    int bend_range = 2;
    bool whole_mode = false;

    uint8_t held[128] = {};
    uint8_t sustained[128] = {};
    bool sustain = false;
    int active = 0;

    bool governor = false;
    int shed_holdoff = 0;
    int cpu_peak = 0;

    int note_limit() const { return whole_mode ? 2 * voice_limit : voice_limit; }

    int partial_section() const { return part < 2 ? part : part + 1; }
    int tone_section() const { return part < 2 ? kUpper : kLower; }

    void apply_levels() {
        patch.set_volume(base_volume * volume * 0.01f);
        patch.set_master_cents(fine_upper + tune, fine_lower + tune);
        patch.set_perf_cutoff(cut * 0.01f);
        patch.set_bend_semis(bend * bend_range);
    }

    // The engine's patch out of temp[] with the macros laid over it. `live`
    // keeps the notes and the effect tails (an edit); otherwise it is a
    // patch change. Voices copy their parameters at note-on, so an edit is
    // heard from the next note; effects and balances follow at once.
    void apply_temp(bool live) {
        dirty = false;
        sanitize(temp);
        uint8_t work[kPatchBytes];
        std::memcpy(work, temp, kPatchBytes);
        if (res || att || rel) {
            for (int s : {kU1, kU2, kL1, kL2}) {
                uint8_t* p = work + s * 64;
                p[14] = static_cast<uint8_t>(clamp(p[14] + res * 30 / 63, 0, 30));
                p[39] = static_cast<uint8_t>(clamp(p[39] + att * 100 / 63, 0, 100));
                p[43] = static_cast<uint8_t>(clamp(p[43] + rel * 100 / 63, 0, 100));
                p[26] = static_cast<uint8_t>(clamp(p[26] + rel * 100 / 63, 0, 100));
            }
        }
        if (vib) {   // tone byte 22: LFO pitch modulation (vibrato) of both tones
            for (int s : {kUpper, kLower}) {
                uint8_t* p = work + s * 64;
                p[22] = static_cast<uint8_t>(clamp(p[22] + vib * 100 / 63, 0, 100));
            }
        }
        d5::PatchSpec spec = d5::patch_from_bytes(work, d50::store().pcm.data());
        base_volume = spec.volume;
        fine_upper = spec.upper.voice.master_cents;
        fine_lower = spec.lower.voice.master_cents;
        whole_mode = spec.key_mode == d5::KeyMode::kWhole;
        bend_range = spec.bend_range;
        if (live) patch.reconfigure(spec);
        else patch.configure(spec, kEngineRate);
        apply_levels();
    }

    void flush() {
        if (dirty && temp_valid) apply_temp(true);
        dirty = false;
    }

    void release_all() {
        sustain = false;
        for (int n = 0; n < 128; ++n) {
            sustained[n] = 0;
            if (held[n]) { patch.note_off(n); held[n] = 0; }
        }
        active = 0;
    }

    // A patch change: the macros that bend the patch go back to neutral.
    void load(const uint8_t* bytes) {
        release_all();
        cut = res = att = rel = vib = 0;
        std::memcpy(temp, bytes, kPatchBytes);
        temp_valid = true;
        apply_temp(false);
    }

    void load_builtin(int i) {
        release_all();
        cut = res = att = rel = vib = 0;
        temp_valid = false;
        dirty = false;
        d5::Preset pr = d5::preset(i % d5::kPresetCount);
        d5::preset_bind(pr.spec, d50::store().pcm.data(), pr.pcm1, pr.pcm2);
        builtin_name = pr.name;
        builtin_chorus = 50;
        base_volume = pr.spec.volume;
        fine_upper = pr.spec.upper.voice.master_cents;
        fine_lower = pr.spec.lower.voice.master_cents;
        whole_mode = pr.spec.key_mode == d5::KeyMode::kWhole;
        bend_range = pr.spec.bend_range;
        patch.configure(pr.spec, kEngineRate);
        patch.set_reverb_balance(0.30f);
        patch.set_chorus_balance(0.50f);
        apply_levels();
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
    select_patch(0);
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
    m.flush();
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
    m.flush();                      // the note must start with the edits made so far
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
    if (impl_) impl_->release_all();
}

void Synth::silence() {
    if (!impl_) return;
    impl_->release_all();
    impl_->patch.silence();
    impl_->rs.clear();
}

void Synth::set_bend(float normalized) {
    if (!impl_) return;
    if (normalized < -1.0f) normalized = -1.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    impl_->bend = normalized;
    impl_->patch.set_bend_semis(normalized * impl_->bend_range);
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

void Synth::set_governor(bool on) {
    if (impl_) { impl_->governor = on; impl_->shed_holdoff = 0; }
}

// ---- which patch plays -------------------------------------------------------

bool Synth::select_patch(int index) {
    if (!impl_ || index < 0 || index >= patch_count()) return false;
    impl_->source = kFactory;
    impl_->index = index;
    if (have_bank()) impl_->load(d50::store().patches[index].data());
    else impl_->load_builtin(index);
    return true;
}

bool Synth::select_user(int slot) {
    if (!impl_ || slot < 0 || slot >= kUserSlots) return false;
    impl_->source = kUser;
    impl_->index = slot;
    impl_->load(d50::store().user[slot].data());
    return true;
}

void Synth::select_init() {
    if (!impl_) return;
    uint8_t b[kPatchBytes];
    init_patch(b);
    impl_->source = kInit;
    impl_->index = 0;
    impl_->load(b);
}

void Synth::reload() {
    if (!impl_) return;
    switch (impl_->source) {
        case kFactory: select_patch(impl_->index); break;
        case kUser: select_user(impl_->index); break;
        default: select_init(); break;
    }
}

Synth::Source Synth::source() const { return impl_ ? impl_->source : kFactory; }
int Synth::patch() const { return impl_ ? impl_->index : 0; }

std::string Synth::name() const {
    if (!impl_) return std::string();
    return impl_->temp_valid ? decode_name(impl_->temp + kPatch * 64, 18) : impl_->builtin_name;
}

// ---- controls ------------------------------------------------------------------

bool Synth::available(const Control& c) const {
    if (!impl_) return false;
    if (c.kind != 'M') return impl_->temp_valid;
    switch (c.index) {
        case kMacro_res: case kMacro_att: case kMacro_rel: case kMacro_vib: return impl_->temp_valid;
        default: return true;
    }
}

int Synth::get_control(const Control& c) const {
    if (!impl_) return c.lo;
    const Impl& m = *impl_;
    int section = kPatch;
    const Param* table = kPatchParams;
    switch (c.kind) {
        case 'P': section = m.partial_section(); table = kPartialParams; break;
        case 'T': section = m.tone_section(); table = kCommonParams; break;
        case 'G': break;
        default:
            switch (c.index) {
                case kMacro_cut: return m.cut;
                case kMacro_res: return m.res;
                case kMacro_att: return m.att;
                case kMacro_rel: return m.rel;
                case kMacro_vib: return m.vib;
                case kMacro_chorus:
                    return m.temp_valid ? m.temp[kUpper * 64 + 45] : m.builtin_chorus;
                case kMacro_volume: return m.volume;
                case kMacro_part: return m.part;
                case kMacro_tune: return m.tune;
                case kMacro_voices: return m.voice_limit;
                default: return c.lo;
            }
    }
    if (!m.temp_valid) return c.lo;
    const Param& p = table[c.index];
    return m.temp[section * 64 + p.offset] - p.shift;
}

bool Synth::set_control(const Control& c, int v) {
    if (!available(c)) return false;
    Impl& m = *impl_;
    v = clamp(v, c.lo, c.hi);
    int section = kPatch;
    const Param* table = kPatchParams;
    switch (c.kind) {
        case 'P': section = m.partial_section(); table = kPartialParams; break;
        case 'T': section = m.tone_section(); table = kCommonParams; break;
        case 'G': break;
        default:
            switch (c.index) {
                case kMacro_cut: m.cut = v; m.patch.set_perf_cutoff(v * 0.01f); break;
                case kMacro_res: m.res = v; m.dirty = true; break;
                case kMacro_att: m.att = v; m.dirty = true; break;
                case kMacro_rel: m.rel = v; m.dirty = true; break;
                case kMacro_vib: m.vib = v; m.dirty = true; break;
                case kMacro_chorus:
                    if (m.temp_valid) {
                        m.temp[kUpper * 64 + 45] = m.temp[kLower * 64 + 45] =
                            static_cast<uint8_t>(v);
                        m.dirty = true;
                    } else {
                        m.builtin_chorus = v;
                        m.patch.set_chorus_balance(v * 0.01f);
                    }
                    break;
                case kMacro_volume: m.volume = v; m.apply_levels(); break;
                case kMacro_part: m.part = v; break;
                case kMacro_tune: m.tune = v; m.apply_levels(); break;
                case kMacro_voices: m.voice_limit = v; break;
                default: return false;
            }
            return true;
    }
    const Param& p = table[c.index];
    m.temp[section * 64 + p.offset] = static_cast<uint8_t>(v + p.shift);
    m.dirty = true;
    return true;
}

std::string Synth::control_text(const Control& c, int v) const {
    char buf[32];
    const char* names = c.names;
    if (std::strncmp(names, "@note:", 6) == 0) {
        const int note = std::atoi(names + 6) + (v - c.lo);
        std::snprintf(buf, sizeof buf, "%s%d", kNoteNames[note % 12], note / 12 - 1);
        return buf;
    }
    if (std::strcmp(names, "@pcm") == 0) {
        const int i = clamp(v - 1, 0, d5::kPcmCount - 1);
        std::snprintf(buf, sizeof buf, "%.6s", d5::kPcmSamples[i].name);
        return buf;
    }
    if (names[0]) {
        const char* a = names;
        for (int k = v - c.lo; k > 0; --k) {
            const char* q = std::strchr(a, '|');
            if (!q) break;
            a = q + 1;
        }
        const char* q = std::strchr(a, '|');
        return q ? std::string(a, q - a) : std::string(a);
    }
    if (c.lo < 0 && v != 0) std::snprintf(buf, sizeof buf, "%+d", v);
    else std::snprintf(buf, sizeof buf, "%d", v);
    return buf;
}

// ---- patch bytes -----------------------------------------------------------------

bool Synth::editable() const { return impl_ && impl_->temp_valid; }

int Synth::peek(int address) const {
    if (!editable() || address < 0 || address >= kPatchBytes) return -1;
    return impl_->temp[address];
}

bool Synth::poke(int address, const uint8_t* data, int len) {
    if (!editable() || address < 0 || address >= kPatchBytes || len <= 0) return false;
    if (address + len > kPatchBytes) len = kPatchBytes - address;
    const Limits& lim = limits();
    for (int k = 0; k < len; ++k) {
        const uint8_t v = data[k] & 0x7F;
        const int a = address + k;
        impl_->temp[a] = v > lim.max[a] ? lim.max[a] : v;
    }
    impl_->dirty = true;
    return true;
}

bool Synth::set_param(int section, const Param& p, int panel_value) {
    const uint8_t v = static_cast<uint8_t>(clamp(panel_value + p.shift, 0, p.max));
    return poke(section * 64 + p.offset, &v, 1);
}

int Synth::get_param(int section, const Param& p) const {
    const int v = peek(section * 64 + p.offset);
    return v < 0 ? 0 : v - p.shift;
}

bool Synth::set_name(int section, const char* text) {
    if (section != kUpper && section != kLower && section != kPatch) return false;
    const int len = section == kPatch ? 18 : 10;
    uint8_t bytes[18];
    encode_name(bytes, len, text);
    return poke(section * 64, bytes, len);
}

std::string Synth::get_name(int section) const {
    if (!impl_) return std::string();
    if (!impl_->temp_valid) return section == kPatch ? impl_->builtin_name : std::string();
    if (section != kUpper && section != kLower && section != kPatch) return std::string();
    return decode_name(impl_->temp + section * 64, section == kPatch ? 18 : 10);
}

bool Synth::store(int slot) {
    if (!editable()) return false;
    Store& s = d50::store();
    if (slot < 0) slot = impl_->source == kUser ? impl_->index : 0;
    if (slot >= kUserSlots) return false;
    sanitize(impl_->temp);
    std::memcpy(s.user[slot].data(), impl_->temp, kPatchBytes);
    s.user_names[slot] = decode_name(impl_->temp + kPatch * 64, 18);
    // What plays is now that slot.
    impl_->source = kUser;
    impl_->index = slot;
    return true;
}

// Roland's one-way exclusive, as the D-50 speaks it:
//   F0 41 <device> 14 <command> <address, 3 bytes> <data ...> <checksum> F7
// DT1 (12) carries data, RQ1 (11) asks for it by address and size. Two
// regions exist, as in the firmware: 00-00-00, the 448 bytes being played,
// and 02-00-00, the sixty-four patches of the internal memory -- here the
// user bank.
bool Synth::sysex(const uint8_t* d, int n, SysexOut out, void* ctx) {
    if (!impl_) return false;
    if (n < 10 || d[0] != 0xF0 || d[n - 1] != 0xF7) return false;
    if (d[1] != 0x41 || d[3] != 0x14) return false;
    const int dev = d[2];
    const int addr = ((d[5] & 0x7F) << 14) | ((d[6] & 0x7F) << 7) | (d[7] & 0x7F);
    if (roland_checksum(d + 5, n - 7) != d[n - 2]) return false;
    Store& s = d50::store();

    if (d[4] == 0x12) {
        const uint8_t* data = d + 8;
        int len = n - 10;
        if (addr >= kTempBase && addr < kTempBase + kPatchBytes) {
            // A patch sent to a built-in preset turns it into bytes.
            if (!impl_->temp_valid) {
                init_patch(impl_->temp);
                impl_->temp_valid = true;
                impl_->source = kInit;
                impl_->index = 0;
            }
            poke(addr - kTempBase, data, len);
        } else if (addr >= kIntBase) {
            const Limits& lim = limits();
            int off = addr - kIntBase;
            bool current = false;
            while (len > 0) {
                if (off >= kBankBytes) {
                    // Behind the patches: the reverb data, kept as it comes.
                    const int at = off - kBankBytes;
                    if (at >= kReverbBytes) break;
                    if (s.reverb.empty()) s.reverb.assign(kReverbBytes, 0);
                    int m = kReverbBytes - at;
                    if (m > len) m = len;
                    for (int k = 0; k < m; ++k) s.reverb[at + k] = data[k] & 0x7F;
                    break;
                }
                const int slot = off / kPatchBytes, in = off % kPatchBytes;
                int m = kPatchBytes - in;
                if (m > len) m = len;
                uint8_t* dst = s.user[slot].data();
                for (int k = 0; k < m; ++k) {
                    const uint8_t v = data[k] & 0x7F;
                    dst[in + k] = v > lim.max[in + k] ? lim.max[in + k] : v;
                }
                s.user_names[slot] = decode_name(dst + kPatch * 64, 18);
                if (impl_->source == kUser && slot == impl_->index) current = true;
                off += m; data += m; len -= m;
            }
            // The one being played follows, without a patch change.
            if (current) {
                std::memcpy(impl_->temp, s.user[impl_->index].data(), kPatchBytes);
                impl_->dirty = true;
            }
        }
        return true;
    }

    if (d[4] == 0x11 && n >= 13 && out) {
        int size = ((d[8] & 0x7F) << 14) | ((d[9] & 0x7F) << 7) | (d[10] & 0x7F);
        if (size <= 0 || size > 0x10000) return true;
        if (addr >= kTempBase && addr < kTempBase + kPatchBytes) {
            if (!impl_->temp_valid) return true;
            int m = kPatchBytes - (addr - kTempBase);
            if (m > size) m = size;
            send_dt1(out, ctx, dev, addr, impl_->temp + (addr - kTempBase), m);
        } else if (addr >= kIntBase) {
            std::vector<uint8_t> run;
            user_run(run, addr - kIntBase, size);
            if (!run.empty())
                send_dt1(out, ctx, dev, addr, run.data(), static_cast<int>(run.size()));
        }
        return true;
    }
    return true;
}

void Synth::dump_sysex(SysexOut out, void* ctx, int device) const {
    if (!editable() || !out) return;
    send_dt1(out, ctx, device, kTempBase, impl_->temp, kPatchBytes);
}

void Synth::dump_user(SysexOut out, void* ctx, int device) const {
    if (!impl_ || !out) return;
    std::vector<uint8_t> run;
    user_run(run, 0, user_bytes());
    send_dt1(out, ctx, device, kIntBase, run.data(), static_cast<int>(run.size()));
}

Synth::State Synth::state() const {
    State s{};
    if (!impl_) return s;
    const Impl& m = *impl_;
    s.notes_held = m.active;
    s.voices_sounding = m.patch.voices_sounding();
    s.cpu_peak = m.cpu_peak;
    s.sustain = m.sustain;
    s.governor = m.governor;
    return s;
}

}  // namespace d50
