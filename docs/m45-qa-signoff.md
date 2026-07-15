# M45 QA Sign-off — DEF-M45-WRITEDRQ (WD2793 disk-write data-integrity fix)

- Milestone ID: **M45** (DEF-M45-WRITEDRQ, DEC-0067)
- Commit under test: **`4a8f8e0`** (`fix(fdc): WD2793 write DRQ edge-handshake + first-byte gate`)
- Tag target: **v1.1.1** (CRITICAL data-integrity fix)
- QA agent: msx-qa | Date: 2026-07-12
- Verdict: **CONDITIONAL PASS** (release-gate condition: owner live re-save-and-load of YS II)

All gates below were independently re-executed from a **from-scratch `build/`** (deleted +
reconfigured). Nothing is inherited from the developer's tree. Environmental note: SDL3 is not
installed on this host (no `SDL3Config.cmake` anywhere on `C:`), so the sanctioned headless-only
fallback (`-DSONY_MSX_ENABLE_SDL3=OFF`, same `build/` tree per CLAUDE.md) was used. The M45 change
is entirely in `src/devices/fdc/` (core/headless) — every M45-relevant test runs headless.

---

## 1. Hard-gate results

| Gate | Result | Evidence |
| --- | --- | --- |
| Clean rebuild (from-scratch `build/`) | **PASS** | `rm -rf build` → reconfigure → `cmake --build build --config Debug` exit 0, 0 compiler errors. `sony_msx_headless.exe` built. **`sony_msx_sdl3.exe` NOT built — SDL3 unavailable in this env (sanctioned fallback).** |
| `ctest -LE m24_slow_full_sweep` | **PASS 206/206** headless. NOT 220/220. | The 14-test delta vs the developer's 220 is the **SDL3-gated frontend tests** (not built in headless config). None are FDC/disk-write tests. All 13 M45-relevant FDC/disk tests present and green. |
| Commit hygiene — src scope | **PASS** | `git show --name-only 4a8f8e0` = exactly `src/devices/fdc/wd2793.cpp` + `wd2793.h` (2 files, +68/−15). No other src touched. |
| Commit hygiene — zero cpu/core | **PASS** | `git diff --name-only v1.0.40..HEAD -- src/devices/cpu src/core` = **EMPTY** → Z80 core byte-identical since last clean ZEXALL sweep → ZEXALL withheld-substitution justified. |
| Commit hygiene — no AI trailer | **PASS** | `git show -s 4a8f8e0` message has no `Co-Authored-By`/`Claude`/`Generated with` trailer. (A naive `-i AI` grep only hits body substrings "t**ai**l", "/W**AI**T".) |
| Asset gate | **PASS** | `tools/validate-assets.ps1` → `Asset validation result: True` (7 BIOS + 5 ROMs). `tools/checksum-assets.ps1` → `docs/asset-checksums.txt` regenerated (SHA256). |

Note on the test file / registration: `tests/`, `docs/`, `debug/`, `tools/` are **gitignored**
(DEC-0044; untracked in `71b71e1`). The new `wd2793_write_stall_unit_test.cpp`, its CMakeLists
registration, and this sign-off doc are therefore **local-only** — which is why `4a8f8e0` is
correctly src-only and the tree is clean. This matches the established M42/M43 pattern.

---

## 2. Non-tautology + forensic-match verdict (the whole point)

**Non-tautology — PROVEN by adversarial revert (non-destructive per DEC-0049):** backed up
`wd2793.{cpp,h}` → replaced with the pre-M45 parent (`4a8f8e0^`) content → rebuilt core + test →
ran → restored from backup → confirmed `git diff` **empty** (byte-identical to HEAD) → rebuilt →
re-ran green.

- **Fixed code (HEAD):** `wd2793_write_stall_unit_test` — **all cases pass**, byte-perfect in
  BOTH `--fast-disk` modes.
- **Reverted (pre-M45) code:** **FAILS** with the exact DEC-0067 signature, in **accurate mode
  only** (fast mode stayed clean → confirms the old bug was `!fast_disk_`-gated):
  - Case 1 (12000-cycle mid-transfer stall @ byte 130): **spurious_zeros = 105**,
    first_mismatch = 130, mismatches = 382.
  - Case 2 (steady drift, +300 cyc/byte): **spurious_zeros = 316**, first_mismatch = 1.
  - Case 3 (multi-sector 0xB0 stall @ byte 700): spurious_zeros = 78 + **INTRQ never raised**
    (truncated/early-finished command).

