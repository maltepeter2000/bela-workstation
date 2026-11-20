// The user bank of tx81zn~ and tx81z~: 64 voices in txuser-bank.syx beside the patch, stored as
// two TX81Z voice bulk dumps (VMEM, 32 voices each), so the file loads into a real TX81Z (or any
// librarian) and dumps from one load here. Slots without a voice in the file hold the Init Voice.
// Also the Init Voice itself and the slot labels (11 ... 88) shown on the display.
#pragma once

#include "m_pd.h"
#include "txvoice.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace txbank {

constexpr int SLOTS = 64;
constexpr const char *FILE_NAME = "txuser-bank.syx";

// the TX81Z's "Init Voice" (owner's manual, Init Voice? in SINGLE: Edit): algorithm 1, only OP1
// sounding (out 90), all operators sine at ratio 1.00, AR 31 D1R 31 D1L 15 D2R 0 RR 15, LFO
// triangle speed 35, transpose C3, bend range 4, full-time portamento 0, FC volume 40, MW pitch
// 50, name INIT VOICE.
inline void init_voice(uint8_t vc[93], uint8_t ac[23])
{
	std::memset(vc, 0, 93);
	std::memset(ac, 0, 23);
	for (int op = 0; op < 4; op++)        // VCED order: OP4, OP3, OP2, OP1
	{
		uint8_t *v = vc + op * 13;
		v[0] = 31; v[1] = 31; v[2] = 0; v[3] = 15; v[4] = 15;   // AR D1R D2R RR D1L
		v[10] = op == 3 ? 90 : 0;                              // OUT: OP1 only
		v[11] = 4;                                             // CRS: ratio 1.00
		v[12] = 3;                                             // DET: 0
	}
	vc[52] = 0; vc[53] = 0;                 // algorithm 1, feedback 0
	vc[54] = 35; vc[59] = 2;                // LFO speed 35, triangle
	vc[62] = 24;                            // transpose C3
	vc[64] = 4;                             // pitch bend range
	vc[67] = 40;                            // FC volume
	vc[68] = 1;                             // sustain foot switch on
	vc[71] = 50;                            // MW pitch
	vc[75] = 50;                            // BC pitch bias 0
	const char name[] = "INIT VOICE";
	for (int k = 0; k < 10; k++) vc[77 + k] = uint8_t(name[k]);
	const uint8_t peg[6] = { 99, 99, 99, 50, 50, 50 };   // DX21 pitch EG, unused
	for (int k = 0; k < 6; k++) vc[87 + k] = peg[k];
}

inline std::string slot_label(int slot)   // user slot 0-63 -> "11" ... "88"
{
	return std::to_string((slot / 8) * 10 + slot % 8 + 11);
}

// all slots the Init Voice (records as in txvoice.h)
inline void clear(std::vector<uint8_t> &bank)
{
	uint8_t vc[93], ac[23];
	init_voice(vc, ac);
	bank.assign(SLOTS * txn::VOICE_RECORD, 0);
	for (int i = 0; i < SLOTS; i++) txn::pack_voice(vc, ac, bank.data() + i * txn::VOICE_RECORD);
}

// reads the VMEM dumps of a .syx file into the bank (slot 0 on); the rest stays the Init Voice
inline bool read(void *owner, const char *who, const std::string &path, std::vector<uint8_t> &bank, bool quiet)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) { if (!quiet) pd_error(owner, "%s: cannot read %s", who, path.c_str()); return false; }
	std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	std::vector<uint8_t> recs(SLOTS * txn::VOICE_RECORD);
	int n = txn::vmem_read(data.data(), data.size(), recs.data(), SLOTS);
	if (n < 0) { pd_error(owner, "%s: %s: checksum error", who, path.c_str()); return false; }
	if (n == 0) { pd_error(owner, "%s: %s: no TX81Z voice bulk dump (VMEM)", who, path.c_str()); return false; }
	clear(bank);
	std::memcpy(bank.data(), recs.data(), size_t(n) * txn::VOICE_RECORD);
	post("%s: user bank %s (%d voices)", who, path.c_str(), n);
	return true;
}

// writes the bank as two VMEM dumps; channel 0 (omni) / 1-16 goes into the dump headers
inline bool write(void *owner, const char *who, const std::string &path, const std::vector<uint8_t> &bank, int channel)
{
	std::vector<uint8_t> out(2 * txn::VMEM_DUMP);
	int ch = channel > 0 ? channel - 1 : 0;
	txn::vmem_dump(bank.data(), ch, out.data());
	txn::vmem_dump(bank.data() + 32 * txn::VOICE_RECORD, ch, out.data() + txn::VMEM_DUMP);
	std::ofstream f(path, std::ios::binary);
	if (!f || !f.write(reinterpret_cast<const char *>(out.data()), std::streamsize(out.size())))
	{
		pd_error(owner, "%s: cannot write %s", who, path.c_str());
		return false;
	}
	return true;
}

} // namespace txbank
