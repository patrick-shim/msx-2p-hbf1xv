#!/usr/bin/env python3
# ============================================================================
#  Sony HB-F1XV MSX2+ Emulator - openMSX .omr replay -> HBF1XV input-script
#
#  Converts an openMSX 21.0 <replay> recording's <events> keyboard-matrix log
#  into this project's deterministic HBF1XV-INPUT-SCRIPT v1 format
#  (machine.input_script). Keyboard events only (no joystick events present in
#  the source replay). Local validation tooling; NOT part of src/.
#
#  Grounding:
#   - openMSX EmuTime unit: MAIN_FREQ = 3579545 * 960
#     (references/openmsx-21.0/src/EmuDuration.hh:15). Our machine T-state
#     clock ticks at the Z80 3579545 Hz, so:  T_state = emutime / 960.
#   - KeyMatrixState semantics
#     (references/openmsx-21.0/src/input/Keyboard.cc:886-887):
#       userKeyMatrix[row] &= ~press;   // 'press'   bits => key DOWN
#       userKeyMatrix[row] |=  release; // 'release' bits => key UP
#     press/release are disjoint single-or-multi-bit masks; bit index == column.
#   - (row,col) -> key name mirrors src/peripherals/msx_key_names.cpp
#     (rows 0-8; the same table the input-script player resolves through
#     peripherals::key_name_to_row_col()).
# ============================================================================

import argparse
import gzip
import re
import sys

MAIN_FREQ = 3579545 * 960          # openMSX EmuTime units per second
TSTATE_DIV = 960                   # emutime -> our Z80 T-state

# (row, col) -> key name, mirroring src/peripherals/msx_key_names.cpp exactly.
# col == the KeyMatrixState bit index (bit0 = col0).
ROWCOL_TO_KEY = {
    (0, 0): "0", (0, 1): "1", (0, 2): "2", (0, 3): "3",
    (0, 4): "4", (0, 5): "5", (0, 6): "6", (0, 7): "7",
    (1, 0): "8", (1, 1): "9", (1, 2): "MINUS", (1, 3): "EQUALS",
    (1, 4): "BACKSLASH", (1, 5): "LEFTBRACKET", (1, 6): "RIGHTBRACKET", (1, 7): "SEMICOLON",
    # (2,0) ":" intentionally unmapped in msx_key_names.cpp
    (2, 1): "APOSTROPHE", (2, 2): "COMMA", (2, 3): "PERIOD", (2, 4): "SLASH",
    (2, 5): "GRAVE", (2, 6): "A", (2, 7): "B",
    (3, 0): "C", (3, 1): "D", (3, 2): "E", (3, 3): "F",
    (3, 4): "G", (3, 5): "H", (3, 6): "I", (3, 7): "J",
    (4, 0): "K", (4, 1): "L", (4, 2): "M", (4, 3): "N",
    (4, 4): "O", (4, 5): "P", (4, 6): "Q", (4, 7): "R",
    (5, 0): "S", (5, 1): "T", (5, 2): "U", (5, 3): "V",
    (5, 4): "W", (5, 5): "X", (5, 6): "Y", (5, 7): "Z",
    (6, 0): "LSHIFT", (6, 1): "LCTRL", (6, 2): "LALT", (6, 3): "CAPSLOCK",
    (6, 4): "LGUI", (6, 5): "F1", (6, 6): "F2", (6, 7): "F3",
    (7, 0): "F4", (7, 1): "F5", (7, 2): "ESCAPE", (7, 3): "TAB",
    (7, 4): "END", (7, 5): "BACKSPACE", (7, 6): "PAGEUP", (7, 7): "RETURN",
    (8, 0): "SPACE", (8, 1): "HOME", (8, 2): "INSERT", (8, 3): "DELETE",
    (8, 4): "LEFT", (8, 5): "UP", (8, 6): "DOWN", (8, 7): "RIGHT",
}


def load_events(path):
    with gzip.open(path, "rb") as f:
        data = f.read().decode("utf-8", "replace")
    ev = data[data.find("<events>"):data.find("</events>") + 10]
    load_events.raw_events = ev
    items = re.findall(r'<item id="\d+" type="([^"]+)">(.*?)</item>', ev, re.S)
    out = []
    for typ, body in items:
        tm = re.search(r"<time>\s*<time>(\d+)</time>", body)
        t = int(tm.group(1)) if tm else None
        rec = {"type": typ, "emutime": t, "body": body}
        if typ == "KeyMatrixState":
            rec["row"] = int(re.search(r"<row>(\d+)</row>", body).group(1))
            rec["press"] = int(re.search(r"<press>(\d+)</press>", body).group(1))
            rec["release"] = int(re.search(r"<release>(\d+)</release>", body).group(1))
        elif typ == "MSXCommandEvent":
            rec["tokens"] = re.findall(r"<item>([^<]*)</item>", body)
        out.append(rec)
    return out


def find_reset_emutime(events):
    # MSXCommandEvent bodies contain nested <item>tok</item> tokens, so the
    # coarse item splitter truncates them. Locate the reset command directly in
    # the raw <events> XML: the <time> immediately preceding "<item>reset</item>".
    raw = getattr(load_events, "raw_events", "")
    pos = raw.find("<item>reset</item>")
    if pos < 0:
        return None
    times = list(re.finditer(r"<time>\s*<time>(\d+)</time>", raw[:pos]))
    return int(times[-1].group(1)) if times else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("omr")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--origin", choices=["reset", "zero"], default="reset",
                    help="Time origin for T=0. 'reset' shifts by the replay's "
                         "reset command (our cold-boot-with-disk == openMSX "
                         "post-reset-with-disk). 'zero' uses raw emutime/960.")
    args = ap.parse_args()

    events = load_events(args.omr)
    t_reset = find_reset_emutime(events) if args.origin == "reset" else 0
    if args.origin == "reset" and t_reset is None:
        print("no reset command found; falling back to origin=zero", file=sys.stderr)
        t_reset = 0

    lines = ["HBF1XV-INPUT-SCRIPT v1"]
    emitted = 0
    last_t = 0
    for e in events:
        if e["type"] != "KeyMatrixState":
            continue
        raw = e["emutime"] - t_reset
        if raw < 0:
            continue
        tstate = raw // TSTATE_DIV
        row = e["row"]
        for bit in range(8):
            mask = 1 << bit
            if e["press"] & mask:
                key = ROWCOL_TO_KEY.get((row, bit))
                if key is None:
                    print(f"WARN: unmapped (row={row},bit={bit})", file=sys.stderr)
                    continue
                lines.append(f"T={tstate} KEY={key} DOWN")
                emitted += 1
                last_t = max(last_t, tstate)
            if e["release"] & mask:
                key = ROWCOL_TO_KEY.get((row, bit))
                if key is None:
                    print(f"WARN: unmapped (row={row},bit={bit})", file=sys.stderr)
                    continue
                lines.append(f"T={tstate} KEY={key} UP")
                emitted += 1
                last_t = max(last_t, tstate)
    lines.append("[END]")

    with open(args.out, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    fps_cycles = 59736  # frame_cycles_per_frame reported by the headless build
    print(f"origin={args.origin} t_reset={t_reset}")
    print(f"emitted {emitted} key events -> {args.out}")
    print(f"last T={last_t}  (~frame {last_t // fps_cycles}, ~{last_t / 3579545:.1f}s post-origin)")


if __name__ == "__main__":
    main()
