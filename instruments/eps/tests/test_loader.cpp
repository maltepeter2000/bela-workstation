// Checks eps_loader against the synthetic fixtures from make_fixtures.py.
#include "../pd/engine/eps_loader.h"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { fprintf(stderr, "  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static long sum(const std::vector<int16_t> &v) {
    long s = 0;
    for (int16_t x : v) s += x;
    return s;
}

static void checkInstrument(const eps::Instrument &in) {
    CHECK(in.name == "TEST SYNTH");
    CHECK(in.instrumentId == eps::kIdEps16Plus);
    CHECK(in.keyLow == 24 && in.keyHigh == 108 && in.transpose == -2);
    CHECK(in.patches[0] == 1 && in.patches[1] == 3 && in.patches[2] == 2 && in.patches[3] == 4);
    CHECK(in.keyDownLayers == 7 && in.keyUpLayers == 2);
    CHECK(in.sizeBlocks == 99);
    CHECK(in.layers.size() == 3);
    if (in.layers.size() == 3) {
        const eps::Layer &a = in.layers[0], &b = in.layers[1];
        CHECK(a.index == 1 && a.name == "LAYER A" && a.velocityLow == 0 && a.velocityHigh == 127);
        CHECK(a.waveSampleForNote(21) == 1 && a.waveSampleForNote(59) == 1);
        CHECK(a.waveSampleForNote(60) == 2 && a.waveSampleForNote(108) == 2);
        CHECK(a.waveSampleForNote(20) == 0 && a.waveSampleForNote(109) == 0);
        CHECK(b.index == 2 && b.velocityLow == 64 && b.glideTime == 5);
        CHECK(b.waveSampleForNote(50) == 0 && b.waveSampleForNote(51) == 3 && b.waveSampleForNote(80) == 3);
        CHECK(b.waveSampleForNote(81) == 0);
    }
    CHECK(in.waveSamples.size() == 4);
    auto it1 = in.waveSamples.find(1), it2 = in.waveSamples.find(2), it3 = in.waveSamples.find(3);
    if (it1 == in.waveSamples.end() || it2 == in.waveSamples.end() || it3 == in.waveSamples.end()) {
        ++failures;
        return;
    }
    const eps::WaveSample &w1 = it1->second, &w2 = it2->second, &w3 = it3->second;
    CHECK(w1.name == "SAW LOW" && w1.rootNote == 48 && w1.sampleRate == 44100);
    CHECK(w1.pcm.size() == 1100);
    CHECK(w1.loopMode == 2 && w1.loopStart == 200 && w1.loopEnd == 1000 && w1.sampleEnd == 1099);
    CHECK(w1.keyLow == 21 && w1.keyHigh == 59);
    CHECK(w1.volume == 100 && w1.panEps == 4 && w1.panSigned == -30 && w1.fineTune == -5);
    CHECK(w1.pitchEnvAmount == -16 && w1.lfoPitchAmount == 8 && w1.pitchBendRange == 2);
    CHECK(w1.filterMode == 2 && w1.filterCutoff[0] == 80 && w1.filterCutoff[1] == 90);
    CHECK(w1.filterKeyAmount[0] == 10 && w1.filterEnvAmount[0] == 30 && w1.filterModSource[0] == 5);
    CHECK(w1.filterModAmount[0] == 40 && w1.ampModAmount == 50);
    CHECK(w1.crossfade[0] == 1 && w1.crossfade[3] == 4);
    CHECK(w1.lfoWaveform == 2 && w1.lfoSpeed == 33 && w1.lfoDepth == 64 && w1.lfoDelay == 20);
    CHECK(w1.lfoDepthModAmount == 0 && w1.lfoDepthModSource == 10 && w1.lfoMode == 1);
    // Envelope seed 3 (amp): see envelope() in make_fixtures.py
    CHECK(w1.ampEnv.type == 3 && w1.ampEnv.softLevel[0] == 69 && w1.ampEnv.hardLevel[0] == 73);
    CHECK(w1.ampEnv.time[1] == 31 && w1.ampEnv.softLevel[1] == 81 && w1.ampEnv.time[2] == 46);
    CHECK(w1.ampEnv.time[4] == 76 && w1.ampEnv.hardLevel[4] == 121);
    CHECK(w1.ampEnv.time[5] == 91 && w1.ampEnv.level5 == -20 && w1.ampEnv.time[6] == 3);
    CHECK(w1.pitchEnv.type == 1 && w1.filterEnv.time[6] == 2);
    CHECK(w2.sampleRate == 25000 && w2.loopMode == 2 && w2.loopStart == 64 && w2.loopEnd == 640 && w2.pcm.size() == 704);
    CHECK(w1.waveModAmount() == 0);
    auto it4 = in.waveSamples.find(4);
    CHECK(it4 != in.waveSamples.end());
    if (it4 != in.waveSamples.end()) {
        const eps::WaveSample &w4 = it4->second;
        CHECK(w4.waveModType() == 6 && w4.waveModTypeRaw() == 7 && w4.waveModSource() == 10 && w4.waveModAmount() == 127);
        CHECK(w4.waveModAmountDisplay() == 99 && w4.waveModRangeSamples() == 1024);
        CHECK(w4.loopStart == 0 && w4.loopEnd == 100 && w4.sampleEnd == 799 && w4.pcm.size() == 800);
    }
    CHECK(w3.copyOfWaveSample == 1 && w3.pcm.empty());
    const eps::WaveSample *owner = in.pcmOwner(w3);
    CHECK(owner == &w1);
}

