// test_wave: the wavesample commands (eps_wave).
#include "../pd/engine/eps_loader.h"
#include "../pd/engine/eps_record.h"
#include "../pd/engine/eps_wave.h"

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

static double freqOf(const std::vector<int16_t> &x, long a, long b, double sr) {
    long first = -1, last = -1, n = 0;
    for (long i = a + 1; i < b; ++i)
        if (x[size_t(i - 1)] < 0 && x[size_t(i)] >= 0) { if (first < 0) first = i; last = i; ++n; }
    return n > 1 ? double(n - 1) * sr / double(last - first) : 0;
}

// An instrument with one wavesample: 0.5 s of a 440 Hz sine at 29.8 kHz, looped.
static Instrument make() {
    Instrument in;
    int layer = 0;
    std::string err;
    const int sr = sampleRateOfIndex(21);
    std::vector<int16_t> pcm(size_t(sr / 2));
    for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = int16_t(lround(8000 * std::sin(2 * M_PI * 440 * double(i) / sr)));
    placeRecording(in, RecordDest::NewInstrument, 0, 0, pcm, 21, 69, layer, err);
    WaveSample &w = in.waveSamples[1];
    w.loopMode = 2;
    w.rawLoopStart = 5000u << 9;
    w.rawLoopEnd = 12345u << 9;
    return in;
}

static bool reparse(const Instrument &in) {
    std::vector<uint8_t> f;
    std::string err;
    Instrument b;
    return buildInstrument(in, f, err) && parseInstrument(f.data(), f.size(), "", b, err) &&
           b.waveSamples.size() == in.waveSamples.size();
}

int main() {
    std::string err;
    int sel = 0;
    WaveArgs a;
    const int sr = sampleRateOfIndex(21);
    {   // truncate
        Instrument in = make();
        WaveSample &w = in.waveSamples[1];
        w.rawStart = 1000u << 9;
        w.rawEnd = 13000u << 9;
        CHECK(waveCommand(in, 0, 1, WaveCmd::Truncate, a, sel, err));
        CHECK(w.pcm.size() == 12000 + 8 && w.sampleStart == 0 && w.sampleEnd == 12000);
        CHECK(w.loopStart == 4000 && (w.rawLoopEnd >> 9) == 11345);
        CHECK(!waveCommand(in, 0, 1, WaveCmd::Truncate, a, sel, err) && err == "NOTHING TO TRUNCATE");
        CHECK(reparse(in));
    }
    {   // cross fade: at loop end the data is where loop start is
        Instrument in = make();
        const std::vector<int16_t> old = in.waveSamples[1].pcm;
        CHECK(waveCommand(in, 0, 1, WaveCmd::CrossFade, a, sel, err));
        const auto &d = in.waveSamples[1].pcm;
        CHECK(d[12345] == old[5000] && d[12344] != old[12344] && d[5000] == old[5000]);
        CHECK(d[12345 - 3672] == old[12345 - 3672]);          // 50 % of 7345, before the zone
        a.zone = 99;
        Instrument in2 = make();
        CHECK(!waveCommand(in2, 0, 1, WaveCmd::EnsembleXFade, a, sel, err) && err == "INCREMENT LOOPSTART");
        CHECK(waveCommand(in2, 0, 1, WaveCmd::ReverseXFade, a, sel, err));
        a.zone = 50;
        Instrument in3 = make();
        a.zone = 10;
        CHECK(waveCommand(in3, 0, 1, WaveCmd::BowtieXFade, a, sel, err));
        a.zone = 50;
        CHECK(std::abs(in3.waveSamples[1].pcm[12344] - in3.waveSamples[1].pcm[5000]) < 400);
    }
    {   // convert sample rate: same pitch, fewer samples
        Instrument in = make();
        a.rate = 4;                                   // 14.9 kHz
        CHECK(waveCommand(in, 0, 1, WaveCmd::ConvertRate, a, sel, err));
        const WaveSample &w = in.waveSamples[1];
        const int to = sampleRateOfIndex(42);
        CHECK(w.sampleRateIndex == 42 && std::labs(long(w.sampleEnd) - to / 2) <= 2);
        CHECK(std::fabs(freqOf(w.pcm, 100, long(w.sampleEnd) - 100, to) - 440.0) < 0.5);
        CHECK(std::labs(long(w.loopStart) - 5000L * to / sr) <= 1);
        CHECK(reparse(in));
        a.rate = 1;
    }
    {   // amp and data commands
        Instrument in = make();
        auto &d = in.waveSamples[1].pcm;
        CHECK(waveCommand(in, 0, 1, WaveCmd::Normalize, a, sel, err));
        int pk = 0;
        for (int16_t v : d) pk = std::max(pk, std::abs(int(v)));
        CHECK(pk >= 32766);
        a.range = WaveRange::StartToLoop;
        CHECK(waveCommand(in, 0, 1, WaveCmd::FadeIn, a, sel, err) && d[0] == 0);
        a.range = WaveRange::Loop;
        const std::vector<int16_t> before = d;
        CHECK(waveCommand(in, 0, 1, WaveCmd::Reverse, a, sel, err) && d[5000] == before[12344]);
        CHECK(waveCommand(in, 0, 1, WaveCmd::Reverse, a, sel, err) && d == before);
        CHECK(waveCommand(in, 0, 1, WaveCmd::Invert, a, sel, err) && d[6000] == -before[6000]);
        a.scaleStart = a.scaleEnd = 50;
        CHECK(waveCommand(in, 0, 1, WaveCmd::Scale, a, sel, err) && std::abs(d[6000] + before[6000] / 2) <= 1);
        CHECK(waveCommand(in, 0, 1, WaveCmd::Smooth, a, sel, err));
        CHECK(waveCommand(in, 0, 1, WaveCmd::Clear, a, sel, err) && d[6000] == 0 && d[4999] == before[4999]);
        a.range = WaveRange::Sample;
    }
    {   // new, copy, delete
        Instrument in = make();
        CHECK(waveCommand(in, 0, 1, WaveCmd::NewWs, a, sel, err) && sel == 2);
        CHECK(in.layers[0].waveSampleForNote(60) == 2 && in.layers[0].waveSampleForNote(65) == 1);
        a.toLayer = 0;
        CHECK(waveCommand(in, 0, 1, WaveCmd::CopyWs, a, sel, err) && sel == 3);
        CHECK(in.waveSamples[3].copyOfWaveSample == 1 && in.waveSamples[3].pcm.empty());
        CHECK(in.layers[0].waveSampleForNote(70) == 3);
        CHECK(!waveCommand(in, 0, 1, WaveCmd::DeleteWs, a, sel, err) && err == "WS HAS COPIES");
        CHECK(!waveCommand(in, 0, 1, WaveCmd::Truncate, a, sel, err) && err == "WS HAS COPIES");
        a.withData = true;
        CHECK(waveCommand(in, 0, 3, WaveCmd::CopyWs, a, sel, err) && sel == 4 && in.waveSamples[4].pcm == in.waveSamples[1].pcm);
        CHECK(waveCommand(in, 0, 3, WaveCmd::DeleteWs, a, sel, err) && !in.waveSamples.count(3));
        CHECK(reparse(in));
    }
    std::printf("test_wave: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
