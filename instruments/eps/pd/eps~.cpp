// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Peter
//
// eps~ -- Ensoniq EPS / EPS-16 PLUS / ASR-10 instruments for Pure Data: the
// instruments are read from their own disk images and files (EFE, IMG, EDE,
// EDA, GKH) and played on a model of the ES5505 voice chip.
//
//   inlet 1   float: note (velocity from inlet 2; velocity 0 = note off)
//             list:  note velocity [channel]
//             any:   a message, see below
//   inlet 2   float: velocity for the next note, 0..127 (default 100)
//   inlet 3   float or list: raw MIDI bytes, as [midiin] gives them: notes,
//             bend, program change, channel and poly pressure, CC 1 (wheel),
//             2 (XCTRL), 4 (pedal), 7 (volume), 64 (sustain)
//             any:   a message (the same set as inlet 1)
//   outlet 1  signal, left
//   outlet 2  signal, right
//   outlet 3  messages: info <name> <0-127> <text> for every change, the
//             instrument that plays, library and user names, channel ...
//             display <title> <value>: two lines for a display
//             floats: MIDI bytes (thru) for [midiout]
//
// Instruments ("presets"): every instrument in every file of the library
// folder, in the order of the file names and of the instruments in a disk image.
// The library is the folder eps/ beside the patch (eps.i~ sends library <dir>
// at load), else eps/ beside the external.
//   library <dir>       use this folder, read its list
//   rescan              read the list again
//   preset n            load library instrument n (0-based), offsets to default
//   presetstep [n]      the next (n = -1: the previous) library instrument
//   open <file> [k]     load instrument k (or the first) of any file
//   presets             list the library
// User bank (64 slots, text, epsuser-bank.txt): a slot keeps the file, the
// instrument and the offsets.
//   user n, userstep [n], store [n], save, savearm, saveslot n, savecancel,
//   userwrite [file], userread [file], users, init, reload
// Controls (tools/eps_params.py): <name> <value>, <name>, n <name> <0-127>,
//   ninfo <name>, cycle <name> [n], dump
// Playing: bend f (-1..1), mod f (wheel 0..1), pressure f, polypressure note f,
//   pedal f, xctrl f (all 0..1, the EPS mod sources), sustain 0/1, stop, reset,
//   channel n (0 = all), txchannel n, thru 0/1, status
//
// Loading runs in a thread of its own; the finished instrument is swapped in on
// Pd's scheduler (a clock), so the audio does not stop while a file is read.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <dirent.h>
#endif

extern "C" {
#include "m_pd.h"
}

#include "eps_sampler.h"

namespace {

struct Control {
    const char* name;
    char kind;
    int lo, hi, def;
    const char* label;
    const char* names;
};
#include "eps_params.h"

t_class* eps_class = nullptr;
t_class* eps_proxy_class = nullptr;

const char kUserFile[] = "epsuser-bank.txt";
constexpr float kOutputGain = 0.4f;
constexpr int kUserSlots = 64;

// ---- controls ----------------------------------------------------------------

const Control* find_control(const char* n) {
    for (const Control& c : kControls)
        if (!std::strcmp(c.name, n)) return &c;
    return nullptr;
}
int control_index(const Control* c) { return static_cast<int>(c - kControls); }

// The same mapping as juno~: up to 16 values in zones of equal width, else linear.
int to127(const Control& c, int v) {
    const int n = c.hi - c.lo + 1;
    v = std::clamp(v, c.lo, c.hi);
    if (n <= 16) return std::min(((v - c.lo) * 2 + 1) * 64 / n, 127);
    return static_cast<int>(std::lround(double(v - c.lo) * 127.0 / double(c.hi - c.lo)));
}
int from127(const Control& c, double x) {
    x = std::clamp(x, 0.0, 127.0);
    const int n = c.hi - c.lo + 1;
    if (n <= 16) return c.lo + std::min(static_cast<int>(x * n / 128.0), n - 1);
    return c.lo + static_cast<int>(std::lround(x * (c.hi - c.lo) / 127.0));
}

std::string value_text(const Control& c, int v) {
    if (c.names[0]) {
        const char* p = c.names;
        for (int i = c.lo; i < v && p; ++i) {
            p = std::strchr(p, '|');
            if (p) ++p;
        }
        if (p) {
            const char* e = std::strchr(p, '|');
            return std::string(p, e ? static_cast<size_t>(e - p) : std::strlen(p));
        }
    }
    char b[32];
    if (!std::strcmp(c.name, "vibrate")) {
        std::snprintf(b, sizeof b, "%.1fHz", 0.1 * std::pow(120.0, v / 127.0));
    } else if (!std::strcmp(c.name, "tune")) {
        std::snprintf(b, sizeof b, "%+dct", v);
    } else if (c.lo < 0) {
        std::snprintf(b, sizeof b, "%+d", v);
    } else {
        std::snprintf(b, sizeof b, "%d", v);
    }
    return b;
}

// ---- library and user bank (one per Pd, as the user banks of the others) -------

struct LibEntry {
    std::string file;     // path as given (relative to the library or absolute)
    int index = 0;        // which instrument of the file (0-based, in directory order)
    std::string name;     // shown name
};

struct Slot {
    bool used = false;
    LibEntry entry;
    int value[kCtlCount] = {};
};

std::string g_ext_dir;                 // the external's folder
std::string g_lib_dir;                 // the library folder in use
std::vector<LibEntry> g_lib;
Slot g_user[kUserSlots];
bool g_data_tried = false;

std::string join(const std::string& dir, const std::string& f) {
    if (f.empty() || f[0] == '/' || (f.size() > 1 && f[1] == ':')) return f;
    return dir + "/" + f;
}

std::string lower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

bool is_image_name(const std::string& n) {
    const std::string l = lower(n);
    for (const char* e : {".efe", ".efa", ".img", ".ede", ".eda", ".gkh", ".hda", ".iso"})
        if (l.size() > std::strlen(e) && l.compare(l.size() - std::strlen(e), std::string::npos, e) == 0)
            return true;
    return false;
}

std::string spaces_to_underscore(std::string s) {
    while (!s.empty() && s.back() == ' ') s.pop_back();
    for (char& ch : s) if (ch == ' ') ch = '_';
    return s;
}

int scan_library(const std::string& dir) {
    g_lib.clear();
    g_lib_dir = dir;
    std::vector<std::string> files;
#ifndef _WIN32
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n[0] != '.' && is_image_name(n)) files.push_back(n);
        }
        closedir(d);
    }