int main(int argc, char **argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    long sineSum = 0, sawSum = 0;
    if (FILE *f = fopen((dir + "/expected.txt").c_str(), "r")) {
        char key[32];
        long v;
        while (fscanf(f, "%31s %ld", key, &v) == 2)
            { if (std::string(key) == "sine_sum") sineSum = v; if (std::string(key) == "saw_sum") sawSum = v; }
        fclose(f);
    }
    for (const char *name : {"test.efe", "test.img", "test.ede"}) {
        printf("%s\n", name);
        eps::Disk disk;
        std::string err;
        if (!disk.open(dir + "/" + name, err)) {
            fprintf(stderr, "  FAIL open: %s\n", err.c_str());
            ++failures;
            continue;
        }
        auto inst = disk.instruments();
        CHECK(inst.size() == 1);
        if (inst.empty()) continue;
        eps::Instrument in;
        if (!disk.loadInstrument(*inst[0], in, err)) {
            fprintf(stderr, "  FAIL load: %s\n", err.c_str());
            ++failures;
            continue;
        }
        checkInstrument(in);
        CHECK(sum(in.waveSamples[2].pcm) == sineSum);
        CHECK(sum(in.waveSamples[1].pcm) == sawSum);
        if (std::string(name) != "test.efe") {
            CHECK(disk.label() == "TESTDSK" && disk.osVersion() == "1.30");
            CHECK(disk.isStreaming() == (std::string(name) == "test.img"));
            CHECK(inst[0]->path == "00 03" && inst[0]->contiguousBlocks == 3);
        }
    }

    // Raw image loaded from memory instead of streamed from file
    {
        FILE *f = fopen((dir + "/test.img").c_str(), "rb");
        std::vector<uint8_t> d;
        if (f) {
            int ch;
            while ((ch = fgetc(f)) != EOF) d.push_back(uint8_t(ch));
            fclose(f);
        }
        eps::Disk disk;
        std::string err;
        CHECK(disk.openMemory(d, "test.img", err));
        CHECK(!disk.isStreaming());
        auto inst = disk.instruments();
        eps::Instrument in;
        CHECK(inst.size() == 1 && disk.loadInstrument(*inst[0], in, err));
        if (!in.layers.empty()) checkInstrument(in);
    }

    // Robustness: truncated and garbage inputs must fail cleanly.
    {
        eps::Disk disk;
        std::string err;
        CHECK(!disk.openMemory(std::vector<uint8_t>(100, 0x55), "junk.img", err));
        std::vector<uint8_t> tiny(700, 0);
        eps::Instrument in;
        CHECK(!eps::parseInstrument(tiny.data(), 100, "x", in, err));
        // Offsets pointing past the end
        tiny[100] = 0xFF; tiny[103] = 0xFF;
        CHECK(!eps::parseInstrument(tiny.data(), tiny.size(), "x", in, err));
    }

    printf(failures ? "%d FAILURE(S)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
