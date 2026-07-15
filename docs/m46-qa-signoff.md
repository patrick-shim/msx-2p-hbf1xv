# M46 / v1.1.2 — QA Sign-off

- Milestone ID: **M46** (commit `70db39f` on `main`; tag target v1.1.2, owner-run per DEC-0047)
- Decision of record: **DEC-0071** + its M46 test-cadence addendum (`agent-protocol/channels/decisions.md:857-874`)
- Planner package: `docs/m46-planner-package.md`
- QA Owner: MSX QA Agent. Independent re-verification on the developer's existing `build/` (incremental build only; no reconfigure / no clean rebuild — M45-incident guardrail honored).
- Baseline compared: `v1.1.1` = `a248198` → `HEAD` = `70db39f`.

## 1. Regression Scope

Frontend + a small amount of machine wiring, **zero device behavior change**:
`src/frontend/*` (CLI parse, resolver, banner, FM-PAC slot-2 auto-load) and the headless
`src/main.cpp --debug-session` load path. Diff `v1.1.1..HEAD` touches 11 files
(`CMakeLists.txt`, `src/frontend/sdl3_cli.{h,cpp}`, `src/frontend/sdl3_app.{h,cpp}`,
`src/frontend/sdl3_main.cpp`, `src/frontend/session_summary.{h,cpp}` [new],
`src/machine/cartridge_cli.{h,cpp}`, `src/main.cpp`), +609/-100. **`src/devices/cpu` and
`src/core` diffs are EMPTY** (AC-22, re-confirmed twice this session) → Z80/core byte-identical,
ZEXALL withheld-substituted per DEC-0071 (sweep correctly NOT run).

Regression matrix (per the DEC-0071-addendum trimmed gate):
CLI/launch-path parsing, session-defaults resolver, `--stock`/`--no-fast-disk`/`--no-fmpac`,
`--slotN` alias equivalence, FM-PAC slot-2 auto-load + SRAM + skip paths, banner label text,
RAM byte-identity oracles (the default-flip drift guard), cartridge/slot/mapper, FDC/disk/boot.

## 2. Regression Matrix Status

Gate re-run (exact addendum command):
`ctest --test-dir build -C Debug -R "frontend|machine|hbf1xv_m|cartridge|slot|mapper|_ram|fmpac|sram|fdc|disk|boot|cli|input_script|m4[0-9]" -E "zexall" --output-on-failure`