#endif
    std::sort(files.begin(), files.end());
    for (const std::string& f : files) {
        eps::Disk disk;
        std::string err;
        if (!disk.open(join(dir, f), err)) {
            post("eps~: %s: %s", f.c_str(), err.c_str());
            continue;
        }
        const auto list = disk.instruments();
        const bool single = disk.format() == eps::ImageFormat::Efe;
        for (size_t k = 0; k < list.size(); ++k) {
            LibEntry le;
            le.file = f;
            le.index = static_cast<int>(k);
            le.name = spaces_to_underscore(single ? list[k]->name : list[k]->name);
            g_lib.push_back(le);
        }
    }
    return static_cast<int>(g_lib.size());
}

bool write_user(const std::string& path, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) { err = "cannot write " + path; return false; }
    std::fprintf(f, "# eps~ user bank: slot, file, instrument number in the file, name, then the "
                    "controls as name=value\n");
    for (int s = 0; s < kUserSlots; ++s) {
        const Slot& u = g_user[s];
        if (!u.used) continue;
        std::fprintf(f, "%d\t%s\t%d\t%s", s, u.entry.file.c_str(), u.entry.index, u.entry.name.c_str());
        for (int i = 0; i < kCtlCount; ++i)
            if (kControls[i].kind == 'O') std::fprintf(f, "\t%s=%d", kControls[i].name, u.value[i]);
        std::fprintf(f, "\n");
    }
    const bool ok = !std::ferror(f);
    std::fclose(f);
    if (!ok) err = "write error on " + path;
    return ok;
}

int read_user(const std::string& path, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) { err = "cannot read " + path; return -1; }
    for (Slot& u : g_user) u = Slot();
    char line[2048];
    int got = 0;
    while (std::fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        std::vector<std::string> fields;
        std::string cur;
        for (char* p = line; *p && *p != '\n' && *p != '\r'; ++p) {
            if (*p == '\t') { fields.push_back(cur); cur.clear(); }
            else cur += *p;
        }
        fields.push_back(cur);
        if (fields.size() < 4) continue;
        const int s = std::atoi(fields[0].c_str());
        if (s < 0 || s >= kUserSlots) continue;
        Slot& u = g_user[s];
        u.used = true;
        u.entry.file = fields[1];
        u.entry.index = std::atoi(fields[2].c_str());
        u.entry.name = fields[3];
        for (int i = 0; i < kCtlCount; ++i) u.value[i] = kControls[i].def;
        for (size_t k = 4; k < fields.size(); ++k) {
            const size_t eq = fields[k].find('=');
            if (eq == std::string::npos) continue;
            const Control* c = find_control(fields[k].substr(0, eq).c_str());
            if (c && c->kind == 'O')
                u.value[control_index(c)] = std::clamp(std::atoi(fields[k].c_str() + eq + 1), c->lo, c->hi);
        }
        ++got;
    }
    std::fclose(f);
    return got;
}

