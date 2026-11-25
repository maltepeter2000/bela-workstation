// Mutates the fixtures randomly and loads them; run under ASan/UBSan to catch
// out-of-bounds reads on corrupt disk images.
#include "../pd/engine/eps_loader.h"
#include "../pd/engine/eps_sampler.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>

int main(int argc, char **argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    std::mt19937 rng(1234);
    int loaded = 0, rejected = 0;
    for (const char *name : {"test.efe", "test.img", "test.ede"}) {
        std::ifstream f(dir + "/" + name, std::ios::binary);
        std::vector<uint8_t> orig((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        for (int iter = 0; iter < 3000; ++iter) {
            std::vector<uint8_t> d = orig;
            int edits = 1 + rng() % 16;
            for (int e = 0; e < edits; ++e) {
                // Bias edits toward headers, directories and the instrument header.
                size_t span = (rng() % 4 == 0) ? d.size() : std::min<size_t>(d.size(), 20000);
                d[rng() % span] = uint8_t(rng());
            }
            if (rng() % 10 == 0) d.resize(rng() % d.size());
            eps::Disk disk;
            std::string err;
            if (!disk.openMemory(d, name, err)) { ++rejected; continue; }
            for (const eps::DirEntry *e : disk.instruments()) {
                eps::Instrument in;
                if (disk.loadInstrument(*e, in, err)) {
                    ++loaded;
                    for (auto &kv : in.waveSamples) (void)in.pcmOwner(kv.second);
                    // Play the corrupt instrument too: the sampler must not
                    // read outside sample memory whatever the parameters say.
                    if (iter % 4 == 0) {
                        eps::Sampler::Settings set;
                        eps::Sampler s(set);
                        if (s.load(in, err)) {
                            float l[256], r[256];
                            s.setModWheel(float(rng() % 128) / 127.f);
                            for (int k = 0; k < 6; ++k) s.noteOn(int(rng() % 128), 1 + int(rng() % 127));
                            s.setPatch(int(rng() % 4));
                            for (int k = 0; k < 4; ++k) s.process(l, r, 256);
                            s.noteOff(60);
                            s.process(l, r, 256);
                        }
                    }
                } else {
                    ++rejected;
                }
            }
        }
    }
    printf("fuzz done: %d loaded, %d rejected, no crash\n", loaded, rejected);
    return 0;
}
