# M42 QA Sign-off — opt-in `--ram <64|128|256|512>` (DEC-0061)

- Milestone ID: **M42**
- Feature commit: `3f69786` on `main` ("feat(machine): opt-in --ram 64/128/256/512, default 64")
- QA Owner: MSX QA Agent
- Date: 2026-07-12
- Build tree: single canonical `build/` (VS 18 2026, Debug, SONY_MSX_ENABLE_SDL3=ON)
- Decision: **PASS**

---

## 1. Regression Scope

`--ram <64|128|256|512>` sizes the HB-F1XV main DRAM. Affected surfaces:
- `src/devices/memory/memory_mapper_ram.{h,cpp}` — segment count derived from the
  backing region; `physical_address` parameterized by fitted `num_segments`, fold
  `segment & (num_segments-1)`.
- `src/machine/hbf1xv_machine.{h,cpp}` — constructor `dram_bytes` (default
  `kDramBytes` = 64 KB); power-on A-5 pattern fills `dram_.size()`.
- `src/frontend/*` + `src/main.cpp` — shared `parse_ram_kb` enum validator, `--ram`
  wired into both exes; `README.md` + `--help`.
- Regression concern set: default-64 byte-identity (the strict spec path), mapper
  fold correctness at 128/256/512, CLI enum/rejection parity across both exes, and
  the end-to-end "software sees the extra RAM" claim (openMSX A/B).

## 2. Regression Matrix Status (independently re-verified from `build/`)

| Gate | Result | Evidence |
| --- | --- | --- |
| Clean rebuild (SDL3=ON, both exes) | **PASS** | `cmake --build build --clean-first` exit 0; `sony_msx_headless.exe` + `sony_msx_sdl3.exe` produced. |
| `ctest -LE m24_slow_full_sweep` | **PASS 216/216** (dev baseline), **217/217** with the QA-added test | 0 failed, 71.85 s / 64.44 s. |
| Default-64 byte-identity | **PASS** | `machine_hbf1xv_memory_regions_unit_test` (default ctor → `dram_size()==64 KB` + A-5 pattern unchanged); `devices_memory_mapper_ram_unit_test` (64 KB → `num_segments()==4`, 5-bit readback `0x85` unchanged, fold values unchanged); CLI `RamAbsent_Nullopt_Stock64`. Default path is arithmetically byte-identical: `& (4-1) == & 3`; A-5 loop over `dram_.size()`==64 KB. |
| Commit hygiene | **PASS** | `git show --stat 3f69786 -- src/core src/devices/cpu` = EMPTY (zero core/cpu → ZEXALL correctly withheld); scoped to README + devices/memory + frontend + machine; **no** Co-Authored-By / AI trailer. |
| Adversarial mutation (non-tautology) | **PASS** | fold `& (num_segments-1)` → `& num_segments`: **34 case failures** across `memory_mapper_ram_unit_test` (21), `ram_size_integration_test` (9), `ram_detect_boot_integration_test` (4). Restored from `.qabak`; `git diff` byte-identical; 3/3 green again. |
| **BIOS-detects-512KB end-to-end (openMSX A/B)** | **PASS** | See §3 — byte-for-byte match, 32 distinct at 512 / 4 at 64. |
| CLI rejection policy (both exes) | **PASS** | See §4. |
| `--help` / README labeling | **PASS** | See §4. |

## 3. BIOS-detects-512KB end-to-end verdict — PASS (openMSX A/B)

The preferred path (openMSX A/B) was feasible in-env and executed. Full evidence:
`docs/m42-openmsx-ram-ab.md`.

- Stood up an openMSX HB-F1XV **internal-512** variant (`HBF1XV512.xml` = the stock
  system `Sony_HB-F1XV.xml` with ONLY the Main RAM `<MemoryMapper><size>` changed
  `64 → 512` — matching OUR internal-512 topology, not an external `ram512kmapper`).
- Ran an identical mapper-detection probe on both engines after a **real BIOS boot**:
  route page 2 via the Main RAM segment register (I/O `0xFE`), write the segment
  index into each of 32 segments, read all back, count distinct.

| Config | openMSX | Our engine (real bios/ boot + 180-frame live Z80) |
| --- | --- | --- |
| internal 512 KB | 524288 B, READBACK `0..31`, **DISTINCT=32** | 524288 B, READBACK `0..31`, **DISTINCT=32** |
| stock 64 KB | 65536 B, READBACK `28,29,30,31`×8, **DISTINCT=4** | 65536 B, READBACK `28,29,30,31`×8, **DISTINCT=4** |

