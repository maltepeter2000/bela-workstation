#!/usr/bin/env python3
"""lkgfx_view: run an instrument external in Pd (no GUI, batch), send it messages and show the
Launchkey pictures it sends (lkgfx SysEx, common/gfx/lkgfx.h) as text, optionally as PNG.

  lkgfx_view.py <dir with the external> <object> [-a args] [--png out.png] -- "msg 1" "msg 2" ...

Every message gets its own picture: the pieces since the message before are applied to the
display's bitmap, which is printed after each message ("#" = pixel)."""
import argparse, os, re, subprocess, sys, tempfile

W, PAGES = 128, 6


def run(extdir, obj, args, msgs):
    lines = ["#N canvas 0 0 600 400 12;",
             "#X obj 10 10 r in;",
             f"#X obj 10 40 {obj} {args};".replace("  ", " "),
             "#X obj 10 200 route lkgfx;",
             "#X obj 10 230 print LKGFX;",
             "#X obj 200 10 loadbang;"]
    conns = ["#X connect 0 0 1 0;", "#X connect 2 0 3 0;"]
    for o in range(6):
        conns.append(f"#X connect 1 {o} 2 0;")
    # messages one by one, 300 ms apart (pictures are throttled to 50 ms and go out 2 a ms)
    seq = []
    for i, m in enumerate(msgs):
        seq.append(f"#X obj {10 + i % 4 * 140} {280 + i // 4 * 30} delay {300 * (i + 1)};")
    base = len(lines) - 1   # the #N canvas line is no object
    lines += seq
    for i, m in enumerate(msgs):
        k = base + len(msgs) + i
        esc = m.replace(",", " \\,").replace(";", " \\;")
        lines.append(f"#X msg 10 {600 + i * 30} \\; mark {i} \\; in {esc};")
        conns.append(f"#X connect 4 0 {base + i} 0;")
        conns.append(f"#X connect {base + i} 0 {k} 0;")
    q = len(lines) - 1
    lines.append(f"#X obj 400 10 delay {300 * (len(msgs) + 1) + 200};")
    lines.append("#X msg 400 40 \\; pd quit;")
    lines.append("#X obj 400 70 r mark;")
    lines.append("#X obj 400 100 print MARK;")
    conns += [f"#X connect 4 0 {q} 0;", f"#X connect {q} 0 {q + 1} 0;", f"#X connect {q + 2} 0 {q + 3} 0;"]
    with tempfile.TemporaryDirectory() as d:
        if os.environ.get("LKGFX_KEEP"): d = os.environ["LKGFX_KEEP"]
        p = os.path.join(d, "t.pd")
        open(p, "w").write("\n".join(lines + conns) + "\n")
        r = subprocess.run(["pd", "-nogui", "-noaudio", "-nomidi", "-stderr", "-path", extdir, p],
                           capture_output=True, text=True, timeout=30)
    return r.stderr


def unpack7(b):
    out = []
    for i in range(0, len(b), 8):
        hi, grp = b[i], b[i + 1:i + 8]
        out += [v | ((hi >> k) & 1) << 7 for k, v in enumerate(grp)]
    return out


def frames(log):
    fb = [[0] * W for _ in range(PAGES)]
    pics, cur, shown = {}, None, False
    for ln in log.splitlines():
        m = re.match(r"MARK: (\d+)", ln)
        if m:
            cur, shown = int(m.group(1)), False
            continue
        m = re.match(r"LKGFX: (.*)", ln)
        if not m or cur is None:
            if ln.strip() and "LKGFX" not in ln and "MARK" not in ln:
                print("pd:", ln, file=sys.stderr)
            continue
        b = [int(float(v)) for v in m.group(1).split()]
        if b[:5] == [0xF0, 0x7D, 0x4C, 0x4B, 0x02]:
            page, x, n = b[5:8]
            data = unpack7(b[8:-1])[:n]
            fb[page][x:x + n] = data
        elif b[:5] == [0xF0, 0x7D, 0x4C, 0x4B, 0x03]:
            pics[cur] = [row[:] for row in fb]
    return pics


def ascii(fb):
    rows = []
    for y in range(PAGES * 8):
        rows.append("".join("#" if fb[y >> 3][x] >> (y & 7) & 1 else "." for x in range(W)))
    return "\n".join(rows)


def png(pics, msgs, path):
    from PIL import Image, ImageDraw
    sc, pad = 3, 18
    im = Image.new("RGB", (W * sc + 8, len(msgs) * (PAGES * 8 * sc + pad) + 4), (40, 40, 40))
    d = ImageDraw.Draw(im)
    for i, m in enumerate(msgs):
        y0 = i * (PAGES * 8 * sc + pad)
        d.text((4, y0 + 2), m, fill=(200, 200, 120))
        fb = pics.get(i)
        for y in range(PAGES * 8):
            for x in range(W):
                on = fb and fb[y >> 3][x] >> (y & 7) & 1
                d.rectangle([4 + x * sc, y0 + pad - 4 + y * sc, 4 + x * sc + sc - 1, y0 + pad - 4 + y * sc + sc - 1],
                            fill=(230, 240, 255) if on else (0, 0, 0))
    im.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("extdir")
    ap.add_argument("obj")
    ap.add_argument("-a", "--args", default="")
    ap.add_argument("--png")
    ap.add_argument("msgs", nargs="*")
    a = ap.parse_args()
    log = run(os.path.abspath(a.extdir), a.obj, a.args, a.msgs)
    pics = frames(log)
    for i, m in enumerate(a.msgs):
        print(f"== {m}")
        print(ascii(pics[i]) if i in pics else "(no picture)")
    if a.png:
        png(pics, a.msgs, a.png)


if __name__ == "__main__":
    main()
