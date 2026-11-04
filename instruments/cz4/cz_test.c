/* Desktop test for cz_core: sanity checks, chip-vs-clean error, CPU timing
 * and demo renders (24-bit stereo WAV). Build: make test && ./cz_test */
#include "cz_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define FS 44100.0f
#define BLOCK 16u

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- WAV out */
typedef struct { FILE *f; unsigned frames; } wav_t;
static void put32(FILE *f, unsigned v) { fputc(v, f); fputc(v >> 8, f); fputc(v >> 16, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned v) { fputc(v, f); fputc(v >> 8, f); }
static int wav_open(wav_t *w, const char *path)
{
    w->f = fopen(path, "wb"); w->frames = 0;
    if (!w->f) return -1;
    fwrite("RIFF\0\0\0\0WAVEfmt ", 1, 16, w->f);
    put32(w->f, 16); put16(w->f, 1); put16(w->f, 2); put32(w->f, (unsigned)FS);
    put32(w->f, (unsigned)FS * 6u); put16(w->f, 6); put16(w->f, 24);
    fwrite("data\0\0\0\0", 1, 8, w->f);
    return 0;
}
static void wav_write(wav_t *w, const float *l, const float *r, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        const float s[2] = {l[i], r[i]};
        for (int c = 0; c < 2; c++) {
            float x = s[c] > 1.0f ? 1.0f : s[c] < -1.0f ? -1.0f : s[c];
            int v = (int)lrintf(x * 8388607.0f);
            fputc(v & 255, w->f); fputc((v >> 8) & 255, w->f); fputc((v >> 16) & 255, w->f);
        }
    }
    w->frames += n;
}
static void wav_close(wav_t *w)
{
    unsigned data = w->frames * 6u;
    fseek(w->f, 4, SEEK_SET); put32(w->f, 36u + data);
    fseek(w->f, 40, SEEK_SET); put32(w->f, data);
    fclose(w->f);
}

/* ------------------------------------------------- 1. chip vs clean error */
static void test_chip_vs_clean(void)
{
    double worst = 0.0; int ww = 0, wn = 0, wd = 0;
    for (int wave = 0; wave < 8; wave++) for (int win = 0; win < 7; win++) {
        for (int d = 0; d <= 1020; d += 60) {
            double err = 0.0; int count = 0;
            for (uint32_t k = 0; k < 4096; k++) {
                uint32_t ph = k << 20;           /* covers the 11-bit grid twice */
                float a = cz_wave_clean((unsigned)wave, (unsigned)win, ph, (float)d), b = cz_wave_chip((unsigned)wave, (unsigned)win, ph, (float)d);
                CHECK(isfinite(a) && isfinite(b), "non-finite wave %d win %d dcw %d", wave, win, d);
                CHECK(a >= -1.001f && a <= 1.001f && b >= -1.001f && b <= 1.001f,
                      "range wave %d win %d dcw %d: %f %f", wave, win, d, a, b);
                err += (double)(a - b) * (a - b); count++;
            }
            err = sqrt(err / count);
            if (err > worst) { worst = err; ww = wave; wn = win; wd = d; }
        }
    }
    printf("chip vs clean: worst RMS difference %.4f (wave %d, window %d, dcw %d)\n", worst, ww, wn, wd);
    /* quantization noise is small; a logic error in the integer path is not */
    CHECK(worst < 0.08, "chip path deviates too much from clean path");
}