void load_data() {
    if (g_data_tried) return;
    g_data_tried = true;
    const char* h = class_gethelpdir(eps_class);
    g_ext_dir = h ? h : ".";
    const int n = scan_library(g_ext_dir + "/eps");
    if (n) post("eps~: library %s/eps: %d instruments", g_ext_dir.c_str(), n);
    std::string err;
    const std::string user = g_ext_dir + "/" + kUserFile;
    if (FILE* f = std::fopen(user.c_str(), "r")) {
        std::fclose(f);
        const int got = read_user(user, err);
        if (got >= 0) post("eps~: %s: %d user slots", kUserFile, got);
    }
}

std::string user_path(int argc, t_atom* argv) {
    if (argc > 0 && argv[0].a_type == A_SYMBOL) return join(g_ext_dir, atom_getsymbol(argv)->s_name);
    return g_ext_dir + "/" + kUserFile;
}

// ---- the loader thread ------------------------------------------------------------
// The scheduler puts a request (state 1), the thread reads the file and builds a
// complete Sampler (state 3), the scheduler's clock swaps it in and hands the old
// one back for deletion. Nothing in the scheduler thread waits or allocates large.

struct Loader {
    std::mutex m;                       // guards the request / result fields (short holds only)
    std::string path, label;
    int index = 0;
    float sr = 44100.f;
    int voices = 20;
    std::atomic<int> state{0};          // 0 idle, 1 requested, 2 loading, 3 done
    std::unique_ptr<eps::Sampler> result;
    std::string error, name;
    std::vector<std::unique_ptr<eps::Sampler>> trash;
    std::atomic<bool> quit{false};
    std::thread thread;

    void run() {
        while (!quit.load()) {
            {
                std::vector<std::unique_ptr<eps::Sampler>> old;
                {
                    std::lock_guard<std::mutex> g(m);
                    old.swap(trash);
                }
            }                           // the old samplers are freed here, outside the lock
            if (state.load() != 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(3));
                continue;
            }
            state.store(2);
            std::string p, err, nm;
            int k;
            eps::Sampler::Settings set;
            {
                std::lock_guard<std::mutex> g(m);
                p = path;
                k = index;
                set.sampleRate = sr;
                set.maxVoices = voices;
                // Several layers per key add up (CLASSIC PAD: a chord of three
                // reaches full scale at 1.0). volume 100 = this level.
                set.masterGain = kOutputGain;
            }
            std::unique_ptr<eps::Sampler> s;
            eps::Disk disk;
            if (disk.open(p, err)) {
                const auto list = disk.instruments();
                if (k < 0 || k >= static_cast<int>(list.size())) {
                    err = list.empty() ? "no instrument in the file" : "no instrument " + std::to_string(k);
                } else {
                    eps::Instrument inst;
                    if (disk.loadInstrument(*list[k], inst, err)) {
                        s.reset(new eps::Sampler(set));
                        if (!s->load(inst, err)) s.reset();
                        else nm = inst.name;
                    }
                }
            }
            {
                std::lock_guard<std::mutex> g(m);
                result = std::move(s);
                error = err;
                name = nm;
            }
            state.store(3);
        }
    }
};

// ---- the object -----------------------------------------------------------------

struct t_eps;

struct t_eps_proxy {
    t_pd pd;
    t_eps* owner;
};

struct Midi {
    int status = 0;
    int data[2] = {0, 0};
    int have = 0;
    bool in_sx = false;
};

enum Source { kNone = -1, kInit = 0, kLibrary = 1, kUser = 2, kOpen = 3 };

struct t_eps {
    t_object obj;
    t_float velocity;
    t_eps_proxy proxy;
    t_outlet* info;
    t_clock* poll;
    eps::Sampler* sampler;               // the playing instrument (null = none yet)
    Loader* loader;
    Midi* midi;
    int value[kCtlCount];                // the controls
    int shown[kCtlCount];                // what outlet 3 said last
    float sr;
    int channel, txchannel, thru;
    int source, index, last_user, store_to;
    LibEntry* entry;                     // what plays (or loads)
    std::string* name;
    float wheel, bend;
    float ctl[3];                        // pressure, pedal, xctrl
    bool sustain;
    bool pending;                        // a request waits for the loader to be free
    LibEntry* pending_entry;
};

void out_value(t_eps* x, const char* key, t_float v) {
    t_atom a;
    SETFLOAT(&a, v);
    outlet_anything(x->info, gensym(key), 1, &a);
}

void out_slot(t_eps* x, const char* key, int index, const std::string& name) {
    t_atom a[2];
    SETFLOAT(&a[0], index);
    SETSYMBOL(&a[1], gensym(name.empty() ? "-" : name.c_str()));
    outlet_anything(x->info, gensym(key), 2, a);
}

void out_display(t_eps* x, const std::string& title, const std::string& value) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(title.c_str()));
    SETSYMBOL(&a[1], gensym(value.empty() ? "-" : value.c_str()));
    outlet_anything(x->info, gensym("display"), 2, a);
}

