// eps_fs.cpp — see eps_fs.h.
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64   // hard disk images over 2 GB on 32-bit systems (Bela)
#endif
#include "eps_fs.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <sys/types.h>

namespace eps {

namespace {

constexpr uint32_t kBlk = 512;
constexpr uint32_t kEntry = 26;
constexpr int kEntries = 39;
constexpr int kMaxFiles = 38;
constexpr int kSubdir = 2, kParent = 8;

uint32_t be16(const uint8_t *p) { return uint32_t(p[0]) << 8 | p[1]; }
uint32_t be24(const uint8_t *p) { return uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2]; }
uint32_t be32(const uint8_t *p) { return be16(p) << 16 | be16(p + 2); }
void put16(uint8_t *p, uint32_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }
void put24(uint8_t *p, uint32_t v) { p[0] = uint8_t(v >> 16); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v); }
void put32(uint8_t *p, uint32_t v) { put16(p, v >> 16); put16(p + 2, v); }

std::string trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
    return s;
}

void setName(uint8_t *e, const std::string &name) {
    const std::string n = ensoniqName(name);
    for (int i = 0; i < 12; ++i) e[2 + i] = uint8_t(i < int(n.size()) ? n[size_t(i)] : ' ');
}

} // namespace

std::string ensoniqName(const std::string &s) {
    std::string r;
    for (char c : s) {
        if (r.size() == 12) break;
        unsigned char u = (unsigned char)c;
        if (u >= 'a' && u <= 'z') u = uint8_t(u - 32);
        if (u == '_') u = ' ';
        const bool ok = (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == ' ' || u == '*' ||
                        u == '-' || u == '+';
        r += ok ? char(u) : '-';
    }
    return trim(r);
}

Volume::~Volume() { close(); }

void Volume::close() {
    if (f_) fclose(f_);
    f_ = nullptr;
    saved_.clear();
    fat_.clear();
    parentOf_.clear();
}

bool Volume::readBlock(uint32_t b, uint8_t *p) {
    return f_ && fseeko(f_, off_t(b) * off_t(kBlk), SEEK_SET) == 0 && fread(p, 1, kBlk, f_) == kBlk;
}
bool Volume::journalBlock(uint32_t b) {
    if (journal_.empty()) return true;
    if (saved_.size() != total_) saved_.assign(total_, false);
    if (b >= total_ || saved_[b]) return true;
    uint8_t old[kBlk];
    if (!readBlock(b, old)) return false;
    FILE *j = fopen(journal_.c_str(), "ab");
    if (!j) return false;
    uint8_t num[4];
    put32(num, b);
    const bool ok = fwrite(num, 1, 4, j) == 4 && fwrite(old, 1, kBlk, j) == kBlk && fflush(j) == 0;
    fclose(j);
    if (ok) saved_[b] = true;
    return ok;
}

bool Volume::writeBlock(uint32_t b, const uint8_t *p) {
    if (!journalBlock(b)) return false;
    return f_ && fseeko(f_, off_t(b) * off_t(kBlk), SEEK_SET) == 0 && fwrite(p, 1, kBlk, f_) == kBlk;
}

bool Volume::readDir(uint32_t first, std::vector<uint8_t> &d, std::string &err) {
    d.assign(2 * kBlk, 0);
    const uint32_t second = dirSecond(first);
    if (first < 3 || second >= total_ || !readBlock(first, d.data()) || !readBlock(second, d.data() + kBlk)) {
        err = "cannot read the directory at block " + std::to_string(first);
        return false;
    }
    if (d[1022] != 'D' || d[1023] != 'R') { err = "no directory at block " + std::to_string(first); return false; }
    return true;
}
bool Volume::writeDir(uint32_t first, const std::vector<uint8_t> &d) {
    return writeBlock(first, d.data()) && writeBlock(dirSecond(first), d.data() + kBlk);
}

bool Volume::open(const std::string &path, std::string &err) {
    close();
    f_ = fopen(path.c_str(), "r+b");
    if (!f_) { err = "cannot open " + path + " for writing"; return false; }
    uint8_t b[kBlk];
    if (!readBlock(1, b) || b[38] != 'I' || b[39] != 'D') { err = "not an Ensoniq disk image"; close(); return false; }
    if (be32(b + 10) != kBlk) { err = "block size is not 512"; close(); return false; }
    total_ = be32(b + 14);
    fseeko(f_, 0, SEEK_END);
    const off_t bytes = ftello(f_);
    if (total_ < 16 || bytes < 0 || uint64_t(total_) * kBlk > uint64_t(bytes)) {
        err = "image smaller than its device ID block says";
        close();
        return false;
    }
    if (!readBlock(2, b) || b[28] != 'O' || b[29] != 'S') { err = "missing OS block"; close(); return false; }
    free_ = be32(b);
    fatBlocks_ = (total_ + 169) / 170;
    fat_.assign(total_, 0);
    uint32_t freeCount = 0;
    for (uint32_t k = 0; k < fatBlocks_; ++k) {
        if (!readBlock(5 + k, b)) { err = "cannot read the FAT"; close(); return false; }
        for (uint32_t i = 0; i < 170; ++i) {
            const uint32_t blk = k * 170 + i;
            if (blk >= total_) break;
            fat_[blk] = be24(b + 3 * i);
            freeCount += fat_[blk] == 0;
        }
    }
    if (freeCount != free_) {
        err = "free block count (" + std::to_string(free_) + ") differs from the FAT (" + std::to_string(freeCount) +
              "): not writing to this image";
        close();
        return false;
    }
    if (!checkDir(3, 0, 0, err)) { close(); return false; }
    return true;
}

// Every subdirectory entry's size must be the number of files in it, and its
// first entry the parent pointer: the layout this writer keeps up.
bool Volume::checkDir(uint32_t first, uint32_t parent, int depth, std::string &err) {
    if (depth > 32) { err = "directories nested too deep"; return false; }
    parentOf_[first] = parent;
    std::vector<uint8_t> d;
    if (!readDir(first, d, err)) return false;
    for (int i = 0; i < kEntries; ++i) {
        const uint8_t *e = d.data() + i * kEntry;
        if (e[1] != kSubdir) continue;
        const uint32_t sub = be32(e + 18);
        std::vector<uint8_t> s;
        if (!readDir(sub, s, err)) return false;
        int files = 0;
        for (int k = 0; k < kEntries; ++k) files += s[size_t(k) * kEntry + 1] != 0 && s[size_t(k) * kEntry + 1] != kParent;
        if (s[1] != kParent || be32(s.data() + 18) != first || int(be16(e + 14)) != files) {
            err = "directory '" + trim(std::string(reinterpret_cast<const char *>(e + 2), 12)) +
                  "' is laid out differently than expected: not writing to this image";
            return false;
        }
        if (!checkDir(sub, first, depth + 1, err)) return false;
    }
    return true;
}

bool Volume::dirOfPath(const std::string &path, bool fileInPath, uint32_t &dirBlock, std::string &err) {
    std::vector<int> slots;
    for (size_t p = 0; p < path.size();) {
        while (p < path.size() && path[p] == ' ') ++p;
        if (p >= path.size()) break;
        slots.push_back(std::atoi(path.c_str() + p));
        while (p < path.size() && path[p] != ' ') ++p;
    }
    if (fileInPath && !slots.empty()) slots.pop_back();
    dirBlock = 3;
    for (int s : slots) {
        std::vector<uint8_t> d;
        if (s < 0 || s >= kEntries || !readDir(dirBlock, d, err)) { if (err.empty()) err = "bad path"; return false; }
        const uint8_t *e = d.data() + s * kEntry;
        if (e[1] != kSubdir) { err = "path " + path + " is not a directory"; return false; }
        dirBlock = be32(e + 18);
    }
    return true;
}

bool Volume::list(uint32_t dirBlock, std::vector<FsEntry> &out, std::string &err) {
    std::vector<uint8_t> d;
    if (!readDir(dirBlock, d, err)) return false;
    out.clear();
    for (int i = 0; i < kEntries; ++i) {
        const uint8_t *e = d.data() + i * kEntry;
        if (!e[1]) continue;
        FsEntry fe;
        fe.slot = i;
        fe.type = e[1];
        fe.name = trim(std::string(reinterpret_cast<const char *>(e + 2), 12));
        fe.size = be16(e + 14);
        fe.contiguous = be16(e + 16);
        fe.first = be32(e + 18);
        out.push_back(fe);
    }
    return true;
}

// First fit: one contiguous run if there is one, else the free blocks in order.
bool Volume::allocate(uint32_t n, std::vector<uint32_t> &blocks) {
    blocks.clear();
    if (n == 0 || n > free_) return false;
    const uint32_t start = 5 + fatBlocks_;
    uint32_t run = 0;
    for (uint32_t b = start; b < total_; ++b) {
        run = fat_[b] == 0 ? run + 1 : 0;
        if (run == n) {
            for (uint32_t k = b + 1 - n; k <= b; ++k) blocks.push_back(k);
            return true;
        }
    }
    for (uint32_t b = start; b < total_ && blocks.size() < n; ++b)
        if (fat_[b] == 0) blocks.push_back(b);
    return blocks.size() == n;
}

bool Volume::flushFat(const std::vector<uint32_t> &blocks) {
    std::vector<uint32_t> fb;
    for (uint32_t b : blocks) fb.push_back(5 + b / 170);
    std::sort(fb.begin(), fb.end());
    fb.erase(std::unique(fb.begin(), fb.end()), fb.end());
    for (uint32_t k : fb) {
        uint8_t b[kBlk];
        if (!readBlock(k, b)) return false;
        for (uint32_t i = 0; i < 170; ++i) {
            const uint32_t blk = (k - 5) * 170 + i;
            if (blk < total_) put24(b + 3 * i, fat_[blk]);
        }
        if (!writeBlock(k, b)) return false;
    }
    return true;
}

bool Volume::flushFree() {
    uint8_t b[kBlk];
    if (!readBlock(2, b)) return false;
    put32(b, free_);
    return writeBlock(2, b);
}

void Volume::freeChain(uint32_t first) {
    std::vector<uint32_t> chain;
    for (uint32_t b = first, guard = 0; b >= 5 + fatBlocks_ && b < total_ && guard < total_; ++guard) {
        const uint32_t next = fat_[b];
        if (next == 0) break;
        chain.push_back(b);
        fat_[b] = 0;
        ++free_;
        if (next == 1) break;
        b = next;
    }
    flushFat(chain);
}

bool Volume::adjustDirCount(uint32_t dirBlock, int delta, std::string &err) {
    if (dirBlock == 3) return true;
    const auto it = parentOf_.find(dirBlock);
    if (it == parentOf_.end()) { err = "unknown directory"; return false; }
    std::vector<uint8_t> p;
    if (!readDir(it->second, p, err)) return false;
    for (int i = 0; i < kEntries; ++i) {
        uint8_t *e = p.data() + i * kEntry;
        if (e[1] == kSubdir && be32(e + 18) == dirBlock) {
            put16(e + 14, uint32_t(std::max(0, int(be16(e + 14)) + delta)));
            return writeDir(it->second, p) || (err = "write failed", false);
        }
    }
    err = "directory entry not found in its parent";
    return false;
}

bool Volume::writeFile(uint32_t dirBlock, const std::string &name, int type, const std::vector<uint8_t> &data,
                       int replaceSlot, int &slot, bool &exists, std::string &err) {
    exists = false;
    if (!f_) { err = "no image open"; return false; }
    std::vector<uint8_t> d;
    if (!readDir(dirBlock, d, err)) return false;
    const std::string nm = ensoniqName(name);
    if (nm.empty()) { err = "empty name"; return false; }
    int files = 0, freeSlot = -1;
    for (int i = 0; i < kEntries; ++i) {
        const uint8_t *e = d.data() + i * kEntry;
        if (!e[1]) { if (freeSlot < 0) freeSlot = i; continue; }
        if (e[1] != kParent) ++files;
        if (i != replaceSlot && e[1] != kParent && trim(std::string(reinterpret_cast<const char *>(e + 2), 12)) == nm &&
            e[1] == type)
            exists = true;
    }
    if (replaceSlot < 0 && exists) { err = "a file named " + nm + " is already there"; return false; }
    if (replaceSlot >= 0) {
        const uint8_t *e = d.data() + replaceSlot * kEntry;
        if (replaceSlot >= kEntries || e[1] == 0 || e[1] == kParent || e[1] == kSubdir) {
            err = "nothing to replace in slot " + std::to_string(replaceSlot);
            return false;
        }
    } else if (freeSlot < 0 || files >= kMaxFiles) {
        err = "the directory is full (38 files)";
        return false;
    }
    const uint32_t n = uint32_t((data.size() + kBlk - 1) / kBlk);
    // Blocks the replaced file frees count as free for the new one.
    uint32_t reclaim = 0;
    if (replaceSlot >= 0) reclaim = be16(d.data() + replaceSlot * kEntry + 14);
    if (n == 0 || n > free_ + reclaim) { err = "not enough free blocks on the disk"; return false; }
    if (replaceSlot >= 0) freeChain(be32(d.data() + replaceSlot * kEntry + 18));
    std::vector<uint32_t> blocks;
    if (!allocate(n, blocks)) { err = "not enough free blocks on the disk"; return false; }
    for (uint32_t i = 0; i < n; ++i) {
        uint8_t b[kBlk] = {};
        const size_t off = size_t(i) * kBlk;
        std::memcpy(b, data.data() + off, std::min<size_t>(kBlk, data.size() - off));
        if (!writeBlock(blocks[i], b)) { err = "write failed"; return false; }
        fat_[blocks[i]] = i + 1 < n ? blocks[i + 1] : 1;
    }
    free_ -= n;
    if (!flushFat(blocks) || !flushFree()) { err = "write failed"; return false; }
    uint32_t contig = 1;
    while (contig < n && blocks[contig] == blocks[contig - 1] + 1) ++contig;
    slot = replaceSlot >= 0 ? replaceSlot : freeSlot;
    uint8_t *e = d.data() + slot * kEntry;
    std::memset(e, 0, kEntry);
    e[1] = uint8_t(type);
    setName(e, nm);
    put16(e + 14, n);
    put16(e + 16, contig);
    put32(e + 18, blocks[0]);
    if (!writeDir(dirBlock, d)) { err = "write failed"; return false; }
    if (replaceSlot < 0 && !adjustDirCount(dirBlock, +1, err)) return false;
    fflush(f_);
    return true;
}

bool Volume::makeDir(uint32_t dirBlock, const std::string &name, int &slot, std::string &err) {
    std::vector<uint8_t> d;
    if (!readDir(dirBlock, d, err)) return false;
    std::string parentName = "ROOT";
    if (dirBlock != 3) {
        // the parent's name is in its parent's entry
        std::vector<uint8_t> pp;
        const auto it = parentOf_.find(dirBlock);
        if (it != parentOf_.end() && readDir(it->second, pp, err))
            for (int i = 0; i < kEntries; ++i)
                if (pp[size_t(i) * kEntry + 1] == kSubdir && be32(pp.data() + i * kEntry + 18) == dirBlock)
                    parentName = trim(std::string(reinterpret_cast<const char *>(pp.data() + i * kEntry + 2), 12));
    }
    std::vector<uint8_t> nd(2 * kBlk, 0);
    nd[1] = kParent;
    setName(nd.data(), parentName);
    put16(nd.data() + 16, 2);
    put32(nd.data() + 18, dirBlock);
    nd[1022] = 'D';
    nd[1023] = 'R';
    bool exists = false;
    if (!writeFile(dirBlock, name, kSubdir, nd, -1, slot, exists, err)) return false;
    // A directory entry: size = files in it (0), contiguous 2.
    if (!readDir(dirBlock, d, err)) return false;
    uint8_t *e = d.data() + slot * kEntry;
    const uint32_t first = be32(e + 18);
    if (fat_[first] != first + 1) { err = "directory blocks not contiguous"; return false; }
    put16(e + 14, 0);
    put16(e + 16, 2);
    parentOf_[first] = dirBlock;
    if (!writeDir(dirBlock, d)) { err = "write failed"; return false; }
    fflush(f_);
    return true;
}

bool Volume::deleteFile(uint32_t dirBlock, int slot, std::string &err) {
    if (!f_) { err = "no image open"; return false; }
    std::vector<uint8_t> d;
    if (!readDir(dirBlock, d, err)) return false;
    if (slot < 0 || slot >= kEntries) { err = "no such slot"; return false; }
    uint8_t *e = d.data() + slot * kEntry;
    if (!e[1] || e[1] == kParent) { err = "nothing to delete in slot " + std::to_string(slot); return false; }
    const uint32_t first = be32(e + 18);
    if (e[1] == kSubdir) {
        std::vector<uint8_t> sd;
        if (!readDir(first, sd, err)) return false;
        for (int i = 0; i < kEntries; ++i)
            if (sd[size_t(i) * kEntry + 1] && sd[size_t(i) * kEntry + 1] != kParent) { err = "the folder is not empty"; return false; }
    }
    freeChain(first);
    if (!flushFree()) { err = "write failed"; return false; }
    std::memset(e, 0, kEntry);
    if (!writeDir(dirBlock, d)) { err = "write failed"; return false; }
    if (!adjustDirCount(dirBlock, -1, err)) return false;
    parentOf_.erase(first);
    fflush(f_);
    return true;
}

bool undoJournal(const std::string &image, const std::string &journal, std::string &err) {
    FILE *j = fopen(journal.c_str(), "rb");
    if (!j) { err = "no journal " + journal; return false; }
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> recs;
    uint8_t num[4];
    while (fread(num, 1, 4, j) == 4) {
        std::vector<uint8_t> b(kBlk);
        if (fread(b.data(), 1, kBlk, j) != kBlk) break;
        recs.emplace_back(be32(num), std::move(b));
    }
    fclose(j);
    FILE *f = fopen(image.c_str(), "r+b");
    if (!f) { err = "cannot open " + image; return false; }
    bool ok = true;
    for (auto it = recs.rbegin(); it != recs.rend(); ++it)
        ok = ok && fseeko(f, off_t(it->first) * off_t(kBlk), SEEK_SET) == 0 &&
             fwrite(it->second.data(), 1, kBlk, f) == kBlk;
    fclose(f);
    if (!ok) { err = "write error"; return false; }
    std::remove(journal.c_str());
    return true;
}

bool writeEfe(const std::string &path, const std::string &name, int type, const std::vector<uint8_t> &data,
              std::string &err) {
    uint8_t h[kBlk] = {};
    const std::string nm = ensoniqName(name);
    std::string text = "\r\nEps File:       ";
    text += nm + std::string(12 - nm.size(), ' ');
    text += "    ";
    text += type == 3 ? "Instrument   " : "File         ";
    text += "\r\n\x1a";
    std::memcpy(h, text.data(), text.size());   // 0x32 bytes
    const uint32_t n = uint32_t((data.size() + kBlk - 1) / kBlk);
    h[0x32] = uint8_t(type);
    put16(h + 0x34, n);
    put16(h + 0x36, n);
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) { err = "cannot write " + path; return false; }
    std::vector<uint8_t> body(size_t(n) * kBlk, 0);
    std::memcpy(body.data(), data.data(), data.size());
    const bool ok = fwrite(h, 1, kBlk, f) == kBlk && fwrite(body.data(), 1, body.size(), f) == body.size();
    fclose(f);
    if (!ok) err = "cannot write " + path;
    return ok;
}

} // namespace eps
