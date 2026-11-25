// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Peter
//
// juno~ -- Roland Juno-60 for Pure Data, on the PicoFaceJ6 engine, with the
// patch format and the SysEx of the Juno-106.
//
//   inlet 1   float: note (velocity from inlet 2; velocity 0 = note off)
//             list:  note velocity [channel]
//             any:   a message, see below
//   inlet 2   float: velocity for the next note, 0..127 (default 100)
//   inlet 3   float or list: raw MIDI bytes, as [midiin] gives them: notes,
//             bend, program change, CC 1 7 64 and Juno-106 SysEx
//             any:   a message (the same set as inlet 1)
//   outlet 1  signal, left
//   outlet 2  signal, right
//   outlet 3  messages: info <name> <0-127> <text> for every change, preset /
//             user names, channel ...   floats: SysEx bytes for [midiout]
//             display <title> <value>: two lines for a display (the Launchkey
//             through [lkdisplay], the OLED on Bela) -- a control set by a message
//             (VCF freq / 64; not by a patch change), the patch that plays
//             (Preset 12 / Harpsichord 1), a store (Stored / User 11)
//
// Controls (the names of tools/juno_params.py, the same for the controller
// layer [junocc] and the editor [junoedit]):
//   <name> <value>      set a control, in the values the panel shows
//   <name>              report it
//   n <name> <0-127>    set it from a pot or a MIDI CC
//   cycle <name> [n]    the next value (n = -1: the previous one), wrapping
//                       round: cycle chorus goes off, I, II, I+II, off
//   ninfo <name>        report it as info
//
// Patches:
//   preset n            a factory preset: 0..55 the Juno-60 sounds, then the
//                       Juno-106 banks (.syx) beside the external
//   user n              a slot of the user bank, 0..55
//   init / reload       the init patch / the same patch again, edits dropped
//   store [n]           the playing patch into user slot n (in memory); without
//                       n the user slot selected or stored last
//   save                store [no n] and userwrite in one
//   presetstep [n]      the next factory preset (n = -1: the previous one)
//   userstep [n]        the same for the user bank
//   userwrite [file]    write the user bank (default: junouser-bank.syx)
//   userread [file]     read it
//   usersend            the user bank as Juno-106 SysEx bytes
//   presets / users     list the names
//
//   param addr [v]      one of the 18 patch bytes
//   dump                every control as info
//   sysex b b b ...     one Juno-106 SysEx message as a list of bytes
//   sysexdump           the playing patch as SysEx bytes
//   thru 0/1            MIDI thru (default 1): notes, controllers, program
//                       changes, bend, pressure, clock and SysEx arriving at
//                       inlet 3 leave outlet 3 unchanged, notes from inlet 1
//                       on txchannel
//   sysexout 0/1        behave like a Juno-106: send a patch when one is
//                       selected and a parameter message for every edit
//
// Playing: bend f (-1..1), mod f (0..1), sustain 0/1, stop, reset, channel n
// (receive, 0 = all), txchannel n (send, 1..16), status.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#include <dirent.h>
#endif

extern "C" {
#include "m_pd.h"
}

#include "juno_core.h"
#include "../../../common/gfx/lkgfx_pd.h"

