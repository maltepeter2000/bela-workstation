/* SPDX-License-Identifier: BSD-3-Clause
 * pdoly8_test : desktop test of pdoly8_core, and demo WAVs
 *   ./pdoly8_test           run the checks
 *   ./pdoly8_test demos     checks, then demo WAVs into demos/
 */
#include "pdoly8_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS 44100.0f
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* --------------------------------------------------------------- audio */
typedef struct { float *l, *r; size_t n, cap; } buf_t;

static void buf_render(p8_engine_t *e, buf_t *b, float sec)
{
    size_t n = (size_t)(sec * FS);
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2;
        b->l = realloc(b->l, b->cap * sizeof(float)); b->r = realloc(b->r, b->cap * sizeof(float));
    }
    for (size_t i = 0; i < n; i += 64) {
        unsigned m = (unsigned)(n - i < 64 ? n - i : 64);
        p8_render(e, b->l + b->n + i, b->r + b->n + i, m);
    }
    b->n += n;
}

static void buf_free(buf_t *b) { free(b->l); free(b->r); memset(b, 0, sizeof *b); }

static void put16(FILE *f, unsigned v) { fputc((int)(v & 255), f); fputc((int)(v >> 8 & 255), f); }
static void put32(FILE *f, unsigned v) { put16(f, v & 0xffff); put16(f, v >> 16); }

static void wav_write(const char *path, const buf_t *b)
{
    FILE *f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); return; }
    unsigned bytes = (unsigned)b->n * 4;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, (unsigned)FS); put32(f, (unsigned)FS * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, bytes);
    for (size_t i = 0; i < b->n; i++) {
        float s[2] = {b->l[i], b->r[i]};
        for (int c = 0; c < 2; c++) {
            float x = s[c] > 1.0f ? 1.0f : s[c] < -1.0f ? -1.0f : s[c];
            int v = (int)lrintf(x * 32767.0f);
            put16(f, (unsigned)(v & 0xffff));
        }
    }
    fclose(f);
    printf("wrote %s (%.1f s)\n", path, (double)b->n / (double)FS);
}

static float peak(const buf_t *b, size_t from)
{
    float p = 0.0f;
    for (size_t i = from; i < b->n; i++) { float a = fabsf(b->l[i]) > fabsf(b->r[i]) ? fabsf(b->l[i]) : fabsf(b->r[i]); if (a > p) p = a; }
    return p;
}

/* frequency from rising zero crossings of the left channel */
static float freq(const buf_t *b, size_t from)
{
    int first = -1, last = -1, count = 0;
    for (size_t i = from + 1; i < b->n; i++)
        if (b->l[i - 1] < 0.0f && b->l[i] >= 0.0f) {
            float fr = (float)(i - 1) + b->l[i - 1] / (b->l[i - 1] - b->l[i]);
            if (first < 0) first = (int)fr, count = 0; else count++;
            last = (int)fr;
            (void)fr;
        }
    return count > 0 ? (float)count * FS / (float)(last - first) : 0.0f;
}

static void setup(p8_engine_t *e, int mode, float wav1, float har1, float wav2, float har2, float lev1, float lev2)
{
    e->mode = mode;
    e->wav[0] = wav1 / 127.0f; e->har[0] = har1; e->wav[1] = wav2 / 127.0f; e->har[1] = har2;
    e->lev[0] = lev1 / 127.0f; e->lev[1] = lev2 / 127.0f;
    p8_update(e); p8_retune(e); p8_settle(e);
}

/* -------------------------------------------------------------- checks */
static void test_rom(void)
{
    /* the reference point of the chip: code 0x21 gives 440 Hz on 8' at 2119040 Hz */
    float f8 = 2.0f * p8_code_hz16(0x21, 2119040.0f);
    CHECK(fabsf(f8 - 440.0f) < 0.01f, "code 0x21 8' = %.3f Hz", (double)f8);
    /* scale error of the ROM against equal temperament, all codes */
    float worst = 0.0f;
    for (int c = 0; c < 85; c++) {
        float f = 2.0f * p8_code_hz16(c, 2119040.0f), et = 440.0f * exp2f((float)(c - 0x21) / 12.0f);
        float cents = 1200.0f * log2f(f / et);
        if (fabsf(cents) > fabsf(worst)) worst = cents;
    }
    printf("ROM scale: worst deviation from equal temperament %.2f cents\n", (double)worst);
    CHECK(fabsf(worst) < 3.0f, "ROM scale off by %.2f cents", (double)worst);
}

