// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Peter
//
// d50~ -- Roland D-50 for Pure Data, on the PicoFaceD5 LA engine.
//
//   inlet 1   float: note (velocity from inlet 2; velocity 0 = note off)
//             list:  note velocity
//             any:   a command, see below
//   inlet 2   float: velocity for the next note, 0..127 (default 100)
//   inlet 3   any:   a command (the same set as inlet 1)
//             float: one byte of a MIDI SysEx stream, as [sysexin] gives it
//             list:  several such bytes
//   outlet 1  signal, left
//   outlet 2  signal, right
//   outlet 3  messages: preset names and state, for a GUI or a headless host
//             floats:   SysEx bytes, answers to requests and "sysexdump"
//
// Commands:
//   preset n          select a patch (0-based over all loaded banks)
//   presets           list every patch as "preset <n> <name>", then "presets <count>"
//   info              report the current state, one message per value
//   bend f            pitch wheel, -1..1          bendrange n   0..12 semitones
//   mod f             modulation lever, 0..1      aftertouch f  0..1
//   sustain 0/1       hold pedal                  porta 0/1, portatime 0..100
//   volume n          0..100                      tune n        -50..50 cents
//   voices n          polyphony per tone, 1..8    balance n     tone balance 0..100
//   reverb n          balance 0..100              reverbtype n  1..32
//   chorus n          balance 0..100              chorustype n  1..8
//   chorusrate n, chorusdepth n                   0..100
//   eqlowfreq n (0..15), eqlowgain n (-12..12), eqhighfreq n (0..21),
//   eqhighq n (0..8), eqhighgain n (-12..12)
//   governor 0/1      drop released tails when the CPU runs out
//   stop              all notes off               reset   notes and effect tails
//
// Patch editing (needs a loaded bank; every edit sounds at once):
//   <section> <name> v   set a parameter by name, in panel values. Sections:
//                        u1 u2 (upper partials), upper (upper common),
//                        l1 l2, lower, patch. Without v: report the value.
//   <section> name text  name of the patch or of a tone (upper, lower)
//   param addr v         set one of the 448 patch bytes, stored value
//   param addr           report it
//   dump                 report every named parameter
//   store [n]            copy the playing patch to slot n (default: its own)
//   sysex b b b ...      one D-50 SysEx message as a list of bytes
//   sysexdump            send the playing patch as SysEx bytes
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

struct t_d50;

struct t_d50_proxy {
    t_pd pd;
    t_d50* owner;
};

struct t_d50 {
    t_object obj;
    d50::Synth* synth;
    t_float velocity;
    t_d50_proxy proxy;
    t_outlet* info;
    std::vector<uint8_t>* sx;   // SysEx bytes arriving one at a time
};

// ---- data files -------------------------------------------------------------
// d50_pcm.bin and any *.syx sit beside the external (or beside the patch).
// Loaded once, by the first object created.

bool g_data_tried = false;

bool file_exists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void load_banks(const std::string& dir) {
#ifndef _WIN32
    std::vector<std::string> files;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n.size() < 5) continue;
            std::string ext = n.substr(n.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".syx") files.push_back(n);
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());
    // The factory card leads, so patch 0 never moves when banks join.
    auto fac = std::find(files.begin(), files.end(), "PND50-00.syx");
    if (fac != files.end()) std::rotate(files.begin(), fac, fac + 1);
    for (const std::string& n : files) {
        std::string err;
        const int got = d50::load_syx((dir + "/" + n).c_str(), err);
        if (got < 0) pd_error(nullptr, "d50~: %s: %s", n.c_str(), err.c_str());
        else post("d50~: %s: %d patches", n.c_str(), got);
    }
#else
    std::string err;
    const int got = d50::load_syx((dir + "/PND50-00.syx").c_str(), err);
    if (got > 0) post("d50~: PND50-00.syx: %d patches", got);
#endif
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
        load_banks(dir);
        if (!d50::have_bank())
            post("d50~: no .syx bank beside d50_pcm.bin -- playing the eight built-in presets");
        return;
    }
    pd_error(nullptr, "d50~: d50_pcm.bin not found beside the external or the patch. "
                      "Create it from your own ROM images with tools/make_d50_data.py.");
}

// ---- output -----------------------------------------------------------------

void out_value(t_d50* x, const char* key, t_float v) {
    t_atom a;
    SETFLOAT(&a, v);
    outlet_anything(x->info, gensym(key), 1, &a);
}

void out_preset(t_d50* x, int index) {
    t_atom a[2];
    SETFLOAT(&a[0], index);
    SETSYMBOL(&a[1], gensym(d50::patch_name(index)));
    outlet_anything(x->info, gensym("preset"), 2, a);
}

