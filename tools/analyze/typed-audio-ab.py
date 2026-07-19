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

"""M41-S1 spectral / RMS audio A/B comparator
(docs/m41-production-qa-plan.md §5.3).

Compares two WAV captures -- OUR side (tools/convert/audio-dump-to-wav.py output) vs
openMSX (`record start -audioonly` output) -- for the M41 audio A/B suite
(PSG P1-P5, FM/OPLL F1-F4). Unlike tools/analyze/audio-ab.py (a burst-
silence *presence* detector, insufficient for pitch/timbre parity) this tool
measures three complementary, rate-independent similarities:

  1. Dominant-peak frequency delta (CENTS). Onset-aligned, Hann-windowed FFT;
     the strongest spectral peak (>= a floor freq) on each side, converted to
     Hz using each WAV's own rate, compared as 1200*log2(f_ours/f_omx).
  2. Band-energy COSINE similarity. A coarse log-spaced band-energy vector
     (Hz bands, so a 44.1 kHz vs any-rate capture compare directly); cosine of
     the two normalized vectors -- a timbre/spectral-shape match.
  3. RMS-envelope Pearson CORRELATION. Per-10 ms RMS over the common duration
     resampled to a shared time grid -- an amplitude-shape / attack-decay match
     (the OPLL A/B is envelope-accurate, NOT sample-exact: KD-FM-SYNTH, the
     license-blocked YM2413 attack table).

PASS thresholds are subsystem-specific (§4) and applied by the CALLER (the
harness); this tool reports the raw metrics + a coarse verdict against the
default PSG-tone thresholds (peak <= 5 cents AND cosine >= 0.98 AND env >=
0.95). Deterministic: pure numpy arithmetic over the WAV payloads.

--self-check proves the metric is NON-VACUOUS: self-vs-self reads perfect
(0 cents / cosine 1 / env 1); a pitch-shifted copy is DETECTED (cents delta
tracks the shift). Requires numpy.

Exit code: 0 = MATCH (or self-check pass); 1 = mismatch / self-check fail;
2 = usage/parse error.
"""

import argparse
import math
import struct
import sys
import wave

import numpy as np

FLOOR_HZ = 40.0          # ignore DC / sub-audio when picking the dominant peak
CEIL_HZ = 20000.0        # analysis ceiling (both engines resample below this)
N_BANDS = 24             # log-spaced band-energy vector length
ENV_MS = 10.0            # RMS-envelope window (ms)
ANALYSIS_SECONDS = 0.5   # fixed FFT payload length past onset (steady-state)


def read_wav_mono(path):
    """Return (rate, float64 mono samples in [-1,1]). Mixes stereo to mono."""
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


def onset_index(samples, frac=0.1):
    """First index whose |sample| exceeds frac * peak (silence-trim)."""
    peak = float(np.max(np.abs(samples))) if samples.size else 0.0
    if peak <= 1e-9:
        return 0
    thr = frac * peak
    idx = np.argmax(np.abs(samples) > thr)
    return int(idx)


def dominant_peak_hz(seg, rate):
    """Hann-windowed FFT dominant peak (>= FLOOR_HZ), parabolic-interpolated."""
    if seg.size < 4:
        return 0.0
    win = np.hanning(seg.size)
    spec = np.abs(np.fft.rfft(seg * win))
    freqs = np.fft.rfftfreq(seg.size, d=1.0 / rate)
    lo = np.searchsorted(freqs, FLOOR_HZ)
    hi = np.searchsorted(freqs, min(CEIL_HZ, rate / 2.0))
    if hi <= lo + 1:
        return 0.0
    band = spec[lo:hi]
    k = int(np.argmax(band)) + lo
    if 1 <= k < spec.size - 1 and spec[k] > 0:
        a, b, c = spec[k - 1], spec[k], spec[k + 1]
        denom = (a - 2 * b + c)
        delta = 0.5 * (a - c) / denom if abs(denom) > 1e-12 else 0.0
    else:
        delta = 0.0
    bin_hz = rate / seg.size
    return (k + delta) * bin_hz


