// eps_sampler.cpp — see eps_sampler.h. Values marked CALIBRATE are first
// guesses that should be replaced by tables measured on the original OS.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Winline"
#endif
#include "eps_sampler.h"

#include <cmath>
#include <cstring>

namespace eps {

namespace {

const float kEnvSeconds[100] = {
    0, 0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.07f, 0.08f, 0.09f, 0.10f, 0.11f, 0.11f, 0.12f,
    0.13f, 0.14f, 0.15f, 0.16f, 0.17f, 0.19f, 0.20f, 0.22f, 0.23f, 0.25f, 0.27f, 0.29f, 0.31f, 0.33f,
    0.35f, 0.38f, 0.41f, 0.44f, 0.47f, 0.50f, 0.54f, 0.58f, 0.62f, 0.66f, 0.71f, 0.76f, 0.82f, 0.88f,
    0.94f, 1.0f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 2.0f, 2.1f, 2.3f, 2.4f, 2.6f,
    2.8f, 3.0f, 3.2f, 3.5f, 3.7f, 4.0f, 4.3f, 4.6f, 4.9f, 5.3f, 5.7f, 6.1f, 6.5f, 7.0f, 7.5f, 8.1f,
    8.6f, 9.3f, 9.9f, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 24, 26, 28, 30, 32, 34, 37,
    39, 42, 45, 49};

// CALIBRATE: envelope level 0..1 (stored 0..127) -> gain; assumed linear.
float envLevelGain(float level01) { return level01 <= 0.f ? 0.f : (level01 > 1.f ? 1.f : level01); }

// CALIBRATE: wavesample volume 0..127 -> gain; assumed linear.
float wsVolumeGain(int v) { return v <= 0 ? 0.f : float(v > 127 ? 127 : v) / 127.f; }

// Fast nearest ES5505 volume code: gain = (16 + m) * 2^(e - 20).
uint8_t gainToCode(float g) {
    if (g <= 1.f / float(1 << 20) * 16.f) return 0;
    int e2;
    float m = std::frexp(g, &e2);          // g = m * 2^e2, m in [0.5, 1)
    int e = e2 + 15;                        // (16+mant)/32 == m  ->  e - 20 + 5 == e2
    int mant = int(m * 32.f + 0.5f) - 16;
    if (mant > 15) { mant = 0; ++e; }
    if (e > 15) return 255;
    if (e < 0) return 0;
    return uint8_t(e << 4 | mant);
}

float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

// CALIBRATE: LFO speed 0..99 -> Hz (table of ConvertWithMoss: slow below 20,
// then 0.25 Hz steps up to about 21 Hz).
float lfoHz(int speed) {
    static const float kSlow[20] = {0.05f, 0.05f, 0.06f, 0.06f, 0.06f, 0.07f, 0.07f, 0.08f, 0.08f, 0.09f,
                                    0.10f, 0.11f, 0.13f, 0.14f, 0.17f, 0.20f, 0.25f, 0.33f, 0.50f, 1.0f};
    speed = clampi(speed, 0, 99);
    return speed < 20 ? kSlow[speed] : 1.f + float(speed - 19) * 0.25f;
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

// CALIBRATE: the noise source's rate (wavesample "random freq" 0..99).
float noiseHz(int freq) { return 1.f + 0.5f * float(clampi(freq, 0, 99)); }

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

float envelopeSeconds(int v) { return kEnvSeconds[v < 0 ? 0 : (v > 99 ? 99 : v)]; }

const char *modSourceName(int src) {
    static const char *const kNames[kSrcCount] = {"LFO", "ENV1", "ENV2", "ENV3", "NOISE", "VEL", "PR+VL",
                                                  "VEL2", "KBD", "KBD2", "WHEEL", "PEDAL", "XCTRL",
                                                  "PRESS", "WHL+PR", "PITCH"};
    return src >= 0 && src < kSrcCount ? kNames[src] : "?";
}

// ---------------------------------------------------------------- envelope

void Sampler::Env::start(float) {
    stage = 1;
    value = level[0];
    target = level[1];
}

void Sampler::Env::release_(float) {
    if (stage >= 6) return;
    stage = 6;
    releaseLevel = value;
}

float Sampler::Env::tick(float dt, float) {
    if (stage >= 1 && stage <= 4) {
        float t = time[stage];
        float from = stage == 1 ? level[0] : level[stage - 1];
        if (t <= 0.f) { value = level[stage]; ++stage; }
        else {
            value += (level[stage] - from) * dt / t;
            bool reached = (level[stage] >= from) ? value >= level[stage] : value <= level[stage];
            if (reached) { value = level[stage]; ++stage; }
        }
    } else if (stage == 6) {
        if (release <= 0.f) { value = 0.f; stage = 7; }
        else {
            value -= releaseLevel * dt / release;
            if (value <= 0.f) { value = 0.f; stage = 7; }
        }
    }
    return value;
}

// ---------------------------------------------------------------- sampler

Sampler::Sampler(const Settings &s) : s_(s) {
    if (s_.maxVoices < 1) s_.maxVoices = 1;
    if (s_.maxVoices > otis::ES5505::kVoices) s_.maxVoices = otis::ES5505::kVoices;
    chip_.writeReg(0, otis::ACT, uint16_t(s_.maxVoices - 1));
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

// K2 in the modes with a high-pass (0, 1). The chip's high-pass is
// y[n] = x[n] - x[n-1] + (K/8192 + 1/2) * y[n-1], a pole at p = 1/2 + K/8192:
// K = (exp(-2 pi f / fs) - 1/2) * 8192. The cutoff value is the corner frequency
// as for the low-pass (MOOG FUNK: cutoff 150 with the filter 2 envelope at
// -127 opens the bass at the key, thins it on release). Highest corner about
// fs * ln 2 / 2 pi (4.9 kHz at 44.1 kHz).
uint16_t Sampler::cutoffToKHighpass(float c01) const {
    const float f = 20.f * std::pow(0.45f * s_.sampleRate / 20.f, clampf(c01, 0.f, 1.f));
    const float p = std::exp(-2.f * float(M_PI) * f / s_.sampleRate);
    int k12 = int((p - 0.5f) * 8192.f);
    k12 = clampi(k12, 0, 4095);
    return uint16_t(k12 << 4);
}

uint16_t Sampler::cutoffToK(float c01) const {
    // CALIBRATE: cutoff 0..1 -> 20 Hz .. 0.45 * fs, exponential; K is the
    // one-pole coefficient in 12 bits (shifted into the 16-bit register).
    float f = 20.f * std::pow(0.45f * s_.sampleRate / 20.f, clampf(c01, 0.f, 1.f));
    float k = 1.f - std::exp(-2.f * float(M_PI) * f / s_.sampleRate);
    int k12 = int(k * 4096.f);
    if (k12 > 4095) k12 = 4095;
    if (k12 < 0) k12 = 0;
    return uint16_t(k12 << 4);
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
    auto setupEnv = [&](Env &e, const Envelope &src, bool amp) {
        e = Env();
        for (int i = 0; i < 5; ++i)
            e.level[i] = (src.softLevel[i] + (src.hardLevel[i] - src.softLevel[i]) * sl.vel) / 127.f;
        for (int i = 1; i <= 4; ++i) {
            const int o = !amp ? 0 : (i == 1 ? off_.attack : off_.decay);
            e.time[i] = envelopeSeconds(src.time[i] + o);
        }
        e.releasePanel[0] = src.time[5];
        e.releasePanel[1] = src.time[6];
        e.release = envelopeSeconds(src.time[5]) + envelopeSeconds(src.time[6]);
        e.start(s_.sampleRate);
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
    sl.cutoff = clampi(ws.filterCutoff[0] + ws.filterKeyAmount[0] * (key - 60) / 64, 0, 150);
    sl.cutoff2 = clampi(ws.filterCutoff[1] + ws.filterKeyAmount[1] * (key - 60) / 64, 0, 150);
    sl.filterEnvAmount = ws.filterEnvAmount[0];

    // Pan: EPS uses 8 positions, EPS-16 PLUS / ASR a signed value.
    if (inst_.instrumentId == kIdEps) {
        if (ws.panEps >= 1 && ws.panEps <= 8) sl.pan = (float(ws.panEps - 1) / 7.f) * 2.f - 1.f;
    } else {
        sl.pan = clampf(float(ws.panSigned) / 127.f, -1.f, 1.f);
    }
    sl.ampBase = wsVolumeGain(ws.volume) * s_.masterGain;
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
    sl.amp.release = envelopeSeconds(sl.amp.releasePanel[0] + off_.release) +
                     envelopeSeconds(sl.amp.releasePanel[1] + off_.release);
    sl.amp.release_(s_.sampleRate);
    sl.filt.release_(s_.sampleRate);
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
// ones (LFO, noise, KBD, pitch wheel) -1..1. CALIBRATE: VEL2, PR+VL, KBD and
// KBD2 scaling are guesses.
float Sampler::source(const Slot &sl, int src) const {
    switch (src) {
        case kSrcLfo:   return sl.lfoOut;
        case kSrcEnv1:  return sl.pit.value;
        case kSrcEnv2:  return sl.filt.value;
        case kSrcEnv3:  return sl.amp.value;
        case kSrcNoise: return sl.noise;
        case kSrcVel:   return sl.vel;
        case kSrcPrVl:  return clampf(sl.vel + pressureOf(sl), 0.f, 1.f);
        case kSrcVel2:  return sl.vel * sl.vel;
        case kSrcKbd:   return clampf(float(sl.key - 60) / 48.f, -1.f, 1.f);
        case kSrcKbd2:  return clampf(float(sl.key - 21) / 87.f, 0.f, 1.f);
        case kSrcPress: return pressureOf(sl);
        case kSrcWhlPr: return clampf(mod_[kSrcWheel] + pressureOf(sl), 0.f, 1.f);
        default:        return (src >= 0 && src < kSrcCount) ? mod_[src] : 0.f;
    }
}

static bool isBipolar(int src) { return src == kSrcLfo || src == kSrcNoise || src == kSrcKbd || src == kSrcPitch; }

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
    const float a = sl.amp.tick(dt, s_.sampleRate);
    const float f = sl.filt.tick(dt, s_.sampleRate);
    const float p = sl.pit.tick(dt, s_.sampleRate);
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
    const float delay = envelopeSeconds(ws.lfoDelay);
    const float fade = delay > 0.f ? clampf(sl.lfoElapsed / delay, 0.f, 1.f) : 1.f;
    sl.lfoOut = lfoWave(ws.lfoWaveform, sl.lfoPhase) * depth * fade;

    // Levels: amp envelope, then the amp modulation or the crossfade.
    float g = sl.ampBase * envLevelGain(a);
    const float am = source(sl, ws.ampModSource);
    if (sl.xfade) {
        const float x = (isBipolar(ws.ampModSource) ? (am + 1.f) * 0.5f : am) * 127.f;
        g *= crossfade(x, ws.crossfade);
    } else if (ws.ampModAmount) {
        const float amt = float(int8_t(ws.ampModAmount)) / 127.f;
        if (isBipolar(ws.ampModSource)) g *= clampf(1.f + 0.5f * amt * am, 0.f, 2.f);
        else g *= amt >= 0.f ? 1.f - amt + amt * am : 1.f + amt * am;
    }
    float pan = sl.pan;
    if (ws.panModAmount) pan += float(int8_t(ws.panModAmount)) / 127.f * source(sl, ws.panModSource);
    pan = clampf(pan, -1.f, 1.f);
    const float pl = std::cos((pan + 1.f) * float(M_PI) / 4.f), pr = std::sin((pan + 1.f) * float(M_PI) / 4.f);
    chip_.writeReg(v, otis::LVOL, uint16_t(gainToCode(g * pl) << 8));
    chip_.writeReg(v, otis::RVOL, uint16_t(gainToCode(g * pr) << 8));

    // Filters: cutoff (+ offset), envelope (+ offset) and modulation, both filters,
    // every tick, so the offsets reach notes that are sounding. Amount 127 of a
    // modulation sweeps the whole cutoff range (as ConvertWithMoss reads it).
    const int envAmt = clampi(sl.filterEnvAmount + off_.filterEnv, -127, 127);
    float c = float(sl.cutoff + off_.cutoff) + float(envAmt) * f;
    c += float(ws.filterModAmount[0]) / 127.f * 150.f * source(sl, ws.filterModSource[0]);
    float c2 = float(sl.cutoff2 + off_.cutoff2) + float(ws.filterEnvAmount[1]) * f;
    c2 += float(ws.filterModAmount[1]) / 127.f * 150.f * source(sl, ws.filterModSource[1]);
    chip_.writeReg(v, otis::K1, cutoffToK(clampf(c, 0.f, 150.f) / 150.f));
    const bool k2Highpass = (ws.filterMode & 2) == 0;
    chip_.writeReg(v, otis::K2, k2Highpass ? cutoffToKHighpass(clampf(c2, 0.f, 150.f) / 150.f)
                                           : cutoffToK(clampf(c2, 0.f, 150.f) / 150.f));

    // Pitch: the instrument's pitch envelope, LFO, modulation and noise (all in
    // 1/8 semitone), glide, then bend, master tune and the vibrato of eps~.
    if (sl.glide != 0.f && dt > 0.f) {
        const float step = sl.glideRate * dt;
        sl.glide = std::fabs(sl.glide) <= step ? 0.f : sl.glide - (sl.glide > 0.f ? step : -step);
    }
    float eighths = float(ws.pitchEnvAmount) * p + float(ws.lfoPitchAmount) * sl.lfoOut;
    if (ws.pitchModAmount) eighths += float(ws.pitchModAmount) * source(sl, ws.pitchModSource);
    if (ws.randomModAmount) eighths += float(ws.randomModAmount) * sl.noise;
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
