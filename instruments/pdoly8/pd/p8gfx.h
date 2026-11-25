/* SPDX-License-Identifier: BSD-3-Clause
 * p8gfx: a 128 x 48 picture (SSD1306 pages, LSB top) for the Launchkey display, see p8gfx.c. */
#ifndef P8GFX_H_INCLUDED
#define P8GFX_H_INCLUDED
#include <math.h>

#define P8GFX_W     128
#define P8GFX_PAGES 6
#define P8GFX_H     (P8GFX_PAGES * 8)

typedef struct {
    unsigned char fb[P8GFX_PAGES][P8GFX_W];     /* the picture */
    unsigned char sent[P8GFX_PAGES][P8GFX_W];   /* what the Launchkey has */
} p8gfx_t;

typedef void (*p8gfx_send_fn)(void *ctx, const unsigned char *msg, int len);

void p8gfx_clear(p8gfx_t *g, const char *title);   /* empty, the text line at the top (5 x 7, capitals) */
void p8gfx_env(p8gfx_t *g, const float deg[6]);    /* a d bp sl su r, 0..31 */
void p8gfx_wave(p8gfx_t *g, const float w[4]);     /* foot weights 16' 8' 4' 2' */
/* the changed 32-column pieces (all: every piece) as SysEx, then "show"; returns the pieces sent */
int  p8gfx_flush(p8gfx_t *g, int all, p8gfx_send_fn send, void *ctx);
#endif
