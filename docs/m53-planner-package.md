# M53 Planner Package — Standalone MSX Disk Utility (`msx-disk.exe`)

- Milestone ID: M53
- Title: Standalone MSX Disk Utility (`msx-disk` — create / read / format)
- Decision authority: DEC-0080 (opens M53, ratifies the new `src/diskutils/` + root `diskutils/`
  scope area). Request: REQ-M53-001. Milestone entry: `agent-protocol/state/milestones.md` (M53).
- Spec Owner: MSX Planner Agent · Developer Owner: MSX Developer Agent · QA Owner: MSX QA Agent
- Objective: Ship ONE host-side executable `msx-disk` with three modes — `--create` (new,
  fully-formatted blank 720 KB `.dsk`), `--read` (hex dump to stdout), `--format` (re-format an
  existing image in place) — whose output is BYTE-EXACT compatible with the Sony HB-F1XV MSX2+
  (WD2793 / Sony Disk ROM): 720 KB 3.5" DD, 80 trk × 2 sides × 9 sec/trk × 512 B/sec (737,280 B),
  MSX-DOS FAT12 with a correct MSX boot sector. Source under a NEW `src/diskutils/`; the compiled
  binary is post-build-copied into the owner-created root `diskutils/` from the ONE `build/` tree.
- Constraints (hard): additive, build-isolated target ONLY; the emulator's pre-M53 deterministic
  suite (247 fast-subset) stays byte-identical; the emulator NEVER depends on `src/diskutils/`
  (one-way isolation). openMSX A/B and ZEXALL are N/A (host-side file utility, zero cpu/core/device
  behavior) per DEC-0080. LOW blast radius → NORMAL human-release gate (owner user-test), no
  auto-close.

---

## 1. Scope and Assumptions

### 1.1 In scope
- New source tree `src/diskutils/` producing a single standalone executable `msx-disk`
  (`msx-disk.exe` on Windows).
- Three modes: `--create`, `--read`, `--format`; a `--help`/usage path; deterministic exit codes.
- A byte-exact 720 KB MSX-DOS FAT12 blank-image writer (create + format share one builder).
- A deterministic hex-dump reader.
- CMake wiring: an additive static library + executable target in the ONE `build/` tree, with a
  POST_BUILD copy of the binary into root `diskutils/`.
- `.gitignore` line for the built binary; `src/CLAUDE.md` responsibility line for `src/diskutils/`.
- Deterministic unit tests (byte oracles, round-trip, hex-dump, CLI validation) + one integration
  test that mounts a tool-produced image in the emulator's OWN FDC/DiskDrive and reads it back.

### 1.2 Non-goals / Excluded scope
- NO emulator behavior change. Zero edits to `src/core`, `src/devices`, `src/machine`,
  `src/frontend` (enforced by an empty-diff accuracy gate, §7).
- NO bootable-system disk. `--create`/`--format` produce a NON-bootable data/files disk — the boot
  code region (LBA 0, offset 0x1E..0x1FF) is all-zero and NO MSX-DOS system files are written.
  Making it "boot MSX-DOS" would require copying licensed system files (`MSXDOS.SYS` /
  `COMMAND.COM`), which is out of scope and a redistribution violation. HONEST claim: the image is
  a genuinely EMPTY formatted medium — **Disk-BASIC mountable and DOS-formattable-recognizable**
  (an empty root directory ⇒ zero files), NOT a bootable OS disk. This mirrors the documented
  behavior of the M36-S-c prior art `tools/format-blank-disk.ps1` (its default non-bootable
  save-target; header lines 37-46).
