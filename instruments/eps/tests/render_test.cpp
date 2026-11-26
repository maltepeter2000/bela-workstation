// Offline render test for eps::Sampler on the synthetic fixture instrument:
// pitch accuracy, looping, release, copy-wavesamples, layer/patch selection,
// voice stealing and a CPU benchmark. Writes demo.wav for listening.
#include "../pd/engine/eps_sampler.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) { fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

constexpr float SR = 44100.f;

static std::vector<float> render(eps::Sampler &s, int frames, std::vector<float> *right = nullptr) {
    std::vector<float> l(static_cast<size_t>(frames)), r(static_cast<size_t>(frames));
    for (int i = 0; i < frames; i += 128) {
        int n = frames - i < 128 ? frames - i : 128;
        s.process(&l[size_t(i)], &r[size_t(i)], n);
    }
    if (right) *right = r;
    for (size_t i = 0; i < l.size(); ++i) l[i] += r[i];
    return l;
}

// Autocorrelation pitch estimate on x[from, from+len).
static double pitch(const std::vector<float> &x, size_t from, size_t len) {
    double best = -1e30;
    int bestLag = 0;
    std::vector<double> ac(2000, 0.0);
    for (int lag = 20; lag < 1500; ++lag) {
        double s = 0, e1 = 0, e2 = 0;
        for (size_t i = 0; i < len; ++i) {
            double a = x[from + i], b = x[from + i + size_t(lag)];
            s += a * b; e1 += a * a; e2 += b * b;
        }
        ac[size_t(lag)] = s / std::sqrt(e1 * e2 + 1e-20);
    }
    // First lag whose normalized correlation is a local max above 0.9.
    for (int lag = 21; lag < 1499; ++lag) {
        if (ac[size_t(lag)] > 0.9 && ac[size_t(lag)] >= ac[size_t(lag - 1)] && ac[size_t(lag)] >= ac[size_t(lag + 1)]) {
            bestLag = lag;
            best = ac[size_t(lag)];
            break;
        }
    }
    if (!bestLag) return 0;
    (void)best;
    double y0 = ac[size_t(bestLag - 1)], y1 = ac[size_t(bestLag)], y2 = ac[size_t(bestLag + 1)];
    double d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2);
    return SR / (bestLag + d);
}

static double rms(const std::vector<float> &x, size_t from, size_t len) {
    double s = 0;
    for (size_t i = from; i < from + len && i < x.size(); ++i) s += double(x[i]) * x[i];
    return std::sqrt(s / double(len));
}

static void writeWav(const char *path, const std::vector<float> &l, const std::vector<float> &r) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    uint32_t n = uint32_t(l.size());
    fwrite("RIFF", 1, 4, f); w32(36 + n * 4); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(44100); w32(44100 * 4); w16(4); w16(16);
    fwrite("data", 1, 4, f); w32(n * 4);
    for (uint32_t i = 0; i < n; ++i) {
        auto cv = [](float x) { x = x > 1 ? 1 : (x < -1 ? -1 : x); return int16_t(std::lround(x * 32767)); };
        int16_t a = cv(l[i]), b = cv(r[i]);
        fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
    }
    fclose(f);
}


// Switch every modulation of an instrument off.
static void neutral(eps::Instrument &in) {
    for (auto &kv : in.waveSamples) {
        eps::WaveSample &w = kv.second;
        w.pitchEnvAmount = w.lfoPitchAmount = w.pitchModAmount = w.randomModAmount = 0;
        w.lfoDepth = w.lfoDepthModAmount = w.lfoRateModAmount = 0;
        w.ampModAmount = w.panModAmount = 0;
        w.filterModAmount[0] = w.filterModAmount[1] = 0;
        w.filterEnvAmount[1] = 0;
        w.volumeModCrossfadeCurve = 0;
    }
}

