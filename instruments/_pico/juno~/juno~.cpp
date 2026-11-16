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
//
// Controls (the names of tools/juno_params.py, the same for the controller
// layer [junocc] and the editor [junoedit]):
//   <name> <value>      set a control, in the values the panel shows
//   <name>              report it
//   n <name> <0-127>    set it from a pot or a MIDI CC
//   ninfo <name>        report it as info
//
// Patches:
//   preset n            a factory preset: 0..55 the Juno-60 sounds, then the
//                       Juno-106 banks (.syx) beside the external
//   user n              a slot of the user bank, 0..55
//   init / reload       the init patch / the same patch again, edits dropped
//   store [n]           the playing patch into user slot n (in memory)
//   userwrite [file]    write the user bank (default: junouser-bank.syx)
//   userread [file]     read it
//   usersend            the user bank as Juno-106 SysEx bytes
//   presets / users     list the names
//
//   param addr [v]      one of the 18 patch bytes
//   dump                every control as info
//   sysex b b b ...     one Juno-106 SysEx message as a list of bytes
//   sysexdump           the playing patch as SysEx bytes
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

void out_source(t_juno* x) {
    const int src = x->synth->source();
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

    if (const juno::Control* ctl = juno::find_control(c)) {
        if (argc > 0) y.set_control(*ctl, n);
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
        // Always answered, also when the pot stays inside one value's zone.
        if (y.get_control(*nc) == before) out_info(x, i);
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
    } else if (is("store")) {
        if (!y.store(argc > 0 ? n : -1))
            pd_error(x, "juno~: store: expected 0..%d", juno::kUserSlots - 1);
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
        out_value(x, "sustain", y.sustain());
        out_value(x, "sounding", y.voices_sounding());
    }
    else if (is("bend")) y.set_bend(f);
    else if (is("mod")) y.set_mod(f);
    else if (is("sustain")) y.set_sustain(f != 0);
    else if (is("stop")) y.all_notes_off();
    else if (is("reset")) y.silence();
    else { pd_error(x, "juno~: no method for '%s'", c); return; }
    report(x, false, echo);
}

void juno_note(t_juno* x, t_float note, t_float velocity) {
    const int n = static_cast<int>(note);
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
    if (b >= 0xF8) return;                                   // real time
    if (b == 0xF0) { m.in_sx = true; m.sx.assign(1, 0xF0); return; }
    if (m.in_sx) {
        if (b < 0x80) {
            if (m.sx.size() < 64) m.sx.push_back(static_cast<uint8_t>(b));
            return;
        }
        m.in_sx = false;
        if (b == 0xF7) {
            m.sx.push_back(0xF7);
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

    if (argc > 0 && argv[0].a_type == A_FLOAT)
        x->synth->select_patch(static_cast<int>(atom_getfloat(argv)));
    report(x, true);
    return x;
}

void juno_free(t_juno* x) {
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