The test genuinely detects the bug; it is not a tautology.

**Forensic match — CONFIRMED (independent re-analysis).** I compared the human's corrupt
`disks/games/ys2/disk2.dsk` against pristine `disks/games/ys2-pristine/ys2-d2.dsk` byte-for-byte:

- `disk1.dsk` == pristine `ys2-d1.dsk` (0 differing sectors) → no cross-disk / CHS / drive-select
  bug, confirming DEC-0067's forensics.
- `disk2.dsk` differs in **exactly 4 sectors**: **LBA 9, 10, 11, 16** (C0/H1/S1,S2,S3,S8 — the
  save area), with total zeros/sector = **98, 110, 98, 99** (96–108 injected over non-zero
  pristine bytes) and near-whole-sector byte divergence (502–507/512) = the "dense zeros +
  shifted survivors + truncated tail" signature.

The adversarial revert's **105 spurious zeros** on a 12000-cycle stall lands squarely inside the
real disk2's **98–110 zeros/sector** band. **This is the proof the fix targets the actual
corruption mechanism** — the accurate-mode absolute-deadline zero-substitution the fix removed.

---

## 3. openMSX write A/B — ATTEMPTED, BLOCKED (harness/environment), NOT tooling-faked

openMSX **is** available in WSL (`/usr/bin/openmsx`, v19.1, Ubuntu-24.04) with a custom
`HBF1XV512` machine — so this is **not** a hard tooling block. I made **four genuine attempts** to
drive a known-payload BSAVE (payload `(7*i+10)&255`, header FE/C000/C1FF/C003 — byte-matching our
`pat2`) to a fresh formatted `-diska` and compare the written `.dsk`:

1. Developer's `bsave2.tcl` keymatrix + blank `-diska` → openMSX `DI; HALT` at boot, **0 bytes
   written**.
2/3. Robust `type`-command BASIC entry, disk-insert-after-boot, both with and without `-diska` at
   power-on → **`DI; HALT` at boot, 0 bytes written** in both.
4. VRAM/`puts` diagnostics to confirm the BASIC prompt → no retrievable headless output; minimal
   `puts` did not emit.

Root obstacle: `HBF1XV512` reaches `DI; HALT` when booted **interactively** to Disk BASIC in this
session (it booted fine for M42/M37 which loaded a non-interactive Z80 probe, not interactive
BASIC), and openMSX headless script stdout is not retrievable here. This is the same wall the fix
agent hit; the developer's `bsave2.tcl` arm was never validated. **I did not fabricate an A/B.**

**Release-confidence judgment (honest):** the missing openMSX write A/B is **acceptable for
release** here, because — unlike the M37 read-timing A/B, which had a genuine raw-track-vs-logical
framing divergence — a `.dsk` is a **flat logical-sector** image. A well-behaved write of payload
P produces `sector == P` in ANY correct WD2793 model **by construction** (no gap/CRC/address-mark
framing sits between the CPU bytes and the on-disk logical sector). So an openMSX write A/B would
only re-confirm what the following ALREADY establish independently:
(a) the forensic-signature match (§2), (b) the source-mirror grounding (§6), and (c) the real
within-emulator payload write landing byte-perfect and mode-independent (§4). The remaining
end-to-end assurance is better obtained from the **owner's live YS II re-save-and-load** (§8), which
is the real-world equivalent and directly closes the DEC-0067 origin.

---

## 4. BIOS-write byte-perfect check (strict superset for the working case) — PASS

The developer's `--debug-session` BSAVE fixtures **do** land real payloads (the earlier
"doesn't land a payload" was a superseded attempt). Independently re-verified:

- **512-byte BSAVE** (`pat2`, header `FE 00C0 FFC1 03C0` + 512-byte body) → lands at **LBA 14
  offset 0** in `bsave2-normal.dsk`, full 519 bytes byte-exact.
- **2048-byte multi-sector BSAVE** (`patM`) → lands at **LBA 14**, full 2055 bytes byte-exact.
- **Mode independence:** `bsave2-normal.dsk` **== BYTE-IDENTICAL ==** `bsave2-fast.dsk`;
  `bsaveM-normal.dsk` **==** `bsaveM-fast.dsk`; `basic-normal.dsk` **==** `basic-fast.dsk`.

This is a **real payload** BIOS-path write (BASIC BSAVE → DSKIO → sony_fdc → WD2793), byte-perfect
and identical with/without `--fast-disk`. The working case is a strict superset.

---

## 5. No-regression FDC/disk verdict — PASS (one item env-blocked)