namespace {

t_class* juno_class = nullptr;
t_class* juno_proxy_class = nullptr;

const char kUserFile[] = "junouser-bank.syx";

struct t_juno;

struct t_juno_proxy {
    t_pd pd;
    t_juno* owner;
};

struct Midi {
    std::vector<uint8_t> sx;   // a SysEx message being collected
    bool in_sx = false;
    int status = 0;            // running status of channel messages
    int data[2] = {0, 0};
    int have = 0;
};

struct Shown {                 // what outlet 3 last said
    std::vector<int> value;    // per control; INT32_MIN = not shown yet
    int bytes[juno::kPatchBytes];  // the patch as last sent or seen
    int source = -1, index = -1;
};

struct t_juno {
    t_object obj;
    juno::Synth* synth;
    t_float velocity;
    t_juno_proxy proxy;
    t_outlet* info;
    Midi* midi;
    Shown* shown;
    int channel;               // receive, 0 = all
    int txchannel;             // send, 1..16
    int sysexout;              // send SysEx for every change, as a Juno-106 does
    int thru;                  // pass incoming MIDI on to outlet 3
    int store_to;              // savearm .. save: the user slot chosen, -1 none
    lkg_pd_t* gfx;             // Launchkey pictures (common/gfx): 1 ADSR, 2 filter, 3 the DCO wave
};

// ---- data files -------------------------------------------------------------
// Nothing is needed. Juno-106 banks (*.syx) beside the external join the
// factory presets, and the user bank (junouser-bank.syx) is read from there.
// Loaded once, by the first object created.

bool g_data_tried = false;
std::string g_data_dir;

bool file_exists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void load_data() {
    if (g_data_tried) return;
    g_data_tried = true;
    const char* h = class_gethelpdir(juno_class);
    if (!h || !*h) return;
    g_data_dir = h;
    std::vector<std::string> files;
#ifndef _WIN32
    if (DIR* d = opendir(g_data_dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.size() < 5 || n == kUserFile) continue;
            std::string ext = n.substr(n.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".syx") files.push_back(n);
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());
#endif
    for (const std::string& n : files) {
        std::string err;
        const int got = juno::load_syx((g_data_dir + "/" + n).c_str(), err);
        if (got < 0) pd_error(nullptr, "juno~: %s: %s", n.c_str(), err.c_str());
        else post("juno~: %s: %d patches", n.c_str(), got);
    }
    const std::string user = g_data_dir + "/" + kUserFile;
    if (file_exists(user)) {
        std::string err;
        const int got = juno::load_user(user.c_str(), err);
        if (got < 0) pd_error(nullptr, "juno~: %s: %s", kUserFile, err.c_str());
        else post("juno~: %s: user bank, %d patches", kUserFile, got);
    }
}

std::string user_path(int argc, t_atom* argv) {
    if (argc > 0 && argv[0].a_type == A_SYMBOL) {
        const std::string f = atom_getsymbol(argv)->s_name;
        if (!f.empty() && (f[0] == '/' || (f.size() > 1 && f[1] == ':'))) return f;
        return g_data_dir + "/" + f;
    }
    return g_data_dir + "/" + kUserFile;
}

// ---- output -----------------------------------------------------------------

void out_value(t_juno* x, const char* key, t_float v) {
    t_atom a;
    SETFLOAT(&a, v);
    outlet_anything(x->info, gensym(key), 1, &a);
}

void out_slot(t_juno* x, const char* key, int index, const char* name) {
    t_atom a[2];
    SETFLOAT(&a[0], index);
    SETSYMBOL(&a[1], gensym(name));
    outlet_anything(x->info, gensym(key), 2, a);
}

// SysEx leaves the way it arrives: one float per byte, so the third outlet
// goes through [route float] straight into [midiout] or another juno~.
void out_sysex(void* ctx, const uint8_t* msg, int len) {
    t_juno* x = static_cast<t_juno*>(ctx);
    for (int i = 0; i < len; ++i) outlet_float(x->info, msg[i]);
}

void out_info(t_juno* x, int i) {
    const juno::Control& c = juno::control(i);
    const int v = x->synth->get_control(c);
    x->shown->value[i] = v;
    t_atom a[3];
    SETSYMBOL(&a[0], gensym(c.name));
    SETFLOAT(&a[1], juno::control_to127(c, v));
    SETSYMBOL(&a[2], gensym(x->synth->control_text(c, v).c_str()));
    outlet_anything(x->info, gensym("info"), 3, a);
}

// One line for a display (the OLED on Bela, later the Launchkey): what was
// turned, which patch plays, what was stored.
// ---- Launchkey pictures (custom firmware branch display-gfx) ------------------
// The ADSR (attack, decay, sustain, release), the filter curve (cutoff, res, hpf) and the DCO
// wave (pulse, saw, sub, pwm, pwmmode, noise) instead of the text line. "lkgfx 0" = text.

int gfx_kind(const char* n) {
    for (const char* k : {"attack", "decay", "sustain", "release"}) if (!std::strcmp(n, k)) return 1;
    for (const char* k : {"cutoff", "res", "hpf"}) if (!std::strcmp(n, k)) return 2;
    for (const char* k : {"pulse", "saw", "sub", "pwm", "pwmmode", "noise"}) if (!std::strcmp(n, k)) return 3;
    return 0;
}

float ctl(const juno::Synth& y, const char* name) {
    const juno::Control* c = juno::find_control(name);
    return c ? static_cast<float>(y.get_control(*c)) : 0.0f;
}

void gfx_draw(void* owner, lkg_t* g, int kind) {
    const juno::Synth& y = *static_cast<t_juno*>(owner)->synth;
    if (kind == 1) {
        // times grow steeply along the slider (1 ms .. 3 s, 2 ms .. 12 s): width 2 + 40 x^1.7
        auto w = [](float v) { return 2.0f + 40.0f * std::pow(v / 127.0f, 1.7f); };
        const float sus = ctl(y, "sustain") / 127.0f;
        const float lvl[3] = {1.0f, sus, 0.0f};
        const float wd[3] = {w(ctl(y, "attack")), w(ctl(y, "decay")), w(ctl(y, "release"))};
        lkg_env(g, 3, 0.0f, lvl, wd, 1, 24.0f, nullptr);
    } else if (kind == 2) {
        // 24 dB low pass; the HPF positions 2 and 3 cut the bass (0 and 1: none drawn)
        static const float kHp[4] = {0.0f, 0.0f, 0.14f, 0.24f};
        const int hpf = std::clamp(static_cast<int>(ctl(y, "hpf")), 0, 3);
        lkg_filter(g, ctl(y, "cutoff") / 127.0f, ctl(y, "res") / 127.0f, LKG_LP24, kHp[hpf]);
    } else if (kind == 3) {
        // the DCO's mixer: saw, pulse (width by pwm in MAN mode), sub square an octave down,
        // noise; two periods of the DCO across the width
        const bool saw = ctl(y, "saw") > 0, pulse = ctl(y, "pulse") > 0;
        const float sub = ctl(y, "sub") / 127.0f, noise = ctl(y, "noise") / 127.0f;
        const float pw = ctl(y, "pwmmode") > 0 ? 0.5f - 0.45f * ctl(y, "pwm") / 127.0f : 0.5f;
        float v[LKG_W], lo = 0.0f, hi = 0.0f;
        unsigned rnd = 12345u;
        for (int i = 0; i < LKG_W; ++i) {
            const float ph = static_cast<float>(i % 64) / 64.0f, ph2 = static_cast<float>(i) / 128.0f;
            float s = 0.0f;
            if (saw) s += 1.0f - 2.0f * ph;
            if (pulse) s += ph < pw ? 1.0f : -1.0f;
            s += sub * (ph2 < 0.5f ? 1.0f : -1.0f);
            rnd = rnd * 1103515245u + 12345u;
            s += noise * 0.6f * (static_cast<float>(rnd >> 16 & 0x7fff) / 16384.0f - 1.0f);
            v[i] = s;
            lo = std::min(lo, s);
            hi = std::max(hi, s);
        }
        const float m = std::max(std::max(-lo, hi), 1.0f);
        lkg_plot(g, v, LKG_W, -m, m);
        lkg_hdots(g, 0, LKG_W - 1, static_cast<int>(lroundf(lkg_ly(0.5f))), 4);
    }
}

void out_display(t_juno* x, const std::string& title, const std::string& value) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(title.c_str()));
    SETSYMBOL(&a[1], gensym(value.c_str()));
    outlet_anything(x->info, gensym("display"), 2, a);
}

