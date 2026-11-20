// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Peter
//
// d50~ -- Roland D-50 for Pure Data, on the PicoFaceD5 LA engine.
//
//   inlet 1   float: note (velocity from inlet 2; velocity 0 = note off)
//             list:  note velocity [channel]
//             any:   a message, see below
//   inlet 2   float: velocity for the next note, 0..127 (default 100)
//   inlet 3   float or list: raw MIDI bytes, as [midiin] gives them: notes,
//             bend, aftertouch, program change, CC 1 5 7 64 65 and D-50 SysEx
//             any:   a message (the same set as inlet 1)
//   outlet 1  signal, left
//   outlet 2  signal, right
//   outlet 3  messages: info <name> <0-127> <text> for every change, preset /
//             user names, channel ...   floats: SysEx bytes for [midiout]
//
// Controls (the names of tools/d50_params.py, the same for the controller
// layer [d50cc] and the editor [d50edit]):
//   <name> <value>      set a control, in the values the D-50 shows
//   <name>              report it
//   n <name> <0-127>    set it from a pot or a MIDI CC
//   ninfo <name>        report it as info
// A partial name (cutoff, level ...) acts on the selected partial (part 0..3 =
// U1 U2 L1 L2), a tone name (structure, lfo1rate ...) on that partial's tone.
//
// Patches:
//   preset n            a factory preset (0-based over all loaded banks)
//   user n              a slot of the user bank, 0..63
//   init / reload       the init patch / the same patch again, edits dropped
//   store [n]           the playing patch into user slot n (in memory)
//   userwrite [file]    write the user bank (default: d50user-bank.syx)
//   userread [file]     read it
//   usersend            the user bank as SysEx bytes (to a D-50 or a librarian)
//   presets / users     list the names
//
// By address, without changing the selection:
//   <section> <name> [v]   sections u1 u2 upper l1 l2 lower patch
//   <section> name text    name of the patch or of a tone (upper, lower)
//   param addr [v]         one of the 448 patch bytes, stored value
//   dump                   every named parameter of every section
//   sysex b b b ...        one D-50 SysEx message as a list of bytes
//   sysexdump              the playing patch as SysEx bytes
//
// Playing: bend f (-1..1), mod f, aftertouch f (0..1), sustain 0/1, stop,
// reset, governor 0/1, channel n (receive, 0 = all), txchannel n (send, 1..16),
// status.
//
// The engine's headers are large inline functions by design, and Pd's method
// tables are casts by design; the build helper's warnings for both are noise.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include <algorithm>
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

#include "d50_core.h"

