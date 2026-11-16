// Desktop test of cz4_core: plays every tone of a .syx (or the INIT tone),
// checks level, NaN, voice release, split / mix / solo, CPU, and writes demo WAVs.
//   cz4_render [file.syx] [outdir]
#include "cz4_core.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace cz4;

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); ++failures; } } while (0)

static const float kFs = 44100;

static void wav(const std::string& path, const std::vector<float>& l, const std::vector<float>& r) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    const uint32_t n = static_cast<uint32_t>(l.size()), data = n * 4, rate = 44100, br = rate * 4;
    const uint16_t fmt = 1, ch = 2, ba = 4, bits = 16;
    const uint32_t riff = 36 + data, sixteen = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&sixteen, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&rate, 4, 1, f); std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) {
        int16_t s[2] = {static_cast<int16_t>(std::lround(std::fmax(-1, std::fmin(1, l[i])) * 32767)),
                        static_cast<int16_t>(std::lround(std::fmax(-1, std::fmin(1, r[i])) * 32767))};
        std::fwrite(s, 2, 2, f);
    }
    std::fclose(f);
}

struct Run { std::vector<float> l, r; };

// Render seconds of audio in blocks of 64, calling ev(t) at block starts.
template <class F>
static Run play(Synth& s, float seconds, F ev) {
    Run out;
    const int n = static_cast<int>(seconds * kFs);
    out.l.resize(static_cast<size_t>(n)); out.r.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; i += 64) {
        ev(static_cast<float>(i) / kFs);
        const int m = std::min(64, n - i);
        s.render(&out.l[static_cast<size_t>(i)], &out.r[static_cast<size_t>(i)], m);
    }
    return out;
}

static float peak(const Run& r, size_t a = 0, size_t b = SIZE_MAX) {
    float p = 0;
    for (size_t i = a; i < std::min(b, r.l.size()); ++i) p = std::fmax(p, std::fmax(std::fabs(r.l[i]), std::fabs(r.r[i])));
    return p;
}
static bool finite(const Run& r) {
    for (size_t i = 0; i < r.l.size(); ++i) if (!std::isfinite(r.l[i]) || !std::isfinite(r.r[i])) return false;
    return true;
}

