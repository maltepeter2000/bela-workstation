// eps_fs_tool: write into Ensoniq disk images from the command line (tests and
// checks of eps_fs against the original OS).
//   eps_fs_tool mkdir <image> <dir path|-> <name>
//   eps_fs_tool put <image> <dir path|-> <instrument file (EFE or image)> [name] [cutoff]
//   eps_fs_tool ls <image> <dir path|->
//   eps_fs_tool check <image>
//   eps_fs_tool undo <image>       restore it from <image>.undo (eps~'s journal)
// <dir path> is a list of slot numbers like "03 07" (one argument), "-" = root.
#include "../pd/engine/eps_fs.h"
#include "../pd/engine/eps_loader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: see the source\n"); return 2; }
    const std::string cmd = argv[1], img = argv[2];
    if (cmd == "undo") {
        std::string e;
        if (!eps::undoJournal(img, img + ".undo", e)) { fprintf(stderr, "undo: %s\n", e.c_str()); return 1; }
        printf("restored %s\n", img.c_str());
        return 0;
    }
    const std::string dirPath = argc > 3 && std::strcmp(argv[3], "-") ? argv[3] : "";
    eps::Volume v;
    std::string err;
    if (!v.open(img, err)) { fprintf(stderr, "open: %s\n", err.c_str()); return 1; }
    if (cmd == "check") { printf("ok: %u blocks, %u free\n", v.totalBlocks(), v.freeBlocks()); return 0; }
    uint32_t dir = 3;
    if (!v.dirOfPath(dirPath, false, dir, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    int slot = -1;
    if (cmd == "mkdir" && argc > 4) {
        if (!v.makeDir(dir, argv[4], slot, err)) { fprintf(stderr, "mkdir: %s\n", err.c_str()); return 1; }
        printf("directory in slot %d\n", slot);
        return 0;
    }
    if (cmd == "ls") {
        std::vector<eps::FsEntry> l;
        if (!v.list(dir, l, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
        for (const auto &e : l) printf("%02d type %2d %-12s size %u contig %u first %u\n", e.slot, e.type, e.name.c_str(), e.size, e.contiguous, e.first);
        return 0;
    }
    if (cmd == "put" && argc > 4) {
        eps::Disk d;
        if (!d.open(argv[4], err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
        auto list = d.instruments();
        eps::Instrument in;
        if (list.empty() || !d.loadInstrument(*list[0], in, err)) { fprintf(stderr, "load: %s\n", err.c_str()); return 1; }
        if (argc > 5) in.name = eps::ensoniqName(argv[5]);
        if (argc > 6) for (auto &kv : in.waveSamples) kv.second.filterCutoff[0] = std::atoi(argv[6]);
        std::vector<uint8_t> data;
        if (!eps::encodeInstrument(in, data, err)) { fprintf(stderr, "encode: %s\n", err.c_str()); return 1; }
        bool exists = false;
        if (!v.writeFile(dir, in.name, 3, data, -1, slot, exists, err)) { fprintf(stderr, "put: %s\n", err.c_str()); return 1; }
        printf("%s in slot %d\n", in.name.c_str(), slot);
        return 0;
    }
    fprintf(stderr, "unknown command\n");
    return 2;
}