| Oracle | # | Result |
| --- | --- | --- |
| devices_fdc_disk_image_unit_test | 70 | PASS |
| devices_fdc_wd2793_type1..4_unit_test | 71–74 | PASS |
| devices_fdc_sony_fdc_unit_test | 75 | PASS |
| devices_fdc_wd2793_fastdisk_unit_test | 76 | PASS |
| **devices_fdc_wd2793_write_stall_unit_test (NEW)** | 77 | PASS |
| machine_hbf1xv_m16_fdc_integration_test | 78 | PASS |
| machine_hbf1xv_m16_boot_checkpoint_integration_test | 79 | PASS |
| devices_disk_image_writeback_unit_test | 162 | PASS |
| machine_hbf1xv_m36_disk_save_persist_integration_test | 164 | PASS |
| hbf1xv_m28_c5_disk_boot_investigation_system_test | 181 | PASS |
| **frontend_sdl3_app_multi_disk_integration_test** | — | **NOT RUN — SDL3-gated, not built in headless env.** |

`multi_disk` is a frontend hot-**swap** test (not a write-path test); the write path it would
exercise indirectly is covered by disk_image_writeback + m36_disk_save_persist (both green). Low
residual risk; recommend a confirmatory SDL3=ON run on a host that has SDL3 before final tag.

---

## 6. Source-mirror grounding (read, not copied) — verified

