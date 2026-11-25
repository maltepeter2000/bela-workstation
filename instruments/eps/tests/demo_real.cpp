// Renders a short demo of an EPS instrument file to a stereo WAV:
// a few notes and chords; with --sweep the mod wheel moves 0 -> 1 -> 0 while
// a chord is held (transwave / loop position modulation).
//   demo_real <file.efe> <out.wav> [--sweep] [--offset N]
#include "../pd/engine/eps_sampler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void writeWav(const char *path, const std::vector<float> &l, const std::vector<float> &r) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    float peak = 1e-9f;
    for (size_t i = 0; i < l.size(); ++i) peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(r[i])));
    const float norm = 0.9f / peak;   // normalize for listening
    uint32_t n = uint32_t(l.size());
    fwrite("RIFF", 1, 4, f); w32(36 + n * 4); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(44100); w32(44100 * 4); w16(4); w16(16);
    fwrite("data", 1, 4, f); w32(n * 4);
    for (uint32_t i = 0; i < n; ++i) {
        int16_t a = int16_t(std::lround(l[i] * norm * 32767)), b = int16_t(std::lround(r[i] * norm * 32767));
        fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
    }
    fclose(f);
    printf("  %s: %.1f s, peak before normalizing %.3f\n", path, double(n) / 44100, double(peak));
}

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    bool sweep = false;
    int offset = 0;
    for (int i = 3; i < argc; ++i) {
        if (!strcmp(argv[i], "--sweep")) sweep = true;
        if (!strcmp(argv[i], "--offset") && i + 1 < argc) offset = atoi(argv[++i]);
    }
    eps::Disk disk;
    std::string err;
    if (!disk.open(argv[1], err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    eps::Instrument inst;
    if (!disk.loadInstrument(*disk.instruments()[0], inst, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    eps::Sampler::Settings set;
    set.pitchOffset = offset;
    eps::Sampler s(set);
    if (!s.load(inst, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }

    std::vector<float> L, R;
    auto run = [&](float seconds, float wheelFrom = -1, float wheelTo = -1) {
        int frames = int(seconds * 44100);
        std::vector<float> l(64), r(64);
        for (int i = 0; i < frames; i += 64) {
            if (wheelFrom >= 0) s.setModWheel(wheelFrom + (wheelTo - wheelFrom) * float(i) / float(frames));
            s.process(l.data(), r.data(), 64);
            L.insert(L.end(), l.begin(), l.end());
            R.insert(R.end(), r.begin(), r.end());
        }
    };
    const int melody[] = {48, 55, 60, 63, 67, 60, 55, 51};
    for (int n : melody) {
        s.noteOn(n, 110);
        run(0.3f);
        s.noteOff(n);
        run(0.05f);
    }
    run(0.5f);
    const int chord[] = {60, 63, 67, 70};
    for (int n : chord) s.noteOn(n, 100);
    if (sweep) {
        run(4.f, 0.f, 1.f);
        run(4.f, 1.f, 0.f);
    } else {
        run(2.5f);
    }
    for (int n : chord) s.noteOff(n);
    run(2.5f);
    printf("%s (%s)\n", inst.name.c_str(), inst.modelName());
    writeWav(argv[2], L, R);
    return 0;
}
