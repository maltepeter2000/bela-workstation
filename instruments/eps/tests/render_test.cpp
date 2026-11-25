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

int main(int argc, char **argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    eps::Disk disk;
    std::string err;
    if (!disk.open(dir + "/test.efe", err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    eps::Instrument inst;
    if (!disk.loadInstrument(*disk.instruments()[0], inst, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }

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
        const int harmonic[3] = {1, 4, 7};   // 6 steps reachable: round(0.5*6)=3 -> wave 3 -> harmonic 4
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
        CHECK(ratio[1] > 0.3);     // wave 3: strong 4th harmonic
        CHECK(ratio[2] > 0.6);     // wave 6: strong 7th harmonic

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
        CHECK(harmonicLevel(b, 10000, f0 * 7) / harmonicLevel(b, 10000, f0) > 0.6);
        CHECK(harmonicLevel(a, 10000, f0 * 7) / harmonicLevel(a, 10000, f0) < 0.02);
    }

    // Wave mod RANGE limits the window travel: with range 256 samples only
    // 2 of the 6 possible steps are reachable, so full wheel = wave 2.
    {
        eps::Instrument limited = inst;
        limited.waveSamples[4].waveModRaw[3] = 0;   // 256 samples
        eps::Sampler s(set);
        s.load(limited, err);
        s.setPatch(3);
        s.setModWheel(1.f);
        s.noteOn(71, 120);
        render(s, 1024);
        const otis::Voice &v = s.chipForDebug().voice(0);
        const uint32_t base = (v.end >> 11) - 100 - 200;   // window start = base + 200
        CHECK(((v.start >> 11) - base) == 200);
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
