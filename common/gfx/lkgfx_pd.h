/* SPDX-License-Identifier: BSD-3-Clause
 * lkgfx_pd: the Pd side of the Launchkey pictures (lkgfx.h), shared by all instruments.
 *
 * The instrument asks for a picture with lkg_pd_show(p, kind, title) when a setting that has one
 * was changed; at most every 50 ms the draw function is called (lkg_clear with the title is
 * done), the changed pieces go out as "lkgfx <bytes>" messages on the outlet, two a millisecond
 * (a whole picture at once does not reach the Launchkey from Pd on macOS). [lkdisplay] passes
 * them to the MIDI out. "lkgfx 0" in the instrument (lkg_pd_on) switches the pictures off, the
 * text line comes back. */
#ifndef LKGFX_PD_H_INCLUDED
#define LKGFX_PD_H_INCLUDED
#include "m_pd.h"
#include "lkgfx.h"
#include <stdio.h>

#define LKG_PD_PER_MS 2
#define LKG_PD_QMAX   (LKG_PAGES * 4 + 2)

typedef void (*lkg_draw_fn)(void *owner, lkg_t *g, int kind);

typedef struct {
    lkg_t g;
    t_outlet *out;
    void *owner;
    lkg_draw_fn draw;
    t_clock *clk, *qclk;
    double last;                    /* logical time of the last picture sent */
    int on, kind;                   /* kind: the instrument's picture, 0 none */
    char title[96];
    unsigned char q[LKG_PD_QMAX][LKG_MSG_MAX];
    int qlen[LKG_PD_QMAX], qn, qi;
} lkg_pd_t;

static inline void lkg_pd_qsend(void *ctx, const unsigned char *m, int len)
{
    lkg_pd_t *p = (lkg_pd_t *)ctx;
    if (len > LKG_MSG_MAX || p->qn >= LKG_PD_QMAX) return;
    memcpy(p->q[p->qn], m, (size_t)len);
    p->qlen[p->qn++] = len;
}

static inline void lkg_pd_qtick(lkg_pd_t *p)
{
    for (int k = 0; k < LKG_PD_PER_MS && p->qi < p->qn; k++, p->qi++) {
        t_atom a[LKG_MSG_MAX];
        const int len = p->qlen[p->qi];
        for (int i = 0; i < len; i++) SETFLOAT(a + i, p->q[p->qi][i]);
        outlet_anything(p->out, gensym("lkgfx"), len, a);
    }
    if (p->qi < p->qn) clock_delay(p->qclk, 1.0);
    else p->qn = p->qi = 0;
}

static inline void lkg_pd_tick(lkg_pd_t *p)
{
    if (p->qn) { clock_delay(p->clk, 10.0); return; }   /* the last picture is still going out */
    if (!p->kind) return;
    lkg_clear(&p->g, p->title);
    p->draw(p->owner, &p->g, p->kind);
    /* everything again after a pause (the Launchkey may have been switched off) */
    const int all = clock_gettimesince(p->last) > 3000.0;
    lkg_flush(&p->g, all, lkg_pd_qsend, p);
    p->last = clock_getlogicaltime();
    lkg_pd_qtick(p);
}

static inline void lkg_pd_init(lkg_pd_t *p, t_outlet *out, lkg_draw_fn draw, void *owner)
{
    memset(p, 0, sizeof *p);
    p->out = out;
    p->draw = draw;
    p->owner = owner;
    p->clk = clock_new(p, (t_method)lkg_pd_tick);
    p->qclk = clock_new(p, (t_method)lkg_pd_qtick);
    p->on = 1;
    p->last = -1e9;
}

static inline void lkg_pd_free(lkg_pd_t *p)
{
    if (p->clk) clock_free(p->clk);
    if (p->qclk) clock_free(p->qclk);
    p->clk = p->qclk = 0;
}

static inline void lkg_pd_on(lkg_pd_t *p, int on) { p->on = on != 0; }

/* A picture of the kind (> 0) with the title "label value" (a trailing ".0" dropped).
 * Returns 0 when the pictures are off or kind is 0: then the text line is the instrument's job. */
static inline int lkg_pd_show(lkg_pd_t *p, int kind, const char *label, const char *value)
{
    if (!p->on || kind <= 0) return 0;
    char v[64];
    snprintf(v, sizeof v, "%s", value ? value : "");
    size_t n = strlen(v);
    if (n > 2 && !strcmp(v + n - 2, ".0")) v[n - 2] = 0;
    snprintf(p->title, sizeof p->title, "%s%s%s", label ? label : "", *v ? " " : "", v);
    p->kind = kind;
    const double since = clock_gettimesince(p->last);
    clock_delay(p->clk, since >= 50.0 ? 0.0 : 50.0 - since);
    return 1;
}
#endif
