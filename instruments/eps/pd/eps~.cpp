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
//   inlet 4   signal: the audio to sample (the wave recorder, see rec below)
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
// The wave recorder (SAMPLE on the EPS): the audio of inlet 4 becomes a wavesample.
//   rec rate n (0-6: 44.6 .. 11.2 kHz), rec ratestep d, rec thresh n (0-19, 0 = at once),
//   rec pre ms (0-127), rec dest n (0 new WS in the selected layer, 1 replace the selected
//   WS, 2 new layer, 3 new instrument), rec level 0/1 (level display), rec start, rec stop
//   (then the next key is the root key; stop again cancels), rec root n,
//   rec learn root|lo|hi (the next key sets it for the selected WS), rec input n (eps.i~:
//   Pd's audio input 0 none, 1 in 1 (default), 2 in 2, 3 both; inlet 2 always), rec status.
//   Outlet 3: recstate, recinfo, recmeter, recset ..., sampled <ws> <layer>.
// The wavesample commands (COMMAND/Wave, LFO, Amp of the EPS) on the selected wavesample:
//   cmd <name>|<n>, cmd step d, cmd range|depth|zone|sstart|send|rate|smooth|layer|data n,
//   cmd do, then cmd old / cmd new (KEEP=OLD NEW), cmd keep / cmd cancel, cmd status.
//   Outlet 3: cmdstate, cmdinfo, cmdset <key> <value> <text>.
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
#include <functional>
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
#include "eps_record.h"
#include "eps_wave.h"
#include "eps_surface.h"
#include "../../../common/gfx/lkgfx_pd.h"

