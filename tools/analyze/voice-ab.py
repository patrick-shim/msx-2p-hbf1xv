#!/usr/bin/env python3
"""M39-A digitized-voice A/B analyzer (Fix B sync-before-change).

Quantifies the sub-frame voice content the batch mix collapses vs the
interleaved sync path recovers. Per 4096-sample block, per channel:
  - ac_rms      : RMS after removing the block mean (total energy).
  - hf_rms      : RMS of the first difference x[n]-x[n-1] (high-frequency /
                  sub-frame proxy). The batch mix holds volume constant within
                  each ~147-sample video frame, so its waveform is a ~60 Hz
                  staircase with near-zero hf_rms; the sync path recovers the
                  real kHz PCM, so hf_rms rises sharply.
  - zcr         : zero-crossings/sample about the block mean.
Reports the peak block per channel and the AFTER/BEFORE hf_rms ratio -- the
headline "voice recovered" number. Pure arithmetic; deterministic.
"""
import struct
import sys
import wave

BLOCK = 4096


def load(path):
    with wave.open(path, "rb") as w:
        ch, n = w.getnchannels(), w.getnframes()
        vals = struct.unpack(f"<{(len(w.readframes(n))) // 2}h",
                             wave.open(path, "rb").readframes(n))
    return ch, vals


def blocks(samples):
    out = []
    for b in range(len(samples) // BLOCK):
        x = samples[b * BLOCK:(b + 1) * BLOCK]
        m = sum(x) / BLOCK
        ac = (sum((v - m) ** 2 for v in x) / BLOCK) ** 0.5
        hf = (sum((x[i] - x[i - 1]) ** 2 for i in range(1, BLOCK)) / (BLOCK - 1)) ** 0.5
        flips = sum(1 for i in range(1, BLOCK) if (x[i] - m < 0) != (x[i - 1] - m < 0))
        out.append((ac, hf, flips / (BLOCK - 1)))
    return out


def main():
    before_p, after_p = sys.argv[1], sys.argv[2]
    bc, bvals = load(before_p)
    ac_, avals = load(after_p)
    for ch in range(min(bc, ac_)):
        bb = blocks(bvals[ch::bc])
        ab = blocks(avals[ch::ac_])
        n = min(len(bb), len(ab))
        b_peak = max(bb[:n], key=lambda t: t[1])
        a_peak = max(ab[:n], key=lambda t: t[1])
        b_hf = max((t[1] for t in bb[:n]), default=0.0)
        a_hf = max((t[1] for t in ab[:n]), default=0.0)
        ratio = (a_hf / b_hf) if b_hf > 1e-9 else float("inf")
        print(f"channel {ch}:")
        print(f"  BEFORE(batch): peak ac_rms={b_peak[0]:8.1f} hf_rms={b_peak[1]:8.1f} zcr={b_peak[2]:.3f}")
        print(f"  AFTER (sync) : peak ac_rms={a_peak[0]:8.1f} hf_rms={a_peak[1]:8.1f} zcr={a_peak[2]:.3f}")
        print(f"  hf_rms voice-band recovery AFTER/BEFORE = {ratio:6.1f}x")


if __name__ == "__main__":
    main()
