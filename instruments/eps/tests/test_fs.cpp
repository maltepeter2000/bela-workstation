// test_fs: write into a copy of the fixture image (test.img) with eps_fs and read it
// back with the loader: a directory, a file in it, a replaced file, the checks of
// Volume::open, the journal restoring the image byte for byte.
#include "../pd/engine/eps_fs.h"
#include "../pd/engine/eps_loader.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static std::vector<uint8_t> slurp(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

int main(int argc, char **argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    const std::string img = dir + "/fs_test.img", journal = img + ".undo";
    const std::vector<uint8_t> orig = slurp(dir + "/test.img");
    { std::ofstream o(img, std::ios::binary); o.write(reinterpret_cast<const char *>(orig.data()), std::streamsize(orig.size())); }
    std::remove(journal.c_str());
    CHECK(eps::ensoniqName("pad bright!_x") == "PAD BRIGHT-");
    CHECK(eps::ensoniqName("a very long name") == "A VERY LONG");
    CHECK(eps::ensoniqName("a_b") == "A B");

    eps::Instrument in;
    std::vector<uint8_t> data;
    std::string err;
    {
        eps::Disk d;
        CHECK(d.open(img, err));
        auto l = d.instruments();
        CHECK(!l.empty() && d.loadInstrument(*l[0], in, err));
        in.waveSamples.begin()->second.filterCutoff[0] = 77;
        in.name = "EDITED";
        CHECK(eps::encodeInstrument(in, data, err));
    }
    eps::Volume v;
    CHECK(v.open(img, err));
    if (!v.isOpen()) { fprintf(stderr, "  open: %s\n", err.c_str()); return 1; }
    v.setJournal(journal);
    const uint32_t free0 = v.freeBlocks();
    int slot = -1, dslot = -1;
    bool exists = false;
    CHECK(!v.makeDir(3, "sounds", dslot, err));   // there already
    CHECK(v.makeDir(3, "new dir", dslot, err));
    uint32_t sub = 0;
    char p[8];
    snprintf(p, sizeof p, "%02d", dslot);
    CHECK(v.dirOfPath(p, false, sub, err));
    CHECK(v.writeFile(sub, "edited", eps::kInstrument, data, -1, slot, exists, err));
    CHECK(slot == 1);
    CHECK(!v.writeFile(sub, "EDITED", eps::kInstrument, data, -1, slot, exists, err) && exists);
    CHECK(v.writeFile(sub, "EDITED", eps::kInstrument, data, 1, slot, exists, err));   // replace
    const uint32_t n = uint32_t((data.size() + 511) / 512);
    CHECK(v.freeBlocks() == free0 - 2 - n);
    CHECK(!v.deleteFile(3, dslot, err));                // the folder is not empty
    {   // a second file in the folder, deleted again: the blocks come back
        int s2 = -1;
        CHECK(v.writeFile(sub, "second", eps::kInstrument, data, -1, s2, exists, err));
        CHECK(v.deleteFile(sub, s2, err) && v.freeBlocks() == free0 - 2 - n);
        CHECK(!v.deleteFile(sub, s2, err));
    }
    v.close();
    {
        eps::Volume again;   // its consistency checks pass on what was written
        CHECK(again.open(img, err));
        std::vector<eps::FsEntry> l;
        CHECK(again.list(3, l, err));
        bool dirSeen = false;
        for (const auto &e : l) dirSeen = dirSeen || (e.type == eps::kSubdir && e.name == "NEW DIR" && e.size == 1);
        CHECK(dirSeen);
    }
    {
        eps::Disk d;
        CHECK(d.open(img, err));
        bool found = false;
        for (const eps::DirEntry *e : d.instruments()) {
            if (e->name != "EDITED") continue;
            eps::Instrument back;
            CHECK(d.loadInstrument(*e, back, err));
            CHECK(back.waveSamples.begin()->second.filterCutoff[0] == 77);
            found = true;
        }
        CHECK(found);
    }
    {   // a folder without its DR marker is left out, the rest of the disk stays
        std::fstream f(img, std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(std::streamoff(sub) * 512 + 1022);
        f.write("XX", 2);
        f.close();
        eps::Disk d;
        CHECK(d.open(img, err));
        CHECK(d.warnings().size() == 1 && d.warnings()[0].find("NEW DIR") == 0);
        bool edited = false;
        for (const eps::DirEntry *e : d.instruments()) edited = edited || e->name == "EDITED";
        CHECK(!edited && !d.instruments().empty());
        std::fstream g(img, std::ios::binary | std::ios::in | std::ios::out);
        g.seekp(std::streamoff(sub) * 512 + 1022);
        g.write("DR", 2);
    }
    CHECK(eps::undoJournal(img, journal, err));
    CHECK(slurp(img) == orig);
    std::remove(img.c_str());
    printf("fs: %s\n", failures ? "FAILED" : "all checks passed");
    return failures ? 1 : 0;
}
