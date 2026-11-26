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
// Folders, as on the EPS: the library's images are folders, with the Ensoniq
// directories inside; EFE files at the top level are single instruments.
//   browse              show the current folder (folder, bsel)
//   bstep [n], bselect k   move the selection in the folder
//   benter              enter the selected directory or load the selected instrument
//   bup                 the folder above
//   userstep [n]        the next (previous) instrument in the folder of the one playing
// Saving (always in the Ensoniq format, readable on the EPS): the offsets of the
// controls are written into the instrument, then set back to 0.
//   saveas <name>       a new file in the current folder: into the image, or an
//                       EFE at the top level; refused if the name is there
//   replace             overwrite the file the instrument was loaded from
//   save                saveas with a free name made from the instrument's (pads)
//   mkdir <name>        a new directory in the current folder of an image
// Names: 12 characters of A-Z 0-9 space * - + ('_' = space). Changes to an image
// are journaled: <image>.undo keeps the old blocks (eps_fs_tool undo restores).
//   init, reload
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
#include "eps_edit.h"
#include "eps_fs.h"
#include "eps_sysex.h"

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

constexpr float kOutputGain = 1.0f;   // a voice at full level is -12 dB (0xE0), as on the EPS

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
    std::string path;     // slot numbers in the image ("03 07"), "" for EFE files
};

// The files of the library with their directory entries (read once per scan).
struct LibFile {
    std::string file;
    bool image = false;                  // a disk image (folders), else a single file
    std::vector<eps::DirEntry> entries;
};

std::string g_ext_dir;                 // the external's folder
std::string g_lib_dir;                 // the library folder in use
std::vector<LibEntry> g_lib;
std::vector<LibFile> g_files;
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
    g_files.clear();
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
        const bool image = disk.format() == eps::ImageFormat::Raw;
        LibFile lf;
        lf.file = f;
        lf.image = image;
        lf.entries = disk.files();
        g_files.push_back(lf);
        for (size_t k = 0; k < list.size(); ++k) {
            LibEntry le;
            le.file = f;
            le.index = static_cast<int>(k);
            le.name = spaces_to_underscore(list[k]->name);
            le.path = image ? list[k]->path : "";
            g_lib.push_back(le);
        }
    }
    return static_cast<int>(g_lib.size());
}

void load_data() {
    if (g_data_tried) return;
    g_data_tried = true;
    const char* h = class_gethelpdir(eps_class);
    g_ext_dir = h ? h : ".";
    const int n = scan_library(g_ext_dir + "/eps");
    if (n) post("eps~: library %s/eps: %d instruments", g_ext_dir.c_str(), n);
}

// ---- folders ----------------------------------------------------------------------

std::string parent_path(const std::string& p) {
    const size_t k = p.rfind(' ');
    return k == std::string::npos ? "" : p.substr(0, k);
}

const LibFile* lib_file(const std::string& f) {
    for (const LibFile& lf : g_files) if (lf.file == f) return &lf;
    return nullptr;
}

int lib_index(const std::string& file, const std::string& path, int index) {
    for (size_t i = 0; i < g_lib.size(); ++i)
        if (g_lib[i].file == file && (path.empty() ? g_lib[i].index == index : g_lib[i].path == path))
            return static_cast<int>(i);
    return -1;
}

struct BItem {
    char kind;            // 'D' folder, 'I' instrument
    std::string name;
    std::string file, path;
    int lib = -1;         // instrument: index in g_lib
};