void out_info(t_eps* x, int i) {
    const Control& c = kControls[i];
    x->shown[i] = x->value[i];
    const std::string t = value_text(c, x->value[i]);   // one word, no spaces
    t_atom a[3];
    SETSYMBOL(&a[0], gensym(c.name));
    SETFLOAT(&a[1], to127(c, x->value[i]));
    SETSYMBOL(&a[2], gensym(t.c_str()));
    outlet_anything(x->info, gensym("info"), 3, a);
}

// The controls -> the sampler.
void apply(t_eps* x) {
    eps::Sampler* s = x->sampler;
    if (!s) return;
    const int* v = x->value;
    eps::Sampler::Offsets o;
    o.cutoff = v[kC_cut];
    o.cutoff2 = v[kC_cut2];
    o.attack = v[kC_att];
    o.release = v[kC_rel];
    o.decay = v[kC_dec];
    o.filterEnv = v[kC_fenv];
    o.wave = v[kC_wave];
    o.transpose = v[kC_transpose];
    o.tune = v[kC_tune];
    s->setOffsets(o);
    s->setVibrato(v[kC_vib] / 127.f, 0.1f * std::pow(120.f, v[kC_vibrate] / 127.f));
    s->setBendRange(v[kC_bend]);
    s->setVolume(v[kC_volume] / 100.f);
    if (s->voices() != v[kC_voices]) s->setVoices(v[kC_voices]);
    s->setPatch(v[kC_patch]);
    s->setModWheel(x->wheel);
    s->setPressure(x->ctl[0]);
    s->setPedal(x->ctl[1]);
    s->setXctrl(x->ctl[2]);
    s->setBend(x->bend);
    s->setSustain(x->sustain);
}

void report(t_eps* x) {
    for (int i = 0; i < kCtlCount; ++i)
        if (x->value[i] != x->shown[i]) out_info(x, i);
}

void out_source(t_eps* x) {
    const std::string& nm = *x->name;
    if (x->source == kLibrary) {
        out_display(x, "Preset " + std::to_string(x->index), nm);
        out_slot(x, "preset", x->index, nm);
    } else if (x->source == kUser) {
        out_display(x, "User " + std::to_string(x->index), nm);
        out_slot(x, "user", x->index, nm);
    } else if (x->source == kOpen) {
        out_display(x, "File", nm);
    } else if (x->source == kInit) {
        out_display(x, "Init", nm);
        outlet_anything(x->info, gensym("init"), 0, nullptr);
    }
    t_atom a;
    SETSYMBOL(&a, gensym(nm.empty() ? "-" : nm.c_str()));
    outlet_anything(x->info, gensym("name"), 1, &a);
}

void eps_poll(t_eps* x);

// Ask the loader for an instrument; if it is busy, the newest request waits.
void request(t_eps* x, const LibEntry& e) {
    Loader& L = *x->loader;
    *x->entry = e;
    if (L.state.load() != 0) {
        *x->pending_entry = e;
        x->pending = true;
        clock_delay(x->poll, 5);
        return;
    }
    {
        std::lock_guard<std::mutex> g(L.m);
        L.path = join(g_lib_dir.empty() ? g_ext_dir : g_lib_dir, e.file);
        L.index = e.index;
        L.sr = x->sr;
        L.voices = x->value[kC_voices];
    }
    L.state.store(1);
    out_display(x, "Loading", e.name);
    clock_delay(x->poll, 5);
}

void eps_poll(t_eps* x) {
    Loader& L = *x->loader;
    if (L.state.load() == 3) {
        std::unique_ptr<eps::Sampler> s;
        std::string err, nm;
        {
            std::lock_guard<std::mutex> g(L.m);
            s = std::move(L.result);
            err = L.error;
            nm = L.name;
        }
        L.state.store(0);
        if (!s) {
            pd_error(x, "eps~: %s: %s", x->entry->file.c_str(), err.c_str());
            out_display(x, "Load failed", x->entry->name);
        } else {
            eps::Sampler* old = x->sampler;
            x->sampler = s.release();
            if (x->sampler->sampleRate() != x->sr) x->sampler->setSampleRate(x->sr);
            apply(x);
            if (old) {
                std::lock_guard<std::mutex> g(L.m);
                L.trash.emplace_back(old);
            }
            *x->name = spaces_to_underscore(nm);
            out_source(x);
            const eps::Instrument& in = x->sampler->instrument();
            t_atom a[3];
            SETSYMBOL(&a[0], gensym(in.modelName()));
            SETFLOAT(&a[1], static_cast<t_float>(in.layers.size()));
            SETFLOAT(&a[2], static_cast<t_float>(in.waveSamples.size()));
            outlet_anything(x->info, gensym("loaded"), 3, a);
        }
    }
    if (x->pending && L.state.load() == 0) {
        x->pending = false;
        request(x, *x->pending_entry);
        return;
    }
    if (L.state.load() != 0 || x->pending) clock_delay(x->poll, 5);
}

