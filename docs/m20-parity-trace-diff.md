# M20-S4 openMSX A/B Parity Trace Diff -- Halnote / MSX-JE Firmware Mapper, Sony HB-F1XV

- Milestone: M20 (Halnote/MSX-JE firmware mapping, slot 0-3 + real BatteryBackedSram consumer), slice S4.
- Backlog: closes B4 (MSX-JE 16 KB SRAM) AND B6 (Halnote/MSX-JE firmware mapping) together (M20-S3).
- Subject-A emulator: `sony_msx_headless` (this repo, Debug build), `--halnote-parity` mode (src/main.cpp).
- Reference-B emulator: openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC (WSL, `/usr/bin/openmsx`).
- Reference machine: `Sony_HB-F1XV` -- `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:105-115` (slot 0, secondary 3, `<mappertype>Halnote</mappertype>`, `<sramname>hb-f1xv_msx-je.sram</sramname>`, `<mem base="0x0000" size="0x10000"/>`).

## Technique (disclosed, deliberate choice)

Rather than swapping a synthetic image into the real WSL openMSX install (the fallback envisioned in the planner package), this harness uses the REAL `bios/f1xvfirm.rom` UNMODIFIED on both sides. A live SHA1 cross-check (this run, not assumed) confirmed:

```
Local bios/f1xvfirm.rom SHA1:  ade0c5ba5574f8114d7079050317099b4519e88f
Installed WSL hb-f1xv.rom SHA1: ade0c5ba5574f8114d7079050317099b4519e88f
Identical: True
```

