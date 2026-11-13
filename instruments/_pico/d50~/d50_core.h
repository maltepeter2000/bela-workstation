// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Michi71 (PicoFaceD5 bridge this derives from)
// SPDX-FileCopyrightText: 2026 Peter (Pure Data adaptation)
//
// d50_core.h -- the PicoFaceD5 LA engine behind a host-neutral interface.
//
// This replaces the firmware's D5_Bridge: the same patch handling, levels,
// controllers and note bookkeeping, without anything of the Pico. Three
// things differ from the bridge:
//
//   * The PCM data and the patch banks are loaded from files at run time, so
//     the binary carries no ROM content.
//   * Nothing is global per instance: several synths can run side by side.
//   * The output is float, at any sample rate (the engine runs at its native
//     32 kHz and is resampled).
//
// No Pure Data in here; d50~.cpp is the only file that knows m_pd.h.
#ifndef D50_CORE_H
#define D50_CORE_H

#include <cstdint>
#include <string>

namespace d50 {

constexpr int kPatchBytes = 448;
constexpr float kEngineRate = 32000.0f;

// ---- shared, read-only data ------------------------------------------------
// One copy for every instance. Not thread-safe to load; load before use.

// Loads the converted PCM (262144 16-bit words, from tools/make_d50_data.py).
bool load_pcm(const char* path, std::string& err);
bool pcm_loaded();

// Appends the patches of a D-50 SysEx bulk dump. Returns how many it added,
// or -1 with a reason in `err`.
int load_syx(const char* path, std::string& err);

// Patches available: the loaded banks, or the eight built-in presets when no
// bank has been loaded.
int patch_count();
bool have_bank();
const char* patch_name(int index);

// ---- patch memory ----------------------------------------------------------
// A patch is 448 bytes: seven blocks of 64 in the order of the D-50's own
// dump. This is also the machine's temporary area at SysEx address 00-00-00.
enum Section { kU1 = 0, kU2, kUpper, kL1, kL2, kLower, kPatch, kSectionCount };

struct Param {
    const char* name;
    uint8_t offset;   // inside its 64-byte block
    uint8_t max;      // largest stored value
    int8_t shift;     // panel value = stored value - shift
};

const char* section_name(int section);
int section_from_name(const char* name);            // -1 if unknown
const Param* section_params(int section, int& count);
const Param* find_param(int section, const char* name);

// Receives one complete SysEx message, F0 to F7.
typedef void (*SysexOut)(void* ctx, const uint8_t* msg, int len);

// ---- one instrument --------------------------------------------------------
class Synth {
public:
    Synth();
    ~Synth();
    Synth(const Synth&) = delete;
    Synth& operator=(const Synth&) = delete;

    bool ok() const { return impl_ != nullptr; }

    void set_sample_rate(float sr);
    // Overwrites `frames` samples in each buffer.
    void render(float* left, float* right, int frames);

    void note_on(int note, float velocity);   // velocity 0..1
    void note_off(int note);
    void all_notes_off();
    void silence();                           // notes and effect tails

    bool select_patch(int index);
    int patch() const;
    const char* name() const;
    const char* structure_name() const;

    void set_volume(int percent);             // 0..100
    void set_tune(int cents);                 // -50..+50
    void set_bend(float normalized);          // -1..+1, times the bend range
    void set_bend_range(int semis);           // 0..12
    void set_mod(float w);                    // 0..1
    void set_aftertouch(float a);             // 0..1
    void set_sustain(bool on);
    void set_porta(bool on);
    void set_porta_time(int percent);         // 0..100
    void set_voice_limit(int voices);         // per tone, 1..8

    // The D-50's own effect and mix parameters. All follow the patch when it
    // changes.
    void set_reverb(int balance);             // 0..100
    void set_reverb_type(int type);           // 1..32
    void set_chorus(int balance);             // 0..100
    void set_chorus_type(int type);           // 1..8
    void set_chorus_rate(int v);              // 0..100
    void set_chorus_depth(int v);             // 0..100
    void set_eq_low_freq(int v);              // 0..15
    void set_eq_low_gain(int db);             // -12..+12
    void set_eq_high_freq(int v);             // 0..21
    void set_eq_high_q(int v);                // 0..8
    void set_eq_high_gain(int db);            // -12..+12
    void set_tone_balance(int v);             // 0..100

    // ---- patch editing (needs a loaded bank) ------------------------------
    bool editable() const;
    int peek(int address) const;                     // 0..447, -1 if not editable
    // Writes into the playing patch; takes effect at once, without clearing
    // the effects. Values are limited to the range of their parameter.
    bool poke(int address, const uint8_t* data, int len);
    bool set_param(int section, const Param& p, int panel_value);
    int get_param(int section, const Param& p) const;
    // Names: 18 characters for the patch, 10 for a tone (kUpper, kLower).
    bool set_name(int section, const char* text);
    std::string get_name(int section) const;
    // Copies the playing patch into a slot of the loaded banks (in memory
    // only). index < 0 means the slot it was selected from.
    bool store(int index);

    // One D-50 SysEx message in (DT1 writes, RQ1 is answered through `out`).
    // Returns false if it was not a D-50 message or its checksum was wrong.
    bool sysex(const uint8_t* msg, int len, SysexOut out, void* ctx);
    // The playing patch as DT1 messages, the way the D-50 sends it.
    void dump_sysex(SysexOut out, void* ctx, int device = 0) const;

    // Retire released tails when a block costs more than its time. Off by
    // default; meant for small machines.
    void set_governor(bool on);

    struct State {
        int patch, volume, tune, bend_range, voice_limit;
        int reverb, reverb_type, chorus, chorus_type, chorus_rate, chorus_depth;
        int eq_low_freq, eq_low_gain, eq_high_freq, eq_high_q, eq_high_gain;
        int tone_balance;
        int notes_held, voices_sounding, cpu_peak;
        bool porta, sustain, governor;
        int porta_time;
    };
    State state() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace d50

#endif  // D50_CORE_H