#include <strings.h>

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
    // Instead of a file: build the instrument here (the wave recorder).
    std::function<bool(eps::Instrument&, std::string&)> make;
    bool made = false;                  // the result came from make
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
            std::function<bool(eps::Instrument&, std::string&)> job;
            {
                std::lock_guard<std::mutex> g(m);
                p = path;
                k = index;
                job.swap(make);
                set.sampleRate = sr;
                set.maxVoices = voices;
                // Several layers per key add up (CLASSIC PAD: a chord of three
                // reaches full scale at 1.0). volume 100 = this level.
                set.masterGain = kOutputGain;
            }
            std::unique_ptr<eps::Sampler> s;
            eps::Disk disk;
            if (job) {
                eps::Instrument inst;
                if (job(inst, err)) {
                    s.reset(new eps::Sampler(set));
                    if (!s->load(inst, err)) s.reset();
                    else nm = inst.name;
                }
            } else if (disk.open(p, err)) {
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
                made = static_cast<bool>(job);
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

enum Source { kNone = -1, kInit = 0, kLibrary = 1, kOpen = 3, kSampled = 4 };

// The wave recorder (as SAMPLE on the EPS): level display, threshold, pre-trigger,
// recording at the host rate, then PLAY ROOT KEY; the loader thread converts the
// recording to the EPS rate, puts it into the instrument and builds the new sampler.
struct Recorder {
    enum State { Off, Level, Armed, Recording, RootKey };
    int state = Off;
    int shownState = -1;          // what recstate said last
    int rate = 0;                 // 0..6: eps::kRecordRateIndex, 44.6 .. 11.2 kHz
    int thresh = 0;               // 0..19: the star of the level display, 0 = at once
    int pre = 15;                 // pre-trigger in ms (0..127)
    int dest = 0;                 // eps::RecordDest
    int input = 1;                // eps.i~: Pd's audio input 0 none, 1 in 1, 2 in 2, 3 both
    bool monitor = false;         // hear the input while level / armed / recording
    std::vector<float> mon;       // the input of one block (it may share its buffer with an output)
    int learn = 0;                // the next key sets 1 root, 2 low key, 3 high key
    bool full = false;            // the memory is full (set in the audio thread)
    std::vector<float> buf, ring; // recording and pre-trigger ring (host rate)
    size_t len = 0, ringPos = 0, preN = 0;
    float level = 0;              // threshold as amplitude
    float peak = 0;               // since the last meter
    float hold = 0;               // the meter's peak, falling
    float recSr = 44100.f;        // host rate of the recording
    bool waiting = false;         // a job waits for the loader
    std::function<bool(eps::Instrument&, std::string&)> job;
    // filled by the job (loader thread), read by eps_poll after it finished
    struct Result { int layer = 0, ws = 0; };
    std::shared_ptr<Result> result;
};

// What the Launchkey surface shows (eps_lk.inc): two big lines, the underlined part of the second.
struct LkScreen {
    std::string b1, b2;
    int ulFrom = -1, ulLen = 0;
    float db = -100.f;                   // the level on the SAMPLE page
};

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
    Recorder* rec;
    t_clock* recclock;                   // level display while sampling
    // the wavesample commands: what runs, and the instrument before (KEEP=OLD NEW)
    struct Wave {
        int cmd = 1;                     // eps::WaveCmd
        eps::WaveArgs args;
        eps::Sampler* other = nullptr;   // the one not playing: old while NEW plays, or new
        bool showingOld = false;
    }* wave;
    int jobkind;                         // what the loader's job is: 1 recording, 2 wave command
    lkg_pd_t* gfx;                       // Launchkey pictures: 1 envelope, 2 filter, 3 LFO
    int touchedOffset;                   // the envelope picture: 1 att / dec / rel, 2 fatt ... turned
    // the Launchkey surface (eps_lk.inc)
    eps::Surface* lk;
    bool lkOn;
    int padch;                           // channel of the pads
    int fbSent[8];                       // encoder values sent back (-1 = send again)
    int ledSent[16];                     // pad colours sent
    t_clock* lkclock;
    LkScreen* scr;
    double legendUntil;                  // the pad legend shows until then
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

// The text line for a display (the Launchkey text popup, the editor).
void out_display_text(t_eps* x, const std::string& title, const std::string& value) {
    t_atom a[2];
    SETSYMBOL(&a[0], gensym(title.c_str()));
    SETSYMBOL(&a[1], gensym(value.empty() ? "-" : value.c_str()));
    outlet_anything(x->info, gensym("display"), 2, a);
}

bool lk_text(t_eps* x, const std::string& title, const std::string& value);
bool lk_shown(t_eps* x, const char* name, const std::string& label, const std::string& value, int pic);
void lk_draw(t_eps* x, lkg_t* g, int kind);
void lk_refresh(t_eps* x, bool force);
void lk_show_home(t_eps* x);
void lk_meter(t_eps* x, float db);

// A text for the display: drawn by the Launchkey surface when it is on, else the text line.
void out_display(t_eps* x, const std::string& title, const std::string& value) {
    if (!lk_text(x, title, value)) out_display_text(x, title, value);
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

// ---- Launchkey pictures (common/gfx, custom firmware branch display-gfx) ---------
// 1 the envelope selected in the editor (att / dec / rel: the amp envelope with them),
// 2 the filters of the selected wavesample, 3 its LFO. "lkgfx 0" = the text line.

int gfx_kind(const char* n) {
    if (!std::strncmp(n, "e_", 2) || !std::strcmp(n, "att") || !std::strcmp(n, "dec") || !std::strcmp(n, "rel") ||
        !std::strcmp(n, "fatt") || !std::strcmp(n, "fdec") || !std::strcmp(n, "frel")) return 1;
    if (!std::strcmp(n, "w_fmode") || !std::strcmp(n, "w_cut1") || !std::strcmp(n, "w_cut2") ||
        !std::strcmp(n, "cut") || !std::strcmp(n, "cut2")) return 2;
    if (!std::strcmp(n, "w_lwave") || !std::strcmp(n, "w_lspeed") || !std::strcmp(n, "w_ldepth") ||
        !std::strcmp(n, "w_ldelay") || !std::strcmp(n, "w_lmode")) return 3;
    return 0;
}

void gfx_draw(void* owner, lkg_t* g, int kind) {
    t_eps* x = static_cast<t_eps*>(owner);
    if (kind >= 10) { lk_draw(x, g, kind); return; }
    if (!x->sampler) return;
    const eps::Instrument& in = x->sampler->instrument();
    auto it = in.waveSamples.find(x->editor->ws);
    if (it == in.waveSamples.end()) return;
    const eps::WaveSample& w = it->second;
    if (kind == 1) {
        // L0 -T1- L1 -T2- L2 -T3- L3 -T4- L4 (sustain) -T5- L5 -T6- 0; hard levels; times 0..99
        // grow steeply (width 2 + 40 x^1.7); att / dec / rel only on the amp envelope
        // touchedOffset: 1 the amp envelope with att / dec / rel, 2 the filter envelope with
        // fatt / fdec / frel
        const int env = x->touchedOffset == 1 ? 2 : (x->touchedOffset == 2 ? 1 : x->editor->env);
        const eps::Envelope& e = env == 2 ? w.ampEnv : (env == 0 ? w.pitchEnv : w.filterEnv);
        int t[7];
        for (int i = 1; i <= 6; ++i) t[i] = e.time[i];
        if (env >= 1) {
            const bool amp = env == 2;
            t[1] = std::clamp(t[1] + x->value[amp ? kC_att : kC_fatt], 0, 99);
            for (int i = 2; i <= 4; ++i) t[i] = std::clamp(t[i] + x->value[amp ? kC_dec : kC_fdec], 0, 99);
            for (int i = 5; i <= 6; ++i) t[i] = std::clamp(t[i] + x->value[amp ? kC_rel : kC_frel], 0, 99);
        }
        auto wd = [](int v) { return 2.0f + 40.0f * std::pow(v / 99.0f, 1.7f); };
        const float lvl[6] = {e.hardLevel[1] / 127.f, e.hardLevel[2] / 127.f, e.hardLevel[3] / 127.f,
                              e.hardLevel[4] / 127.f, std::max(0, e.level5) / 127.f, 0.f};
        const float ws[6] = {wd(t[1]), wd(t[2]), wd(t[3]), wd(t[4]), wd(t[5]), wd(t[6])};
        lkg_env(g, 6, e.hardLevel[0] / 127.f, lvl, ws, e.mode == 1 ? -1 : 3, 16.0f, nullptr);
    } else if (kind == 2) {
        // F1: 2 or 3 low-pass poles at cutoff 1, F2: 2 or 1 poles, low or high pass, at
        // cutoff 2 (filter mode); cutoff 0..150 along the axis (from ~132 open); no resonance
        const int mode = w.filterMode & 3;
        const int n1 = (mode & 1) ? 3 : 2, n2 = (mode & 1) ? 1 : 2;
        const bool hp = (mode & 2) == 0;
        const float f1 = std::clamp(w.filterCutoff[0] + x->value[kC_cut], 0, 150) / 140.f;
        const float f2 = std::clamp(w.filterCutoff[1] + x->value[kC_cut2], 0, 150) / 140.f;
        float v[LKG_W];
        for (int i = 0; i < LKG_W; ++i) {
            const float t = static_cast<float>(i) / (LKG_W - 1);
            const float r1 = std::pow(1000.f, t - f1), r2 = std::pow(1000.f, t - f2);
            float m = 1.f / std::sqrt(1.f + std::pow(r1, 2.f * n1));
            const float d2 = std::sqrt(1.f + std::pow(r2, 2.f * n2));
            m *= hp ? std::pow(r2, static_cast<float>(n2)) / d2 : 1.f / d2;
            v[i] = std::clamp(20.f * std::log10(std::max(m, 1e-6f)), -48.f, 6.f);
        }
        lkg_plot(g, v, LKG_W, -48.f, 6.f);
        lkg_hdots(g, 0, LKG_W - 1, static_cast<int>(lroundf(lkg_ly(48.f / 54.f))), 4);
    } else if (kind == 3) {
        // the LFO over time: speed sets the cycles shown, delay fades it in, depth the size;
        // +tri / +sine stay above the line
        const int wave = std::clamp(w.lfoWaveform, 0, 6);
        const float cycles = 1.f + w.lfoSpeed / 14.f, fade = 0.6f * w.lfoDelay / 99.f;
        const float depth = 0.15f + 0.85f * w.lfoDepth / 127.f;
        float v[LKG_W];
        for (int i = 0; i < LKG_W; ++i) {
            const float t = static_cast<float>(i) / (LKG_W - 1);
            const float ph = std::fmod(t * cycles, 1.f);
            const float tri = ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph;
            const float sine = std::sin(2.f * static_cast<float>(M_PI) * ph);
            float s = 0.f;
            switch (wave) {
                case 0: s = tri; break;
                case 1: s = 0.5f * (tri + sine); break;
                case 2: s = sine; break;
                case 3: s = 0.5f * (tri + 1.f); break;
                case 4: s = 0.5f * (sine + 1.f); break;
                case 5: s = 1.f - 2.f * ph; break;
                default: s = ph < 0.5f ? 1.f : -1.f; break;
            }
            const float env = fade > 0.f ? std::clamp(t / fade, 0.f, 1.f) : 1.f;
            v[i] = s * env * depth;
        }
        lkg_plot(g, v, LKG_W, -1.f, 1.f);
        lkg_hdots(g, 0, LKG_W - 1, static_cast<int>(lroundf(lkg_ly(0.5f))), 4);
    }
}

// A change for the display: the picture if it has one, else the text line.
void out_shown(t_eps* x, const char* name, const std::string& label, const std::string& value) {
    const int kind = gfx_kind(name);
    x->touchedOffset = (!std::strcmp(name, "att") || !std::strcmp(name, "dec") || !std::strcmp(name, "rel")) ? 1
                     : (!std::strcmp(name, "fatt") || !std::strcmp(name, "fdec") || !std::strcmp(name, "frel")) ? 2 : 0;
    if (lk_shown(x, name, label, value, kind)) return;
    if (!lkg_pd_show(x->gfx, kind, label.c_str(), value.c_str())) out_display_text(x, label, value);
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
    o.fAttack = v[kC_fatt];
    o.fDecay = v[kC_fdec];
    o.fRelease = v[kC_frel];
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
    } else if (x->source == kSampled) {
        out_display(x, "Sampled", nm);
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
void wave_settle(t_eps* x);

void request(t_eps* x, const LibEntry& e) {
    wave_settle(x);
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
        bool made = false;
        {
            std::lock_guard<std::mutex> g(L.m);
            s = std::move(L.result);
            err = L.error;
            nm = L.name;
            made = L.made;
        }
        L.state.store(0);
        if (!s && made && x->jobkind == 2) {
            pd_error(x, "eps~: %s: %s", eps::waveCmdName(static_cast<eps::WaveCmd>(x->wave->cmd)), err.c_str());
            out_display(x, eps::waveCmdName(static_cast<eps::WaveCmd>(x->wave->cmd)), err);
            t_atom e;
            SETSYMBOL(&e, gensym("idle"));
            outlet_anything(x->info, gensym("cmdstate"), 1, &e);
            SETSYMBOL(&e, gensym(spaces_to_underscore(err).c_str()));
            outlet_anything(x->info, gensym("cmdinfo"), 1, &e);
        } else if (!s && made) {
            pd_error(x, "eps~: sample: %s", err.c_str());
            out_display(x, "Not sampled", err);
        } else if (!s) {
            pd_error(x, "eps~: %s: %s", x->entry->file.c_str(), err.c_str());
            out_display(x, "Load failed", x->entry->name);
        } else {
            eps::Sampler* old = x->sampler;
            x->sampler = s.release();
            if (x->sampler->sampleRate() != x->sr) x->sampler->setSampleRate(x->sr);
            apply(x);
            if (old && made && x->jobkind == 2) {
                x->wave->other = old;            // KEEP=OLD NEW: the old one waits
                x->wave->showingOld = false;
            } else if (old) {
                std::lock_guard<std::mutex> g(L.m);
                L.trash.emplace_back(old);
            }
            *x->name = spaces_to_underscore(nm);
            if (!made) {
                // the patch select mode of the instrument: 0* *0 ** start in that patch,
                // LIVE and HELD in 00 (as the OS does after loading)
                const int psel = x->sampler->instrument().patchSelectMode;
                x->value[kC_patch] = psel >= 2 && psel <= 4 ? psel - 1 : 0;
                apply(x);
                report(x);
            }
            out_source(x);
            const eps::Instrument& in = x->sampler->instrument();
            if (made && x->rec->result && x->jobkind == 2) {
                x->editor->layer = x->rec->result->layer;
                if (x->rec->result->ws) x->editor->ws = x->rec->result->ws;
                t_atom r;
                SETSYMBOL(&r, gensym("new"));
                outlet_anything(x->info, gensym("cmdstate"), 1, &r);
                SETSYMBOL(&r, gensym("KEEP=OLD_NEW"));
                outlet_anything(x->info, gensym("cmdinfo"), 1, &r);
            } else if (made && x->rec->result) {
                // the new wavesample selected in the editor
                x->editor->layer = x->rec->result->layer;
                x->editor->ws = x->rec->result->ws;
                t_atom r[2];
                SETFLOAT(&r[0], x->rec->result->ws);
                SETFLOAT(&r[1], x->rec->result->layer + 1);
                outlet_anything(x->info, gensym("sampled"), 2, r);
            }
            t_atom a[3];
            SETSYMBOL(&a[0], gensym(in.modelName()));
            SETFLOAT(&a[1], static_cast<t_float>(in.layers.size()));
            SETFLOAT(&a[2], static_cast<t_float>(in.waveSamples.size()));
            outlet_anything(x->info, gensym("loaded"), 3, a);
            x->editor->fit(in);
            follow_entry(x);
            out_sel(x);
            out_edump(x, "ilmwepf");
            if (made && x->jobkind == 2)
                out_display(x, eps::waveCmdName(static_cast<eps::WaveCmd>(x->wave->cmd)), "KEEP=OLD NEW");
            else if (x->lkOn)
                lk_show_home(x);
            lk_refresh(x, false);
        }
    }
    if (x->pending && L.state.load() == 0) {
        x->pending = false;
        request(x, *x->pending_entry);
        return;
    }
    if (x->rec->waiting && L.state.load() == 0) {
        x->rec->waiting = false;
        {
            std::lock_guard<std::mutex> g(L.m);
            L.make = std::move(x->rec->job);
            L.sr = x->sr;
            L.voices = x->value[kC_voices];
        }
        L.state.store(1);
    }
    if (L.state.load() != 0 || x->pending || x->rec->waiting) clock_delay(x->poll, 5);
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
        w.filterEnv.time[1] = std::clamp(w.filterEnv.time[1] + v(kC_fatt), 0, 99);
        for (int i = 2; i <= 4; ++i) w.filterEnv.time[i] = std::clamp(w.filterEnv.time[i] + v(kC_fdec), 0, 99);
        for (int i = 5; i <= 6; ++i) w.filterEnv.time[i] = std::clamp(w.filterEnv.time[i] + v(kC_frel), 0, 99);
        w.fineTune = std::clamp(w.fineTune + static_cast<int>(std::lround(v(kC_tune) * 128.0 / 100.0)), -127, 127);
    }
    for (int c : {kC_transpose, kC_cut, kC_cut2, kC_fenv, kC_att, kC_dec, kC_rel, kC_fatt, kC_fdec, kC_frel, kC_tune})
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

// ---- the wave recorder ---------------------------------------------------------------

const char* const kDestNames[4] = {"new_WS", "replace_WS", "new_layer", "new_inst"};
const char* const kRecStates[5] = {"off", "level", "armed", "recording", "rootkey"};

int rec_rate_hz(const Recorder& R) { return eps::sampleRateOfIndex(eps::kRecordRateIndex[R.rate]); }

std::string rate_text(int hz) {
    char b[16];
    std::snprintf(b, sizeof b, "%.1fkHz", hz / 1000.0);
    return b;
}

// The threshold as amplitude: 0 = none, 1..19 = -54 .. 0 dB in 3 dB steps.
float rec_level(int t) { return t <= 0 ? 0.f : std::pow(10.f, (-57.f + 3.f * t) / 20.f); }

// Seconds of recording that fit into the memory left beside the instrument.
double rec_free_seconds(t_eps* x) {
    const Recorder& R = *x->rec;
    const bool fresh = R.dest == int(eps::RecordDest::NewInstrument) || !x->sampler;
    long words = fresh ? eps::kSampleMemoryWords : eps::freeWords(x->sampler->instrument());
    if (!fresh && R.dest == int(eps::RecordDest::Replace)) {
        auto it = x->sampler->instrument().waveSamples.find(x->editor->ws);
        if (it != x->sampler->instrument().waveSamples.end()) words += long(it->second.pcm.size());
    }
    return std::max(0.0, double(words - 300) / rec_rate_hz(R));
}

void out_recset(t_eps* x) {
    const Recorder& R = *x->rec;
    t_atom a[3];
    SETSYMBOL(&a[0], gensym("rate"));
    SETFLOAT(&a[1], R.rate);
    SETSYMBOL(&a[2], gensym(rate_text(rec_rate_hz(R)).c_str()));
    outlet_anything(x->info, gensym("recset"), 3, a);
    SETSYMBOL(&a[0], gensym("thresh"));
    SETFLOAT(&a[1], R.thresh);
    outlet_anything(x->info, gensym("recset"), 2, a);
    SETSYMBOL(&a[0], gensym("pre"));
    SETFLOAT(&a[1], R.pre);
    outlet_anything(x->info, gensym("recset"), 2, a);
    SETSYMBOL(&a[0], gensym("dest"));
    SETFLOAT(&a[1], R.dest);
    SETSYMBOL(&a[2], gensym(kDestNames[R.dest]));
    outlet_anything(x->info, gensym("recset"), 3, a);
    SETSYMBOL(&a[0], gensym("monitor"));
    SETFLOAT(&a[1], R.monitor ? 1 : 0);
    outlet_anything(x->info, gensym("recset"), 2, a);
    static const char* const kInputs[4] = {"inlet", "in_1", "in_2", "in_1+2"};
    SETSYMBOL(&a[0], gensym("input"));
    SETFLOAT(&a[1], R.input);
    SETSYMBOL(&a[2], gensym(kInputs[R.input]));
    outlet_anything(x->info, gensym("recset"), 3, a);
}

void out_recstate(t_eps* x, const std::string& info) {
    Recorder& R = *x->rec;
    t_atom a;
    if (R.state != R.shownState) {
        R.shownState = R.state;
        SETSYMBOL(&a, gensym(kRecStates[R.state]));
        outlet_anything(x->info, gensym("recstate"), 1, &a);
    }
    SETSYMBOL(&a, gensym(info.empty() ? "-" : info.c_str()));
    outlet_anything(x->info, gensym("recinfo"), 1, &a);
}

// What the recorder would do, for the display: "44.6kHz_new_WS_free_23.5s".
std::string rec_summary(t_eps* x) {
    const Recorder& R = *x->rec;
    char b[64];
    std::snprintf(b, sizeof b, "%s_%s_free_%.1fs", rate_text(rec_rate_hz(R)).c_str(), kDestNames[R.dest],
                  rec_free_seconds(x));
    return b;
}

// The level display of the EPS: 20 columns, '|' up to the peak, '*' the threshold.
void out_recmeter(t_eps* x) {
    Recorder& R = *x->rec;
    R.hold = std::max(R.peak, R.hold * 0.7f);
    R.peak = 0;
    const float db = R.hold > 0 ? 20.f * std::log10(R.hold) : -100.f;
    std::string m(20, '.');
    for (int c = 0; c < 20; ++c)
        if (db >= -57.f + 3.f * c) m[size_t(c)] = '|';
    if (R.thresh > 0) m[size_t(std::min(R.thresh, 19))] = '*';
    else m[0] = '*';
    t_atom a;
    SETSYMBOL(&a, gensym(m.c_str()));
    outlet_anything(x->info, gensym("recmeter"), 1, &a);
    lk_meter(x, db);
}

void rec_stop(t_eps* x);

void eps_rec_tick(t_eps* x) {
    Recorder& R = *x->rec;
    if (R.state == Recorder::Off || R.state == Recorder::RootKey) return;
    out_recmeter(x);
    if (R.state == Recorder::Recording) {
        char b[32];
        std::snprintf(b, sizeof b, "rec_%.1fs%s", R.len / R.recSr, R.full ? "_full" : "");
        out_recstate(x, b);
        if (R.full) {
            rec_stop(x);
            lk_refresh(x, false);
            return;
        }
    }
    clock_delay(x->recclock, 50);
}

// Level display (the SAMPLE button): the meter runs, nothing is recorded.
void rec_level_mode(t_eps* x, bool on) {
    Recorder& R = *x->rec;
    if (R.state == Recorder::Recording || R.state == Recorder::RootKey) return;
    R.state = on ? Recorder::Level : Recorder::Off;
    out_recstate(x, rec_summary(x));
    if (on) {
        out_display(x, "Sample", rate_text(rec_rate_hz(R)) + " " + kDestNames[R.dest]);
        clock_delay(x->recclock, 50);
    }
}

// ENTER: record now (threshold 0) or when the input crosses the threshold.
void rec_start(t_eps* x) {
    Recorder& R = *x->rec;
    if (R.state == Recorder::Recording || R.state == Recorder::Armed) return;
    if (R.dest == int(eps::RecordDest::Replace) &&
        (!x->sampler || !x->sampler->instrument().waveSamples.count(x->editor->ws))) {
        pd_error(x, "eps~: rec: no wavesample selected to replace");
        return;
    }
    if (R.dest == int(eps::RecordDest::NewLayer) && x->sampler && x->sampler->instrument().layers.size() >= 8) {
        pd_error(x, "eps~: rec: the instrument has 8 layers");
        return;
    }
    R.recSr = x->sr;
    const double secs = rec_free_seconds(x);
    if (secs < 0.05) { pd_error(x, "eps~: rec: no sample memory left"); out_display(x, "Memory", "full"); return; }
    R.buf.assign(size_t(secs * R.recSr), 0.f);
    R.ring.assign(size_t(0.128 * R.recSr) + 1, 0.f);
    R.preN = std::min(R.ring.size() - 1, size_t(R.pre * 0.001 * R.recSr));
    R.ringPos = 0;
    R.len = 0;
    R.full = false;
    R.level = rec_level(R.thresh);
    R.state = R.thresh > 0 ? Recorder::Armed : Recorder::Recording;
    out_recstate(x, R.state == Recorder::Armed ? "waiting_for_threshold" : "rec_0.0s");
    out_display(x, R.state == Recorder::Armed ? "Armed" : "Recording", rate_text(rec_rate_hz(R)));
    clock_delay(x->recclock, 50);
}

// CANCEL: stop the recording (then PLAY ROOT KEY), or leave the level display.
void rec_stop(t_eps* x) {
    Recorder& R = *x->rec;
    if (R.state == Recorder::Recording) {
        R.buf.resize(R.len);
        R.ring.clear();
        if (R.len < 16) {
            R.state = Recorder::Off;
            R.buf.clear();
            out_recstate(x, "nothing_recorded");
            return;
        }
        R.state = Recorder::RootKey;
        char b[48];
        std::snprintf(b, sizeof b, "%.2fs_PLAY_ROOT_KEY", R.len / R.recSr);
        out_recstate(x, b);
        out_display(x, "PLAY ROOT KEY", "");
        return;
    }
    if (R.state == Recorder::RootKey) R.buf.clear();
    R.state = Recorder::Off;
    R.buf.clear();
    R.ring.clear();
    out_recstate(x, "cancelled");
    out_display(x, "Sample", "cancelled");
}

// The root key: the recording goes into the instrument (in the loader thread).
void rec_place(t_eps* x, int root) {
    Recorder& R = *x->rec;
    if (R.state != Recorder::RootKey) return;
    wave_settle(x);
    x->jobkind = 1;
    R.state = Recorder::Off;
    eps::RecordDest dest = static_cast<eps::RecordDest>(R.dest);
    if (!x->sampler || x->sampler->instrument().layers.empty()) dest = eps::RecordDest::NewInstrument;
    auto base = std::make_shared<eps::Instrument>();
    if (dest != eps::RecordDest::NewInstrument) *base = x->sampler->instrument();
    auto rec = std::make_shared<std::vector<float>>(std::move(R.buf));
    R.buf.clear();
    auto res = std::make_shared<Recorder::Result>();
    R.result = res;
    const int layer = x->editor->layer, ws = x->editor->ws, idx = eps::kRecordRateIndex[R.rate];
    const double from = R.recSr, to = rec_rate_hz(R);
    R.job = [base, rec, res, dest, layer, ws, idx, from, to, root](eps::Instrument& out, std::string& err) {
        const std::vector<int16_t> pcm = eps::resampleRecording(*rec, from, to);
        int layerOut = 0;
        const int n = eps::placeRecording(*base, dest, layer, ws, pcm, idx, root, layerOut, err);
        if (!n) return false;
        std::vector<uint8_t> file;
        if (!eps::buildInstrument(*base, file, err)) return false;
        if (!eps::parseInstrument(file.data(), file.size(), base->name, out, err)) return false;
        res->layer = layerOut;
        res->ws = n;
        return true;
    };
    if (dest == eps::RecordDest::NewInstrument) {
        *x->entry = LibEntry();
        x->source = kSampled;
        x->index = -1;
    }
    R.waiting = true;
    out_recstate(x, "placing");
    clock_delay(x->poll, 5);
}

// A key played: the root key after a recording, or a key the editor waits for.
bool rec_key(t_eps* x, int note) {
    Recorder& R = *x->rec;
    if (R.state == Recorder::RootKey) {
        rec_place(x, note);
        return true;
    }
    if (!R.learn || !x->sampler) return false;
    eps::Instrument& in = x->sampler->editInstrument();
    auto it = in.waveSamples.find(x->editor->ws);
    const int what = R.learn;
    R.learn = 0;
    if (it == in.waveSamples.end()) return true;
    eps::WaveSample& w = it->second;
    if (what == 1) w.rootNote = note;
    else eps::setKeyRange(in, x->editor->layer, x->editor->ws, what == 2 ? note : w.keyLow, what == 3 ? note : w.keyHigh);
    out_sel(x);
    out_edump(x, "w");
    out_display(x, what == 1 ? "Root key" : (what == 2 ? "Key low" : "Key high"), std::to_string(note));
    return true;
}

void rec_command(t_eps* x, int argc, t_atom* argv) {
    Recorder& R = *x->rec;
    if (argc < 1 || argv[0].a_type != A_SYMBOL) return;
    const std::string w = atom_getsymbol(argv)->s_name;
    const int n = argc > 1 ? static_cast<int>(atom_getfloat(argv + 1)) : 0;
    const bool idle = R.state == Recorder::Off || R.state == Recorder::Level;
    if (w == "rate" || w == "ratestep") {
        if (!idle) return;
        R.rate = w == "rate" ? std::clamp(n, 0, 6) : std::clamp(R.rate + (argc > 1 ? n : 1), 0, 6);
    } else if (w == "thresh") {
        R.thresh = std::clamp(n, 0, 19);
        R.level = rec_level(R.thresh);
    } else if (w == "pre") {
        if (!idle) return;
        R.pre = std::clamp(n, 0, 127);
    } else if (w == "dest") {
        if (!idle) return;
        R.dest = std::clamp(n, 0, 3);
    } else if (w == "input") {
        R.input = std::clamp(n, 0, 3);
    } else if (w == "monitor") {
        R.monitor = n != 0;
    } else if (w == "level") {
        rec_level_mode(x, argc < 2 || n != 0);
        return;
    } else if (w == "start") {
        rec_start(x);
        return;
    } else if (w == "stop") {
        rec_stop(x);
        return;
    } else if (w == "root") {
        if (R.state == Recorder::RootKey && argc > 1) rec_place(x, std::clamp(n, 0, 127));
        return;
    } else if (w == "learn") {
        const std::string k = argc > 1 && argv[1].a_type == A_SYMBOL ? atom_getsymbol(argv + 1)->s_name : "";
        R.learn = k == "root" ? 1 : k == "lo" ? 2 : k == "hi" ? 3 : 0;
        if (R.learn) out_display(x, "Play a key", k == "root" ? "root key" : (k == "lo" ? "low key" : "high key"));
        return;
    } else if (w == "status") {
        R.shownState = -1;
    } else {
        pd_error(x, "eps~: rec: no method '%s'", w.c_str());
        return;
    }
    out_recset(x);
    if (idle) out_recstate(x, rec_summary(x));
    if (w == "rate") out_display(x, "Sample rate", rate_text(rec_rate_hz(R)));
    else if (w == "dest") out_display(x, "Sample into", kDestNames[R.dest]);
}

// ---- the wavesample commands (COMMAND/Wave, LFO, Amp pages) ---------------------------

const char* const kSmoothNames[5] = {"very_fine", "fine", "medium", "coarse", "very_coarse"};

void out_wavestate(t_eps* x, const char* state, const std::string& info) {
    t_atom a;
    SETSYMBOL(&a, gensym(state));
    outlet_anything(x->info, gensym("cmdstate"), 1, &a);
    SETSYMBOL(&a, gensym(info.empty() ? "-" : info.c_str()));
    outlet_anything(x->info, gensym("cmdinfo"), 1, &a);
}

void out_waveset(t_eps* x) {
    const t_eps::Wave& W = *x->wave;
    const eps::WaveArgs& A = W.args;
    auto one = [x](const char* k, int v, const std::string& text) {
        t_atom a[3];
        SETSYMBOL(&a[0], gensym(k));
        SETFLOAT(&a[1], v);
        SETSYMBOL(&a[2], gensym(text.empty() ? "-" : text.c_str()));
        outlet_anything(x->info, gensym("cmdset"), 3, a);
    };
    char b[16];
    std::snprintf(b, sizeof b, "%.1fdB", eps::scaleDepthDb(A.depth));
    one("cmd", W.cmd, eps::waveCmdName(static_cast<eps::WaveCmd>(W.cmd)));
    one("range", static_cast<int>(A.range), eps::waveRangeName(A.range));
    one("depth", A.depth, b);
    one("zone", A.zone, "");
    one("sstart", A.scaleStart, "");
    one("send", A.scaleEnd, "");
    one("rate", A.rate, rate_text(eps::sampleRateOfIndex(eps::kRecordRateIndex[A.rate])));
    one("smooth", A.smooth, kSmoothNames[A.smooth]);
    one("layer", A.toLayer + 1, "");
    one("data", A.withData ? 1 : 0, A.withData ? "params_&_data" : "params_only");
    one("ws", A.toWs, A.toWs ? "WS_" + std::to_string(A.toWs) : "-");
}

void trash_sampler(t_eps* x, eps::Sampler* s) {
    if (!s) return;
    std::lock_guard<std::mutex> g(x->loader->m);
    x->loader->trash.emplace_back(s);
}

// Swap what plays with the other one (KEEP=OLD NEW).
void wave_swap(t_eps* x) {
    t_eps::Wave& W = *x->wave;
    if (!W.other || !x->sampler) return;
    x->sampler->allNotesOff();
    std::swap(x->sampler, W.other);
    W.showingOld = !W.showingOld;
    if (x->sampler->sampleRate() != x->sr) x->sampler->setSampleRate(x->sr);
    apply(x);
    x->editor->fit(x->sampler->instrument());
    out_sel(x);
    out_edump(x, "ilmwepf");
}

// The audition ends: what plays now stays (an edit, a load, a save ...).
void wave_settle(t_eps* x) {
    t_eps::Wave& W = *x->wave;
    if (!W.other) return;
    trash_sampler(x, W.other);
    W.other = nullptr;
    W.showingOld = false;
    out_wavestate(x, "idle", "");
}

void wave_run(t_eps* x) {
    t_eps::Wave& W = *x->wave;
    Recorder& R = *x->rec;
    if (!x->sampler) { pd_error(x, "eps~: cmd: no instrument"); return; }
    if (R.waiting || x->loader->state.load() != 0) { pd_error(x, "eps~: cmd: busy"); return; }
    if (R.state != Recorder::Off && R.state != Recorder::Level) { pd_error(x, "eps~: cmd: the recorder is busy"); return; }
    wave_settle(x);
    auto base = std::make_shared<eps::Instrument>(x->sampler->instrument());
    auto res = std::make_shared<Recorder::Result>();
    R.result = res;
    const int layer = x->editor->layer, ws = x->editor->ws;
    const eps::WaveCmd cmd = static_cast<eps::WaveCmd>(W.cmd);
    const eps::WaveArgs args = W.args;
    R.job = [base, res, layer, ws, cmd, args](eps::Instrument& out, std::string& err) {
        int sel = ws, selLayer = layer;
        if (!eps::waveCommand(*base, layer, ws, cmd, args, sel, err, &selLayer)) return false;
        std::vector<uint8_t> file;
        if (!eps::buildInstrument(*base, file, err)) return false;
        if (!eps::parseInstrument(file.data(), file.size(), base->name, out, err)) return false;
        res->layer = cmd == eps::WaveCmd::CopyWs ? args.toLayer : selLayer;
        res->ws = sel;
        return true;
    };
    x->jobkind = 2;
    R.waiting = true;
    out_wavestate(x, "busy", eps::waveCmdName(cmd));
    out_display(x, eps::waveCmdName(cmd), "DATA BEING PROCESSED");
    clock_delay(x->poll, 5);
}

void wave_command(t_eps* x, int argc, t_atom* argv) {
    t_eps::Wave& W = *x->wave;
    eps::WaveArgs& A = W.args;
    if (argc < 1) return;
    const int cnt = static_cast<int>(eps::WaveCmd::Count);
    if (argv[0].a_type == A_FLOAT) {                     // cmd n: the command by number
        W.cmd = std::clamp(static_cast<int>(atom_getfloat(argv)), 0, cnt - 1);
        out_waveset(x);
        out_display(x, "Command", eps::waveCmdName(static_cast<eps::WaveCmd>(W.cmd)));
        return;
    }
    const std::string w = atom_getsymbol(argv)->s_name;
    const int n = argc > 1 ? static_cast<int>(atom_getfloat(argv + 1)) : 0;
    int named = -1;
    for (int i = 0; i < cnt; ++i)
        if (w == eps::waveCmdName(static_cast<eps::WaveCmd>(i))) named = i;
    if (named >= 0) {                                    // cmd crossfade: the command by name
        W.cmd = named;
    } else if (w == "step") {
        W.cmd = ((W.cmd + (argc > 1 ? n : 1)) % cnt + cnt) % cnt;
    } else if (w == "range") {
        A.range = static_cast<eps::WaveRange>(std::clamp(n, 0, 3));
    } else if (w == "depth") {
        A.depth = std::clamp(n, 0, 5);
    } else if (w == "zone") {
        A.zone = std::clamp(n, 1, 99);
    } else if (w == "sstart") {
        A.scaleStart = std::clamp(n, 0, 200);
    } else if (w == "send") {
        A.scaleEnd = std::clamp(n, 0, 200);
    } else if (w == "rate") {
        A.rate = std::clamp(n, 0, 6);
    } else if (w == "smooth") {
        A.smooth = std::clamp(n, 0, 4);
    } else if (w == "layer") {
        A.toLayer = std::clamp(n, 1, 8) - 1;
    } else if (w == "data") {
        A.withData = n != 0;
    } else if (w == "ws") {
        A.toWs = std::clamp(n, 0, 127);
    } else if (w == "do") {
        wave_run(x);
        return;
    } else if (w == "old" || w == "new") {
        if (W.other && W.showingOld != (w == "old")) wave_swap(x);
        if (W.other) {
            out_wavestate(x, W.showingOld ? "old" : "new", "KEEP=OLD_NEW");
            out_display(x, eps::waveCmdName(static_cast<eps::WaveCmd>(W.cmd)), "KEEP=OLD NEW");
        }
        return;
    } else if (w == "keep" || w == "cancel") {
        if (!W.other) return;
        if (W.showingOld == (w == "keep")) wave_swap(x);     // keep: NEW plays, cancel: OLD
        const bool kept = w == "keep";
        wave_settle(x);
        out_display(x, eps::waveCmdName(static_cast<eps::WaveCmd>(W.cmd)), kept ? "COMMAND COMPLETED" : "CANCELLED");
        return;
    } else if (w == "status") {
    } else {
        pd_error(x, "eps~: cmd: no command or setting '%s'", w.c_str());
        return;
    }
    out_waveset(x);
    if (named >= 0 || w == "step") out_display(x, "Command", eps::waveCmdName(static_cast<eps::WaveCmd>(W.cmd)));
}

// ---- the Launchkey surface -----------------------------------------------------------

#include "eps_lk.inc"

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

void eps_command_core(t_eps* x, t_symbol* s, int argc, t_atom* argv) {
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
        wave_settle(x);
        // e <name> <0-127>: change a parameter of the loaded instrument (editor)
        if (argc < 2 || argv[0].a_type != A_SYMBOL || !x->sampler) return;
        const int id = eps::Editor::find(atom_getsymbol(argv)->s_name);
        if (id < 0) { pd_error(x, "eps~: e: no parameter '%s'", atom_getsymbol(argv)->s_name); return; }
        x->editor->set127(x->sampler->editInstrument(), id, atom_getfloat(argv + 1));
        const char sc = eps::kEditParams[id].scope;
        if (sc == 'p' || sc == 'f') out_edump(x, "pf");   // the points limit each other
        else out_einfo(x, id);
        out_shown(x, eps::kEditParams[id].name, eps::kEditParams[id].label,
                  x->editor->text(x->sampler->editInstrument(), id));
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
    } else if (is("lkgfx")) {
        lkg_pd_on(x->gfx, n != 0);
        return;
    } else if (is("lk")) {
        lk_message(x, argc, argv);
        return;
    } else if (is("padchannel")) {
        x->padch = std::clamp(n, 1, 16);
        return;
    } else if (is("rec")) {
        rec_command(x, argc, argv);
        return;
    } else if (is("cmd")) {
        wave_command(x, argc, argv);
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
        wave_settle(x);
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
    if (touched) out_shown(x, touched->name, touched->label, value_text(*touched, x->value[control_index(touched)]));
}

void eps_command(t_eps* x, t_symbol* s, int argc, t_atom* argv) {
    eps_command_core(x, s, argc, argv);
    lk_refresh(x, false);
}

// follow key: the editor shows the layer and wavesample of the key played
void follow_note(t_eps* x, int n) {
    if (!x->sampler || !x->editor->follow) return;
    const eps::Instrument& in = x->sampler->instrument();
    const int mask = in.patches[std::clamp(x->value[kC_patch], 0, 3)];
    if (x->editor->followKey(in, n, mask)) {
        out_sel(x);
        out_edump(x, "lmwepf");
    }
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
    if (velocity > 0 && rec_key(x, n)) return;
    if (!x->sampler) return;
    if (velocity > 0) x->sampler->noteOn(n, std::clamp(static_cast<int>(velocity), 1, 127));
    else x->sampler->noteOff(n);
    if (velocity > 0) follow_note(x, n);
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
    const int type = status & 0xF0;
    // The Launchkey surface takes its pads and encoders first (they are not passed thru).
    if (x->lkOn && (status & 0x0F) + 1 == x->padch && x->padch != lk_channel(x)) {   // the pads
        if ((type == 0x90 || type == 0x80) && d1 >= 36 && d1 <= 51) lk_pad(x, d1, type == 0x90 && d2 > 0);
        else if (type == 0xB0 && d1 == 3) lk_refresh(x, true);   // Drum mode entered: the colours
        return;
    }
    const bool mine = !x->channel || (status & 0x0F) + 1 == x->channel;
    if (x->lkOn && mine && type == 0xB0 && d1 == 3) {          // the Launchkey asks for the values
        lk_refresh(x, true);
        return;
    }
    if (x->lkOn && mine && type == 0xB0 && lk_encoder(x, d1, d2)) return;
    if (x->thru) {
        outlet_float(x->info, status);
        outlet_float(x->info, d1);
        const int t = status & 0xF0;
        if (t != 0xC0 && t != 0xD0) outlet_float(x->info, d2);
    }
    if (x->channel && (status & 0x0F) + 1 != x->channel) return;
    eps::Sampler* s = x->sampler;
    if (x->lkOn && type == 0x90 && d2 > 0 && x->lk->selHeld) {   // SEL + key: select, silently
        x->lk->keyUsed();
        if (s && x->editor->followKey(s->instrument(), d1, s->instrument().patches[std::clamp(x->value[kC_patch], 0, 3)])) {
            out_sel(x);
            out_edump(x, "lmwepf");
        }
        lk_show_home(x);
        return;
    }
    switch (status & 0xF0) {
        case 0x90:
            if (d2 > 0 && rec_key(x, d1)) return;
            if (!s) return;
            if (d2 > 0) s->noteOn(d1, d2);
            else s->noteOff(d1);
            if (d2 > 0) follow_note(x, d1);
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
    lk_refresh(x, false);
}

void eps_proxy_list(t_eps_proxy* p, t_symbol*, int argc, t_atom* argv) {
    for (int i = 0; i < argc; ++i) eps_proxy_float(p, atom_getfloat(argv + i));
}

// ---- dsp ------------------------------------------------------------------------

// The recorder's part of the audio: meter, threshold and pre-trigger, recording.
// Before the sampler writes: the input may share its buffer with an output.
void record_block(Recorder& R, const t_sample* in, int n) {
    if (R.state < Recorder::Level || R.state > Recorder::Recording) return;
    float pk = R.peak;
    for (int i = 0; i < n; ++i) {
        const float v = in[i];
        const float a = std::fabs(v);
        if (a > pk) pk = a;
        if (R.state == Recorder::Armed) {
            R.ring[R.ringPos] = v;
            R.ringPos = (R.ringPos + 1) % R.ring.size();
            if (a >= R.level) {
                // start: the last pre-trigger samples first (this one included)
                const size_t m = std::min(R.preN + 1, R.buf.size());
                const size_t sz = R.ring.size();
                for (size_t k = 0; k < m; ++k) R.buf[k] = R.ring[(R.ringPos + sz - m + k) % sz];
                R.len = m;
                R.state = Recorder::Recording;
            }
        } else if (R.state == Recorder::Recording) {
            if (R.len < R.buf.size()) R.buf[R.len++] = v;
            else R.full = true;
        }
    }
    R.peak = pk;
}

t_int* eps_perform(t_int* w) {
    t_eps* x = reinterpret_cast<t_eps*>(w[1]);
    t_sample* l = reinterpret_cast<t_sample*>(w[3]);
    t_sample* r = reinterpret_cast<t_sample*>(w[4]);
    const int n = static_cast<int>(w[5]);
    Recorder& R = *x->rec;
    const t_sample* in = reinterpret_cast<t_sample*>(w[2]);
    const bool mon = R.monitor && R.state >= Recorder::Level && R.state <= Recorder::Recording &&
                     static_cast<int>(R.mon.size()) >= n;
    if (mon) std::copy(in, in + n, R.mon.begin());
    record_block(R, in, n);
    if (x->sampler) {
        x->sampler->process(l, r, n);
    } else {
        std::fill(l, l + n, 0.0f);
        std::fill(r, r + n, 0.0f);
    }
    if (mon)
        for (int i = 0; i < n; ++i) { l[i] += R.mon[size_t(i)]; r[i] += R.mon[size_t(i)]; }
    return w + 6;
}

void eps_dsp(t_eps* x, t_signal** sp) {
    x->sr = sp[0]->s_sr;
    x->rec->mon.assign(static_cast<size_t>(sp[0]->s_n), 0.f);
    if (x->sampler) x->sampler->setSampleRate(x->sr);
    // sp[0]: the audio to sample (inlet 4), sp[1], sp[2]: left, right
    dsp_add(eps_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, static_cast<t_int>(sp[0]->s_n));
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
    x->rec = new Recorder();
    x->wave = new t_eps::Wave();
    x->jobkind = 0;
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
    x->recclock = clock_new(x, reinterpret_cast<t_method>(eps_rec_tick));
    x->epsch = 1;
    x->sxpct = -1;
    x->sysex = new eps::SysexLink([x](const std::vector<uint8_t>& p) {
        for (uint8_t b : p) outlet_float(x->info, b);
    });

    floatinlet_new(&x->obj, &x->velocity);
    x->proxy.pd = eps_proxy_class;
    x->proxy.owner = x;
    inlet_new(&x->obj, &x->proxy.pd, nullptr, nullptr);
    inlet_new(&x->obj, &x->obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->obj, &s_signal);
    outlet_new(&x->obj, &s_signal);
    x->info = outlet_new(&x->obj, nullptr);
    x->gfx = new lkg_pd_t();
    lkg_pd_init(x->gfx, x->info, gfx_draw, x);
    x->touchedOffset = 0;
    x->lk = new eps::Surface();
    x->lkOn = false;
    x->padch = 10;
    for (int& v : x->fbSent) v = -1;
    for (int& v : x->ledSent) v = -1;
    x->lkclock = clock_new(x, reinterpret_cast<t_method>(eps_lk_tick));
    x->scr = new LkScreen();
    x->legendUntil = 0;

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
    clock_free(x->recclock);
    lkg_pd_free(x->gfx);
    delete x->gfx;
    clock_free(x->lkclock);
    delete x->lk;
    delete x->scr;
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
    delete x->rec;
    delete x->wave->other;
    delete x->wave;
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
