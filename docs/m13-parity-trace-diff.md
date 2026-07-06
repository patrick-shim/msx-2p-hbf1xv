# M13 openMSX A/B Parity Trace Diff — RAM/ROM Memory Devices & Slot Population

- Milestone: M13 (RAM/ROM Memory Devices & Slot Population), slice S5.
- Subject-A emulator: `sony_msx_headless` (this repo, Debug build).
- Reference-B emulator: openMSX 19.1 (WSL, `/usr/bin/openmsx`,
  `flavour: debian components: ALSAMIDI CORE GL LASERDISC`).
- Reference machine: **`Sony_HB-F1XV`** — its XML is exactly
  `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`, so slot/ROM/mapper
  semantics match by construction. The five local `bios/*.rom` SHA1s equal the XML
  "confirmed by Meits" revisions (see `docs/m13-implementation-report.md` §2), so
  both emulators execute the same ROM images.
- Comparator: `tools/trace-diff.py` — diffs every ARCHITECTURAL Z80 field (PC,
  opcode bytes, A, F + all flag bits, B, C, D, E, H, L, AF, BC, DE, HL, AF', BC',
  DE', HL', IX, IY, SP, I, R, IFF1, IFF2, IM). Empty B side ⇒ BLOCKED (never a
  pass). VRAM/VDP state is EXCLUDED (owned by M14).
- Harness: `tools/openmsx-mem-parity.ps1` (extends the M10-S4
  `tools/openmsx-trace-parity.ps1` mechanism to the genuine Sony_HB-F1XV machine).
- Capture date: 2026-07-06.

Both subjects produced a GENUINE captured trace on both emulators (no fabrication).

---

## Subject 1 — CPU-visible RAM/ROM probe (BIOS-independent)  → PARITY

Probe `tests/parity/m13_mem_probe.bin` (25 bytes) run from a RAM-mapped state at
`0xC000` on both emulators, 13 instructions to HALT. It exercises the exact
CPU-visible memory behaviours M13 delivers:

- mapper-RAM write + read-back (`LD (0xC800),A` / `LD A,(0xC800)` → `B = 0xEE`);
- mapper segment switch + S1985 read-back (`OUT (0xFC),0x25` / `IN A,(0xFC)` →
  `C = 0x80 | (0x25 & 0x1F) = 0x85`, the `100xxxxx` pattern);
- BIOS byte read via a page-0 slot switch (`OUT (0xA8),0xFC` → page 0 = slot 0-0
  BIOS; `LD A,(0x0000)` → `D = 0xF3` = BIOS image byte 0), while the code runs
  from page 3 (slot 3-0 RAM) which is never remapped.

**Result: ARCHITECTURAL PARITY — EMPTY DIFF.** All 13 instructions match on every
architectural field. openMSX's final register file: `bc=EE85` (B=0xEE RAM,
C=0x85 mapper read-back), `de=F300` (D=0xF3 BIOS byte) — identical to Subject-A.

Raw traces: `build/m13_mem_A.txt` (emulator), `build/m13_mem_B.txt` (openMSX).
Per-subject diff: `build/m13_mem_probe_diff.md`.

---

## Subject 2 — BIOS-boot checkpoint (authentic reset)  → PARITY

Authentic reset (`#A8 = 0`, `PC = 0x0000`, slot-0 BIOS) single-stepped on both
emulators from an identical reset register file (all-zero, `SP = 0xFFFF`, IM1),
emitting architectural state before each instruction.

- Committed/default checkpoint window K = 24 instructions
  (`tools/openmsx-mem-parity.ps1 -BootSteps`), aligned with the internal
  `machine_hbf1xv_bios_boot_integration_test` (32-step image-grounded boot check).
- **Result at K = 24: ARCHITECTURAL PARITY — EMPTY DIFF.**
- Extended probe to **K = 200 instructions: still ARCHITECTURAL PARITY — EMPTY
  DIFF** (`build/m13_boot_A_long.txt` vs `build/m13_boot_B_long.txt`). The early
  HB-F1XV BIOS boot (DI; `JP 0x0416`; then a run of `OUT (n),A` port-init writes)
  does not READ an unimplemented device inside this window, so no VDP/PSG/RTC
  boundary is reached — the M13 memory/slot/mapper fabric alone is sufficient for
  the CPU to fetch and execute real BIOS opcodes in lockstep with openMSX.

Note (boundary honesty): the checkpoint is deliberately bounded before the BIOS
first *reads* an unimplemented device (VDP `#98/#99`, PSG, RTC — owned by M14+).
Such a read, when reached, is a device-scope boundary, not an M13 memory defect;
none occurred within the 200-instruction window captured here.

Raw traces: `build/m13_boot_A.txt` (emulator), `build/m13_boot_B.txt` (openMSX).
Per-subject diff: `build/m13_boot_diff.md`.

---

## Adversarial comparator self-check

`tools/trace-diff.py` is the same comparator validated in M10–M12: an empty B
side returns exit 2 (BLOCKED, never a pass), and any single architectural-field
mismatch returns exit 1 (DIVERGENCE) with the concrete row. Both M13 subjects
returned exit 0 (empty diff) with non-empty B traces (13 and 24/200 rows), so the
parity claims are backed by genuine captures on both emulators.

## Reproduce

```
cmake --build build --config Debug --target sony_msx_headless
tools/openmsx-mem-parity.ps1            # both subjects; writes build/m13_*_diff.md
```