The M45 FSM is grounded in `references/openmsx-21.0/src/fdc/WD2793.cc`:
- `setDataReg` (:235–247) — a CPU write during write-sector deasserts DRQ (`drqTime.reset(infinity)`);
  ours re-bases `drq_deadline_ = t + cycles_per_byte()` (DRQ won't re-assert until t+cadence). ✔
- `startWriteSector`/`checkStartWrite` (:646–701) — CHECK_WRITE scheduled 8 byte-periods after the
  first DRQ; abort + LOST_DATA writing NOTHING if the first byte is absent. Ours:
  `write_check_deadline_ = drq_deadline_ + 8*cycles_per_byte()`, aborted in `sync()` while
  `data_index_==0`; re-armed per sector via `finish_write_sector`→`begin_write_sector` (multi-sector
  case 3 passes byte-perfect, confirming the re-arm). ✔
- `/WAIT`: `PhilipsFDC.cc:99–100` — "IRQ and DRQ lines are not connected to Z80 interrupt request";
  our `sony_fdc.cpp:58–68` polls DRQ/IRQ via the `0x3FFF` status window → no `/WAIT` model needed. ✔
  (Minor: the commit message cites `0x7FFF`; the code path is `address & 0x3FFF` = `0x3FFF`.
  Cosmetic citation nit, mechanism correct.)

---

## 7. Failures and risk ranking

No blocker/critical failures. Residual observations:

- **[MEDIUM — accuracy, non-blocking] The fix is CPU-paced, not openMSX's scheduled-tick model.**
  openMSX's `writeSectorData` (:742–782) substitutes `0x00`+LOST_DATA on a genuinely-missed
  **scheduled** FSM tick (fixed disk byte-rate). The M45 implementation instead makes each disk
  byte follow the CPU write (re-based edge handshake) and **never substitutes mid-transfer** —
  LOST_DATA is confined to the first-byte CHECK_WRITE gate. Consequences: (a) strictly **safer**
  for data integrity (a merely-slow writer is preserved, never corrupted); (b) **invisible to all
  real software** (real saves use DSKIO with interrupts off + a tight OUTI/DRQ poll that never
  underruns); (c) architecturally consistent with our **logical-`.dsk`** model (no physical
  rotational byte-rate to enforce). It is a deliberate, defensible simplification that meets
  DEC-0067's data-integrity mandate, but it does **diverge from the letter** of the DEC-0067
  directive ("substitute … ONLY on a genuinely-missed scheduled tick"). Recommend documenting it
  and tracking a future full cycle-accurate write model (parallel to the deferred command-engine
  cycle-timing) as a non-blocking backlog item.
- **[LOW — coverage] openMSX write A/B not obtained** (§3) — mitigated by forensic match + source
  grounding + within-emulator byte-perfect mode-independence; recommend the owner live re-save test.
- **[LOW — coverage] `multi_disk` + SDL3 build not verified in-env** (SDL3 absent) — recommend one
  SDL3=ON `ctest` (expect 220/220) on an SDL3-capable host before the final tag.
- **[INFO] 206/206 not 220/220** — purely the SDL3-gated delta; not a regression.

## Required fixes

**None are blocking.** For the tag: owner live re-save-and-load of YS II (below). Recommended
follow-ups (non-blocking): confirm SDL3=ON 220/220 on an SDL3 host; document the CPU-paced
write-model deviation in a backlog row.

---

## 8. Sign-off decision — CONDITIONAL PASS → v1.1.1

**CONDITIONAL PASS.** Every hard gate is green from a from-scratch build; the non-tautology and
the forensic-signature match are definitive; the BIOS-write working case is byte-perfect and
mode-independent; all runnable FDC/disk oracles pass. The single condition is not a code defect —
it is the real-world end-to-end confirmation that substitutes for the (environment-blocked) openMSX
write A/B:

- **RELEASE CONDITION (should gate the tag): the owner performs a LIVE YS II in-game save on the
  fixed build (default accurate timing AND `--fast-disk`), then reloads the saved room and confirms
  no SP-underflow/crash.** This is the direct real-world closure of the DEC-0067 origin (the human's
  corrupt save) and is not headlessly reproducible.
- Recommended non-blocking: one SDL3=ON `ctest` (expect 220/220) on an SDL3-capable host.

**Release recommendation:** on the owner's live re-save-and-load passing, tag **v1.1.1** at
`4a8f8e0` and push (owner-run, per DEC-0047 — the assistant does not tag/push). Note for the owner:
the human's ALREADY-corrupted `disk2.dsk` is **not** deterministically recoverable (DEC-0067); the
fix prevents *future* corruption — restore pristine `ys2-d2.dsk` or re-save after the fix.

---

## 9. Proposed agent-protocol updates (for the coordinator to apply — QA does not edit the ledger)

**`agent-protocol/state/milestones.md` — new M45 block:**
> **M45 — DONE (QA CONDITIONAL PASS, DEC-0067, 2026-07-12; tag target v1.1.1 @ `4a8f8e0`,
> owner-run per DEC-0047).** CRITICAL WD2793 disk-write data-integrity fix (DEF-M45-WRITEDRQ) —
> the accurate-mode absolute-deadline zero-substitution that destroyed a real YS II save
> (disk2 LBA 9/10/11/16, 98–110 zeros/sector). Re-modeled to an openMSX-grounded DRQ edge
> handshake + one-byte pipeline (drq_deadline_ re-based on the CPU write, never accumulating) +
> a first-byte CHECK_WRITE gate (abort+LOST_DATA writing NOTHING → no all-zero sector); `!fast_disk_`
> zero-sub branch removed (fast = pure timing-scale). `src/devices/fdc/wd2793.{cpp,h}` only
> (+68/−15); zero cpu/core (ZEXALL withheld-substituted). QA (docs/m45-qa-signoff.md): from-scratch
> build, fast ctest **206/206** headless (SDL3 unavailable in-env; 220/220 SDL3=ON unverified),
> asset gate PASS. **Non-tautology PROVEN** (adversarial pre-M45 revert reproduces 105/316/78
> spurious zeros, accurate-mode-only) + **forensic match** (105-zero stall signature == real disk2
> 98–110 zeros). BIOS BSAVE byte-perfect + fast==accurate byte-identical (512B & 2048B payloads).
> openMSX write A/B ATTEMPTED but BLOCKED (HBF1XV512 DI;HALT interactive boot in-env) — release
> confidence rests on forensic + source grounding + owner live re-save gate. Residual (non-blocking):
> CPU-paced write model diverges from openMSX's scheduled-tick underrun (safer, invisible to real
> software); multi_disk/SDL3 build unverified in-env.

**`agent-protocol/state/current-phase.md` — Active Phase:**
> **M45 — DONE (QA CONDITIONAL PASS, DEC-0067, 2026-07-12; release target v1.1.1 @ `4a8f8e0`).**
> Condition: owner LIVE YS II re-save-and-load (accurate + --fast-disk) confirms no SP-underflow.
> Then owner tags v1.1.1 + pushes. NOTE: M44 Phase 1 (`a27a1c4`) + README/diagram commits remain
> unpushed ahead of v1.1.0; the v1.1.1 tag ships them together.

**`agent-protocol/channels/decisions.md` — close DEC-0067:**
> DEC-0067 RESOLVED (2026-07-12): fix `4a8f8e0` verified by M45 QA (CONDITIONAL PASS,
> docs/m45-qa-signoff.md). Non-tautology + forensic-signature match confirm the corruption
> mechanism is eliminated. Tag v1.1.1 gated on the owner's live YS II re-save-and-load.
