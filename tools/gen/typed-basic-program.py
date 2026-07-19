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

"""M41-S2 typed-at-Ok A/B input generator (keyboard + BASIC-exec scenarios).

Emits, from a SINGLE (row,col,shift) keystroke sequence, BOTH driver forms so
the identical MSX keyboard-matrix events are injected on both engines
(docs/m41-production-qa-plan.md §3.2/§3.5):

  * OURS   : an `HBF1XV-INPUT-SCRIPT v1` file (T=<tstate> KEY=<name> DOWN/UP),
             consumed by src/machine/input_script + main.cpp --input-script.
  * openMSX: a Tcl fragment of `after time <s> { keymatrixdown/up <row> <mask> }`
             (NOT `type` -- that would add a host-layout-translation variable;
             the brief mandates raw matrix events).

Both sides start from the settled MSX-BASIC `Ok` prompt reached with NO disk
(the disk-AUTOEXEC path is BLOCKED per phase0-findings.md). The absolute clocks
differ (our boot->Ok ~frame 1300; openMSX ~20 s) but each side is settled at Ok
before typing and settles again after, so the compared TEXT output is a
deterministic function of the keystroke sequence, not of cross-engine cycle
alignment.

Key positions mirror src/peripherals/msx_key_names.cpp exactly (the same table
the input-script player and the SDL3 mapper resolve through). The `:`/`*` key
is (row2,col0) = key-name "RCTRL" (unshifted `:`, SHIFT `*`) -- the "new
Right-Ctrl :/*" the brief calls out.

Usage:
  python tools/gen/typed-basic-program.py --text "10PRINT2*3\nLIST\n" \
      --out-script debug/m41/typed/k1.script --out-tcl debug/m41/typed/k1.tcl \
      --start-frame 1350 --omx-start 22.0
"""
import argparse
import sys

# char -> (row, col, shift). Positions from src/peripherals/msx_key_names.cpp.
# Shift pairings follow the MSX JIS layout; since BOTH engines decode through
# the SAME BIOS, any (row,col,shift) yields identical text on both -- so the A/B
# holds regardless, and only "is it valid BASIC" depends on these being right.
CHARMAP = {}
for i, ch in enumerate("01234567"):
    CHARMAP[ch] = (0, i, False)
for i, ch in enumerate("89"):
    CHARMAP[ch] = (1, i, False)
_LETTERS = {
    "A": (2, 6), "B": (2, 7),
    "C": (3, 0), "D": (3, 1), "E": (3, 2), "F": (3, 3), "G": (3, 4), "H": (3, 5),
    "I": (3, 6), "J": (3, 7),
    "K": (4, 0), "L": (4, 1), "M": (4, 2), "N": (4, 3), "O": (4, 4), "P": (4, 5),
    "Q": (4, 6), "R": (4, 7),
    "S": (5, 0), "T": (5, 1), "U": (5, 2), "V": (5, 3), "W": (5, 4), "X": (5, 5),
    "Y": (5, 6), "Z": (5, 7),
}
for ch, rc in _LETTERS.items():
    CHARMAP[ch] = (rc[0], rc[1], False)
# symbols (unshifted) -- HB-F1XV JIS matrix (confirmed by S2 typed-at-Ok A/B:
# (1,3)->'^', '=' is SHIFT+'-').  JIS row1 = 8 9 - ^ Y @ [ ;  (bits0..7);
# JIS row2 = : ] , . / _ A B (bits0..7).  So '@' is (1,5), '[' is (1,6),
# ']' is (2,1).  (S3 add: '@' for MSX-MUSIC MML presets '@n'.)
CHARMAP.update({
    "-": (1, 2, False), "^": (1, 3, False), "@": (1, 5, False), "[": (1, 6, False),
    ";": (1, 7, False), ":": (2, 0, False), "]": (2, 1, False), ",": (2, 2, False),
    ".": (2, 3, False), "/": (2, 4, False), " ": (8, 0, False), "\n": (7, 7, False),
    # MSX JIS: '=' is SHIFT+'-' (the '-'/'=' key at row1,col2), NOT (1,3) which
    # decodes to '^' on the HB-F1XV BIOS (verified via typed-at-Ok A/B).
    "=": (1, 2, True),
})
# symbols (shifted) -- MSX JIS shift pairings
CHARMAP.update({
    "*": (2, 0, True),   # the :/* key, SHIFT
    "!": (0, 1, True), '"': (0, 2, True), "#": (0, 3, True), "$": (0, 4, True),
    "%": (0, 5, True), "&": (0, 6, True), "(": (1, 0, True), ")": (1, 1, True),
    "+": (1, 7, True), "?": (2, 4, True), "<": (2, 2, True), ">": (2, 3, True),
})