/* --------------------------------------------------- 2. render sanity */
static void test_render_sanity(void)
{
    static cz_engine_t e;
    float l[BLOCK], r[BLOCK];
    const float eras[3] = {0.0f, 0.5f, 1.0f};
    for (int ei = 0; ei < 3; ei++) for (int mode = 0; mode < 4; mode++) for (int mod = 0; mod < 4; mod++) {
        cz_init(&e, FS);
        e.line_mode = mode; e.mod = mod; e.detune = cz_detune(1, 0, 7, 10);
        e.line[0] = (cz_line_t){0.0f, 6.0f, 1};     /* saw / reso tri pair */
        e.line[1] = (cz_line_t){3.4f, 3.4f, 0};     /* between double sine and saw-pulse */
        cz_set_era(&e, eras[ei]); e.vint.drift = 1.0f;
        const float notes[4] = {48, 55, 60, 64};
        for (int v = 0; v < 4; v++) cz_voice_on(&e, v, notes[v]);
        double sum = 0.0, peak = 0.0, sum_l = 0.0; unsigned frames = (unsigned)FS * 2u;
        for (unsigned f = 0; f < frames; f += BLOCK) {
            float d = 0.5f + 0.5f * sinf((float)f * 0.0003f);
            cz_render(&e, l, r, BLOCK, d, d);
            for (unsigned i = 0; i < BLOCK; i++) {
                CHECK(isfinite(l[i]) && isfinite(r[i]), "non-finite output era %.1f mode %d mod %d", eras[ei], mode, mod);
                if (!isfinite(l[i])) return;
                sum += l[i] * l[i]; sum_l += l[i];
                if (fabs(l[i]) > peak) peak = fabs(l[i]);
                if (fabs(r[i]) > peak) peak = fabs(r[i]);
            }
        }
        double rms = sqrt(sum / frames), dc = sum_l / frames;
        CHECK(peak < 1.0 && rms > 0.01, "level era %.1f mode %d mod %d: peak %.3f rms %.3f", eras[ei], mode, mod, peak, rms);
        CHECK(fabs(dc) < 0.02, "DC offset era %.1f mode %d mod %d: %.4f", eras[ei], mode, mod, dc);
        if (mode == 3 && mod == 0)
            printf("render era %.1f, mode 1+2': peak %.3f, rms %.3f, dc %+.4f\n", eras[ei], peak, rms, dc);
    }
    /* gate reset: all phases to zero */
    cz_init(&e, FS); cz_voice_on(&e, 0, 60); cz_render(&e, l, r, BLOCK, 0.3f, 0.3f);
    cz_gate_reset(&e);
    CHECK(e.v[0].ph[0] == 0 && e.v[0].tog[0] == 0, "gate reset");
    /* silent voices stay silent */
    cz_init(&e, FS); cz_render(&e, l, r, BLOCK, 1.0f, 1.0f);
    CHECK(l[BLOCK - 1] == 0.0f && r[BLOCK - 1] == 0.0f, "idle engine not silent");
}

/* ------------------------------------------- 2b. morph has no jumps */
static void test_morph_continuity(void)
{
    static cz_engine_t a, b;
    float la[256], ra[256], lb[256], rb[256];
    double worst = 0.0;
    for (int k = 1; k <= 10; k++) {
        cz_init(&a, FS); cz_init(&b, FS);
        a.morph_ms = b.morph_ms = 0.0f;
        a.line[0].pos = (float)k - 0.001f; b.line[0].pos = (float)k + (k < 10 ? 0.001f : 0.0f);
        cz_voice_on(&a, 0, 57.0f); cz_voice_on(&b, 0, 57.0f);
        for (int rep = 0; rep < 8; rep++) {
            cz_render(&a, la, ra, 256, 0.6f, 0.6f); cz_render(&b, lb, rb, 256, 0.6f, 0.6f);
            for (int i = 0; i < 256; i++) if (fabs(la[i] - lb[i]) > worst) worst = fabs(la[i] - lb[i]);
        }
    }
    printf("morph continuity: max output step across integer positions %.5f\n", worst);
    CHECK(worst < 0.005, "wave morph jumps at an integer position");
}

/* ------------------------------- 2c. DCW per line, line 2 split off */
static void test_lines(void)
{
    static cz_engine_t a, b;
    float la[64], ra[64], lb[64], rb[64];
    double d = 0.0, quiet = 0.0, loud = 0.0;
    /* at mix 1 only line 2 is heard: line 1's DCW must change nothing */
    cz_init(&a, FS); cz_init(&b, FS);
    a.line_mode = b.line_mode = CZ_LINE_1_2; a.line_amp[0] = b.line_amp[0] = 0.0f; a.line_amp[1] = b.line_amp[1] = 1.0f;
    a.amp_s[0] = b.amp_s[0] = 0.0f; a.amp_s[1] = b.amp_s[1] = 1.0f;
    cz_voice_on(&a, 0, 48); cz_voice_on(&b, 0, 48);
    for (int k = 0; k < 100; k++) {
        cz_render2(&a, la, ra, 64, 0.0f, 0.0f, 0.3f, 0.3f); cz_render2(&b, lb, rb, 64, 0.9f, 0.9f, 0.3f, 0.3f);
        for (int i = 0; i < 64; i++) if (fabs(la[i] - lb[i]) > d) d = fabs(la[i] - lb[i]);
    }
    CHECK(d == 0.0, "line 1 DCW leaks into line 2 (%g)", d);
    /* split: line 2 silent until it gets its own note, then sounds */
    cz_init(&a, FS); a.line_mode = CZ_LINE_1_2; a.line_amp[0] = a.amp_s[0] = 0.0f; a.line_amp[1] = a.amp_s[1] = 1.0f; a.split = 1;
    cz_voice_on(&a, 0, 48);
    for (int k = 0; k < 50; k++) { cz_render(&a, la, ra, 64, 0.3f, 0.3f); for (int i = 0; i < 64; i++) quiet += la[i] * la[i]; }
    cz_line2_on(&a, 1, 67);
    for (int k = 0; k < 50; k++) { cz_render(&a, la, ra, 64, 0.3f, 0.3f); for (int i = 0; i < 64; i++) loud += la[i] * la[i]; }
    CHECK(quiet == 0.0 && loud > 1.0, "split line 2: quiet %g loud %g", quiet, loud);
    printf("lines: line 1 DCW isolated at mix 1, split line 2 silent until its note\n");
}

