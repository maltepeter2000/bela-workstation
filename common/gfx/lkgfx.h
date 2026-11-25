/* SPDX-License-Identifier: BSD-3-Clause
 * lkgfx: pictures for the Launchkey Mini MK4 display, shared by all instruments (C and C++,
 * header only, include it by its relative path).
 *
 * Custom firmware maltepeter2000/launchkey_firmware, branch display-gfx (cfw/lk_gfx.h): a 128 x 48
 * bitmap in SSD1306 pages (8 rows a byte, LSB on top) below the popup's top line. SysEx:
 *   F0 7D 4C 4B 02 page x n <n bytes, 7-bit packed> F7   bitmap data
 *   F0 7D 4C 4B 03 F7                                    show
 * lkg_flush sends the 32-column pieces that changed since the last flush, then "show".
 *
 * Layout used by every picture: a text line at the top (rows 0..6, 5 x 7 capitals), the drawing
 * from LKG_TOP to LKG_BOT. */
#ifndef LKGFX_H_INCLUDED
#define LKGFX_H_INCLUDED
#include <math.h>
#include <string.h>

#define LKG_W     128
#define LKG_PAGES 6
#define LKG_H     (LKG_PAGES * 8)
#define LKG_TOP   11.0f
#define LKG_BOT   ((float)LKG_H - 2.0f)

typedef struct {
    unsigned char fb[LKG_PAGES][LKG_W];     /* the picture */
    unsigned char sent[LKG_PAGES][LKG_W];   /* what the Launchkey has */
} lkg_t;

typedef void (*lkg_send_fn)(void *ctx, const unsigned char *msg, int len);

/* ---- drawing ------------------------------------------------------------------------------ */

static inline void lkg_px(lkg_t *g, int x, int y)
{
    if (x >= 0 && x < LKG_W && y >= 0 && y < LKG_H) g->fb[y >> 3][x] |= (unsigned char)(1u << (y & 7));
}

static inline void lkg_line(lkg_t *g, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy)) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        lkg_px(g, (int)lroundf(x0 + dx * t), (int)lroundf(y0 + dy * t));
    }
}

/* dotted lines, every step-th pixel */
static inline void lkg_hdots(lkg_t *g, int x0, int x1, int y, int step)
{
    for (int x = x0; x <= x1; x += step) lkg_px(g, x, y);
}
static inline void lkg_vdots(lkg_t *g, int x, int y0, int y1, int step)
{
    for (int y = y0; y <= y1; y += step) lkg_px(g, x, y);
}

static inline void lkg_rect(lkg_t *g, int x, int y, int w, int h)
{
    lkg_line(g, (float)x, (float)y, (float)(x + w - 1), (float)y);
    lkg_line(g, (float)x, (float)(y + h - 1), (float)(x + w - 1), (float)(y + h - 1));
    lkg_line(g, (float)x, (float)y, (float)x, (float)(y + h - 1));
    lkg_line(g, (float)(x + w - 1), (float)y, (float)(x + w - 1), (float)(y + h - 1));
}

static inline void lkg_fill(lkg_t *g, int x, int y, int w, int h)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) lkg_px(g, x + i, y + j);
}

