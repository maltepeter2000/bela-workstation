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

} // namespace

float envelopeSeconds(int v) { return kEnvSeconds[v < 0 ? 0 : (v > 99 ? 99 : v)]; }

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

void Sampler::startVoice(const WaveSample &ws, int note, int velocity, bool held) {
    const WaveSample *owner = inst_.pcmOwner(ws);
    if (!owner || owner->pcm.empty()) return;
    int v = allocate();
    if (v < 0) return;
    Slot &sl = slots_[v];

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
    const int key = note + inst_.transpose + off_.transpose;
    const double semis = double(key - ws.rootNote + s_.pitchOffset) + double(ws.fineTune) / 128.0;
    const double ratio = double(owner->sampleRate) / s_.sampleRate * std::pow(2.0, semis / 12.0);
    // FC has 10 fractional bits but bit 0 is ignored by the chip: round to
    // the nearest even value (resolution 2/1024, i.e. up to ~3 cent at low
    // playback ratios, as on the original hardware).
    sl.baseRatio = ratio;
    long fc = std::lround(ratio * 512.0) * 2;
    if (fc < 2) fc = 2;
    if (fc > 0xfffe) fc = 0xfffe;

    chip_.writeReg(v, otis::CR, uint16_t(ctrl | otis::kStop0));   // stop while programming
    chip_.setAddresses(v, st, en, acc);
    chip_.setFrequency(v, uint16_t(fc));
    chip_.writeReg(v, otis::LVOL, 0);
    chip_.writeReg(v, otis::RVOL, 0);

    // Filter: static cutoff + key tracking; envelope applied in updateControl.
    sl.cutoff = clampi(ws.filterCutoff[0] + ws.filterKeyAmount[0] * (key - 60) / 64, 0, 150);
    sl.cutoff2 = clampi(ws.filterCutoff[1] + ws.filterKeyAmount[1] * (key - 60) / 64, 0, 150);
    sl.filterEnvAmount = ws.filterEnvAmount[0];
    chip_.writeReg(v, otis::K1, cutoffToK(float(clampi(sl.cutoff + off_.cutoff, 0, 150)) / 150.f));
    // K2: second cutoff. In the highpass modes (0, 1) a larger K moves the
    // highpass corner down, so 150 = "open" for both filter types. MOOG FUNK
    // (mode 0, cutoff2 150) confirms that 150 must not thin out the sound.
    chip_.writeReg(v, otis::K2, cutoffToK(float(clampi(sl.cutoff2 + off_.cutoff2, 0, 150)) / 150.f));

    // Amp: velocity interpolates between soft and hard levels.
    const float vel = clampf(float(velocity) / 127.f, 0.f, 1.f);
    auto setupEnv = [&](Env &e, const Envelope &src, bool amp) {
        e = Env();
        for (int i = 0; i < 5; ++i)
            e.level[i] = (src.softLevel[i] + (src.hardLevel[i] - src.softLevel[i]) * vel) / 127.f;
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

    // Pan: EPS uses 8 positions, EPS-16 PLUS / ASR a signed value.
    float pan = 0.f;
    if (inst_.instrumentId == kIdEps) {
        if (ws.panEps >= 1 && ws.panEps <= 8) pan = (float(ws.panEps - 1) / 7.f) * 2.f - 1.f;
    } else {
        pan = clampf(float(ws.panSigned) / 127.f, -1.f, 1.f);
    }
    sl.panL = std::cos((pan + 1.f) * float(M_PI) / 4.f);
    sl.panR = std::sin((pan + 1.f) * float(M_PI) / 4.f);
    sl.ampBase = wsVolumeGain(ws.volume) * s_.masterGain;
    sl.sustained = false;

    sl.base = base;
    sl.loopStart = lStart;
    sl.loopEnd = lEnd;
    sl.sampleEnd = sEnd;
    sl.shift = 0;
    sl.waveModType = -1;
    if ((mode == 2 || mode == 3 || mode == 4) && ws.waveModAmount() != 0 &&
        (ws.waveModType() == 0 || ws.waveModType() == 6)) {
        sl.waveModType = ws.waveModType();
        sl.waveModSource = ws.waveModSource() & 15;
        sl.waveModDepth = float(ws.waveModAmount()) / 127.f;
        sl.waveModRange = ws.waveModRangeSamples();
    }

    sl.active = true;
    sl.held = held;
    sl.note = note;
    sl.age = ++clock_;
    sl.ws = &ws;
    sl.loopUntilRelease = mode == 4;
    sl.releaseEnd29 = a29(sEnd);

    updateControl(v, 0.f);
    chip_.writeReg(v, otis::CR, ctrl);   // go
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
        startVoice(it->second, note, velocity, true);
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
        startVoice(it->second, note, 64, false);
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

void Sampler::updateControl(int v, float dt) {
    Slot &sl = slots_[v];
    if (!sl.active) return;
    if (dt > 0.f && chip_.stopped(v)) { sl.active = false; return; }
    float a = sl.amp.tick(dt, s_.sampleRate);
    float f = sl.filt.tick(dt, s_.sampleRate);
    if (sl.amp.done()) {
        chip_.writeReg(v, otis::CR, otis::kStop0);
        sl.active = false;
        return;
    }
    float g = sl.ampBase * envLevelGain(a);
    chip_.writeReg(v, otis::LVOL, uint16_t(gainToCode(g * sl.panL) << 8));
    chip_.writeReg(v, otis::RVOL, uint16_t(gainToCode(g * sl.panR) << 8));
    // Filters: cutoff (+ offset) and the filter envelope (+ offset), every tick, so
    // the offsets reach notes that are sounding.
    const int envAmt = clampi(sl.filterEnvAmount + off_.filterEnv, -127, 127);
    const float c = float(sl.cutoff + off_.cutoff) + float(envAmt) * f;
    chip_.writeReg(v, otis::K1, cutoffToK(clampf(c, 0.f, 150.f) / 150.f));
    chip_.writeReg(v, otis::K2, cutoffToK(float(clampi(sl.cutoff2 + off_.cutoff2, 0, 150)) / 150.f));
    // Pitch: bend, master tune and vibrato on top of the note's ratio.
    const float vib = vibDepth_ * std::sin(vibPhase_);
    const double semis = double(bend_) * bendRange_ + off_.tune / 100.0 + double(vib);
    long fc = std::lround(sl.baseRatio * std::pow(2.0, semis / 12.0) * 512.0) * 2;
    chip_.writeReg(v, otis::FC, uint16_t(fc < 2 ? 2 : (fc > 0xfffe ? 0xfffe : fc)));
    updateWaveMod(v);
}

void Sampler::setModSource(int source, float value) {
    if (source >= 0 && source < 16) mod_[source] = clampf(value, 0.f, 1.f);
}

// Wave modulation: moves the loop window through the sample.
//   Type 6 (Transwave): window jumps in steps of the loop length, i.e. through
//     the single-cycle waves of a transwave set (e.g. 128 x 400 samples).
//   Type 0 (Loop Position): window moves continuously.
// The playback position is moved by the same amount so the phase inside the
// wave is kept (no jump back to the loop start).
void Sampler::updateWaveMod(int v) {
    Slot &sl = slots_[v];
    if (sl.waveModType < 0) return;
    const int64_t loopLen = int64_t(sl.loopEnd) - sl.loopStart;
    // How far the window can move: limited by the sample end and by the
    // wave mod RANGE parameter (CLASSIC PAD: 32K of its 51200 samples).
    int64_t room = int64_t(sl.sampleEnd) - sl.loopEnd;
    if (room > int64_t(sl.waveModRange)) room = int64_t(sl.waveModRange);
    if (loopLen <= 0 || room <= 0) return;
    // The wave offset (0..127) adds to the mod source, as if the wheel were turned up.
    const float src = clampf(mod_[sl.waveModSource] + float(off_.wave) / 127.f, 0.f, 1.f);
    float m = clampf(src * sl.waveModDepth, -1.f, 1.f);
    if (m < 0.f) m = 0.f;   // CALIBRATE: negative amounts
    int64_t target;
    if (sl.waveModType == 6) {
        const int64_t steps = room / loopLen;
        target = int64_t(std::lround(double(m) * double(steps))) * loopLen;
    } else {
        target = int64_t(std::lround(double(m) * double(room)));
    }
    if (target == sl.shift) return;
    const int64_t delta = target - sl.shift;
    sl.shift = target;
    const uint32_t st = (sl.base + sl.loopStart + uint32_t(target)) << 9;
    const uint32_t en = (sl.base + sl.loopEnd + uint32_t(target)) << 9;
    // Current position, chip-internal (11 fractional bits) -> register format.
    const otis::Voice &cv = chip_.voice(v);
    const int64_t oldSt29 = int64_t(sl.base + sl.loopStart + uint32_t(target - delta)) << 9;
    int64_t acc29 = int64_t(cv.accum >> 2);
    if (acc29 < oldSt29) {
        // Still before the loop (attack part): only move the window.
        chip_.writeReg(v, otis::STRT_H, uint16_t(st >> 16));
        chip_.writeReg(v, otis::STRT_L, uint16_t(st));
        chip_.writeReg(v, otis::END_H, uint16_t(en >> 16));
        chip_.writeReg(v, otis::END_L, uint16_t(en));
        return;
    }
    acc29 += delta << 9;
    if (acc29 < int64_t(st)) acc29 = st;
    if (acc29 > int64_t(en)) acc29 = st;   // was still in the attack part: restart window
    chip_.writeReg(v, otis::STRT_H, uint16_t(st >> 16));
    chip_.writeReg(v, otis::STRT_L, uint16_t(st));
    chip_.writeReg(v, otis::END_H, uint16_t(en >> 16));
    chip_.writeReg(v, otis::END_L, uint16_t(en));
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
