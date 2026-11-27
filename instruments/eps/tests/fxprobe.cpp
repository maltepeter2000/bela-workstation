// fxprobe ROM OS INSTRUMENT.efe: load the instrument's effect into the effect machine, list
// its parameters, run noise through it. Needs the user's EPS-16 PLUS ROM and OS file (not part
// of eps~); not in make test.
#include "../pd/engine/eps_fx.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
static std::vector<uint8_t> load(const char *f) {
    std::ifstream s(f, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(s)), {});
}
int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "fxprobe ROM OS INSTRUMENT.efe\n"); return 1; }
    eps::FxMachine fx;
    std::string err;
    if (!fx.setSystem(load(argv[1]), load(argv[2]), err)) { printf("system: %s\n", err.c_str()); return 1; }
    std::vector<uint8_t> in = load(argv[3]);
    size_t skip = in.size() > 512 && in[0] == 0x0d ? 512 : 0;
    const uint8_t *d = in.data() + skip;
    const bool fxfile = skip && (in[0x32] == 24 || in[0x32] == 0x18);   // an effect file: the block itself
    const uint32_t off = fxfile ? 0 : (uint32_t(d[647] >> 4) << 20) | uint32_t(d[644]) << 12 | uint32_t(d[646]) << 4 | (d[647] & 15);
    size_t len = in.size() - skip - off;
    if (!fx.load(d + off, len, err)) { printf("load: %s\n", err.c_str()); return 1; }
    if (getenv("DUMP")) {
        FILE *f = fopen(getenv("DUMP"), "w");
        for (int i = 0; i < 160; ++i) fprintf(f, "i %d %012llx\n", i, (unsigned long long)fx.esp().instruction(i));
        for (int i = 0; i < 192; ++i) fprintf(f, "g %d %06x\n", i, fx.esp().gpr(i) & 0xffffff);
        fclose(f);
    }
    printf("effect \"%s\", rate %.0f Hz, %zu parameters\n", fx.name().c_str(), fx.rate(), fx.params().size());
    for (const auto &p : fx.params())
        printf("  %-16s t%02x %d..%d = %d%s%s\n", p.name.c_str(), p.type, p.lo, p.hi, p.value, p.variation ? "  (variation)" : "",
               p.items.empty() ? "" : ("  [" + p.items[size_t(p.value) < p.items.size() ? size_t(p.value) : 0] + "]").c_str());
    unsigned seed = 1;
    long long peak = 0, sum = 0;
    const int n = int(fx.rate());
    for (int f = 0; f < n; ++f) {
        if (!getenv("NOTICK") && f % (n / 77) == 0) fx.tick();
        if (getenv("NOTICK") && f == 0) for (int k = 0; k < 12; ++k) fx.tick();
        int16_t x[6];
        for (int c = 0; c < 6; ++c) { seed = seed * 1103515245u + 12345u; x[c] = f < n / 10 ? int16_t(int16_t(seed >> 16) / (getenv("DIV") ? atoi(getenv("DIV")) : 8)) : 0; }
        int16_t l, r;
        fx.frame(x, l, r);
        peak = std::max<long long>(peak, std::abs(l) + 0LL);
        if (f > n / 10) sum += std::abs(l);
        static long long seg = 0;
        seg = std::max<long long>(seg, std::abs(l) + 0LL);
        if (f % (n / 10) == n / 10 - 1) { printf("  %.1f s: peak %lld\n", (f + 1) / double(n), seg); seg = 0; }
    }
    printf("1 s: noise burst 0.1 s, output peak %lld, tail mean %.1f\n", peak, double(sum) / (n * 0.9));
    return 0;
}
