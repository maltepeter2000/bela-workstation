// Host test of the TX81Z pictures (pd/txgfx.h): every algorithm, an envelope and the 8 waves,
// printed as text (one picture after "== <name>", "#" = pixel). c++ -std=c++17 -Ipd tests/test_txgfx.cpp
#include "txgfx.h"
#include <cstring>

struct Mock {
	int v[93] = {}, a[23] = {};
	int get_param(char g, int n) const { return g == 'V' ? v[n] : a[n]; }
};

static void print(const char *name, const lkg_t &g)
{
	std::printf("== %s\n", name);
	for (int y = 0; y < LKG_H; y++) {
		for (int x = 0; x < LKG_W; x++) std::putchar(g.fb[y >> 3][x] >> (y & 7) & 1 ? '#' : '.');
		std::putchar('\n');
	}
}

int main()
{
	Mock m;
	lkg_t g;
	char t[32];
	for (int alg = 0; alg < 8; alg++) {
		m.v[52] = alg; m.v[53] = alg % 8;
		std::snprintf(t, sizeof t, "ALGORITHM %d", alg + 1);
		lkg_clear(&g, t); txgfx_draw(m, &g, 1); print(t, g);
	}
	const int b = txfm::OP1 * 13;
	m.v[b] = 28; m.v[b + 1] = 14; m.v[b + 2] = 6; m.v[b + 3] = 7; m.v[b + 4] = 10;
	lkg_clear(&g, "OP1 ATTACK RATE 28"); txgfx_draw(m, &g, 2); print("env", g);
	for (int w = 0; w < 8; w++) {
		m.a[txfm::OP1 * 5 + 3] = w;
		std::snprintf(t, sizeof t, "OP1 WAVEFORM W%d", w + 1);
		lkg_clear(&g, t); txgfx_draw(m, &g, 6); print(t, g);
	}
	return 0;
}