def band_energy_vector(seg, rate, edges):
    """Coarse log-spaced (Hz) band-energy vector, L2-normalized.

    `edges` (N_BANDS+1 Hz values) are shared by both sides so a 44.1 kHz and a
    lower-rate capture bin into the SAME Hz bands and compare directly."""
    if seg.size < 4:
        return np.zeros(len(edges) - 1)
    win = np.hanning(seg.size)
    power = np.abs(np.fft.rfft(seg * win)) ** 2
    freqs = np.fft.rfftfreq(seg.size, d=1.0 / rate)
    vec = np.zeros(len(edges) - 1)
    for i in range(len(edges) - 1):
        m = (freqs >= edges[i]) & (freqs < edges[i + 1])
        vec[i] = power[m].sum()
    norm = np.linalg.norm(vec)
    return vec / norm if norm > 0 else vec


def rms_envelope(samples, rate, common_ms, n_bins):
    """RMS per common-time bin over the first (n_bins * common_ms) ms."""
    env = np.zeros(n_bins)
    win = int(rate * common_ms / 1000.0)
    if win < 1:
        win = 1
    for i in range(n_bins):
        a = i * win
        b = a + win
        chunk = samples[a:b]
        env[i] = math.sqrt(float(np.mean(chunk * chunk))) if chunk.size else 0.0
    return env


def cosine(u, v):
    nu, nv = np.linalg.norm(u), np.linalg.norm(v)
    if nu == 0 or nv == 0:
        return 0.0
    return float(np.dot(u, v) / (nu * nv))


def pearson(u, v):
    if u.size < 2 or v.size < 2:
        return 0.0
    u = u - u.mean()
    v = v - v.mean()
    du, dv = np.linalg.norm(u), np.linalg.norm(v)
    if du == 0 or dv == 0:
        return 0.0
    return float(np.dot(u, v) / (du * dv))


def analyze(wav_ours, wav_omx, label="",
            cents_tol=5.0, cosine_min=0.98, env_min=0.95):
    r1, s1 = read_wav_mono(wav_ours)
    r2, s2 = read_wav_mono(wav_omx)
    # Onset-align (silence-trim each independently).
    s1 = s1[onset_index(s1):]
    s2 = s2[onset_index(s2):]
    # Fixed analysis payload past onset (steady-state window).
    n1 = min(s1.size, int(ANALYSIS_SECONDS * r1))
    n2 = min(s2.size, int(ANALYSIS_SECONDS * r2))
    seg1, seg2 = s1[:n1], s2[:n2]

    peak1 = dominant_peak_hz(seg1, r1)
    peak2 = dominant_peak_hz(seg2, r2)
    if peak1 > 0 and peak2 > 0:
        cents = 1200.0 * math.log2(peak1 / peak2)
    else:
        cents = float("nan")

    # Common (shared) log-spaced band edges so the two vectors are comparable
    # regardless of sample rate (ceil = min Nyquist, capped at CEIL_HZ).
    common_ceil = min(CEIL_HZ, r1 / 2.0, r2 / 2.0)
    edges = np.logspace(math.log10(FLOOR_HZ), math.log10(common_ceil), N_BANDS + 1)
    cos = cosine(band_energy_vector(seg1, r1, edges), band_energy_vector(seg2, r2, edges))

    # Envelope over the common duration on a shared 10 ms grid.
    common_s = min(n1 / r1, n2 / r2)
    n_bins = max(1, int(common_s * 1000.0 / ENV_MS))
    e1 = rms_envelope(s1, r1, ENV_MS, n_bins)
    e2 = rms_envelope(s2, r2, ENV_MS, n_bins)
    env_corr = pearson(e1, e2)

    match = (not math.isnan(cents) and abs(cents) <= cents_tol
             and cos >= cosine_min and env_corr >= env_min)
    verdict = "MATCH" if match else "DELTA"

    print(f"M41AUDIO label={label} rate_ours={r1} rate_omx={r2}")
    print(f"  peak_ours={peak1:.2f}Hz peak_omx={peak2:.2f}Hz "
          f"peak_delta_cents={cents:.2f}")
    print(f"  band_cosine={cos:.4f} env_correlation={env_corr:.4f}")
    print(f"  thresholds: cents<={cents_tol} cosine>={cosine_min} env>={env_min}")
    print(f"  verdict={verdict}")
    return {"cents": cents, "cosine": cos, "env": env_corr, "verdict": verdict}


