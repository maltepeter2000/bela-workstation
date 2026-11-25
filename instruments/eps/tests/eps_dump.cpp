// eps_dump — inspect Ensoniq EPS/EPS-16+/ASR images and export wavesamples.
//
//   eps_dump <image>                      list all files
//   eps_dump <image> <path|name> [outdir] dump one instrument; with outdir,
//                                         write one WAV per wavesample
#include "../pd/engine/eps_loader.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace eps;

static const char *noteName(int n) {
    static char buf[24];
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    snprintf(buf, sizeof buf, "%s%d", names[((n % 12) + 12) % 12], n / 12 - 1);
    return buf;
}

static const char *loopModeName(int m) {
    switch (m) {
        case 0: return "no loop";
        case 1: return "backward";
        case 2: return "loop";
        case 3: return "bidirectional";
        case 4: return "loop until release";
        default: return "other";
    }
}

static void printEnv(const char *label, const Envelope &e) {
    printf("    %-6s lvl(soft) %2d %2d %2d %2d %2d | lvl(hard) %2d %2d %2d %2d %2d | t %2d %2d %2d %2d %2d %2d | rel %d vsw %d mode %d\n",
           label, e.softLevel[0], e.softLevel[1], e.softLevel[2], e.softLevel[3], e.softLevel[4],
           e.hardLevel[0], e.hardLevel[1], e.hardLevel[2], e.hardLevel[3], e.hardLevel[4],
           e.time[1], e.time[2], e.time[3], e.time[4], e.time[5], e.time[6], e.level5,
           e.velocitySwitch, e.mode);
}

static void dumpInstrument(const Instrument &in, const char *outdir) {
    printf("Instrument \"%s\" (%s, id 0x%04X)\n", in.name.c_str(), in.modelName(), in.instrumentId);
    const std::string lo = noteName(in.keyLow), hi = noteName(in.keyHigh);
    printf("  keys %s..%s  transpose %d  patches %02X %02X %02X %02X  down %02X  up %02X\n",
           lo.c_str(), hi.c_str(), in.transpose,
           in.patches[0], in.patches[1], in.patches[2], in.patches[3], in.keyDownLayers, in.keyUpLayers);

    for (const Layer &l : in.layers) {
        printf("  Layer %d \"%s\" vel %d..%d stereoLink %d glide %d/%d\n", l.index, l.name.c_str(),
               l.velocityLow, l.velocityHigh, l.stereoLink, l.glideMode, l.glideTime);
        // Compress the 88-key map into ranges.
        int start = 0;
        for (int k = 1; k <= 88; ++k) {
            if (k == 88 || l.keyMap[k] != l.keyMap[start]) {
                if (l.keyMap[start]) {
                    std::string lo = noteName(start + Layer::kFirstNote);
                    printf("    %-4s..%-4s -> WS %d\n", lo.c_str(), noteName(k - 1 + Layer::kFirstNote),
                           l.keyMap[start]);
                }
                start = k;
            }
        }
    }

    for (const auto &kv : in.waveSamples) {
        const WaveSample &ws = kv.second;
        const WaveSample *owner = in.pcmOwner(ws);
        printf("  WS %d \"%s\" root %s  %d Hz  %zu samples%s\n", ws.index, ws.name.c_str(),
               noteName(ws.rootNote), ws.sampleRate, owner ? owner->pcm.size() : size_t(0),
               ws.copyOfWaveSample ? (" (copy of WS " + std::to_string(ws.copyOfWaveSample) + ")").c_str() : "");
        printf("    start %u end %u  loop %u..%u (%s, mode %d)  keys %d..%d\n", ws.sampleStart, ws.sampleEnd,
               ws.loopStart, ws.loopEnd, loopModeName(ws.loopMode), ws.loopMode, ws.keyLow, ws.keyHigh);
        printf("    volume %d  pan %d/%d  fine %d  bend %d  filter mode %d cutoff %d/%d keyamt %d/%d envamt %d/%d\n",
               ws.volume, ws.panEps, ws.panSigned, ws.fineTune, ws.pitchBendRange, ws.filterMode,
               ws.filterCutoff[0], ws.filterCutoff[1], ws.filterKeyAmount[0], ws.filterKeyAmount[1],
               ws.filterEnvAmount[0], ws.filterEnvAmount[1]);
        if (ws.waveModAmount() != 0)
            printf("    wave mod: type %d%s source %d%s amount %+d range %u samples (raw %d %d %d %d %d)\n",
                   ws.waveModType(), ws.waveModType() == 6 ? " (transwave)" : "", ws.waveModSource(),
                   ws.waveModSource() == 10 ? " (wheel)" : "", ws.waveModAmountDisplay(), ws.waveModRangeSamples(),
                   ws.waveModRaw[0], ws.waveModRaw[1], ws.waveModRaw[2], ws.waveModRaw[3], ws.waveModRaw[4]);
        printf("    lfo wave %d speed %d depth %d delay %d mode %d -> pitch %d\n", ws.lfoWaveform,
               ws.lfoSpeed, ws.lfoDepth, ws.lfoDelay, ws.lfoMode, ws.lfoPitchAmount);
        printEnv("pitch", ws.pitchEnv);
        printEnv("filter", ws.filterEnv);
        printEnv("amp", ws.ampEnv);

        if (outdir && owner && ws.copyOfWaveSample == 0 && !owner->pcm.empty()) {
            char path[1024];
            snprintf(path, sizeof path, "%s/ws%03d.wav", outdir, ws.index);
            std::string err;
            if (!writeWav(path, ws, owner->pcm, err)) fprintf(stderr, "%s\n", err.c_str());
            else printf("    -> %s\n", path);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <image> [<file path|name> [outdir]]\n", argv[0]);
        return 2;
    }
    Disk disk;
    std::string err;
    if (!disk.open(argv[1], err)) {
        fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    if (argc == 2) {
        printf("%s  label \"%s\"  OS %s\n", imageFormatName(disk.format()), disk.label().c_str(),
               disk.osVersion().empty() ? "-" : disk.osVersion().c_str());
        for (const DirEntry &e : disk.files())
            printf("  %-12s %-10s %-12s %5u blocks\n", e.path.c_str(), fileTypeName(e.type), e.name.c_str(),
                   e.sizeBlocks);
        return 0;
    }
    const DirEntry *target = nullptr;
    for (const DirEntry &e : disk.files())
        if (e.path == argv[2] || e.name == argv[2]) { target = &e; break; }
    if (!target) {
        fprintf(stderr, "error: no file '%s'\n", argv[2]);
        return 1;
    }
    Instrument in;
    if (!disk.loadInstrument(*target, in, err)) {
        fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    dumpInstrument(in, argc > 3 ? argv[3] : nullptr);
    return 0;
}
