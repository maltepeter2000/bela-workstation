#!/usr/bin/env python3
"""Auswertung einer VRS-23-Aufnahme des Testsignals.

Aufnahme:  Kanal 1 = Effect L, Kanal 2 = Effect R,
           optional Kanal 3 = Orig. Out (dient zur Zeitausrichtung).
Ohne Kanal 3 muss die Aufnahme sample-genau zur Wiedergabe liegen
(DAW: Testsignal abspielen und gleichzeitig aufnehmen, Latenzausgleich an).

Aufruf:  python3 analyse_vrs23.py aufnahme.wav [Bezeichnung]
Ausgabe: <Bezeichnung>_analyse.png und Tabelle der Echo-Spitzen im Terminal.
"""
import sys
import numpy as np
from scipy.io import wavfile
from scipy.signal import fftconvolve, find_peaks, hilbert

IR_LEN = 1.5      # s, ausgewertete Laenge der Impulsantwort
PEAK_FLOOR = 40   # dB unter Maximum, darunter keine Spitzen


def load(path):
    fs, x = wavfile.read(path)
    if x.dtype.kind == "i":
        x = x / float(np.iinfo(x.dtype).max)
    x = x.astype(float)
    if x.ndim == 1:
        x = x[:, None]
    return fs, x


def inverse_sweep(meta):
    fs, f1, T, L = int(meta["fs"]), float(meta["f1"]), float(meta["T"]), float(meta["L"])
    t = np.arange(int(T * fs)) / fs
    s = np.sin(2 * np.pi * f1 * L * (np.exp(t / L) - 1))
    return s[::-1] * np.exp(-t / L)


def align_offset(ref, fs, meta):
    """Versatz der Aufnahme gegen das Testsignal ueber den ersten Impuls."""
    tp = float(meta["pulse_times"][0])
    seg = ref[: int((tp + 2.0) * fs)]
    i = int(np.argmax(np.abs(seg)))
    return i - int(tp * fs)


def envelope_db(x, fs, smooth_ms=0.5):
    e = np.abs(hilbert(x))
    n = max(1, int(fs * smooth_ms / 1000))
    e = np.convolve(e, np.ones(n) / n, mode="same")
    return 20 * np.log10(e / (e.max() + 1e-12) + 1e-9)


def echo_peaks(env_db, fs):
    pk, _ = find_peaks(env_db, height=-PEAK_FLOOR, prominence=3, distance=int(0.004 * fs))
    return pk


def grid_period(env_db, fs, lo_ms=10, hi_ms=200):
    """Dominante Periode der Huellkurve (Autokorrelation) – Hinweis auf festes Echo-Raster."""
    e = 10 ** (env_db / 20)
    e = e - e.mean()
    ac = fftconvolve(e, e[::-1])[len(e) - 1:]
    ac /= ac[0] + 1e-12
    lo, hi = int(lo_ms * fs / 1000), int(hi_ms * fs / 1000)
    k = lo + int(np.argmax(ac[lo:hi]))
    return 1000 * k / fs, ac[k]


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    path = sys.argv[1]
    label = sys.argv[2] if len(sys.argv) > 2 else path.rsplit(".", 1)[0]
    meta = np.load("vrs23_sweep_meta.npz")
    fs, x = load(path)
    assert fs == int(meta["fs"]), f"Aufnahme muss {int(meta['fs'])} Hz haben"

    off = align_offset(x[:, 2], fs, meta) if x.shape[1] >= 3 else 0
    print(f"Zeitversatz Aufnahme: {1000 * off / fs:.2f} ms")

    inv = inverse_sweep(meta)
    s0 = int(float(meta["sweep_start"]) * fs) + off
    seg = x[s0: s0 + len(inv) + int((IR_LEN + 1) * fs), :2]
    names = ["Effect L", "Effect R"][: seg.shape[1]]

    irs = []
    for c in range(seg.shape[1]):
        full = fftconvolve(seg[:, c], inv)
        z = len(inv) - 1
        irs.append(full[z: z + int(IR_LEN * fs)])

    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(3, 1, figsize=(11, 10))
    t_ms = np.arange(int(IR_LEN * fs)) / fs * 1000
    gmax = max(np.abs(ir).max() for ir in irs) + 1e-12

    for c, (ir, nm) in enumerate(zip(irs, names)):
        env = envelope_db(ir / gmax * np.abs(ir).max(), fs)
        env = env + 20 * np.log10(np.abs(ir).max() / gmax)
        pk = echo_peaks(env, fs)
        per, strength = grid_period(env, fs)
        print(f"\n{nm}: {len(pk)} Echo-Spitzen, Raster-Periode {per:.1f} ms (Korrelation {strength:.2f})")
        print("   Nr   Zeit ms   Pegel dB")
        for i, p in enumerate(pk[:25]):
            print(f"  {i + 1:3d}  {t_ms[p]:8.1f}  {env[p]:8.1f}")
        ax[0].plot(t_ms, env, lw=0.8, label=nm)
        ax[0].plot(t_ms[pk], env[pk], "x", ms=4)
        dens = np.histogram(t_ms[pk], bins=np.arange(0, IR_LEN * 1000 + 50, 50))[0]
        ax[2].step(np.arange(0, IR_LEN * 1000, 50), dens, where="post", label=nm)
        spec = 20 * np.log10(np.abs(np.fft.rfft(ir[: int(0.5 * fs)])) + 1e-9)
        f = np.fft.rfftfreq(int(0.5 * fs), 1 / fs)
        ax[1].semilogx(f[1:], spec[1:] - spec.max(), lw=0.6, label=nm)

    ax[0].set(xlim=(0, IR_LEN * 1000), ylim=(-60, 3), xlabel="Zeit (ms)", ylabel="Pegel (dB)",
              title=f"{label}: Impulsantwort aus Sweep (Huellkurve)")
    ax[1].set(xlim=(20, 12000), ylim=(-60, 3), xlabel="Frequenz (Hz)", ylabel="dB",
              title="Frequenzgang der ersten 500 ms (Kammfilter = regelmaessiges Raster)")
    ax[2].set(xlabel="Zeit (ms)", ylabel="Spitzen pro 50 ms", title="Echodichte")
    for a in ax:
        a.grid(alpha=0.3)
        a.legend()
    fig.tight_layout()
    out = f"{label}_analyse.png"
    fig.savefig(out, dpi=120)
    print(f"\nGrafik: {out}")


if __name__ == "__main__":
    main()
