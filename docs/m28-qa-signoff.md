# M28 QA Sign-off — Release Candidate: Backlog Closure Sweep + Full-Project Health Audit
## (combined with DEC-0026: VDP S#2 VR/HR boot-hang fix, landed in the same closure commit)

- Milestone ID: M28
- QA Owner: MSX QA Agent
- Dispatch: REQ-M28-003 (`agent-protocol/channels/requests.md:1462-1478`)
- Planner package: `docs/m28-planner-package.md`
- Implementation report: `docs/m28-implementation-report.md`
- Health audit: `docs/m28-release-candidate-audit.md`
- C5 parity trace: `docs/m28-parity-trace-diff.md`
- DEC-0026 bug-fix report: `docs/vdp-vr-hr-boot-hang-fix-report.md`
- DEC-0026 decision record: `agent-protocol/channels/decisions.md:235-241`

All findings below were independently reproduced this cycle from a clean rebuild — nothing is a
restatement of the developer's or coordinator's transcripts without direct re-verification. Two
separate changesets land in the same closure commit and are reviewed together per REQ-M28-003:
**(A)** M28's own approved scope (E2, C5, health audit) and **(B)** DEC-0026 (VDP VR/HR fix,
outside M28's approved scope, landed afterward in the same tree).

---

## 1. Regression Scope

**(A) M28**: E2 (YM2413 register-write timing gate, `src/devices/audio/ym2413_opll.{h,cpp}`,
default-off), C5 (full-boot/auto-disk-boot-trigger investigation, outcome (b) — honest,
non-forced negative finding, not a failure), and the four-part release-candidate health audit
(`docs/m28-release-candidate-audit.md`).

**(B) DEC-0026**: a real bug fix to the already-shipped M14 `V9958Vdp` device
(`src/devices/video/v9958_vdp.{h,cpp}`) — S#2 status bits VR (bit6)/HR (bit5) were hardcoded to a
fixed 0, causing the real BIOS's VDP-init wait-for-toggle poll to hang forever (permanent black
screen on a real SDL3 boot). Fixed via a new, optional, pull-style `VdpClockSource` (X-pattern of
the existing `RtcClockSource`/`FdcClockSource`/`Ym2413ClockSource` adapters), wired additively in
`src/machine/hbf1xv_machine.{h,cpp}`.