void set_defaults(t_eps* x, bool keep_settings) {
    for (int i = 0; i < kCtlCount; ++i)
        if (!keep_settings || kControls[i].kind == 'O') x->value[i] = kControls[i].def;
}

bool select_library(t_eps* x, int n) {
    if (n < 0 || n >= static_cast<int>(g_lib.size())) return false;
    set_defaults(x, true);
    x->source = kLibrary;
    x->index = n;
    request(x, g_lib[static_cast<size_t>(n)]);
    apply(x);
    return true;
}

bool select_user(t_eps* x, int n) {
    if (n < 0 || n >= kUserSlots || !g_user[n].used) return false;
    for (int i = 0; i < kCtlCount; ++i)
        if (kControls[i].kind == 'O') x->value[i] = g_user[n].value[i];
    x->source = kUser;
    x->index = n;
    x->last_user = n;
    // the same instrument again: only the controls change, nothing is loaded
    const LibEntry& e = g_user[n].entry;
    if (x->sampler && e.file == x->entry->file && e.index == x->entry->index) {
        *x->name = e.name;
        apply(x);
        out_source(x);
    } else {
        request(x, e);
        apply(x);
    }
    return true;
}

void step_user(t_eps* x, int by) {
    // the next used slot from the one chosen last, wrapping
    int s = x->source == kUser ? x->index : x->last_user;
    for (int k = 0; k < kUserSlots; ++k) {
        s = ((s + (by >= 0 ? 1 : -1)) % kUserSlots + kUserSlots) % kUserSlots;
        if (g_user[s].used) { select_user(x, s); return; }
    }
    out_display(x, "User bank", "empty");
}

bool store(t_eps* x, int n) {
    if (n < 0) n = x->store_to >= 0 ? x->store_to : (x->last_user >= 0 ? x->last_user : 0);
    if (n >= kUserSlots || x->entry->file.empty()) return false;
    Slot& u = g_user[n];
    u.used = true;
    u.entry = *x->entry;
    u.entry.name = *x->name;
    for (int i = 0; i < kCtlCount; ++i) u.value[i] = x->value[i];
    x->source = kUser;
    x->index = n;
    x->last_user = n;
    return true;
}