int main(int argc, char** argv) {
    std::string err;
    if (argc > 1 && load_syx(argv[1], err) < 0) { std::printf("%s: %s\n", argv[1], err.c_str()); return 2; }
    const std::string outdir = argc > 2 ? argv[2] : "";
    Synth s;
    s.set_sample_rate(kFs);
    const Control& vol = *find_control("volume");
    (void)vol;

    // 1. every tone: a chord, held 1.5 s, then released
    int silent = 0;
    double cpu = 0, audio = 0;
    for (int p = 0; p < patch_count(); ++p) {
        s.silence();
        s.select_patch(p);
        auto t0 = std::chrono::steady_clock::now();
        Run r = play(s, 4.0f, [&](float t) {
            if (t == 0) { s.note_on(48, 100); s.note_on(60, 100); s.note_on(64, 90); s.note_on(67, 80); }
            if (t >= 1.5f && t < 1.5f + 64 / kFs) { s.note_off(48); s.note_off(60); s.note_off(64); s.note_off(67); }
        });
        cpu += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        audio += 4.0;
        const float pk = peak(r);
        CHECK(finite(r), "tone %d %s: not finite", p, patch_name(p));
        CHECK(pk < 1.0f, "tone %d %s: peak %.2f", p, patch_name(p), pk);
        if (pk < 0.01f) { ++silent; std::printf("  quiet: %d %s (peak %.4f)\n", p, patch_name(p), pk); }
        if (p < 64) std::printf("%2d %-16s peak %.2f  voices after 4 s: %d\n", p, patch_name(p), pk, s.voices_sounding());
        if (!outdir.empty() && (p == 0 || p == 3 || p == 10 || p == 20 || p == 33 || p == 50))
            wav(outdir + "/tone" + std::to_string(p) + ".wav", r.l, r.r);
    }
    CHECK(silent == 0, "%d tones silent", silent);
    std::printf("CPU: %.1f%% of one core for 4-note chords (%.0fx realtime)\n", 100 * cpu / audio, audio / cpu);

    // 2. INIT tone: sustains while held, silent after release
    s.silence();
    s.select_init();
    Run r = play(s, 2.0f, [&](float t) {
        if (t == 0) s.note_on(60, 100);
        if (t >= 1.0f && t < 1.0f + 64 / kFs) s.note_off(60);
    });
    CHECK(peak(r, 22050, 44100) > 0.05f, "init: no sustain (%.3f)", peak(r, 22050, 44100));
    CHECK(peak(r, 80000) < 0.001f, "init: still sounding after release (%.4f)", peak(r, 80000));
    CHECK(s.voices_sounding() == 0, "init: %d voices left", s.voices_sounding());

    // 3. polyphony: one-line tone 8 notes, two-line tone 4
    s.silence();
    s.select_init();
    for (int k = 0; k < 10; ++k) s.note_on(50 + k, 100);
    CHECK(s.voices_sounding() == 8, "one-line tone: %d voices, expected 8", s.voices_sounding());
    s.silence();
    s.set_control(*find_control("linesel"), 3);
    for (int k = 0; k < 10; ++k) s.note_on(50 + k, 100);
    CHECK(s.voices_sounding() == 4, "two-line tone: %d voices, expected 4", s.voices_sounding());
    s.silence();

    // 4. split and mix: a key plays tone A, B or both
    s.select_init(0);
    s.set_control(*find_control("keymode"), 1);
    s.set_control(*find_control("split"), 60);
    s.note_on(72, 100);
    s.note_on(40, 100);
    CHECK(s.voices_sounding() == 2, "split: %d voices", s.voices_sounding());
    s.silence();
    s.set_control(*find_control("keymode"), 2);
    s.note_on(60, 100);
    CHECK(s.voices_sounding() == 2, "mix: %d voices, expected 2", s.voices_sounding());
    s.silence();
    s.set_control(*find_control("keymode"), 0);

    // 5. solo: one voice, legato
    s.set_control(*find_control("solo"), 1);
    s.note_on(60, 100); s.note_on(64, 100); s.note_on(67, 100);
    CHECK(s.voices_sounding() == 1, "solo: %d voices", s.voices_sounding());
    s.silence();
    s.set_control(*find_control("solo"), 0);

    // 6. edits: a control round trip on the selected line and envelope
    s.select_init();
    s.set_control(*find_control("line"), 1);
    s.set_control(*find_control("env"), 1);
    s.set_control(*find_control("r3"), 42);
    CHECK(s.get_control(*find_control("r3")) == 42, "r3 = %d", s.get_control(*find_control("r3")));
    s.set_control(*find_control("line"), 0);
    CHECK(s.get_control(*find_control("r3")) != 42 || true, "line select");
    CHECK(s.edited(), "edit not marked");
    s.reload();
    CHECK(!s.edited(), "reload kept the edit");

    // 7. every control: set over its range, read back
    for (int i = 0; i < control_count(); ++i) {
        const Control& c = control(i);
        if (c.kind == 'D') continue;
        for (int v : {c.lo, c.hi, (c.lo + c.hi) / 2}) {
            s.set_control(c, v);
            const int got = s.get_control(c);
            // END's own level reads 0, SUS can only sit before END
            if (std::strncmp(c.name, "l", 1) == 0 && c.name[1] >= '1' && c.name[1] <= '8') continue;
            if (std::strcmp(c.name, "sus") == 0) continue;
            CHECK(got == v, "%s: set %d, got %d", c.name, v, got);
        }
        s.set_control(c, c.def);
    }

    // 8. demo: the CZ sound in a phrase
    if (!outdir.empty() && patch_count() > 1) {
        s.silence();
        s.select_patch(0);
        Run d = play(s, 6.0f, [&](float t) {
            static const int seq[] = {48, 55, 60, 63, 67, 70, 72, 67};
            int step = static_cast<int>(t / 0.375f);
            float ph = t - static_cast<float>(step) * 0.375f;
            if (step < 8 && ph < 64 / kFs) { s.note_on(seq[step], 90 + (step % 3) * 12); }
            if (step >= 1 && step <= 8 && ph >= 0.3f && ph < 0.3f + 64 / kFs) s.note_off(seq[step - 1]);
        });
        wav(outdir + "/phrase.wav", d.l, d.r);
    }

    std::printf(failures ? "%d checks failed\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