namespace {

t_class* d50_class = nullptr;
t_class* d50_proxy_class = nullptr;

const char kUserFile[] = "d50user-bank.syx";

struct t_d50;

struct t_d50_proxy {
    t_pd pd;
    t_d50* owner;
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
    std::string name, upper, lower;
    int source = -1, index = -1;
    std::string source_name;
};

struct t_d50 {
    t_object obj;
    d50::Synth* synth;
    t_float velocity;
    t_d50_proxy proxy;
    t_outlet* info;
    Midi* midi;
    Shown* shown;
    int channel;               // receive, 0 = all
    int txchannel;             // send, 1..16
};

// ---- data files -------------------------------------------------------------
// d50_pcm.bin, the factory banks (*.syx) and the user bank (d50user-bank.syx)
// sit beside the external, or beside the patch. Loaded once, by the first
// object created.

bool g_data_tried = false;
std::string g_data_dir;

bool file_exists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void load_banks(const std::string& dir) {
    std::vector<std::string> files;
#ifndef _WIN32
    if (DIR* d = opendir(dir.c_str())) {
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
    // The factory card leads, so preset 0 never moves when banks join.
    auto fac = std::find(files.begin(), files.end(), "PND50-00.syx");
    if (fac != files.end()) std::rotate(files.begin(), fac, fac + 1);
#else
    if (file_exists(dir + "/PND50-00.syx")) files.push_back("PND50-00.syx");
#endif
    for (const std::string& n : files) {
        std::string err;
        const int got = d50::load_syx((dir + "/" + n).c_str(), err);
        if (got < 0) pd_error(nullptr, "d50~: %s: %s", n.c_str(), err.c_str());
        else post("d50~: %s: %d patches", n.c_str(), got);
    }
    const std::string user = dir + "/" + kUserFile;
    if (file_exists(user)) {
        std::string err;
        const int got = d50::load_user(user.c_str(), err);
        if (got < 0) pd_error(nullptr, "d50~: %s: %s", kUserFile, err.c_str());
        else post("d50~: %s: user bank, %d patches", kUserFile, got);
    }
}

void load_data(t_canvas* canvas) {
    if (g_data_tried) return;
    g_data_tried = true;
    std::vector<std::string> dirs;
    if (const char* h = class_gethelpdir(d50_class)) dirs.push_back(h);
    if (canvas) dirs.push_back(canvas_getdir(canvas)->s_name);
    for (const std::string& dir : dirs) {
        const std::string pcm = dir + "/d50_pcm.bin";
        if (!file_exists(pcm)) continue;
        std::string err;
        if (!d50::load_pcm(pcm.c_str(), err)) {
            pd_error(nullptr, "d50~: %s: %s", pcm.c_str(), err.c_str());
            return;
        }
        g_data_dir = dir;
        load_banks(dir);
        if (!d50::have_bank())
            post("d50~: no factory bank (.syx) beside d50_pcm.bin -- playing the eight built-in presets");
        return;
    }
    pd_error(nullptr, "d50~: d50_pcm.bin not found beside the external or the patch. "
                      "Create it from your own ROM images with tools/make_d50_data.py.");
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

void out_value(t_d50* x, const char* key, t_float v) {
    t_atom a;
    SETFLOAT(&a, v);
    outlet_anything(x->info, gensym(key), 1, &a);
}

void out_slot(t_d50* x, const char* key, int index, const char* name) {
    t_atom a[2];
    SETFLOAT(&a[0], index);
    SETSYMBOL(&a[1], gensym(name));
    outlet_anything(x->info, gensym(key), 2, a);
}

// SysEx leaves the way it arrives: one float per byte, so the third outlet
// goes through [route float] straight into [midiout] or another d50~.
void out_sysex(void* ctx, const uint8_t* msg, int len) {
    t_d50* x = static_cast<t_d50*>(ctx);
    for (int i = 0; i < len; ++i) outlet_float(x->info, msg[i]);
}

void out_info(t_d50* x, int i) {
    const d50::Control& c = d50::control(i);
    if (!x->synth->available(c)) return;
    const int v = x->synth->get_control(c);
    x->shown->value[i] = v;
    t_atom a[3];
    SETSYMBOL(&a[0], gensym(c.name));
    SETFLOAT(&a[1], d50::control_to127(c, v));
    SETSYMBOL(&a[2], gensym(x->synth->control_text(c, v).c_str()));
    outlet_anything(x->info, gensym("info"), 3, a);
}

void out_source(t_d50* x) {
    const int src = x->synth->source();
    if (src == d50::Synth::kFactory) out_slot(x, "preset", x->synth->patch(), d50::patch_name(x->synth->patch()));
    else if (src == d50::Synth::kUser) out_slot(x, "user", x->synth->patch(), d50::user_name(x->synth->patch()));
    else outlet_anything(x->info, gensym("init"), 0, nullptr);
}

void out_names(t_d50* x) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(x->shown->name.c_str()));
    outlet_anything(x->info, gensym("name"), 1, a);
    SETSYMBOL(&a[0], gensym(x->shown->upper.c_str()));
    SETSYMBOL(&a[1], gensym(x->shown->lower.c_str()));
    outlet_anything(x->info, gensym("tones"), 2, a);
}

// Everything that changed since outlet 3 last spoke: the patch that plays,
// its names, and every control -- in the logical time of the change, which is
// how the controller layer tells its own encoder from a change made elsewhere.
// `quiet` only takes note of the state (at creation, nothing is listening).
void report(t_d50* x, bool quiet = false) {
    d50::Synth& y = *x->synth;
    Shown& s = *x->shown;
    const int src = y.source(), idx = y.patch();
    const std::string src_name = src == d50::Synth::kFactory ? d50::patch_name(idx)
                               : src == d50::Synth::kUser ? d50::user_name(idx) : "";
    if (src != s.source || idx != s.index || src_name != s.source_name) {
        s.source = src; s.index = idx; s.source_name = src_name;
        if (!quiet) out_source(x);
    }
    const std::string name = y.name();
    const std::string upper = y.get_name(d50::kUpper), lower = y.get_name(d50::kLower);
    if (name != s.name || upper != s.upper || lower != s.lower) {
        s.name = name; s.upper = upper; s.lower = lower;
        if (!quiet) out_names(x);
    }
    for (int i = 0; i < d50::control_count(); ++i) {
        const d50::Control& c = d50::control(i);
        if (!y.available(c)) continue;
        const int v = y.get_control(c);
        if (v == s.value[i]) continue;
        if (quiet) s.value[i] = v;
        else out_info(x, i);
    }
}

bool need_bytes(t_d50* x, const char* what) {
    if (x->synth->editable()) return true;
    pd_error(x, "d50~: %s: a built-in preset has no patch data -- load a bank, or use user / init", what);
    return false;
}

// ---- messages ---------------------------------------------------------------

void out_param(t_d50* x, int section, const d50::Param& p) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(p.name));
    SETFLOAT(&a[1], x->synth->get_param(section, p));
    outlet_anything(x->info, gensym(d50::section_name(section)), 2, a);
}

