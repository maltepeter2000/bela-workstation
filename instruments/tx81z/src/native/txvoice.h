// TX81Z voice records -> VCED / ACED parameter blocks.
//
// The ROM keeps the preset banks A-D and the battery RAM the internal bank as 78-byte records:
// the voice bulk format (VMEM) bytes 0..66 followed by its TX81Z additions, bytes 73..83 (the
// unused DX21 pitch EG bytes 67..72 are left out). Checked against the firmware's own VCED / ACED
// dumps of all 128 programs (tools/native/vmem.py).
#pragma once

#include <cstddef>
#include <cstdint>

namespace txn {

constexpr size_t VOICE_RECORD = 78;
constexpr size_t ROM_PRESETS = 0x51C0;     // ROM file offset: banks A, B, C, D (32 records each)
constexpr size_t NVRAM_INTERNAL = 0x0001;  // battery RAM offset: internal bank (32 records)

inline void unpack_voice(const uint8_t *rec, uint8_t vced[93], uint8_t aced[23])
{
	for (int oi = 0; oi < 4; oi++)   // operator blocks in VCED order
	{
		const uint8_t *o = rec + oi * 10;
		uint8_t *v = vced + oi * 13;
		for (int k = 0; k < 6; k++) v[k] = o[k];          // AR D1R D2R RR D1L LS
		v[8] = (o[6] >> 6) & 1;                            // AME
		v[7] = (o[6] >> 3) & 7;                            // EBS
		v[9] = o[6] & 7;                                   // KVS
		v[10] = o[7];                                      // OUT
		v[11] = o[8];                                      // CRS
		v[6] = (o[9] >> 3) & 3;                            // RS
		v[12] = o[9] & 7;                                  // DET
		const uint8_t *x = rec + 67 + oi * 2;
		uint8_t *a = aced + oi * 5;
		a[4] = (x[0] >> 4) & 3;                            // EG shift
		a[0] = (x[0] >> 3) & 1;                            // FIX
		a[1] = x[0] & 7;                                   // FIX range
		a[3] = (x[1] >> 4) & 7;                            // OSW
		a[2] = x[1] & 15;                                  // FINE
	}
	vced[58] = (rec[40] >> 6) & 1; vced[53] = (rec[40] >> 3) & 7; vced[52] = rec[40] & 7;
	vced[54] = rec[41]; vced[55] = rec[42]; vced[56] = rec[43]; vced[57] = rec[44];
	vced[60] = (rec[45] >> 4) & 7; vced[61] = (rec[45] >> 2) & 3; vced[59] = rec[45] & 3;
	vced[62] = rec[46]; vced[64] = rec[47];
	vced[70] = (rec[48] >> 4) & 1; vced[63] = (rec[48] >> 3) & 1; vced[68] = (rec[48] >> 2) & 1;
	vced[69] = (rec[48] >> 1) & 1; vced[65] = rec[48] & 1;
	vced[66] = rec[49]; vced[67] = rec[50];
	for (int k = 0; k < 6; k++) vced[71 + k] = rec[51 + k];
	for (int k = 0; k < 10; k++) vced[77 + k] = rec[57 + k];
	static const uint8_t PEG[6] = { 99, 99, 99, 50, 50, 50 };   // DX21 pitch EG, unused
	for (int k = 0; k < 6; k++) vced[87 + k] = PEG[k];
	aced[20] = rec[75]; aced[21] = rec[76]; aced[22] = rec[77];
}

// VCED + ACED -> a 78-byte voice record (the inverse of unpack_voice; the DX21 pitch EG is dropped).
inline void pack_voice(const uint8_t vced[93], const uint8_t aced[23], uint8_t *rec)
{
	for (size_t k = 0; k < VOICE_RECORD; k++) rec[k] = 0;
	for (int oi = 0; oi < 4; oi++)
	{
		uint8_t *o = rec + oi * 10;
		const uint8_t *v = vced + oi * 13;
		for (int k = 0; k < 6; k++) o[k] = v[k];
		o[6] = uint8_t(((v[8] & 1) << 6) | ((v[7] & 7) << 3) | (v[9] & 7));
		o[7] = v[10];
		o[8] = v[11];
		o[9] = uint8_t(((v[6] & 3) << 3) | (v[12] & 7));
		uint8_t *x = rec + 67 + oi * 2;
		const uint8_t *a = aced + oi * 5;
		x[0] = uint8_t(((a[4] & 3) << 4) | ((a[0] & 1) << 3) | (a[1] & 7));
		x[1] = uint8_t(((a[3] & 7) << 4) | (a[2] & 15));
	}
	rec[40] = uint8_t(((vced[58] & 1) << 6) | ((vced[53] & 7) << 3) | (vced[52] & 7));
	rec[41] = vced[54]; rec[42] = vced[55]; rec[43] = vced[56]; rec[44] = vced[57];
	rec[45] = uint8_t(((vced[60] & 7) << 4) | ((vced[61] & 3) << 2) | (vced[59] & 3));
	rec[46] = vced[62]; rec[47] = vced[64];
	rec[48] = uint8_t(((vced[70] & 1) << 4) | ((vced[63] & 1) << 3) | ((vced[68] & 1) << 2) |
		((vced[69] & 1) << 1) | (vced[65] & 1));
	rec[49] = vced[66]; rec[50] = vced[67];
	for (int k = 0; k < 6; k++) rec[51 + k] = vced[71 + k];
	for (int k = 0; k < 10; k++) rec[57 + k] = vced[77 + k];
	rec[75] = aced[20]; rec[76] = aced[21]; rec[77] = aced[22];
}

// Voice bulk dump (VMEM, 32 voices): F0 43 0n 04 20 00 <32 x 128 bytes> <checksum> F7. A voice is
// VMEM bytes 0..66 = record 0..66, 67..72 the DX21 pitch EG, 73..83 = record 67..77, 84..127 zero.
constexpr size_t VMEM_VOICE = 128;
constexpr size_t VMEM_DUMP = 6 + 32 * VMEM_VOICE + 2;

inline void vmem_from_record(const uint8_t *rec, uint8_t *vm)
{
	static const uint8_t PEG[6] = { 99, 99, 99, 50, 50, 50 };
	for (size_t k = 0; k < VMEM_VOICE; k++) vm[k] = 0;
	for (int k = 0; k < 67; k++) vm[k] = rec[k];
	for (int k = 0; k < 6; k++) vm[67 + k] = PEG[k];
	for (int k = 0; k < 11; k++) vm[73 + k] = rec[67 + k];
}

inline void record_from_vmem(const uint8_t *vm, uint8_t *rec)
{
	for (int k = 0; k < 67; k++) rec[k] = vm[k] & 0x7F;
	for (int k = 0; k < 11; k++) rec[67 + k] = vm[73 + k] & 0x7F;
}

// one dump of 32 records; channel 0-15
inline void vmem_dump(const uint8_t *records, int channel, uint8_t *out)
{
	out[0] = 0xF0; out[1] = 0x43; out[2] = uint8_t(channel & 15); out[3] = 0x04; out[4] = 0x20; out[5] = 0x00;
	unsigned sum = 0;
	for (int i = 0; i < 32; i++)
	{
		vmem_from_record(records + i * VOICE_RECORD, out + 6 + i * VMEM_VOICE);
		for (size_t k = 0; k < VMEM_VOICE; k++) sum += out[6 + i * VMEM_VOICE + k];
	}
	out[VMEM_DUMP - 2] = uint8_t((128 - (sum & 127)) & 127);
	out[VMEM_DUMP - 1] = 0xF7;
}

// finds the VMEM dumps in data (any other SysEx is skipped) and fills records from slot 0 on,
// at most max_records; returns the number of voices read (a multiple of 32), -1 if a checksum fails
inline int vmem_read(const uint8_t *data, size_t n, uint8_t *records, int max_records)
{
	int got = 0;
	for (size_t i = 0; i + VMEM_DUMP <= n && got + 32 <= max_records; i++)
	{
		const uint8_t *d = data + i;
		if (d[0] != 0xF0 || d[1] != 0x43 || (d[2] & 0xF0) != 0 || d[3] != 0x04 || d[4] != 0x20 || d[5] != 0x00)
			continue;
		if (d[VMEM_DUMP - 1] != 0xF7) continue;
		unsigned sum = 0;
		for (size_t k = 0; k < 32 * VMEM_VOICE; k++) sum += d[6 + k];
		if (((sum + d[VMEM_DUMP - 2]) & 127) != 0) return -1;
		for (int v = 0; v < 32; v++) record_from_vmem(d + 6 + v * VMEM_VOICE, records + (got + v) * VOICE_RECORD);
		got += 32;
		i += VMEM_DUMP - 1;
	}
	return got;
}

} // namespace txn