static void test_knots(void)
{
    float g[4];
    for (int i = 0; i < P8_KNOTS; i++) {
        float x = (float)i * 127.0f / 14.0f;
        for (float dx = -1.0f; dx <= 1.0f; dx += 0.5f) {
            float h = x + dx;
            if (h < 0.0f || h > 127.0f) continue;
            p8_foot_gains(h, 1.0f, g);
            for (int f = 0; f < 4; f++)
                CHECK(g[f] == (float)((P8_KNOT[i] >> (3 - f)) & 1), "knot %d at %.1f foot %d = %g", i, (double)h, f, (double)g[f]);
        }
    }
    /* exactly one foot changes between neighbours, no combination twice, none empty */
    int seen[16] = {0};
    for (int i = 0; i < P8_KNOTS; i++) {
        CHECK(P8_KNOT[i] != 0 && !seen[P8_KNOT[i]], "knot %d repeats or is empty", i);
        seen[P8_KNOT[i]] = 1;
        if (i) { unsigned d = P8_KNOT[i] ^ P8_KNOT[i - 1]; CHECK(d && !(d & (d - 1)), "knots %d/%d differ in more than one foot", i - 1, i); }
    }
    /* continuity: no jump anywhere on the path */
    float prev[4], worst = 0.0f;
    p8_foot_gains(0.0f, 1.0f, prev);
    for (float h = 0.01f; h <= 127.0f; h += 0.01f) {
        p8_foot_gains(h, 1.0f, g);
        for (int f = 0; f < 4; f++) { float d = fabsf(g[f] - prev[f]); if (d > worst) worst = d; prev[f] = g[f]; }
    }
    printf("foot path: 15 knots exact (+-1 step), largest change per 0.01 step %.4f\n", (double)worst);
    CHECK(worst < 0.01f, "foot path jumps by %g", (double)worst);
}

static void test_pitch(void)
{
    p8_engine_t e; buf_t b = {0};
    p8_init(&e, FS);
    setup(&e, P8_WHOLE, 0, 91, 0, 64, 127, 0);              /* square, 8' alone */
    p8_voice_on(&e, 0, 69.0f);
    buf_render(&e, &b, 1.0f);
    float f = freq(&b, 4410);
    printf("A4, 8' alone: %.2f Hz\n", (double)f);
    CHECK(fabsf(f - 440.0f) < 0.3f, "A4 8' at %.2f Hz", (double)f);
    buf_free(&b);

    /* detune 3 (-20 cents) and interval 7 on group 2 */
    p8_init(&e, FS);
    setup(&e, P8_DOUBLE, 0, 91, 0, 91, 0, 127);             /* only DCO2 audible */
    e.interval = 7; e.detune_cents = 20.0f; p8_retune(&e);
    p8_voice_on(&e, 0, 69.0f);
    buf_render(&e, &b, 1.0f);
    f = freq(&b, 4410);
    float expect = 2.0f * p8_code_hz16(0x21 + 7, 2119040.0f) * exp2f(-20.0f / 1200.0f);
    printf("DCO2 +7, detune -20 ct: %.2f Hz (expected %.2f)\n", (double)f, (double)expect);
    CHECK(fabsf(f - expect) < 0.3f, "DCO2 at %.2f, expected %.2f", (double)f, (double)expect);
    buf_free(&b);
}