void eps_command(t_eps* x, t_symbol* s, int argc, t_atom* argv) {
    const char* c = s->s_name;
    const t_float f = argc > 0 ? atom_getfloat(argv) : 0;
    const int n = static_cast<int>(f);
    auto is = [c](const char* k) { return std::strcmp(c, k) == 0; };
    const Control* touched = nullptr;

    if (const Control* ctl = find_control(c)) {
        if (argc > 0) {
            x->value[control_index(ctl)] = std::clamp(n, ctl->lo, ctl->hi);
            touched = ctl;
        } else {
            out_info(x, control_index(ctl));
        }
    } else if (is("n")) {
        if (argc < 2 || argv[0].a_type != A_SYMBOL) return;
        const char* name = atom_getsymbol(argv)->s_name;
        const Control* nc = find_control(name);
        if (!nc) {
            if (std::strcmp(name, "none") && std::strcmp(name, "-")) pd_error(x, "eps~: n: no control '%s'", name);
            return;
        }
        const int i = control_index(nc);
        const int before = x->value[i];
        x->value[i] = from127(*nc, atom_getfloat(argv + 1));
        touched = nc;
        if (x->value[i] == before) out_info(x, i);   // always answered
    } else if (is("cycle")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        const Control* nc = find_control(atom_getsymbol(argv)->s_name);
        if (!nc) return;
        const int span = nc->hi - nc->lo + 1;
        const int by = argc > 1 ? static_cast<int>(atom_getfloat(argv + 1)) : 1;
        const int i = control_index(nc);
        x->value[i] = nc->lo + (((x->value[i] - nc->lo + by) % span) + span) % span;
        touched = nc;
    } else if (is("ninfo")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        if (const Control* nc = find_control(atom_getsymbol(argv)->s_name)) out_info(x, control_index(nc));
    } else if (is("dump")) {
        for (int i = 0; i < kCtlCount; ++i) out_info(x, i);
    } else if (is("library")) {
        const std::string dir = argc > 0 && argv[0].a_type == A_SYMBOL ? atom_getsymbol(argv)->s_name
                                                                         : g_ext_dir + "/eps";
        const int got = scan_library(dir);
        post("eps~: library %s: %d instruments", dir.c_str(), got);
        out_value(x, "presets", got);
    } else if (is("rescan")) {
        out_value(x, "presets", scan_library(g_lib_dir.empty() ? g_ext_dir + "/eps" : g_lib_dir));
    } else if (is("preset")) {
        if (argc < 1 || !select_library(x, n)) {
            if (g_lib.empty()) pd_error(x, "eps~: preset: the library (%s) is empty", g_lib_dir.c_str());
            else pd_error(x, "eps~: preset: expected 0..%d", static_cast<int>(g_lib.size()) - 1);
        }
    } else if (is("presetstep")) {
        if (g_lib.empty()) { out_display(x, "Library", "empty"); return; }
        const int cnt = static_cast<int>(g_lib.size());
        const int from = x->source == kLibrary ? x->index : (argc > 0 && n < 0 ? 0 : -1);
        select_library(x, ((from + (argc > 0 ? n : 1)) % cnt + cnt) % cnt);
    } else if (is("open")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        LibEntry e;
        e.file = atom_getsymbol(argv)->s_name;
        e.index = argc > 1 ? static_cast<int>(atom_getfloat(argv + 1)) : 0;
        e.name = e.file;
        set_defaults(x, true);
        x->source = kOpen;
        x->index = -1;
        request(x, e);
        apply(x);
    } else if (is("user")) {
        if (argc < 1 || !select_user(x, n)) pd_error(x, "eps~: user: slot %d is empty", n);
    } else if (is("userstep")) {
        step_user(x, argc > 0 ? n : 1);
    } else if (is("init")) {
        set_defaults(x, true);
        x->source = kInit;
        apply(x);
        out_source(x);
    } else if (is("reload")) {
        if (x->source == kLibrary) select_library(x, x->index);
        else if (x->source == kUser) select_user(x, x->index);
    } else if (is("store")) {
        if (!store(x, argc > 0 ? n : -1)) pd_error(x, "eps~: store: nothing loaded, or not 0..%d", kUserSlots - 1);
        else out_display(x, "Stored", "User " + std::to_string(x->index));
    } else if (is("savearm")) {
        x->store_to = x->last_user >= 0 ? x->last_user : 0;
        out_display(x, "Store to?", "User " + std::to_string(x->store_to));
        return;
    } else if (is("saveslot")) {
        if (x->store_to >= 0) {
            x->store_to = ((x->store_to + (argc > 0 ? n : 1)) % kUserSlots + kUserSlots) % kUserSlots;
            out_display(x, "Store to?", "User " + std::to_string(x->store_to));
        }
        return;
    } else if (is("savecancel")) {
        if (x->store_to >= 0) out_display(x, "Store", "cancelled");
        x->store_to = -1;
        return;
    } else if (is("save")) {
        const bool ok = store(x, -1);
        x->store_to = -1;
        if (ok) {
            std::string err;
            if (!write_user(g_ext_dir + "/" + kUserFile, err)) pd_error(x, "eps~: save: %s", err.c_str());
            out_display(x, "Stored", "User " + std::to_string(x->index));
        }
    } else if (is("userwrite")) {
        std::string err;
        const std::string path = user_path(argc, argv);
        if (!write_user(path, err)) pd_error(x, "eps~: userwrite: %s", err.c_str());
        else post("eps~: user bank written to %s", path.c_str());
    } else if (is("userread")) {
        std::string err;
        const std::string path = user_path(argc, argv);
        const int got = read_user(path, err);
        if (got < 0) pd_error(x, "eps~: userread: %s", err.c_str());
        else post("eps~: user bank: %d slots from %s", got, path.c_str());
    } else if (is("presets")) {
        for (size_t i = 0; i < g_lib.size(); ++i) out_slot(x, "preset", static_cast<int>(i), g_lib[i].name);
        out_value(x, "presets", static_cast<t_float>(g_lib.size()));
    } else if (is("users")) {
        for (int i = 0; i < kUserSlots; ++i)
            if (g_user[i].used) out_slot(x, "user", i, g_user[i].entry.name);
        out_value(x, "users", kUserSlots);
    } else if (is("channel")) {
        x->channel = std::clamp(n, 0, 16);
        out_value(x, "channel", x->channel);
    } else if (is("txchannel")) {
        x->txchannel = std::clamp(n, 1, 16);
        out_value(x, "txchannel", x->txchannel);
    } else if (is("thru")) {
        x->thru = n != 0;
        out_value(x, "thru", x->thru);
    } else if (is("status")) {
        out_source(x);
        out_value(x, "channel", x->channel);
        out_value(x, "txchannel", x->txchannel);
        out_value(x, "thru", x->thru);
        out_value(x, "presets", static_cast<t_float>(g_lib.size()));
        out_value(x, "sounding", x->sampler ? x->sampler->activeVoiceCount() : 0);
    } else if (is("bend")) {
        x->bend = f;
        if (x->sampler) x->sampler->setBend(f);
        return;
    } else if (is("mod")) {
        x->wheel = f;
        if (x->sampler) x->sampler->setModWheel(f);
        return;
    } else if (is("pressure") || is("pedal") || is("xctrl")) {
        if (x->sampler) {
            if (is("pressure")) x->sampler->setPressure(f);
            else if (is("pedal")) x->sampler->setPedal(f);
            else x->sampler->setXctrl(f);
        }
        x->ctl[is("pressure") ? 0 : (is("pedal") ? 1 : 2)] = f;
        return;
    } else if (is("polypressure")) {
        if (argc >= 2 && x->sampler) x->sampler->setPolyPressure(n, atom_getfloat(argv + 1));
        return;
    } else if (is("sustain")) {
        x->sustain = f != 0;
        if (x->sampler) x->sampler->setSustain(x->sustain);
        return;
    } else if (is("stop") || is("reset")) {
        if (x->sampler) x->sampler->allNotesOff();
        return;
    } else {
        pd_error(x, "eps~: no method for '%s'", c);
        return;
    }
    apply(x);
    report(x);
    if (touched) out_display(x, touched->label, value_text(*touched, x->value[control_index(touched)]));
}

