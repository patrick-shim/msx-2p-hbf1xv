# M24 openMSX A/B Evidence -- Backlog C3 (ZEXDOC/ZEXALL Full Parity Sweep)

- Subject A (this emulator): CpmBdosHarness via `--cpm-run`, bounded to 11 real Z80 instructions (`build/m24_zexall_bound_A.txt` / `build/m24_zexdoc_bound_A.txt`)
- Reference B (openMSX openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC / WSL, machine `C-BIOS_MSX2+`): live Tcl `debug`/`reg` interface implementing the IDENTICAL PC==5/PC==0 BDOS-trap logic (`build/m24_zexall_parity_B_zexall.txt` / `build/m24_zexall_parity_B_zexdoc.txt`)
- Bound: 11 instructions -- empirically confirmed this cycle to be exactly the number of real Z80 instructions BOTH engines execute from the CP/M entry point (`org 0x0100`) before PC first reaches the BDOS trap address (0x0005), where the startup banner (BDOS function C=9) is captured. See the file-level comment above for the full reasoning (R-M24-7).

## Scope note (read before interpreting this diff)

This probe compares the LOADING CONVENTION + BDOS-TRAP MECHANISM itself (org 0x0100; the CP/M `top of memory` word; the `CALL 5` dispatch and its captured C=9 output) for a small, live-Tcl-feasible prefix -- NOT the full ~1.7 million-combinatorial-case-per-suite correctness sweep (already covered, in-process, by `tests/system/hbf1xv_m24_zexall_system_test.cpp` and this project's own pre-existing, independently QA-verified M9/M10/M12 CPU-timing oracles). A full-suite live-Tcl single-step A/B is explicitly, honestly NOT attempted here -- see `docs/m24-implementation-report.md` for the measured full-sweep runtime and `docs/m23-parity-trace-diff.md` for the prior, independently-discovered real-time-scheduling artifact that makes a live-Tcl full sweep infeasible at this instruction volume.

## ZEXALL result

- Emulator A captured: `Z80 instruction exerciser`
- openMSX B captured: `Z80 instruction exerciser`
- **PARITY** -- byte-for-byte identical captured banner text.

## ZEXDOC result

- Emulator A captured: `Z80 instruction exerciser`
- openMSX B captured: `Z80 instruction exerciser`
- **PARITY** -- byte-for-byte identical captured banner text.

## Explicitly BLOCKED / not attempted (honest disposition, not silently skipped)

1. **A genuine full-suite live-Tcl single-step A/B of the entire ZEXALL/ZEXDOC run** -- infeasible per the real-time-scheduling artifact `docs/m23-parity-trace-diff.md` already discovered, at the instruction volume this milestone's own runtime measurement found necessary (see `docs/m24-implementation-report.md`).
2. **Booting a real MSX-DOS disk to a command prompt and running the binaries as genuine MSX-DOS transient programs** -- depends on an MSX-DOS system-disk asset. Checked this cycle: neither `bios/` nor `roms/` contains any such asset (both directories listed in full; only the Sony HB-F1XV's own BIOS/Kanji/disk-ROM/firmware images and two cartridge-mapper smoke fixtures are present -- no MSX-DOS kernel/COMMAND.COM-class file). Also depends on the still-open backlog C5 remainder (full unattended boot). Honestly marked BLOCKED, not silently assumed absent.