# (row,col) -> input-script KEY name (mirrors msx_key_names.cpp).
ROWCOL_NAME = {
    (0, 0): "0", (0, 1): "1", (0, 2): "2", (0, 3): "3", (0, 4): "4", (0, 5): "5",
    (0, 6): "6", (0, 7): "7",
    (1, 0): "8", (1, 1): "9", (1, 2): "MINUS", (1, 3): "EQUALS", (1, 4): "BACKSLASH",
    (1, 5): "LEFTBRACKET", (1, 6): "RIGHTBRACKET", (1, 7): "SEMICOLON",
    (2, 0): "RCTRL", (2, 1): "APOSTROPHE", (2, 2): "COMMA", (2, 3): "PERIOD",
    (2, 4): "SLASH", (2, 5): "GRAVE", (2, 6): "A", (2, 7): "B",
    (3, 0): "C", (3, 1): "D", (3, 2): "E", (3, 3): "F", (3, 4): "G", (3, 5): "H",
    (3, 6): "I", (3, 7): "J",
    (4, 0): "K", (4, 1): "L", (4, 2): "M", (4, 3): "N", (4, 4): "O", (4, 5): "P",
    (4, 6): "Q", (4, 7): "R",
    (5, 0): "S", (5, 1): "T", (5, 2): "U", (5, 3): "V", (5, 4): "W", (5, 5): "X",
    (5, 6): "Y", (5, 7): "Z",
    (6, 0): "LSHIFT",
    (7, 7): "RETURN", (8, 0): "SPACE",
}
FRAME_CYCLES = 59736  # headless frame_cycles_per_frame


def main(argv):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--text", required=True, help="literal text; use \\n for RETURN")
    p.add_argument("--out-script", required=True)
    p.add_argument("--out-tcl", required=True)
    p.add_argument("--start-frame", type=int, default=1350)
    p.add_argument("--hold-frames", type=int, default=3)
    p.add_argument("--gap-frames", type=int, default=3)
    p.add_argument("--omx-start", type=float, default=22.0)
    p.add_argument("--omx-hold", type=float, default=0.06)
    p.add_argument("--omx-gap", type=float, default=0.06)
    # S3 AUDIO mode: instead of the screenshot tail, emit a `record` capture of a
    # fixed steady-state window that starts REC_AFTER seconds after the last
    # keystroke and lasts REC_DUR seconds. AUDIO_BASE is the openMSX record base
    # path (WSL). `set throttle off` runs the emulator at max wall-clock speed
    # (emu-time `after` stamps still fire at the correct emu time).
    p.add_argument("--audio-record", default="", help="openMSX record base path (WSL); enables audio tail")
    p.add_argument("--rec-after", type=float, default=2.0, help="sec after last keystroke to start record")
    p.add_argument("--rec-dur", type=float, default=2.5, help="record window length (sec)")
    args = p.parse_args(argv[1:])

    text = args.text.replace("\\n", "\n")
    for ch in text:
        if ch.upper() not in CHARMAP:
            p.error("unmapped character %r" % ch)

    script = ["HBF1XV-INPUT-SCRIPT v1"]
    tcl = []
    frame = args.start_frame
    omx_t = args.omx_start
    per_frame = args.hold_frames + args.gap_frames
    per_sec = args.omx_hold + args.omx_gap
    for ch in text:
        row, col, shift = CHARMAP[ch.upper()]
        name = ROWCOL_NAME[(row, col)]
        mask = 1 << col
        t_down = frame * FRAME_CYCLES
        t_up = (frame + args.hold_frames) * FRAME_CYCLES
        if shift:
            script.append("T=%d KEY=LSHIFT DOWN" % t_down)
        script.append("T=%d KEY=%s DOWN" % (t_down, name))
        script.append("T=%d KEY=%s UP" % (t_up, name))
        if shift:
            script.append("T=%d KEY=LSHIFT UP" % t_up)
        # openMSX matrix events
        if shift:
            tcl.append("after time %.3f {keymatrixdown 6 0x01}" % omx_t)
        tcl.append("after time %.3f {keymatrixdown %d 0x%02X}" % (omx_t, row, mask))
        tcl.append("after time %.3f {keymatrixup %d 0x%02X}" % (omx_t + args.omx_hold, row, mask))
        if shift:
            tcl.append("after time %.3f {keymatrixup 6 0x01}" % (omx_t + args.omx_hold))
        frame += per_frame
        omx_t += per_sec
    script.append("[END]")

    settle_frame = frame + 120
    settle_sec = omx_t + 2.5
    with open(args.out_script, "w", newline="\n") as f:
        f.write("\n".join(script) + "\n")
    with open(args.out_tcl, "w", newline="\n") as f:
        if args.audio_record:
            rec_t0 = omx_t + args.rec_after
            rec_t1 = rec_t0 + args.rec_dur
            f.write("set throttle off\n")
            f.write("\n".join(tcl) + "\n")
            f.write("after time %.3f {record start -audioonly \"%s\"}\n" % (rec_t0, args.audio_record))
            f.write("after time %.3f {record stop; exit}\n" % rec_t1)
        else:
            f.write("\n".join(tcl) + "\n")
            f.write("after time %.3f {screenshot -raw /tmp/m41typed.png; exit}\n" % settle_sec)

    print("typed A/B generated:")
    print("  chars=%d  our capture-frame>=%d  omx capture-sec>=%.2f"
          % (len(text), settle_frame, settle_sec))
    if args.audio_record:
        print("  AUDIO tail: omx rec [%.2f, %.2f]s  our steady window >= frame %d"
              % (omx_t + args.rec_after, omx_t + args.rec_after + args.rec_dur, frame + 30))
    print("  script=%s  tcl=%s" % (args.out_script, args.out_tcl))
    print("  suggested: --frames %d (ours)" % settle_frame)
    print("  last-keystroke: our frame ~%d  omx ~%.2fs" % (frame, omx_t))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