void out_source(t_juno* x) {
    const int src = x->synth->source();
    const int idx = x->synth->patch();
    if (src == juno::Synth::kFactory) out_display(x, "Preset " + std::to_string(idx), x->synth->name());
    else if (src == juno::Synth::kUser)
        out_display(x, std::string("User ") + juno::user_name(idx), x->synth->name());
    else out_display(x, "Init", "");
    if (src == juno::Synth::kFactory) out_slot(x, "preset", x->synth->patch(), juno::patch_name(x->synth->patch()));
    else if (src == juno::Synth::kUser) out_slot(x, "user", x->synth->patch(), juno::user_name(x->synth->patch()));
    else outlet_anything(x->info, gensym("init"), 0, nullptr);
    t_atom a;
    SETSYMBOL(&a, gensym(x->synth->name().c_str()));
    outlet_anything(x->info, gensym("name"), 1, &a);
}

// Everything that changed since outlet 3 last spoke: the patch that plays and
// every control -- in the logical time of the change, which is how the
// controller layer tells its own encoder from a change made elsewhere.
// `quiet` only takes note of the state. `echo` is false for changes that came
// in as SysEx: a Juno-106 does not send back what it receives.
void report(t_juno* x, bool quiet = false, bool echo = true) {
    juno::Synth& y = *x->synth;
    Shown& s = *x->shown;
    const bool send = !quiet && echo && x->sysexout;
    const int src = y.source(), idx = y.patch();
    bool patch_sent = false;
    if (src != s.source || idx != s.index) {
        s.source = src; s.index = idx;
        if (!quiet) out_source(x);
        if (send) { y.dump_sysex(out_sysex, x, x->txchannel - 1); patch_sent = true; }
    }
    // The patch bytes that changed, as a Juno-106 would send them. Chorus I+II
    // moves two bytes, and both go out.
    for (int b = 0; b < juno::kPatchBytes; ++b) {
        const int v = y.peek(b);
        if (v == s.bytes[b]) continue;
        s.bytes[b] = v;
        if (send && !patch_sent) y.dump_param(out_sysex, x, x->txchannel - 1, b);
    }
    for (int i = 0; i < juno::control_count(); ++i) {
        const juno::Control& c = juno::control(i);
        const int v = y.get_control(c);
        if (v == s.value[i]) continue;
        if (quiet) { s.value[i] = v; continue; }
        out_info(x, i);
    }
}