void out_section_name(t_d50* x, int section) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym("name"));
    SETSYMBOL(&a[1], gensym(x->synth->get_name(section).c_str()));
    outlet_anything(x->info, gensym(d50::section_name(section)), 2, a);
}

// "<section> <name> [value]" and "<section> name <text ...>"
void d50_section(t_d50* x, int section, int argc, t_atom* argv) {
    const char* sec = d50::section_name(section);
    if (!need_bytes(x, sec)) return;
    if (argc < 1 || argv[0].a_type != A_SYMBOL) {
        pd_error(x, "d50~: %s: expected a parameter name", sec);
        return;
    }
    const char* name = atom_getsymbol(argv)->s_name;
    if (std::strcmp(name, "name") == 0 &&
        (section == d50::kUpper || section == d50::kLower || section == d50::kPatch)) {
        if (argc > 1) {
            std::string text;
            for (int i = 1; i < argc; ++i) {
                if (i > 1) text += ' ';
                if (argv[i].a_type == A_SYMBOL) {
                    text += atom_getsymbol(argv + i)->s_name;   // may hold spaces
                } else {
                    char buf[64];
                    atom_string(argv + i, buf, sizeof buf);
                    text += buf;
                }
            }
            x->synth->set_name(section, text.c_str());
        } else {
            out_section_name(x, section);
        }
        return;
    }
    const d50::Param* p = d50::find_param(section, name);
    if (!p) {
        pd_error(x, "d50~: %s: no parameter '%s'", sec, name);
        return;
    }
    if (argc > 1) x->synth->set_param(section, *p, static_cast<int>(atom_getfloat(argv + 1)));
    else out_param(x, section, *p);
}

void d50_sysex_bytes(t_d50* x, const std::vector<uint8_t>& msg) {
    if (!x->synth->sysex(msg.data(), static_cast<int>(msg.size()), out_sysex, x))
        pd_error(x, "d50~: sysex: not a D-50 message, or a wrong checksum");
}

void select_program(t_d50* x, int pgm) {
    d50::Synth& y = *x->synth;
    if (y.source() == d50::Synth::kUser) {
        y.select_user(pgm & 63);
    } else {
        const int base = y.source() == d50::Synth::kFactory ? y.patch() / 64 * 64 : 0;
        if (!y.select_patch(base + pgm)) y.select_patch(pgm);
    }
}

