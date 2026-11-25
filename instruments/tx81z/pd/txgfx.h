// SPDX-License-Identifier: BSD-3-Clause
// txgfx: the TX81Z pictures for the Launchkey display (common/gfx): the algorithm, an operator's
// envelope, an operator's wave. Used by tx81zn~ (and its host test tests/test_txgfx.cpp).
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "../../../common/gfx/lkgfx.h"
#include "txfm.h"

static const int OPBLOCK[4] = { txfm::OP1, txfm::OP2, txfm::OP3, txfm::OP4 };   // VCED / ACED block of OP1..OP4

// The 8 algorithms, signal from left to right, the carriers drop to the line at the bottom:
// per operator OP1..OP4 column and row (3 rows), then the connections (from, to; 0 = end).
struct t_algpic { int col[4], row[4]; int con[4][2]; };
static const t_algpic ALGPIC[8] = {
	{ { 3, 2, 1, 0 }, { 1, 1, 1, 1 }, { { 4, 3 }, { 3, 2 }, { 2, 1 }, { 0, 0 } } },
	{ { 2, 1, 0, 0 }, { 1, 1, 2, 0 }, { { 4, 2 }, { 3, 2 }, { 2, 1 }, { 0, 0 } } },
	{ { 2, 1, 0, 1 }, { 1, 2, 2, 0 }, { { 3, 2 }, { 2, 1 }, { 4, 1 }, { 0, 0 } } },
	{ { 2, 1, 1, 0 }, { 1, 2, 0, 0 }, { { 4, 3 }, { 3, 1 }, { 2, 1 }, { 0, 0 } } },
	{ { 3, 2, 1, 0 }, { 1, 1, 1, 1 }, { { 4, 3 }, { 2, 1 }, { 0, 0 }, { 0, 0 } } },
	{ { 2, 1, 0, 1 }, { 2, 2, 2, 0 }, { { 4, 3 }, { 4, 2 }, { 4, 1 }, { 0, 0 } } },
	{ { 3, 2, 1, 0 }, { 1, 1, 1, 1 }, { { 4, 3 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
	{ { 3, 2, 1, 0 }, { 1, 1, 1, 1 }, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } } },
};

template <class E> static void gfx_alg(E &eng, lkg_t *g)
{
	const int alg = eng.get_param('V', 52) & 7, fb = eng.get_param('V', 53);
	const t_algpic &a = ALGPIC[alg];
	const int BW = 13, BH = 11;
	auto bx = [&](int op) { return 8 + a.col[op - 1] * 28; };
	auto by = [&](int op) { return 11 + a.row[op - 1] * 11; };
	bool carrier[4] = { true, true, true, true };
	for (int k = 0; k < 4; k++)
		if (a.con[k][0]) carrier[a.con[k][0] - 1] = false;
	for (int op = 1; op <= 4; op++) {
		lkg_rect(g, bx(op), by(op), BW, BH);
		const char d[2] = { char('0' + op), 0 };
		lkg_text(g, bx(op) + 4, by(op) + 2, d);
	}
	for (int k = 0; k < 4 && a.con[k][0]; k++) {
		const int f = a.con[k][0], t = a.con[k][1];
		if (a.col[t - 1] > a.col[f - 1] && std::abs(a.row[t - 1] - a.row[f - 1]) < 2)   // right edge to left edge
			lkg_line(g, float(bx(f) + BW), float(by(f) + BH / 2), float(bx(t) - 1), float(by(t) + BH / 2));
		else                                   // below: bottom to top
			lkg_line(g, float(bx(f) + BW / 2), float(by(f) + BH), float(bx(t) + BW / 2), float(by(t) - 1));
	}
	// the carriers to the output line, and the line to the right edge
	int xl = LKG_W, xr = 0;
	for (int op = 1; op <= 4; op++) {
		if (!carrier[op - 1]) continue;
		const int cx = bx(op) + BW / 2;
		lkg_line(g, float(cx), float(by(op) + BH), float(cx), 46.0f);
		xl = cx < xl ? cx : xl;
		xr = cx > xr ? cx : xr;
	}
	lkg_line(g, float(xl), 46.0f, float(LKG_W - 1), 46.0f);
	// OP4's feedback: a loop above its box, filled box corner when it is on
	const int fx = bx(4), fy = by(4);
	lkg_line(g, float(fx + BW - 3), float(fy - 1), float(fx + BW - 3), float(fy - 3 < 8 ? 8 : fy - 3));
	if (fy - 3 >= 8) {
		lkg_line(g, float(fx + BW - 3), float(fy - 3), float(fx + 2), float(fy - 3));
		lkg_line(g, float(fx + 2), float(fy - 3), float(fx + 2), float(fy - 1));
	}
	char t[8];
	std::snprintf(t, sizeof t, "FB%d", fb);
	lkg_text(g, LKG_W - 6 * 3, 11, t);
}

// The envelope: attack (ar 0-31) to the top, decay 1 (d1r) to the decay 1 level (d1l 0-15),
// decay 2 (d2r, 0 = none) while the key is held (dotted line: key up), release (rr 1-15).
template <class E> static void gfx_env(E &eng, lkg_t *g, int op)
{
	const int b = OPBLOCK[op] * 13;
	const float ar = float(eng.get_param('V', b + 0)), d1r = float(eng.get_param('V', b + 1));
	const float d2r = float(eng.get_param('V', b + 2)), rr = float(eng.get_param('V', b + 3));
	const float d1l = float(eng.get_param('V', b + 4)) / 15.0f;
	const float l2 = d2r > 0 ? d1l * (1.0f - d2r / 31.0f * 0.9f) : d1l;
	const float lvl[4] = { 1.0f, d1l, l2, 0.0f };
	const float w[4] = { 2.0f + (31.0f - ar) * 1.4f,
		d1r > 0 ? 2.0f + (31.0f - d1r) * 1.2f * (0.3f + 0.7f * (1.0f - d1l)) : 40.0f,
		32.0f,
		2.0f + (15.0f - rr) * 3.0f };
	float xs[5];
	lkg_env(g, 4, 0.0f, lvl, w, -1, 0.0f, xs);
	lkg_vdots(g, int(lroundf(xs[3])), int(LKG_TOP), int(LKG_BOT), 3);
}

// The 8 TX81Z waves (OPZ), one period: W1 sine, W2 sine with flattened negative half (sin|sin|),
// W3 half sine, W4 half of W2, W5..W8 the same at double speed in the first half, silent after.
static float tx_wave(int w, float ph)
{
	const float pi2 = 6.2831853f;
	if (w >= 4) {
		if (ph >= 0.5f) return 0.0f;
		ph *= 2.0f;
		w -= 4;
		if (w >= 2) { const float s = std::sin(pi2 * ph); return w == 2 ? std::fabs(s) : s * std::fabs(s) * (s < 0 ? -1.0f : 1.0f); }
	}
	const float s = std::sin(pi2 * ph);
	switch (w) {
	case 0: return s;
	case 1: return s >= 0 ? s : s * std::fabs(s);
	case 2: return s >= 0 ? s : 0.0f;
	default: return s >= 0 ? s * s : 0.0f;
	}
}

template <class E> static void gfx_wave(E &eng, lkg_t *g, int op)
{
	const int w = eng.get_param('A', OPBLOCK[op] * 5 + 3) & 7;
	float v[LKG_W];
	for (int i = 0; i < LKG_W; i++) v[i] = tx_wave(w, float(i % 64) / 64.0f);   // two periods
	lkg_plot(g, v, LKG_W, -1.0f, 1.0f);
	lkg_hdots(g, 0, LKG_W - 1, int(lroundf(lkg_ly(0.5f))), 4);
}

// kind 1 the algorithm, 2..5 the envelope of OP1..OP4, 6..9 the wave of OP1..OP4;
// eng: anything with get_param(group, number)
template <class E> static void txgfx_draw(E &eng, lkg_t *g, int kind)
{
	if (kind == 1) gfx_alg(eng, g);
	else if (kind >= 2 && kind <= 5) gfx_env(eng, g, kind - 2);
	else if (kind >= 6 && kind <= 9) gfx_wave(eng, g, kind - 6);
}