/* 5 x 7 capitals, digits and a few signs; 6 px a character */
static inline const unsigned char *lkg_glyph(char ch)
{
    static const struct { char c; unsigned char r[7]; } FONT[] = {
        {'0', {0xe, 0x11, 0x13, 0x15, 0x19, 0x11, 0xe}},   {'1', {0x4, 0xc, 0x4, 0x4, 0x4, 0x4, 0xe}},
        {'2', {0xe, 0x11, 0x1, 0x2, 0x4, 0x8, 0x1f}},      {'3', {0x1e, 0x1, 0x1, 0xe, 0x1, 0x1, 0x1e}},
        {'4', {0x2, 0x6, 0xa, 0x12, 0x1f, 0x2, 0x2}},      {'5', {0x1f, 0x10, 0x1e, 0x1, 0x1, 0x11, 0xe}},
        {'6', {0x6, 0x8, 0x10, 0x1e, 0x11, 0x11, 0xe}},    {'7', {0x1f, 0x1, 0x2, 0x4, 0x8, 0x8, 0x8}},
        {'8', {0xe, 0x11, 0x11, 0xe, 0x11, 0x11, 0xe}},    {'9', {0xe, 0x11, 0x11, 0xf, 0x1, 0x2, 0xc}},
        {'A', {0xe, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},  {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
        {'C', {0xe, 0x11, 0x10, 0x10, 0x10, 0x11, 0xe}},   {'D', {0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c}},
        {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}}, {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
        {'G', {0xe, 0x11, 0x10, 0x17, 0x11, 0x11, 0xf}},   {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
        {'I', {0xe, 0x4, 0x4, 0x4, 0x4, 0x4, 0xe}},        {'J', {0x7, 0x2, 0x2, 0x2, 0x2, 0x12, 0xc}},
        {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}}, {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
        {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}}, {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
        {'O', {0xe, 0x11, 0x11, 0x11, 0x11, 0x11, 0xe}},   {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
        {'Q', {0xe, 0x11, 0x11, 0x11, 0x15, 0x12, 0xd}},   {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
        {'S', {0xf, 0x10, 0x10, 0xe, 0x1, 0x1, 0x1e}},     {'T', {0x1f, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4}},
        {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0xe}},  {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0xa, 0x4}},
        {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0xa}},  {'X', {0x11, 0x11, 0xa, 0x4, 0xa, 0x11, 0x11}},
        {'Y', {0x11, 0x11, 0x11, 0xa, 0x4, 0x4, 0x4}},     {'Z', {0x1f, 0x1, 0x2, 0x4, 0x8, 0x10, 0x1f}},
        {' ', {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},        {'-', {0x0, 0x0, 0x0, 0x1f, 0x0, 0x0, 0x0}},
        {'+', {0x0, 0x4, 0x4, 0x1f, 0x4, 0x4, 0x0}},       {'.', {0x0, 0x0, 0x0, 0x0, 0x0, 0xc, 0xc}},
        {':', {0x0, 0xc, 0xc, 0x0, 0xc, 0xc, 0x0}},        {'/', {0x1, 0x1, 0x2, 0x4, 0x8, 0x10, 0x10}},
        {'%', {0x18, 0x19, 0x2, 0x4, 0x8, 0x13, 0x3}},     {'>', {0x8, 0x4, 0x2, 0x1, 0x2, 0x4, 0x8}},
        {'<', {0x2, 0x4, 0x8, 0x10, 0x8, 0x4, 0x2}},       {'\'', {0x4, 0x4, 0x8, 0x0, 0x0, 0x0, 0x0}},
        {'=', {0x0, 0x0, 0x1f, 0x0, 0x1f, 0x0, 0x0}},      {'(', {0x2, 0x4, 0x8, 0x8, 0x8, 0x4, 0x2}},
        {')', {0x8, 0x4, 0x2, 0x2, 0x2, 0x4, 0x8}},        {'#', {0xa, 0xa, 0x1f, 0xa, 0x1f, 0xa, 0xa}},
    };
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    for (unsigned k = 0; k < sizeof FONT / sizeof FONT[0]; k++)
        if (FONT[k].c == ch) return FONT[k].r;
    return 0;
}

/* returns the x after the text */
static inline int lkg_text(lkg_t *g, int x, int y, const char *s)
{
    for (; *s && x < LKG_W; s++, x += 6) {
        const unsigned char *r = lkg_glyph(*s);
        if (!r) continue;
        for (int j = 0; j < 7; j++)
            for (int c = 0; c < 5; c++)
                if (r[j] >> (4 - c) & 1) lkg_px(g, x + c, y + j);
    }
    return x;
}

/* empty, the title on the text line */
static inline void lkg_clear(lkg_t *g, const char *title)
{
    memset(g->fb, 0, sizeof g->fb);
    if (title) lkg_text(g, 0, 0, title);
}

/* level 0..1 -> y in the drawing area */
static inline float lkg_ly(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return LKG_BOT - v * (LKG_BOT - LKG_TOP);
}

/* An envelope of n segments from level lvl0: segment k goes to lvl[k] (0..1) over the width
 * w[k] (any unit). hold >= 0: a plateau of the width holdw after segment hold (the sustain).
 * Squeezed to the width when it does not fit. xs (n + 1 entries, may be 0) gets the x of the
 * start and of every segment's end (the plateau's end for the segment after it starts there).
 * The zero line is dotted. */
static inline void lkg_env(lkg_t *g, int n, float lvl0, const float *lvl, const float *w,
                           int hold, float holdw, float *xs)
{
    float tot = hold >= 0 ? holdw : 0.0f;
    for (int k = 0; k < n; k++) tot += w[k];
    const float room = (float)(LKG_W - 1), sc = tot > room ? room / tot : 1.0f;
    float x = 0.0f, y = lkg_ly(lvl0);
    if (xs) xs[0] = 0.0f;
    for (int k = 0; k < n; k++) {
        const float x1 = x + w[k] * sc, y1 = lkg_ly(lvl[k]);
        lkg_line(g, x, y, x1, y1);
        x = x1; y = y1;
        if (k == hold) {
            const float xh = x + holdw * sc;
            lkg_line(g, x, y, xh, y);
            x = xh;
        }
        if (xs) xs[k + 1] = x;
    }
    lkg_hdots(g, 0, LKG_W - 1, (int)LKG_BOT + 1, 4);
}

/* n values (lo..hi, bottom to top) as a line across the width */
static inline void lkg_plot(lkg_t *g, const float *v, int n, float lo, float hi)
{
    if (n < 2) return;
    const float span = hi - lo != 0.0f ? hi - lo : 1.0f;
    float px = 0.0f, py = lkg_ly((v[0] - lo) / span);
    for (int i = 1; i < n; i++) {
        const float x = (float)i * (float)(LKG_W - 1) / (float)(n - 1), y = lkg_ly((v[i] - lo) / span);
        lkg_line(g, px, py, x, y);
        px = x; py = y;
    }
}

/* The same as dots (every step-th value), for a second trace behind the first */
static inline void lkg_plot_dots(lkg_t *g, const float *v, int n, float lo, float hi, int step)
{
    const float span = hi - lo != 0.0f ? hi - lo : 1.0f;
    for (int i = 0; i < n; i += step)
        lkg_px(g, (int)lroundf((float)i * (float)(LKG_W - 1) / (float)(n - 1)), (int)lroundf(lkg_ly((v[i] - lo) / span)));
}

/* Filter response over 20 Hz .. 20 kHz (log x), -36 .. +18 dB.
 * fc: cutoff 0..1 along the axis; res 0..1 (1 = the resonance peak at +18 dB);
 * type: 0 low pass 24 dB, 1 low pass 12 dB, 2 high pass 12 dB, 3 band pass 12 dB.
 * hp: an extra 6 dB high pass at 0..1 along the axis (0 = none, as the Juno's HPF). */
enum { LKG_LP24 = 0, LKG_LP12, LKG_HP12, LKG_BP12 };
static inline void lkg_filter(lkg_t *g, float fc, float res, int type, float hp)
{
    float v[LKG_W];
    const float q = 0.5f + res * res * 7.5f;               /* Q 0.5 .. 8 */
    for (int i = 0; i < LKG_W; i++) {
        const float t = (float)i / (float)(LKG_W - 1);
        const float wr = powf(1000.0f, t - fc);              /* f / fc, three decades across */
        const float w2 = wr * wr;
        const float den = sqrtf((1.0f - w2) * (1.0f - w2) + w2 / (q * q));
        float m;
        switch (type) {
        case LKG_LP24: {                                     /* 2 poles plain (Butterworth), 2 with the peak */
            const float plain = 1.0f / sqrtf(1.0f + w2 * w2);
            m = plain / den;
            break;
        }
        case LKG_HP12: m = w2 / den; break;
        case LKG_BP12: m = (wr / q) / den; break;
        default: m = 1.0f / den; break;
        }
        if (hp > 0.0f) {
            const float h = powf(1000.0f, t - hp);
            m *= h / sqrtf(1.0f + h * h);
        }
        float db = 20.0f * log10f(m > 1e-6f ? m : 1e-6f);
        if (db < -36.0f) db = -36.0f;
        if (db > 18.0f) db = 18.0f;
        v[i] = db;
    }
    lkg_plot(g, v, LKG_W, -36.0f, 18.0f);
    lkg_hdots(g, 0, LKG_W - 1, (int)lroundf(lkg_ly(36.0f / 54.0f)), 4);   /* 0 dB, dotted */
}

/* ---- SysEx -------------------------------------------------------------------------------- */

static inline void lkg_pack7(const unsigned char *in, int n, unsigned char *out, int *k)
{
    for (int i = 0; i < n; i += 7) {
        int m = n - i < 7 ? n - i : 7;
        unsigned char hi = 0;
        for (int b = 0; b < m; b++) hi |= (unsigned char)(((in[i + b] >> 7) & 1u) << b);
        out[(*k)++] = hi;
        for (int b = 0; b < m; b++) out[(*k)++] = in[i + b] & 0x7F;
    }
}

#define LKG_MSG_MAX 48   /* the longest message: a 32-column piece */

/* the changed 32-column pieces (all: every piece) as SysEx, then "show"; returns the pieces sent */
static inline int lkg_flush(lkg_t *g, int all, lkg_send_fn send, void *ctx)
{
    int sent = 0;
    for (int p = 0; p < LKG_PAGES; p++)
        for (int x = 0; x < LKG_W; x += 32) {
            if (!all && !memcmp(&g->fb[p][x], &g->sent[p][x], 32)) continue;
            unsigned char m[LKG_MSG_MAX];
            int k = 0;
            m[k++] = 0xF0; m[k++] = 0x7D; m[k++] = 0x4C; m[k++] = 0x4B; m[k++] = 0x02;
            m[k++] = (unsigned char)p; m[k++] = (unsigned char)x; m[k++] = 32;
            lkg_pack7(&g->fb[p][x], 32, m, &k);
            m[k++] = 0xF7;
            send(ctx, m, k);
            memcpy(&g->sent[p][x], &g->fb[p][x], 32);
            sent++;
        }
    static const unsigned char show[] = {0xF0, 0x7D, 0x4C, 0x4B, 0x03, 0xF7};
    send(ctx, show, (int)sizeof show);
    return sent;
}
#endif