// The chip voice that sounds (the first not stopped one), -1 if none.
static int sounding(const eps::Sampler &s) {
    for (int v = 0; v < otis::ES5505::kVoices; ++v)
        if (!s.chipForDebug().stopped(v)) return v;
    return -1;
}

static void run(eps::Sampler &s, int frames) {
    float l[64], r[64];
    for (int i = 0; i < frames; i += 64) s.process(l, r, 64);
}

// Each feature of the OS side on its own, read back from the chip registers.
static int s_vol(const eps::Sampler &s) { int v = sounding(s); return v < 0 ? 0 : int(s.chipForDebug().voice(v).lvol); }
static void modulation_tests(const eps::Instrument &base) {
    eps::Sampler::Settings set;
    set.sampleRate = SR;
    std::string err;
    auto fc = [](const eps::Sampler &s, int v) { return double(s.chipForDebug().voice(v).freqcount); };

    // pitch envelope: relative to its sustain level L4 (measured). L0 127, then
    // a 49 s segment down to L1..L4 = 0: at the start amount 96 gives
    // 96 * 127 / 128 eighths (+11.9 semitones).
    {
        eps::Instrument in = base;
        eps::Sampler a(set), b(set);
        a.load(in, err);
        a.noteOn(50, 100);
        run(a, 512);
        eps::WaveSample &w = in.waveSamples[1];
        w.pitchEnvAmount = 96;
        for (int i = 0; i < 5; ++i) w.pitchEnv.softLevel[i] = w.pitchEnv.hardLevel[i] = i == 0 ? 127 : 0;
        for (int i = 1; i <= 6; ++i) w.pitchEnv.time[i] = i == 1 ? 99 : 0;
        b.load(in, err);
        b.noteOn(50, 100);
        run(b, 512);
        const double ratio = fc(b, sounding(b)) / fc(a, sounding(a));
        const double want = std::pow(2.0, 96.0 * 127.0 / 128.0 / 8.0 / 12.0);
        printf("  pitch envelope +96, L0 127 over L4 0: ratio %.3f (%.3f)\n", ratio, want);
        CHECK(std::fabs(ratio - want) < 0.01);
    }
    // LFO: sine, depth 127, pitch 16 (1/16 semitone each: +-1 semitone); delay
    // fades it in; wheel on LFO depth
    {
        eps::Instrument in = base;
        eps::WaveSample &w = in.waveSamples[1];
        w.lfoWaveform = 2;
        w.lfoSpeed = 40;           // 6.25 Hz
        w.lfoDepth = 127;
        w.lfoPitchAmount = 16;
        w.lfoDelay = 0;
        w.lfoMode = 1;
        eps::Sampler s(set);
        s.load(in, err);
        s.noteOn(50, 100);
        double lo = 1e9, hi = 0;
        for (int i = 0; i < 200; ++i) {
            run(s, 64);
            const double f = fc(s, sounding(s));
            lo = std::min(lo, f);
            hi = std::max(hi, f);
        }
        const double cents = 1200 * std::log2(hi / lo);
        printf("  LFO sine +-1 semitone: swing %.0f cent (200)\n", cents);
        CHECK(std::fabs(cents - 200) < 10);

        w.lfoDelay = 60;           // 6.6 s fade in: hardly moves in the first 0.1 s
        eps::Sampler d(set);
        d.load(in, err);
        d.noteOn(50, 100);
        lo = 1e9; hi = 0;
        for (int i = 0; i < 70; ++i) {
            run(d, 64);
            const double f = fc(d, sounding(d));
            lo = std::min(lo, f);
            hi = std::max(hi, f);
        }
        printf("  LFO delay 3 s: first 0.1 s swing %.0f cent\n", 1200 * std::log2(hi / lo));
        CHECK(1200 * std::log2(hi / lo) < 30);

        w.lfoDelay = 0;
        w.lfoDepth = 0;
        w.lfoDepthModSource = eps::kSrcWheel;
        w.lfoDepthModAmount = 127;
        eps::Sampler m(set);
        m.load(in, err);
        m.noteOn(50, 100);
        lo = 1e9; hi = 0;
        for (int i = 0; i < 200; ++i) { run(m, 64); const double f = fc(m, sounding(m)); lo = std::min(lo, f); hi = std::max(hi, f); }
        const double still = 1200 * std::log2(hi / lo);
        m.setModWheel(1.f);
        lo = 1e9; hi = 0;
        for (int i = 0; i < 200; ++i) { run(m, 64); const double f = fc(m, sounding(m)); lo = std::min(lo, f); hi = std::max(hi, f); }
        printf("  wheel -> LFO depth: %.0f cent without, %.0f cent with the wheel\n", still, 1200 * std::log2(hi / lo));
        CHECK(still < 5);
        CHECK(1200 * std::log2(hi / lo) > 180);
    }
    // pitch modulation by velocity: +1 semitone at full velocity
    {
        eps::Instrument in = base;
        in.waveSamples[1].pitchModSource = eps::kSrcVel1;
        in.waveSamples[1].pitchModAmount = 8;
        eps::Sampler a(set), b(set);
        a.load(in, err);
        b.load(in, err);
        a.noteOn(50, 127);
        b.noteOn(50, 1);
        run(a, 256);
        run(b, 256);
        const double cents = 1200 * std::log2(fc(a, sounding(a)) / fc(b, sounding(b)));
        printf("  pitch mod VEL +8: %.0f cent between velocity 127 and 1 (~99)\n", cents);
        CHECK(std::fabs(cents - 99) < 6);
    }
    // filter 1 by pressure, filter 2 by the filter envelope
    {
        eps::Instrument in = base;
        eps::WaveSample &w = in.waveSamples[1];
        w.filterCutoff[0] = 0;
        w.filterEnvAmount[0] = 0;
        w.filterKeyAmount[0] = 0;
        w.filterModSource[0] = eps::kSrcPress;
        w.filterModAmount[0] = 127;
        eps::Sampler s(set);
        s.load(in, err);
        s.noteOn(50, 100);
        run(s, 256);
        const int v = sounding(s);
        const uint32_t closed = s.chipForDebug().voice(v).k1;
        s.setPressure(1.f);
        run(s, 256);
        const uint32_t open = s.chipForDebug().voice(v).k1;
        s.setPressure(0.f);
        s.setPolyPressure(50, 1.f);
        run(s, 256);
        const uint32_t poly = s.chipForDebug().voice(v).k1;
        printf("  filter 1 by pressure: K1 %u -> %u (poly pressure %u)\n", closed, open, poly);
        CHECK(open > 8 * (closed + 16));
        CHECK(poly == open);
    }
    // amp by velocity, crossfade by key, pan by wheel
    {
        eps::Instrument in = base;
        in.waveSamples[1].ampModSource = eps::kSrcVel1;
        in.waveSamples[1].ampModAmount = 127;
        eps::Sampler a(set), b(set);
        a.load(in, err);
        b.load(in, err);
        a.noteOn(50, 127);
        b.noteOn(50, 32);
        run(a, 256);
        run(b, 256);
        const int la = s_vol(a), lb = s_vol(b);
        printf("  amp mod VEL1 127: volume code %d at velocity 127, %d at 32\n", la, lb);
        CHECK(la - lb >= 25);       // 4 bits per 6 dB step: about -12 dB

        eps::Instrument x = base;
        eps::WaveSample &w = x.waveSamples[1];   // keys 21..59
        w.volumeModCrossfadeCurve = 1;
        w.ampModSource = eps::kSrcKbd;
        w.crossfade[0] = 0; w.crossfade[1] = 0; w.crossfade[2] = 20; w.crossfade[3] = 50;
        eps::Sampler lo(set), hi(set);
        lo.load(x, err);
        hi.load(x, err);
        lo.noteOn(24, 100);        // key 22: KBD (22-60)/48 -> x = 13: full
        hi.noteOn(60, 100);        // key 58: x = 61: beyond D -> silent
        run(lo, 256);
        run(hi, 256);
        printf("  crossfade by KBD: volume %d low, %d high\n", s_vol(lo), s_vol(hi));
        CHECK(s_vol(lo) > 150 && s_vol(hi) == 0);

        eps::Instrument p = base;
        p.waveSamples[1].panModSource = eps::kSrcWheel;
        p.waveSamples[1].panModAmount = 127;
        p.waveSamples[1].panSigned = 0;
        eps::Sampler ps(set);
        ps.load(p, err);
        ps.noteOn(50, 100);
        run(ps, 256);
        const int v = sounding(ps);
        const int l0 = int(ps.chipForDebug().voice(v).lvol), r0 = int(ps.chipForDebug().voice(v).rvol);
        ps.setModWheel(1.f);
        run(ps, 256);
        const int l1 = int(ps.chipForDebug().voice(v).lvol), r1 = int(ps.chipForDebug().voice(v).rvol);
        printf("  pan by wheel: L/R %d/%d -> %d/%d\n", l0, r0, l1, r1);
        // centre: right 0.6 dB lower; a positive pan value raises LVOL (measured)
        CHECK(std::abs(l0 - r0) <= 2 && l1 > r1 + 40);
    }
    // glide: layer A mono, glide time 30 (0.44 s)
    {
        eps::Instrument in = base;
        in.layers[0].glideMode = 1;
        in.layers[0].glideTime = 30;
        eps::Sampler s(set);
        s.load(in, err);
        s.noteOn(38, 100);         // key 36
        run(s, 4096);
        const double from = fc(s, sounding(s));
        s.noteOn(50, 100);         // key 48: an octave up
        run(s, 2048);
        CHECK(s.activeVoiceCount() == 1);
        const double mid = fc(s, sounding(s));
        run(s, int(SR));
        const double to = fc(s, sounding(s));
        printf("  glide: %.0f -> %.0f (after 46 ms) -> %.0f\n", from, mid, to);
        CHECK(std::fabs(to / from - 2.0) < 0.01);
        CHECK(mid > from * 1.01 && mid < to * 0.99);
    }
    // noise on the pitch
    {
        eps::Instrument in = base;
        in.waveSamples[1].randomModAmount = 127;   // +-1 semitone
        in.waveSamples[1].randomFreq = 99;         // new value 2.9 times a second
        eps::Sampler s(set);
        s.load(in, err);
        s.noteOn(50, 100);
        double lo = 1e9, hi = 0;
        for (int i = 0; i < 1400; ++i) { run(s, 64); const double f = fc(s, sounding(s)); lo = std::min(lo, f); hi = std::max(hi, f); }
        printf("  random on pitch: swing %.0f cent in 2 s\n", 1200 * std::log2(hi / lo));
        CHECK(1200 * std::log2(hi / lo) > 30 && 1200 * std::log2(hi / lo) <= 205);
    }
    // Loop start-X on WS1 (loop 200..1000, first in memory): the loop grows
    // backwards 2^(5.25 * value) times (measured) and the pitch follows the loop
    // length. Wheel 0.05: 800 * 2^(0.2625) = 959 -> start 41; wheel 0.1:
    // 800 * 2^(0.525) = 1151 -> start -151, in front of the sample (in the
    // wavesample header, as on the original). Positions relative to the sample.
    {
        auto lsx = [&](float wheel, uint32_t &st, double &fcv) {
            eps::Instrument in = base;
            eps::WaveSample &w = in.waveSamples[1];
            w.waveModRaw[0] = eps::kSrcWheel;
            w.waveModRaw[1] = 127;
            w.waveModRaw[2] = 6;
            w.waveModRaw[3] = 6;   // loop start-X
            eps::Sampler s(set);
            s.load(in, err);
            s.setModWheel(wheel * 127.f / 128.f);
            s.noteOn(50, 100);
            run(s, 64); run(s, 64);
            const int v = sounding(s);
            st = s.chipForDebug().voice(v).start >> 11;
            fcv = fc(s, v);
        };
        uint32_t a0, a1, a2; double f0v, f1v, f2v;
        lsx(0.f, a0, f0v); lsx(0.05f, a1, f1v); lsx(0.1f, a2, f2v);
        const long b = long(a0) - 200;                    // sample start in memory
        const long st1 = long(a1) - b, st2 = long(a2) - b;
        printf("  loop start-X: start 200 / %ld / %ld, pitch x%.3f / x%.3f\n", st1, st2, f1v / f0v, f2v / f0v);
        CHECK(st1 >= 38 && st1 <= 44 && st2 >= -154 && st2 <= -148);
        CHECK(std::fabs(f1v / f0v - (1000.0 - st1) / 800.0) < 0.01 && std::fabs(f2v / f0v - (1000.0 - st2) / 800.0) < 0.01);
    }

    // wave mod types 1 (sample start), 3 (loop start), 4 (loop end) on WS1
    // (loop 200..1000, end 1099), wheel = 1, range 13 = 256 samples; travel
    // 256 * 127/128 * 127/128 = 252
    {
        for (int type : {1, 3, 4}) {
            eps::Instrument in = base;
            eps::WaveSample &w = in.waveSamples[1];
            w.waveModRaw[0] = eps::kSrcWheel;
            w.waveModRaw[1] = 127;
            w.waveModRaw[2] = 13;          // range 256 samples
            w.waveModRaw[3] = type + 1;    // stored type numbers start with 0 = off
            eps::Sampler s(set);
            s.load(in, err);
            s.setModWheel(1.f);
            s.noteOn(50, 100);
            const int v = sounding(s);
            const otis::Voice &cv = s.chipForDebug().voice(v);
            double acc0 = double(cv.accum) / 2048.0;
            run(s, 64);
            uint32_t st = cv.start >> 11, en = cv.end >> 11;
            // positions relative to the sample start in memory (base: the
            // unmoved loop point: end 1000 for types 1 and 3, start 200 for 4)
            const uint32_t b = type == 4 ? st - 200 : en - 1000;
            acc0 -= b; st -= b; en -= b;
            printf("  wave mod type %d: start %u end %u, first position %.1f\n", type, st, en, acc0);
            if (type == 1) CHECK(acc0 >= 240 && acc0 < 256);          // start moved, into the loop
            if (type == 3) CHECK(st >= 200 + 250 && st <= 200 + 254 && en == 1000);
            if (type == 4) CHECK(st == 200 && en == 1099);            // clamped to the sample end
        }
    }
}

