# M53 Implementation Report — Standalone MSX Disk Utility (`msx-disk`)

> Provenance note: the developer agent returned this report inline; the coordinator persisted it
> here VERBATIM on 2026-07-15 for the QA handoff, adding only this note and §9. Coordinator
> independently re-verified before persisting: `git status` = exactly ` M .gitignore` +
> ` M CMakeLists.txt` + untracked `diskutils/` + `src/diskutils/`; emulator-src diff
> (`src/core|devices|machine|frontend`) EMPTY; `diskutils/msx-disk.exe` present (907,776 B);
> coordinator smoke run reproduced `--create` exit 0 / SHA256
> `6F1A79835E0421178B01207B196FA245C127C976FA0C6ABC3AAA57E6B0CE5188` / 737,280 B / correct
> sector-0 BPB dump / missing-target `--format` exit 2. (`src/CLAUDE.md` is gitignored at
> `.gitignore:224` — its new responsibility line is a local-only doc change, consistent with the
> repo's CLAUDE.md convention.)

**Milestone:** M53 · **Decision:** DEC-0080 · **Request:** REQ-M53-002 · **Date:** 2026-07-15
**Developer:** MSX Developer Agent · **Charter:** `docs/m53-planner-package.md` (slices S1→S5)
**Status:** COMPLETE — all AC-1..AC-7 met, all evidence gates green. NOT committed (coordinator owns landing).

## 1. Milestone Target

Ship one build-isolated, self-contained host-side executable `msx-disk` with three modes —
`--create` (new fully-formatted blank 720 KB `.dsk`), `--format` (re-format in place), `--read`
(deterministic xxd-style hex dump) — whose blank-image output is byte-exact with the Sony
HB-F1XV MSX2+ 720 KB FAT12 layout. Source under a NEW `src/diskutils/`; the compiled binary
post-build-copied into repo-root `diskutils/`. Emulator suite stays byte-identical (247
fast-subset); the emulator never depends on the tool and the shipped tool never links the
emulator (one-way isolation).

## 2. Code Changes (per-slice what / where)

**S1 — Isolated scaffold + CMake + structure files**
- NEW `src/diskutils/msx_disk_format.{h,cpp}` — `struct DiskFormat` geometry constants +
  `build_blank_image()` (pure function, 737,280 B, layout copied from
  `tools/format-blank-disk.ps1:82-133`, cross-checked vs `src/devices/fdc/disk_image.cpp:43-85`).
  NO emulator header.
- NEW `src/diskutils/hex_dump.{h,cpp}` — `write_hex_dump(std::ostream&, std::span<const uint8_t>,
  uint64_t base_offset)`; the §2.3 xxd/hexdump-C hybrid (8-hex offset, 8+8 byte split, ASCII
  gutter, short-line padding).
- NEW `src/diskutils/cli.{h,cpp}` — `struct Args`, `parse_args(argc,argv)`, `run(Args,out,err)`,
  `print_usage()`; all file I/O + exit codes 0/1/2/3 live here (stream-injected for unit
  testability).
- NEW `src/diskutils/main.cpp` — thin `main()` linking ONLY `msx_diskutil`.
- `CMakeLists.txt:120-146` — additive `msx_diskutil` STATIC lib + `msx_disk` exe
  (`OUTPUT_NAME msx-disk`) OUTSIDE the SDL3 guard; POST_BUILD `make_directory` +
  `copy_if_different "$<TARGET_FILE:msx_disk>" → diskutils/`. Links NOTHING from `sony_msx_core`.
- `.gitignore:22-27` — ignore `/diskutils/msx-disk` + `/diskutils/msx-disk.exe`; NEW tracked
  `diskutils/.gitkeep`.
- `src/CLAUDE.md` — added the `src/diskutils/` responsibility line + build-isolation boundary
  rule (package §6.4 text verbatim).

**S2 `--create`**: `build_blank_image()` + overwrite guard (refuse existing w/o `--force`,
exit 3). **S3 `--format`**: size guard (exists + exactly 737,280 B else refuse w/o `--force`,
exit 3; missing → exit 2) then overwrite. **S4 `--read`**: `write_hex_dump()` +
`--sector <0..1439>` / `--range <startHex-endHex>` (mutually exclusive), whole-image default.
**S5**: FDC-mount integration cross-check + evidence gates.

**Tracked git diff is exactly two files:** `.gitignore` (+7), `CMakeLists.txt` (+26). Everything
else (`src/diskutils/*`, `diskutils/`, `tests/*`, `docs/*`, `src/CLAUDE.md`) lands in untracked
or DEC-0047-local-only trees.

## 3. Byte-Layout Verification Evidence

- **AC-2 (SHA256 == PS1 blank):** `msx-disk --create` output SHA256 =
  `6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188`, **byte-identical** (`cmp`:
  no differences) to `tools/format-blank-disk.ps1 -OutFile x.dsk`. This is the documented golden
  constant. (Running the PS1 in this environment requires `-ExecutionPolicy Bypass`.)
- **AC-3 (machine-compatible):** `diskutils_bpb_matches_machine_unit_test` proves the tool's
  boot/BPB `[0..29]`, `{510,511}`, and FAT-seed bytes `{512..514, 2048..2050}` are byte-identical
  to `DiskImage::synthesize()` — and that the two images correctly diverge in the data area
  (tool zero-filled vs synthesize()'s test pattern), so the comparison is non-trivial.
- **A2/A3 (`disks/msxdos23.dsk` cross-check, ran — disk present):** geometry subset `0x0B..0x1D`
  is **identical** to the tool (bytes/sector 512, media 0xF9, total 1440, sectors/FAT 3,
  sectors/track 9, heads 2). Legitimately-differing DOS-system-disk fields, deliberately NOT
  compared: OEM name (`SONYMSX ` vs `28 7C 77 31 36 49 48 43`), the boot-code region (tool
  all-zero non-bootable vs a real boot loader + `VOL_ID` DOS2 volume label), and the 0x1FE/0x1FF
  boot signature (tool `55 AA` vs msxdos23 `00 00` — real MSX-DOS disks omit the PC-style
  signature). The test is skip-when-absent for lean checkouts.

## 4. Unit Test Results (5 new)

`ctest -R diskutil` and full fast ctest — all GREEN:
- `diskutils_blank_layout_unit_test` (§6.1) — size, JMP/OEM, every BPB field 0x0B..0x1D, boot
  sig, all-zero boot-code region, both F9-FF-FF FAT seeds + zero remainder, empty root dir, zero
  data area, two-call determinism. **Passed.**
- `diskutils_bpb_matches_machine_unit_test` (§6.2, links `msx_diskutil`+`sony_msx_core`) —
  BPB/FAT vs `synthesize()` + msxdos23 geometry cross-check. **Passed.**
- `diskutils_create_format_roundtrip_unit_test` (§6.3) — format-over-garbage → blank;
  create-then-format idempotent; two creates byte-identical (via the real CLI path). **Passed.**
- `diskutils_hexdump_unit_test` (§6.4) — exact §2.3 golden line, base_offset column, short-line
  gutter alignment, empty-span=no-output, `--sector 1`=32 lines from 0x200 F9 FF FF,
  `--range 200-203`=single 3-byte line. **Passed.**
- `diskutils_cli_validation_unit_test` (§6.5) — full exit-code matrix: no-mode/dup-mode/
  missing-path/unknown-opt/bad-sector/bad-range/mutual-exclusion → 1; missing target → 2;
  overwrite & wrong-size refusals → 3; `--force` & help → 0. **Passed.**

## 5. Integration Test Results (1 new)

- `machine_diskutil_fdc_mount_integration_test` (§6.6, links both) — **Passed.** The emulator's
  OWN FDC/DiskDrive headlessly mounts a tool-`build_blank_image()` image and a
  tool-`--format`-produced image (via the real CLI code path over a temp file); LBA 0 is
  streamed back **through the real FDC datapath** (M16 Restore + Read-Sector CPU probe)
  byte-identical to the mounted image (EB FE 90 … F9 … 55 AA); LBA 1 reads FAT seed `F9 FF FF`
  +zeros; LBA 7 reads an all-zero (empty ⇒ directory-readable) root directory. Determinism:
  streamed LBA 0 identical across two runs. This is the honest AC-4 "mount + directory-readable"
  gate — explicitly NOT a DOS/BASIC boot (M28 R6).

**Full fast suite: 253/253 passed** (247 pre-M53 + 6 new), 45.2 s. Name-for-name comparison:
exactly 6 tests added, ZERO baseline tests removed/renamed/re-outcomed.

## 6. Evidence Gate Outputs

1. **validate-assets.ps1:** `Asset validation result: True` (7 BIOS + 1 ROM `fmpac.rom`).
2. **checksum-assets.ps1 → docs/asset-checksums.txt:** byte-identical to prior except the
   `Generated at:` timestamp line (tool touches no `bios/`/`roms/`).
3. **`cmake --build build --config Debug`:** EXIT 0 — both emulator exes + `msx_diskutil.lib` +
   `msx-disk.exe`. After deleting `diskutils/msx-disk.exe`, the POST_BUILD copy re-created it
   (907,776 B). Single canonical `build/` tree only (no stray `build-*`).
4. **`ctest -C Debug --output-on-failure -E hbf1xv_m24_zexall_system_test`:**
   `100% tests passed, 0 failed out of 253`. ZEXALL (#141) excluded per DEC-0080 N/A (zero
   cpu/core diff — proven by the empty isolation diff).
5. **Scope attestation:** `git status` = ` M .gitignore`, ` M CMakeLists.txt`, `?? diskutils/`,
   `?? src/diskutils/`. `git diff --stat -- src/core src/devices src/machine src/frontend` =
   **EMPTY**. `diskutils/msx-disk.exe` ignored; `diskutils/.gitkeep` tracked.
6. **Functional smoke (built exe):** `--create` (737,280 B, exit 0) → `--read --sector 0`
   (correct boot/BPB) → corrupt at offset 7168 → `--format` restores SHA to the golden
   `6f1a79…` → `cmp` vs a fresh `--create` IDENTICAL; wrong-size `--format` w/o `--force` →
   exit 3; `--range 1fe-200` → `000001fe: 55 aa … |U.|`. All correct.

- **openMSX A/B: N/A** (host-side file utility, zero device behavior — DEC-0080; confirmed by
  the empty emulator-src diff).

## 7. Known Issues / Honest Residuals

- **Non-bootable by design (§1.2):** `--create`/`--format` produce a genuinely EMPTY,
  DOS-recognizable data/files medium (empty root ⇒ zero files, all-zero boot-code region). It is
  NOT a bootable OS disk — writing `MSXDOS.SYS`/`COMMAND.COM` would be a redistribution
  violation and is out of scope. AC-4 is deliberately an FDC sector-mount + FAT12-structure
  readback, not a full DOS boot (the M28 finding that a headless DOS boot is not reliably
  reached in-budget).
- **`disks/` msxdos23 cross-check is skip-when-absent:** it ran here (asset present); a lean
  checkout without `disks/` prints SKIP and returns 0 (untracked-asset guard).
- **PowerShell execution policy:** re-running the PS1 SHA256 equivalence needs
  `powershell -ExecutionPolicy Bypass -File tools/format-blank-disk.ps1` in this environment.
  The tool itself and all C++ tests are unaffected.
- No other residuals; no scope escalation was required (zero emulator-source edits became
  necessary).

## 8. QA Handoff

- **Verify from the ONE `build/` tree** (DEC-0041): clean rebuild + full fast ctest; confirm
  253/253 with the pre-M53 247 byte-identical and the 6 new green; ZEXALL optional
  (empty-diff-proven unchanged).
- **Re-run gates:** validate-assets (True), checksum diff (timestamp-only), the empty-diff
  isolation gate over `src/core|devices|machine|frontend`, and confirm `diskutils/msx-disk.exe`
  lands from the POST_BUILD copy.
- **Independently reproduce AC-2:** `msx-disk --create x.dsk` SHA256 ==
  `6f1a79835e0421178b01207b196fa245c127c976fa0c6abc3aaa57e6b0ce5188` ==
  `tools/format-blank-disk.ps1` blank.
- **NORMAL human-release gate** (owner user-test) follows QA sign-off; no auto-close.

## 9. Key Paths (coordinator-appended)

- Tool sources: `src/diskutils/{msx_disk_format,hex_dump,cli}.{h,cpp}`, `src/diskutils/main.cpp`
- Tests: `tests/unit/diskutils/diskutils_{blank_layout,bpb_matches_machine,create_format_roundtrip,hexdump,cli_validation}_unit_test.cpp`,
  `tests/integration/machine/machine_diskutil_fdc_mount_integration_test.cpp`
- Wiring: `CMakeLists.txt:120-146`, `tests/CMakeLists.txt:2001-2049`, `.gitignore:22-27`,
  `src/CLAUDE.md`
- Built binary: `diskutils/msx-disk.exe` (+ tracked `diskutils/.gitkeep`)