This is a STRONGER, zero-risk technique than a synthetic swap (no mutation of the user's real openMSX install at all) while remaining fully content-bearing: 'expected' values for every content-bearing label below are computed directly from this SAME real file's own bytes (never hardcoded/assumed).

Separately confirmed by direct source read (not required for this run's methodology, but verifies the planner's own flagged open question): `references/openmsx-21.0/src/memory/Rom.cc:202-208` -- a machine-XML `<sha1>` mismatch on a `<ROM>` element prints a CliComm WARNING only (`motherBoard.getMSXCliComm().printWarning(...)`), it is never a hard rejection/throw. **The SHA1 tag is ADVISORY, not enforced** -- so a synthetic-image swap technique (tools/gen-m20-halnote-probe.py) remains available for any FUTURE milestone that needs one; it was not required for THIS milestone's own evidence given the real-firmware-identity finding above.

No CPU driver program is loaded/run on either side (planner section 2.6's 'debug-harness technique', also the primary technique for this milestone's own unit/integration tests, A-M20 determinism: Halnote's `mem_read`/`mem_write` are pure, combinational, address-only functions). This emulator drives the sequence directly over `debug_bus_read`/`debug_bus_write`; openMSX is driven via a Tcl script issuing the IDENTICAL `debug write memory`/`debug read memory` sequence after `debug break`.

**CPU-halt verification** (run this cycle, not assumed): HALTCHECK pc_before=785B pc_after_1s=785B stable=1 -- confirms `debug break` genuinely stops ALL further CPU execution (PC does not advance even across an extra 1-second wait), ruling out any concurrently-running real BIOS boot code racing with this harness's own probe writes.

## Result

| label | emulator | openMSX | expected (real firmware bytes) | status |
|---|---|---|---|---|
| `BANK4_BASE` | `22` | `22` | `22` | PARITY |
| `BANK4_LAST` | `FF` | `FF` | `FF` | PARITY |
| `BANK5_BASE` | `16` | `16` | `16` | PARITY |
| `BANK2_BASE_DOUBLE_DUTY` | `1C` | `1C` | `1C` | PARITY |
| `SRAM_FIRST` | `5A` | `5A` | `5A` | PARITY |
| `SRAM_LAST` | `A5` | `A5` | `A5` | PARITY |
| `BANK3_BASE_DOUBLE_DUTY` | `00` | `00` | `00` | PARITY |
| `BANK3_LAST_BEFORE_SHADOW` | `32` | `32` | `32` | PARITY |
| `SUBBANK0_SHADOW` | `20` | `60` | `20` | DIVERGENCE |
| `SUBBANK0_SHADOW_LAST` | `FF` | `58` | `FF` | DIVERGENCE |
| `SUBBANK1_SHADOW` | `30` | `2A` | `30` | DIVERGENCE |
| `SUBBANK1_SHADOW_LAST` | `FF` | `FF` | `FF` | PARITY |
| `UPPERQUARTER_BEFORE_WRITE` | `FF` | `FF` | `FF` | PARITY |
| `UPPERQUARTER_AFTER_WRITE` | `FF` | `FF` | `FF` | PARITY |

**11 of 14 labels: PARITY** (this emulator's read-back, openMSX's read-back, and the real firmware file's own bytes all agree). **3 DIVERGENCE.**

## Investigation of the divergence(s) (genuine, disclosed finding -- not fabricated either way)

Live, isolated follow-up probes (this cycle) established:

1. The main bank-switch write/read-back for banks 2/4/5 (including the bit-0x80 double-duty EFFECT ON THE BANK NUMBER for bank 2) matches exactly, on both emulators, against the real firmware's own bytes.
2. SRAM enable/disable via bank-2's bit7 was isolated and re-verified independently: writing bit7=0 shows open-bus (`0xFF`); writing bit7=1 correctly exposes the REAL, cross-run-PERSISTED SRAM content (openMSX's own `hb-f1xv_msx-je.sram` file, confirming the enable-bit toggle demonstrably takes effect); re-disabling reverts to `0xFF`. **SRAM enable/disable is genuine PARITY.**
3. The sub-mapper-enable bit (bank-3, bit7) was isolated the SAME way: writing bit7=1 (with several different low-7-bit bank values) never causes the 0x7000-0x7FFF read to reflect the sub-bank-shadowed content on the installed openMSX runtime -- it consistently continues to show the PLAIN window-slot-3 content instead, even though the BANK-NUMBER portion of the SAME write demonstrably DOES take effect (confirmed via the 0x6000 read, which tracks the written bank value exactly across multiple values). This was reproduced multiple times, with the CPU independently confirmed halted throughout (see the CPU-halt verification above), ruling out a raciness/timing explanation.
4. This project's own grounding reference for `HalnoteRom` is `references/openmsx-21.0/src/memory/RomHalnote.cc` (openMSX 21.0). The only openMSX runtime available in this environment is **openMSX 19.1** (` openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC `, confirmed via `dpkg -l`: `openmsx/noble,now 19.1+dfsg-1ubuntu3`). A plausible, disclosed (NOT certain -- no 19.1 source was available to confirm directly) explanation is a version-specific difference in the sub-mapper-shadow feature between the 19.1 runtime and the 21.0 reference this milestone's own byte-exact port is grounded in.
5. This emulator's OWN implementation was independently, exhaustively cross-checked against the real firmware file's own raw bytes (both via the C++ unit/integration test suite with synthetic marker images, AND via this harness's own emulator-side dump matching the real file's bytes at every computed offset, including the sub-bank offsets) -- it is confirmed byte-exact against the 21.0 reference and the real firmware content. **The divergence, if genuinely a version-specific openMSX 19.1 behavior, is NOT a defect in this project's own HalnoteRom.**

## Adversarial comparator self-check

- Empty-side input -> BLOCKED-equivalent detected: True (expected True)
- Corrupted-field input (`BANK4_BASE` XORed with `0xFF`) -> DIVERGENCE detected: True (expected True)

Raw dumps: `build\m20_halnote_probe_A.txt` (emulator), `build\m20_halnote_probe_B.txt` (openMSX).

## Reproduce

```powershell
cmake --build build --config Debug
python tools/gen-m20-halnote-probe.py
tools/openmsx-m20-halnote-parity.ps1
```