/* ------------- 2d. morph quality: sine at DCW 0, level across the morph */
static void cycle_of(cz_engine_t *e, float pos, float dcw, int pm, int s0, float *x, int n)
{
    float l[64], r[64];
    cz_init(e, FS); e->morph_ms = 0.0f; e->phase_morph = pm; e->sine0 = s0;
    e->level = 1.0f; e->spread = 0.0f; e->line[0].pos = pos; e->line_mode = CZ_LINE_1;
    e->a4 = FS / (float)n; cz_voice_on(e, 0, 69.0f);
    for (int k = 0; k < 20; k++) cz_render(e, l, r, 64, dcw, dcw);
    cz_voice_reset(e, 0); e->v[0].dc[0] = 0.0f;
    for (int i = 0; i < n; i += 64) { cz_render(e, l, r, 64, dcw, dcw); for (int j = 0; j < 64; j++) x[i + j] = l[j]; }
}
static double harm_k(const float *x, int n, int k)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; i++) { re += x[i] * cos(6.283185307179586 * k * i / n); im += x[i] * sin(6.283185307179586 * k * i / n); }
    return 2 * sqrt(re * re + im * im) / n;
}
static double rms_of(const float *x, int n) { double s = 0; for (int i = 0; i < n; i++) s += x[i] * x[i]; return sqrt(s / n); }
static void test_morph_quality(void)
{
    static cz_engine_t e;
    float x[1024]; const int n = 1024;
    double worst_h = 0.0, worst_db = 0.0;
    for (int p = 0; p <= (int)CZ_MORPH_MAX; p++) {        /* sine mode: only the fundamental */
        cycle_of(&e, (float)p, 0.0f, 1, 1, x, n);
        double h1 = harm_k(x, n, 1), rest = 0;
        for (int k = 2; k <= 6; k++) rest += harm_k(x, n, k);
        if (rest / h1 > worst_h) worst_h = rest / h1;
    }
    for (int p = 0; p < (int)CZ_MORPH_MAX; p++) {         /* level at every midpoint */
        double a, b, m;
        cycle_of(&e, (float)p, 0.7f, 1, 1, x, n); a = rms_of(x, n);
        cycle_of(&e, (float)p + 1.0f, 0.7f, 1, 1, x, n); b = rms_of(x, n);
        cycle_of(&e, (float)p + 0.5f, 0.7f, 1, 1, x, n); m = rms_of(x, n);
        double db = 20 * log10(m / (0.5 * (a + b)));
        if (fabs(db) > fabs(worst_db)) worst_db = db;
    }
    printf("morph quality: DCW 0 overtones/fundamental %.4f, worst midpoint level %+.1f dB\n", worst_h, worst_db);
    CHECK(worst_h < 0.01, "sine mode: DCW 0 not a pure sine (%g)", worst_h);
    CHECK(fabs(worst_db) < 2.0, "morph midpoint level off by %.1f dB", worst_db);
}

