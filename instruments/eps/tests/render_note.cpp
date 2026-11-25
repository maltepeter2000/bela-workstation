// Debug helper: render one note of an instrument to a mono float dump and a WAV.
//   render_note <image> <file path|name> <note> <velocity> <seconds> <out.wav> [patch]
#include "../pd/engine/eps_sampler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: render_note <image> <file> <note> <vel> <seconds> <out.wav> [patch] [noteoff_s]\n");
        return 2;
    }
    eps::Disk disk;
    std::string err;
    if (!disk.open(argv[1], err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    const eps::DirEntry *target = nullptr;
    for (const auto &e : disk.files())
        if (e.path == argv[2] || e.name == argv[2]) target = &e;
    if (!target) { fprintf(stderr, "no such file\n"); return 1; }
    eps::Instrument inst;
    if (!disk.loadInstrument(*target, inst, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }

    eps::Sampler::Settings set;
    eps::Sampler s(set);
    if (!s.load(inst, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    if (argc > 7) s.setPatch(atoi(argv[7]));
    const int note = atoi(argv[3]), vel = atoi(argv[4]);
    const int frames = int(atof(argv[5]) * set.sampleRate);
    const int offAt = argc > 8 ? int(atof(argv[8]) * set.sampleRate) : frames;
    s.noteOn(note, vel);
    std::vector<float> L(static_cast<size_t>(frames)), R(static_cast<size_t>(frames));
    for (int i = 0; i < frames; i += 64) {
        if (i <= offAt && offAt < i + 64) s.noteOff(note);
        int n = frames - i < 64 ? frames - i : 64;
        s.process(&L[size_t(i)], &R[size_t(i)], n);
    }
    FILE *f = fopen(argv[6], "wb");
    if (!f) return 1;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    uint32_t n = uint32_t(frames);
    fwrite("RIFF", 1, 4, f); w32(36 + n * 4); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(44100); w32(44100 * 4); w16(4); w16(16);
    fwrite("data", 1, 4, f); w32(n * 4);
    float peak = 0;
    for (uint32_t i = 0; i < n; ++i) {
        peak = std::fmax(peak, std::fmax(std::fabs(L[i]), std::fabs(R[i])));
        auto cv = [](float x) { x = x > 1 ? 1 : (x < -1 ? -1 : x); return int16_t(std::lround(x * 32767)); };
        int16_t a = cv(L[i]), b = cv(R[i]);
        fwrite(&a, 2, 1, f); fwrite(&b, 2, 1, f);
    }
    fclose(f);
    printf("rendered %s note %d vel %d: peak %.3f, voices at end %d\n", inst.name.c_str(), note, vel, peak,
           s.activeVoiceCount());
    return 0;
}