int main(int argc, char **argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    eps::Disk disk;
    std::string err;
    if (!disk.open(dir + "/test.efe", err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    eps::Instrument inst;
    if (!disk.loadInstrument(*disk.instruments()[0], inst, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    // The fixture sets LFO, pitch envelope and modulation values (checked by
    // test_loader). For the pitch and level tests below they are switched off;
    // modulation_tests() turns them on one by one.
        neutral(inst);
    modulation_tests(inst);

    eps::Sampler::Settings set;
    set.sampleRate = SR;

    struct Case { int note; double expect; const char *what; };
    // WS2: 64-sample sine at 25 kHz, root C5 -> 390.625 Hz, forward loop
    // WS1: 100-sample saw period at 44.1 kHz, root C3 -> 441 Hz, forward loop
    // Both have fine tune -5 (1/128 semitone units) -> factor kFine.
    const double kFine = std::pow(2.0, -5.0 / 128.0 / 12.0);
    const Case cases[] = {
        {74, kFine * 25000.0 / 64, "WS2 sine at root (transpose -2)"},
        {86, kFine * 2 * 25000.0 / 64, "WS2 sine +12"},
        {81, kFine * 25000.0 / 64 * std::pow(2.0, 7 / 12.0), "WS2 sine +7"},
        {48 + 2, kFine * 441.0 * std::pow(2.0, 0 / 12.0), "WS1 saw at root (transpose -2)"},
        {36 + 2, kFine * 441.0 / 2, "WS1 saw -12"},
    };
    for (const Case &c : cases) {
        eps::Sampler s(set);
        CHECK(s.load(inst, err));
        s.noteOn(c.note, 100);
        auto x = render(s, int(SR));          // 1 s, well past the 640/1000-sample loops
        double f = pitch(x, 30000, 4096);
        double cents = f > 0 ? 1200 * std::log2(f / c.expect) : 9999;
        printf("  note %3d  %-32s expect %8.3f Hz  got %8.3f Hz  (%+.2f cent)  rms %.4f\n", c.note, c.what,
               c.expect, f, cents, rms(x, 30000, 4096));
        // Tolerance: FC quantization (<= ~1.7 cent after rounding) plus the
        // resolution of the autocorrelation estimate.
        CHECK(std::fabs(cents) < 4.0);
        CHECK(rms(x, 30000, 4096) > 1e-3);     // loop keeps it sounding
    }

    // Release: note off -> silent and voice freed (release time ~ t5+t6).
    {
        eps::Sampler s(set);
        s.load(inst, err);
        s.noteOn(72, 100);
        render(s, int(SR * 0.5));
        CHECK(s.activeVoiceCount() == 1);
        s.noteOff(72);
        auto x = render(s, int(SR * 120));   // max release 49+49 s
        CHECK(s.activeVoiceCount() == 0);
        CHECK(rms(x, x.size() - 4096, 4096) < 1e-6);
    }

    // Patch 1 enables layer B (velocity 64..127) holding WS3, a copy of WS1
    // without loop: plays 1000 samples, then the chip stops the voice.
    {
        eps::Sampler s(set);
        s.load(inst, err);
        s.setPatch(1);
        s.noteOn(62, 100);                     // key 60 -> layer A WS2 + layer B WS3
        render(s, 256);
        CHECK(s.activeVoiceCount() == 2);
        render(s, int(SR * 0.2));
        CHECK(s.activeVoiceCount() == 1);      // WS3 one-shot finished, WS2 loops
        eps::Sampler s2(set);
        s2.load(inst, err);
        s2.setPatch(1);
        s2.noteOn(62, 40);                     // below layer B velocity range
        render(s2, 256);
        CHECK(s2.activeVoiceCount() == 1);
    }

    // Transwave: patch 3 = layer C = WS4 (8 waves of 100 samples, wave k has
    // harmonic k+1 mixed in). Mod wheel moves the loop window wave by wave.
    {
        auto harmonicLevel = [](const std::vector<float> &x, size_t from, double freq) {
            double re = 0, im = 0;
            for (size_t i = 0; i < 8820; ++i) {
                double w = 0.5 - 0.5 * std::cos(2 * M_PI * double(i) / 8820.0);
                re += w * x[from + i] * std::cos(2 * M_PI * freq * double(i) / SR);
                im += w * x[from + i] * std::sin(2 * M_PI * freq * double(i) / SR);
            }
            return std::sqrt(re * re + im * im);
        };
        const double f0 = 441.0 * kFine;   // key 69 = root, 100-sample waves at 44.1 kHz
        double ratio[3];
        const float wheel[3] = {0.f, 0.5f, 1.f};
        // 6 steps reachable; floor(127/128 * wheel * 6) waves (measured): 0.5 -> wave 2
        // -> harmonic 3, 1.0 -> wave 5 -> harmonic 6
        const int harmonic[3] = {1, 3, 6};
        for (int t = 0; t < 3; ++t) {
            eps::Sampler s(set);
            s.load(inst, err);
            s.setPatch(3);
            s.setModWheel(wheel[t]);
            s.noteOn(69 + 2, 120);
            auto x = render(s, int(SR));
            double fund = harmonicLevel(x, 20000, f0);
            double h = harmonicLevel(x, 20000, f0 * harmonic[t]);
            ratio[t] = t == 0 ? harmonicLevel(x, 20000, f0 * 7) / fund : h / fund;
            double p = pitch(x, 20000, 4096);
            printf("  transwave wheel %.1f: pitch %.2f Hz, harmonic %d / fundamental = %.3f\n", wheel[t],
                   p, t == 0 ? 7 : harmonic[t], ratio[t]);
            CHECK(std::fabs(1200 * std::log2(p / f0)) < 5.0);
        }
        CHECK(ratio[0] < 0.02);    // wave 0: pure sine
        CHECK(ratio[1] > 0.25);    // wave 2: 3rd harmonic
        CHECK(ratio[2] > 0.6);     // wave 5: strong 6th harmonic

        // Moving the wheel while the note is held changes the wave without
        // restarting the voice.
        eps::Sampler s(set);
        s.load(inst, err);
        s.setPatch(3);
        s.noteOn(71, 120);
        auto a = render(s, int(SR * 0.5));
        s.setModWheel(1.f);
        auto b = render(s, int(SR * 0.5));
        CHECK(s.activeVoiceCount() == 1);
        CHECK(harmonicLevel(b, 10000, f0 * 6) / harmonicLevel(b, 10000, f0) > 0.6);
        CHECK(harmonicLevel(a, 10000, f0 * 6) / harmonicLevel(a, 10000, f0) < 0.02);
    }

    // Transwave ignores RANGE (measured): with range 256 samples, full wheel
    // and amount 127 the window still takes floor(127/128 * 6) = 5 waves.
    {
        eps::Instrument limited = inst;
        limited.waveSamples[4].waveModRaw[2] = 13;  // range 256 samples
        uint32_t st[2];
        for (int k = 0; k < 2; ++k) {
            eps::Sampler s(set);
            s.load(limited, err);
            s.setPatch(3);
            s.setModWheel(float(k));
            s.noteOn(71, 120);
            render(s, 1024);
            st[k] = s.chipForDebug().voice(0).start >> 11;
        }
        printf("  transwave, range 256: window moved %u samples (500)\n", st[1] - st[0]);
        CHECK(st[1] - st[0] == 500);
    }

    // Performance controls of eps~: bend, tune, transpose, sustain, volume, cutoff.
    {
        auto pitchOf = [&](auto setup) {
            eps::Sampler s(set);
            s.load(inst, err);
            setup(s);
            s.noteOn(50, 100);                // WS1 saw at root: 441 Hz * fine
            auto x = render(s, int(SR * 0.8));
            return pitch(x, 20000, 4096);
        };
        const double base = pitchOf([](eps::Sampler &) {});
        const double bent = pitchOf([](eps::Sampler &s) { s.setBendRange(2); s.setBend(1.f); });
        eps::Sampler::Offsets o;
        o.tune = 50;
        const double tuned = pitchOf([&](eps::Sampler &s) { s.setOffsets(o); });
        o = eps::Sampler::Offsets();
        o.transpose = -12;
        const double down = pitchOf([&](eps::Sampler &s) { s.setOffsets(o); });
        printf("  bend +2: %+.1f cent, tune +50: %+.1f cent, transpose -12: %+.1f cent\n",
               1200 * std::log2(bent / base), 1200 * std::log2(tuned / base), 1200 * std::log2(down / base));
        CHECK(std::fabs(1200 * std::log2(bent / base) - 200) < 5);
        CHECK(std::fabs(1200 * std::log2(tuned / base) - 50) < 5);
        CHECK(std::fabs(1200 * std::log2(down / base) + 1200) < 5);

        // sustain: note off while the pedal is down keeps the voice; pedal up releases it
        eps::Sampler s(set);
        s.load(inst, err);
        s.setSustain(true);
        s.noteOn(50, 100);
        render(s, 1000);
        s.noteOff(50);
        render(s, int(SR * 2));
        CHECK(s.activeVoiceCount() == 1);
        s.setSustain(false);
        render(s, int(SR * 100));
        CHECK(s.activeVoiceCount() == 0);

        // volume and cutoff offset act on a sounding note
        auto play = [&](float volume, int cutoff) {
            eps::Sampler v(set);
            v.load(inst, err);
            v.noteOn(50, 100);
            render(v, int(SR * 0.3));          // the offsets act on a note already sounding
            v.setVolume(volume);
            eps::Sampler::Offsets o2;
            o2.cutoff = cutoff;
            v.setOffsets(o2);
            return render(v, int(SR * 0.5));
        };
        auto loud = play(1.f, 0), half = play(0.5f, 0), darker = play(1.f, -63);
        const double ratio = rms(half, 10000, 4096) / rms(loud, 10000, 4096);
        printf("  volume 0.5: rms ratio %.3f, cutoff -63: rms ratio %.3f\n", ratio,
               rms(darker, 10000, 4096) / rms(loud, 10000, 4096));
        CHECK(std::fabs(ratio - 0.5) < 0.02);
        CHECK(rms(darker, 10000, 4096) < 0.7 * rms(loud, 10000, 4096));

        // voices: fewer voices steal sooner
        eps::Sampler w(set);
        w.load(inst, err);
        w.setVoices(4);
        for (int n = 40; n < 50; ++n) w.noteOn(n, 100);
        render(w, 256);
        CHECK(w.activeVoiceCount() == 4);
    }

    // Voice stealing: 30 notes on a 20-voice sampler.
    {
        eps::Sampler s(set);
        s.load(inst, err);
        for (int n = 40; n < 70; ++n) s.noteOn(n, 100);
        render(s, 512);
        CHECK(s.activeVoiceCount() == 20);
    }

    // Key range: notes outside the instrument range are ignored.
    {
        eps::Sampler s(set);
        s.load(inst, err);
        s.noteOn(20, 100);
        s.noteOn(120, 100);
        render(s, 256);
        CHECK(s.activeVoiceCount() == 0);
    }

    // CPU: 20 sustained voices for 10 s.
    {
        eps::Sampler s(set);
        s.load(inst, err);
        for (int n = 0; n < 20; ++n) s.noteOn(50 + n, 100);
        std::vector<float> l(128), r(128);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < int(SR * 10) / 128; ++i) s.process(l.data(), r.data(), 128);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("  CPU: 20 voices, 10 s audio in %.3f s on this machine (%.2f%% of real time)\n", secs,
               secs / 10 * 100);
    }

    // Demo phrase for listening.
    {
        eps::Sampler s(set);
        s.load(inst, err);
        std::vector<float> L, R, l, r;
        const int notes[] = {60, 64, 67, 72, 76, 79, 84};
        for (int n : notes) {
            s.noteOn(n, 110);
            l = render(s, int(SR * 0.35), &r);
            for (size_t i = 0; i < r.size(); ++i) l[i] -= r[i];
            L.insert(L.end(), l.begin(), l.end());
            R.insert(R.end(), r.begin(), r.end());
            s.noteOff(n);
        }
        l = render(s, int(SR * 1.5), &r);
        for (size_t i = 0; i < r.size(); ++i) l[i] -= r[i];
        L.insert(L.end(), l.begin(), l.end());
        R.insert(R.end(), r.begin(), r.end());
        writeWav((dir + "/demo.wav").c_str(), L, R);
    }

    printf(failures ? "%d FAILURE(S)\n" : "all render checks passed\n", failures);
    return failures ? 1 : 0;
}