/* ---------------- 2e. ring modulation keeps the level of line 2 */
static double ring_level(cz_engine_t *e, int ring, float wav, float dcw)
{
    float l[256], r[256]; double s = 0, s2 = 0; long n = 0;
    cz_init(e, FS); e->line_mode = CZ_LINE_1_2; e->morph_ms = 0.0f; e->spread = 0.0f; e->level = 1.0f;
    e->line[0].pos = e->line[1].pos = wav; e->detune = 7.0f; e->mod = ring ? CZ_MOD_RING : 0;
    e->line_amp[0] = e->amp_s[0] = 0.0f; e->line_amp[1] = e->amp_s[1] = 1.0f;
    cz_voice_on(e, 0, 57.0f);
    for (int k = 0; k < 100; k++) cz_render(e, l, r, 256, dcw, dcw);
    for (int k = 0; k < 300; k++) { cz_render(e, l, r, 256, dcw, dcw); for (int i = 0; i < 256; i++) { s += l[i]; s2 += l[i] * l[i]; n++; } }
    double m = s / n; return sqrt(s2 / n - m * m);
}
static void test_ring_level(void)
{
    static cz_engine_t e;
    double worst = 0.0;
    for (int w = 0; w <= (int)CZ_MORPH_MAX; w++) for (int d = 0; d <= 4; d++) {
        double alone = ring_level(&e, 0, (float)w, 0.25f * d);
        if (alone < 0.05) continue;      /* pulse waves at full DCW are nearly silent themselves */
        double db = 20 * log10(ring_level(&e, 1, (float)w, 0.25f * d) / alone);
        if (fabs(db) > fabs(worst)) worst = db;
    }
    printf("ring level: worst difference to line 2 alone over all waves and DCW %+.1f dB\n", worst);
    CHECK(fabs(worst) < 3.5, "ring modulation level off by %.1f dB", worst);
}

/* -------------------------------------------------------- 3. CPU timing */
static void test_cpu(void)
{
    static cz_engine_t e;
    float l[BLOCK], r[BLOCK];
    const float eras[4] = {0.0f, 0.1f, 0.5f, 1.0f};
    for (int ei = 0; ei < 4; ei++) {
        cz_init(&e, FS); e.line_mode = CZ_LINE_1_2; e.mod = CZ_MOD_RING; cz_set_era(&e, eras[ei]);
        for (int v = 0; v < 4; v++) cz_voice_on(&e, v, 48.0f + 4.0f * v);
        unsigned frames = (unsigned)FS * 20u;
        clock_t t0 = clock();
        for (unsigned f = 0; f < frames; f += BLOCK) cz_render(&e, l, r, BLOCK, 0.4f, 0.6f);
        double sec = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("cpu era %.1f: 20 s of 4 voices x 2 lines in %.3f s (%.2f %% of realtime on this machine)\n",
               eras[ei], sec, 100.0 * sec / 20.0);
    }
}

/* ------------------------------------------------------------ 4. demos */
/* A small patch emulation: external AD envelope on DCW, external VCA, one
 * gate (global phase reset) per chord. Chords arrive as 4 voices (USB-MIDI
 * in the real module). */
typedef struct { float t, dcw_amt, dcw_dec, amp_dec; } env_t;
static float env_ad(float t, float attack, float decay) { return t < attack ? t / attack : expf(-(t - attack) / decay); }

static void render_phrase(const char *path, cz_engine_t *e, float seconds, float bpm,
                          float era_from, float era_to, float dcw_base, float dcw_amt, float decay,
                          int wave_walk)
{
    static const float CHORDS[4][4] = {{48, 55, 59, 64}, {45, 52, 57, 60}, {41, 48, 52, 57}, {43, 50, 55, 59}};
    wav_t w; float l[BLOCK], r[BLOCK];
    if (wav_open(&w, path)) { printf("cannot write %s\n", path); failures++; return; }
    unsigned frames = (unsigned)(seconds * FS), step = (unsigned)(FS * 60.0f / bpm / 2.0f);
    unsigned since = step; int chord = -1, hit = 0;
    for (unsigned f = 0; f < frames; f += BLOCK) {
        float pos = (float)f / (float)frames;
        cz_set_era(e, era_from + (era_to - era_from) * pos);
        if (since >= step) {                         /* gate: new chord every 2 steps */
            since = 0; hit++;
            if (hit % 2 == 1) {
                chord = (chord + 1) % 4;
                for (int v = 0; v < 4; v++) cz_voice_on(e, v, CHORDS[chord][v]);
                if (wave_walk == 1) e->line[0].pos = (float)((hit / 2) % 8);   /* panel 1..8 */
            }
            cz_gate_reset(e);
        }
        float t = (float)since / FS;
        float env = env_ad(t, 0.004f, decay);
        float dcw = dcw_base + dcw_amt * env;
        dcw = dcw > 1.0f ? 1.0f : dcw;
        if (wave_walk == 2) e->line[0].pos = CZ_MORPH_MAX * pos;   /* continuous sweep */
        cz_render(e, l, r, BLOCK, dcw, dcw);
        float vca = env_ad(t, 0.002f, decay * 1.6f);   /* external VCA */
        for (unsigned i = 0; i < BLOCK; i++) { l[i] *= vca; r[i] *= vca; }
        wav_write(&w, l, r, BLOCK);
        since += BLOCK;
    }
    wav_close(&w);
    printf("wrote %s\n", path);
}

