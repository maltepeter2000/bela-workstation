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
constexpr int kUserSlots = 64;
constexpr float kEngineRate = 32000.0f;

// ---- shared, read-only data ------------------------------------------------
// One copy for every instance. Not thread-safe to load; load before use.

// Loads the converted PCM (262144 16-bit words, from tools/make_d50_data.py).
bool load_pcm(const char* path, std::string& err);
bool pcm_loaded();

// Appends the patches of a D-50 SysEx bulk dump to the factory presets.
// Returns how many it added, or -1 with a reason in `err`.
int load_syx(const char* path, std::string& err);

// Factory presets: the loaded banks, or the eight built-in presets when no
// bank has been loaded. Read-only.
int patch_count();
bool have_bank();
const char* patch_name(int index);

// The user bank: 64 patches that can be stored to, shared by every instance.
// It starts as 64 init patches; a D-50 bulk dump replaces it. The file is
// the machine's own format, patches only (112 messages).
int load_user(const char* path, std::string& err);       // patches read, or -1
bool write_user(const char* path, std::string& err);
const char* user_name(int slot);

// ---- patch memory ----------------------------------------------------------
// A patch is 448 bytes: seven blocks of 64 in the order of the D-50's own
// dump. This is also the machine's temporary area at SysEx address 00-00-00.
enum Section { kU1 = 0, kU2, kUpper, kL1, kL2, kLower, kPatch, kSectionCount };

struct Param {
    const char* name;
    uint8_t offset;      // inside its 64-byte block
    uint8_t max;         // largest stored value
    int8_t shift;        // panel value = stored value - shift
    const char* label;
    const char* names;   // see tools/d50_params.py
};

const char* section_name(int section);
int section_from_name(const char* name);            // -1 if unknown
const Param* section_params(int section, int& count);
const Param* find_param(int section, const char* name);

// The flat names of the controller layer and the editor. A partial control
// acts on the selected partial, a tone control on that partial's tone.
struct Control {
    const char* name;
    char kind;           // 'P' partial, 'T' tone, 'G' patch, 'M' macro
    int index;           // into the section table, or the macro number
    int lo, hi;          // panel values
    const char* label;
    const char* names;
};

int control_count();
const Control& control(int i);
const Control* find_control(const char* name);
// Pot position 0..127 <-> panel value: up to 16 values in equal zones, more
// of them linear (as tx81z~ and pdoly8~).
int control_from127(const Control& c, float pot);
int control_to127(const Control& c, int value);

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

    void set_bend(float normalized);          // -1..+1, times the bend range
    void set_mod(float w);                    // 0..1
    void set_aftertouch(float a);             // 0..1
    void set_sustain(bool on);
    // Retire released tails when a block costs more than its time. Off by
    // default; meant for small machines.
    void set_governor(bool on);

    // ---- which patch plays ----------------------------------------------
    enum Source { kFactory = 0, kUser = 1, kInit = 2 };
    bool select_patch(int index);             // a factory preset
    bool select_user(int slot);               // a slot of the user bank
    void select_init();                       // the init patch
    void reload();                            // the same again, edits dropped
    Source source() const;
    int patch() const;                        // index within its source
    std::string name() const;

    // ---- controls ---------------------------------------------------------
    // Patch controls need patch bytes: a bank, the user bank or the init
    // patch. The built-in presets only take the macros that are not bytes.
    bool available(const Control& c) const;
    int get_control(const Control& c) const;             // panel value
    bool set_control(const Control& c, int panel_value); // limited to its range
    std::string control_text(const Control& c, int panel_value) const;

    // ---- patch bytes ------------------------------------------------------
    bool editable() const;
    int peek(int address) const;                     // 0..447, -1 if not editable
    // Writes into the playing patch. Takes effect with the next note; values
    // are limited to the range of their parameter. Chorus, equalizer, reverb
    // and the balances act at once.
    bool poke(int address, const uint8_t* data, int len);
    bool set_param(int section, const Param& p, int panel_value);
    int get_param(int section, const Param& p) const;
    // Names: 18 characters for the patch, 10 for a tone (kUpper, kLower).
    bool set_name(int section, const char* text);
    std::string get_name(int section) const;
    // Copies the playing patch into a slot of the user bank (in memory; see
    // write_user). slot < 0: the slot it came from, or 0.
    bool store(int slot);

    // One D-50 SysEx message in (DT1 writes, RQ1 is answered through `out`).
    // Address 00-00-00 is the playing patch, 02-00-00 the user bank.
    // Returns false if it was not a D-50 message or its checksum was wrong.
    bool sysex(const uint8_t* msg, int len, SysexOut out, void* ctx);
    // The playing patch, or the whole user bank, as DT1 messages.
    void dump_sysex(SysexOut out, void* ctx, int device = 0) const;
    void dump_user(SysexOut out, void* ctx, int device = 0) const;

    struct State {
        int notes_held, voices_sounding, cpu_peak;
        bool sustain, governor;
    };
    State state() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace d50

#endif  // D50_CORE_H
