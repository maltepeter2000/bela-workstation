// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Michi71 (PicoFaceJ6 engine this drives)
// SPDX-FileCopyrightText: 2026 Peter (Pure Data adaptation)
//
// juno_core.cpp -- see juno_core.h.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#endif
#include "juno_core.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "juno/juno.h"
#include "juno106_tables.h"

namespace juno {

#include "juno_params.h"

namespace {

typedef std::array<uint8_t, kPatchBytes> Bytes;

constexpr int kControlCount = static_cast<int>(sizeof(kControls) / sizeof(kControls[0]));

// A plain sawtooth to start from: 8', filter open, organ envelope, no chorus.
const Bytes kInitPatch = {{60, 0, 0, 0, 0, 100, 0, 0, 0, 64, 64, 0, 64, 127, 20, 0, 0x72, 0x18}};

// ---- the switch bytes of a Juno-106 patch --------------------------------------
//   16: bit 0 16', 1 8', 2 4', 3 pulse, 4 sawtooth, 5 chorus off, 6 chorus I (0 = II)
//   17: bit 0 PWM manual, 1 envelope negative, 2 VCA gate, 3-4 HPF (3 = position 0)
//       bit 5: chorus I+II, the Juno-60's third mode -- not a Juno-106 bit
int get_switch(const Bytes& b, int sw) {
    const int a = b[16], c = b[17];
    switch (sw) {
        case kSw_range: return (a & 4) ? 2 : ((a & 1) ? 0 : 1);
        case kSw_pulse: return (a >> 3) & 1;
        case kSw_saw: return (a >> 4) & 1;
        case kSw_chorus: return (a & 0x20) ? 0 : ((a & 0x40) ? ((c & 0x20) ? 3 : 1) : 2);
        case kSw_pwmmode: return c & 1;
        case kSw_envpol: return (c >> 1) & 1;
        case kSw_vcamode: return (c >> 2) & 1;
        case kSw_hpf: return 3 - ((c >> 3) & 3);
        default: return 0;
    }
}

void set_switch(Bytes& b, int sw, int v) {
    int a = b[16], c = b[17];
    auto bit = [](int& x, int mask, bool on) { x = on ? (x | mask) : (x & ~mask); };
    switch (sw) {
        case kSw_range: a = (a & ~7) | (1 << v); break;
        case kSw_pulse: bit(a, 0x08, v != 0); break;
        case kSw_saw: bit(a, 0x10, v != 0); break;
        case kSw_chorus:
            bit(a, 0x20, v == 0);
            if (v != 0) bit(a, 0x40, v != 2);
            bit(c, 0x20, v == 3);
            break;
        case kSw_pwmmode: bit(c, 0x01, v != 0); break;
        case kSw_envpol: bit(c, 0x02, v != 0); break;
        case kSw_vcamode: bit(c, 0x04, v != 0); break;
        case kSw_hpf: c = (c & ~0x18) | ((3 - v) << 3); break;
        default: break;
    }
    b[16] = static_cast<uint8_t>(a & 0x7F);
    b[17] = static_cast<uint8_t>(c & 0x7F);
}

// ---- shared patch memory -----------------------------------------------------
struct Preset {
    Bytes bytes;
    std::string name;
};

int slider7(float v) {
    const int i = static_cast<int>(std::lround(v * 127.0f));
    return i < 0 ? 0 : (i > 127 ? 127 : i);
}

// A sound of the engine's own table in the 18 bytes of a Juno-106 patch.
Bytes from_program(const JunoProgram& g) {
    const float* p = g.param;
    Bytes b = kInitPatch;
    b[0] = static_cast<uint8_t>(slider7(p[JUNO_LFO_RATE]));
    b[1] = static_cast<uint8_t>(slider7(p[JUNO_LFO_DELAY]));
    b[2] = static_cast<uint8_t>(slider7(p[JUNO_DCO_LFO]));
    b[3] = static_cast<uint8_t>(slider7(p[JUNO_DCO_PWM]));
    b[4] = static_cast<uint8_t>(slider7(p[JUNO_DCO_NOISE]));
    b[5] = static_cast<uint8_t>(slider7(p[JUNO_VCF_FREQ]));
    b[6] = static_cast<uint8_t>(slider7(p[JUNO_VCF_RES]));
    b[7] = static_cast<uint8_t>(slider7(p[JUNO_VCF_ENV]));
    b[8] = static_cast<uint8_t>(slider7(p[JUNO_VCF_LFO]));
    b[9] = static_cast<uint8_t>(slider7(p[JUNO_VCF_KYBD]));
    b[10] = static_cast<uint8_t>(slider7(p[JUNO_VCA_LEVEL]));
    b[11] = static_cast<uint8_t>(slider7(p[JUNO_ENV_ATTACK]));
    b[12] = static_cast<uint8_t>(slider7(p[JUNO_ENV_DECAY]));
    b[13] = static_cast<uint8_t>(slider7(p[JUNO_ENV_SUSTAIN]));
    b[14] = static_cast<uint8_t>(slider7(p[JUNO_ENV_RELEASE]));
    // The Juno-106 has no sub switch: off is level 0.
    b[15] = static_cast<uint8_t>(junoParamOn(p[JUNO_DCO_SUB]) ? slider7(p[JUNO_DCO_SUB_LEVEL]) : 0);
    b[16] = b[17] = 0;
    // The engine's table holds the octave of a sound twice, as DCO range and
    // as a transposition; a Juno-106 patch has the range only.
    int range = junoParamStep(p[JUNO_DCO_RANGE], JUNO_RANGE_COUNT) + junoParamStep(p[JUNO_TRANSPOSE], 5) - 2;
    range = range < 0 ? 0 : (range > 2 ? 2 : range);
    set_switch(b, kSw_range, range);
    set_switch(b, kSw_pulse, junoParamOn(p[JUNO_DCO_PULSE]));
    set_switch(b, kSw_saw, junoParamOn(p[JUNO_DCO_SAW]));
    set_switch(b, kSw_chorus, junoParamStep(p[JUNO_CHORUS], 4));
    // Pulse width from the envelope becomes the fixed width it had while a key
    // is held.
    const int pwm_mode = junoParamStep(p[JUNO_DCO_PWM_MODE], 3);
    if (pwm_mode == 2) b[3] = static_cast<uint8_t>(slider7(p[JUNO_DCO_PWM] * p[JUNO_ENV_SUSTAIN]));
    set_switch(b, kSw_pwmmode, pwm_mode != 0);
    set_switch(b, kSw_envpol, junoParamStep(p[JUNO_VCF_POLARITY], 2) == 0);
    set_switch(b, kSw_vcamode, junoParamStep(p[JUNO_VCA_MODE], 2));
    set_switch(b, kSw_hpf, junoParamStep(p[JUNO_HPF], JUNO_HPF_POSITIONS));
    return b;
}

struct Store {
    std::vector<Preset> factory;
    Bytes user[kUserSlots];
    std::string user_names[kUserSlots];
    Store() {
        for (int i = 0; i < JUNO_NPROGRAMS; ++i)
            factory.push_back({from_program(junoPrograms[i]), junoPrograms[i].name});
        for (int i = 0; i < kUserSlots; ++i) {
            user[i] = kInitPatch;
            user_names[i] = std::string(1, static_cast<char>('1' + i / 8)) + static_cast<char>('1' + i % 8);
        }
    }
};

Store& mem() {
    static Store s;
    return s;
}

// ---- Juno-106 messages ---------------------------------------------------------
//   F0 41 30 0n pp <18 bytes> F7    a patch, with its number
//   F0 41 31 0n pp <18 bytes> F7    the panel (manual)
//   F0 41 32 0n pp vv F7            one of the 18 bytes
// Some software leaves the number out of the first two; both lengths are read.
struct Message {
    int type = 0, channel = 0, number = -1;
    Bytes bytes{};
    int value = 0;
};

bool parse(const uint8_t* m, int len, Message& out) {
    if (len < 7 || m[0] != 0xF0 || m[1] != 0x41 || m[len - 1] != 0xF7) return false;
    out.type = m[2];
    out.channel = m[3] & 0x0F;
    const int body = len - 5;
    if (out.type == 0x32) {
        if (body != 2 || m[4] >= kPatchBytes) return false;
        out.number = m[4];
        out.value = m[5] & 0x7F;
        return true;
    }
    if (out.type != 0x30 && out.type != 0x31) return false;
    if (body != kPatchBytes && body != kPatchBytes + 1) return false;
    const uint8_t* d = m + 4;
    out.number = -1;
    if (body == kPatchBytes + 1) out.number = *d++;
    for (int i = 0; i < kPatchBytes; ++i) out.bytes[i] = d[i] & 0x7F;
    return true;
}

bool read_file(const char* path, std::vector<Message>& out, std::string& err) {
    FILE* f = fopen(path, "rb");
    if (!f) { err = "cannot open the file"; return false; }
    std::vector<uint8_t> data;
    uint8_t buf[4096];
    size_t got;
    while ((got = fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + got);
    fclose(f);
    size_t i = 0;
    while (i < data.size()) {
        if (data[i] != 0xF0) { ++i; continue; }
        size_t j = i + 1;
        while (j < data.size() && data[j] < 0x80) ++j;
        if (j < data.size() && data[j] == 0xF7) {
            Message m;
            if (parse(&data[i], static_cast<int>(j - i + 1), m) && m.type != 0x32) out.push_back(m);
            i = j + 1;
        } else {
            i = j;
        }
    }
    if (out.empty()) { err = "no Juno-106 patches in the file"; return false; }
    return true;
}

void make_patch(uint8_t* msg, int channel, int number, const Bytes& b) {
    msg[0] = 0xF0; msg[1] = 0x41; msg[2] = 0x30;
    msg[3] = static_cast<uint8_t>(channel & 0x0F);
    msg[4] = static_cast<uint8_t>(number & 0x7F);
    std::memcpy(msg + 5, b.data(), kPatchBytes);
    msg[5 + kPatchBytes] = 0xF7;
}
constexpr int kPatchMessage = kPatchBytes + 6;

float zone(int k, int steps) { return (static_cast<float>(k) + 0.5f) / static_cast<float>(steps); }

}  // namespace

int load_syx(const char* path, std::string& err) {
    std::vector<Message> msgs;
    if (!read_file(path, msgs, err)) return -1;
    std::string stem = path;
    const size_t slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    int n = 0;
    for (const Message& m : msgs) {
        // Group, bank and number as a Juno-106 shows them: A11 .. B88.
        const int k = m.number >= 0 ? m.number : n;
        char tag[8];
        snprintf(tag, sizeof tag, "%c%d%d", (k & 64) ? 'B' : 'A', ((k >> 3) & 7) + 1, (k & 7) + 1);
        mem().factory.push_back({m.bytes, stem + " " + tag});
        ++n;
    }
    return n;
}

int patch_count() { return static_cast<int>(mem().factory.size()); }

const char* patch_name(int index) {
    if (index < 0 || index >= patch_count()) return "";
    return mem().factory[static_cast<size_t>(index)].name.c_str();
}

int load_user(const char* path, std::string& err) {
    std::vector<Message> msgs;
    if (!read_file(path, msgs, err)) return -1;
    int n = 0, next = 0;
    for (const Message& m : msgs) {
        const int slot = m.number >= 0 ? m.number : next;
        next = slot + 1;
        if (slot >= kUserSlots) continue;
        mem().user[slot] = m.bytes;
        ++n;
    }
    return n;
}

bool write_user(const char* path, std::string& err) {
    FILE* f = fopen(path, "wb");
    if (!f) { err = "cannot write the file"; return false; }
    for (int i = 0; i < kUserSlots; ++i) {
        uint8_t msg[kPatchMessage];
        make_patch(msg, 0, i, mem().user[i]);
        fwrite(msg, 1, sizeof msg, f);
    }
    fclose(f);
    return true;
}

const char* user_name(int slot) {
    if (slot < 0 || slot >= kUserSlots) return "";
    return mem().user_names[slot].c_str();
}

int control_count() { return kControlCount; }
const Control& control(int i) { return kControls[i]; }

const Control* find_control(const char* name) {
    for (int i = 0; i < kControlCount; ++i)
        if (std::strcmp(kControls[i].name, name) == 0) return &kControls[i];
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

// ---- one instrument ----------------------------------------------------------
struct Synth::Impl {
    Juno engine;
    Bytes temp = kInitPatch;
    int setting[kSettingCount];
    Source source = kFactory;
    bool model106 = false;
    int lastFactory = 0, lastUser = 0;   // where the step and save messages start from
    int index = 0;
    bool sustain = false;

    Impl() {
        for (int i = 0; i < kSettingCount; ++i) setting[i] = kSettingDefaults[i];
    }

    void set(int id, float v) {
        if (engine.getParameter(id) != v) engine.setParameter(id, v);
    }

    // The patch bytes and the settings into the engine's own parameters.
    void apply(bool all = false) {
        if (all)
            for (int i = 0; i < JUNO_TOTAL_COUNT; ++i) engine.setParameter(i, engine.getParameter(i));
        // The Juno-106 model remaps most sliders: k106*[value] is the Juno-60 slider
        // at which the engine does what the Juno-106 does (juno106_tables.h).
        const bool m106 = setting[kSet_model] == 1;
        if (m106 != model106) { engine.setModel106(m106); model106 = m106; }
        auto slider = [&](int id, int byte, const float* t106 = nullptr) {
            set(id, (m106 && t106) ? t106[temp[byte]] : static_cast<float>(temp[byte]) / 127.0f);
        };
        slider(JUNO_LFO_RATE, 0);
        engine.setLfoRateHz(m106 ? k106LfoHz[temp[0]] : 0.0f);
        slider(JUNO_LFO_DELAY, 1, k106LfoDelay);
        slider(JUNO_DCO_LFO, 2, k106DcoLfo);
        slider(JUNO_DCO_PWM, 3, k106Pwm);
        slider(JUNO_DCO_NOISE, 4);
        slider(JUNO_VCF_FREQ, 5, k106Cutoff);
        slider(JUNO_VCF_RES, 6, k106Res);
        slider(JUNO_VCF_ENV, 7, k106Env);
        slider(JUNO_VCF_LFO, 8, k106VcfLfo);
        slider(JUNO_VCF_KYBD, 9);
        slider(JUNO_VCA_LEVEL, 10);
        engine.setVcaGain(m106 ? k106VcaGain[temp[10]] : -1.0f);
        slider(JUNO_ENV_ATTACK, 11, k106Attack);
        slider(JUNO_ENV_DECAY, 12, k106Decay);
        slider(JUNO_ENV_SUSTAIN, 13);
        slider(JUNO_ENV_RELEASE, 14, k106Release);
        slider(JUNO_DCO_SUB_LEVEL, 15);
        set(JUNO_DCO_SUB, temp[15] > 0 ? 1.0f : 0.0f);
        set(JUNO_DCO_RANGE, zone(get_switch(temp, kSw_range), JUNO_RANGE_COUNT));
        set(JUNO_DCO_PULSE, static_cast<float>(get_switch(temp, kSw_pulse)));
        set(JUNO_DCO_SAW, static_cast<float>(get_switch(temp, kSw_saw)));
        set(JUNO_CHORUS, zone(get_switch(temp, kSw_chorus), 4));
        set(JUNO_DCO_PWM_MODE, zone(get_switch(temp, kSw_pwmmode), 3));
        set(JUNO_VCF_POLARITY, zone(get_switch(temp, kSw_envpol) ? 0 : 1, 2));
        set(JUNO_VCA_MODE, zone(get_switch(temp, kSw_vcamode), 2));
        set(JUNO_HPF, zone(get_switch(temp, kSw_hpf), JUNO_HPF_POSITIONS));

        set(JUNO_MASTER, static_cast<float>(setting[kSet_volume]) / 127.0f);
        set(JUNO_TUNE, static_cast<float>(setting[kSet_tune] + 50) / 100.0f);
        set(JUNO_BEND_RANGE, zone(setting[kSet_bend], 8));
        set(JUNO_TRANSPOSE, zone(setting[kSet_octave] + 2, 5));
        set(JUNO_LFO_TRIG, zone(0, 2));
        set(JUNO_ARP_ON, 0.0f);
        set(JUNO_HOLD, 0.0f);
        engine.setCalibration(static_cast<float>(setting[kSet_calibrate]) / 127.0f);
        engine.setChorusNoise(static_cast<float>(setting[kSet_chorusnoise]) / 127.0f);
        engine.setVelocitySens(static_cast<float>(setting[kSet_vellevel]) / 127.0f,
                               static_cast<float>(setting[kSet_velcutoff]) / 127.0f);
    }

    const Bytes& stored() const {
        if (source == kFactory) return mem().factory[static_cast<size_t>(index)].bytes;
        if (source == kUser) return mem().user[index];
        return kInitPatch;
    }
};

Synth::Synth() : impl_(new Impl()) { impl_->temp = impl_->stored(); impl_->apply(true); }
Synth::~Synth() { delete impl_; }

void Synth::set_sample_rate(float sr) {
    impl_->engine.setSampleRate(sr);
    impl_->engine.setModel106(false);   // apply() sets the model again
    impl_->model106 = false;
    impl_->apply(true);
}

void Synth::render(float* left, float* right, int frames) {
    impl_->engine.processFloat(left, right, frames);
}

void Synth::note_on(int note, float velocity) {
    int v = static_cast<int>(std::lround(velocity * 127.0f));
    impl_->engine.noteOn(note, v < 1 ? 1 : (v > 127 ? 127 : v));
}
void Synth::note_off(int note) { impl_->engine.noteOff(note); }
void Synth::all_notes_off() { impl_->engine.stopVoices(); impl_->sustain = false; }
void Synth::silence() { impl_->engine.resetVoices(); impl_->sustain = false; }

void Synth::set_bend(float n) {
    n = n < -1 ? -1 : (n > 1 ? 1 : n);
    impl_->engine.setPitchBend(8192 + static_cast<int>(n * (n < 0 ? 8192.0f : 8191.0f)));
}
void Synth::set_mod(float w) { impl_->engine.setModWheel(w < 0 ? 0 : (w > 1 ? 1 : w)); }
void Synth::set_sustain(bool on) {
    impl_->sustain = on;
    impl_->engine.processMidiController(64, on ? 127 : 0);
}

bool Synth::select_patch(int index) {
    if (index < 0 || index >= patch_count()) return false;
    impl_->source = kFactory;
    impl_->index = index;
    impl_->lastFactory = index;
    reload();
    return true;
}
bool Synth::select_user(int slot) {
    if (slot < 0 || slot >= kUserSlots) return false;
    impl_->source = kUser;
    impl_->index = slot;
    impl_->lastUser = slot;
    reload();
    return true;
}

bool Synth::step(Source src, int by) {
    if (src == kFactory) {
        const int n = patch_count();
        return select_patch(((impl_->lastFactory + by) % n + n) % n);
    }
    return select_user(((impl_->lastUser + by) % kUserSlots + kUserSlots) % kUserSlots);
}
void Synth::select_init() {
    impl_->source = kInit;
    impl_->index = 0;
    reload();
}
void Synth::reload() {
    impl_->temp = impl_->stored();
    impl_->apply();
}
Synth::Source Synth::source() const { return impl_->source; }
int Synth::patch() const { return impl_->index; }
std::string Synth::name() const {
    if (impl_->source == kFactory) return patch_name(impl_->index);
    if (impl_->source == kUser) return std::string("User ") + user_name(impl_->index);
    return "Init";
}

int Synth::get_control(const Control& c) const {
    if (c.kind == 'P') return impl_->temp[c.index];
    if (c.kind == 'W') return get_switch(impl_->temp, c.index);
    return impl_->setting[c.index];
}

void Synth::set_control(const Control& c, int v) {
    v = v < c.lo ? c.lo : (v > c.hi ? c.hi : v);
    if (c.kind == 'P') impl_->temp[c.index] = static_cast<uint8_t>(v);
    else if (c.kind == 'W') set_switch(impl_->temp, c.index, v);
    else impl_->setting[c.index] = v;
    impl_->apply();
}

std::string Synth::control_text(const Control& c, int v) const {
    if (c.names[0]) {
        int k = v - c.lo;
        const char* p = c.names;
        while (k > 0 && *p) { if (*p++ == '|') --k; }
        const char* e = p;
        while (*e && *e != '|') ++e;
        if (k == 0 && e > p) return std::string(p, e);
    }
    return std::to_string(v);
}

int Synth::peek(int address) const {
    return (address < 0 || address >= kPatchBytes) ? -1 : impl_->temp[address];
}
bool Synth::poke(int address, int value) {
    if (address < 0 || address >= kPatchBytes) return false;
    impl_->temp[address] = static_cast<uint8_t>(value < 0 ? 0 : (value > 127 ? 127 : value));
    impl_->apply();
    return true;
}

bool Synth::store(int slot) {
    if (slot < 0) slot = impl_->lastUser;
    if (slot >= kUserSlots) return false;
    mem().user[slot] = impl_->temp;
    impl_->source = kUser;
    impl_->index = slot;
    impl_->lastUser = slot;
    return true;
}

bool Synth::sysex(const uint8_t* msg, int len) {
    Message m;
    if (!parse(msg, len, m)) return false;
    if (m.type == 0x32) impl_->temp[m.number] = static_cast<uint8_t>(m.value);
    else impl_->temp = m.bytes;
    impl_->apply();
    return true;
}

void Synth::dump_sysex(SysexOut out, void* ctx, int channel) const {
    uint8_t msg[kPatchMessage];
    make_patch(msg, channel, impl_->source == kInit ? 0 : impl_->index, impl_->temp);
    out(ctx, msg, sizeof msg);
}

void Synth::dump_param(SysexOut out, void* ctx, int channel, int address) const {
    if (address < 0 || address >= kPatchBytes) return;
    const uint8_t msg[7] = {0xF0, 0x41, 0x32, static_cast<uint8_t>(channel & 0x0F),
                            static_cast<uint8_t>(address), impl_->temp[address], 0xF7};
    out(ctx, msg, sizeof msg);
}

void Synth::dump_user(SysexOut out, void* ctx, int channel) const {
    for (int i = 0; i < kUserSlots; ++i) {
        uint8_t msg[kPatchMessage];
        make_patch(msg, channel, i, mem().user[i]);
        out(ctx, msg, sizeof msg);
    }
}

int Synth::voices_sounding() const { return impl_->engine.activeVoices(); }
bool Synth::sustain() const { return impl_->sustain; }

}  // namespace juno
