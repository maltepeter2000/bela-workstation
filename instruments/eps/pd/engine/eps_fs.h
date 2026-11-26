// eps_fs.h — writing files into Ensoniq disk images (EPS / EPS-16 PLUS / ASR file
// system), floppy images and SCSI hard disk images alike (ZuluSCSI / BlueSCSI).
//
// Layout (512-byte blocks), as written by the original EPS-16 PLUS OS 1.30
// (checked in the emulator: CREATE DIRECTORY, SAVE):
//   block 1      device ID block, total blocks at byte 14 (32 bit), 'ID' at 38
//   block 2      OS block, free blocks at byte 0 (32 bit)
//   blocks 3-4   root directory: 39 entries of 26 bytes, 'DR' in the last 2 bytes
//   blocks 5..   FAT, 170 entries of 3 bytes per block: 0 free, 1 end of file,
//                else the next block
//   entry        [1] type, [2..13] name, [14..15] size in blocks, [16..17] first
//                contiguous blocks, [18..21] first block
//   subdirectory entry: type 2, size = number of files in it, contiguous 2; its
//                first entry is type 8 (parent) with the parent's name and block.
// A directory holds 38 files (the manual).
#pragma once

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace eps {

// A name as the EPS can show and enter it: 12 characters of A-Z 0-9 space * - +
// (the characters the OS offers in its name fields). Lower case becomes upper
// case, '_' a space, everything else '-'.
std::string ensoniqName(const std::string &s);

struct FsEntry {
    int slot = 0;              // 0..38 within the directory
    int type = 0;
    std::string name;          // trimmed
    uint32_t size = 0, contiguous = 0, first = 0;
};

class Volume {
public:
    Volume() = default;
    ~Volume();
    Volume(const Volume &) = delete;
    Volume &operator=(const Volume &) = delete;

    // Open a raw image for writing. Checks that the image is consistent in the
    // ways this writer relies on (free count = free FAT entries, directory sizes =
    // file counts); otherwise it refuses, and nothing is written.
    bool open(const std::string &path, std::string &err);
    void close();
    // Before a block is changed for the first time, its old content is appended to
    // this file (block number, 512 bytes); undoJournal() restores it.
    void setJournal(const std::string &path) { journal_ = path; }
    bool isOpen() const { return f_ != nullptr; }
    uint32_t totalBlocks() const { return total_; }
    uint32_t freeBlocks() const { return free_; }

    // The directory at a path of slot numbers ("" = root, "03 07" as DirEntry::path
    // of the loader gives for a file; the last number is the file itself when
    // fileInPath is true).
    bool dirOfPath(const std::string &path, bool fileInPath, uint32_t &dirBlock, std::string &err);
    bool list(uint32_t dirBlock, std::vector<FsEntry> &out, std::string &err);

    // Write a file into a directory. replaceSlot >= 0 overwrites that entry (its
    // blocks are freed first), else the first free slot is used and a file of the
    // same name is refused (exists = true). Returns the slot.
    bool writeFile(uint32_t dirBlock, const std::string &name, int type, const std::vector<uint8_t> &data,
                   int replaceSlot, int &slot, bool &exists, std::string &err);
    bool makeDir(uint32_t dirBlock, const std::string &name, int &slot, std::string &err);

private:
    bool readBlock(uint32_t b, uint8_t *p);
    bool writeBlock(uint32_t b, const uint8_t *p);
    bool readDir(uint32_t first, std::vector<uint8_t> &d, std::string &err);
    bool writeDir(uint32_t first, const std::vector<uint8_t> &d);
    uint32_t fat(uint32_t b) const { return fat_[b]; }
    bool flushFat(const std::vector<uint32_t> &blocks);
    bool flushFree();
    bool allocate(uint32_t n, std::vector<uint32_t> &blocks);
    void freeChain(uint32_t first);
    bool checkDir(uint32_t first, uint32_t parent, int depth, std::string &err);
    bool adjustDirCount(uint32_t dirBlock, int delta, std::string &err);

    bool journalBlock(uint32_t b);
    FILE *f_ = nullptr;
    std::string journal_;
    std::vector<bool> saved_;
    uint32_t total_ = 0, free_ = 0, fatBlocks_ = 0;
    std::vector<uint32_t> fat_;
    std::map<uint32_t, uint32_t> parentOf_;   // directory first block -> parent's first block
};

// Restore the blocks a journal holds (newest first, so the oldest content wins)
// and delete the journal.
bool undoJournal(const std::string &image, const std::string &journal, std::string &err);

// An EFE file (Giebler's PC container): 512-byte header, then the file.
bool writeEfe(const std::string &path, const std::string &name, int type, const std::vector<uint8_t> &data,
              std::string &err);

} // namespace eps