void d50_command(t_d50* x, t_symbol* s, int argc, t_atom* argv) {
    d50::Synth& y = *x->synth;
    if (!y.ok()) return;
    const char* c = s->s_name;
    const t_float f = argc > 0 ? atom_getfloat(argv) : 0;
    const int n = static_cast<int>(f);
    auto is = [c](const char* k) { return std::strcmp(c, k) == 0; };
    const int section = d50::section_from_name(c);

    if (const d50::Control* ctl = d50::find_control(c)) {
        const int i = static_cast<int>(ctl - &d50::control(0));
        if (!y.available(*ctl)) { need_bytes(x, c); return; }
        if (argc > 0) y.set_control(*ctl, n);
        else out_info(x, i);
    } else if (is("n")) {
        if (argc < 2 || argv[0].a_type != A_SYMBOL) return;
        const char* name = atom_getsymbol(argv)->s_name;
        const d50::Control* nc = d50::find_control(name);
        if (!nc) {
            if (std::strcmp(name, "none") != 0 && std::strcmp(name, "-") != 0)
                pd_error(x, "d50~: n: no control '%s'", name);
            return;
        }
        if (!y.available(*nc)) return;
        y.set_control(*nc, d50::control_from127(*nc, atom_getfloat(argv + 1)));
        // Always answered, also when the pot stays inside one value's zone.
        out_info(x, static_cast<int>(nc - &d50::control(0)));
    } else if (is("ninfo")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        if (const d50::Control* nc = d50::find_control(atom_getsymbol(argv)->s_name))
            out_info(x, static_cast<int>(nc - &d50::control(0)));
    } else if (section >= 0) {
        d50_section(x, section, argc, argv);
    } else if (is("param")) {
        if (!need_bytes(x, "param")) return;
        if (argc < 1 || n < 0 || n >= d50::kPatchBytes) {
            pd_error(x, "d50~: param: expected an address 0..%d", d50::kPatchBytes - 1);
        } else if (argc > 1) {
            const int v = static_cast<int>(atom_getfloat(argv + 1));
            const uint8_t b = static_cast<uint8_t>(v < 0 ? 0 : (v > 127 ? 127 : v));
            y.poke(n, &b, 1);
        } else {
            t_atom a[2];
            SETFLOAT(&a[0], n);
            SETFLOAT(&a[1], y.peek(n));
            outlet_anything(x->info, gensym("param"), 2, a);
        }
    } else if (is("dump")) {
        if (!need_bytes(x, "dump")) return;
        out_section_name(x, d50::kPatch);
        out_section_name(x, d50::kUpper);
        out_section_name(x, d50::kLower);
        for (int sec = 0; sec < d50::kSectionCount; ++sec) {
            int count = 0;
            const d50::Param* p = d50::section_params(sec, count);
            for (int i = 0; i < count; ++i) out_param(x, sec, p[i]);
        }
    } else if (is("preset")) {
        if (argc < 1 || !y.select_patch(n))
            pd_error(x, "d50~: preset: expected 0..%d", d50::patch_count() - 1);
        else x->shown->source = -2;      // always answered, also for the same one again
    } else if (is("user")) {
        if (argc < 1 || !y.select_user(n))
            pd_error(x, "d50~: user: expected 0..%d", d50::kUserSlots - 1);
        else x->shown->source = -2;
    } else if (is("init")) {
        y.select_init();
        x->shown->source = -2;
    } else if (is("reload")) {
        y.reload();
        x->shown->source = -2;
    } else if (is("store")) {
        if (!need_bytes(x, "store")) return;
        if (!y.store(argc > 0 ? n : -1))
            pd_error(x, "d50~: store: expected 0..%d", d50::kUserSlots - 1);
    } else if (is("userwrite")) {
        if (g_data_dir.empty()) return;
        std::string err;
        const std::string path = user_path(argc, argv);
        if (!d50::write_user(path.c_str(), err)) pd_error(x, "d50~: userwrite: %s", err.c_str());
        else post("d50~: user bank written to %s", path.c_str());
    } else if (is("userread")) {
        if (g_data_dir.empty()) return;
        std::string err;
        const std::string path = user_path(argc, argv);
        const int got = d50::load_user(path.c_str(), err);
        if (got < 0) pd_error(x, "d50~: userread: %s", err.c_str());
        else {
            post("d50~: user bank: %d patches from %s", got, path.c_str());
            if (y.source() == d50::Synth::kUser) y.reload();
        }
    } else if (is("usersend")) {
        y.dump_user(out_sysex, x, x->txchannel - 1);
    } else if (is("presets")) {
        const int count = d50::patch_count();
        for (int i = 0; i < count; ++i) out_slot(x, "preset", i, d50::patch_name(i));
        out_value(x, "presets", count);
    } else if (is("users")) {
        for (int i = 0; i < d50::kUserSlots; ++i) out_slot(x, "user", i, d50::user_name(i));
        out_value(x, "users", d50::kUserSlots);
    } else if (is("sysex")) {
        std::vector<uint8_t> msg;
        if (argc > 0 && atom_getfloat(argv) != 0xF0) msg.push_back(0xF0);
        for (int i = 0; i < argc; ++i)
            msg.push_back(static_cast<uint8_t>(static_cast<int>(atom_getfloat(argv + i)) & 0xFF));
        if (msg.empty() || msg.back() != 0xF7) msg.push_back(0xF7);
        d50_sysex_bytes(x, msg);
    } else if (is("sysexdump")) {
        if (need_bytes(x, "sysexdump")) y.dump_sysex(out_sysex, x, x->txchannel - 1);
    } else if (is("channel")) {
        x->channel = n < 0 ? 0 : (n > 16 ? 16 : n);
        out_value(x, "channel", x->channel);
    } else if (is("txchannel")) {
        x->txchannel = n < 1 ? 1 : (n > 16 ? 16 : n);
        out_value(x, "txchannel", x->txchannel);
    } else if (is("status")) {
        const d50::Synth::State st = y.state();
        out_source(x);
        out_names(x);
        out_value(x, "channel", x->channel);
        out_value(x, "txchannel", x->txchannel);
        out_value(x, "sustain", st.sustain);
        out_value(x, "governor", st.governor);
        out_value(x, "held", st.notes_held);
        out_value(x, "sounding", st.voices_sounding);
        out_value(x, "cpu", st.cpu_peak);
    }
    else if (is("bend")) y.set_bend(f);
    else if (is("mod")) y.set_mod(f);
    else if (is("aftertouch")) y.set_aftertouch(f);
    else if (is("sustain")) y.set_sustain(f != 0);
    else if (is("governor")) y.set_governor(f != 0);
    else if (is("stop")) y.all_notes_off();
    else if (is("reset")) y.silence();
    else { pd_error(x, "d50~: no method for '%s'", c); return; }
    report(x);
}

