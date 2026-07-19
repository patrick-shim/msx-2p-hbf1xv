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

"""M34 audio A/B burst-metric analyzer (DEC-0043 Defect A;
docs/m34-planner-package.md section 2.7).

Applies the EXACT section-2.6.4 high-pitch-burst metric to a WAV capture:
partition each channel's PCM into 4,096-sample blocks (full blocks only);
per block compute the integer mean m = floor(sum/4096), x~ = x - m,
ZCR = #{i>=1 : (x~[i-1]<0) != (x~[i]<0)} / 4095 (zero counts as
non-negative), AC_RMS = floor(sqrt(sum(x~^2)/4096)). A block is a BURST
block iff ZCR >= 0.70 AND AC_RMS >= 2500 (int16 scale).

Used on the openMSX (WSL) `record start -audioonly` reference capture of the
game's title -> weapon-select transition: the reference must show ZERO
burst blocks (real-lineage emulation renders the tone-period-0 ultrasonic
silence idiom silent -- openMSX generates chips at native rate and
band-limit-resamples, references/openmsx-21.0/src/sound/AY8910.cc:38-39,482
+ ResampledSoundDevice.hh:23,29,46-48; behaviour reference only). The same
metric fires 17/35 blocks on this project's own PRE-M34 point-sampling
build (docs/m34-implementation-report.md, the recorded discrimination
baseline), so a silent verdict here is a measured property, not a tautology.

Determinism: pure arithmetic over the WAV payload; no wall clock, no
environment content in the output.

Exit code: 0 = zero burst blocks in every channel; 1 = burst block(s) found;
2 = usage/parse error.
"""

import argparse
import math
import struct
import sys
import wave

BLOCK = 4096
ZCR_FLIPS_THRESHOLD = 2867  # flips/4095 >= 0.70  <=>  flips >= 2867
RMS_THRESHOLD = 2500


def block_metrics(samples):
    blocks = []
    for b in range(len(samples) // BLOCK):
        x = samples[b * BLOCK:(b + 1) * BLOCK]
        m = sum(x) // BLOCK
        flips = 0
        acc = 0
        prev_neg = (x[0] - m) < 0
        for i, v in enumerate(x):
            d = v - m
            acc += d * d
            neg = d < 0
            if i >= 1 and neg != prev_neg:
                flips += 1
            prev_neg = neg
        rms = math.floor(math.sqrt(acc / BLOCK))
        blocks.append((flips, rms, flips >= ZCR_FLIPS_THRESHOLD and rms >= RMS_THRESHOLD))
    return blocks


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("wav", help="input WAV (16-bit PCM, mono or stereo)")
    parser.add_argument("--skip-seconds", type=float, default=0.0,
                        help="skip this many seconds from the start before analyzing")
    args = parser.parse_args()

    try:
        with wave.open(args.wav, "rb") as w:
            channels = w.getnchannels()
            width = w.getsampwidth()
            rate = w.getframerate()
            n = w.getnframes()
            payload = w.readframes(n)
    except (OSError, wave.Error) as e:
        print(f"error: cannot read {args.wav}: {e}", file=sys.stderr)
        return 2
    if width != 2:
        print(f"error: expected 16-bit PCM, got {8 * width}-bit", file=sys.stderr)
        return 2

    values = struct.unpack(f"<{len(payload) // 2}h", payload)
    skip_frames = int(args.skip_seconds * rate)
    print(f"wav: rate={rate} channels={channels} frames={n} "
          f"skip={skip_frames} frames ({args.skip_seconds}s)")

    any_burst = 0
    for ch in range(channels):
        samples = values[ch + skip_frames * channels::channels]
        blocks = block_metrics(samples)
        bursts = [i for i, (_, _, burst) in enumerate(blocks) if burst]
        max_flips = max((f for f, _, _ in blocks), default=0)
        max_rms = max((r for _, r, _ in blocks), default=0)
        print(f"channel {ch}: blocks={len(blocks)} burst_blocks={len(bursts)} "
              f"max_zcr_flips={max_flips} (zcr={max_flips / 4095.0:.3f}) max_ac_rms={max_rms}")
        for i in bursts:
            flips, rms, _ = blocks[i]
            t = args.skip_seconds + i * BLOCK / rate
            print(f"  BURST block {i} (~{t:.2f}s): zcr_flips={flips} ac_rms={rms}")
        any_burst += len(bursts)

    print(f"verdict: {'BURST-PRESENT' if any_burst else 'SILENT (zero burst blocks)'}")
    return 1 if any_burst else 0


if __name__ == "__main__":
    sys.exit(main())