static void demos(const char *dir)
{
    static cz_engine_t e;
    char path[512];

    /* ERA sweep 0 -> 1: saw/reso pair, 1+1' detuned */
    cz_init(&e, FS);
    e.line[0] = (cz_line_t){0.0f, 6.0f, 1};
    e.line_mode = CZ_LINE_1_1; e.detune = cz_detune(1, 0, 0, 12); e.spread = 0.7f;
    snprintf(path, sizeof path, "%s/demo_era_sweep.wav", dir);
    render_phrase(path, &e, 24.0f, 110.0f, 0.0f, 1.0f, 0.05f, 0.7f, 0.35f, 0);

    /* A/B: same phrase clean and at ERA 0.6 */
    for (int k = 0; k < 2; k++) {
        cz_init(&e, FS);
        e.line[0] = (cz_line_t){1.0f, 1.0f, 0};     /* square */
        e.line[1] = (cz_line_t){7.0f, 7.0f, 0};     /* reso, trapezoid window */
        e.line_mode = CZ_LINE_1_2; e.detune = cz_detune(1, 1, 0, 5); e.spread = 0.6f;
        if (k) e.vint.drift = 0.5f;
        snprintf(path, sizeof path, "%s/demo_ab_%s.wav", dir, k ? "era060" : "clean");
        render_phrase(path, &e, 9.0f, 100.0f, k ? 0.6f : 0.0f, k ? 0.6f : 0.0f, 0.1f, 0.6f, 0.4f, 0);
    }

    /* panel waves 1..8, one per chord, ERA 0.5 */
    cz_init(&e, FS); e.line_mode = CZ_LINE_1; e.spread = 0.4f;
    snprintf(path, sizeof path, "%s/demo_waves_1to8.wav", dir);
    render_phrase(path, &e, 17.5f, 110.0f, 0.5f, 0.5f, 0.1f, 0.75f, 0.3f, 1);

    /* continuous wave morph 0 -> 10 under steady chords, ERA 0.3 */
    cz_init(&e, FS); e.line_mode = CZ_LINE_1_1; e.detune = cz_detune(1, 0, 0, 8); e.spread = 0.6f;
    snprintf(path, sizeof path, "%s/demo_morph.wav", dir);
    render_phrase(path, &e, 20.0f, 60.0f, 0.3f, 0.3f, 0.35f, 0.35f, 2.0f, 2);

    /* ring and noise modulation, 1+2', ERA 0.4 */
    cz_init(&e, FS);
    e.line[0] = (cz_line_t){3.0f, 3.0f, 0};     /* double sine */
    e.line[1] = (cz_line_t){4.0f, 4.0f, 0};     /* saw-pulse */
    e.line_mode = CZ_LINE_1_2; e.detune = cz_detune(1, 1, 7, 0); e.mod = CZ_MOD_RING; e.line_amp[0] = 0.3f; e.line_amp[1] = 0.7f;
    snprintf(path, sizeof path, "%s/demo_ring.wav", dir);
    render_phrase(path, &e, 9.0f, 100.0f, 0.4f, 0.4f, 0.0f, 0.8f, 0.3f, 0);
    e.mod = CZ_MOD_NOISE; e.line[1] = (cz_line_t){0.0f, 0.0f, 0};
    snprintf(path, sizeof path, "%s/demo_noise.wav", dir);
    render_phrase(path, &e, 9.0f, 100.0f, 0.4f, 0.4f, 0.1f, 0.6f, 0.25f, 0);
}

int main(int argc, char **argv)
{
    test_chip_vs_clean();
    test_render_sanity();
    test_morph_continuity();
    test_lines();
    test_morph_quality();
    test_ring_level();
    test_cpu();
    if (argc > 1) demos(argv[1]);
    printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