def _write_tone(path, rate, freq, seconds=1.0, amp=0.5, decay=False):
    n = int(rate * seconds)
    t = np.arange(n) / rate
    env = np.exp(-3.0 * t) if decay else np.ones(n)
    y = amp * env * np.sin(2 * math.pi * freq * t)
    pcm = np.clip(y * 32767, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def self_check(workdir):
    import os
    os.makedirs(workdir, exist_ok=True)
    a = os.path.join(workdir, "sc_440.wav")
    b = os.path.join(workdir, "sc_440b.wav")       # identical copy (different file)
    c = os.path.join(workdir, "sc_466.wav")        # +100 cents (semitone)
    d = os.path.join(workdir, "sc_440_22k.wav")    # same pitch, different rate
    _write_tone(a, 44100, 440.0, decay=True)
    _write_tone(b, 44100, 440.0, decay=True)
    _write_tone(c, 44100, 440.0 * 2 ** (100 / 1200.0), decay=True)  # 100 cents up
    _write_tone(d, 22050, 440.0, decay=True)

    ok = True
    print("--- self-check (1): self vs identical copy -> perfect ---")
    r = analyze(a, b, label="self")
    if not (abs(r["cents"]) < 1.0 and r["cosine"] > 0.999 and r["env"] > 0.999
            and r["verdict"] == "MATCH"):
        print("self-check FAIL: self-vs-self is not perfect")
        ok = False
    else:
        print("self-check PASS: self-vs-self perfect")

    print("--- self-check (2): 440 vs +100-cent copy -> DETECTED ---")
    r = analyze(a, c, label="shifted")
    if not (r["verdict"] == "DELTA" and abs(abs(r["cents"]) - 100.0) < 5.0):
        print(f"self-check FAIL: 100-cent shift not detected "
              f"(measured {r['cents']:.2f} cents, verdict {r['verdict']})")
        ok = False
    else:
        print(f"self-check PASS: 100-cent shift detected "
              f"(measured {abs(r['cents']):.2f} cents magnitude)")

    print("--- self-check (3): same pitch, 44.1k vs 22.05k -> rate-independent MATCH ---")
    r = analyze(a, d, label="rate")
    if not (abs(r["cents"]) < 5.0 and r["cosine"] >= 0.90):
        print(f"self-check FAIL: cross-rate same pitch not matched "
              f"(cents {r['cents']:.2f}, cosine {r['cosine']:.3f})")
        ok = False
    else:
        print("self-check PASS: cross-rate same-pitch peak matches (rate-independent)")
    return ok


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--ours", help="our WAV (tools/convert/audio-dump-to-wav.py output)")
    p.add_argument("--omx", help="openMSX WAV (record output)")
    p.add_argument("--label", default="")
    p.add_argument("--cents-tol", type=float, default=5.0)
    p.add_argument("--cosine-min", type=float, default=0.98)
    p.add_argument("--env-min", type=float, default=0.95)
    p.add_argument("--self-check", action="store_true")
    p.add_argument("--workdir", default="debug/m41/audio", help="self-check scratch dir")
    args = p.parse_args(argv[1:])

    if args.self_check:
        return 0 if self_check(args.workdir) else 1

    if not (args.ours and args.omx):
        print("error: --ours and --omx required (or --self-check)", file=sys.stderr)
        return 2
    try:
        r = analyze(args.ours, args.omx, label=args.label,
                    cents_tol=args.cents_tol, cosine_min=args.cosine_min,
                    env_min=args.env_min)
    except (OSError, wave.Error, ValueError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 2
    return 0 if r["verdict"] == "MATCH" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