static void test_modes_levels(void)
{
    p8_engine_t e; buf_t b = {0};
    /* WHOLE: group 2 stays silent even with lev2 up */
    p8_init(&e, FS);
    setup(&e, P8_WHOLE, 127, 64, 127, 64, 0, 127);
    p8_voice_on(&e, 0, 60.0f);
    buf_render(&e, &b, 0.5f);
    CHECK(peak(&b, 2205) < 1e-4f, "WHOLE: group 2 audible (%g)", (double)peak(&b, 2205));
    buf_free(&b);

    /* worst case: 4 voices, square, all feet, both DCOs; lev scaled to sum 1 */
    float worst = 0.0f;
    int chords[3][4] = {{48, 55, 60, 64}, {36, 48, 60, 72}, {60, 60, 60, 60}};
    for (int c = 0; c < 3; c++) {
        p8_init(&e, FS);
        setup(&e, P8_DOUBLE, 0, 64, 0, 64, 64, 63);
        for (int k = 0; k < 4; k++) p8_voice_on(&e, k, (float)chords[c][k]);
        buf_render(&e, &b, 2.0f);
        float p = peak(&b, 4410);
        if (p > worst) worst = p;
        buf_free(&b);
    }
    printf("worst-case peak, 4 voices square all feet, level 0.5: %.3f\n", (double)worst);
    CHECK(worst <= 1.0f, "clips: %.3f", (double)worst);

    /* single saw note all feet, as a level reference */
    p8_init(&e, FS);
    setup(&e, P8_WHOLE, 127, 64, 0, 64, 127, 0);
    p8_voice_on(&e, 0, 57.0f);
    buf_render(&e, &b, 1.0f);
    printf("one saw note, all feet: peak %.3f\n", (double)peak(&b, 4410));
    buf_free(&b);
}

/* ------------------------------------------------------------- demos */
static void chord(p8_engine_t *e, const int *notes, int n)
{
    for (int k = 0; k < 4; k++) { if (k < n) p8_voice_on(e, k, (float)notes[k]); else p8_voice_off(e, k); }
}
static void all_off(p8_engine_t *e) { for (int k = 0; k < 4; k++) p8_voice_off(e, k); }

static const int PROG[4][4] = {{48, 55, 59, 64}, {45, 52, 60, 67}, {41, 52, 57, 64}, {43, 50, 59, 65}};

static void demo_single(void)
{
    p8_engine_t e; buf_t b = {0};
    p8_init(&e, FS);
    /* 1: saw, all feet (the classic Poly-800 string/brass base) */
    setup(&e, P8_WHOLE, 127, 64, 0, 64, 127, 0);
    for (int i = 0; i < 4; i++) { chord(&e, PROG[i], 4); buf_render(&e, &b, 1.9f); all_off(&e); buf_render(&e, &b, 0.1f); }
    buf_render(&e, &b, 0.5f);
    /* 2: square 16' + 4' (organ-like) */
    setup(&e, P8_WHOLE, 0, 27, 0, 64, 127, 0);
    for (int i = 0; i < 4; i++) { chord(&e, PROG[i], 4); buf_render(&e, &b, 1.9f); all_off(&e); buf_render(&e, &b, 0.1f); }
    buf_render(&e, &b, 0.5f);
    /* 3: saw 8' + 4' + 2', arpeggio */
    setup(&e, P8_WHOLE, 127, 73, 0, 64, 127, 0);
    static const int arp[16] = {60, 64, 67, 71, 72, 71, 67, 64, 57, 60, 64, 67, 69, 67, 64, 60};
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 16; i++) { p8_voice_on(&e, i & 3, (float)arp[i]); buf_render(&e, &b, 0.14f); p8_voice_off(&e, i & 3); buf_render(&e, &b, 0.03f); }
    buf_render(&e, &b, 0.8f);
    wav_write("demos/pdoly8_single.wav", &b);
    buf_free(&b);
}

static void demo_double(void)
{
    p8_engine_t e; buf_t b = {0};
    p8_init(&e, FS);
    /* 1: DCO1 saw 16'+8', DCO2 saw 16'+8' detune 3 (-20 ct): the fat unison */
    setup(&e, P8_DOUBLE, 127, 9, 127, 9, 64, 63);
    e.detune_cents = 20.0f; p8_retune(&e);
    for (int i = 0; i < 4; i++) { chord(&e, PROG[i], 4); buf_render(&e, &b, 1.9f); all_off(&e); buf_render(&e, &b, 0.1f); }
    buf_render(&e, &b, 0.5f);
    /* 2: DCO1 square 8', DCO2 saw 8'+4' a fifth up, detune 1 */
    setup(&e, P8_DOUBLE, 0, 91, 127, 100, 70, 57);
    e.interval = 7; e.detune_cents = 20.0f / 3.0f; p8_retune(&e);
    for (int i = 0; i < 4; i++) { chord(&e, PROG[i], 4); buf_render(&e, &b, 1.9f); all_off(&e); buf_render(&e, &b, 0.1f); }
    buf_render(&e, &b, 0.5f);
    /* 3: bass line, DCO1 square 16', DCO2 saw all feet one octave up, detune 2 */
    setup(&e, P8_DOUBLE, 0, 0, 127, 64, 70, 57);
    e.interval = 12; e.detune_cents = 40.0f / 3.0f; p8_retune(&e);
    static const int bass[16] = {36, 36, 48, 36, 39, 36, 43, 46, 36, 36, 48, 36, 34, 34, 46, 41};
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 16; i++) { p8_voice_on(&e, 0, (float)bass[i]); buf_render(&e, &b, 0.15f); p8_voice_off(&e, 0); buf_render(&e, &b, 0.04f); }
    buf_render(&e, &b, 0.8f);
    wav_write("demos/pdoly8_double.wav", &b);
    buf_free(&b);
}

