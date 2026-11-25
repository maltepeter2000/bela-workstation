/* SPDX-License-Identifier: BSD-3-Clause
 * p8gfx: pictures for the Launchkey display (custom firmware maltepeter2000/launchkey_firmware,
 * branch display-gfx, cfw/lk_gfx.h): a 128 x 48 bitmap with a text line and the DEG shape or the
 * timbre's wave, sent as SysEx in 32-column pieces (only the pieces that changed). */
#include "p8gfx.h"
#include <string.h>

typedef struct { char c; unsigned char r[7]; } glyph_t;
static const glyph_t FONT[] = {
    {'0', {0xe, 0x11, 0x13, 0x15, 0x19, 0x11, 0xe}},
    {'1', {0x4, 0xc, 0x4, 0x4, 0x4, 0x4, 0xe}},
    {'2', {0xe, 0x11, 0x1, 0x2, 0x4, 0x8, 0x1f}},
    {'3', {0x1e, 0x1, 0x1, 0xe, 0x1, 0x1, 0x1e}},
    {'4', {0x2, 0x6, 0xa, 0x12, 0x1f, 0x2, 0x2}},
    {'5', {0x1f, 0x10, 0x1e, 0x1, 0x1, 0x11, 0xe}},
    {'6', {0x6, 0x8, 0x10, 0x1e, 0x11, 0x11, 0xe}},
    {'7', {0x1f, 0x1, 0x2, 0x4, 0x8, 0x8, 0x8}},
    {'8', {0xe, 0x11, 0x11, 0xe, 0x11, 0x11, 0xe}},
    {'9', {0xe, 0x11, 0x11, 0xf, 0x1, 0x2, 0xc}},
    {'A', {0xe, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
    {'C', {0xe, 0x11, 0x10, 0x10, 0x10, 0x11, 0xe}},
    {'D', {0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c}},
    {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
    {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
    {'G', {0xe, 0x11, 0x10, 0x17, 0x11, 0x11, 0xf}},
    {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
    {'I', {0xe, 0x4, 0x4, 0x4, 0x4, 0x4, 0xe}},
    {'J', {0x7, 0x2, 0x2, 0x2, 0x2, 0x12, 0xc}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
    {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0xe, 0x11, 0x11, 0x11, 0x11, 0x11, 0xe}},
    {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
    {'Q', {0xe, 0x11, 0x11, 0x11, 0x15, 0x12, 0xd}},
    {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
    {'S', {0xf, 0x10, 0x10, 0xe, 0x1, 0x1, 0x1e}},
    {'T', {0x1f, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0xe}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0xa, 0x4}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0xa}},
    {'X', {0x11, 0x11, 0xa, 0x4, 0xa, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x11, 0xa, 0x4, 0x4, 0x4}},
    {'Z', {0x1f, 0x1, 0x2, 0x4, 0x8, 0x10, 0x1f}},
    {' ', {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}},
    {'-', {0x0, 0x0, 0x0, 0x1f, 0x0, 0x0, 0x0}},
    {'.', {0x0, 0x0, 0x0, 0x0, 0x0, 0xc, 0xc}},
    {':', {0x0, 0xc, 0xc, 0x0, 0xc, 0xc, 0x0}},
    {'/', {0x1, 0x1, 0x2, 0x4, 0x8, 0x10, 0x10}},
    {'%', {0x18, 0x19, 0x2, 0x4, 0x8, 0x13, 0x3}},
    {'>', {0x8, 0x4, 0x2, 0x1, 0x2, 0x4, 0x8}},
};

static void px(p8gfx_t *g, int x, int y)
{
    if (x >= 0 && x < P8GFX_W && y >= 0 && y < P8GFX_H) g->fb[y >> 3][x] |= (unsigned char)(1u << (y & 7));
}

static void line(p8gfx_t *g, float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0, dy = y1 - y0;
    int n = (int)(fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy)) + 1;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        px(g, (int)lroundf(x0 + dx * t), (int)lroundf(y0 + dy * t));
    }
}

static void text(p8gfx_t *g, int x, int y, const char *s)
{
    for (; *s && x < P8GFX_W; s++, x += 6) {
        char ch = *s;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        const glyph_t *f = 0;
        for (unsigned k = 0; k < sizeof FONT / sizeof FONT[0]; k++) if (FONT[k].c == ch) { f = &FONT[k]; break; }
        if (!f) continue;
        for (int r = 0; r < 7; r++)
            for (int col = 0; col < 5; col++)
                if (f->r[r] >> (4 - col) & 1) px(g, x + col, y + r);
    }
}

void p8gfx_clear(p8gfx_t *g, const char *title)
{
    memset(g->fb, 0, sizeof g->fb);
    text(g, 0, 0, title);
}

/* DEG: a, d (time to the break point), bp (level), sl (slope time), su (sustain level), r;
 * levels 0..31 from the bottom up */
void p8gfx_env(p8gfx_t *g, const float deg[6])
{
    const float top = 11.0f, bot = (float)P8GFX_H - 2.0f;
    #define LY(v) (bot - (v) / 31.0f * (bot - top))
    /* times as widths: 2 + 1.6 px a step, sustain 18 px; squeezed when it does not fit */
    float wa = 2.0f + deg[0] * 1.6f, wd = 2.0f + deg[1] * 1.6f, ws = 2.0f + deg[3] * 1.6f, wr = 2.0f + deg[5] * 1.6f;
    float k = (wa + wd + ws + wr + 18.0f) > (float)(P8GFX_W - 1) ? (float)(P8GFX_W - 1) / (wa + wd + ws + wr + 18.0f) : 1.0f;
    float x0 = 0.0f, xa = x0 + wa * k, xd = xa + wd * k, xs = xd + ws * k;
    float xh = xs + 18.0f * k, xr = xh + wr * k;
    line(g, x0, LY(0.0f), xa, LY(31.0f));
    line(g, xa, LY(31.0f), xd, LY(deg[2]));
    line(g, xd, LY(deg[2]), xs, LY(deg[4]));
    line(g, xs, LY(deg[4]), xh, LY(deg[4]));
    line(g, xh, LY(deg[4]), xr, LY(0.0f));
    for (int x = 0; x < P8GFX_W; x += 4) px(g, x, (int)bot + 1);      /* the zero line, dotted */
    #undef LY
}

/* the wave of the feet (16' 8' 4' 2' weights, each foot a square of the counter, 0 / 1), two
 * periods of the 16' across the width */
void p8gfx_wave(p8gfx_t *g, const float w[4])
{
    const float top = 11.0f, bot = (float)P8GFX_H - 2.0f;
    float sum = 0.0f;
    for (int f = 0; f < 4; f++) sum += w[f];
    if (sum <= 1e-6f) sum = 1.0f;
    float prev = -1.0f;
    for (int x = 0; x < P8GFX_W; x++) {
        float t = (float)x / 64.0f, v = 0.0f;              /* 64 px = one period of the 16' */
        for (int f = 0; f < 4; f++) {
            int bit = (int)floorf(t * (float)(1 << (f + 1))) & 1;
            v += bit ? w[f] : 0.0f;
        }
        float y = bot - v / sum * (bot - top);
        if (prev < 0.0f) px(g, x, (int)lroundf(y));
        else line(g, (float)(x - 1), prev, (float)x, y);
        prev = y;
    }
}

static void pack7(const unsigned char *in, int n, unsigned char *out, int *k)
{
    for (int i = 0; i < n; i += 7) {
        int m = n - i < 7 ? n - i : 7;
        unsigned char hi = 0;
        for (int b = 0; b < m; b++) hi |= (unsigned char)(((in[i + b] >> 7) & 1u) << b);
        out[(*k)++] = hi;
        for (int b = 0; b < m; b++) out[(*k)++] = in[i + b] & 0x7F;
    }
}

int p8gfx_flush(p8gfx_t *g, int all, p8gfx_send_fn send, void *ctx)
{
    int sent = 0;
    for (int p = 0; p < P8GFX_PAGES; p++)
        for (int x = 0; x < P8GFX_W; x += 32) {
            if (!all && !memcmp(&g->fb[p][x], &g->sent[p][x], 32)) continue;
            unsigned char m[8 + 37 + 1];
            int k = 0;
            m[k++] = 0xF0; m[k++] = 0x7D; m[k++] = 0x4C; m[k++] = 0x4B; m[k++] = 0x02;
            m[k++] = (unsigned char)p; m[k++] = (unsigned char)x; m[k++] = 32;
            pack7(&g->fb[p][x], 32, m, &k);
            m[k++] = 0xF7;
            send(ctx, m, k);
            memcpy(&g->sent[p][x], &g->fb[p][x], 32);
            sent++;
        }
    static const unsigned char show[] = {0xF0, 0x7D, 0x4C, 0x4B, 0x03, 0xF7};
    send(ctx, show, (int)sizeof show);
    return sent;
}
