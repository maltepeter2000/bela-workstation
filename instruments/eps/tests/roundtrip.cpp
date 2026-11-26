// roundtrip: every instrument of the given images must come out of
// encodeInstrument byte for byte as it was read; a changed field must land in
// the file and change nothing else than its own bytes.
#include "../pd/engine/eps_loader.h"

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv) {
    int failures = 0, count = 0;
    for (int a = 1; a < argc; ++a) {
        eps::Disk disk;
        std::string err;
        if (!disk.open(argv[a], err)) { fprintf(stderr, "%s: %s\n", argv[a], err.c_str()); ++failures; continue; }
        for (const eps::DirEntry *e : disk.instruments()) {
            std::vector<uint8_t> raw, out;
            eps::Instrument in;
            if (!disk.readFile(*e, raw, err) || !disk.loadInstrument(*e, in, err)) {
                fprintf(stderr, "%s %s: %s\n", argv[a], e->name.c_str(), err.c_str());
                ++failures;
                continue;
            }
            ++count;
            if (!eps::encodeInstrument(in, out, err)) { fprintf(stderr, "encode: %s\n", err.c_str()); ++failures; continue; }
            raw.resize(out.size());
            size_t diff = 0;
            for (size_t i = 0; i < out.size(); ++i) diff += out[i] != raw[i];
            if (diff) { fprintf(stderr, "FAIL %s %s: %zu bytes differ\n", argv[a], e->name.c_str(), diff); ++failures; }
            // change some fields, write, read back
            eps::Instrument ch = in;
            ch.name = "ROUNDTRIP";
            ch.transpose = in.transpose == 3 ? 4 : 3;
            ch.layers[0].velocityLow = 7;
            auto &w = ch.waveSamples.begin()->second;
            w.filterCutoff[0] = 33;
            w.fineTune = -5;
            w.ampEnv.level5 = -20;
            w.waveModRaw[3] = 6;
            if (!eps::encodeInstrument(ch, out, err)) { fprintf(stderr, "encode: %s\n", err.c_str()); ++failures; continue; }
            eps::Instrument back;
            if (!eps::parseInstrument(out.data(), out.size(), "", back, err)) { fprintf(stderr, "reparse: %s\n", err.c_str()); ++failures; continue; }
            const auto &wb = back.waveSamples.begin()->second;
            const bool ok = back.name == "ROUNDTRIP" && back.transpose == ch.transpose &&
                            back.layers[0].velocityLow == 7 && wb.filterCutoff[0] == 33 && wb.fineTune == -5 &&
                            wb.ampEnv.level5 == -20 && wb.waveModRaw[3] == 6;
            diff = 0;
            for (size_t i = 0; i < out.size(); ++i) diff += out[i] != raw[i];
            if (!ok || diff > 20) { fprintf(stderr, "FAIL %s %s: changed fields (%zu bytes differ)\n", argv[a], e->name.c_str(), diff); ++failures; }
        }
    }
    printf("roundtrip: %d instruments, %d failures\n", count, failures);
    return failures ? 1 : 0;
}
