// eps_sampler.cpp — see eps_sampler.h. Values marked CALIBRATE are first
// guesses that should be replaced by tables measured on the original OS.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#endif
#include "eps_sampler.h"
#include "eps_tables.h"

#include <cmath>
#include <cstring>

namespace eps {

namespace {

using namespace tables;

float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Volume code (12 bit) of a level 0..127, fractional levels interpolated.
float levelCode(float level) {
    level = clampf(level, 0.f, 127.f);
    const int i = int(level);
    if (i >= 127) return float(kVolumeCode[127]);
    const float f = level - float(i);
    return float(kVolumeCode[i]) + f * float(kVolumeCode[i + 1] - kVolumeCode[i]);
}

// LFO speed 0..99 -> Hz, measured (periods of the original OS); 17..19 from
// ConvertWithMoss, the slow ones between the measured points log-interpolated.
float lfoHz(int speed) {
    static const float kHz[100] = {
        0.0528f, 0.0567f, 0.0607f, 0.0646f, 0.0687f, 0.0752f, 0.0820f, 0.0894f, 0.0975f, 0.1063f,
        0.1159f, 0.1263f, 0.1375f, 0.1627f, 0.1925f, 0.2278f, 0.2705f, 0.33f, 0.50f, 1.0f,
        1.236f, 1.486f, 1.736f, 1.985f, 2.234f, 2.481f, 2.728f, 2.975f, 3.222f, 3.472f,
        3.722f, 3.971f, 4.221f, 4.466f, 4.711f, 4.955f, 5.200f, 5.449f, 5.698f, 5.947f,
        6.196f, 6.448f, 6.700f, 6.953f, 7.205f, 7.436f, 7.667f, 7.899f, 8.130f, 8.380f,
        8.630f, 8.880f, 9.130f, 9.375f, 9.620f, 9.865f, 10.11f, 10.36f, 10.61f, 10.86f,
        11.11f, 11.35f, 11.59f, 11.82f, 12.06f, 12.31f, 12.57f, 12.82f, 13.07f, 13.33f,
        13.58f, 13.84f, 14.10f, 14.35f, 14.59f, 14.84f, 15.08f, 15.33f, 15.58f, 15.83f,
        16.08f, 16.32f, 16.56f, 16.80f, 17.04f, 17.29f, 17.53f, 17.78f, 18.02f, 18.28f,
        18.54f, 18.79f, 19.05f, 19.50f, 19.94f, 20.39f, 20.83f, 24.24f, 27.65f, 31.06f};
    return kHz[clampi(speed, 0, 99)];
}

// The 7 LFO waves at phase 0..1: triangle, sine/triangle, sine, positive
// triangle, positive sine, rising sawtooth, positive square.
float lfoWave(int wave, float ph) {
    const float tri = ph < 0.25f ? 4.f * ph : (ph < 0.75f ? 2.f - 4.f * ph : 4.f * ph - 4.f);
    const float sn = std::sin(2.f * float(M_PI) * ph);
    switch (wave) {
        case 1: return 0.5f * (tri + sn);
        case 2: return sn;
        case 3: return 0.5f * (tri + 1.f);
        case 4: return 0.5f * (sn + 1.f);
        case 5: return 2.f * ph - 1.f;
        case 6: return ph < 0.5f ? 1.f : 0.f;
        default: return tri;
    }
}

// Random pitch: a new value about 1.07 times a second at freq 50, 2.9 at 99
// (measured); between and below exponential (CALIBRATE: only those two points).
float noiseHz(int freq) { return 1.07f * std::exp2((float(clampi(freq, 0, 99)) - 50.f) / 34.4f); }

// Velocity curves of the sources 5 (concave) and 7 (convex), measured at
// 0, 32, 64, 96, 127.
float velCurve(float v, const float *pts) {
    v = clampf(v, 0.f, 1.f) * 4.f;
    const int i = v >= 4.f ? 3 : int(v);
    return pts[i] + (v - float(i)) * (pts[i + 1] - pts[i]);
}
const float kVel2[5] = {0.f, 0.48f, 0.68f, 0.84f, 1.f};
const float kVel3[5] = {0.f, 0.155f, 0.32f, 0.52f, 1.f};

// Crossfade window over a source value 0..127: in from A to B, full to C, out to D.
float crossfade(float x, const int *abcd) {
    const float A = float(abcd[0] & 0x7f), B = float(abcd[1] & 0x7f), C = float(abcd[2] & 0x7f),
                D = float(abcd[3] & 0x7f);
    if (x < A) return 0.f;
    if (x < B) return B > A ? (x - A) / (B - A) : 1.f;
    if (x <= C) return 1.f;
    if (x <= D) return D > C ? (D - x) / (D - C) : 0.f;
    return 0.f;
}

} // namespace

float envelopeSeconds(int v) { return kEnvMs[v < 0 ? 0 : (v > 99 ? 99 : v)] * 0.001f; }

const char *modSourceName(int src) {
    static const char *const kNames[kSrcCount] = {"LFO", "ENV1", "ENV2", "ENV3", "VEL", "VEL2", "PR+VL",
                                                  "VEL3", "KBD", "KBD2", "WHEEL", "PEDAL", "XCTRL",
                                                  "PRESS", "WHL+PR", "PITCH"};
    return src >= 0 && src < kSrcCount ? kNames[src] : "?";
}

// ---------------------------------------------------------------- envelope

// Every segment takes its time, whatever the distance. The amp envelope moves
// in the volume code domain: falling linearly (linear in dB), rising in two
// halves, the first up to kAttackKneeCode below the target, the second the
// rest (measured on the original OS).
void Sampler::Env::enter(int st, float targetLevel, float seconds) {
    stage = st;
    from = value;
    linFrom = lin;
    linTo = clampf(targetLevel, 0.f, 127.f);
    to = code ? (linTo <= 0.f ? float(kVolumeCode[0]) : levelCode(linTo)) : linTo;
    dur = seconds;
    t = 0.f;
    silent = false;
}

void Sampler::Env::start() {
    lin = level[0];
    value = code ? levelCode(level[0]) : level[0];
    enter(1, level[1], time[1]);
}

void Sampler::Env::release_(bool key) {
    if (stage >= 6) return;
    if (key && mode == 1) return;   // FINITE: the key release is ignored (measured)
    // First stage: the level moves by L5 within T5 (L5 0 = it holds), then T6 to
    // silence. The amp envelope moves by L5 / 127 of the whole code range
    // (measured), the others by L5 levels.
    enter(6, lin + relLevel, relSec[0]);
    if (code) {
        const float range = float(kVolumeCode[127] - kVolumeCode[0]);
        to = clampf(from + relLevel / 127.f * range, float(kVolumeCode[0]), float(kVolumeCode[127]));
    }
}

void Sampler::Env::tick(float dt) {
    if (stage < 1 || stage == 5 || stage >= 8) return;
    t += dt;
    const float f = dur <= 0.f ? 1.f : clampf(t / dur, 0.f, 1.f);
    lin = linFrom + (linTo - linFrom) * f;
    if (code && to > from && to - float(kAttackKneeCode) > from) {
        const float knee = to - float(kAttackKneeCode);
        value = f < 0.5f ? from + (knee - from) * f * 2.f : knee + (to - knee) * (f - 0.5f) * 2.f;
    } else {
        value = from + (to - from) * f;
    }
    if (f < 1.f) return;
    if (code && linTo <= 0.f) silent = true;
    if (stage >= 1 && stage < 4) { enter(stage + 1, level[stage + 1], time[stage + 1]); return; }
    if (stage == 4) {
        // Mode 1 (finite) releases by itself after time 4 (measured); CALIBRATE:
        // mode 2 assumed to repeat while the key is held.
        if (mode == 1) release_(false);
        else if (mode == 2) enter(1, level[1], time[1]);
        else stage = 5;
        return;
    }
    if (stage == 6) { enter(7, 0.f, relSec[1]); return; }
    if (stage == 7) { stage = 8; silent = code; }
}

// ---------------------------------------------------------------- sampler

Sampler::Sampler(const Settings &s) : s_(s) {
    if (s_.maxVoices < 1) s_.maxVoices = 1;
    if (s_.maxVoices > otis::ES5505::kVoices) s_.maxVoices = otis::ES5505::kVoices;
    chip_.writeReg(0, otis::ACT, uint16_t(s_.maxVoices - 1));
    buildFilterTables();
}

bool Sampler::load(const Instrument &inst, std::string &err) {
    allNotesOff();
    for (auto &sl : slots_) sl = Slot();
    loaded_ = false;
    inst_ = inst;

    // Place every PCM owner in bank 0 or, if full, bank 1 (1M words each).
    // Two extra words of padding after each sample keep interpolation clean.
    constexpr uint32_t kBankWords = 1u << 20;
    uint32_t used[2] = {0, 0};
    std::vector<int> bankOf(128, -1);
    wsBase_.assign(128, 0);
    for (auto &kv : inst_.waveSamples) {
        const WaveSample &ws = kv.second;
        if (ws.copyOfWaveSample || ws.pcm.empty()) continue;
        uint32_t need = uint32_t(ws.pcm.size()) + 2;
        int bank = used[0] + need <= kBankWords ? 0 : (used[1] + need <= kBankWords ? 1 : -1);
        if (bank < 0) { err = "instrument exceeds 2M words of sample memory"; return false; }
        bankOf[ws.index] = bank;
        wsBase_[ws.index] = used[bank];
        used[bank] += need;
    }
    ram_.assign(size_t(used[0]) + used[1], 0);
    for (auto &kv : inst_.waveSamples) {
        const WaveSample &ws = kv.second;
        if (bankOf[ws.index] < 0) continue;
        size_t off = (bankOf[ws.index] ? used[0] : 0) + wsBase_[ws.index];
        std::memcpy(&ram_[off], ws.pcm.data(), ws.pcm.size() * sizeof(int16_t));
    }
    chip_.reset();
    chip_.writeReg(0, otis::ACT, uint16_t(s_.maxVoices - 1));
    chip_.setMemory(0, ram_.data(), used[0]);
    chip_.setMemory(1, ram_.data() + used[0], used[1]);
    // Remember the bank in the top bit of the base (bit 31 never used by addresses).
    for (int i = 0; i < 128; ++i)
        if (bankOf[i] == 1) wsBase_[i] |= 0x80000000u;
    loaded_ = true;
    return true;
}

int Sampler::allocate() {
    int best = -1;
    uint32_t bestAge = UINT32_MAX;
    // 1) free voice, 2) oldest released, 3) oldest held
    for (int pass = 0; pass < 3 && best < 0; ++pass) {
        for (int v = 0; v < s_.maxVoices; ++v) {
            const Slot &sl = slots_[v];
            bool candidate = pass == 0 ? !sl.active : (pass == 1 ? !sl.held : true);
            if (candidate && (pass == 0 || sl.age < bestAge)) {
                best = v;
                bestAge = sl.age;
                if (pass == 0) break;
            }
        }
    }
    return best;
}

// Cutoff 0..150 -> K, from the tables measured at the chip rate of the EPS
// (29.76 kHz), converted to the host rate so the corner frequencies stay:
// low-pass pole 1 - K/4096, high-pass pole 1/2 + K/8192, each raised to the
// power chip rate / host rate. Built per quarter cutoff unit.
void Sampler::buildFilterTables() {
    const double ratio = double(kChipRate) / double(s_.sampleRate);
    for (int i = 0; i <= 600; ++i) {
        const float c = float(i) * 0.25f;
        const int j = c >= 150.f ? 149 : int(c);
        const float f = c - float(j);
        const double lp = double(kLowpassK[j]) + f * double(kLowpassK[j + 1] - kLowpassK[j]);
        const double hp = double(kHighpassK[j]) + f * double(kHighpassK[j + 1] - kHighpassK[j]);
        const double lpPole = std::pow(1.0 - lp / 4096.0, ratio);
        const double hpPole = std::pow(0.5 + hp / 8192.0, ratio);
        lpK_[i] = uint16_t(clampi(int(std::lround((1.0 - lpPole) * 4096.0)), 0, 4095) << 4);
        hpK_[i] = uint16_t(clampi(int(std::lround((hpPole - 0.5) * 8192.0)), 0, 4095) << 4);
    }
}

uint16_t Sampler::cutoffToK(float c) const {
    return lpK_[clampi(int(std::lround(clampf(c, 0.f, 150.f) * 4.f)), 0, 600)];
}

uint16_t Sampler::cutoffToKHighpass(float c) const {
    return hpK_[clampi(int(std::lround(clampf(c, 0.f, 150.f) * 4.f)), 0, 600)];
}

void Sampler::startVoice(const WaveSample &ws, int note, int velocity, bool held, int layer) {
    const WaveSample *owner = inst_.pcmOwner(ws);
    if (!owner || owner->pcm.empty()) return;
    const int key = note + inst_.transpose + off_.transpose;

    // Layer glide: the layer plays one note at a time; a new note takes over
    // from the one before and glides from its pitch. CALIBRATE: the EPS glide
    // modes (legato only, always, ...) are not told apart yet, every mode but
    // 0 is mono with glide.
    float glideFrom = 0.f, glideSec = 0.f;
    if (layer >= 0 && layer < int(inst_.layers.size()) && inst_.layers[size_t(layer)].glideMode != 0) {
        const Layer &l = inst_.layers[size_t(layer)];
        for (int u = 0; u < s_.maxVoices; ++u) {
            Slot &o = slots_[u];
            if (!o.active || o.layer != layer) continue;
            glideFrom = float(o.key - key) + o.glide;
            chip_.writeReg(u, otis::CR, otis::kStop0);
            o.active = false;
        }
        glideSec = envelopeSeconds(l.glideTime);
        if (glideSec <= 0.f) glideFrom = 0.f;
    }

    int v = allocate();
    if (v < 0) return;
    Slot &sl = slots_[v];
    sl = Slot();

    const uint32_t baseRaw = wsBase_[owner->index];
    const bool bank1 = baseRaw & 0x80000000u;
    const uint32_t base = baseRaw & 0x7fffffffu;
    const uint32_t last = uint32_t(owner->pcm.size()) - 1;
    auto clampPos = [&](uint32_t p) { return p > last ? last : p; };
    uint32_t sStart = clampPos(ws.sampleStart), sEnd = clampPos(ws.sampleEnd);
    uint32_t lStart = clampPos(ws.loopStart), lEnd = clampPos(ws.loopEnd);
    if (sEnd <= sStart) sEnd = last;
    int mode = ws.loopMode;
    if (mode >= 2 && (lEnd <= lStart || lEnd > sEnd)) mode = 0;   // invalid loop -> one-shot
    auto a29 = [&](uint32_t p) { return (base + p) << 9; };

    // The voice's own values first: the mod sources below read them.
    sl.active = true;
    sl.held = held;
    sl.note = note;
    sl.key = key;
    sl.layer = layer;
    sl.vel = clampf(float(velocity) / 127.f, 0.f, 1.f);
    sl.age = ++clock_;
    sl.ws = &ws;

    // Envelopes: velocity interpolates between soft and hard levels.
    // CALIBRATE: the OS interpolates differently (seen in the emulator: hard
    // levels below velocity 72); linear here until that is understood.
    auto setupEnv = [&](Env &e, const Envelope &src, bool amp) {
        e = Env();
        e.code = amp;
        for (int i = 0; i < 5; ++i)
            e.level[i] = float(src.softLevel[i]) + float(src.hardLevel[i] - src.softLevel[i]) * sl.vel;
        // Times in panel steps: attack time velocity shortens time 1 by
        // velocity * amount / 128, keyboard time scaling all times by
        // (key - root) * amount / 64 (both measured on time 1; CALIBRATE: the
        // other times assumed to scale alike).
        const float kb = float(src.kbTimeScaling) * float(key - ws.rootNote) / 64.f;
        auto steps = [&](int panel, float extra) {
            return envelopeSeconds(int(std::lround(clampf(float(panel) - kb - extra, 0.f, 99.f))));
        };
        for (int i = 1; i <= 4; ++i) {
            const int o = !amp ? 0 : (i == 1 ? off_.attack : off_.decay);
            const float velShort = i == 1 ? float(src.time1VelSens) * float(velocity) / 128.f : 0.f;
            e.time[i] = steps(src.time[i] + o, velShort);
        }
        e.releasePanel[0] = src.time[5] - int(std::lround(kb));
        e.releasePanel[1] = src.time[6] - int(std::lround(kb));
        e.relSec[0] = envelopeSeconds(e.releasePanel[0]);
        e.relSec[1] = envelopeSeconds(e.releasePanel[1]);
        e.relLevel = float(src.level5);
        e.mode = src.mode;
        e.start();
    };
    setupEnv(sl.amp, ws.ampEnv, true);
    setupEnv(sl.filt, ws.filterEnv, false);
    setupEnv(sl.pit, ws.pitchEnv, false);

    // LFO: mode 1 restarts with every key, 0 runs on (a phase of its own per
    // voice), 2 humanizes the speed. Noise starts somewhere.
    sl.lfoPhase = ws.lfoMode == 1 ? 0.f : float(nextRandom() & 0xffff) / 65536.f;
    sl.lfoRateMul = ws.lfoMode == 2 ? 0.85f + 0.3f * float(nextRandom() & 0xffff) / 65536.f : 1.f;
    sl.noise = float(int32_t(nextRandom())) / 2147483648.f;
    sl.glide = glideFrom;
    sl.glideRate = glideSec > 0.f ? std::fabs(glideFrom) / glideSec : 0.f;

    uint16_t ctrl = uint16_t(((ws.filterMode & 3) << 10) | (bank1 ? otis::kBank : 0));
    uint32_t st, en, acc;
    switch (mode) {
        case 1:  st = a29(sStart); en = a29(sEnd); acc = en; ctrl |= otis::kDir; break;
        case 2:
        case 4:  st = a29(lStart); en = a29(lEnd); acc = a29(sStart); ctrl |= otis::kLpe; break;
        case 3:  st = a29(lStart); en = a29(lEnd); acc = a29(sStart); ctrl |= otis::kLpe | otis::kBle; break;
        case 0:  st = a29(sStart); en = a29(sEnd); acc = st; break;
        default: st = a29(lStart); en = a29(lEnd); acc = a29(sStart); ctrl |= otis::kLpe; break;
    }

    // Pitch: sample rate relative to the chip rate, plus key offset.
    const double semis = double(key - ws.rootNote + s_.pitchOffset) + double(ws.fineTune) / 128.0;
    sl.baseRatio = double(owner->sampleRate) / s_.sampleRate * std::pow(2.0, semis / 12.0);

    // Filter: static cutoff + key tracking; envelopes and modulation in updateControl.
    // Key tracking: (key - root) * amount / 64 cutoff units (measured).
    sl.cutoff = clampi(ws.filterCutoff[0] + ws.filterKeyAmount[0] * (key - ws.rootNote) / 64, 0, 150);
    sl.cutoff2 = clampi(ws.filterCutoff[1] + ws.filterKeyAmount[1] * (key - ws.rootNote) / 64, 0, 150);
    sl.filterEnvAmount = ws.filterEnvAmount[0];

    // Pan: EPS uses 8 positions, EPS-16 PLUS / ASR a signed value (-128..127).
    if (inst_.instrumentId == kIdEps) {
        if (ws.panEps >= 1 && ws.panEps <= 8) sl.pan = (float(ws.panEps - 1) / 7.f) * 255.f - 128.f;
    } else {
        sl.pan = float(ws.panSigned);
    }
    // Volume: the level table, relative to full level; BOOST +12 dB.
    sl.volCode = levelCode(float(ws.volume)) - float(kVolumeCode[127]) + (ws.boost ? float(kBoostCode) : 0.f);
    // CALIBRATE: the byte before the pitch envelope amount switches the amp
    // modulation to a crossfade (1, seen in DECAY BASS with KBD and 0/0/64/127).
    sl.xfade = ws.volumeModCrossfadeCurve == 1;

    sl.base = base;
    sl.sampleStart = sStart;
    sl.loopStart = lStart;
    sl.loopEnd = lEnd;
    sl.sampleEnd = sEnd;
    sl.waveModType = -1;
    if (ws.waveModAmount() != 0 && ws.waveModType() >= 0 && ws.waveModType() <= 6) {
        const int t = ws.waveModType();
        const bool needsLoop = t != 1;
        if (!needsLoop || mode == 2 || mode == 3 || mode == 4) {
            sl.waveModType = t;
            sl.waveModSource = ws.waveModSource() & 15;
            sl.waveModDepth = float(ws.waveModAmount()) / 127.f;
            sl.waveModRange = ws.waveModRangeSamples();
        }
    }
    // Sample start (types 1 and 2): the note starts later in the sample, set once.
    if ((sl.waveModType == 1 || sl.waveModType == 2) && mode != 1) {
        const float m = clampf(waveModValue(sl), 0.f, 1.f);
        const uint32_t limit = (mode >= 2 ? lStart : sEnd) > sStart + 2 ? (mode >= 2 ? lStart : sEnd) - 2 : sStart;
        uint32_t s2 = sStart + uint32_t(std::lround(double(m) * sl.waveModRange));
        if (s2 > limit) s2 = limit;
        acc = a29(s2);
        if (mode == 0) st = acc;
    }

    sl.loopUntilRelease = mode == 4;
    sl.releaseEnd29 = a29(sEnd);

    chip_.writeReg(v, otis::CR, uint16_t(ctrl | otis::kStop0));   // stop while programming
    chip_.setAddresses(v, st, en, acc);
    chip_.writeReg(v, otis::LVOL, 0);
    chip_.writeReg(v, otis::RVOL, 0);
    updateControl(v, 0.f);                // filters, pitch, levels
    chip_.writeReg(v, otis::CR, ctrl);    // go
}

void Sampler::noteOn(int note, int velocity) {
    if (!loaded_) return;
    if (velocity <= 0) { noteOff(note); return; }
    if (note < inst_.keyLow || note > inst_.keyHigh) return;
    int mask = inst_.patches[patch_] & inst_.keyDownLayers;
    if (inst_.patches[patch_] == 0) mask = inst_.keyDownLayers ? inst_.keyDownLayers : 0xff;
    const int key = note + inst_.transpose + off_.transpose;
    for (size_t i = 0; i < inst_.layers.size(); ++i) {
        if (!(mask & (1 << i))) continue;
        const Layer &l = inst_.layers[i];
        if (velocity < l.velocityLow || velocity > l.velocityHigh) continue;
        int wsIndex = l.waveSampleForNote(key);
        auto it = inst_.waveSamples.find(wsIndex);
        if (wsIndex == 0 || it == inst_.waveSamples.end()) continue;
        startVoice(it->second, note, velocity, true, int(i));
    }
}

void Sampler::noteOff(int note) {
    if (!loaded_) return;
    for (int v = 0; v < s_.maxVoices; ++v) {
        Slot &sl = slots_[v];
        if (!sl.active || !sl.held || sl.note != note || sl.sustained) continue;
        if (sustain_) { sl.sustained = true; continue; }
        releaseSlot(v);
    }
    // Key-up layers fire on release.
    int upMask = inst_.keyUpLayers & (inst_.patches[patch_] ? inst_.patches[patch_] : 0xff);
    if (!upMask) return;
    const int key = note + inst_.transpose + off_.transpose;
    for (size_t i = 0; i < inst_.layers.size(); ++i) {
        if (!(upMask & (1 << i))) continue;
        int wsIndex = inst_.layers[i].waveSampleForNote(key);
        auto it = inst_.waveSamples.find(wsIndex);
        if (wsIndex == 0 || it == inst_.waveSamples.end()) continue;
        // Not held: the voice runs its envelope up to sustain and ends with
        // the sample (or is stolen). CALIBRATE: release behaviour on the EPS.
        startVoice(it->second, note, 64, false, int(i));
    }
}

void Sampler::releaseSlot(int v) {
    Slot &sl = slots_[v];
    sl.held = false;
    sl.sustained = false;
    // The release offset also reaches notes that are already sounding.
    sl.amp.relSec[0] = envelopeSeconds(sl.amp.releasePanel[0] + off_.release);
    sl.amp.relSec[1] = envelopeSeconds(sl.amp.releasePanel[1] + off_.release);
    sl.amp.release_(true);
    sl.filt.release_(true);
    sl.pit.release_(true);
    if (sl.loopUntilRelease) {
        // Leave the loop and play on to the sample end.
        const otis::Voice &cv = chip_.voice(v);
        chip_.writeReg(v, otis::END_H, uint16_t(sl.releaseEnd29 >> 16));
        chip_.writeReg(v, otis::END_L, uint16_t(sl.releaseEnd29));
        chip_.writeReg(v, otis::CR, uint16_t(cv.control & ~(otis::kLpe | otis::kBle)));
    }
}

void Sampler::setSustain(bool on) {
    sustain_ = on;
    if (on) return;
    for (int v = 0; v < s_.maxVoices; ++v)
        if (slots_[v].active && slots_[v].sustained) releaseSlot(v);
}

void Sampler::setVoices(int n) {
    n = n < 1 ? 1 : (n > otis::ES5505::kVoices ? otis::ES5505::kVoices : n);
    for (int v = n; v < s_.maxVoices; ++v) {
        chip_.writeReg(v, otis::CR, otis::kStop0);
        slots_[v].active = false;
    }
    s_.maxVoices = n;
    chip_.writeReg(0, otis::ACT, uint16_t(n - 1));
}

void Sampler::allNotesOff() {
    for (int v = 0; v < otis::ES5505::kVoices; ++v) {
        chip_.writeReg(v, otis::CR, otis::kStop0);
        slots_[v].active = false;
    }
}

// The 16 modulation sources for one voice. Unipolar ones are 0..1, bipolar
// ones (LFO, pitch wheel) -1..1. Measured: 4 and 6 linear velocity (6 with
// pressure added, CALIBRATE), 5 and 7 velocity curves, 8 and 9 key / 127,
// 14 half of wheel plus pressure. CALIBRATE: 12, 13, 15.
float Sampler::source(const Slot &sl, int src) const {
    switch (src) {
        case kSrcLfo:   return sl.lfoOut;
        case kSrcEnv1:  return sl.pit.lin / 127.f;
        case kSrcEnv2:  return sl.filt.lin / 127.f;
        case kSrcEnv3:  return sl.amp.lin / 127.f;
        case kSrcVel:   return sl.vel;
        case kSrcVel2:  return velCurve(sl.vel, kVel2);
        case kSrcPrVl:  return clampf(sl.vel + pressureOf(sl), 0.f, 1.f);
        case kSrcVel3:  return velCurve(sl.vel, kVel3);
        case kSrcKbd:
        case kSrcKbd2:  return clampf(float(sl.key) / 127.f, 0.f, 1.f);
        case kSrcPress: return pressureOf(sl);
        case kSrcWhlPr: return clampf(0.5f * (mod_[kSrcWheel] + pressureOf(sl)), 0.f, 1.f);
        default:        return (src >= 0 && src < kSrcCount) ? mod_[src] : 0.f;
    }
}

static bool isBipolar(int src) { return src == kSrcLfo || src == kSrcPitch; }

float Sampler::pressureOf(const Slot &sl) const {
    const float p = sl.note >= 0 && sl.note < 128 ? polyPressure_[sl.note] : 0.f;
    return p > mod_[kSrcPress] ? p : mod_[kSrcPress];
}

float Sampler::waveModValue(const Slot &sl) const {
    // The wave offset of eps~ (0..127) adds to the source, as if the wheel were turned up.
    const float s = source(sl, sl.waveModSource) + float(off_.wave) / 127.f;
    return clampf(clampf(s, -1.f, 1.f) * sl.waveModDepth, -1.f, 1.f);
}

uint32_t Sampler::nextRandom() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

void Sampler::updateControl(int v, float dt) {
    Slot &sl = slots_[v];
    if (!sl.active) return;
    if (dt > 0.f && chip_.stopped(v)) { sl.active = false; return; }
    const WaveSample &ws = *sl.ws;
    sl.amp.tick(dt);
    sl.filt.tick(dt);
    sl.pit.tick(dt);
    if (sl.amp.done()) {
        chip_.writeReg(v, otis::CR, otis::kStop0);
        sl.active = false;
        return;
    }

    // LFO: speed (and its modulation), depth (and its modulation), delay = fade in.
    if (dt > 0.f) {
        const float rateMod = ws.lfoRateModSource == kSrcLfo ? 0.f
                              : float(ws.lfoRateModAmount) / 127.f * source(sl, ws.lfoRateModSource);
        const float hz = lfoHz(ws.lfoSpeed) * sl.lfoRateMul * std::exp2(2.f * rateMod);
        sl.lfoPhase += hz * dt;
        sl.lfoPhase -= std::floor(sl.lfoPhase);
        sl.lfoElapsed += dt;
        sl.noiseTimer += dt;
        if (sl.noiseTimer >= 1.f / noiseHz(ws.randomFreq)) {
            sl.noiseTimer = 0.f;
            sl.noise = float(int32_t(nextRandom())) / 2147483648.f;
        }
    }
    float depth = float(ws.lfoDepth);
    if (ws.lfoDepthModSource != kSrcLfo) depth += float(ws.lfoDepthModAmount) * source(sl, ws.lfoDepthModSource);
    depth = clampf(depth / 127.f, -1.f, 1.f);
    // Delay: a linear fade-in over twice the envelope time (measured).
    const float delay = 2.f * envelopeSeconds(ws.lfoDelay);
    const float fade = ws.lfoDelay > 0 ? clampf(sl.lfoElapsed / delay, 0.f, 1.f) : 1.f;
    sl.lfoOut = lfoWave(ws.lfoWaveform, sl.lfoPhase) * depth * fade;

    // Levels, in the 12-bit volume code domain of the chip (0x100 = 6 dB): the
    // amp envelope, the wavesample volume and boost, the pan law, then the amp
    // modulation or the crossfade and the master level, all as code offsets.
    // CALIBRATE: amp modulation and crossfade amounts are not measured yet.
    float gmul = 1.f;
    const float am = source(sl, ws.ampModSource);
    if (sl.xfade) {
        const float x = (isBipolar(ws.ampModSource) ? (am + 1.f) * 0.5f : am) * 127.f;
        gmul = crossfade(x, ws.crossfade);
    } else if (ws.ampModAmount) {
        const float amt = float(int8_t(ws.ampModAmount)) / 127.f;
        if (isBipolar(ws.ampModSource)) gmul = clampf(1.f + 0.5f * amt * am, 0.f, 2.f);
        else gmul = amt >= 0.f ? 1.f - amt + amt * am : 1.f + amt * am;
    }
    float pan = sl.pan;
    if (ws.panModAmount) pan += float(int8_t(ws.panModAmount)) * source(sl, ws.panModSource);
    const int pi = clampi(int(std::lround(pan)), -128, 127) + 128;
    const float master = s_.masterGain;
    const bool mute = sl.amp.silent || gmul <= 0.f || master <= 0.f;
    const float base = sl.amp.value + sl.volCode + 256.f * std::log2(mute ? 1.f : gmul * master);
    auto volReg = [&](float code) -> uint16_t {
        if (mute) return 0;
        return uint16_t(clampi(int(code), 0, 0xfff) << 4);
    };
    chip_.writeReg(v, otis::LVOL, volReg(base + float(kPanLeft[pi])));
    chip_.writeReg(v, otis::RVOL, volReg(base + float(kPanRight[pi])));

    // Filters, in cutoff units: cutoff + key tracking (set at note on), the
    // envelope (level * amount / 128) and the modulation (amount * source),
    // all measured; plus the offsets of eps~, every tick.
    const int envAmt = clampi(sl.filterEnvAmount + off_.filterEnv, -127, 127);
    float c = float(sl.cutoff + off_.cutoff) + float(envAmt) * sl.filt.lin / 128.f;
    c += float(ws.filterModAmount[0]) * source(sl, ws.filterModSource[0]);
    float c2 = float(sl.cutoff2 + off_.cutoff2) + float(ws.filterEnvAmount[1]) * sl.filt.lin / 128.f;
    c2 += float(ws.filterModAmount[1]) * source(sl, ws.filterModSource[1]);
    chip_.writeReg(v, otis::K1, cutoffToK(c));
    const bool k2Highpass = (ws.filterMode & 2) == 0;
    chip_.writeReg(v, otis::K2, k2Highpass ? cutoffToKHighpass(c2) : cutoffToK(c2));

    // Pitch: the instrument's pitch envelope, LFO, modulation and noise (all in
    // 1/8 semitone), glide, then bend, master tune and the vibrato of eps~.
    if (sl.glide != 0.f && dt > 0.f) {
        const float step = sl.glideRate * dt;
        sl.glide = std::fabs(sl.glide) <= step ? 0.f : sl.glide - (sl.glide > 0.f ? step : -step);
    }
    // Measured: the pitch envelope works relative to its sustain level L4
    // (1/8 semitone per amount and level / 128), the LFO in 1/16 semitones,
    // the modulation in 1/8, the random amount in 1/128 semitone.
    float eighths = float(ws.pitchEnvAmount) * (sl.pit.lin - sl.pit.level[4]) / 128.f +
                    float(ws.lfoPitchAmount) * sl.lfoOut * 0.5f;
    if (ws.pitchModAmount) eighths += float(ws.pitchModAmount) * source(sl, ws.pitchModSource);
    if (ws.randomModAmount) eighths += float(ws.randomModAmount) * sl.noise / 16.f;
    const float vib = vibDepth_ * std::sin(vibPhase_);
    const double semis = double(eighths) / 8.0 + double(sl.glide) + double(bend_) * bendRange_ +
                         off_.tune / 100.0 + double(vib);
    // FC has 10 fractional bits but bit 0 is ignored by the chip: round to the
    // nearest even value (2/1024 steps, as on the original hardware).
    long fc = std::lround(sl.baseRatio * std::pow(2.0, semis / 12.0) * 512.0) * 2;
    chip_.writeReg(v, otis::FC, uint16_t(fc < 2 ? 2 : (fc > 0xfffe ? 0xfffe : fc)));
    updateWaveMod(v);
}

void Sampler::setModSource(int source, float value) {
    if (source >= 0 && source < kSrcCount) mod_[source] = clampf(value, source == kSrcPitch ? -1.f : 0.f, 1.f);
}

void Sampler::setPolyPressure(int note, float value) {
    if (note >= 0 && note < 128) polyPressure_[note] = clampf(value, 0.f, 1.f);
}

// Wave modulation moves the sample or loop points by the mod source:
//   0 loop position        the loop window moves, continuously
//   1 sample start         the note starts later (set at note on)
//   2 start + loop pos     both
//   3 loop start           the loop start moves, the end stays
//   4 loop end             the loop end moves, the start stays
//   5 loop start-X         as 3 (CALIBRATE: the X variant is not told apart)
//   6 transwave            the window jumps in steps of the loop length
// RANGE limits how far. Where the window moves as a whole, the playback position
// moves with it so the phase inside the wave is kept.
void Sampler::updateWaveMod(int v) {
    Slot &sl = slots_[v];
    const int type = sl.waveModType;
    if (type < 0 || type == 1) return;
    const int64_t loopLen = int64_t(sl.loopEnd) - sl.loopStart;
    if (loopLen <= 0) return;
    const float m = waveModValue(sl);
    const otis::Voice &cv = chip_.voice(v);
    auto write = [&](uint32_t st, uint32_t en) {
        chip_.writeReg(v, otis::STRT_H, uint16_t(st >> 16));
        chip_.writeReg(v, otis::STRT_L, uint16_t(st));
        chip_.writeReg(v, otis::END_H, uint16_t(en >> 16));
        chip_.writeReg(v, otis::END_L, uint16_t(en));
    };

    if (type == 3 || type == 4 || type == 5) {
        // One loop point moves (both directions), within the sample and keeping
        // at least 2 samples of loop.
        int64_t st = sl.loopStart, en = sl.loopEnd;
        const int64_t d = int64_t(std::lround(double(m) * sl.waveModRange));
        if (type == 4) en = std::min<int64_t>(std::max<int64_t>(en + d, st + 2), sl.sampleEnd);
        else st = std::max<int64_t>(std::min<int64_t>(st + d, en - 2), sl.sampleStart);
        if (st - sl.loopStart == sl.startShift && en - sl.loopEnd == sl.endShift) return;
        sl.startShift = st - sl.loopStart;
        sl.endShift = en - sl.loopEnd;
        write(uint32_t(sl.base + st) << 9, uint32_t(sl.base + en) << 9);
        return;
    }

    // 0, 2, 6: the window moves as a whole, forward from where it is stored.
    int64_t room = int64_t(sl.sampleEnd) - sl.loopEnd;
    if (room > int64_t(sl.waveModRange)) room = int64_t(sl.waveModRange);
    if (room <= 0) return;
    const float mp = m < 0.f ? 0.f : m;   // CALIBRATE: negative amounts
    int64_t target;
    if (type == 6) target = int64_t(std::lround(double(mp) * double(room / loopLen))) * loopLen;
    else target = int64_t(std::lround(double(mp) * double(room)));
    if (target == sl.shift) return;
    const int64_t delta = target - sl.shift;
    sl.shift = target;
    const uint32_t st = (sl.base + sl.loopStart + uint32_t(target)) << 9;
    const uint32_t en = (sl.base + sl.loopEnd + uint32_t(target)) << 9;
    const int64_t oldSt29 = int64_t(sl.base + sl.loopStart + uint32_t(target - delta)) << 9;
    int64_t acc29 = int64_t(cv.accum >> 2);
    write(st, en);
    if (acc29 < oldSt29) return;          // still before the loop (attack part)
    acc29 += delta << 9;
    if (acc29 < int64_t(st)) acc29 = st;
    if (acc29 > int64_t(en)) acc29 = st;
    chip_.writeReg(v, otis::ACC_H, uint16_t(uint32_t(acc29) >> 16));
    chip_.writeReg(v, otis::ACC_L, uint16_t(uint32_t(acc29)));
}

void Sampler::process(float *left, float *right, int frames) {
    std::memset(left, 0, sizeof(float) * size_t(frames));
    std::memset(right, 0, sizeof(float) * size_t(frames));
    if (!loaded_) return;
    int done = 0;
    while (done < frames) {
        if (counter_ <= 0) {
            const float dt = float(s_.controlInterval) / s_.sampleRate;
            vibPhase_ += 2.f * float(M_PI) * vibRate_ * dt;
            if (vibPhase_ > 2.f * float(M_PI)) vibPhase_ -= 2.f * float(M_PI);
            for (int v = 0; v < s_.maxVoices; ++v) updateControl(v, dt);
            counter_ = s_.controlInterval;
        }
        int n = frames - done < counter_ ? frames - done : counter_;
        chip_.renderAdd(left + done, right + done, size_t(n));
        if (volume_ != 1.f)
            for (int i = done; i < done + n; ++i) { left[i] *= volume_; right[i] *= volume_; }
        done += n;
        counter_ -= n;
    }
}

int Sampler::activeVoiceCount() const {
    int n = 0;
    for (int v = 0; v < s_.maxVoices; ++v) n += slots_[v].active;
    return n;
}

} // namespace eps