// SysEx leaves the way it arrives: one float per byte, so the third outlet
// goes through [route float] straight into [midiout] or another d50~.
void out_sysex(void* ctx, const uint8_t* msg, int len) {
    t_d50* x = static_cast<t_d50*>(ctx);
    for (int i = 0; i < len; ++i) outlet_float(x->info, msg[i]);
}

void out_param(t_d50* x, int section, const d50::Param& p) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(p.name));
    SETFLOAT(&a[1], x->synth->get_param(section, p));
    outlet_anything(x->info, gensym(d50::section_name(section)), 2, a);
}

void out_name(t_d50* x, int section) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym("name"));
    SETSYMBOL(&a[1], gensym(x->synth->get_name(section).c_str()));
    outlet_anything(x->info, gensym(d50::section_name(section)), 2, a);
}

bool need_bank(t_d50* x, const char* what) {
    if (x->synth->editable()) return true;
    pd_error(x, "d50~: %s: needs a loaded patch bank (.syx)", what);
    return false;
}

// "<section> <name> [value]" and "<section> name <text ...>"
void d50_section(t_d50* x, int section, int argc, t_atom* argv) {
    const char* sec = d50::section_name(section);
    if (!need_bank(x, sec)) return;
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
                char buf[64];
                atom_string(argv + i, buf, sizeof buf);
                if (i > 1) text += ' ';
                text += buf;
            }
            x->synth->set_name(section, text.c_str());
        } else {
            out_name(x, section);
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
    if (!need_bank(x, "sysex")) return;
    if (!x->synth->sysex(msg.data(), static_cast<int>(msg.size()), out_sysex, x))
        pd_error(x, "d50~: sysex: not a D-50 message, or a wrong checksum");
}

// ---- commands ---------------------------------------------------------------

void d50_command(t_d50* x, t_symbol* s, int argc, t_atom* argv) {
    d50::Synth& y = *x->synth;
    const char* c = s->s_name;
    const t_float f = argc > 0 ? atom_getfloat(argv) : 0;
    const int n = static_cast<int>(f);
    auto is = [c](const char* k) { return std::strcmp(c, k) == 0; };

    const int section = d50::section_from_name(c);
    if (section >= 0) {
        d50_section(x, section, argc, argv);
    } else if (is("param")) {
        if (!need_bank(x, "param")) return;
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
        if (!need_bank(x, "dump")) return;
        out_name(x, d50::kPatch);
        out_name(x, d50::kUpper);
        out_name(x, d50::kLower);
        for (int sec = 0; sec < d50::kSectionCount; ++sec) {
            int count = 0;
            const d50::Param* p = d50::section_params(sec, count);
            for (int i = 0; i < count; ++i) out_param(x, sec, p[i]);
        }
    } else if (is("store")) {
        if (!need_bank(x, "store")) return;
        const int slot = argc > 0 ? n : -1;
        if (!y.store(slot)) pd_error(x, "d50~: store: expected 0..%d", d50::patch_count() - 1);
        else out_preset(x, slot < 0 ? y.patch() : slot);
    } else if (is("sysex")) {
        std::vector<uint8_t> msg;
        if (argc > 0 && atom_getfloat(argv) != 0xF0) msg.push_back(0xF0);
        for (int i = 0; i < argc; ++i)
            msg.push_back(static_cast<uint8_t>(static_cast<int>(atom_getfloat(argv + i)) & 0xFF));
        if (msg.empty() || msg.back() != 0xF7) msg.push_back(0xF7);
        d50_sysex_bytes(x, msg);
    } else if (is("sysexdump")) {
        if (need_bank(x, "sysexdump")) y.dump_sysex(out_sysex, x);
    } else if (is("preset")) {
        if (argc < 1 || !y.select_patch(n))
            pd_error(x, "d50~: preset: expected 0..%d", d50::patch_count() - 1);
        else out_preset(x, n);
    } else if (is("presets")) {
        const int count = d50::patch_count();
        for (int i = 0; i < count; ++i) out_preset(x, i);
        out_value(x, "presets", count);
    } else if (is("info")) {
        const d50::Synth::State st = y.state();
        out_preset(x, st.patch);
        t_atom a;
        SETSYMBOL(&a, gensym(y.structure_name()));
        outlet_anything(x->info, gensym("structure"), 1, &a);
        out_value(x, "volume", st.volume);
        out_value(x, "tune", st.tune);
        out_value(x, "bendrange", st.bend_range);
        out_value(x, "voices", st.voice_limit);
        out_value(x, "balance", st.tone_balance);
        out_value(x, "porta", st.porta);
        out_value(x, "portatime", st.porta_time);
        out_value(x, "sustain", st.sustain);
        out_value(x, "reverb", st.reverb);
        out_value(x, "reverbtype", st.reverb_type);
        out_value(x, "chorus", st.chorus);
        out_value(x, "chorustype", st.chorus_type);
        out_value(x, "chorusrate", st.chorus_rate);
        out_value(x, "chorusdepth", st.chorus_depth);
        out_value(x, "eqlowfreq", st.eq_low_freq);
        out_value(x, "eqlowgain", st.eq_low_gain);
        out_value(x, "eqhighfreq", st.eq_high_freq);
        out_value(x, "eqhighq", st.eq_high_q);
        out_value(x, "eqhighgain", st.eq_high_gain);
        out_value(x, "governor", st.governor);
        out_value(x, "held", st.notes_held);
        out_value(x, "sounding", st.voices_sounding);
        out_value(x, "cpu", st.cpu_peak);
    }
    else if (is("bend")) y.set_bend(f);
    else if (is("bendrange")) y.set_bend_range(n);
    else if (is("mod")) y.set_mod(f);
    else if (is("aftertouch")) y.set_aftertouch(f);
    else if (is("sustain")) y.set_sustain(f != 0);
    else if (is("porta")) y.set_porta(f != 0);
    else if (is("portatime")) y.set_porta_time(n);
    else if (is("volume")) y.set_volume(n);
    else if (is("tune")) y.set_tune(n);
    else if (is("voices")) y.set_voice_limit(n);
    else if (is("balance")) y.set_tone_balance(n);
    else if (is("reverb")) y.set_reverb(n);
    else if (is("reverbtype")) y.set_reverb_type(n);
    else if (is("chorus")) y.set_chorus(n);
    else if (is("chorustype")) y.set_chorus_type(n);
    else if (is("chorusrate")) y.set_chorus_rate(n);
    else if (is("chorusdepth")) y.set_chorus_depth(n);
    else if (is("eqlowfreq")) y.set_eq_low_freq(n);
    else if (is("eqlowgain")) y.set_eq_low_gain(n);
    else if (is("eqhighfreq")) y.set_eq_high_freq(n);
    else if (is("eqhighq")) y.set_eq_high_q(n);
    else if (is("eqhighgain")) y.set_eq_high_gain(n);
    else if (is("governor")) y.set_governor(f != 0);
    else if (is("stop")) y.all_notes_off();
    else if (is("reset")) y.silence();
    else pd_error(x, "d50~: no method for '%s'", c);
}

