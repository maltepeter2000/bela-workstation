#!/usr/bin/env python3
"""Build synthetic Ensoniq test images (no copyrighted content).

Creates an EPS-16 PLUS instrument with 2 layers and 3 wavesamples (one is a
copy of another), then wraps it as:
  test.efe  - Giebler EFE
  test.img  - 800K raw disk image, instrument fragmented across the FAT,
              placed inside a sub-directory
  test.ede  - Giebler EDE disk image with skipped empty blocks
Expected values are written to expected.txt for the C++ test.
"""
import math
import struct
import sys
from pathlib import Path

out = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
out.mkdir(parents=True, exist_ok=True)


def w(v):  # parameter in high byte of a word
    return bytes([v & 0xFF, 0])


def text_words(s, n):
    s = s.ljust(n)[:n]
    return b"".join(bytes([ord(c), 0]) for c in s)


def eps_long(v):
    return struct.pack(">HH", (v & 0xFFF) << 4, ((v >> 12) & 0xFFF) << 4)


def enc24(v):
    return bytes([(v >> 12) & 0xFF, 0, (v >> 4) & 0xFF, ((v >> 20) & 0xF) << 4 | (v & 0xF)])


def hb32(v):
    return b"".join(bytes([(v >> s) & 0xFF, 0]) for s in (24, 16, 8, 0))


def envelope(seed):
    # type, L0s, L0h, (t, Ls, Lh) x 4, t5 ; levels 0..127, times 0..99
    vals = [seed % 4]
    for i in range(15):
        is_time = i in (2, 5, 8, 11, 14)
        vals.append((seed * 7 + i * 5) % 100 if is_time else min(127, 60 + seed * 3 + i * 4))
    b = b"".join(w(v) for v in vals)          # type, L0s, L0h, (t,ls,lh)x4, t5
    b += w(1) + w(-20) + w(seed % 100) + w(3) + w(4) + w(0)
    return b


def wavesample(name, copy, root, loop_mode, start, end, lstart, lend, rate_idx, klo, khi, pcm,
               wavemod=(0, 0, 0, 0, 0), cutoff=(80, 90)):
    b = bytes(6) + text_words(name, 12) + w(copy) + w(0)
    b += envelope(1) + envelope(2) + envelope(3)
    b += bytes([root, 0])                                   # root, vol-mod curve
    b += w(-16) + w(8) + w(0) + w(2) + w(0) + w(-5) + w(0)  # pitch block
    b += w(2) + w(cutoff[0]) + w(cutoff[1]) + w(10) + w(0) + w(30) + w(0) + w(5) + w(0) + w(40) + w(0)
    b += bytes([100, 0, 0, 0])                              # volume, bus, amp src, pan src
    b += w(1) + w(2) + w(3) + w(4)                          # crossfade
    b += bytes([4, (-30) & 0xFF, 50, 0])                    # panEPS, panSigned, ampModAmt, panModAmt
    b += bytes([2, 0, 33, 0])                               # lfo wave, boost, speed, rateModAmt
    b += w(64) + bytes([20, 0, 10, 0])                      # depth, delay, depthModAmt, depthSrc, rateSrc
    b += w(1) + w(0) + w(loop_mode)
    b += hb32(start << 9) + hb32(end << 9) + hb32(lstart << 9) + hb32(lend << 9)
    b += w(rate_idx) + w(klo) + w(khi) + b"".join(w(x) for x in wavemod)
    assert len(b) == 0x120 - 4, hex(len(b))
    b += b"".join(struct.pack(">h", s) for s in pcm)
    return eps_long(len(b) + 4) + b


def layer(index, name, vlo, vhi, keymap):
    b = bytes(2) + w(index) + bytes(2) + text_words(name, 12)
    b += bytes([0, 0, 5, 0, 0, 0]) + w(vlo) + w(vhi) + w(0) + w(0)
    b += b"".join(w(k) for k in keymap)
    return eps_long(len(b) + 4) + b