void d50_note(t_d50* x, t_float note, t_float velocity) {
    if (!x->synth->ok()) return;
    const int n = static_cast<int>(note);
    if (velocity > 0) x->synth->note_on(n, velocity / 127.0f);
    else x->synth->note_off(n);
}

void d50_float(t_d50* x, t_float f) { d50_note(x, f, x->velocity); }

void d50_list(t_d50* x, t_symbol*, int argc, t_atom* argv) {
    if (argc >= 3 && x->channel && static_cast<int>(atom_getfloat(argv + 2)) != x->channel) return;
    if (argc >= 2) d50_note(x, atom_getfloat(argv), atom_getfloat(argv + 1));
    else if (argc == 1) d50_float(x, atom_getfloat(argv));
}

void d50_proxy_anything(t_d50_proxy* p, t_symbol* s, int argc, t_atom* argv) {
    d50_command(p->owner, s, argc, argv);
}

// ---- raw MIDI ---------------------------------------------------------------

void midi_message(t_d50* x, int status, int d1, int d2) {
    d50::Synth& y = *x->synth;
    if (x->channel && (status & 0x0F) + 1 != x->channel) return;
    auto set = [&](const char* name, int pot) {
        if (const d50::Control* c = d50::find_control(name))
            if (y.available(*c)) y.set_control(*c, d50::control_from127(*c, static_cast<float>(pot)));
    };
    switch (status & 0xF0) {
        case 0x90:
            if (d2 > 0) y.note_on(d1, d2 / 127.0f);
            else y.note_off(d1);             // a note on with velocity 0
            return;
        case 0x80: y.note_off(d1); return;
        case 0xB0:
            switch (d1) {
                case 1: y.set_mod(d2 / 127.0f); return;
                case 5: set("portatime", d2); break;
                case 7: set("volume", d2); break;
                case 64: y.set_sustain(d2 >= 64); return;
                case 65: set("porta", d2); break;
                case 120: y.silence(); return;
                case 123: y.all_notes_off(); return;
                default: return;          // the CC pages belong to [d50cc]
            }
            break;
        case 0xC0: select_program(x, d1); break;
        case 0xD0: y.set_aftertouch(d1 / 127.0f); return;
        case 0xE0: y.set_bend(((d2 << 7 | d1) - 8192) / 8192.0f); return;
        default: return;
    }
    report(x);
}