void d50_note(t_d50* x, t_float note, t_float velocity) {
    const int n = static_cast<int>(note);
    if (velocity > 0) x->synth->note_on(n, velocity / 127.0f);
    else x->synth->note_off(n);
}

void d50_float(t_d50* x, t_float f) { d50_note(x, f, x->velocity); }

void d50_list(t_d50* x, t_symbol*, int argc, t_atom* argv) {
    if (argc >= 2) d50_note(x, atom_getfloat(argv), atom_getfloat(argv + 1));
    else if (argc == 1) d50_float(x, atom_getfloat(argv));
}

void d50_proxy_anything(t_d50_proxy* p, t_symbol* s, int argc, t_atom* argv) {
    d50_command(p->owner, s, argc, argv);
}

// A MIDI SysEx stream, byte by byte: collected from F0 to F7.
void d50_proxy_float(t_d50_proxy* p, t_float f) {
    t_d50* x = p->owner;
    const int b = static_cast<int>(f) & 0xFF;
    std::vector<uint8_t>& sx = *x->sx;
    if (b == 0xF0) sx.clear();
    else if (sx.empty()) return;              // not inside a message
    if (b >= 0x80 && b != 0xF0 && b != 0xF7) { sx.clear(); return; }
    sx.push_back(static_cast<uint8_t>(b));
    if (b == 0xF7) {
        // Other makers' messages pass by silently on a shared MIDI line.
        if (sx.size() >= 10 && sx[1] == 0x41 && sx[3] == 0x14) d50_sysex_bytes(x, sx);
        sx.clear();
    } else if (sx.size() > 4096) {
        sx.clear();
    }
}

// The same bytes as one list.
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
    x->sx = new std::vector<uint8_t>();
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
    return x;
}

void d50_free(t_d50* x) {
    delete x->synth;
    delete x->sx;
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

    d50_proxy_class = class_new(gensym("d50~ commands"), nullptr, nullptr,
                                sizeof(t_d50_proxy), CLASS_PD | CLASS_NOINLET, A_NULL);
    class_addanything(d50_proxy_class, reinterpret_cast<t_method>(d50_proxy_anything));
    class_addfloat(d50_proxy_class, reinterpret_cast<t_method>(d50_proxy_float));
    class_addlist(d50_proxy_class, reinterpret_cast<t_method>(d50_proxy_list));
}
}