// What a folder shows: at the top the images (folders) and EFE files, in an image
// its directories and instruments in directory order.
std::vector<BItem> folder_items(const std::string& file, const std::string& dir) {
    std::vector<BItem> v;
    if (file.empty()) {
        for (const LibFile& lf : g_files) {
            if (lf.image) { v.push_back({'D', lf.file, lf.file, "", -1}); continue; }
            const int li = lib_index(lf.file, "", 0);
            if (li >= 0) v.push_back({'I', g_lib[size_t(li)].name, lf.file, "", li});
        }
        return v;
    }
    const LibFile* lf = lib_file(file);
    if (!lf) return v;
    for (const eps::DirEntry& e : lf->entries) {
        if (parent_path(e.path) != dir) continue;
        if (e.type == eps::kSubdir) v.push_back({'D', spaces_to_underscore(e.name), file, e.path, -1});
        else if (e.type == eps::kInstrument) v.push_back({'I', spaces_to_underscore(e.name), file, e.path, lib_index(file, e.path, 0)});
    }
    return v;
}

// "IMAGE.hda/PIANOS/GRAND" for a folder.
std::string folder_text(const std::string& file, const std::string& dir) {
    if (file.empty()) return "library";
    std::string t = file;
    const LibFile* lf = lib_file(file);
    std::string p;
    size_t pos = 0;
    while (lf && pos < dir.size()) {
        const size_t sp = dir.find(' ', pos);
        p = dir.substr(0, sp == std::string::npos ? dir.size() : sp);
        for (const eps::DirEntry& e : lf->entries) if (e.path == p) t += "/" + spaces_to_underscore(e.name);
        pos = sp == std::string::npos ? dir.size() : sp + 1;
    }
    return t;
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

enum Source { kNone = -1, kInit = 0, kLibrary = 1, kOpen = 3 };

struct t_eps {
    t_object obj;
    t_float velocity;
    t_eps_proxy proxy;
    t_outlet* info;
    t_clock* poll;
    t_clock* sxclock;                    // SysEx with a real EPS: steps and timeouts
    eps::SysexLink* sysex;
    int epsch;                           // the EPS's MIDI base channel (1-16)
    int sxpct;                           // last progress reported
    eps::Sampler* sampler;               // the playing instrument (null = none yet)
    Loader* loader;
    Midi* midi;
    int value[kCtlCount];                // the controls
    int shown[kCtlCount];                // what outlet 3 said last
    float sr;
    int channel, txchannel, thru;
    int source, index;
    std::string* bfile;                  // the folder shown: library file ("" = top level)
    std::string* bdir;                   // and directory in it (slot numbers)
    int bsel;                            // the selected item in the folder
    LibEntry* entry;                     // what plays (or loads)
    std::string* name;
    float wheel, bend;
    float ctl[3];                        // pressure, pedal, xctrl
    bool sustain;
    bool pending;                        // a request waits for the loader to be free
    LibEntry* pending_entry;
    eps::Editor* editor;                 // selection and edits of the editor
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

// ---- the instrument editor (eps_edit) -------------------------------------------

void out_einfo(t_eps* x, int id) {
    if (!x->sampler) return;
    eps::Instrument& in = x->sampler->editInstrument();
    const std::string t = x->editor->text(in, id);
    t_atom a[3];
    SETSYMBOL(&a[0], gensym(eps::kEditParams[id].name));
    SETFLOAT(&a[1], x->editor->get127(in, id));
    SETSYMBOL(&a[2], gensym(t.c_str()));
    outlet_anything(x->info, gensym("info"), 3, a);
}

// sel <layer> <wavesample in layer> <env> <layers> <layer name> <wavesample name> <info> <wavesamples in layer>
void out_sel(t_eps* x) {
    if (!x->sampler) return;
    const eps::Instrument& in = x->sampler->instrument();
    const eps::Editor& e = *x->editor;
    const std::string ln = e.layer < int(in.layers.size()) ? spaces_to_underscore(in.layers[size_t(e.layer)].name) : "-";
    auto it = in.waveSamples.find(e.ws);
    const std::string wn = it != in.waveSamples.end() ? spaces_to_underscore(it->second.name) : "-";
    const std::string info = e.selectionInfo(in);
    t_atom a[8];
    SETFLOAT(&a[0], e.layer + 1);
    SETFLOAT(&a[1], e.wsPosition(in));
    SETFLOAT(&a[2], e.env + 1);
    SETFLOAT(&a[3], static_cast<t_float>(in.layers.size()));
    SETSYMBOL(&a[4], gensym(ln.empty() ? "-" : ln.c_str()));
    SETSYMBOL(&a[5], gensym(wn.empty() ? "-" : wn.c_str()));
    SETSYMBOL(&a[6], gensym(info.c_str()));
    SETFLOAT(&a[7], static_cast<t_float>(e.layerWaveSamples(in).size()));
    outlet_anything(x->info, gensym("sel"), 8, a);
    t_atom f[2];
    SETFLOAT(&f[0], e.follow);
    SETFLOAT(&f[1], e.zeroCross);
    outlet_anything(x->info, gensym("editmode"), 2, f);
}

// All parameters of the given scopes ("ilmwepf" = everything).
void out_edump(t_eps* x, const char* scopes) {
    if (!x->sampler) return;
    for (int i = 0; i < eps::kEditParamCount; ++i)
        if (std::strchr(scopes, eps::kEditParams[i].scope)) out_einfo(x, i);
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
void follow_entry(t_eps* x);

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
            x->editor->fit(in);
            follow_entry(x);
            out_sel(x);
            out_edump(x, "ilmwepf");
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

void out_folder(t_eps* x) {
    const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
    if (x->bsel >= static_cast<int>(v.size())) x->bsel = static_cast<int>(v.size()) - 1;
    if (x->bsel < 0) x->bsel = 0;
    t_atom a[4];
    SETSYMBOL(&a[0], gensym(folder_text(*x->bfile, *x->bdir).c_str()));
    SETFLOAT(&a[1], static_cast<t_float>(v.size()));
    outlet_anything(x->info, gensym("folder"), 2, a);
    SETFLOAT(&a[0], x->bsel);
    SETFLOAT(&a[1], static_cast<t_float>(v.size()));
    const bool any = !v.empty();
    SETSYMBOL(&a[2], gensym(any ? (v[size_t(x->bsel)].kind == 'D' ? "dir" : "inst") : "-"));
    const std::string shown = any ? v[size_t(x->bsel)].name + (v[size_t(x->bsel)].kind == 'D' ? "/" : "") : "-";
    SETSYMBOL(&a[3], gensym(shown.c_str()));
    outlet_anything(x->info, gensym("bsel"), 4, a);
}

// Show the folder of the instrument that plays, with it selected.
void follow_entry(t_eps* x) {
    const LibEntry& e = *x->entry;
    const LibFile* lf = lib_file(e.file);
    if (!lf) return;
    *x->bfile = lf->image ? e.file : "";
    *x->bdir = lf->image ? parent_path(e.path) : "";
    const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].kind == 'I' && v[i].file == e.file && v[i].path == e.path) x->bsel = static_cast<int>(i);
    out_folder(x);
}

// The offsets of the controls written into the instrument (as far as the EPS has
// the parameter), then set to 0: what plays stays the same.
void bake_offsets(t_eps* x) {
    eps::Instrument& in = x->sampler->editInstrument();
    auto v = [x](int c) { return x->value[c]; };
    in.transpose = std::clamp(in.transpose + v(kC_transpose), -36, 36);
    for (auto& kv : in.waveSamples) {
        eps::WaveSample& w = kv.second;
        w.filterCutoff[0] = std::clamp(w.filterCutoff[0] + v(kC_cut), 0, 150);
        w.filterCutoff[1] = std::clamp(w.filterCutoff[1] + v(kC_cut2), 0, 150);
        w.filterEnvAmount[0] = std::clamp(w.filterEnvAmount[0] + v(kC_fenv), -127, 127);
        w.ampEnv.time[1] = std::clamp(w.ampEnv.time[1] + v(kC_att), 0, 99);
        for (int i = 2; i <= 4; ++i) w.ampEnv.time[i] = std::clamp(w.ampEnv.time[i] + v(kC_dec), 0, 99);
        for (int i = 5; i <= 6; ++i) w.ampEnv.time[i] = std::clamp(w.ampEnv.time[i] + v(kC_rel), 0, 99);
        w.fineTune = std::clamp(w.fineTune + static_cast<int>(std::lround(v(kC_tune) * 128.0 / 100.0)), -127, 127);
    }
    for (int c : {kC_transpose, kC_cut, kC_cut2, kC_fenv, kC_att, kC_dec, kC_rel, kC_tune})
        x->value[c] = kControls[c].def;
    apply(x);
}

std::string pd_text(int argc, t_atom* argv) {
    std::string t;
    for (int i = 0; i < argc; ++i) {
        char b[256];
        atom_string(argv + i, b, sizeof b);
        if (!t.empty()) t += ' ';
        t += b;
    }
    return t;
}

// Write the instrument that plays. replace: over the file it came from; else a new
// file `name` in the folder shown.
bool save_instrument(t_eps* x, std::string name, bool replace, std::string& err) {
    if (!x->sampler) { err = "nothing loaded"; return false; }
    eps::Instrument& in = x->sampler->editInstrument();
    const LibEntry src = *x->entry;
    if (replace) name = in.name;
    name = eps::ensoniqName(name);
    if (name.empty()) { err = "no name"; return false; }
    const std::string file = replace ? src.file : *x->bfile;
    const LibFile* lf = file.empty() ? nullptr : lib_file(file);
    if (replace && !lf) { err = "the instrument is not from the library"; return false; }
    LibEntry ne;
    ne.name = spaces_to_underscore(name);
    // Everything that can refuse is checked before the offsets go into the instrument.
    eps::Volume vol;
    uint32_t dir = 3;
    int replaceSlot = -1;
    std::string dirPath, efePath;
    if (lf && lf->image) {
        const std::string img = join(g_lib_dir, file);
        if (!vol.open(img, err)) return false;
        vol.setJournal(img + ".undo");
        dirPath = replace ? parent_path(src.path) : *x->bdir;
        if (!vol.dirOfPath(dirPath, false, dir, err)) return false;
        if (replace) {
            const size_t sp = src.path.rfind(' ');
            replaceSlot = std::atoi(src.path.c_str() + (sp == std::string::npos ? 0 : sp + 1));
        } else {
            std::vector<eps::FsEntry> l;
            if (!vol.list(dir, l, err)) return false;
            for (const eps::FsEntry& e : l)
                if (e.type == eps::kInstrument && e.name == name) { err = name + " is already in this folder"; return false; }
        }
    } else {
        ne.file = replace ? src.file : spaces_to_underscore(name) + ".efe";
        efePath = join(g_lib_dir, ne.file);
        if (!replace)
            if (FILE* f = std::fopen(efePath.c_str(), "rb")) { std::fclose(f); err = ne.file + " is already there"; return false; }
    }
    bake_offsets(x);
    in.name = name;
    std::vector<uint8_t> data;
    if (!eps::encodeInstrument(in, data, err)) return false;
    if (vol.isOpen()) {
        int slot = -1;
        bool exists = false;
        if (!vol.writeFile(dir, name, eps::kInstrument, data, replaceSlot, slot, exists, err)) return false;
        vol.close();
        char num[8];
        std::snprintf(num, sizeof num, "%02d", slot);
        ne.file = file;
        ne.path = dirPath.empty() ? num : dirPath + " " + num;
    } else if (!eps::writeEfe(efePath, name, eps::kInstrument, data, err)) {
        return false;
    }
    scan_library(g_lib_dir);
    const int li = lib_index(ne.file, ne.path, 0);
    ne.index = li >= 0 ? g_lib[size_t(li)].index : 0;
    *x->entry = ne;
    *x->name = ne.name;
    x->source = kLibrary;
    x->index = li;
    follow_entry(x);
    out_source(x);
    out_value(x, "presets", static_cast<t_float>(g_lib.size()));
    return true;
}

// A free name for the pads' save: the instrument's name with a number at the end.
std::string free_name(t_eps* x) {
    const std::string base = eps::ensoniqName(x->sampler ? x->sampler->instrument().name : "NEW");
    const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
    for (int k = 1; k < 1000; ++k) {
        const std::string suf = " " + std::to_string(k);
        std::string b = base;
        while (!b.empty() && b.size() + suf.size() > 12) b.pop_back();
        while (!b.empty() && b.back() == ' ') b.pop_back();
        const std::string cand = b + suf;
        bool used = false;
        for (const BItem& it : v) used = used || it.name == spaces_to_underscore(cand);
        if (!used) return cand;
    }
    return base;
}

// ---- SysEx with a real EPS ---------------------------------------------------------

// A name not yet used in the folder shown: the name itself, else with a number.
std::string unused_name(t_eps* x, const std::string& want) {
    const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
    auto used = [&v](const std::string& n) {
        for (const BItem& it : v) if (it.name == spaces_to_underscore(n)) return true;
        return false;
    };
    const std::string base = eps::ensoniqName(want.empty() ? "FROM EPS" : want);
    if (!used(base)) return base;
    for (int k = 1; k < 1000; ++k) {
        const std::string suf = " " + std::to_string(k);
        std::string b = base;
        while (!b.empty() && b.size() + suf.size() > 12) b.pop_back();
        while (!b.empty() && b.back() == ' ') b.pop_back();
        if (!used(b + suf)) return b + suf;
    }
    return base;
}

// A fetched instrument file as a new file in the folder shown, then loaded.
bool store_fetched(t_eps* x, const std::string& want, std::vector<uint8_t> data, std::string& err) {
    const std::string name = unused_name(x, want);
    for (int i = 0; i < 12; ++i) data[size_t(10 + 2 * i)] = static_cast<uint8_t>(i < static_cast<int>(name.size()) ? name[size_t(i)] : ' ');
    const LibFile* lf = x->bfile->empty() ? nullptr : lib_file(*x->bfile);
    LibEntry ne;
    if (lf && lf->image) {
        const std::string img = join(g_lib_dir, *x->bfile);
        eps::Volume vol;
        if (!vol.open(img, err)) return false;
        vol.setJournal(img + ".undo");
        uint32_t dir = 3;
        int slot = -1;
        bool exists = false;
        if (!vol.dirOfPath(*x->bdir, false, dir, err) ||
            !vol.writeFile(dir, name, eps::kInstrument, data, -1, slot, exists, err)) return false;
        char num[8];
        std::snprintf(num, sizeof num, "%02d", slot);
        ne.file = *x->bfile;
        ne.path = x->bdir->empty() ? num : *x->bdir + " " + num;
    } else {
        ne.file = spaces_to_underscore(name) + ".efe";
        if (!eps::writeEfe(join(g_lib_dir, ne.file), name, eps::kInstrument, data, err)) return false;
    }
    scan_library(g_lib_dir);
    const int li = lib_index(ne.file, ne.path, 0);
    out_value(x, "presets", static_cast<t_float>(g_lib.size()));
    if (li >= 0) select_library(x, li);
    return true;
}

void eps_sx_tick(t_eps* x) {
    if (!x->sysex) return;
    const double now = clock_gettimesince(0) / 1000.0;
    x->sysex->tick(now);
    if (x->sysex->percent != x->sxpct) {
        x->sxpct = x->sysex->percent;
        t_atom a[2];
        SETSYMBOL(&a[0], gensym(x->sysex->step.c_str()));
        SETFLOAT(&a[1], x->sxpct);
        outlet_anything(x->info, gensym("sysex"), 2, a);
        out_display(x, "SysEx " + std::to_string(x->sxpct) + "%", x->sysex->step);
    }
    std::string err;
    switch (x->sysex->takeDone()) {
    case eps::SysexLink::Done::Instrument:
        if (!store_fetched(x, x->sysex->name, x->sysex->file, err)) {
            pd_error(x, "eps~: fetch: %s", err.c_str());
            out_display(x, "Not saved", err);
        }
        outlet_anything(x->info, gensym("fetched"), 0, nullptr);
        break;
    case eps::SysexLink::Done::WaveSample: {
        eps::WaveSample ws;
        ws.sampleRate = x->sysex->sampleRate;
        const std::string fn = join(g_lib_dir, spaces_to_underscore(eps::ensoniqName(x->sysex->name)) + ".wav");
        if (!eps::writeWav(fn, ws, x->sysex->pcm, err)) pd_error(x, "eps~: fetchws: %s", err.c_str());
        else { post("eps~: wavesample %s -> %s", x->sysex->name.c_str(), fn.c_str()); out_display(x, "Fetched", x->sysex->name); }
        outlet_anything(x->info, gensym("fetched"), 0, nullptr);
        break;
    }
    case eps::SysexLink::Done::Sent:
        out_display(x, "Sent", "to the EPS");
        outlet_anything(x->info, gensym("sent"), 0, nullptr);
        break;
    case eps::SysexLink::Done::Error:
        pd_error(x, "eps~: SysEx: %s", x->sysex->error.c_str());
        out_display(x, "SysEx failed", x->sysex->error);
        break;
    case eps::SysexLink::Done::None:
        break;
    }
    if (x->sysex->busy()) clock_delay(x->sxclock, 20);
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
    } else if (is("e")) {
        // e <name> <0-127>: change a parameter of the loaded instrument (editor)
        if (argc < 2 || argv[0].a_type != A_SYMBOL || !x->sampler) return;
        const int id = eps::Editor::find(atom_getsymbol(argv)->s_name);
        if (id < 0) { pd_error(x, "eps~: e: no parameter '%s'", atom_getsymbol(argv)->s_name); return; }
        x->editor->set127(x->sampler->editInstrument(), id, atom_getfloat(argv + 1));
        const char sc = eps::kEditParams[id].scope;
        if (sc == 'p' || sc == 'f') out_edump(x, "pf");   // the points limit each other
        else out_einfo(x, id);
        if (sc == 'i' || sc == 'm') apply(x);
        return;
    } else if (is("einfo")) {
        if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
        const int id = eps::Editor::find(atom_getsymbol(argv)->s_name);
        if (id >= 0) out_einfo(x, id);
        return;
    } else if (is("edump")) {
        out_sel(x);
        out_edump(x, "ilmwepf");
        return;
    } else if (is("sel")) {
        // sel layer|ws|env <n>, sel layerstep|wsstep <+-1>
        if (argc < 2 || argv[0].a_type != A_SYMBOL || !x->sampler) return;
        const std::string what = atom_getsymbol(argv)->s_name;
        const int v = static_cast<int>(atom_getfloat(argv + 1));
        const eps::Instrument& in = x->sampler->instrument();
        eps::Editor& e = *x->editor;
        if (what == "layer") e.layer = std::clamp(v - 1, 0, std::max(0, int(in.layers.size()) - 1));
        else if (what == "layerstep" && !in.layers.empty())
            e.layer = ((e.layer + v) % int(in.layers.size()) + int(in.layers.size())) % int(in.layers.size());
        else if (what == "env") e.env = std::clamp(v - 1, 0, 2);
        else if (what == "ws") e.selectWs(in, v);
        else if (what == "wsstep") e.stepWs(in, v);
        e.fit(in);
        out_sel(x);
        out_edump(x, what == "env" ? "e" : "lmwepf");
        return;
    } else if (is("follow")) {
        x->editor->follow = n != 0;
        out_sel(x);
        return;
    } else if (is("zerox")) {
        x->editor->zeroCross = n != 0;
        out_sel(x);
        return;
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
    } else if (is("epsch")) {
        x->epsch = std::clamp(n, 1, 16);
        if (x->sysex) x->sysex->setChannel(x->epsch - 1);
        return;
    } else if (is("fetch") || is("fetchws") || is("send") || is("sendws")) {
        const double now = clock_gettimesince(0) / 1000.0;
        auto num = [&](int i, int def) { return argc > i && argv[i].a_type == A_FLOAT ? static_cast<int>(atom_getfloat(argv + i)) : def; };
        const int inst = std::clamp(num(0, 1), 1, 8) - 1;
        bool ok = false;
        x->sysex->setChannel(x->epsch - 1);
        if (is("fetch")) ok = x->sysex->fetchInstrument(inst, now);
        else if (is("fetchws")) ok = x->sysex->fetchWaveSample(inst, std::clamp(num(1, 1), 1, 8) - 1, std::clamp(num(2, 1), 1, 127), now);
        else if (is("send")) {
            std::vector<uint8_t> data;
            std::string err;
            if (!x->sampler || !eps::encodeInstrument(x->sampler->instrument(), data, err)) {
                pd_error(x, "eps~: send: %s", x->sampler ? err.c_str() : "nothing loaded");
                return;
            }
            ok = x->sysex->sendInstrument(inst, data, now);
        } else {
            std::vector<int16_t> pcm;
            int rate = 0;
            std::string err;
            const std::string wav = argc > 3 && argv[3].a_type == A_SYMBOL ? join(g_lib_dir, atom_getsymbol(argv + 3)->s_name) : "";
            if (wav.empty() || !eps::readWav(wav, pcm, rate, err)) { pd_error(x, "eps~: sendws: %s", wav.empty() ? "give a WAV file" : err.c_str()); return; }
            ok = x->sysex->sendWaveSample(inst, std::clamp(num(1, 1), 1, 8) - 1, std::clamp(num(2, 1), 1, 127), pcm, now);
        }
        if (!ok) { pd_error(x, "eps~: %s: a SysEx transfer is running (sxcancel stops it)", c); return; }
        x->sxpct = -1;
        clock_delay(x->sxclock, 20);
        return;
    } else if (is("sxcancel")) {
        if (x->sysex) x->sysex->cancel();
        out_display(x, "SysEx", "cancelled");
        return;
    } else if (is("browse")) {
        out_folder(x);
        return;
    } else if (is("bstep") || is("bselect")) {
        const int cnt = static_cast<int>(folder_items(*x->bfile, *x->bdir).size());
        if (cnt) x->bsel = is("bselect") ? std::clamp(n, 0, cnt - 1)
                                         : ((x->bsel + (argc > 0 ? n : 1)) % cnt + cnt) % cnt;
        out_folder(x);
        return;
    } else if (is("benter")) {
        const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
        if (x->bsel < 0 || x->bsel >= static_cast<int>(v.size())) return;
        const BItem& it = v[size_t(x->bsel)];
        if (it.kind == 'D') {
            *x->bfile = it.file;
            *x->bdir = it.path;
            x->bsel = 0;
            out_folder(x);
        } else if (it.lib >= 0) {
            select_library(x, it.lib);
        }
        return;
    } else if (is("bup")) {
        if (x->bfile->empty()) return;
        const std::string from = *x->bdir;
        if (x->bdir->empty()) {
            const std::string was = *x->bfile;
            x->bfile->clear();
            const std::vector<BItem> v = folder_items("", "");
            for (size_t i = 0; i < v.size(); ++i) if (v[i].file == was) x->bsel = static_cast<int>(i);
        } else {
            *x->bdir = parent_path(from);
            const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
            for (size_t i = 0; i < v.size(); ++i) if (v[i].path == from) x->bsel = static_cast<int>(i);
        }
        out_folder(x);
        return;
    } else if (is("userstep")) {
        // the next instrument in the folder of the one playing
        follow_entry(x);
        const std::vector<BItem> v = folder_items(*x->bfile, *x->bdir);
        const int cnt = static_cast<int>(v.size()), by = argc > 0 && n < 0 ? -1 : 1;
        for (int k = 1; k <= cnt; ++k) {
            const BItem& it = v[size_t(((x->bsel + by * k) % cnt + cnt) % cnt)];
            if (it.kind == 'I' && it.lib >= 0) { select_library(x, it.lib); break; }
        }
    } else if (is("saveas") || is("replace") || is("save")) {
        std::string err;
        const bool rep = is("replace");
        const std::string name = is("save") ? free_name(x) : pd_text(argc, argv);
        if (!rep && name.empty()) { pd_error(x, "eps~: saveas: give a name"); return; }
        if (!save_instrument(x, name, rep, err)) {
            pd_error(x, "eps~: %s: %s", c, err.c_str());
            out_display(x, "Not saved", err);
        } else {
            out_display(x, "Saved", *x->name);
            outlet_anything(x->info, gensym("saved"), 0, nullptr);
        }
        report(x);
        return;
    } else if (is("mkdir")) {
        std::string err;
        const LibFile* lf = lib_file(*x->bfile);
        if (!lf || !lf->image) { pd_error(x, "eps~: mkdir: open a folder in a disk image first"); return; }
        eps::Volume vol;
        const std::string img = join(g_lib_dir, *x->bfile);
        uint32_t dir = 3;
        int slot = -1;
        const std::string nm = eps::ensoniqName(pd_text(argc, argv));
        if (nm.empty() || !vol.open(img, err) || (vol.setJournal(img + ".undo"), !vol.dirOfPath(*x->bdir, false, dir, err)) ||
            !vol.makeDir(dir, nm, slot, err)) {
            pd_error(x, "eps~: mkdir: %s", nm.empty() ? "give a name" : err.c_str());
            return;
        }
        vol.close();
        scan_library(g_lib_dir);
        out_folder(x);
        return;
    } else if (is("init")) {
        set_defaults(x, true);
        x->source = kInit;
        apply(x);
        out_source(x);
    } else if (is("reload")) {
        if (x->source == kLibrary) select_library(x, x->index);
    } else if (is("presets")) {
        for (size_t i = 0; i < g_lib.size(); ++i) out_slot(x, "preset", static_cast<int>(i), g_lib[i].name);
        out_value(x, "presets", static_cast<t_float>(g_lib.size()));
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
    if (velocity > 0 && x->editor->follow) {
        const eps::Instrument& in = x->sampler->instrument();
        const int mask = in.patches[std::clamp(x->value[kC_patch], 0, 3)];
        if (x->editor->followKey(in, n, mask)) {
            out_sel(x);
            out_edump(x, "lmwepf");
        }
    }
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
            select_library(x, d1);
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
    if (x->sysex && (b == 0xF0 || m.in_sx)) x->sysex->receive(static_cast<uint8_t>(b), clock_gettimesince(0) / 1000.0);
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
    x->editor = new eps::Editor();
    x->sr = sys_getsr() > 0 ? sys_getsr() : 44100.0f;
    x->channel = 0;
    x->txchannel = 1;
    x->thru = 1;
    x->source = kNone;
    x->index = -1;
    x->bfile = new std::string();
    x->bdir = new std::string();
    x->bsel = 0;
    x->wheel = 0;
    x->bend = 0;
    x->ctl[0] = x->ctl[1] = x->ctl[2] = 0;
    x->sustain = false;
    set_defaults(x, false);
    for (int& v : x->shown) v = INT32_MIN;
    x->poll = clock_new(x, reinterpret_cast<t_method>(eps_poll));
    x->sxclock = clock_new(x, reinterpret_cast<t_method>(eps_sx_tick));
    x->epsch = 1;
    x->sxpct = -1;
    x->sysex = new eps::SysexLink([x](const std::vector<uint8_t>& p) {
        for (uint8_t b : p) outlet_float(x->info, b);
    });

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
    clock_free(x->sxclock);
    delete x->sysex;
    x->loader->quit.store(true);
    if (x->loader->thread.joinable()) x->loader->thread.join();
    delete x->loader;
    delete x->sampler;
    delete x->midi;
    delete x->entry;
    delete x->pending_entry;
    delete x->name;
    delete x->editor;
    delete x->bfile;
    delete x->bdir;
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
