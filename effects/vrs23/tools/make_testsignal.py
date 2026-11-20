#!/usr/bin/env python3
"""VRS 23 Testsignal: Impulse + exponentieller Sinus-Sweep.

Erzeugt vrs23_testsignal.wav (48 kHz, 24 bit, mono) und vrs23_sweep_meta.npz,
das analyse_vrs23.py fuer die Entfaltung braucht.

Zeitplan (s):
  0.0 -  1.0  Stille
  1.0          Impuls 1   (bandbegrenzt 6 kHz, -6 dBFS)
  5.0          Impuls 2
  9.0          Impuls 3
 13.0 - 23.0  Sweep 20 Hz - 10 kHz, -12 dBFS, 10 s
 23.0 - 30.0  Stille (Ausklingen)
"""
import numpy as np
from scipy.io import wavfile

FS = 48000
PULSE_TIMES = [1.0, 5.0, 9.0]
SWEEP_START, SWEEP_LEN = 13.0, 10.0
F1, F2 = 20.0, 10000.0
TOTAL = 30.0


def bandlimited_pulse(fc=6000.0, half_len_ms=1.0):
    n = int(FS * half_len_ms / 1000)
    t = np.arange(-n, n + 1) / FS
    h = np.sinc(2 * fc * t) * np.hanning(2 * n + 1)
    return h / np.max(np.abs(h))


def ess(f1, f2, T):
    t = np.arange(int(T * FS)) / FS
    L = T / np.log(f2 / f1)
    x = np.sin(2 * np.pi * f1 * L * (np.exp(t / L) - 1))
    fade = int(0.05 * FS)
    w = np.ones_like(x)
    w[:fade] = np.hanning(2 * fade)[:fade]
    w[-fade:] = np.hanning(2 * fade)[fade:]
    return x * w, L


def write_pcm24(path, y):
    """24-bit PCM mono (von Pd, DAWs und Recordern lesbar)."""
    import wave
    q = np.clip(np.round(y * (2 ** 23 - 1)), -2 ** 23, 2 ** 23 - 1).astype(np.int32)
    b = q.astype("<i4").tobytes()
    b24 = b"".join(b[i:i + 3] for i in range(0, len(b), 4))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(3)
        w.setframerate(FS)
        w.writeframes(b24)


def main():
    y = np.zeros(int(TOTAL * FS))
    p = bandlimited_pulse() * 10 ** (-6 / 20)
    for tp in PULSE_TIMES:
        i = int(tp * FS) - len(p) // 2
        y[i:i + len(p)] += p
    s, L = ess(F1, F2, SWEEP_LEN)
    i0 = int(SWEEP_START * FS)
    y[i0:i0 + len(s)] += s * 10 ** (-12 / 20)

    write_pcm24("vrs23_testsignal.wav", y)
    np.savez("vrs23_sweep_meta.npz", fs=FS, f1=F1, f2=F2, T=SWEEP_LEN, L=L,
             sweep_start=SWEEP_START, pulse_times=np.array(PULSE_TIMES))
    print("vrs23_testsignal.wav geschrieben,", TOTAL, "s")


if __name__ == "__main__":
    main()