// ---- messages ---------------------------------------------------------------

void select_program(t_juno* x, int pgm) {
    juno::Synth& y = *x->synth;
    if (y.source() == juno::Synth::kUser && pgm < juno::kUserSlots) y.select_user(pgm);
    else y.select_patch(pgm);
}

void juno_command(t_juno* x, t_symbol* s, int argc, t_atom* argv) {
    juno::Synth& y = *x->synth;
    const char* c = s->s_name;
    const t_float f = argc > 0 ? atom_getfloat(argv) : 0;
    const int n = static_cast<int>(f);
    auto is = [c](const char* k) { return std::strcmp(c, k) == 0; };
    bool echo = true;
    const juno::Control* touched = nullptr;   // shown on the display afterwards
    bool stored = false;

    if (const juno::Control* ctl = juno::find_control(c)) {
        if (argc > 0) { y.set_control(*ctl, n); touched = ctl; }
        else out_info(x, static_cast<int>(ctl - &juno::control(0)));
    } else if (is("n")) {
        if (argc < 2 || argv[0].a_type != A_SYMBOL) return;
        const char* name = atom_getsymbol(argv)->s_name;
        const juno::Control* nc = juno::find_control(name);
        if (!nc) {
            if (std::strcmp(name, "none") != 0 && std::strcmp(name, "-") != 0)
                pd_error(x, "juno~: n: no control '%s'", name);
            return;
        }
        const int i = static_cast<int>(nc - &juno::control(0));
        const int before = y.get_control(*nc);
        y.set_control(*nc, juno::control_from127(*nc, atom_getfloat(argv + 1)));
        touched = nc;
        // Always answered, also when the pot stays inside one value's zone.
        if (y.get_control(*nc) == before) out_info(x, i);
    } else if (is("cycle")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        const juno::Control* nc = juno::find_control(atom_getsymbol(argv)->s_name);
        if (!nc) { pd_error(x, "juno~: cycle: no control '%s'", atom_getsymbol(argv)->s_name); return; }
        const int span = nc->hi - nc->lo + 1;
        const int by = argc > 1 ? static_cast<int>(atom_getfloat(argv + 1)) : 1;
        const int v = y.get_control(*nc) - nc->lo + by;
        y.set_control(*nc, nc->lo + ((v % span) + span) % span);
        touched = nc;
    } else if (is("ninfo")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        if (const juno::Control* nc = juno::find_control(atom_getsymbol(argv)->s_name))
            out_info(x, static_cast<int>(nc - &juno::control(0)));
    } else if (is("param")) {
        if (argc < 1 || n < 0 || n >= juno::kPatchBytes) {
            pd_error(x, "juno~: param: expected an address 0..%d", juno::kPatchBytes - 1);
        } else if (argc > 1) {
            y.poke(n, static_cast<int>(atom_getfloat(argv + 1)));
        } else {
            t_atom a[2];
            SETFLOAT(&a[0], n);
            SETFLOAT(&a[1], y.peek(n));
            outlet_anything(x->info, gensym("param"), 2, a);
        }
    } else if (is("dump")) {
        for (int i = 0; i < juno::control_count(); ++i) out_info(x, i);
    } else if (is("preset")) {
        if (argc < 1 || !y.select_patch(n))
            pd_error(x, "juno~: preset: expected 0..%d", juno::patch_count() - 1);
        else x->shown->source = -2;      // always answered, also for the same one again
    } else if (is("user")) {
        if (argc < 1 || !y.select_user(n))
            pd_error(x, "juno~: user: expected 0..%d", juno::kUserSlots - 1);
        else x->shown->source = -2;
    } else if (is("init")) {
        y.select_init();
        x->shown->source = -2;
    } else if (is("reload")) {
        y.reload();
        x->shown->source = -2;
    } else if (is("presetstep") || is("userstep")) {
        y.step(is("presetstep") ? juno::Synth::kFactory : juno::Synth::kUser, argc > 0 ? n : 1);
        x->shown->source = -2;
    } else if (is("savearm")) {
        // the pads' store with a question: show the slot, saveslot moves it, save stores there
        x->store_to = y.last_user();
        out_display(x, "Store to?", std::string("User ") + juno::user_name(x->store_to));
        echo = false;
    } else if (is("saveslot")) {
        if (x->store_to >= 0) {
            x->store_to = ((x->store_to + (argc > 0 ? n : 1)) % juno::kUserSlots + juno::kUserSlots) % juno::kUserSlots;
            out_display(x, "Store to?", std::string("User ") + juno::user_name(x->store_to));
        }
        echo = false;
    } else if (is("savecancel")) {
        if (x->store_to >= 0) out_display(x, "Store", "cancelled");
        x->store_to = -1;
        echo = false;
    } else if (is("save")) {
        // store into the slot chosen (savearm), else the one selected or stored last, and write the bank
        stored = y.store(x->store_to);
        x->store_to = -1;
        if (!g_data_dir.empty()) {
            std::string err;
            const std::string path = g_data_dir + "/" + kUserFile;
            if (!juno::write_user(path.c_str(), err)) pd_error(x, "juno~: save: %s", err.c_str());
        }
        x->shown->source = -2;
        echo = false;
    } else if (is("store")) {
        if (!y.store(argc > 0 ? n : -1))
            pd_error(x, "juno~: store: expected 0..%d", juno::kUserSlots - 1);
        else stored = true;
        echo = false;                    // the sound did not change
    } else if (is("userwrite")) {
        if (g_data_dir.empty()) return;
        std::string err;
        const std::string path = user_path(argc, argv);
        if (!juno::write_user(path.c_str(), err)) pd_error(x, "juno~: userwrite: %s", err.c_str());
        else post("juno~: user bank written to %s", path.c_str());
    } else if (is("userread")) {
        if (g_data_dir.empty()) return;
        std::string err;
        const std::string path = user_path(argc, argv);
        const int got = juno::load_user(path.c_str(), err);
        if (got < 0) pd_error(x, "juno~: userread: %s", err.c_str());
        else {
            post("juno~: user bank: %d patches from %s", got, path.c_str());
            if (y.source() == juno::Synth::kUser) y.reload();
        }
    } else if (is("usersend")) {
        y.dump_user(out_sysex, x, x->txchannel - 1);
    } else if (is("presets")) {
        const int count = juno::patch_count();
        for (int i = 0; i < count; ++i) out_slot(x, "preset", i, juno::patch_name(i));
        out_value(x, "presets", count);
    } else if (is("users")) {
        for (int i = 0; i < juno::kUserSlots; ++i) out_slot(x, "user", i, juno::user_name(i));
        out_value(x, "users", juno::kUserSlots);
    } else if (is("sysex")) {
        std::vector<uint8_t> msg;
        if (argc > 0 && atom_getfloat(argv) != 0xF0) msg.push_back(0xF0);
        for (int i = 0; i < argc; ++i)
            msg.push_back(static_cast<uint8_t>(static_cast<int>(atom_getfloat(argv + i)) & 0xFF));
        if (msg.empty() || msg.back() != 0xF7) msg.push_back(0xF7);
        if (!y.sysex(msg.data(), static_cast<int>(msg.size())))
            pd_error(x, "juno~: sysex: not a Juno-106 message");
        echo = false;
    } else if (is("sysexdump")) {
        y.dump_sysex(out_sysex, x, x->txchannel - 1);
    } else if (is("thru")) {
        x->thru = n != 0;
        out_value(x, "thru", x->thru);
    } else if (is("sysexout")) {
        x->sysexout = n != 0;
        out_value(x, "sysexout", x->sysexout);
    } else if (is("channel")) {
        x->channel = n < 0 ? 0 : (n > 16 ? 16 : n);
        out_value(x, "channel", x->channel);
    } else if (is("txchannel")) {
        x->txchannel = n < 1 ? 1 : (n > 16 ? 16 : n);
        out_value(x, "txchannel", x->txchannel);
    } else if (is("status")) {
        out_source(x);
        out_value(x, "channel", x->channel);
        out_value(x, "txchannel", x->txchannel);
        out_value(x, "sysexout", x->sysexout);
        out_value(x, "thru", x->thru);
        out_value(x, "sustain", y.sustain());
        out_value(x, "sounding", y.voices_sounding());
    }
    else if (is("bend")) y.set_bend(f);
    else if (is("mod")) y.set_mod(f);
    else if (is("sustain")) y.set_sustain(f != 0);
    else if (is("stop")) y.all_notes_off();
    else if (is("reset")) y.silence();
    else if (is("lkgfx")) { lkg_pd_on(x->gfx, n != 0); return; }
    else { pd_error(x, "juno~: no method for '%s'", c); return; }
    report(x, false, echo);
    if (touched) {
        const std::string v = y.control_text(*touched, y.get_control(*touched));
        if (!lkg_pd_show(x->gfx, gfx_kind(touched->name), touched->label, v.c_str()))
            out_display(x, touched->label, v);
    }
    if (stored) out_display(x, "Stored", std::string("User ") + juno::user_name(y.patch()));
}

