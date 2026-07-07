# M20 QA Sign-off — Halnote / MSX-JE Firmware Mapping (Slot 0-3) + Real BatteryBackedSram Consumer

- Milestone ID: M20
- Request: QA regression assessment (RESP-M20-002 equivalent cycle)
- Planner package: `docs/m20-planner-package.md`
- Implementation report: `docs/m20-implementation-report.md`
- QA Owner: MSX QA Agent
- Gate: NORMAL human-release-decision gate (no auto-close, DEC-0003's auto-close grant is M12-only).
  This sign-off is a recommendation to the coordinator; the actual release decision + tag remains
  a separate human action.

---

## 1. Regression Scope

M20 adds one new device family (`src/devices/halnote/halnote_rom.{h,cpp}`, `HalnoteRom`) composing
two already-shipped, QA-signed-off primitives — `devices::cartridge::CartridgeRomWindow` (M19) and
`devices::memory::BatteryBackedSram` (M17) — and wires it into `Hbf1xvMachine` at primary slot 0,
secondary slot 3, all 4 CPU pages (previously reserved/open-bus). This is the first cross-family
reuse (`src/devices/halnote/` depending on `src/devices/cartridge/`), and the first time slot 0
gains a second populated secondary slot alongside the M13 BIOS ROM at slot 0-0.

Regression surfaces exercised:
- **New device correctness**: main 8-slot bank window (banks 2-5), SRAM-enable gate wired to a real
  16 KB store (0x0000-0x3FFF), sub-mapper-enable gate + 2 sub-bank registers + the 0x7000-0x7FFF
  shadow-read override (with the 0x6000-0x6FFF non-shadow boundary), permanently-unmapped
  window-slots 6/7 (0xC000-0xFFFF).
- **Machine wiring regression**: the M13 BIOS ROM at slot 0-0 (secondaries 0/1/2 of primary slot 0
  untouched) — this is the sharpest regression risk since slot 0 now has a second populated
  secondary for the first time.
- **Cross-milestone reuse regression**: `CartridgeRomWindow` (M19) and `BatteryBackedSram` (M17)
  behavior must be unchanged — verified by inspection (no edits to either file) and by the full
  M1-M19 suite remaining green.
- **Determinism / CPU-timing-oracle regression**: `HalnoteRom` is a pure combinational device; the
  M9/M12 CPU-timing oracle suites must remain unaffected.
- **Backlog-ledger regression**: full 34-row deferred-backlog re-affirmation, with B4 and B6 closing
  together per the human's explicit directive.
- **A/B parity regression**: real openMSX evidence for the main-bank-switch protocol, SRAM
  enable/content, and the sub-bank shadow quirk specifically.

---

## 2. Regression Matrix Status

| # | Area | Verification performed | Result |
|---|------|------------------------|--------|
| 1 | Full build + test suite | `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF`; `cmake --build build --config Debug`; `ctest --test-dir build -C Debug --output-on-failure` — re-run by QA independently, real captured output (below). | **PASS — 95/95, zero regression vs. M19's 92/92 baseline** |
| 2 | Byte-exact semantics vs. `references/openmsx-21.0/src/memory/RomHalnote.{hh,cc}` | Read the reference file directly (not the implementation's own comments): confirmed `reset()` (`:48-61`), `readMem`/`getReadCacheLine` (`:63-78`), `writeMem` (`:80-126`) line-by-line against `src/devices/halnote/halnote_rom.cpp`. Confirmed the bit-0x80 double-duty ordering (`setRom` called unconditionally BEFORE the enable-bit branch, `:100-123` vs. `halnote_rom.cpp:97-102`); the sub-bank shadow's exact `0x7000<=address<0x8000` range (`:65` vs. `halnote_rom.cpp:59-63`, never `0x6000`); window-slots 6/7 permanent unmap (`:59-60,87` vs. `halnote_rom.cpp:82-84`); sub-bank writes unconditional regardless of enable (`:88-97` vs. `halnote_rom.cpp:85-89`); no `setBlockMask` call anywhere (confirmed by full-file read — matches `CartridgeRomWindow`'s default `block_mask_ = num_blocks_-1 = 127`). | **PASS — byte-exact, independently confirmed against the actual reference source, not just the developer's citations** |
| 3 | Genuine reuse, not reimplementation | Read `src/devices/cartridge/cartridge_rom_window.h` and `src/devices/halnote/halnote_rom.{h,cpp}` in full: `HalnoteRom` calls only `window_.set_bank/set_unmapped/read/image/num_blocks/block_mask` and `sram_.read/write/clear/load/save` — no duplicate bank-resolution or SRAM address-decode math inside `halnote_rom.cpp`. `CartridgeRomWindow` has no user-declared destructor/copy/move members (implicit move-assignable, satisfying `set_image()`'s reconstruct-wholesale need). | **PASS** |
| 4 | BIOS ROM regression guard (M13, slot 0-0) | Read `tests/integration/machine/hbf1xv_m20_halnote_integration_test.cpp` Case 3: samples 64 bytes of slot 0-0 before/after heavy Halnote traffic (bank-switch + SRAM + sub-bank writes), asserts byte-for-byte equality, AND asserts the sample is non-degenerate (not all-same-byte) — a genuine, non-vacuous guard. | **PASS** |
| 5 | Slot-routing arithmetic in M20's own tests | Independently re-derived from `src/devices/chipset/slot_bus.cpp` (`primary_for_page`, `sub_for_page`, `write_ffff`): confirmed primary slot 0 is expanded (`wire_bus()`'s `set_expanded(0, true)`, present since M13), `primary_select_` defaults to 0 after `reset()`/`ppi_.io_write(0xA8,0)`, so `primary_for_page(3)==0`; `debug_bus_write(0xFFFF, 0xFF)` therefore sets `sub_slot_register_[0]=0xFF`, and since `expanded_[0]==true`, every page's 2-bit field independently decodes to secondary slot 3 — exactly what the M20 tests assert via `slot_expanded(0)`/`debug_sub_slot_register(0)` before relying on it (Case 1). This is the correct, independently-recomputed routing, not a repeat of the DEC-0016-class bug (which involved primary slot 3, a different primary, and a `#A8` write that only set ONE page's field while leaving page 3's field at the wrong primary). | **PASS — routing independently re-verified, no DEC-0016-class defect present** |
| 6 | A/B parity divergence disposition | See §3 below — independently re-run the actual harness (not just read the artifact) and additionally re-derived the divergent bytes' origin at the byte level. | **Genuine, disclosed, non-blocking — see independent analysis below** |
| 7 | Deferred-backlog review | Read `docs/m20-planner-package.md` §4 in full (34 rows: B1-B9, C1-C10, D1-D7, E1-E2, F1-F2, G1-G4) and `agent-protocol/state/deferred-backlog.md` in full. Row count independently verified: 9+10+7+2+2+4=34. B4/B6 marked "CLOSES this cycle, together" with correct justification (real `BatteryBackedSram` wired as `HalnoteRom`'s SRAM store; `HalnoteRom` wired at the real slot reading the real firmware); all other 32 rows re-affirmed with a one-line justification each, none silently dropped or incorrectly touched. Note: the backlog ledger file itself still shows B4/B6 as OPEN at the time of this review — this matches the established project pattern (the coordinator updates the ledger to `DONE (Mxx)` only at actual milestone closure, per every prior cycle's own bottom-of-file closure entries, e.g. M19/B7), not a defect. | **PASS** |
| 8 | SRAM persistence (two independent instances) | Read `tests/integration/machine/hbf1xv_m20_halnote_integration_test.cpp` Case 5: `writer` and `reader` are declared in SEPARATE brace scopes — genuinely two independent `Hbf1xvMachine` objects, not reused state within one instance's lifetime. Case 6 confirms unset-path deterministic zero after a SECOND `cold_boot()` on the SAME instance (no cross-boot carry-over). | **PASS** |
| 9 | Determinism (no clock consumption) | Grepped `src/devices/halnote/halnote_rom.{h,cpp}` for `elapsed_cycles`/`Scheduler`/`clock` — zero hits except a doc comment stating the device does NOT consume them. `core::MemoryDevice`'s interface (`src/core/device_contracts.h:35-41`) exposes only `mem_read`/`mem_write`, no time parameter — structurally incapable of clock-coupling. `ctest` reconfirms the M9/M12 CPU-timing oracle suites (`hbf1xv_cpu_step`, `cpu_parity`, etc.) remain green. | **PASS** |
| 10 | Real ROM asset present/correct | Independently computed SHA256 of `bios/f1xvfirm.rom` via PowerShell `Get-FileHash`: `C78C13996A406EE69C2A6B988C53A68B649AFFABEF6E4599E6EA507D5387F044`, size `1,048,576` bytes — matches `docs/asset-checksums.txt:12` exactly. | **PASS** |

### Full evidence-gate output (QA's own run, this cycle)

```
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF
  Configuring done, generating done — no errors.

cmake --build build --config Debug
  Build succeeded (sony_msx_headless.exe + all test executables), no errors.

ctest --test-dir build -C Debug --output-on-failure
  ...
  93/95 Test #93: devices_halnote_rom_unit_test ............... Passed  0.03 sec
  94/95 Test #94: devices_halnote_subbank_unit_test ............ Passed  0.02 sec
  95/95 Test #95: machine_hbf1xv_m20_halnote_integration_test ... Passed  0.44 sec
  100% tests passed, 0 tests failed out of 95
  Total Test time (real) = 2.82 sec
```

This independently reproduces the developer's claimed 95/95 (92 prior + 3 new), zero regression.

---

## 3. Failures and Risk Ranking

No blocking failures found. One residual, disclosed, non-blocking finding, ranked below.

### R-QA-1 (non-blocking): openMSX 19.1 sub-mapper-shadow-read divergence — independently re-investigated, root cause narrowed further than the developer's own report

The implementation report and `docs/m20-parity-trace-diff.md` disclose 11/14 PARITY, 3/14
DIVERGENCE, all three isolated to the sub-mapper-shadow read (`SUBBANK0_SHADOW`,
`SUBBANK0_SHADOW_LAST`, `SUBBANK1_SHADOW`), with a disclosed-but-unconfirmed "version skew (openMSX
19.1 installed vs. 21.0 grounding reference)" hypothesis.

**QA independently re-ran the full harness** (`tools/openmsx-m20-halnote-parity.ps1`, live WSL
openMSX 19.1, real `bios/f1xvfirm.rom` on both sides) rather than trusting the artifact file, and
reproduced the IDENTICAL result byte-for-byte:

```
BANK4_BASE                   emulator=22 openMSX=22 expected=22 [PARITY]
BANK4_LAST                   emulator=FF openMSX=FF expected=FF [PARITY]
BANK5_BASE                   emulator=16 openMSX=16 expected=16 [PARITY]
BANK2_BASE_DOUBLE_DUTY       emulator=1C openMSX=1C expected=1C [PARITY]
SRAM_FIRST                   emulator=5A openMSX=5A expected=5A [PARITY]
SRAM_LAST                    emulator=A5 openMSX=A5 expected=A5 [PARITY]
BANK3_BASE_DOUBLE_DUTY       emulator=00 openMSX=00 expected=00 [PARITY]
BANK3_LAST_BEFORE_SHADOW     emulator=32 openMSX=32 expected=32 [PARITY]
SUBBANK0_SHADOW              emulator=20 openMSX=60 expected=20 [DIVERGENCE]
SUBBANK0_SHADOW_LAST         emulator=FF openMSX=58 expected=FF [DIVERGENCE]
SUBBANK1_SHADOW              emulator=30 openMSX=2A expected=30 [DIVERGENCE]
SUBBANK1_SHADOW_LAST         emulator=FF openMSX=FF expected=FF [PARITY]
UPPERQUARTER_BEFORE_WRITE    emulator=FF openMSX=FF expected=FF [PARITY]
UPPERQUARTER_AFTER_WRITE     emulator=FF openMSX=FF expected=FF [PARITY]
RESULT: 11/14 labels PARITY, 3 DIVERGENCE.
```

QA also independently confirmed (via `wsl sha1sum`) that the actual WSL-installed
`~/.openMSX/share/systemroms/hb-f1xv.rom` has SHA1 `ade0c5ba5574f8114d7079050317099b4519e88f` —
identical to the local `bios/f1xvfirm.rom` (also independently hashed by QA) — so both sides of the
probe are demonstrably running the real, identical firmware. This is genuine, reproducible evidence,
not a fabricated or stale artifact.

**QA went one step further than the developer's own report** to test the "version skew" hypothesis
more concretely: QA read the raw firmware bytes at the exact offsets a "shadow read disabled /
falls through to the plain bank-3 window" hypothesis would predict, i.e. `image[7*0x2000 + (address
& 0x1FFF)]` for `bank(3)==7` (the value written by the probe's `0x87` write to `0x6FFF`):

```
image[7*0x2000+0x1000] = 0x60   <- exactly openMSX's SUBBANK0_SHADOW      (0x60)
image[7*0x2000+0x17FF] = 0x58   <- exactly openMSX's SUBBANK0_SHADOW_LAST (0x58)
image[7*0x2000+0x1800] = 0x2A   <- exactly openMSX's SUBBANK1_SHADOW      (0x2A)
image[7*0x2000+0x1FFF] = 0xFF   <- exactly openMSX's SUBBANK1_SHADOW_LAST (0xFF, already PARITY)
```

This is a decisive, independently-derived confirmation: **openMSX 19.1's actual behavior at all
three divergent addresses is bit-for-bit identical to a plain, non-shadowed read of window-slot 3's
resolved bank (bank 7)** — i.e., on the installed openMSX 19.1 runtime, the sub-mapper shadow
mechanism is simply not being engaged for `debug read memory` at 0x7000-0x7FFF at this point,
despite the bank-number portion of the identical write demonstrably taking effect (confirmed by the
`BANK3_BASE_DOUBLE_DUTY`/`BANK3_LAST_BEFORE_SHADOW` PARITY rows, which reflect bank 7's data
correctly on both sides). This is consistent with either a genuine 19.1-vs-21.0 upstream behavior
difference in the shadow feature, or an openMSX 19.1-specific `debug read memory` cache-line
staleness quirk for this narrow case — either way, it is a property of the REFERENCE runtime's
response to this probe's access pattern, not of this project's `HalnoteRom`.

**QA's independent judgment: this DIVERGENCE is non-blocking and should not gate sign-off**, for
three independently-verified reasons:
1. This project's own `HalnoteRom` implementation is byte-exact against `references/openmsx-21.0/
   src/memory/RomHalnote.cc` — QA read the reference source directly (§2 above) and confirmed the
   shadow-read gating logic (`getReadCacheLine:65`, `subMapperEnabled && 0x7000<=address<0x8000`) is
   ported without modification into `halnote_rom.cpp:59-68`.
2. The dedicated unit tests (`halnote_subbank_unit_test.cpp`) exhaustively, deterministically prove
   the shadow mechanism works correctly in this project's own code — including the exact
   0x6000/0x7000 boundary the planner flagged as the milestone's subtlest risk (R-M20-2) — with no
   dependency on any external runtime.
3. The divergence is isolated to exactly the three sub-bank-shadow-read labels and reproduces
   deterministically; the main-bank-switch (incl. the bit-0x80 double-duty bank-number effect) and
   SRAM enable/read/write (incl. cross-run-persisted real openMSX SRAM content) are genuine PARITY —
   demonstrating the probe technique and CPU-halt discipline are sound, and the divergence is not a
   symptom of a broader methodology flaw.

No fix is required under M20. Recommended disposition: accept as a disclosed, non-blocking A/B
residual, consistent with this project's established openMSX-version-skew handling precedent
(mirrors the M16 C5 residual and M18's disclosed status-bit divergence). If a newer openMSX build
becomes available in a future environment, re-running this probe would be a low-cost, valuable
confirmatory action — not a blocking requirement.

---

## 4. Required Fixes

None required for sign-off. No blocker-level gaps found.

---

## 5. Sign-off Decision (Pass | Conditional Pass | Fail)

**PASS.**

Basis: full evidence-gate suite independently re-executed by QA (build, ctest 95/95 zero regression,
asset checksum cross-check); byte-exact semantics independently re-verified against the actual
`RomHalnote.{hh,cc}` reference source (not just the developer's citations); genuine reuse of
`CartridgeRomWindow`/`BatteryBackedSram` confirmed with no parallel reimplementation; BIOS ROM
regression guard confirmed non-vacuous; slot-routing arithmetic in the new tests independently
recomputed and confirmed free of the DEC-0016-class bug; SRAM persistence confirmed to genuinely
round-trip across two independent machine instances; determinism (no clock coupling) confirmed by
direct inspection; full 34-row deferred-backlog review confirmed complete and accurate, with B4 and
B6 correctly disposed to close together. The disclosed A/B DIVERGENCE (3/14 labels, sub-mapper-shadow
read only) was independently reproduced by QA via a live re-run of the harness and further narrowed,
via independent byte-level analysis of the raw firmware content, to a reference-runtime-side
behavior (plain, non-shadowed bank-3 read) rather than any demonstrated defect in this project's own
`HalnoteRom` — QA's own independent judgment concurs with the developer's disposition that this is a
non-blocking, disclosed residual, on stronger grounds than originally presented.

Recommend: proceed to the separate human release decision for M20 closure (mark B4 and B6
`DONE (M20)` in `agent-protocol/state/deferred-backlog.md` at that time, per the established
ledger-update-at-closure pattern); tag per the human's standard process.