void eps_note(t_eps* x, t_float note, t_float velocity) {
    const int n = static_cast<int>(note);
    if (n < 0 || n > 127) return;
    if (x->thru) {
        const int v = std::clamp(static_cast<int>(velocity), 0, 127);
        outlet_float(x->info, 0x90 | ((x->txchannel - 1) & 0x0F));
        outlet_float(x->info, n);
        outlet_float(x->info, v);
    }
    if (!x->sampler) return;
    if (velocity > 0) x->sampler->noteOn(n, std::clamp(static_cast<int>(velocity), 1, 127));
    else x->sampler->noteOff(n);
}

void eps_float(t_eps* x, t_float f) { eps_note(x, f, x->velocity); }

void eps_list(t_eps* x, t_symbol*, int argc, t_atom* argv) {
    if (argc >= 3 && x->channel && static_cast<int>(atom_getfloat(argv + 2)) != x->channel) return;
    if (argc >= 2) eps_note(x, atom_getfloat(argv), atom_getfloat(argv + 1));
    else if (argc == 1) eps_float(x, atom_getfloat(argv));
}

void eps_proxy_anything(t_eps_proxy* p, t_symbol* s, int argc, t_atom* argv) {
    eps_command(p->owner, s, argc, argv);
}

// ---- raw MIDI -------------------------------------------------------------------

void midi_message(t_eps* x, int status, int d1, int d2) {
    if (x->thru) {
        outlet_float(x->info, status);
        outlet_float(x->info, d1);
        const int t = status & 0xF0;
        if (t != 0xC0 && t != 0xD0) outlet_float(x->info, d2);
    }
    if (x->channel && (status & 0x0F) + 1 != x->channel) return;
    eps::Sampler* s = x->sampler;
    switch (status & 0xF0) {
        case 0x90:
            if (!s) return;
            if (d2 > 0) s->noteOn(d1, d2);
            else s->noteOff(d1);
            return;
        case 0x80:
            if (s) s->noteOff(d1);
            return;
        case 0xB0:
            switch (d1) {
                case 1:
                    x->wheel = d2 / 127.0f;
                    if (s) s->setModWheel(x->wheel);
                    return;
                case 2:
                    x->ctl[2] = d2 / 127.0f;
                    if (s) s->setXctrl(x->ctl[2]);
                    return;
                case 4:
                    x->ctl[1] = d2 / 127.0f;
                    if (s) s->setPedal(x->ctl[1]);
                    return;
                case 7:
                    x->value[kC_volume] = std::clamp(d2, 0, 127);
                    apply(x);
                    report(x);
                    return;
                case 64:
                    x->sustain = d2 >= 64;
                    if (s) s->setSustain(x->sustain);
                    return;
                case 120:
                case 123:
                    if (s) s->allNotesOff();
                    return;
                default:
                    return;   // the CC pages belong to [epscc]
            }
        case 0xC0:
            if (x->source == kUser && d1 < kUserSlots && g_user[d1].used) select_user(x, d1);
            else select_library(x, d1);
            report(x);
            return;
        case 0xA0:
            if (s) s->setPolyPressure(d1, d2 / 127.0f);
            return;
        case 0xD0:
            x->ctl[0] = d1 / 127.0f;
            if (s) s->setPressure(x->ctl[0]);
            return;
        case 0xE0:
            x->bend = ((d2 << 7 | d1) - 8192) / 8192.0f;
            if (s) s->setBend(x->bend);
            return;
        default:
            return;
    }
}

void eps_proxy_float(t_eps_proxy* p, t_float f) {
    t_eps* x = p->owner;
    Midi& m = *x->midi;
    const int b = static_cast<int>(f) & 0xFF;
    if (b >= 0xF8) {
        if (x->thru) outlet_float(x->info, b);
        return;
    }
    if (b == 0xF0) { m.in_sx = true; if (x->thru) outlet_float(x->info, b); return; }
    if (m.in_sx) {
        if (x->thru) outlet_float(x->info, b);
        if (b < 0x80) return;
        m.in_sx = false;
        if (b == 0xF7) return;
    }
    if (b >= 0xF0) { m.status = 0; return; }
    if (b >= 0x80) { m.status = b; m.have = 0; return; }
    if (!m.status) return;
    m.data[m.have++] = b;
    const int need = ((m.status & 0xF0) == 0xC0 || (m.status & 0xF0) == 0xD0) ? 1 : 2;
    if (m.have < need) return;
    m.have = 0;
    midi_message(x, m.status, m.data[0], need == 2 ? m.data[1] : 0);
}