void juno_note(t_juno* x, t_float note, t_float velocity) {
    const int n = static_cast<int>(note);
    if (x->thru && n >= 0 && n < 128) {
        const int v = velocity < 0 ? 0 : (velocity > 127 ? 127 : static_cast<int>(velocity));
        outlet_float(x->info, 0x90 | ((x->txchannel - 1) & 0x0F));
        outlet_float(x->info, n);
        outlet_float(x->info, v);
    }
    if (velocity > 0) x->synth->note_on(n, velocity / 127.0f);
    else x->synth->note_off(n);
}

void juno_float(t_juno* x, t_float f) { juno_note(x, f, x->velocity); }

void juno_list(t_juno* x, t_symbol*, int argc, t_atom* argv) {
    if (argc >= 3 && x->channel && static_cast<int>(atom_getfloat(argv + 2)) != x->channel) return;
    if (argc >= 2) juno_note(x, atom_getfloat(argv), atom_getfloat(argv + 1));
    else if (argc == 1) juno_float(x, atom_getfloat(argv));
}

void juno_proxy_anything(t_juno_proxy* p, t_symbol* s, int argc, t_atom* argv) {
    juno_command(p->owner, s, argc, argv);
}

// ---- raw MIDI ---------------------------------------------------------------

void midi_message(t_juno* x, int status, int d1, int d2) {
    juno::Synth& y = *x->synth;
    if (x->thru) {      // a MIDI thru passes everything, on every channel
        outlet_float(x->info, status);
        outlet_float(x->info, d1);
        const int t = status & 0xF0;
        if (t != 0xC0 && t != 0xD0) outlet_float(x->info, d2);
    }
    if (x->channel && (status & 0x0F) + 1 != x->channel) return;
    switch (status & 0xF0) {
        case 0x90:
            if (d2 > 0) y.note_on(d1, d2 / 127.0f);
            else y.note_off(d1);             // a note on with velocity 0
            return;
        case 0x80: y.note_off(d1); return;
        case 0xB0:
            switch (d1) {
                case 1: y.set_mod(d2 / 127.0f); return;
                case 7:
                    if (const juno::Control* c = juno::find_control("volume")) y.set_control(*c, d2);
                    break;
                case 64: y.set_sustain(d2 >= 64); return;
                case 120: y.silence(); return;
                case 123: y.all_notes_off(); return;
                default: return;          // the CC pages belong to [junocc]
            }
            break;
        case 0xC0: select_program(x, d1); break;
        case 0xE0: y.set_bend(((d2 << 7 | d1) - 8192) / 8192.0f); return;
        default: return;
    }
    report(x);
}

