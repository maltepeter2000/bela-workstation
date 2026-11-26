// test_record: the wave recorder's engine (eps_record) and the instrument builder.
//   test_record <fixtures dir>
// New instrument, multisampling with the split points between root keys, replace,
// new layer, key ranges, the rebuilt file read back, and the sample rate conversion.
#include "../pd/engine/eps_loader.h"
#include "../pd/engine/eps_record.h"
#include "../pd/engine/eps_sampler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace eps;

static int failures = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } \
    } while (0)

static std::vector<float> sine(double hz, double sr, double secs, float amp = 0.5f) {
    std::vector<float> v(size_t(sr * secs));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * float(std::sin(2 * M_PI * hz * double(i) / sr));
    return v;
}

// Frequency from zero crossings (rising) of a 16-bit signal.
static double freqOf(const std::vector<int16_t> &x, double sr) {
    long first = -1, last = -1, n = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) {
            if (first < 0) first = long(i);
            last = long(i);
            ++n;
        }
    return n > 1 ? double(n - 1) * sr / double(last - first) : 0;
}

// Build the file, read it back.
static bool rebuild(const Instrument &in, Instrument &out) {
    std::vector<uint8_t> f;
    std::string err;
    if (!buildInstrument(in, f, err)) { std::printf("build: %s\n", err.c_str()); return false; }
    if (f.size() % 512) return false;
    if (!parseInstrument(f.data(), f.size(), "", out, err)) { std::printf("parse: %s\n", err.c_str()); return false; }
    return true;
}

static int key(const Instrument &in, int layer, int note) { return in.layers[size_t(layer)].waveSampleForNote(note); }

int main(int argc, char **argv) {
    std::string err;
    // rate conversion: 1 kHz at 48 kHz -> 29.8 kHz keeps 1 kHz and the length
    {
        const int to = sampleRateOfIndex(kRecordRateIndex[1]);
        CHECK(to == 29762);
        const auto pcm = resampleRecording(sine(1000, 48000, 1.0), 48000, to);
        CHECK(std::labs(long(pcm.size()) - to) <= 1);
        CHECK(std::fabs(freqOf(pcm, to) - 1000.0) < 0.5);
        // a tone above the new Nyquist is filtered out
        const auto hi = resampleRecording(sine(17000, 48000, 0.5), 48000, sampleRateOfIndex(56));
        long peak = 0;
        for (size_t i = 100; i + 100 < hi.size(); ++i) peak = std::max(peak, std::labs(hi[i]));
        CHECK(peak < 100);
    }
    // new instrument, then multisampling as on the EPS
    Instrument in;
    int layer = -1;
    auto rec = [&](RecordDest d, int l, int ws, int root, double hz) {
        const int idx = kRecordRateIndex[1];
        const auto pcm = resampleRecording(sine(hz, 44100, 0.25), 44100, sampleRateOfIndex(idx));
        return placeRecording(in, d, l, ws, pcm, idx, root, layer, err);
    };
    int w1 = rec(RecordDest::NewInstrument, 0, 0, 60, 261.6);
    CHECK(w1 == 1 && layer == 0 && in.layers.size() == 1 && in.name == "UNNAMED INST");
    CHECK(key(in, 0, 21) == 1 && key(in, 0, 108) == 1);
    CHECK(in.waveSamples[1].pcm.size() == size_t(29762 / 4) + 8);           // 8 zeros after it
    CHECK(in.waveSamples[1].sampleEnd == uint32_t(29762 / 4));
    int w2 = rec(RecordDest::NewWaveSample, 0, 0, 72, 523.3);
    CHECK(w2 == 2);
    CHECK(key(in, 0, 66) == 1 && key(in, 0, 67) == 2 && key(in, 0, 108) == 2);   // split at (60 + 72) / 2
    int w3 = rec(RecordDest::NewWaveSample, 0, 0, 48, 130.8);
    CHECK(w3 == 3 && key(in, 0, 54) == 3 && key(in, 0, 55) == 1 && key(in, 0, 21) == 3);
    CHECK(in.waveSamples[1].keyLow == 55 && in.waveSamples[1].keyHigh == 66);
    // replace keeps keys and parameters
    in.waveSamples[2].filterCutoff[0] = 77;
    CHECK(rec(RecordDest::Replace, 0, 2, 74, 587.3) == 2);
    CHECK(in.waveSamples[2].filterCutoff[0] == 77 && in.waveSamples[2].rootNote == 74 && key(in, 0, 67) == 2);
    // new layer
    CHECK(rec(RecordDest::NewLayer, 0, 0, 60, 440) == 4 && layer == 1 && in.layers.size() == 2);
    CHECK((in.patches[0] & 2) && key(in, 1, 30) == 4);
    // key range: shrink 1 to 58..64, its other keys go to the neighbours
    setKeyRange(in, 0, 1, 58, 64);
    CHECK(key(in, 0, 55) == 3 && key(in, 0, 57) == 3 && key(in, 0, 58) == 1 && key(in, 0, 64) == 1 && key(in, 0, 65) == 2);
    CHECK(in.waveSamples[3].keyHigh == 57 && in.waveSamples[2].keyLow == 65);

    // the file: read back, what plays is the same
    Instrument back;
    CHECK(rebuild(in, back));
    CHECK(back.layers.size() == 2 && back.waveSamples.size() == 4);
    for (int n = 21; n <= 108; ++n) CHECK(key(back, 0, n) == key(in, 0, n));
    for (auto &kv : in.waveSamples) {
        const WaveSample &a = kv.second, &b = back.waveSamples[kv.first];
        CHECK(a.pcm == b.pcm && a.rootNote == b.rootNote && a.sampleRateIndex == b.sampleRateIndex);
        CHECK(a.sampleEnd == b.sampleEnd && a.keyLow == b.keyLow && a.keyHigh == b.keyHigh);
    }
    CHECK(std::fabs(freqOf(back.waveSamples[2].pcm, 29762) - 587.3) < 1.0);
    // it plays
    {
        Sampler::Settings st;
        st.sampleRate = 44100;
        Sampler s(st);
        CHECK(s.load(back, err));
        s.noteOn(74, 100);
        std::vector<float> l(4096), r(4096);
        float pk = 0;
        for (int b = 0; b < 4; ++b) {
            s.process(l.data(), r.data(), 4096);
            for (float v : l) pk = std::max(pk, std::fabs(v));
        }
        CHECK(pk > 0.01f);
    }
    // an instrument from the fixtures: a recording added, everything else stays
    if (argc > 1) {
        Disk d;
        if (d.open(std::string(argv[1]) + "/test.efe", err) && !d.instruments().empty()) {
            Instrument f;
            CHECK(d.loadInstrument(*d.instruments()[0], f, err));
            Instrument copy = f;
            in = f;
            const int n = rec(RecordDest::NewWaveSample, 0, 0, 84, 1046.5);
            CHECK(n > 0);
            Instrument b2;
            CHECK(rebuild(in, b2));
            for (auto &kv : copy.waveSamples) CHECK(b2.waveSamples[kv.first].pcm == kv.second.pcm);
            CHECK(b2.waveSamples.count(n) == 1);
        } else {
            std::printf("no fixture: %s\n", err.c_str());
            ++failures;
        }
    }
    std::printf("test_record: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
