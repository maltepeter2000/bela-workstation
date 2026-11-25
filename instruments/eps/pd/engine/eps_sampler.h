// eps_sampler — plays eps::Instrument data on the otis::ES5505 core.
//
// This layer does the job of the EPS-16 PLUS operating system: voice
// allocation, address/pitch/loop programming and envelopes, written to the
// chip at a control rate. Everything chip-side (interpolation, loops, filter,
// log volume) is the MAME-derived ES5505 model; everything in this file is a
// first approximation of the OS and still needs calibration against the
// original (register traces from the mardlib emulator).
//
// Status of the OS-side behaviour:
//   done (provisional): layers/patches, key- and velocity-mapping, key-up
//     layers, loop modes 0-4, pitch from root/fine/transpose, amp envelope
//     (soft/hard levels by velocity), static filter cutoff + filter envelope,
//     pan, wavesample volume, wave modulation types Loop Position and
//     Transwave from any mod source value set by the host
//   wave mod fields confirmed on the hardware display (CLASSIC PAD)
//   missing: LFO, pitch envelope, mod routing for pitch/filter/amp, glide,
//     crossfades, pitch tables, other wave mod types, effects (ES5510)

#pragma once

#include "eps_loader.h"
#include "es5505.h"

#include <cstdint>
#include <vector>

namespace eps {

class Sampler {
public:
    struct Settings {
        float sampleRate = 44100.f;   // rate the chip is clocked at (host rate)
        int maxVoices = 20;           // EPS-16 PLUS: 20
        int controlInterval = 32;     // samples between envelope/register updates
        float masterGain = 1.f;
        // Semitones added to every pitch. The three real instruments tested
        // so far all sound one octave below their stored root key when the
        // root is read as a MIDI note; set -12/0 after checking the EPS.
        int pitchOffset = 0;
    };

    // Performance offsets over the loaded instrument (the controls of eps~).
    // 0 everywhere = the instrument as stored.
    struct Offsets {
        int cutoff = 0;      // -63..63, in cutoff units (0..150), filter 1
        int cutoff2 = 0;     // -63..63, filter 2 (K2: second lowpass or the highpass)
        int filterEnv = 0;   // -63..63, added to the filter envelope amount
        int attack = 0;      // -63..63 panel steps on time 1 of the amp envelope
        int decay = 0;       // -63..63 on times 2..4
        int release = 0;     // -63..63 on times 5, 6 (also for notes already sounding)
        int wave = 0;        // 0..127, added to the wave mod source (transwave position)
        int transpose = 0;   // semitones, mapping and pitch
        int tune = 0;        // cents
    };

    explicit Sampler(const Settings &s);

    // Not real-time safe (allocates). Replaces the loaded instrument.
    bool load(const Instrument &inst, std::string &err);
    bool loaded() const { return loaded_; }

    // Real-time safe.
    void noteOn(int note, int velocity);
    void noteOff(int note);
    void allNotesOff();
    void setPatch(int patch) { patch_ = patch & 3; }   // patch-select 0..3
    // Mod source values 0..1 by EPS source number (10 = mod wheel, assumed).
    void setModSource(int source, float value);
    void setModWheel(float value) { setModSource(10, value); }
    void setOffsets(const Offsets &o) { off_ = o; }
    const Offsets &offsets() const { return off_; }
    void setBend(float v) { bend_ = v < -1.f ? -1.f : (v > 1.f ? 1.f : v); }   // -1..1
    void setBendRange(int semis) { bendRange_ = semis < 0 ? 0 : (semis > 24 ? 24 : semis); }
    void setVibrato(float depth01, float rateHz) { vibDepth_ = depth01; vibRate_ = rateHz; }
    void setSustain(bool on);
    void setVolume(float gain) { volume_ = gain; }
    void setVoices(int n);
    void setSampleRate(float sr) { if (sr > 0.f) s_.sampleRate = sr; }
    float sampleRate() const { return s_.sampleRate; }
    int voices() const { return s_.maxVoices; }
    const Instrument &instrument() const { return inst_; }
    int patch() const { return patch_; }
    void process(float *left, float *right, int frames);   // overwrites buffers
    int activeVoiceCount() const;
    const otis::ES5505 &chipForDebug() const { return chip_; }

private:
    struct Env {
        // Levels 0..1, times in seconds; stage 0 = start level.
        float level[5] = {};
        float time[5] = {};       // [1]..[4] attack/decay segments, [0] unused
        float release = 0.f;
        int releasePanel[2] = {0, 0};   // times 5, 6: release seconds computed at release
        int stage = 0;            // 1..4 running, 5 sustain, 6 release, 7 done
        float value = 0.f, target = 0.f, step = 0.f;
        float releaseLevel = 0.f;
        void start(float sr);
        void release_(float sr);
        float tick(float dtSeconds, float sr);   // returns value
        bool done() const { return stage == 7; }
    };

    struct Slot {
        bool active = false;
        int note = -1;
        uint32_t age = 0;
        bool held = false;
        bool sustained = false;    // key up while the pedal is down
        double baseRatio = 1.0;    // playback ratio without bend / tune / vibrato
        int cutoff2 = 150;
        const WaveSample *ws = nullptr;
        uint32_t releaseEnd29 = 0;     // END for loop-until-release
        bool loopUntilRelease = false;
        float ampBase = 1.f, panL = 1.f, panR = 1.f;
        int cutoff = 127, filterEnvAmount = 0;
        Env amp, filt;
        // Wave modulation state (addresses in samples, relative to the
        // owner's base word address).
        uint32_t base = 0, loopStart = 0, loopEnd = 0, sampleEnd = 0;
        int waveModType = -1;      // -1 = none
        int waveModSource = 0;
        float waveModDepth = 0.f;  // amount / 127
        uint32_t waveModRange = 0; // samples
        int64_t shift = 0;         // current loop shift in samples
    };

    void startVoice(const WaveSample &ws, int note, int velocity, bool held);
    int allocate();
    void updateControl(int v, float dt);
    void updateWaveMod(int v);
    uint16_t cutoffToK(float cutoff01) const;

    Settings s_;
    otis::ES5505 chip_;
    std::vector<int16_t> ram_;
    std::vector<uint32_t> wsBase_;     // word address per wavesample slot
    Instrument inst_;
    bool loaded_ = false;
    int patch_ = 0;
    uint32_t clock_ = 0;
    Slot slots_[otis::ES5505::kVoices];
    int counter_ = 0;
    float mod_[16] = {};
    Offsets off_;
    float bend_ = 0.f, vibDepth_ = 0.f, vibRate_ = 5.5f, vibPhase_ = 0.f, volume_ = 1.f;
    int bendRange_ = 2;
    bool sustain_ = false;
    void releaseSlot(int v);
};

// Envelope time table (panel 0..99 -> seconds), from ConvertWithMoss.
float envelopeSeconds(int panelValue);

} // namespace eps
