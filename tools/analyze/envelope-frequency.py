#!/usr/bin/env python3
# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator
#  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
#
#  LEGAL NOTICE - Personal reference only.
#  This source code is made available solely for personal, non-commercial
#  reference and educational study. Commercial use, sale, or redistribution
#  for profit is not permitted without the author's written consent.
#  Provided "AS IS", without warranty of any kind.
#  Proprietary BIOS/ROM/disk assets remain the property of their respective
#  rights holders and are NOT licensed by this notice.
# ============================================================================

"""PSG hardware-envelope MODULATION-frequency (AM) measurement.

This is the M41-S3 "FFT of the RMS envelope" method, factored into a
reproducible helper for the DEF-M41-PSGENV A/B re-measurement. It measures the
frequency at which a PSG hardware envelope (R13 shape) amplitude-modulates a
sustained tone, i.e. f_E = f_clk / (512 * EP) for a repeating 32-step envelope.

Method (deterministic, pure numpy):
  1. Read the WAV as mono.
  2. Build a short-window RMS envelope (default 4 ms window / 1 ms hop -> a
     1 kHz envelope-sample stream), removing the tone carrier and keeping only
     the amplitude contour.
  3. DC-remove + Hann-window the envelope, zero-pad, rFFT it, and pick the
     dominant peak inside a low-frequency band (default 0.5..40 Hz), with
     parabolic interpolation for sub-bin precision.

The reported peak Hz is the envelope repetition frequency. Compare OURS vs the
openMSX capture; the datasheet value is f_clk/(512*EP), f_clk = 3,579,545 Hz.

Requires numpy. Exit 0 always (measurement tool); prints one line per WAV.
"""

import argparse
import math
import struct
import sys
import wave

import numpy as np


def read_wav_mono(path):
    with wave.open(path, "rb") as w:
        ch = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        n = w.getnframes()
        payload = w.readframes(n)
    if width != 2:
        raise ValueError(f"{path}: expected 16-bit PCM, got {8 * width}-bit")
    vals = np.array(struct.unpack(f"<{len(payload) // 2}h", payload), dtype=np.float64)
    if ch > 1:
        vals = vals.reshape(-1, ch).mean(axis=1)
    return rate, vals / 32768.0


def rms_envelope(samples, rate, win_ms, hop_ms):
    win = max(1, int(rate * win_ms / 1000.0))
    hop = max(1, int(rate * hop_ms / 1000.0))
    n = 1 + max(0, (samples.size - win) // hop)
    env = np.empty(n)
    for i in range(n):
        a = i * hop
        chunk = samples[a:a + win]
        env[i] = math.sqrt(float(np.mean(chunk * chunk))) if chunk.size else 0.0
    env_rate = rate / hop
    return env, env_rate


def dominant_freq(env, env_rate, lo_hz, hi_hz, pad=8):
    e = env - env.mean()
    if e.size < 8 or np.allclose(e, 0.0):
        return 0.0, 0.0
    win = np.hanning(e.size)
    ew = e * win
    nfft = int(2 ** math.ceil(math.log2(e.size * pad)))
    spec = np.abs(np.fft.rfft(ew, n=nfft))
    freqs = np.fft.rfftfreq(nfft, d=1.0 / env_rate)
    lo = np.searchsorted(freqs, lo_hz)
    hi = np.searchsorted(freqs, hi_hz)
    if hi <= lo + 1:
        return 0.0, 0.0
    band = spec[lo:hi]
    k = int(np.argmax(band)) + lo
    if 1 <= k < spec.size - 1 and spec[k] > 0:
        a, b, c = spec[k - 1], spec[k], spec[k + 1]
        denom = a - 2 * b + c
        delta = 0.5 * (a - c) / denom if abs(denom) > 1e-12 else 0.0
    else:
        delta = 0.0
    bin_hz = env_rate / nfft
    peak_hz = (k + delta) * bin_hz
    return peak_hz, float(spec[k])


def measure(path, win_ms, hop_ms, lo_hz, hi_hz, label=""):
    rate, s = read_wav_mono(path)
    # Trim leading/trailing silence to the sustained region.
    peak = float(np.max(np.abs(s))) if s.size else 0.0
    if peak > 1e-9:
        mask = np.abs(s) > 0.05 * peak
        first = int(np.argmax(mask))
        last = s.size - int(np.argmax(mask[::-1]))
        s = s[first:last]
    env, env_rate = rms_envelope(s, rate, win_ms, hop_ms)
    f_env, mag = dominant_freq(env, env_rate, lo_hz, hi_hz)
    dur = s.size / rate if rate else 0.0
    print(f"ENVFREQ label={label} rate={rate} dur={dur:.2f}s "
          f"env_rate={env_rate:.0f}Hz peak_env_freq={f_env:.3f}Hz")
    return f_env


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("wavs", nargs="+", help="WAV file(s) to measure")
    p.add_argument("--win-ms", type=float, default=4.0)
    p.add_argument("--hop-ms", type=float, default=1.0)
    p.add_argument("--lo-hz", type=float, default=0.5)
    p.add_argument("--hi-hz", type=float, default=40.0)
    p.add_argument("--ep", type=int, default=0,
                   help="if set, also print datasheet f_clk/(512*EP)")
    args = p.parse_args(argv[1:])
    if args.ep > 0:
        print(f"datasheet f_E(EP={args.ep}) = 3579545/(512*{args.ep}) = "
              f"{3579545.0 / (512.0 * args.ep):.3f} Hz")
    for wpath in args.wavs:
        try:
            measure(wpath, args.win_ms, args.hop_ms, args.lo_hz, args.hi_hz,
                    label=wpath)
        except (OSError, wave.Error, ValueError) as e:
            print(f"error: {wpath}: {e}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