static void demo_morph(void)
{
    p8_engine_t e; buf_t b = {0};
    static const int am7[4] = {45, 52, 55, 60};
    p8_init(&e, FS);
    /* 1: har sweep 0 -> 127 -> 0, saw (20 s), held chord */
    setup(&e, P8_WHOLE, 127, 0, 0, 64, 127, 0);
    chord(&e, am7, 4);
    const float T = 10.0f, dt = 0.005f;
    for (float t = 0.0f; t < 2.0f * T; t += dt) {
        float h = t < T ? t / T * 127.0f : (2.0f * T - t) / T * 127.0f;
        e.har[0] = h; p8_update(&e); buf_render(&e, &b, dt);
    }
    /* 2: wav sweep square -> saw -> square at all feet (8 s) */
    e.har[0] = 64.0f;
    for (float t = 0.0f; t < 8.0f; t += dt) {
        e.wav[0] = 0.5f - 0.5f * cosf(t / 8.0f * 2.0f * 3.14159265f); p8_update(&e); buf_render(&e, &b, dt);
    }
    /* 3: both at once, slow LFOs, DOUBLE with detune (14 s) */
    setup(&e, P8_DOUBLE, 64, 64, 64, 64, 64, 63);
    e.detune_cents = 20.0f; p8_retune(&e); chord(&e, am7, 4);
    for (float t = 0.0f; t < 14.0f; t += dt) {
        e.har[0] = 63.5f + 63.5f * sinf(t * 0.45f); e.har[1] = 63.5f - 63.5f * sinf(t * 0.31f);
        e.wav[0] = 0.5f + 0.5f * sinf(t * 0.8f);    e.wav[1] = 0.5f - 0.5f * sinf(t * 0.6f);
        p8_update(&e); buf_render(&e, &b, dt);
    }
    all_off(&e); buf_render(&e, &b, 0.5f);
    wav_write("demos/pdoly8_morph.wav", &b);
    buf_free(&b);

    /* the 15 original foot combinations, 1 s each, on a single note: the
     * reference points that the morph passes through exactly */
    p8_init(&e, FS);
    for (int i = 0; i < P8_KNOTS; i++) {
        setup(&e, P8_WHOLE, 127, (float)i * 127.0f / 14.0f, 0, 64, 127, 0);
        p8_voice_on(&e, 0, 45.0f); buf_render(&e, &b, 0.9f); p8_voice_off(&e, 0); buf_render(&e, &b, 0.1f);
    }
    wav_write("demos/pdoly8_knots.wav", &b);
    buf_free(&b);

    /* aliasing check: high square note, 2' alone, mono (for analysis) */
    p8_init(&e, FS);
    setup(&e, P8_WHOLE, 0, 127, 0, 64, 127, 0);
    e.spread = 0.0f; p8_update(&e);
    p8_voice_on(&e, 0, 84.0f); buf_render(&e, &b, 2.0f);
    wav_write("demos/alias_c6_2ft.wav", &b);
    buf_free(&b);
}

int main(int argc, char **argv)
{
    test_rom();
    test_knots();
    test_pitch();
    test_modes_levels();
    if (argc > 1 && !strcmp(argv[1], "demos")) { demo_single(); demo_double(); demo_morph(); }
    printf(fails ? "%d check(s) failed\n" : "all checks passed\n", fails);
    return fails != 0;
}
