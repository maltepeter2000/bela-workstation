// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Michi71 (PicoFaceJ6 engine this drives)
// SPDX-FileCopyrightText: 2026 Peter (Pure Data adaptation)
//
// juno_core.h -- the PicoFaceJ6 Juno-60 engine behind a host-neutral interface.
//
// The engine is a Juno-60. Its patches are kept here in the format of the
// Juno-106, whose panel is nearly the same and which, unlike the Juno-60, has
// MIDI: 18 bytes, 16 sliders and two switch bytes. That makes Juno-106 patch
// collections playable and lets a patch or the user bank go to a Juno-106 or
// an editor for it. Two things of the Juno-60 do not fit:
//
//   * Chorus I+II. Kept in a bit of the second switch byte a Juno-106 does not
//     use; a Juno-106 plays such a patch with chorus I.
//   * Pulse width from the envelope. Left out; the factory sounds that used it
//     have the fixed width instead.
//
// The arpeggiator of the engine is not used. Velocity and the modulation
// wheel are additions: velocity is off by default, the wheel adds vibrato as
// on a Juno-106.
//
// No Pure Data in here; juno~.cpp is the only file that knows m_pd.h.
#ifndef JUNO_CORE_H
#define JUNO_CORE_H

#include <cstdint>
#include <string>

namespace juno {

constexpr int kPatchBytes = 18;
constexpr int kUserSlots = 56;     // seven banks of eight, as on the Juno-60
constexpr int kVoices = 6;

// ---- shared patch memory ---------------------------------------------------
// One copy for every instance. Not thread-safe to load.

// Factory presets: the 56 sounds of the Juno-60, then every Juno-106 bank
// loaded with load_syx. Read-only.
int load_syx(const char* path, std::string& err);       // patches added, or -1
int patch_count();
const char* patch_name(int index);

// The user bank: 56 patches that can be stored to. It starts as init patches.
// The file is a row of Juno-106 patch messages, numbered 0..55.
int load_user(const char* path, std::string& err);      // patches read, or -1
bool write_user(const char* path, std::string& err);
const char* user_name(int slot);                        // "11" .. "78"

// ---- controls ----------------------------------------------------------------
struct Control {
    const char* name;
    char kind;           // 'P' slider byte, 'W' switch, 'S' setting (not in a patch)
    int index;
    int lo, hi;
    const char* label;
    const char* names;   // "a|b|c" or ""
};

int control_count();
const Control& control(int i);
const Control* find_control(const char* name);
// Pot position 0..127 <-> value: up to 16 values in equal zones, more of them
// linear (as tx81z~, pdoly8~ and d50~).
int control_from127(const Control& c, float pot);
int control_to127(const Control& c, int value);

// Receives one complete SysEx message, F0 to F7.
typedef void (*SysexOut)(void* ctx, const uint8_t* msg, int len);

// ---- one instrument ----------------------------------------------------------
class Synth {
public:
    Synth();
    ~Synth();
    Synth(const Synth&) = delete;
    Synth& operator=(const Synth&) = delete;

    void set_sample_rate(float sr);
    void render(float* left, float* right, int frames);

    void note_on(int note, float velocity);   // velocity 0..1
    void note_off(int note);
    void all_notes_off();
    void silence();

    void set_bend(float normalized);          // -1..+1, times the bend range
    void set_mod(float w);                    // 0..1
    void set_sustain(bool on);

    // ---- which patch plays ----------------------------------------------
    enum Source { kFactory = 0, kUser = 1, kInit = 2 };
    bool select_patch(int index);
    bool select_user(int slot);
    void select_init();
    void reload();
    Source source() const;
    int patch() const;
    std::string name() const;

    // ---- controls ---------------------------------------------------------
    int get_control(const Control& c) const;
    void set_control(const Control& c, int value);      // limited to its range
    std::string control_text(const Control& c, int value) const;

    // ---- patch bytes ------------------------------------------------------
    int peek(int address) const;                        // 0..17
    bool poke(int address, int value);
    // The next (by = 1) or previous (-1) factory preset or user slot, counted
    // from the one selected last in that bank, wrapping round.
    bool step(Source src, int by);
    // Copies the playing patch into a slot of the user bank (in memory; see
    // write_user). slot < 0: the user slot selected or stored last (0 at first).
    bool store(int slot);

    // One Juno-106 message in: a patch (30, 31) replaces the playing patch, a
    // parameter (32) changes one byte of it. False if it was something else.
    bool sysex(const uint8_t* msg, int len);
    // The playing patch, one byte of it, or the whole user bank, as Juno-106
    // messages on MIDI channel `channel` (0..15).
    void dump_sysex(SysexOut out, void* ctx, int channel) const;
    void dump_param(SysexOut out, void* ctx, int channel, int address) const;
    void dump_user(SysexOut out, void* ctx, int channel) const;

    int voices_sounding() const;
    bool sustain() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace juno

#endif  // JUNO_CORE_H