void juno_proxy_float(t_juno_proxy* p, t_float f) {
    t_juno* x = p->owner;
    Midi& m = *x->midi;
    const int b = static_cast<int>(f) & 0xFF;
    if (b >= 0xF8) {                                         // real time
        if (x->thru) outlet_float(x->info, b);
        return;
    }
    if (b == 0xF0) { m.in_sx = true; m.sx.assign(1, 0xF0); return; }
    if (m.in_sx) {
        if (b < 0x80) {
            if (m.sx.size() < 65536) m.sx.push_back(static_cast<uint8_t>(b));
            return;
        }
        m.in_sx = false;
        if (b == 0xF7) {
            m.sx.push_back(0xF7);
            // MIDI thru passes every SysEx message on, whoever it is for.
            if (x->thru)
                out_sysex(x, m.sx.data(), static_cast<int>(m.sx.size()));
            // Other makers' and other Rolands' messages pass by silently.
            if (m.sx.size() >= 7 && m.sx[1] == 0x41 && m.sx[2] >= 0x30 && m.sx[2] <= 0x32 &&
                (!x->channel || (m.sx[3] & 0x0F) + 1 == x->channel)) {
                if (x->synth->sysex(m.sx.data(), static_cast<int>(m.sx.size())))
                    report(x, false, false);
            }
            return;
        }
    }
    if (b >= 0xF0) { m.status = 0; return; }                 // system common
    if (b >= 0x80) { m.status = b; m.have = 0; return; }
    if (!m.status) return;
    m.data[m.have++] = b;
    const int need = ((m.status & 0xF0) == 0xC0 || (m.status & 0xF0) == 0xD0) ? 1 : 2;
    if (m.have < need) return;
    m.have = 0;
    midi_message(x, m.status, m.data[0], need == 2 ? m.data[1] : 0);
}

