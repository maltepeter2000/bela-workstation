// sysex_live: drive eps::SysexLink against an EPS through two files: bytes to the
// EPS are appended to <out>, bytes from it are read from <in> as it grows (the
// emulator of the original OS writes its MIDI output there). For tests only.
//   sysex_live <to-eps file> <from-eps file> inst <n> <result.efe>
//   sysex_live <to-eps file> <from-eps file> ws <inst> <layer> <ws> <result.wav>
//   sysex_live <to-eps file> <from-eps file> send <inst> <file.efe>
//   sysex_live <to-eps file> <from-eps file> sendws <inst> <layer> <ws> <file.wav>
#include "../pd/engine/eps_fs.h"
#include "../pd/engine/eps_loader.h"
#include "../pd/engine/eps_sysex.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

static double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "usage: see the source\n"); return 2; }
    FILE *to = fopen(argv[1], "ab");
    FILE *from = fopen(argv[2], "rb");
    if (!to || !from) { fprintf(stderr, "cannot open the files\n"); return 1; }
    fseek(from, 0, SEEK_END);                    // only what comes from now on
    eps::SysexLink link([to](const std::vector<uint8_t> &p) {
        fwrite(p.data(), 1, p.size(), to);
        fflush(to);
    });
    link.debug_ = std::getenv("SX_DEBUG") != nullptr;
    const std::string what = argv[3];
    bool ok = false;
    if (what == "inst") ok = link.fetchInstrument(std::atoi(argv[4]), now());
    else if (what == "ws") ok = argc > 7 && link.fetchWaveSample(std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]), now());
    else if (what == "send") {
        FILE *f = fopen(argv[5], "rb");
        if (!f) return 1;
        std::vector<uint8_t> d;
        int ch;
        while ((ch = fgetc(f)) != EOF) d.push_back(uint8_t(ch));
        fclose(f);
        if (d.size() > 512 && d[0] == 0x0D && d[1] == 0x0A) d.erase(d.begin(), d.begin() + 512);   // EFE header
        ok = link.sendInstrument(std::atoi(argv[4]), d, now());
    } else if (what == "sendws" && argc > 7) {
        std::vector<int16_t> pcm;
        int rate = 0;
        std::string err;
        if (!eps::readWav(argv[7], pcm, rate, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
        ok = link.sendWaveSample(std::atoi(argv[4]), std::atoi(argv[5]), std::atoi(argv[6]), pcm, now());
    }
    if (!ok) return 2;
    std::string last;
    int lastPct = -1;
    for (;;) {
        int c;
        while ((c = fgetc(from)) != EOF) link.receive(uint8_t(c), now());
        clearerr(from);
        link.tick(now());
        if (link.percent != lastPct || link.step != last) {
            printf("%3d%% %s\n", link.percent, link.step.c_str());
            fflush(stdout);
            lastPct = link.percent; last = link.step;
        }
        const auto d = link.takeDone();
        if (d == eps::SysexLink::Done::Error) { fprintf(stderr, "error: %s\n", link.error.c_str()); return 1; }
        if (d == eps::SysexLink::Done::Instrument) {
            std::string err;
            if (!eps::writeEfe(argv[5], link.name, eps::kInstrument, link.file, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
            printf("instrument %s: %zu bytes -> %s\n", link.name.c_str(), link.file.size(), argv[5]);
            return 0;
        }
        if (d == eps::SysexLink::Done::Sent) { printf("sent\n"); return 0; }
        if (d == eps::SysexLink::Done::WaveSample) {
            eps::WaveSample ws;
            ws.sampleRate = link.sampleRate;
            ws.name = link.name;
            std::string err;
            if (!eps::writeWav(argv[7], ws, link.pcm, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
            printf("wavesample %s: %zu samples, %d Hz -> %s\n", link.name.c_str(), link.pcm.size(), link.sampleRate, argv[7]);
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
