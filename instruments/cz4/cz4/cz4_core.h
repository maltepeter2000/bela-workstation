// SPDX-License-Identifier: BSD-3-Clause
// cz4_core: a Casio CZ-1 as a library -- 16 voices with two lines each, the six
// 8-step envelopes of every voice, velocity and key follow, vibrato, the CZ-1's
// operation memory (split and tone mix of two tones, portamento, glide, bend,
// wheel, aftertouch) and its chorus. The waves come from core/cz_core (the
// uPD933 phase distortion), the tone data from core/cz_tone.
//
// The envelope law, rates, levels and the logarithmic DCA follow the uPD933 as
// modelled in MAME (Devin Acker) and melodee's cz_native.c (BSD-3); timings the
// chip does not fix (vibrato clock, velocity, key follow, portamento, glide,
// chorus) are estimates until they are compared with recordings.
#pragma once
#include <cstdint>
#include <string>

extern "C" {
#include "cz_core.h"
#include "cz_tone.h"
}

namespace cz4 {

constexpr int kToneBytes = CZ_TONE_BYTES;
constexpr int kUserSlots = 64;
constexpr int kOpSlots = 64;
constexpr int kMaxVoices = 16;

struct Control {
    const char* name;
    char kind;
    int index;
    int lo, hi, def;
    const char* label;
    const char* names;   // "a|b|c" or ""
};

#include "cz4_params.h"

int control_count();
const Control& control(int i);
const Control* find_control(const char* name);
// Pot position 0..127 <-> value: up to 16 values in equal zones, more of them
// linear (as juno~, tx81z~, pdoly8~ and d50~).
int control_from127(const Control& c, float pot);
int control_to127(const Control& c, int value);

// ---- banks, shared by all instruments -----------------------------------------
// Factory: every tone of the .syx files beside the external (the INIT tone when
// there are none). User: 64 tones. Operation memory: 64 slots.
int load_syx(const char* path, std::string& err);       // adds to the factory bank
int patch_count();
const char* patch_name(int i);
int load_user(const char* path, std::string& err);
bool write_user(const char* path, std::string& err);
const char* user_name(int i);
int load_ops(const char* path, std::string& err);
bool write_ops(const char* path, std::string& err);
bool op_used(int i);

typedef void (*SysexOut)(void* ctx, const uint8_t* msg, int len);

class Synth {
public:
    enum Source { kInit = 0, kFactory = 1, kUser = 2, kSysex = 3 };

    Synth();
    ~Synth();
    void set_sample_rate(float fs);

    // controls, in panel values
    int get_control(const Control& c) const;
    void set_control(const Control& c, int v);
    std::string control_text(const Control& c, int v) const;

    // tones: t = 0 tone A, 1 tone B; without t the selected edit tone
    bool select_patch(int i, int t = -1);
    bool select_user(int i, int t = -1);
    void select_init(int t = -1);
    void reload(int t = -1);              // the stored tone again, edits dropped
    void set_compare(bool on);            // play the stored tone while on
    bool compare() const;
    bool store(int slot);                 // the edit tone into a user slot (-1: the one it came from)
    void load_tone(const uint8_t* tone144, int t = -1);   // e.g. from SysEx
    const uint8_t* tone(int t = -1) const;
    int source(int t = -1) const;
    int patch(int t = -1) const;          // its number in its bank
    std::string name(int t = -1) const;
    void set_name(const char* n);
    bool edited(int t = -1) const;
    int edit_tone() const;
    void copy_line(int from, int to);     // in the edit tone
    void copy_env(int from_line, int from_env, int to_line, int to_env);

    // operation memory
    bool select_op(int slot);
    bool store_op(int slot);              // -1: the slot it came from
    void init_op();
    int op_slot() const;

    // playing
    void note_on(int key, int velocity);
    void note_off(int key);
    void set_bend(float v);               // -1..1
    void set_mod(float v);                // 0..1
    void set_aftertouch(float v);         // 0..1
    void set_sustain(bool on);
    bool sustain() const;
    void all_notes_off();
    void silence();
    int voices_sounding() const;

    void render(float* l, float* r, int n);

    // SysEx: the tone as a librarian message (prog 0x60 = the playing tone)
    void dump_tone(SysexOut out, void* ctx, int channel, int prog, int t = -1) const;
    void dump_user(SysexOut out, void* ctx, int channel) const;
    // For the CZ handshake: the tone a program number stands for, and storing one.
    bool tone_for_prog(int prog, uint8_t* out144) const;
    bool receive_prog(int prog, const uint8_t* tone144);

    struct Impl;
private:
    Impl* p_;
};

}  // namespace cz4