void juno_proxy_list(t_juno_proxy* p, t_symbol*, int argc, t_atom* argv) {
    for (int i = 0; i < argc; ++i) juno_proxy_float(p, atom_getfloat(argv + i));
}

// ---- dsp --------------------------------------------------------------------

t_int* juno_perform(t_int* w) {
    t_juno* x = reinterpret_cast<t_juno*>(w[1]);
    t_sample* l = reinterpret_cast<t_sample*>(w[2]);
    t_sample* r = reinterpret_cast<t_sample*>(w[3]);
    const int n = static_cast<int>(w[4]);
    x->synth->render(l, r, n);
    return w + 5;
}

void juno_dsp(t_juno* x, t_signal** sp) {
    x->synth->set_sample_rate(sp[0]->s_sr);
    dsp_add(juno_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, static_cast<t_int>(sp[0]->s_n));
}

// ---- lifetime ---------------------------------------------------------------

void* juno_new(t_symbol*, int argc, t_atom* argv) {
    t_juno* x = reinterpret_cast<t_juno*>(pd_new(juno_class));
    load_data();
    x->velocity = 100;
    x->channel = 0;
    x->txchannel = 1;
    x->sysexout = 0;
    x->thru = 1;
    x->store_to = -1;
    x->midi = new Midi();
    x->shown = new Shown();
    x->shown->value.assign(static_cast<size_t>(juno::control_count()), INT32_MIN);
    for (int& b : x->shown->bytes) b = -1;
    x->synth = new juno::Synth();

    floatinlet_new(&x->obj, &x->velocity);
    x->proxy.pd = juno_proxy_class;
    x->proxy.owner = x;
    inlet_new(&x->obj, &x->proxy.pd, nullptr, nullptr);

    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);
    x->info = outlet_new(&x->obj, nullptr);
    x->gfx = new lkg_pd_t();
    lkg_pd_init(x->gfx, x->info, gfx_draw, x);

    if (argc > 0 && argv[0].a_type == A_FLOAT)
        x->synth->select_patch(static_cast<int>(atom_getfloat(argv)));
    report(x, true);
    return x;
}

