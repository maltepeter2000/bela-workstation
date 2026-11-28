// Ensoniq EPS-16 PLUS bank files (type 23): the 8 instruments to load (as paths on the
// disk the bank is on), their volume, output, pan and effect control, the performance
// presets, and the bank effect.
//
// Layout after Thoralt Franz, "The Ensoniq EPS/EPS16+/ASR-10 Bank Format" (v0.7), checked
// against EPS-16 PLUS bank files: a 1536-byte header, then the bank effect as an effect block
// (its length in the first word pair, as in effect files). The header's words are read
// byte by byte here (the document counts them little-endian):
//   $000  length of the header (as an effect length: $6000 0000 = 1536)
//   $004  machine: A8 20 00 00 = EPS-16 PLUS
//   $008  name: 12 characters, one in every even byte
//   $020  tracks with data: one bit per track (bit 0 = track 1), then $FF
//   $022  9 file blocks of 16 bytes (tracks 1-8, the song): copy (bit 7, source in bits 0-3),
//         $FF; device (0 floppy, 1-8 SCSI 0-7), disk name char 1; then 6 pairs: directory
//         index (the slot in the directory; into a folder and on) and disk name char 2-7
//   $0B2  per track: volume ($7F = 99), output (0 WS, 1-3 BUS1-3, 4 AUX, 5 ROTATE)
//   $0C2  per track: pan ($80 = as the WS), effect control (0 off, 1 on)
//   $0D2  8 performance presets of 134 bytes (kept as they are)
//   $600  the bank effect

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eps {

struct BankTrack {
    bool valid = false;
    bool copy = false;          // a copy of another track's instrument
    int copyOf = 0;             // its track (0-7)
    int device = 0;             // 0 floppy, 1-8 SCSI 0-7
    std::string disk;           // the disk's name (7 characters at most)
    std::vector<int> path;      // directory indices: the slot in the root, then in that folder
                                // ... (as far as they lead to a folder; $FF ends)
    int volume = 0x7F;
    int output = 0;
    int pan = 0x80;
    int fxControl = 1;
};

struct Bank {
    std::string name;
    BankTrack track[8];
    std::vector<uint8_t> header;   // the 1536 bytes as read (kept: presets, the song block)
    std::vector<uint8_t> effect;   // the bank effect (empty: none)
};

constexpr size_t kBankHeader = 1536;

bool parseBank(const std::vector<uint8_t> &data, Bank &bank, std::string &err);
// The file data (header, then the effect); fields from the struct, the rest from bank.header
// (or as the OS writes a new bank).
std::vector<uint8_t> buildBank(const Bank &bank);

}  // namespace eps
