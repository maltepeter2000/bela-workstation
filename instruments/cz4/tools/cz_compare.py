#!/usr/bin/env python3
"""Compares the recordings of cz4test.pd: czv-testNN.wav (Arturia CZ V or a real CZ) against
cz4-testNN.wav (cz4~), test by test, and writes report.md (and PNG plots when matplotlib is
there) into the same folder.

usage: python3 tools/cz_compare.py pd/test/rec
needs numpy (pip install numpy; matplotlib optional)
"""
import math
import os
import sys
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cz4_tests as T                                 # noqa: E402

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except Exception:                                     # noqa: BLE001
    plt = None


# ---- reading ---------------------------------------------------------------------------

def read_wav(path):
    with wave.open(path, "rb") as w:
        ch, sw, fs, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw == 3:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) |
             (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    elif sw == 2:
        x = np.frombuffer(raw, dtype="<i2") / 32768.0
    elif sw == 4:
        x = np.frombuffer(raw, dtype="<i4") / float(1 << 31)
    else:
        raise ValueError(f"{path}: {sw}-byte samples")
    x = x.reshape(-1, ch).mean(axis=1)
    return x, fs


def onset(x, fs):
    """time of the first 5 ms frame within 45 dB of the peak (None if silent)"""
    hop = int(fs * 0.005)
    n = len(x) // hop
    if n == 0 or np.max(np.abs(x)) < 1e-4:
        return None
    r = np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1) + 1e-20)
    k = int(np.argmax(r > r.max() * 10 ** (-45 / 20)))
    return max(0, k - 1) * hop / fs


def cut(x, fs, t0):
    return x[int(round(t0 * fs)):]


# Both files of a test start at the same moment (cz4test.pd opens them together), so
# one time base serves both: the first note of cz4~, plus the latency of the reference
# (MIDI, plugin, audio), found where the reference starts in time.
MAX_LATENCY = 0.3


def latencies(rec):
    lat = {}
    for k in range(len(T.TESTS)):
        a = os.path.join(rec, f"czv-test{k + 1:02d}.wav")
        b = os.path.join(rec, f"cz4-test{k + 1:02d}.wav")
        if not (os.path.exists(a) and os.path.exists(b)):
            continue
        xa, fs = read_wav(a)
        xb, fsb = read_wav(b)
        oa, ob = onset(xa, fs), onset(xb, fsb)
        if oa is not None and ob is not None:
            lat[k] = oa - ob
    good = [v for v in lat.values() if -0.01 <= v <= MAX_LATENCY]
    return lat, (float(np.median(good)) if good else 0.0)


# ---- measures --------------------------------------------------------------------------