void juno_free(t_juno* x) {
    lkg_pd_free(x->gfx);
    delete x->gfx;
    delete x->synth;
    delete x->midi;
    delete x->shown;
}

}  // namespace

extern "C" {
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void juno_tilde_setup(void) {
    juno_class = class_new(gensym("juno~"), reinterpret_cast<t_newmethod>(juno_new),
                           reinterpret_cast<t_method>(juno_free), sizeof(t_juno),
                           CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(juno_class, reinterpret_cast<t_method>(juno_float));
    class_addlist(juno_class, reinterpret_cast<t_method>(juno_list));
    class_addanything(juno_class, reinterpret_cast<t_method>(juno_command));
    class_addmethod(juno_class, reinterpret_cast<t_method>(juno_dsp), gensym("dsp"), A_CANT, 0);

    juno_proxy_class = class_new(gensym("juno~ midi"), nullptr, nullptr,
                                 sizeof(t_juno_proxy), CLASS_PD | CLASS_NOINLET, A_NULL);
    class_addanything(juno_proxy_class, reinterpret_cast<t_method>(juno_proxy_anything));
    class_addfloat(juno_proxy_class, reinterpret_cast<t_method>(juno_proxy_float));
    class_addlist(juno_proxy_class, reinterpret_cast<t_method>(juno_proxy_list));
}
}