**Result: 136/143 passed, 7 failed** (matches the planner's ~136/143 expectation). Build:
both exes + all test binaries rebuilt clean (incremental), exit 0.

Key acceptance evidence (all GREEN):

| Area | AC | Evidence (test / source) |
|---|---|---|
| Resolver empty-CLI = 512 / fast-disk ON / FM-PAC ON | AC-1 | `sdl3_cli_unit_test` Case 19 (`Resolver_Empty_Ram512` etc.), concrete-value asserts |
| `--ram 64` / `--stock` = 64 KB byte-identity | AC-2 | Case 20 + sentinel `hbf1xv_ram_size_integration_test` `DefaultCtor_Is64KB_Stock` |
| `--stock` precedence specificity/order-independent | AC-3 | Case 21 (`--stock --ram 512` == `--ram 512 --stock` → 512) |
| fast-disk default ON | AC-4 | Case 22 (`Resolver_FastDiskDefaultOn`, `--no-fast-disk`→false, `--fast-disk`→true) |
| FM-PAC auto-load slot 2 + SRAM bound + slot 1 free | AC-5 | `sdl3_app_fmpac_autoload_integration_test` `Autoload_*` (ran real asset: roms/fmpac.rom present) |
| Coexist with slot-1 game | AC-6 | `Coexist_FmPacInSlot2` + `Coexist_Slot1GameLoaded` |
| Explicit slot 2 wins (no forced FM-PAC) | AC-7 | `ExplicitSlot2_NotForcedToFmPac` / outcome `SkippedSlot2InUse` |
| `--no-fmpac`/`--stock` → no FM-PAC, DEC-0050 SRAM correct | AC-8 | `StockDefault_NoSramPathBound_DEC0050` + `StockDefault_FlushIsNoOp` |
| Graceful absent/corrupt ROM (boot proceeds) | AC-9 | `AbsentRom_InitStillSucceeds_NeverFailsBoot`, `CorruptRom_*` (100-byte truncated) |
| Alias equivalence `--slotN`≡`--cartN` | AC-11 | `cartridge_cli_unit_test` (byte-identical path/type/`type_was_explicit`) |
| Alias collision last-wins | AC-12 | `--cart1 b --slot1 a`→a; `--slot1 a --cart1 b`→b |
| `--slotN-type auto` = non-explicit | AC-13 | `pauto` case, plus unrecognized-value parse error |
| Banner label text (RAM/mode/FM-PAC/SRAM) | AC-14 | `session_summary_unit_test` pins EXACT strings, non-tautology guard present |
| Baseline unweakened / anti-drift sentinel | AC-15 | Sentinel green + `sdl3_cli_session_integration_test` `Case4_DefaultConfig_StaysStock_AntiDriftSeam` |
| Tool `--stock` migration | AC-16 | 9/9 tools carry `--stock`; audio-A/B + boot-parity apply it to the real invocation |
| Empty cpu/core diff (ZEXALL substitute) | AC-22 | `git diff v1.1.1..HEAD -- src/devices/cpu src/core` EMPTY |

**Anti-drift seam (the core safety property) — CONFIRMED at three independent levels:**
1. `Sdl3AppConfig{}` struct field defaults stay stock: `ram_bytes = 64u*1024u` (`sdl3_app.h:177`),
   `fast_disk = false` (`:66`), `fmpac_autoload = false` (`:188`). Read from source.
2. `Hbf1xvMachine` ctor default stays 64 KB: sentinel `DefaultCtor_Is64KB_Stock` GREEN
   (`hbf1xv_ram_size_integration_test.cpp:110`, assertion unchanged in content).
3. `Case4_DefaultConfig_StaysStock_AntiDriftSeam` GREEN — a default config resolves to
   64 KB / no fast-disk / no auto-load. The flipped defaults live ONLY in
   `resolve_session_defaults()` (`sdl3_cli.cpp:250-271`), consumed verbatim by both
   `sdl3_main.cpp` and `main.cpp`.

**Resolver / FM-PAC / DEC-0050:** `resolve_session_defaults` matches planner §2.3 exactly
(explicit per-field > `--stock` > convenience; `--ram`/`fast_disk_opt` override `--stock`
order-independently). The slot-2 auto-load (`sdl3_app.cpp:120-168`, `main.cpp:1026-1069`) applies
the §2.5 gate order: slot-2-occupied → already-present → absent → invalid → load, each recording
a distinct `FmPacAutoloadOutcome` and NEVER failing boot; no non-load path ever fabricates
internal SRAM, so "NO S-RAM AVAILABLE" (DEC-0050) is preserved. Banner ↔ machine state is
authoritative (`loaded_slot` from `machine.fmpac(1/2)` takes precedence over the recorded outcome).
New tests use concrete-value asserts (not non-crash), including a `format_ram_line` stock≠modded
non-tautology guard.

## 3. Failures and Risk Ranking

**7 failures — ALL pre-existing asset-absence from the pre-M46 asset reorg, ZERO M46 code. Risk: Low (test-fixture, not product).**

Verified absent on this machine: `roms/aleste.rom`, `roms/metalgear.rom`, `disks/msxdos22.dsk`
(owner moved games to `games/roms/` and standardized on `disks/msxdos23.dsk`). Each failing test
aborts on an early asset-presence / file-open check, never inside M46 logic:

| # | Test | Failing case (first gate) | Missing asset |
|---|---|---|---|
| 99 | `machine_hbf1xv_m19_aleste_smoke_integration_test` | `AlesteRom_FileOpens` | `roms/aleste.rom` |
| 100 | `machine_hbf1xv_m19_metalgear_smoke_integration_test` | `MetalgearRom_FileOpens` | `roms/metalgear.rom` |
| 150 | `machine_hbf1xv_m27_debug_session_integration_test` | `DiskFlag_RealAssetFileOpens` | `disks/msxdos22.dsk` |
| 153 | `hbf1xv_m27_replay_determinism_system_test` | `AlesteRom_FileOpens_NonEmpty` | `roms/aleste.rom` |
| 184 | `hbf1xv_m28_c5_disk_boot_investigation_system_test` | `Msxdos22Dsk_RealAssetPresent_737280Bytes` (FATAL) | `disks/msxdos22.dsk` |
| 216 | `frontend_sdl3_cli_session_integration_test` | `RepresentativeFlags_Sdl3AppInitSucceeds` ("cannot open --cart1 file: …/roms/aleste.rom") | `roms/aleste.rom` |
| 221 | `frontend_sdl3_app_multi_disk_integration_test` | `BootSingleDisk_InitSuccess` (+9) — `init()` fails because `load_configured_assets` cannot open the disk | `disks/msxdos22.dsk` |

Corroboration that #221 is asset-absence, NOT an M46 `init()` regression: this test builds a
default `Sdl3AppConfig` (`fmpac_autoload = false` → no auto-load path taken); its Case-4
(`/nonexistent/path/disk.dsk`, which *expects* init to fail) still PASSES, and the dedicated
`sdl3_app_fmpac_autoload_integration_test` (#224) shows `init()` succeeding — including the real
FM-PAC auto-load. The failures are purely the missing `msxdos22.dsk` making
`load_configured_assets` return false.

No M46-authored test failed. No behavioral/parity regression observed.

## 4. Required Fixes

None blocking for M46. One **recommendation (local-only test hygiene, NOT part of M46 — do not
fix under this milestone):** the 7 tests hard-code the pre-reorg fixture paths
(`roms/aleste.rom`, `roms/metalgear.rom`, `disks/msxdos22.dsk`). `tests/` is gitignored/untracked,
so this is separate from the M46 source change. Recommended disposition (a follow-up housekeeping
task):
- Repoint to the current layout (`games/roms/…`, `disks/msxdos23.dsk`), **caveat:**
  `aleste.rom` → `games/roms/aleste2.rom` is a different/larger dump (2,097,152 bytes here) with a
  different SHA — any checksum/size assertion must be re-derived, not copied; and
  `games/roms/metalgear.rom` is ALSO absent on this machine, so the Metal Gear smoke test needs a
  present asset or a skip.
- OR convert each to skip-when-absent (the discipline `sdl3_app_fmpac_autoload_integration_test`
  and the `sdl3_cli_session` Case-3 already use), so absence is a SKIP, not a FAIL.

## 5. Sign-off Decision — **Conditional Pass**

The automated M46 surface is fully verified and GREEN: 136/143 (the 7 failures are pre-existing
asset-absence, itemized above, none M46 code); the anti-drift seam is proven at three levels; the
resolver, FM-PAC slot-2 auto-load/skip paths, DEC-0050 SRAM correctness, `--slotN` alias
equivalence/collision, and banner label text are all covered by non-tautological tests; the
cpu/core diff is empty (ZEXALL correctly withheld-substituted); and the tool `--stock` migration is
applied to all 9 launch tools (including the REQUIRED audio A/B `m41-typed-audio-ab.ps1` and the
boot-parity harness, applied to the real invocation, not a comment).

**Single outstanding condition — AC-10 interactive confirmation (owner-gated):** the FM-PAC
slot-2 discovery is BIOS-scan/headless-verified (auto-load into slot 2 + SRAM bind proven by
`sdl3_app_fmpac_autoload_integration_test`), but the **interactive `CALL FMPAC` manager UI + an
in-game SRAM save** cannot be confirmed headlessly. The designated final acceptance gate is the
**owner's live SDL3 check: enriched banner render + `CALL FMPAC` from slot 2 + an in-game SRAM
save/reload**. Per the task framing this is owner-gated, not a Fail.

### Sign-off Conditions
1. **(Owner-gated, AC-10)** Live SDL3 run confirms: the enriched "This session" banner renders
   (convenience + `--stock` + fmpac-absent states), `CALL FMPAC` reaches the manager from slot 2,
   and an in-game SRAM save persists to `roms/fmpac.rom.sram` and reloads. On failure → escalate
   (planner §6.4 fallback: auto-load into slot 1 or fix slot-2 decode = a new decision).
2. **(Optional, conditional openMSX A/B, AC-21)** A single migrated `--stock` A/B re-run
   (`m41-typed-audio-ab.ps1` or `openmsx-m28-c5-boot-parity.ps1`) reproducing an empty/parity diff
   would fully close AC-16/21; given the empty cpu/core diff and byte-identical stock config it is
   corroborative, not blocking for QA.
3. **(Housekeeping, out-of-band)** Address the 7 stale asset-path fixtures per §4 (separate from
   M46; `tests/` is untracked).

- Risk Ranking: **Low** (no device/timing change; empty cpu/core diff; the only failures are
  known missing local fixtures).
- Blocking Issues: **None.**

### Assumptions
- **Assumption:** the sentinel `hbf1xv_ram_size_integration_test.cpp` is "UNCHANGED" cannot be
  git-verified because `tests/` is gitignored/untracked. *Verification performed:* read the file —
  `DefaultCtor_Is64KB_Stock` (line 110) asserts `dram_size() == 64 KB` with the expected value, and
  the test is GREEN. The substantive property (ctor default stays stock) holds.
- **Assumption:** `roms/fmpac.rom` (present, 65,536 bytes, `PAC2OPLL` signature) is a valid FM-PAC
  image so the auto-load integration test exercised the real-asset AC-5/6/7 cases. *Verification:*
  test #224 passed in 2.90 s (the real-asset branch, not the stock-only fast path) and the
  `have_fmpac` header/size guard is in the source.