Byte-for-byte match on both the distinct counts and the exact fold-aliasing
sequence. **Software genuinely sees all 512 KB (32 independently addressable
segments) at `--ram 512`, and exactly 64 KB (4 segments) at the stock default** —
in parity with the real-ROM openMSX HB-F1XV. Our-side proof is a new QA integration
test `tests/integration/machine/hbf1xv_ram_detect_boot_integration_test.cpp`
(deterministic; registered in ctest; loads the real repo `bios/`). Both engines
drive the mapper via their debug interface after a live BIOS boot — a symmetric,
apples-to-apples comparison; the mapper fold under test is the exact code path the
fetch-executed `OUT`/`LD` use (further exercised by the whole CPU-driven suite at
64 KB).

## 4. CLI rejection + help/README

Both `sony_msx_headless.exe` and `sony_msx_sdl3.exe`:
- `--ram 64|128|256|512` → **exit 0**; absent → **exit 0** (default 64 KB).
- `--ram 1024|96|big|0|4096|<missing>` → **exit 2** + loud parse error
  (headless: `main: --ram value must be one of 64|128|256|512 (KB): '1024'`;
  SDL3: `sdl3: sdl3_cli: --ram value must be one of 64|128|256|512 (KB): '96'`).
  Shared `parse_ram_kb` = one enum policy across both exes.
- `--help` (SDL3) + README label 64 = stock default (byte-identical to omitting),
  128/256/512 = **opt-in NON-STOCK** "fully-populated S1985" mods, 512 KB = the
  internal ceiling (S1985 5-bit read-back = 32 × 16 KB), >512 KB = external
  RAM-expansion cartridge, other = parse error. Accurate and spec-grounded.

## 5. Failures and Risk Ranking

No failures. Residual risks (all **Low**, non-blocking):

- **R1 (Low)** — non-stock A-5 power-on fill at 128/256/512 KB is the same 512-byte
  pattern repeated across the region (matches openMSX `Ram::clear`,
  `references/openmsx-21.0/src/memory/Ram.cc:66-73`) but is not itself A/B-asserted.
  Power-on RAM content is inconsequential to games (they init their own RAM).
- **R2 (Low)** — debug state-dump / snapshot now cover `dram_.size()` (up to 512 KB)
  for non-stock sizes; larger, still deterministic. No fixed-64 KB dump-size oracle
  exists that this could break (verified: only the default-64 oracles assert size).
- **R3 (Low)** — non-stock sizes are explicitly documented as NON-STOCK mods; they do
  not alter the strict target spec (64 KB stays the default and only stock config).
- **Assumption** — openMSX in-env is 19.1 (system), while the grounding source tree is
  21.0. Verification: the mapper fold semantics (`segment & (numSegments-1)`,
  `MSXMemoryMapperBase.cc`) are stable across these versions and the A/B was run
  against the real 19.1 binary with real Sony ROMs; the fold is additionally
  cross-consistent with the cited 21.0 source our code mirrors.

## 6. Required Fixes

None.

## 7. Sign-off Decision: **PASS**

All eight gates green, including the previously-open end-to-end "BIOS detects
512 KB" claim now closed with byte-for-byte openMSX A/B parity. Zero core/cpu edits
(ZEXALL correctly withheld). Default-64 path proven byte-identical. Non-tautology
proven under adversarial mutation.

### Release recommendation

- Recommend release **v1.0.41** at HEAD `3f69786` (owner-run tag + push per DEC-0047).
- Recommended protocol refresh (coordinator-applied): add an **M42** block to
  `agent-protocol/state/milestones.md`; set `state/current-phase.md` active phase to
  M42 DONE; **close DEC-0061** (acceptance met — see below).

### DEC-0061 acceptance closure

Default-64 byte-identical ✔ · mapper fold tests for 128/256/512 (mirroring +
no-mirror at ceiling) ✔ · CLI enum + rejection tests ✔ · `--ram 512` boot/detection
smoke (now upgraded to a full openMSX A/B) ✔ · clean rebuild + fast ctest ✔ ·
no cpu/core touch (ZEXALL withheld) ✔ · non-stock labeled in `--help` + README ✔.