def rms_db(x, fs, hop=0.005, win=0.010):
    h, w = int(fs * hop), int(fs * win)
    n = max(0, (len(x) - w) // h)
    t = np.arange(n) * hop + win / 2
    r = np.array([np.sqrt(np.mean(x[i * h:i * h + w] ** 2)) for i in range(n)])
    return t, 20 * np.log10(r + 1e-9)


def f0_track(x, fs, fmin=40, fmax=2000, hop=0.010, win=0.046):
    h, w = int(fs * hop), int(fs * win)
    lo, hi = int(fs / fmax), int(fs / fmin)
    ts, fs_ = [], []
    for i in range(0, len(x) - w - hi, h):
        s = x[i:i + w + hi]
        if np.sqrt(np.mean(s[:w] ** 2)) < 1e-4:
            ts.append((i + w / 2) / fs); fs_.append(np.nan); continue
        # difference function (YIN)
        a = s[:w]
        d = np.array([np.sum((a - s[k:k + w]) ** 2) for k in range(lo, hi)])
        cm = d / (np.cumsum(d) / np.arange(1, len(d) + 1) + 1e-12)
        k = np.argmax(cm < 0.15) if np.any(cm < 0.15) else np.argmin(cm)
        while k + 1 < len(cm) and cm[k + 1] < cm[k]:
            k += 1
        if 0 < k < len(d) - 1:
            den = d[k - 1] - 2 * d[k] + d[k + 1]
            k = k + 0.5 * (d[k - 1] - d[k + 1]) / den if den != 0 else k
        ts.append((i + w / 2) / fs)
        fs_.append(fs / (k + lo))
    return np.array(ts), np.array(fs_)


def brightness(x, fs, f0, hop=0.020, win=0.093):
    """spectral centroid divided by the fundamental: 1 = a sine, higher = brighter"""
    h, w = int(fs * hop), int(fs * win)
    win_ = np.hanning(w)
    freqs = np.fft.rfftfreq(w, 1 / fs)
    t, b = [], []
    for i in range(0, len(x) - w, h):
        m = np.abs(np.fft.rfft(x[i:i + w] * win_))
        e = np.sum(m)
        t.append((i + w / 2) / fs)
        b.append(np.sum(freqs * m) / e / f0 if e > 1e-6 else np.nan)
    return np.array(t), np.array(b)


def window(t, v, a, b):
    s = (t >= a) & (t < b) & np.isfinite(v)
    return v[s]


def med(t, v, a, b):
    w = window(t, v, a, b)
    return float(np.median(w)) if len(w) else float("nan")


def slope(t, v, a, b):
    """dB/s (or units/s) of a straight line through v in [a, b]"""
    s = (t >= a) & (t < b) & np.isfinite(v)
    if s.sum() < 3:
        return float("nan")
    return float(np.polyfit(t[s], v[s], 1)[0])


def cents(f, ref):
    return 1200 * np.log2(f / ref)


def note_hz(p):
    return 440.0 * 2 ** ((p - 69) / 12)


def ramp_time(t, v, a, b, lo=0.1, hi=0.9, rising=True):
    """time from lo to hi of the way between the value at a and the end value near b"""
    s = (t >= a) & (t < b) & np.isfinite(v)
    tt, vv = t[s], v[s]
    if len(vv) < 5:
        return float("nan")
    v0, v1 = np.median(vv[:3]), np.median(vv[-5:])
    if abs(v1 - v0) < 1e-6:
        return float("nan")
    frac = (vv - v0) / (v1 - v0)
    i_lo = np.argmax(frac >= lo)
    i_hi = np.argmax(frac >= hi)
    return float(tt[i_hi] - tt[i_lo]) if i_hi > i_lo else float("nan")


# ---- tests -----------------------------------------------------------------------------

def m_dca(x, fs):
    t, d = rms_db(x, fs)
    pk = float(np.max(window(t, d, 0, 3)))
    tpk = float(t[np.argmax(np.where(t < 3, d, -999))])
    plateau = med(t, d, 4.5, 5.8)
    # the steps by the dB range they cover
    s = (t <= tpk) & (d > pk - 30) & (d < pk - 3)
    att = slope(t[s], d[s], 0, 99) if s.sum() > 2 else float("nan")
    s1 = (t > tpk) & (t < 4) & (d < pk - 2) & (d > pk - 15)
    s2 = (t > tpk) & (t < 5) & (d < pk - 22) & (d > plateau + 4)
    rel_s = (t > 6.0) & (d < plateau - 3) & (d > plateau - 30)
    return [("peak level dB", pk, "dB"),
            ("attack slope dB/s", att, "ratio"),
            ("step 2 slope dB/s", slope(t[s1], d[s1], 0, 99) if s1.sum() > 2 else float("nan"), "ratio"),
            ("step 3 slope dB/s", slope(t[s2], d[s2], 0, 99) if s2.sum() > 2 else float("nan"), "ratio"),
            ("sustain below peak dB", plateau - pk, "diff"),
            ("release slope dB/s", slope(t[rel_s], d[rel_s], 0, 99) if rel_s.sum() > 2 else float("nan"),
             "ratio")], {"level dB": (t, d)}


def m_dcw(x, fs, note=45, a=0.0, b=5.5):
    t, br = brightness(x, fs, note_hz(note))
    mx = float(np.nanmax(window(t, br, a, b)))
    rt = ramp_time(t, br, a + 0.05, b, 0.1, 0.9)
    t50 = float(t[np.argmax(np.where((t > a) & (t < b), br, 0) >= 1 + 0.5 * (mx - 1))])
    return [("brightness at the top", mx, "ratio"),
            ("time to half brightness s", t50, "ratio"),
            ("10-90 % rise s", rt, "ratio")], {"brightness": (t, br)}


def m_dco(x, fs):
    t, f = f0_track(x, fs, 100, 1200)
    c = cents(f, note_hz(57))
    plat = med(t, c, 1.8, 2.9)
    return [("held level, semitones", plat / 100, "diff"),
            ("rise 10-90 % s", ramp_time(t, c, 0.0, 1.5), "ratio"),
            ("fall after release 10-90 % s", ramp_time(t, c, 2.95, 6.0), "ratio"),
            ("pitch 3 s after release cents", med(t, c, 5.8, 6.2), "diff")], {"pitch cents": (t, c)}


def vib_stats(t, c, a, b):
    s = (t >= a) & (t < b) & np.isfinite(c)
    if s.sum() < 20:
        return float("nan"), float("nan")
    v = c[s] - np.median(c[s])
    dt = np.median(np.diff(t[s]))
    sp = np.abs(np.fft.rfft(v * np.hanning(len(v)), 8192))
    fr = np.fft.rfftfreq(8192, dt)
    k = np.argmax(np.where(fr > 0.3, sp, 0))
    depth = (np.percentile(v, 97) - np.percentile(v, 3)) / 2
    return float(fr[k]), float(depth)


def m_vibrato(x, fs):
    t, f = f0_track(x, fs, 100, 600)
    c = cents(f, note_hz(57))
    base = med(t, c, 0.05, 0.3)
    dev = np.abs(c - base)
    s = (t > 0.05) & (t < 4) & (dev > 8)
    delay = float(t[s][0]) if s.any() else float("nan")
    r1, d1 = vib_stats(t, c, 2.0, 3.9)
    r2, d2 = vib_stats(t, c, 5.6, 7.4)
    r3, d3 = vib_stats(t, c, 8.6, 10.4)
    return [("vibrato delay s", delay, "ratio"), ("rate Hz", r1, "ratio"), ("depth cents", d1, "ratio"),
            ("with mod wheel: depth cents", d2, "ratio"), ("with mod wheel: rate Hz", r2, "ratio"),
            ("with aftertouch: depth cents", d3, "ratio")], {"pitch cents": (t, c)}


def m_velocity(x, fs):
    t, d = rms_db(x, fs)
    tb, br = brightness(x, fs, note_hz(57))
    vels = (127, 100, 80, 60, 40, 20, 1)
    ref = med(t, d, 0.3, 0.7)
    out = []
    for k, v in enumerate(vels):
        out.append((f"vel {v}: level dB re 127", med(t, d, k * 1.2 + 0.3, k * 1.2 + 0.7) - ref, "diff"))
    for k, v in enumerate(vels):
        out.append((f"vel {v}: brightness", med(tb, br, k * 1.2 + 0.3, k * 1.2 + 0.7), "ratio"))
    return out, {"level dB": (t, d)}


def m_keyfollow(x, fs):
    t, d = rms_db(x, fs)
    out = []
    for k, p in enumerate((36, 48, 60, 72, 84)):
        a = k * 3.0
        pk = float(np.max(window(t, d, a, a + 0.3)))
        s = (t > a + 0.05) & (t < a + 2.5) & (d < pk - 2) & (d > pk - 30)
        out.append((f"note {p}: decay dB/s", slope(t[s], d[s], 0, 99) if s.sum() > 2 else float("nan"),
                    "ratio"))
    for k, p in enumerate((36, 48, 60, 72, 84)):
        seg = x[int((k * 3.0 + 0.05) * fs):int((k * 3.0 + 0.25) * fs)]
        tb, br = brightness(seg, fs, note_hz(p), hop=0.02, win=0.093 if p < 60 else 0.046)
        out.append((f"note {p}: brightness", float(np.nanmedian(br)) if len(br) else float("nan"),
                    "ratio"))
    return out, {"level dB": (t, d)}


def m_porta(x, fs):
    t, f = f0_track(x, fs, 60, 800)
    c = cents(f, note_hz(48))
    c57 = cents(f, note_hz(57))
    return [("glide up 10-90 % s", ramp_time(t, c, 1.0, 1.9), "ratio"),
            ("glide down 10-90 % s", ramp_time(t, c, 2.0, 2.9), "ratio"),
            ("bend up semitones", med(t, c57, 5.4, 5.9) / 100, "diff"),
            ("bend down semitones", med(t, c57, 6.4, 6.9) / 100, "diff")], {"pitch cents re C3": (t, c)}


def spectrum(x, fs, a, b):
    s = x[int(a * fs):int(b * fs)]
    n = len(s)
    m = np.abs(np.fft.rfft(s * np.hanning(n)))
    return np.fft.rfftfreq(n, 1 / fs), m


def tuned(fr, m, f):
    """the real frequency of a partial near f (the reference may be tuned differently)"""
    s = (fr > f * 0.97) & (fr < f * 1.03)
    return float(fr[s][np.argmax(m[s])]) if s.any() else f


def peak_at(fr, m, f, rel=0.01):
    s = (fr > f * (1 - rel)) & (fr < f * (1 + rel))
    return float(np.max(m[s])) if s.any() else 0.0


def m_ring(x, fs):
    fr, m = spectrum(x, fs, 0.5, 2.5)
    f1 = tuned(fr, m, note_hz(57))
    f2 = f1 * 2 ** (7 / 12)
    top = max(m.max(), 1e-12)
    db = lambda f: 20 * math.log10(peak_at(fr, m, f) / top + 1e-9)   # noqa: E731
    return [("line 1 (220 Hz) dB", db(f1), "diff"), ("line 2 alone (330 Hz) dB", db(f2), "diff"),
            ("sum (550 Hz) dB", db(f1 + f2), "diff"), ("difference (110 Hz) dB", db(f2 - f1), "diff"),
            ("level dB", 20 * math.log10(np.sqrt(np.mean(x[int(0.5 * fs):int(2.5 * fs)] ** 2)) + 1e-9),
             "diff")], {}


def m_noise(x, fs):
    fr, m = spectrum(x, fs, 0.5, 2.5)
    p = m ** 2 + 1e-20
    s = (fr > 50) & (fr < 15000)
    flat = float(np.exp(np.mean(np.log(p[s]))) / np.mean(p[s]))
    cen = float(np.sum(fr[s] * p[s]) / np.sum(p[s]))
    return [("spectral flatness", flat, "ratio"), ("centroid Hz", cen, "ratio"),
            ("line 1 (220 Hz) dB re max", 20 * math.log10(peak_at(fr, m, tuned(fr, m, note_hz(57)), 0.005)
                                                         / m.max() + 1e-9),
             "diff")], {}


def m_detune(x, fs):
    t, d = rms_db(x, fs, hop=0.002, win=0.004)
    s = (t > 0.5) & (t < 4.2)
    env = 10 ** (d[s] / 20)
    env = env - env.mean()
    sp = np.abs(np.fft.rfft(env * np.hanning(len(env)), 1 << 16))
    fr = np.fft.rfftfreq(1 << 16, 0.002)
    k = np.argmax(np.where((fr > 0.05) & (fr < 30), sp, 0))
    beat = float(fr[k])
    f = note_hz(57)
    return [("beat Hz", beat, "ratio"),
            ("detune cents", 1200 * math.log2((f + beat) / f), "ratio")], {}


def m_spectrum(x, fs):
    out, curves = m_dcw(x, fs, note=45, a=0.0, b=4.5)
    fr, m = spectrum(x, fs, 3.5, 4.4)
    f0 = note_hz(45)
    h = np.array([peak_at(fr, m, f0 * k, 0.02) for k in range(1, 25)])
    h = 20 * np.log10(h / (h.max() + 1e-12) + 1e-9)
    out.append(("strongest harmonic (held)", float(np.argmax(h) + 1), "diff"))
    return out, dict(curves, harmonics=(np.arange(1, 25), h))


def m_level(x, fs):
    t, d = rms_db(x, fs)
    one = med(t, d, 0.3, 1.2)
    return [("one note dB", one, "diff"), ("chord of 4 re one dB", med(t, d, 2.3, 3.2) - one, "diff"),
            ("chord of 8 re one dB", med(t, d, 4.3, 5.2) - one, "diff"),
            ("peak dBFS", 20 * math.log10(np.max(np.abs(x)) + 1e-9), "diff")], {"level dB": (t, d)}


MEASURES = dict(dca=m_dca, dcw=m_dcw, dco=m_dco, vibrato=m_vibrato, velocity=m_velocity,
                keyfollow=m_keyfollow, porta=m_porta, ring=m_ring, noise=m_noise, detune=m_detune,
                spectrum=m_spectrum, level=m_level)


def fmt(v):
    if v is None or (isinstance(v, float) and math.isnan(v)):
        return "–"
    return f"{v:.3g}" if abs(v) < 1000 else f"{v:.0f}"


def compare(rec):
    lines = ["# cz4~ against the reference", "",
             f"Recordings in `{rec}`. Reference = czv-testNN.wav, cz4 = cz4-testNN.wav. "
             "Ratio = cz4 / reference (1 = equal), diff = cz4 − reference.", ""]
    lat, lat_all = latencies(rec)
    lines += [f"Latency of the reference (median): {lat_all * 1000:.0f} ms.", ""]
    for k, t in enumerate(T.TESTS):
        a = os.path.join(rec, f"czv-test{k + 1:02d}.wav")
        b = os.path.join(rec, f"cz4-test{k + 1:02d}.wav")
        lines.append(f"## {t['name']}")
        lines.append("")
        lines.append(t["about"])
        lines.append("")
        if not (os.path.exists(a) and os.path.exists(b)):
            lines += ["(not recorded)", ""]
            continue
        xa, fs = read_wav(a)
        xb, fsb = read_wav(b)
        ob = onset(xb, fsb) or 0.0
        own = lat.get(k)
        if own is not None and own > MAX_LATENCY:
            lines += [f"**The reference starts {own:.2f} s late: its first notes are missing "
                      "(tone not ready?). Measures of that part are not valid.**", ""]
        latency = own if own is not None and -0.01 <= own <= MAX_LATENCY else lat_all
        xa, xb = cut(xa, fs, max(0.0, ob + latency)), cut(xb, fsb, ob)
        if np.max(np.abs(xa)) < 1e-4:
            lines += ["(the reference recording is silent)", ""]
            continue
        ra, ca = MEASURES[t["measure"]](xa, fs)
        rb, cb = MEASURES[t["measure"]](xb, fsb)
        lines += ["| measure | reference | cz4 | compare |", "|---|---|---|---|"]
        for (name, va, kind), (_, vb, _) in zip(ra, rb):
            if kind == "ratio" and va and not math.isnan(va) and not math.isnan(vb):
                cmpv = f"× {vb / va:.2f}"
            elif not math.isnan(va) and not math.isnan(vb):
                cmpv = f"{vb - va:+.3g}"
            else:
                cmpv = "–"
            lines.append(f"| {name} | {fmt(va)} | {fmt(vb)} | {cmpv} |")
        lines.append("")
        if plt and ca:
            fig, axes = plt.subplots(len(ca), 1, figsize=(9, 2.8 * len(ca)), squeeze=False)
            for ax, key in zip(axes[:, 0], ca):
                (ta, va), (tb, vb) = ca[key], cb[key]
                if key == "harmonics":
                    ax.bar(ta - 0.2, va, 0.4, label="reference")
                    ax.bar(tb + 0.2, vb, 0.4, label="cz4~")
                else:
                    ax.plot(ta, va, label="reference", lw=1)
                    ax.plot(tb, vb, label="cz4~", lw=1)
                ax.set_title(f"{t['name']}: {key}")
                ax.legend()
                ax.grid(alpha=0.3)
            fig.tight_layout()
            png = f"test{k + 1:02d}.png"
            fig.savefig(os.path.join(rec, png), dpi=90)
            plt.close(fig)
            lines += [f"![{t['name']}]({png})", ""]
    out = os.path.join(rec, "report.md")
    open(out, "w").write("\n".join(lines) + "\n")
    return out


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    print("wrote", compare(sys.argv[1]))