void d50_proxy_float(t_d50_proxy* p, t_float f) {
    t_d50* x = p->owner;
    if (!x->synth->ok()) return;
    Midi& m = *x->midi;
    const int b = static_cast<int>(f) & 0xFF;
    if (b >= 0xF8) return;                                   // real time
    if (b == 0xF0) { m.in_sx = true; m.sx.assign(1, 0xF0); return; }
    if (m.in_sx) {
        if (b < 0x80) {
            if (m.sx.size() < 8192) m.sx.push_back(static_cast<uint8_t>(b));
            return;
        }
        m.in_sx = false;
        if (b == 0xF7) {
            m.sx.push_back(0xF7);
            // Other makers' messages pass by silently on a shared MIDI line.
            if (m.sx.size() >= 10 && m.sx[1] == 0x41 && m.sx[3] == 0x14) {
                d50_sysex_bytes(x, m.sx);
                report(x);
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

void d50_proxy_list(t_d50_proxy* p, t_symbol*, int argc, t_atom* argv) {
    for (int i = 0; i < argc; ++i) d50_proxy_float(p, atom_getfloat(argv + i));
}

// ---- dsp --------------------------------------------------------------------

t_int* d50_perform(t_int* w) {
    t_d50* x = reinterpret_cast<t_d50*>(w[1]);
    t_sample* l = reinterpret_cast<t_sample*>(w[2]);
    t_sample* r = reinterpret_cast<t_sample*>(w[3]);
    const int n = static_cast<int>(w[4]);
    x->synth->render(l, r, n);
    return w + 5;
}

void d50_dsp(t_d50* x, t_signal** sp) {
    x->synth->set_sample_rate(sp[0]->s_sr);
    dsp_add(d50_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, static_cast<t_int>(sp[0]->s_n));
}

// ---- lifetime ---------------------------------------------------------------

void* d50_new(t_symbol*, int argc, t_atom* argv) {
    t_d50* x = reinterpret_cast<t_d50*>(pd_new(d50_class));
    load_data(canvas_getcurrent());
    x->velocity = 100;
    x->channel = 0;
    x->txchannel = 1;
    x->midi = new Midi();
    x->shown = new Shown();
    x->shown->value.assign(static_cast<size_t>(d50::control_count()), INT32_MIN);
    x->synth = new d50::Synth();
    if (!x->synth->ok() && d50::pcm_loaded()) pd_error(x, "d50~: out of memory");

    floatinlet_new(&x->obj, &x->velocity);
    x->proxy.pd = d50_proxy_class;
    x->proxy.owner = x;
    inlet_new(&x->obj, &x->proxy.pd, nullptr, nullptr);

    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);
    x->info = outlet_new(&x->obj, nullptr);

    if (argc > 0 && argv[0].a_type == A_FLOAT)
        x->synth->select_patch(static_cast<int>(atom_getfloat(argv)));
    if (x->synth->ok()) report(x, true);
    return x;
}

void d50_free(t_d50* x) {
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
void d50_tilde_setup(void) {
    d50_class = class_new(gensym("d50~"), reinterpret_cast<t_newmethod>(d50_new),
                          reinterpret_cast<t_method>(d50_free), sizeof(t_d50),
                          CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(d50_class, reinterpret_cast<t_method>(d50_float));
    class_addlist(d50_class, reinterpret_cast<t_method>(d50_list));
    class_addanything(d50_class, reinterpret_cast<t_method>(d50_command));
    class_addmethod(d50_class, reinterpret_cast<t_method>(d50_dsp), gensym("dsp"), A_CANT, 0);

    d50_proxy_class = class_new(gensym("d50~ midi"), nullptr, nullptr,
                                sizeof(t_d50_proxy), CLASS_PD | CLASS_NOINLET, A_NULL);
    class_addanything(d50_proxy_class, reinterpret_cast<t_method>(d50_proxy_anything));
    class_addfloat(d50_proxy_class, reinterpret_cast<t_method>(d50_proxy_float));
    class_addlist(d50_proxy_class, reinterpret_cast<t_method>(d50_proxy_list));
}
}
