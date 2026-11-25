// Reads every file on an image through the directory + FAT and reports the
// byte count, to verify FAT-chain handling on real disks.
#include "../pd/engine/eps_loader.h"

#include <cstdio>

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    eps::Disk disk;
    std::string err;
    if (!disk.open(argv[1], err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
    int bad = 0;
    for (const eps::DirEntry &e : disk.files()) {
        if (e.type == eps::kSubdir || e.type == eps::kParentPtr) continue;
        std::vector<uint8_t> d;
        bool ok = disk.readFile(e, d, err);
        printf("%-8s %-12s %5u blocks  contiguous %5u  first %5u  -> %s %zu bytes\n", e.path.c_str(),
               e.name.c_str(), e.sizeBlocks, e.contiguousBlocks, e.firstBlock, ok ? "ok" : err.c_str(),
               d.size());
        if (!ok || d.size() != size_t(e.sizeBlocks) * 512) ++bad;
    }
    return bad ? 1 : 0;
}