void eps_proxy_list(t_eps_proxy* p, t_symbol*, int argc, t_atom* argv) {
    for (int i = 0; i < argc; ++i) eps_proxy_float(p, atom_getfloat(argv + i));
}

// ---- dsp ------------------------------------------------------------------------

t_int* eps_perform(t_int* w) {
    t_eps* x = reinterpret_cast<t_eps*>(w[1]);
    t_sample* l = reinterpret_cast<t_sample*>(w[2]);
    t_sample* r = reinterpret_cast<t_sample*>(w[3]);
    const int n = static_cast<int>(w[4]);
    if (x->sampler) {
        x->sampler->process(l, r, n);
    } else {
        std::fill(l, l + n, 0.0f);
        std::fill(r, r + n, 0.0f);
    }
    return w + 5;
}

void eps_dsp(t_eps* x, t_signal** sp) {
    x->sr = sp[0]->s_sr;
    if (x->sampler) x->sampler->setSampleRate(x->sr);
    dsp_add(eps_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, static_cast<t_int>(sp[0]->s_n));
}

// ---- lifetime -------------------------------------------------------------------

void* eps_new(t_symbol*, int argc, t_atom* argv) {
    t_eps* x = reinterpret_cast<t_eps*>(pd_new(eps_class));
    load_data();
    x->velocity = 100;
    x->info = nullptr;
    x->sampler = nullptr;
    x->loader = new Loader();
    x->midi = new Midi();
    x->entry = new LibEntry();
    x->pending_entry = new LibEntry();
    x->name = new std::string();
    x->pending = false;
    x->sr = sys_getsr() > 0 ? sys_getsr() : 44100.0f;
    x->channel = 0;
    x->txchannel = 1;
    x->thru = 1;
    x->source = kNone;
    x->index = -1;
    x->last_user = -1;
    x->store_to = -1;
    x->wheel = 0;
    x->bend = 0;
    x->ctl[0] = x->ctl[1] = x->ctl[2] = 0;
    x->sustain = false;
    set_defaults(x, false);
    for (int& v : x->shown) v = INT32_MIN;
    x->poll = clock_new(x, reinterpret_cast<t_method>(eps_poll));

    floatinlet_new(&x->obj, &x->velocity);
    x->proxy.pd = eps_proxy_class;
    x->proxy.owner = x;
    inlet_new(&x->obj, &x->proxy.pd, nullptr, nullptr);
    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);
    x->info = outlet_new(&x->obj, nullptr);

    Loader* L = x->loader;
    L->thread = std::thread([L] { L->run(); });

    // [eps~ n]: start with library instrument n, [eps~ file.efe]: with that file
    if (argc > 0 && argv[0].a_type == A_FLOAT) select_library(x, static_cast<int>(atom_getfloat(argv)));
    else if (argc > 0 && argv[0].a_type == A_SYMBOL) {
        LibEntry e;
        e.file = atom_getsymbol(argv)->s_name;
        e.name = e.file;
        x->source = kOpen;
        request(x, e);
    }
    for (int i = 0; i < kCtlCount; ++i) x->shown[i] = x->value[i];   // quiet at creation
    return x;
}

void eps_free(t_eps* x) {
    clock_free(x->poll);
    x->loader->quit.store(true);
    if (x->loader->thread.joinable()) x->loader->thread.join();
    delete x->loader;
    delete x->sampler;
    delete x->midi;
    delete x->entry;
    delete x->pending_entry;
    delete x->name;
}

}  // namespace

extern "C" {
#if defined(_WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void eps_tilde_setup(void) {
    eps_class = class_new(gensym("eps~"), reinterpret_cast<t_newmethod>(eps_new),
                          reinterpret_cast<t_method>(eps_free), sizeof(t_eps), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(eps_class, reinterpret_cast<t_method>(eps_float));
    class_addlist(eps_class, reinterpret_cast<t_method>(eps_list));
    class_addanything(eps_class, reinterpret_cast<t_method>(eps_command));
    class_addmethod(eps_class, reinterpret_cast<t_method>(eps_dsp), gensym("dsp"), A_CANT, 0);

    eps_proxy_class = class_new(gensym("eps~ midi"), nullptr, nullptr, sizeof(t_eps_proxy),
                                CLASS_PD | CLASS_NOINLET, A_NULL);
    class_addanything(eps_proxy_class, reinterpret_cast<t_method>(eps_proxy_anything));
    class_addfloat(eps_proxy_class, reinterpret_cast<t_method>(eps_proxy_float));
    class_addlist(eps_proxy_class, reinterpret_cast<t_method>(eps_proxy_list));
}
}