saw = [int(12000 * (2 * (i % 100) / 100 - 1)) for i in range(1100)]
sine = [int(20000 * math.sin(2 * math.pi * i / 64)) for i in range(704)]
# Transwave set: 8 single-cycle waves of 100 samples; wave k = sine with
# harmonic k+1 mixed in, so the scan position is measurable.
tw = [int(10000 * math.sin(2 * math.pi * (i % 100) / 100)
          + (i // 100) * 1500 * math.sin(2 * math.pi * (i // 100 + 1) * (i % 100) / 100))
      for i in range(800)]

ws_blocks = {
    1: wavesample("SAW LOW", 0, 48, 2, 0, 1099, 200, 1000, 1, 21, 59, saw),
    2: wavesample("SINE HI", 0, 72, 2, 0, 703, 64, 640, 25, 60, 108, sine),
    3: wavesample("SAW COPY", 1, 60, 0, 0, 999, 0, 999, 1, 21, 108, []),
    4: wavesample("TRANSWAVE", 0, 69, 2, 0, 799, 0, 100, 1, 21, 108, tw, (10, 127, 6, 2, 0), (150, 150)),   # range 2 = 1024 samples
}
keymap1 = [1] * 39 + [2] * 49          # A0..B3 -> WS1, C4.. -> WS2
keymap2 = [0] * 30 + [3] * 30 + [0] * 28
keymap3 = [4] * 88

hdr = bytearray(644)
# total size / slot address / unknown left zero except name etc.
hdr[10:34] = text_words("TEST SYNTH", 12)
p = 34
for v in (0, 1, 0):
    hdr[p:p + 2] = w(v); p += 2
hdr[p:p + 2] = struct.pack(">H", 99); p += 2
hdr[p:p + 2] = w(0); p += 2
for v in (0x01, 0x03, 0x02, 0x04):
    hdr[p:p + 2] = w(v); p += 2
for v in (0x07, 0x02, 0):
    hdr[p:p + 2] = w(v); p += 2
p += 2
hdr[p:p + 2] = struct.pack(">H", 0xFFFF); p += 2
for v in (24, 108, -2):
    hdr[p:p + 2] = w(v); p += 2
assert p == 68

body = bytearray()
offsets_layer, offsets_ws = [], {}
for i, (nm, vlo, vhi, km) in enumerate([("LAYER A", 0, 127, keymap1), ("LAYER B", 64, 127, keymap2),
                                         ("LAYER C", 0, 127, keymap3)]):
    offsets_layer.append(644 + len(body))
    body += layer(i + 1, nm, vlo, vhi, km)
for idx, blk in ws_blocks.items():
    offsets_ws[idx] = 644 + len(body)
    body += blk
for i, off in enumerate(offsets_layer):
    hdr[100 + 4 * i:104 + 4 * i] = enc24(off)
for idx, off in offsets_ws.items():
    hdr[132 + 4 * idx:136 + 4 * idx] = enc24(off)
inst = bytes(hdr) + bytes(body)
inst += bytes((-len(inst)) % 512)
blocks = len(inst) // 512

# ---- EFE
efe = bytearray(512)
efe[0:2] = b"\r\n"
efe[2:0x12] = b"Eps File:".ljust(16)
efe[0x12:0x1e] = b"TEST SYNTH  "
efe[0x22:0x2f] = b"Instrument   "
efe[0x2f:0x32] = b"\r\n\x1a"
efe[0x32] = 3
efe[0x34:0x36] = struct.pack(">H", blocks)
efe[0x36:0x38] = struct.pack(">H", blocks)
(out / "test.efe").write_bytes(bytes(efe) + inst)

# ---- IMG: 1600 blocks
TOTAL = 1600
img = bytearray(TOTAL * 512)
dev = bytearray(512)
dev[1] = 0x80; dev[2] = 1
dev[10:14] = struct.pack(">I", 512)
dev[14:18] = struct.pack(">I", TOTAL)
dev[30:38] = b"\xffTESTDSK"
dev[38:40] = b"ID"
img[512:1024] = dev
img[1024:1028] = struct.pack(">I", 100)
img[1024 + 4:1024 + 8] = bytes([1, 30, 1, 0])
img[1024 + 28:1024 + 30] = b"OS"


def fat_set(b, nxt):
    fb, i = 5 + b // 170, b % 170
    off = fb * 512 + i * 3
    img[off:off + 3] = nxt.to_bytes(3, "big")


for fb in range(5, 15):
    img[fb * 512 + 510:fb * 512 + 512] = b"FB"
for b in range(15):
    fat_set(b, 1)


def dir_entry(t, name, size, contig, first):
    return bytes([0, t]) + name.ljust(12).encode()[:12] + struct.pack(">HHI", size, contig, first) + bytes(4)


# sub-directory at blocks 20-21
root = bytearray(1024)
root[0:26] = dir_entry(2, "SOUNDS", 2, 2, 20)
root[1022:1024] = b"DR"
img[3 * 512:5 * 512] = root
fat_set(20, 21); fat_set(21, 1)

# instrument fragmented: first 3 blocks at 30.., rest at 100.. in reverse-ish order
layout = list(range(30, 33)) + list(range(100, 100 + blocks - 3))
for i, b in enumerate(layout):
    img[b * 512:(b + 1) * 512] = inst[i * 512:(i + 1) * 512]
    fat_set(b, layout[i + 1] if i + 1 < len(layout) else 1)
sub = bytearray(1024)
sub[0:26] = dir_entry(8, "", 0, 0, 3)                 # parent pointer
sub[26 * 3:26 * 4] = dir_entry(3, "TEST SYNTH", blocks, 3, 30)
sub[1022:1024] = b"DR"
img[20 * 512:22 * 512] = sub
(out / "test.img").write_bytes(bytes(img))

# ---- EDE: skip all-empty blocks after block 15
skip = bytearray(200)
kept = bytearray()
for b in range(TOTAL):
    blk = img[b * 512:(b + 1) * 512]
    if b >= 15 and not any(blk):
        skip[b // 8] |= 0x80 >> (b % 8)
    else:
        kept += blk
ede = bytearray(512)
ede[0:2] = b"\r\n"
ede[78:80] = b"\r\n"
ede[157:160] = b"\r\n\x1a"
ede[160:360] = skip
ede[509] = 0
ede[510:512] = struct.pack(">H", 3)
(out / "test.ede").write_bytes(bytes(ede) + bytes(kept))

# Expected values (checked by the C++ test)
(out / "expected.txt").write_text(
    f"blocks {blocks}\nsaw_sum {sum(saw)}\nsine_sum {sum(sine)}\ntw_sum {sum(tw)}\n"
    f"saw_len {len(saw)}\nsine_len {len(sine)}\n")
print("fixtures written to", out, "instrument blocks:", blocks)