- NO netplay/GUI/multi-format (only flat `.dsk`, the repo's disk format). NO FAT16/DOS2-subdir
  authoring; NO volume-serial/time-derived fields (determinism, §4.4).
- The tool does NOT link the emulator core and the core does NOT link the tool (§5).

### 1.3 Assumptions (each with a verification action)
- **A1 — root `diskutils/` folder present + tracked.** DEC-0080 says the owner created it empty and
  "not gitignored". A `Glob diskutils*` at the repo root returned no files, consistent with an
  empty (or not-yet-present) folder. Git cannot track an empty directory, so a placeholder is
  needed. *Verify:* developer confirms the folder exists; add a tracked `diskutils/.gitkeep` so the
  folder persists in git; POST_BUILD `cmake -E make_directory` guards a missing folder (§6, R7).
- **A2 — `disks/msxdos23.dsk` BPB geometry matches the spec.** The planner could NOT hexdump the
  binary (no shell; 737,280 B exceeds the read cap). A `Grep` confirmed only a *binary match* in the
  OEM/BPB region (offset ~11), not the exact bytes. *Verify:* developer hexdumps `disks/msxdos23.dsk`
  LBA 0 offsets 0x0B..0x1D and diffs vs the spec (§4.2); confirm bytes/sector=512, media=0xF9, total
  sectors=1440, sectors/FAT=3, sectors/track=9, heads=2; DOCUMENT the fields that legitimately
  differ on a DOS system disk (OEM string, boot code, possibly sectors/cluster, DOS2 volume id).
  `disks/` content is untracked (`.gitignore:162`) so any test that reads it must **skip-when-absent**
  (return 0), exactly as `hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp:150-156` does.
- **A3 — OEM string does not affect mountability.** The BIOS/Disk ROM reads BPB *geometry*
  (0x0B..0x1D), not the OEM name (0x03..0x0A), to mount. *Verify:* the FDC-mount integration test
  (§6.6) reads the FAT12 structure back through the real datapath and validates it regardless of
  OEM; the chosen OEM "SONYMSX " is exactly the M41-A/B-validated prior-art value
  (`format-blank-disk.ps1:98`).
- **A4 — the binary is gitignored by default.** The global `*.exe` rule (`.gitignore:11`) already
  ignores it; explicit lines are added for the POSIX no-extension binary + discoverability (§6.5).
  *Verify:* `git status` shows `diskutils/msx-disk*` untracked after a build.
- **A5 — 247 is the current fast-subset baseline.** Per the M53 milestone entry + M52 record.
  *Verify:* developer captures the pre-change ctest count before adding tool tests, then re-runs
  after, asserting the 247 pre-M53 tests are name-for-name and outcome-for-outcome unchanged.

---

## 2. Spec Summary (Interfaces & Behavior)

### 2.1 Source-of-truth for the byte layout (chosen + grounded)
The blank-image byte layout's **primary source of truth is `tools/format-blank-disk.ps1`**
(M36-S-c prior art, DEC-0050), the *working, A/B-validated* empty-formatted 720 KB layout
(lines 82-133). It is corroborated byte-for-byte, at the boot-sector/BPB/FAT level, by two more
sources that AGREE with it (no conflict to resolve):

| Source | Tier | What it grounds | Concrete path |
| --- | --- | --- | --- |
| `format-blank-disk.ps1` | repo prior art (A/B-validated M41) | full BLANK layout incl. zero data area | `tools/format-blank-disk.ps1:82-133` |
| `DiskImage::synthesize()` | repo, byte-oracle-tested | boot sector + BPB + FAT seed (machine's OWN expectation) | `src/devices/fdc/disk_image.cpp:28-88`; constants `src/devices/fdc/disk_image.h:37-43` |
| existing byte oracles | repo test | 0=EB, 21=F9, 24=09, 26=02, 510/511=55 AA | `tests/unit/devices/fdc/disk_image_unit_test.cpp:145-161` |
| openMSX `DiskImageUtils` | tier 2 (behavior ref, EFFECT-only) | 720<sec≤1440 branch: sides 2, FATs 2, spFAT 3, spCluster 2, dirEntries 112, media 0xF9, reserved 1, forced 1440 sectors; layout math | `references/openmsx-21.0/src/fdc/DiskImageUtils.cc:181,190,330-395` |
| fMSX `FDIDisk` | tier 3 (triangulation) | geometry `{2,80,9,512}` = 720 KB MSX format | `references/fmsx-60/source/EMULib/FDIDisk.c:55` |
| `disks/msxdos23.dsk` | oracle (geometry cross-check) | real 737,280-B MSX-DOS floppy geometry | `disks/msxdos23.dsk` (untracked; A2) |

**Precedence note (DEC-0073):** no conflict exists — all three tiers agree on the 720 KB FAT12
geometry, so the repo prior art is adopted directly. `synthesize()` is used ONLY for the
boot-sector/BPB/FAT-seed comparison, NOT for the whole image, because `synthesize()` deliberately
fills the root-dir + data area with a NON-zero per-sector test pattern
(`disk_image.cpp:34-39`) — that is a valid FDC test medium but an INVALID empty filesystem. The tool
must follow `format-blank-disk.ps1`'s ZERO-filled data area to produce a genuinely empty,
mountable, DOS-recognizable disk.

### 2.2 Byte-exact 720 KB FAT12 layout (verified against the repo artifacts, not the prompt)
Geometry: 80 tracks × 2 sides × 9 sectors × 512 B = **1440 sectors = 737,280 bytes**, media
descriptor **0xF9**. LBA↔CHS is `(track*2 + side)*9 + (sector-1)` (`disk_image.cpp:90-102`).

Boot sector (LBA 0), all multi-byte fields little-endian — VERIFIED identical in
`format-blank-disk.ps1:94-122` AND `disk_image.cpp:43-73`:

| Offset (dec / hex) | Bytes | Meaning | Value |
| --- | --- | --- | --- |
| 0..2 / 0x00..0x02 | `EB FE 90` | JMP short + NOP | fixed |
| 3..10 / 0x03..0x0A | `53 4F 4E 59 4D 53 58 20` | OEM name | `"SONYMSX "` |
| 11..12 / 0x0B..0x0C | `00 02` | bytes/sector | 512 |
| 13 / 0x0D | `02` | sectors/cluster | 2 |
| 14..15 / 0x0E..0x0F | `01 00` | reserved sectors | 1 |
| 16 / 0x10 | `02` | number of FATs | 2 |
| 17..18 / 0x11..0x12 | `70 00` | root dir entries | 112 |
| 19..20 / 0x13..0x14 | `A0 05` | total sectors | 1440 |
| 21 / 0x15 | `F9` | media descriptor | 0xF9 |
| 22..23 / 0x16..0x17 | `03 00` | sectors/FAT | 3 |
| 24..25 / 0x18..0x19 | `09 00` | sectors/track | 9 |
| 26..27 / 0x1A..0x1B | `02 00` | heads (sides) | 2 |
| 28..29 / 0x1C..0x1D | `00 00` | hidden sectors | 0 |
| 30..509 / 0x1E..0x1FD | `00...` | boot code region | ALL ZERO (non-bootable, §1.2) |
| 510..511 / 0x1FE..0x1FF | `55 AA` | boot signature | fixed |

Region map (derived from the BPB; confirmed by openMSX `DiskImageUtils.cc:391-396`:
`fatStart=resv=1`, `rootDirStart=1+2*3=7`, `dataStart=7+112/16=14`):

| LBA | Region | Contents |
| --- | --- | --- |
| 0 | Boot sector | table above |
| 1..3 | FAT copy #1 (3 sectors) | byte0..2 = `F9 FF FF`, rest `00` |
| 4..6 | FAT copy #2 (3 sectors) | byte0..2 = `F9 FF FF`, rest `00` |
| 7..13 | Root directory (112 entries × 32 B = 7 sectors) | ALL `00` (empty ⇒ zero files) |
| 14..1439 | Data area (1426 sectors) | ALL `00` |

FAT seeding: only the FIRST three bytes of EACH FAT copy are set (`F9 FF FF`) — the media byte
plus the two reserved FAT12 entries (cluster 0/1); `format-blank-disk.ps1:124-131`,
`disk_image.cpp:75-85`. Everything not listed above is `0x00`.

**Determinism (auditable):** the whole image is a PURE FUNCTION of these constants — no wall-clock,
no host input, no volume serial. The tool's `--create` output MUST be byte-identical (same SHA256)
to `tools/format-blank-disk.ps1 -OutFile x.dsk` (its default non-bootable blank). That equality is
an acceptance oracle (§4.1 AC-2 / §6.2).

### 2.3 CLI grammar
```
msx-disk --create <out.dsk> [--force]
msx-disk --format <disk.dsk> [--force]
msx-disk --read   <disk.dsk> [--sector <lba> | --range <startHex>-<endHex>]
msx-disk --help | -h
```
Rules:
- EXACTLY ONE of `--create | --format | --read | --help` per invocation; zero or ≥2 mode flags ⇒
  usage error.
- A file path argument is REQUIRED for `--create`, `--format`, `--read` (no implicit default path).
- `--create <out.dsk>`: writes the blank 720 KB image. If `<out.dsk>` already exists, REFUSE unless
  `--force` (R3 mitigation — never silently overwrite).
- `--format <disk.dsk>`: re-formats in place (writes the same blank image over the file). The target
  MUST exist (else I/O error). If it exists but is NOT exactly 737,280 bytes, REFUSE unless
  `--force` (R3 — never destroy an unexpected/wrong-size file; `--force` normalizes it to 737,280).
- `--read <disk.dsk>`: hex-dumps to stdout. Default = whole image. `--sector <lba>` dumps exactly
  that one 512-byte sector (lba 0..1439). `--range <startHex>-<endHex>` dumps a byte range
  (inclusive start, exclusive end; hex offsets). `--sector` and `--range` are mutually exclusive.
- `--read` does NOT require the file to be 737,280 bytes (it dumps whatever is there); an
  out-of-range `--sector`/`--range` ⇒ usage error.

Hex-dump line format (deterministic, `xxd`-style; pure function of bytes — NO timestamps):
```
0000fe00: eb fe 90 53 4f 4e 59 4d  53 58 20 00 02 02 01 00  |...SONYMSX .....|
```
8-hex-digit zero-padded byte offset, `: `, 16 bytes as lowercase 2-hex each (space-separated, an
extra space after the 8th byte), `  |` + 16-char ASCII gutter (printable `0x20..0x7E` verbatim,
else `.`) + `|`. A trailing short line pads the hex columns with spaces so the gutter aligns.
Lowercase hex + fixed grouping is fixed by spec so two dumps of identical bytes are byte-identical.

Exit codes (small, deterministic set):
| Code | Meaning |
| --- | --- |
| 0 | success (incl. `--help`) |
| 1 | usage / argument error (missing/duplicate mode, missing path, bad `--sector`/`--range`) |
| 2 | I/O error (cannot open/read/write; `--format`/`--read` target not found) |
| 3 | safety refusal (`--create` target exists w/o `--force`; `--format` wrong-size w/o `--force`) |

### 2.4 Module layout under `src/diskutils/` (namespace `sony_msx::diskutils`)
- `msx_disk_format.{h,cpp}` — geometry constants + `build_blank_image() -> std::vector<uint8_t>`
  (737,280 B, the §2.2 layout). Both `--create` and `--format` write this. Pure function of
  constants. Self-contained (does NOT include any emulator header).
- `hex_dump.{h,cpp}` — `write_hex_dump(std::ostream&, span<const uint8_t>, uint64_t base_offset)`
  deterministic formatter (§2.3).
- `cli.{h,cpp}` — `struct Args`, `parse_args(argc, argv) -> Args|error`, `run(const Args&,
  std::ostream& out, std::ostream& err) -> int` (returns the exit code; file I/O lives here). Taking
  streams makes `run()` unit-testable without spawning a process.
- `main.cpp` — thin `int main(int argc, char** argv){ return run(parse..., std::cout, std::cerr); }`.

---

## 3. Reuse-vs-Copy Decision (dependency direction is absolute)

**Decision: the tool is FULLY SELF-CONTAINED. It does NOT link `sony_msx_core` and does NOT
`#include` any emulator header. The shared disk-layout knowledge is re-expressed from the documented
spec (§2.2), NOT via a code dependency.** Rationale (this is the option that keeps the emulator suite
*provably* byte-identical, as REQ-M53-001 asks):
1. **Provable isolation.** A separate target with no shared translation units and no link edge to
   `sony_msx_core` cannot perturb the emulator's build or its 247 deterministic tests. Coupling the
   tool to the core (even in the *legal* tool→core direction) would add a rebuild/behavior-risk
   surface for zero benefit.
2. **Correctness forces it anyway.** `DiskImage::synthesize()` produces a NON-empty image (test
   pattern in the data area, `disk_image.cpp:34-39`); the tool needs the ZERO-filled blank of
   `format-blank-disk.ps1`. The tool cannot reuse `synthesize()` as-is — it needs a different
   (blank) layout — so there is no meaningful code to reuse.
3. **Dependency-direction rule (DEC-0080, R4).** The emulator NEVER depends on `src/diskutils/`
   (forbidden). The reverse (tool→core) is *permitted* but *declined* here for isolation.
4. **The layout facts are tiny public FAT12/hardware constants** (a ~40-value table), not
   license-sensitive work — independent re-expression is legitimate and carries no
   copy-into-`src/` concern.

**Belt-and-suspenders (proves machine-parity WITHOUT coupling the binary):** ONE unit test
(`diskutils_bpb_matches_machine_unit_test`, §6.2) links BOTH `msx_diskutil` AND `sony_msx_core` and
asserts the tool's boot-sector/BPB region (offsets 0..29, 510..511) and FAT-seed bytes
(LBA 1 / LBA 4, first 3 bytes) are byte-identical to `DiskImage::synthesize()`. Tests are allowed to
link both libraries (existing tests already link `sony_msx_core`); this proves the tool's output
matches the machine's OWN expectation at the byte level while the shipped `msx-disk` binary stays
core-free.

---

## 4. Acceptance Criteria

### 4.1 Milestone acceptance (all must hold)
- **AC-1 (three modes work):** `--create <p>` writes a 737,280-byte file matching §2.2 byte-for-byte;
  `--format <p>` on an existing 737,280-byte file yields the same bytes; `--read <p>` emits the §2.3
  hex dump to stdout; `--help` prints usage (exit 0).
- **AC-2 (byte-exact + deterministic):** `--create` output SHA256 == `tools/format-blank-disk.ps1`
  default output SHA256 (documented, developer-run once); two `--create` runs are byte-identical.
- **AC-3 (machine-compatible):** the tool's boot-sector/BPB/FAT-seed bytes equal
  `DiskImage::synthesize()`'s (unit test §6.2); the geometry fields (0x0B..0x1D) equal
  `disks/msxdos23.dsk`'s where valid (skip-when-absent; A2), divergences documented.
- **AC-4 (emulator mounts it):** the emulator's OWN FDC/DiskDrive headlessly mounts a tool-CREATED
  and a tool-FORMATTED image, reads LBA 0/1/7 back through the real sector datapath, and the FAT12
  structure is valid (boot sig `55 AA`, media `F9`, FAT seed `F9 FF FF`, empty root directory ⇒
  directory-readable, zero files). This is the "mount + directory-readable" gate, NOT a DOS boot
  (§1.2; honest per the M28 finding that a full DOS boot is not reached headlessly in-budget).
- **AC-5 (safety):** `--create` refuses an existing target w/o `--force` (exit 3); `--format`
  refuses a wrong-size target w/o `--force` (exit 3); missing `--format`/`--read` target ⇒ exit 2;
  no/duplicate mode or bad `--sector`/`--range` ⇒ exit 1.
- **AC-6 (isolation):** `git diff` over `src/core`, `src/devices`, `src/machine`, `src/frontend` is
  EMPTY; the emulator's pre-M53 247 fast-subset tests are name/outcome byte-identical; the new
  binary lands at `diskutils/msx-disk.exe`.
- **AC-7 (structure):** `src/CLAUDE.md` gains the `src/diskutils/` responsibility line (§6.4);
  `.gitignore` covers the binary (§6.5); the target builds in the standard
  `cmake --build build --config Debug` flow with no effect on existing targets.

### 4.2 Per-slice acceptance — see §6 (each slice lists AC-Sn).

---

## 5. Milestones (template)

- Milestone ID: M53
- Objective: §Objective above.
- Included Scope: §1.1.
- Excluded Scope: §1.2.
- Dependencies: `tools/format-blank-disk.ps1`, `src/devices/fdc/disk_image.{h,cpp}` (read-only
  reference), root `CMakeLists.txt`, `tests/CMakeLists.txt`, owner-created root `diskutils/` (A1).
- Interfaces Affected: NEW `src/diskutils/*`; root `CMakeLists.txt` (additive target + POST_BUILD);
  `tests/CMakeLists.txt` (additive test targets); `.gitignore`; `src/CLAUDE.md`. ZERO emulator
  behavior interfaces.
- Acceptance Criteria: §4.
- Unit Test Plan: §6.1-6.5 unit tests.
- System Integration Test Plan: §6.6 FDC-mount integration test.
- Regression Impact: none intended; enforced by AC-6 empty-diff + 247 byte-identity.
- Exit Criteria: AC-1..AC-7 met, QA sign-off recommending Pass, owner user-test (NORMAL
  human-release gate; no auto-close).

---

## 6. Slice Decomposition, CMake/Structure Mechanics, and Tests

### Slice S1 — Isolated target scaffold + CMake post-build copy + structure files
Establish the build-isolated tool skeleton and prove byte-identity BEFORE adding layout logic
(de-risks R1 early).
- Add `src/diskutils/{msx_disk_format,hex_dump,cli}.{h,cpp}` + `main.cpp` as stubs; `cli::run`
  handles `--help` / no-args (usage → exit 1) only.
- **CMake wiring (root `CMakeLists.txt`, ADD AFTER the `sony_msx_headless` block ~line 118, OUTSIDE
  the `if(SONY_MSX_ENABLE_SDL3)` guard so it always builds):**
  ```cmake
  # --- M53 (DEC-0080): standalone host-side MSX disk utility --------------------
  # Build-ISOLATED: msx_diskutil links NOTHING from sony_msx_core and sony_msx_core
  # never links it (one-way isolation). Additive only; zero effect on the emulator
  # targets or the deterministic suite.
  add_library(msx_diskutil STATIC
      src/diskutils/msx_disk_format.cpp
      src/diskutils/hex_dump.cpp
      src/diskutils/cli.cpp
  )
  target_include_directories(msx_diskutil PUBLIC src)

  add_executable(msx_disk src/diskutils/main.cpp)
  target_link_libraries(msx_disk PRIVATE msx_diskutil)
  set_target_properties(msx_disk PROPERTIES OUTPUT_NAME msx-disk)

  # Post-build: copy the ONE build-tree binary into the owner-created root
  # diskutils/ (DEC-0041: no second build tree). make_directory guards a fresh
  # clone / lean checkout where the folder is absent (R7).
  add_custom_command(TARGET msx_disk POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/diskutils"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "$<TARGET_FILE:msx_disk>" "${CMAKE_SOURCE_DIR}/diskutils/"
      VERBATIM)
  ```
  `$<TARGET_FILE:msx_disk>` resolves the platform suffix (`msx-disk.exe` on Windows, `msx-disk`
  elsewhere), so the copy is cross-platform.
- **`.gitignore` (append, after the build-output block):**
  ```
  # M53 (DEC-0080): the msx-disk host tool is a build product, post-build-copied
  # from the ONE build/ tree; ignore the binary, keep the folder scaffolding.
  # (The global *.exe rule already covers the .exe; the no-extension line covers
  # the POSIX binary and is listed for discoverability.)
  /diskutils/msx-disk
  /diskutils/msx-disk.exe
  ```
  Add a tracked `diskutils/.gitkeep` so the folder persists in git (A1).
- **`src/CLAUDE.md` responsibility line** (§6.4 below).
- **AC-S1:** build all targets (both emulator exes + `msx_diskutil` + `msx_disk`) green in the
  standard SDL3=ON flow; `diskutils/msx-disk.exe` present after build; `msx-disk` (no args) prints
  usage → exit 1; full ctest 247/247 pre-M53 byte-identical; `git diff` over the four emulator src
  trees EMPTY.

### Slice S2 — `--create` blank-FAT12 writer
- Implement `build_blank_image()` (§2.2) + `--create <out> [--force]` file write + overwrite guard.
- Tests: **6.1** `diskutils_blank_layout_unit_test`, **6.2** `diskutils_bpb_matches_machine_unit_test`.
- **AC-S2:** `--create` writes 737,280 B; every §2.2 byte oracle passes; two runs byte-identical;
  SHA256 == `format-blank-disk.ps1` default (AC-2, documented); overwrite refused w/o `--force`
  (exit 3), allowed with it; BPB/boot/FAT-seed == `DiskImage::synthesize()`.

### Slice S3 — `--format` in place
- Implement `--format <disk> [--force]`: size guard (exists + exactly 737,280 B, else refuse w/o
  `--force`) then overwrite with `build_blank_image()`.
- Test: **6.3** `diskutils_create_format_roundtrip_unit_test`.
- **AC-S3:** format of a 737,280-B file yields bytes identical to `--create` output (round-trip);
  `format(garbage-filled 737,280-B image)` == blank; wrong-size target refused w/o `--force`
  (exit 3); missing target ⇒ exit 2.

### Slice S4 — `--read` deterministic hex dump
- Implement `write_hex_dump()` + `--read <disk> [--sector N | --range A-B]` (§2.3).
- Test: **6.4** `diskutils_hexdump_unit_test`, **6.5** `diskutils_cli_validation_unit_test`.
- **AC-S4:** hex-dump of a known buffer equals the golden string exactly (offset + hex + gutter);
  two dumps byte-identical; `--sector N` slices the right 512 B; `--range A-B` correct; missing file
  ⇒ exit 2; bad range/sector ⇒ exit 1.

### Slice S5 — emulator-FDC integration cross-check + evidence gates
- Test: **6.6** `machine_diskutil_fdc_mount_integration_test`.
- Run all §7 evidence gates; produce the implementation report.
- **AC-S5:** integration test green (create + format image both mount + read back valid FAT12);
  full ctest with pre-M53 247 byte-identical + new tool tests green; validate-assets + checksums
  unchanged; AC-6 empty-diff confirmed.

### 6.1 `diskutils_blank_layout_unit_test` (unit; links `msx_diskutil`)
Asserts, on `build_blank_image()`: size == 737,280; `[0..2]==EB FE 90`; `[3..10]=="SONYMSX "`;
every BPB field of §2.2 (0x0B..0x1D); `[510..511]==55 AA`; `[0x1E..0x1FD]` all `00`; FAT copy #1
(LBA 1, offset 512) and #2 (LBA 4, offset 2048) first 3 bytes `F9 FF FF` and the remainder of each
FAT sector `00`; root dir (LBA 7..13, offsets 3584..7167) all `00`; data area (LBA 14.., offset
7168..) all `00`. Determinism: two `build_blank_image()` calls compare equal; SHA256 stable.

### 6.2 `diskutils_bpb_matches_machine_unit_test` (unit; links `msx_diskutil` + `sony_msx_core`)
`auto tool = build_blank_image(); auto mach = DiskImage::synthesize();` assert
`tool[i]==mach[i]` for `i in [0..29] ∪ {510,511}` and for the FAT-seed bytes at offsets
512..514 and 2048..2050. This proves the tool's boot sector/BPB/FAT match the machine's OWN
tested expectation (§3 belt-and-suspenders). OPTIONAL (A2, skip-when-absent): read
`SONY_MSX_DISKS_DIR/msxdos23.dsk` and assert the geometry subset (0x0B..0x0C, 0x15, 0x13..0x14,
0x16..0x17, 0x18..0x19, 0x1A..0x1B) matches; if the untracked disk is absent, print SKIP + return 0
(pattern: `hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp:150-156`); wire
`SONY_MSX_DISKS_DIR` via `target_compile_definitions` (pattern:
`tests/CMakeLists.txt:89-91` with `SONY_MSX_DISKS_DIR_ABS = ${CMAKE_SOURCE_DIR}/disks`).

### 6.3 `diskutils_create_format_roundtrip_unit_test` (unit; links `msx_diskutil`)
Write a temp file pre-filled with a non-zero pattern (737,280 B) → run the `--format` code path on
it → read back → assert == `build_blank_image()`. Also `create` then `format` on the same path is
idempotent (bytes unchanged). Uses `std::filesystem::temp_directory_path()` +
`std::filesystem::remove`, mirroring `hbf1xv_m36_disk_save_persist_integration_test.cpp:46-50,63-65`.

### 6.4 `diskutils_hexdump_unit_test` (unit; links `msx_diskutil`)
Feed a fixed small byte vector (e.g. the 16-byte boot-sector prefix) to `write_hex_dump()` into a
`std::ostringstream`; assert the exact golden string (offset, hex grouping, ASCII gutter, short-line
padding). Determinism: two calls produce identical output. `--sector`/`--range` slice math tested
against a synthetic full image.

### 6.5 `diskutils_cli_validation_unit_test` (unit; links `msx_diskutil`)
Drive `parse_args` + `run` with argv arrays into string streams (no subprocess): no mode → exit 1;
`--create --read` → exit 1; `--create <existing>` w/o `--force` → exit 3; `--create <existing>
--force` → exit 0; `--format <missing>` → exit 2; `--format <wrongsize>` w/o `--force` → exit 3;
`--read <missing>` → exit 2; `--read <p> --sector 9999` → exit 1; `--help` → exit 0. Temp files via
`std::filesystem`.

### 6.6 `machine_diskutil_fdc_mount_integration_test` (integration; links `msx_diskutil` + `sony_msx_core`)
Reuses the proven M16 FDC harness + the M36/M28 `DiskImage(bytes)`+`attach_image` mount pattern:
1. `auto bytes = build_blank_image();` construct `Hbf1xvMachine m; m.cold_boot();
   m.disk_image() = DiskImage(bytes); m.disk_drive().attach_image(&m.disk_image());` (pattern:
   `hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp:104-111`).
2. Read LBA 0 back **through the real FDC datapath** using the Restore + Read-Sector(LBA 0) CPU
   probe of `hbf1xv_m16_fdc_integration_test.cpp:94-167` (or, minimally, `read_chs(0,0,1,...)` which
   the same test uses at line 156 as its oracle); assert the streamed 512 B == `bytes[0..511]`
   (`EB FE 90 ... F9 ... 55 AA`).
3. Directly `read_chs` LBA 1 (FAT) → first 3 bytes `F9 FF FF`, rest `00`; LBA 7 (root dir) → all
   `00` (empty directory = directory-readable, zero files). This is the honest AC-4 "mount +
   directory-readable" oracle, explicitly NOT a DOS/BASIC boot.
4. Repeat 1-3 for a tool-FORMATTED image (write `bytes` to a temp file, run the `--format` path on
   it, reload). Determinism: run twice, streamed bytes byte-identical.
- **Test registration** (`tests/CMakeLists.txt`, pattern `:459-463`): `add_executable` +
  `target_link_libraries(... PRIVATE sony_msx_core msx_diskutil)` +
  `target_compile_definitions(... PRIVATE SONY_MSX_DISKS_DIR=...)` (only if the optional msxdos23
  cross-check is included) + `add_test`.

### 6.4 (structure) `src/CLAUDE.md` responsibility line
Add under "Strict folder responsibilities":
> - `src/diskutils/` — standalone HOST-side disk utilities (the `msx-disk` create/read/format tool
>   producing a byte-exact 720 KB MSX-DOS FAT12 `.dsk`). Builds a SEPARATE binary; it MUST NOT
>   depend on emulator runtime code and the emulator MUST NOT depend on it (one-way build isolation,
>   DEC-0080).

Add under "Boundary rules":
> - `src/diskutils/` is build-isolated: it neither links `sony_msx_core` nor is linked by it. Shared
>   disk-layout facts are re-expressed from the documented spec, never via a code dependency.

### Test count
Pre-M53 fast subset: **247** (+1 optional ZEXALL). New: **5 unit + 1 integration = 6** →
new total ≈ **253**. Exact final count is the developer's to confirm; the 247 pre-M53 tests MUST be
name-for-name and outcome-for-outcome unchanged (AC-6).

---

## 7. Evidence Gates (run and capture; never fabricate)
- **Build:** `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=ON` then
  `cmake --build build --config Debug` — both emulator exes + `msx_diskutil` + `msx_disk` green;
  `diskutils/msx-disk.exe` present after the POST_BUILD copy.
- **ctest:** `ctest --test-dir build -C Debug --output-on-failure` — pre-M53 247 byte-identical,
  new tool tests green (~253 total).
- **Assets:** `tools/validate-assets.ps1` PASS + `tools/checksum-assets.ps1 -OutFile
  docs/asset-checksums.txt` unchanged (the tool touches no `bios/`/`roms/`).
- **Isolation accuracy gate (M50-style empty-diff):** `git diff` over `src/core`, `src/devices`,
  `src/machine`, `src/frontend` is EMPTY — the change touches ONLY `src/diskutils/*`,
  `CMakeLists.txt`, `tests/CMakeLists.txt`, `.gitignore`, `src/CLAUDE.md`, `diskutils/.gitkeep`,
  docs.
- **openMSX A/B: N/A** (host-side file utility, no device behavior) — DEC-0080.
- **ZEXALL: N/A** (zero cpu/core diff) — DEC-0080.

---

## 8. Risks and Mitigations
- **R1 (DEC-0080) — CMake/global-flag side effects perturb the 247 suite.** Mitigation: additive,
  build-isolated target; no edit to `sony_msx_core` sources/flags; the target inherits only the
  pre-existing global `/utf-8` (`CMakeLists.txt:29-31`); full-ctest byte-identity + empty-diff gate.
- **R2 (DEC-0080) — wrong FAT12 / boot-sector layout.** Mitigation: triple-grounded layout (§2.1,
  all three tiers AGREE); byte-oracle unit test vs the machine's own `synthesize()` (§6.2); the
  msxdos23 geometry cross-check; the emulator-FDC mount integration test (§6.6); SHA256 equality vs
  the M41-A/B-validated `format-blank-disk.ps1` blank (AC-2).
- **R3 (DEC-0080) — in-place `--format` data destruction.** Mitigation: `--format` requires an
  existing, exactly-737,280-B target (else refuse, exit 3) unless explicit `--force`; `--create`
  refuses overwrite w/o `--force`; no wildcard/recursive operations; validated in §6.5.
- **R4 (DEC-0080) — scope creep into emulator internals.** Mitigation: one-way dependency rule (tool
  never linked by core; tool doesn't link core, §3); `src/CLAUDE.md` boundary line; AC-6 empty-diff.
- **R5 (planner) — planner could not independently hexdump `msxdos23.dsk`** (binary; no shell).
  Mitigation: developer derives its BPB golden at implementation time; PRIMARY machine-parity oracle
  is the in-repo, tested `DiskImage::synthesize()` (§6.2), not the untracked disk; the msxdos23
  cross-check is skip-when-absent (A2).
- **R6 (planner) — a full Disk-BASIC `FILES` prompt is not reliably reached headlessly in-budget**
  (the M28 finding: the disk-ROM window may never page in within a bounded run,
  `hbf1xv_m28_c5_disk_boot_investigation_system_test.cpp:203-215`). Mitigation: AC-4's gate is
  FDC-level sector mount + FAT12-structure readback (deterministic, §6.6), explicitly NOT a DOS/BASIC
  boot; stated honestly in §1.2.
- **R7 (planner) — POST_BUILD copy fails if `diskutils/` is absent** on a fresh/lean clone.
  Mitigation: POST_BUILD `cmake -E make_directory` before the copy; tracked `diskutils/.gitkeep`.
- **R8 (planner) — non-Windows binary naming.** Mitigation: `OUTPUT_NAME msx-disk` +
  `$<TARGET_FILE:msx_disk>` (CMake resolves the suffix); `.gitignore` covers both names.

---

## 9. Developer Handoff
- **Start at Slice S1** (isolation scaffold + CMake + `.gitignore` + `src/CLAUDE.md` + `.gitkeep`),
  and prove the 247 byte-identity + empty-diff BEFORE any layout logic — this de-risks R1 first.
- Then S2 (`--create` writer + §6.1/§6.2 oracles), S3 (`--format` + §6.3), S4 (`--read` + §6.4/§6.5),
  S5 (§6.6 integration + all §7 gates).
- **Source of truth for bytes:** copy the layout from `tools/format-blank-disk.ps1:82-133`
  (zero-filled data area — NOT `synthesize()`'s test pattern). Cross-check boot/BPB/FAT against
  `src/devices/fdc/disk_image.cpp:43-85`.
- **Reuse patterns, do not reinvent:** DiskImage mount = `DiskImage(bytes)` + `attach_image`
  (`hbf1xv_m28_c5_..._system_test.cpp:104-111`); FDC read-back probe =
  `hbf1xv_m16_fdc_integration_test.cpp:94-167`; temp-file discipline =
  `hbf1xv_m36_disk_save_persist_integration_test.cpp:46-65`; test registration =
  `tests/CMakeLists.txt:459-463` (+ `:89-91` for a compile-def path).
- **Do NOT** `#include` any emulator header from the shipped tool sources or link `sony_msx_core`
  into `msx_diskutil`/`msx_disk` (only the two cross-oracle TESTS may link both).
- **Verify A1/A2/A4/A5** as listed in §1.3 during S1/S2.
- **Deliverables:** the tool sources + tests + the CMake/`.gitignore`/`src/CLAUDE.md`/`.gitkeep`
  edits; an implementation report `docs/m53-implementation-report.md` with the §7 evidence
  (build log, ctest counts pre/post, empty-diff proof, `--create` SHA256 vs the PS1 blank, and the
  `git diff` scope). NORMAL human-release gate — no auto-close; QA sign-off required before owner
  user-test.

---
Planner package path: `docs/m53-planner-package.md`.