Regression surface reviewed: the full M1-M27 suite plus all M28/DEC-0026 new tests (both build
configurations touched are exercised via the primary headless configuration, which is the one this
cycle's mandatory full-unfiltered-`ctest` gate targets), `git diff v1.0.27` across the entire
touched-file set, the 34-row deferred-backlog registry, and a direct, independently-authored
before/after reproduction of DEC-0026's headline claim (§5).

## 2. Regression Matrix Status

| # | Check | Method | Result |
|---|---|---|---|
| 1 | Clean rebuild (headless) | `cmake -S . -B build-qa -DSONY_MSX_ENABLE_SDL3=OFF && cmake --build build-qa --config Debug` (fresh directory, not reused from any prior agent's build) | Succeeds, zero errors (only benign CP949-encoding warnings on non-ASCII comment characters, pre-existing across the whole tree) |
| 2 | **Full, unfiltered `ctest`** (mandatory per `feedback_slow_test_cadence.md`'s mechanical gate — confirmed fired: `git diff v1.0.27 --name-only -- src/devices/ src/peripherals/ src/core/` shows touches to `src/devices/audio/ym2413_opll.{h,cpp}` and `src/devices/video/v9958_vdp.{h,cpp}`) | `ctest --test-dir build-qa -C Debug --output-on-failure` | **146/146 passed, 0 failed.** `hbf1xv_m24_zexall_system_test` Passed, 1498.52 sec. `Total Test time (real) = 1515.76 sec`. |
| 3 | ZEXALL/ZEXDOC hard assertions | `build-qa/Testing/Temporary/LastTest.log` read directly (not merely trusted from the ctest summary line) | `ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` / `ZEXDOC:` identical — **exact match** to the developer's and coordinator's independently-claimed figures |
| 4 | E2 regression pre-check (R-M28-1) | `rg "0x7C\|0x7D\|#7C\|#7D"` on `tests/unit/devices/audio_ym2413_opll_unit_test.cpp` and `tests/integration/machine/hbf1xv_m17_ym2413_integration_test.cpp`; `git diff v1.0.27 --stat` on both files | Both files confirmed to contain the exact back-to-back write patterns the developer's report describes (direct `debug_io_write()` 0-cycle-apart pokes in the integration test's Case 2); `git diff v1.0.27` on both files is **empty** — byte-for-byte unmodified, confirming the gate's default-off resolution avoided any test edit |
| 5 | E2 unit/integration tests | `ctest --test-dir build-qa -C Debug -R "ym2413_write_timing"` | **2/2 passed** (`devices_audio_ym2413_write_timing_unit_test`, `machine_hbf1xv_m28_ym2413_write_timing_integration_test`) |
| 6 | C5 internal diagnostic | `ctest --test-dir build-qa -C Debug -R "hbf1xv_m28_c5_disk_boot_investigation_system_test"` + direct binary invocation to capture stdout | **1/1 passed.** Core finding reconfirmed: `disk_rom_page1_ever_selected=0` (with and without scripted SPACE), `max_pc=0xff5c` — **exact match**. See §4 for a material finding on the `final_pc` figure specifically. |
| 7 | C5 openMSX A/B parity | `tools/openmsx-m28-c5-boot-parity.ps1 -Emulator build-qa/Debug/sony_msx_headless.exe -WorkDir build-qa` against real WSL `/usr/bin/openmsx` (openMSX 19.1, confirmed present) | **ARCHITECTURAL PARITY over 3000 instructions** — empty diff, reproduced independently against the current, post-DEC-0026 tree |
| 8 | Health audit — placeholder-file finding | `rg "device_placeholder\|peripheral_placeholder\|DevicePlaceholder\|PeripheralPlaceholder"` | 7 hits, all historical/doc-only (`agent-protocol/`, `docs/m28-*`) — zero remaining `src/` references, confirming clean removal |
| 9 | Health audit — source inventory | `find src -name "*.h" \| wc -l` / `*.cpp` | 75 headers / 65 sources — exact match to the audit's own re-enumeration |
| 10 | Health audit — CMake glob risk (R-M28-7) | `grep -n "GLOB" CMakeLists.txt tests/CMakeLists.txt` | Zero hits — confirmed explicit source lists throughout, removal was safe |
| 11 | Health audit — untracked TODO/STUB sweep | `rg -i "TODO\|FIXME\|STUB\|not implemented\|NotImplemented" src/` | 7 hits, individually read — all historical/grounding prose, zero untracked gaps, matches the audit's own accounting |
| 12 | Health audit — executable launches | `build-qa/Debug/sony_msx_headless.exe` (bare + `--debug-session ... --disk ...`); `build/sdl3-on/Debug/sony_msx_sdl3.exe --disk ... --hidden-window --max-frames 30` (SDL_VIDEODRIVER=dummy) | Both launch cleanly with real BIOS/disk assets, produce the expected `debug-session:`/frontend banner output and real trace/state artifacts under `debug/traces/` |
| 13 | Asset validation | `tools/validate-assets.ps1` | `Asset validation result: True` — 7 BIOS files, 2 ROM files, matches audit |
| 14 | New VDP VR/HR tests | `ctest --test-dir build-qa -C Debug -R "vdp_vr_hr"` | **2/2 passed** (`devices_vdp_vr_hr_raster_status_unit_test`, `machine_hbf1xv_vdp_vr_hr_raster_integration_test`) |
| 15 | M24 CP/M isolation test (masked-bits fix) | Direct read of `git diff v1.0.27 -- tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp` | Confirmed passed within the 146/146 full run; scope judged sound — see §6 |
| 16 | CPU/core zero-touch (hard, zero-tolerance) | `git diff v1.0.27 --stat -- src/devices/cpu/ src/core/` | **Empty** — zero touch across both changesets, confirmed |
| 17 | Full touched-file inventory | `git diff v1.0.27 --name-only` | `src/devices/audio/ym2413_opll.{h,cpp}` (E2), `src/devices/video/v9958_vdp.{h,cpp}` (DEC-0026), `src/machine/hbf1xv_machine.{h,cpp}` (both, additive wiring only), two placeholder-file deletions, `tests/CMakeLists.txt` + new test files, one test-file edit (`hbf1xv_m24_cpm_run_integration_test.cpp`), `README.md`, and `agent-protocol/`/`docs/` ledger files — no file outside this inventory |
| 18 | DEC-0026 headline claim | Direct, independently-authored before/after reproduction — see §5 | **Directly confirmed**: pre-fix code hangs (VDP registers stay all-zero through 500 real CPU-driven frames); post-fix code configures the VDP (R#0-R#8 genuinely non-zero from frame 74 onward) |

## 3. C5 / E2 / Health-Audit Findings (M28's own approved scope)

- **E2**: gate mechanism independently verified against the cited 12/84-master-cycle constants
  (`references/fact-sheets/Yamaha YM2413 FM Chip.md` §8); defaults off, wiring is a behavioral
  no-op for every existing caller (confirmed via the unmodified M17 test files, check #4 above).
  A/B disposition of N/A for the drop-behavior comparison is correctly, honestly framed (openMSX
  disables this emulation by design) — not fabricated as PARITY or DIVERGENCE.
- **C5**: outcome (b) is genuine, not force-closed. The core, headline finding — the disk-ROM
  window (primary 3, sub 2) is never paged into page 1 within 20,000,000 instructions, with or
  without a scripted keypress, using a real bootable `disks/msxdos22.dsk` — is independently
  reproduced exactly (check #6). The openMSX A/B architectural-parity claim over the early-boot
  window is also independently reproduced (check #7). See §4 below for one material, non-blocking
  finding on this row's secondary evidence.
- **Health audit**: all four parts' concrete command-level claims independently reconfirmed (checks
  #8-#13). The two applied fixes (placeholder-file removal, one stale `CassettePort` citation path)
  are both small, mechanical, and correctly within the fix-scope rule — no behavior-affecting change
  to any already-QA-signed device.

## 4. Finding — C5's `docs/m28-parity-trace-diff.md` Result 3 is now stale post-DEC-0026 (Medium, non-blocking to the core claim)

Independently re-running `hbf1xv_m28_c5_disk_boot_investigation_system_test` against the current,
post-DEC-0026 tree (check #6 above) reproduces the **core** finding exactly
(`disk_rom_page1_ever_selected=0`, `max_pc=0xff5c`), but the **`final_pc`** figure differs from what
is recorded in `docs/m28-parity-trace-diff.md` Result 3 and echoed in
`agent-protocol/state/deferred-backlog.md`'s C5 row:

```
Recorded (docs/m28-parity-trace-diff.md, pre-DEC-0026 capture): final_pc=0x2c03
QA reproduction (current tree, post-DEC-0026, two independent runs):   final_pc=0x7ff2 (both runs identical)
```

This is **not** a flakiness/nondeterminism bug — QA's own two consecutive runs against the current
binary are byte-identical (`0x7ff2` both times), consistent with the test's own claimed
determinism. The explanation is structural and independently confirmable: `docs/m28-parity-trace-diff.md`
and the M28 implementation report were captured **before** DEC-0026 landed (the implementation
report's own §4 closing note says so explicitly: "a SEPARATE, later coordinator-landed bug fix —
DEC-0026 ... required one further full unfiltered ctest re-run" — it does not mention re-running
the C5 trace/diagnostic). DEC-0026 is independently, directly proven by this QA cycle (§5) to
materially change real, CPU-driven boot behavior past the point where the BIOS's VDP-init
wait-for-toggle loop previously hung forever — a 20,000,000-instruction boot trajectory measured
*before* that fix is not the same trajectory measured *after* it. `docs/vdp-vr-hr-boot-hang-fix-report.md`
§4 itself already discloses that, post-fix, the boot proceeds substantially further and eventually
reaches a *different* kind of terminal state (a permanent HALT via stack corruption from an
unrelated, already-disclosed memory-mapper content divergence at instruction #145,503) rather than
the pre-fix "settles into a steady low-PC idle loop" — `0x7FF2` is consistent with this
already-disclosed downstream dead end, not a new, unexplained defect.

**Why this is not blocker-level**: the actual C5 acceptance-criterion finding — the disk-ROM window
is never mapped in — is unaffected and independently reconfirmed on the current, post-DEC-0026
code. C5's `IN-PROGRESS (M28 partial)`, outcome-(b) disposition is correct and unaffected.

**Why this still needs a fix before the tag closes**: `docs/m28-parity-trace-diff.md` and
`deferred-backlog.md`'s C5 row are both cited as this cycle's evidence artifacts, and their specific
secondary numbers (the `final_pc=0x2C03` "idle low-PC prompt loop" narrative, and the
`msxdos23.dsk` cross-check `final_pc=0x2BF7`) no longer match what the shipped code actually
produces. Leaving stale, now-inaccurate trace numbers in a QA-reviewed evidence document undermines
future audits that may cite these figures verbatim.

**Required fix (small, mechanical, documentation-only — does not require re-opening C5 or any code
change)**: refresh `docs/m28-parity-trace-diff.md` Result 3 (and the corresponding
`deferred-backlog.md` C5-row citation) with the post-DEC-0026 figures, OR add an explicit inline
disclosure note that these specific numbers were captured pre-DEC-0026 and that DEC-0026
demonstrably changes the measured trajectory beyond the early-boot window (citing this QA report).
Either resolution is acceptable; the core C5 conclusion needs no change either way.

## 5. DEC-0026 Independent Reproduction (the headline claim — verified directly, not merely trusted)

Per REQ-M28-003 item 5, QA constructed a small, throwaway, non-committed diagnostic (not part of
the repository, built in an isolated scratch CMake project against the repo's own
`sony_msx_core` static library) mirroring `src/frontend/sdl3_app.cpp:194-210`'s exact
`Sdl3App::run_one_frame()` loop shape — a real, CPU-driven cold boot: construct `Hbf1xvMachine`,
`set_asset_root("bios")`, `cold_boot()`, then repeatedly `step_cpu_instruction()` until each frame
boundary and call `on_vsync_boundary()` — checking `machine.vdp().control_register(0..8)` every
frame.

**Post-fix (current tree)**:

```
frame=0   R#0-8= 0 0 0 0 0 0 0 0 0
frame=50  R#0-8= 0 0 0 0 0 0 0 0 0
[QA] R#0-8 FIRST became non-zero at frame=74
frame=74  R#0-8= 0 20 0 0 0 0 0 0 0
frame=100 R#0-8= 0 20 6 80 0 36 7 7 8
frame=200 R#0-8= 0 60 6 80 0 36 7 7 8
...
[QA] RESULT became_nonzero=1 first_nonzero_frame=74 max_frames=500
```

The VDP genuinely configures its mode/palette/screen-enable registers (R#1 mode/blank bits, R#2
name-table base, R#3/R#5 color/sprite-attribute tables, R#6/R#7/R#8 sprite-pattern/color/behavior)
within 74 frames — well inside the report's own "~300 frames" claim.

**Pre-fix (v1.0.27, built from a real `git worktree add --detach <path> v1.0.27` checkout, the
same diagnostic recompiled against that tree's own `V9958Vdp`/`Hbf1xvMachine` unmodified)**:

```
frame=0   R#0-8= 0 0 0 0 0 0 0 0 0
frame=50  R#0-8= 0 0 0 0 0 0 0 0 0
...
frame=500 R#0-8= 0 0 0 0 0 0 0 0 0
[QA] RESULT became_nonzero=0 first_nonzero_frame=0 max_frames=500
```

**This is a direct, independent, controlled before/after confirmation of DEC-0026's headline
claim**: on the exact pre-fix code, a real CPU-driven boot leaves every VDP mode/control register
permanently zero through 500 frames (the black-screen bug, reproduced firsthand, not merely read
about); on the exact post-fix code (only `src/devices/video/v9958_vdp.{h,cpp}` and the additive
`src/machine/hbf1xv_machine.{h,cpp}` wiring differ), the identical diagnostic shows the VDP
genuinely configuring within 74 frames. The worktree was removed after use
(`git worktree remove --force`); the main working tree was never touched by this verification.

Code-level grounding cross-checked directly: `src/devices/video/v9958_vdp.h:23` (`VdpClockSource`
interface), `v9958_vdp.cpp:423-457` (`peek_status_register(2)`'s live VR/HR computation, pull-style,
`clock_source_ != nullptr` gated), `hbf1xv_machine.h`/`.cpp` (`VdpRasterClock`, wired
unconditionally in `wire_bus()` — intentional, since this is the actual bug fix, not gated like
E2). The line/frame timing constants used (`sync[0,100)+left-erase[100,202)+left-border[202,258)
+display[258,1282)+right-border[1282,1341)+right-erase[1341,1368)`; `LN=0: 13+26+192+25+3+3=262`,
`LN=1: 13+16+212+15+3+3=262`) were independently cross-checked against
`references/fact-sheets/Yamaha V9958 VDP.md:124-125` §7 and match exactly.

## 6. M24 CP/M Isolation Test Mask — Independent Judgment

`git diff v1.0.27 -- tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp` shows
exactly one change: inside `capture_device_snapshot()` (called identically for both the "before"
and "after" 10,000-real-CPU-instruction snapshots), the loop over `V9958Vdp::kNumStatusRegs` masks
bits `0x60` (VR|HR) out of **only** status register index `s == 2`, with a five-line inline comment
explaining why (S#2's VR/HR are now legitimately time-dependent since real, unmasked wall-clock-
equivalent CPU cycles elapse between the two snapshots in this specific test's own flat-RAM driver,
which never calls `on_vsync_boundary()`).

**QA's independent judgment: this scoping is correct and narrow.**

- The mask applies to exactly one bit-field (`0x60`) on exactly one status register (`s==2`) in
  exactly one test file — confirmed via `git diff v1.0.27 --name-only`, no other test file is
  touched for this purpose.
- The masking function is shared between the "before" and "after" captures (`capture_device_snapshot`
  is called at both `snapshot_before` and `snapshot_after` call sites, confirmed via `rg -n
  "capture_device_snapshot"` on the file) — it is not a one-sided fudge that only hides a
  post-change value.
- Every other byte in the same 34+ KB snapshot (all other status registers, all 32 command-engine
  registers, the full 512-entry palette, the VRAM pointer, and all 131,072 VRAM bytes) remains an
  exact, unmasked, byte-for-byte comparison — confirmed by reading the full diff (the mask is the
  ONLY change to the function).
- The test's actual invariant — no CROSS-DEVICE STATE LEAKAGE from the CP/M harness (i.e., the
  harness never issues a VDP I/O instruction, so nothing the harness itself does should perturb VDP
  state) — is preserved: VR/HR now legitimately vary with elapsed real time regardless of what the
  CP/M harness does, so masking them out of THIS SPECIFIC test is the correct way to keep testing
  what the test actually means to test, not a weakening of its coverage. The dedicated
  `vdp_vr_hr` unit/integration tests (§2 check #14) are the correct, non-masked place VR/HR's own
  behavior is actually verified.

This is not scope-creep or test-weakening — it is a narrowly-targeted, well-reasoned, symmetric
correction to one test's own oracle in response to a genuine, disclosed, additive behavior change.

## 7. Backlog Review (34 rows)

Full `agent-protocol/state/deferred-backlog.md` re-read this cycle (520 lines).

| Row(s) | Expected disposition | Confirmed |
|---|---|---|
| E2 | `DONE (M28)` | Confirmed, row content matches the implementation report exactly |
| C5 | `IN-PROGRESS (M28 partial)`, new finding recorded | Confirmed; outcome (b) recorded verbatim, not force-closed |
| C1, D4 | Ruling unchanged (UNBUILDABLE-WITHOUT-FABRICATION, no milestone number); both carry a DEC-0026 factual cross-reference note | Confirmed — both rows carry the identical "NOTE (2026-07-08, DEC-0026, factual only, no status change)" paragraph verbatim |
| E1 | `DEFERRED → M30` | Confirmed, scoping caveat (formulaically-derivable subset only, attack-curve remainder named) present verbatim |
| C10 | `DEFERRED → M31` | Confirmed |
| F1 | `DEFERRED → M32` | Confirmed |
| F2 | `DEFERRED → M33` | Confirmed |
| G1 | `DEFERRED → M29` | Confirmed |
| G2, G3, G4 | Unchanged (indefinite/on-demand) | Confirmed, rows byte-for-byte unchanged |
| B1-B9, C2-C4, C6-C9, D1-D3, D5-D7 (23 rows) | Re-affirmed `DONE`, unchanged | Confirmed, none touched this cycle |

**Finding (Low, non-blocking)**: `deferred-backlog.md`'s own top trailer summary block (the
"Last updated: 2026-07-08 ..." narrative at the end of the file) still states the pre-DEC-0026 test
count (**144/144**) and does not mention DEC-0026 at all — only the row-level C1/D4 notes do.
Similarly, `agent-protocol/state/current-phase.md`, `milestones.md`, and
`definition-of-done.yaml`'s M28 entry describe only the developer-implementation-complete
(pre-DEC-0026) state; none of the three currently references DEC-0026, though `decisions.md`
(DEC-0026 entry) and `deferred-backlog.md`'s row-level notes do. This is a ledger-currency gap, not
a functional defect — recommended as a required fix before final tag/closure (update the trailer
count to 146/146 and add a one-line DEC-0026 cross-reference to all three state files), mirroring
this project's own established "stale-status drift corrected same-cycle" practice (M14/M19).

## 8. Failures and Risk Ranking

No test failures. No CPU/core touch. No license-sensitive-scope violation (independently confirmed:
`git diff v1.0.27` shows zero touch to any file outside the inventory in §2 check #17; no numeric
table >20 entries introduced; C1/D4's ruling is untouched).

| Finding | Severity | Blocking? |
|---|---|---|
| C5 trace-diff Result 3 (`final_pc`) is stale post-DEC-0026; core finding unaffected (§4) | Medium | Required fix before tag close, does not require re-opening C5 |
| Ledger trailer/state files (current-phase.md, milestones.md, definition-of-done.yaml, deferred-backlog.md trailer) don't yet reference DEC-0026 / show the stale 144/144 count (§7) | Low | Recommended fix before tag close |

No Critical/High findings. No blocker-level gap in either changeset's core technical claims.

## 9. Required Fixes

1. **(Medium)** Refresh or explicitly caveat `docs/m28-parity-trace-diff.md` Result 3's `final_pc`
   figures (and the corresponding citation in `deferred-backlog.md`'s C5 row) to reflect that they
   were captured pre-DEC-0026 and that DEC-0026 measurably changes the boot trajectory beyond the
   early-boot window — core C5 conclusion (disk-ROM window never paged in) needs no change.
2. **(Low)** Update `agent-protocol/state/current-phase.md`, `milestones.md`, and
   `definition-of-done.yaml`'s M28 entry to reference DEC-0026 (landed in the same closure
   commit/tag as M28); refresh `deferred-backlog.md`'s top trailer test count from 144/144 to
   146/146.

Neither fix requires touching `src/devices/cpu/`, `src/core/`, re-opening C1/D4/E1/C10/F1/F2/G1, or
re-running the slow sweep. Both are documentation/ledger-only corrections.

## 10. Sign-off Decision: **Conditional Pass**

**Rationale**: every substantive technical claim in both changesets — E2's gate mechanism and
regression-safety, C5's core disk-ROM-never-engaged finding and openMSX A/B parity, the health
audit's four parts, DEC-0026's headline black-screen-to-configured-VDP fix (independently
reproduced firsthand via a genuine, controlled before/after diagnostic — see §5, the most important
single claim per the dispatch), the M24 test-mask's narrow and correct scoping, and the
zero-tolerance CPU/core isolation — is independently reproduced and confirmed sound. The full,
unfiltered 146/146 `ctest` (including ZEXALL/ZEXDOC `error_markers==0`) is independently reproduced
from a clean rebuild.

The Conditional Pass is gated solely on the two required fixes in §9, both small, mechanical,
documentation/ledger-only corrections that do not require any code change, do not touch
`src/devices/cpu/`/`src/core/`, and do not require another full QA cycle to verify — a quick
re-check of the two edited documents/state files is sufficient once applied. Per guardrails, this
report does not mark an unconditional Pass while these gaps remain unresolved, but neither
represents a blocker-level defect in the underlying implementation.

**Recommendation**: apply §9's two fixes, then close M28 (tag) with DEC-0026 folded into the same
release note, since both are already correctly bundled in this closure commit.
