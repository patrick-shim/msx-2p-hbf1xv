# M24 QA Sign-off — ZEXDOC/ZEXALL Full Parity Sweep (backlog C3)

- Milestone ID: M24
- QA Owner: MSX QA Agent
- Developer package reviewed: `docs/m24-planner-package.md`, `docs/m24-implementation-report.md`,
  `docs/m24-parity-trace-diff.md`
- Coordinator's own independent verification reviewed: RESP-M24-002 (`agent-protocol/channels/responses.md`)
- Baseline: `v1.0.23` (M23 closed, 121/121 tests)
- This is a **CPU-validation milestone**: no new device, no new hardware-modeling code. The
  question this sign-off answers is narrower and more mechanical than most prior milestones: did a
  genuine, generic, license-isolated CP/M/BDOS harness really run the real, committed
  `zexall.com`/`zexdoc.com` GPL binaries to completion, and did they really report 134/134 genuine
  PASS, with zero regression and zero device-boundary violation elsewhere?

Every finding below is from artifacts I read directly and commands I ran myself this cycle,
starting from a **fully clean rebuild** (`build/` deleted, reconfigured, rebuilt from scratch) —
not the coordinator's or developer's build output, not their `git diff`, not their SHA-256, not
their combinatorial recount, not their openMSX A/B run. Where the requesting message explicitly
asked me to reproduce something "AGAIN, independently, not take the coordinator's word for it
either," I did so from scratch, including a full independent ~23-minute re-run of the slow
124th test.

---

## 1. Regression Scope

- **New machine-layer harness** (`src/machine/cpm_bdos_harness.{h,cpp}`) — a small, generic CP/M
  `.com` loader + `CALL 5`/`JP 0` BDOS-trap mechanism. The single highest-scrutiny question for
  this file: does it carry any zexall/zexdoc-specific knowledge (license-isolation risk)? Read in
  full, end-to-end — it does not.
- **CLI extension** (`src/main.cpp`, `--cpm-run` mode) — claimed purely additive.
- **Build wiring** (`CMakeLists.txt`, `tests/CMakeLists.txt`) — claimed purely additive (new
  source file registration, new test executables, one new compile definition, one new CTest
  LABEL/TIMEOUT).
- **New tests**: 1 unit (`cpm_bdos_harness_unit_test.cpp`, synthetic fixture only), 1 integration
  (`hbf1xv_m24_cpm_run_integration_test.cpp`, synthetic fixture + device-isolation invariant), 1
  system test (`hbf1xv_m24_zexall_system_test.cpp`, the REAL `zexall.com`/`zexdoc.com`, ~23 minutes
  wall-clock).
- **New non-shipped tool** (`tools/zexall-report.py`) — parses captured BDOS output into a
  per-group PASS/FAIL table; the single place closest to "derived from the binary's own content"
  (recognizes observed output markers), and therefore the second-highest license-scrutiny surface.
- **New reference asset** (`references/zexall/`, GPL v2, staged not yet committed) — the real
  YAZE-AG ZEXALL/ZEXDOC suite.
- **Explicit non-changes claimed and independently checked**: zero changes to `src/devices/`,
  `src/peripherals/`, `src/core/` since `v1.0.23`.
- **Full regression suite**: the entire M1-M23 corpus (121 tests) plus 3 new M24 tests (124 total,
  2 fast + 1 genuinely slow ~23-minute test).
- **openMSX 19.1/WSL A/B evidence** for the bounded-prefix loading-convention/BDOS-trap mechanism,
  independently re-run by QA (not merely read).
- **Two explicit judgment calls** this sign-off is asked to make (not force-resolved upstream):
  DEC-0021's disk-boot-A/B disposition, and the system test's ctest-level pass/fail gate design.

---

## 2. Regression Matrix Status

| Area | Verification performed by QA | Result |
|---|---|---|
| Clean rebuild | Deleted `build/` entirely; `cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF` (fresh configure, not a reconfigure into an existing cache); `cmake --build build --config Debug` | Build succeeded, zero errors (only pre-existing C4819 code-page warnings, the same class already present in every prior milestone's build) |
| Fast test subset | `ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure` | **123/123 passed, 0 failed, 6.45s** — matches the developer's/coordinator's own claimed 121 prior + 2 new fast M24 tests |
| **Slow full-sweep system test (THE headline claim)** | `ctest --test-dir build -C Debug -R hbf1xv_m24_zexall_system_test --output-on-failure`, run to genuine completion myself (not delegated, not accepted from the developer's or coordinator's own logs) | **1/1 passed, 1392.06 sec (~23m12s)**. Read the exact captured stderr directly from `build/Testing/Temporary/LastTest.log` (not the summary line): `ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` and `ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0` — **a THIRD independent, from-scratch, byte-for-byte reproduction** of the exact instruction count and marker split (developer: 1453.25s; coordinator: 1359.34s; QA (me): 1392.06s — three different wall-clock times from three different runs/machine-load conditions, but the identical 5,764,169,474-instruction count and identical 67/0 marker split every time). This is strong, independent, repeated-run confirmation of full determinism, not a cached/one-off result. |
| `git diff v1.0.23` scope, `src/devices/`/`src/peripherals`/`src/core/` | `git diff v1.0.23 --name-only -- src/devices/ src/peripherals/ src/core/` (ran myself) | **Empty** — zero changes, confirmed independently |
| `git diff v1.0.23 -- src/main.cpp` (full diff read, not stat) | Read the complete diff myself | Confirmed purely additive: one new `#include`, one new self-contained `run_cpm()` function, one new CLI dispatch branch (`argc >= 2 && argv[1] == "--cpm-run"`) inserted between two pre-existing branches with no collision. Zero lines of any pre-existing function touched. |
| `git diff v1.0.23 -- CMakeLists.txt tests/CMakeLists.txt` (full diff read) | Read directly | `CMakeLists.txt`: one additive source-registration line. `tests/CMakeLists.txt`: 47 new lines registering 3 new test executables + the `SONY_MSX_ZEXALL_DIR_ABS` compile definition (mirrors the established `SONY_MSX_BIOS_DIR_ABS` pattern, DEC-0016 watch-item) + the `m24_slow_full_sweep` LABEL/7200s TIMEOUT. No existing test target's registration touched. |
| `CpmBdosHarness` license-isolation claim | Read `src/machine/cpm_bdos_harness.h` and `.cpp` in full, end-to-end (112 + 91 lines) | Confirmed: implements only the generic CP/M convention (load at `0x0100`, top-of-memory word at `0x0006`, `CALL 5`/`JP 0` traps, RET-synthesis via stack pop). Zero message strings, zero CRC values, zero group names/counts, nothing transcribed from `zexall.z80`/`zexdoc.z80`. The one size-guard constant (`0xFF00` default) is a generic, documented, defensive default, not a zexall-derived magic number. |
| Unit-test fixtures | Read `tests/unit/machine/cpm_bdos_harness_unit_test.cpp` and `tests/integration/machine/hbf1xv_m24_cpm_run_integration_test.cpp` in full | Confirmed: all fixtures are small, hand-written, documented byte-by-byte Z80 programs (`"HI$"`/`"BYE$"` messages, an unrelated function-99 fixture, a self-jump infinite loop) — structurally and textually unrelated to `zexall.z80`/`zexdoc.z80`'s own content. The real GPL binaries are loaded ONLY by the system test, never by these two files. |
| `tools/zexall-report.py` self-check | `python tools/zexall-report.py --self-check` (ran myself) | **SELF-CHECK PASSED** (9/9 checks) — independently confirmed the decoy-substring defense (a group name containing the literal text "OK"/"ERROR" is not falsely matched, since the parser anchors on the fixed 30-byte structural offset, never a bare substring search), the missing-banner and corrupted-terminator malformed-input cases both raise `ParseError` rather than silently returning a partial/guessed result. |
| Device-isolation invariant | Read `hbf1xv_m24_cpm_run_integration_test.cpp`'s `capture_device_snapshot()` in full | Confirmed comprehensive: 16 PSG registers, all VDP control/command/status registers + all 16 palette entries + the full 128 KB VRAM + the VRAM pointer, 6 PPI fields, all RTC blocks/registers + mode register + address latch, 4 FDC registers — snapshotted before and after the harness run, asserted byte-for-byte identical. This is a real, comprehensive "nothing strayed into device state" proof, not a token check. |
| Adversarial self-check (§4.4 of the implementation report) | Read the reasoning directly; independently recomputed SHA-256 of the real committed binaries myself | `sha256sum references/zexall/zexall.com` = `6e2da55147a04f28d303d5da6a1e6b771557ac244653590a0f24a2d39c8537e8` (8704 bytes) — **exactly matches** the implementation report's §4.4 cited digest, independently recomputed by me from the real file on disk, not copy-pasted. `git status --porcelain references/zexall/` shows the file staged as `A` (added), never `M` (modified) — consistent with "never actually written to since being staged." The self-check's described mechanism (in-memory-only byte corruption, never touching the real file) is architecturally sound given `CpmBdosHarness::load_com` takes `std::vector<std::uint8_t> image` **by value** (confirmed by signature read) — a caller-side corruption of its own copy cannot touch the source file on disk by construction. |
| Combinatorial total re-derivation (§4.2, developer's 1,727,144 vs. planner's ≈1,727,400) | Independent, from-scratch Python pass over `references/zexall/zexall.z80`, run by me, not copied from the developer's or coordinator's own script | `grep -cE "^\s*;.*\([\d,]+ cycles\)\s*$"` / Python regex sum: **66 matched header lines summing to 1,661,608**. Read the one group whose header omits its own total (`<daa,cpl,scf,ccf>`, `zexall.z80:290-296`) directly: its increment-vector sub-annotation states `(65,536 cycles)` with an accompanying `(1 cycle)` shift-vector, giving a group total of `65,536 x 1 = 65,536`. **1,661,608 + 65,536 = 1,727,144** — exactly matching the developer's and coordinator's own independently-stated figure, reproduced by me from scratch, not merely re-read. The 0.015% gap vs. the planner's rougher "≈1,727,400" estimate is confirmed genuinely immaterial: it never gates any pass/fail decision (the actual acceptance criterion is the suite's own 67-group "OK"/"ERROR" verdict, which is what the harness actually asserts on). |
| A-M24-5 (identical 67-group order, both suites) | `diff` of `zexall.z80:100-168` vs `zexdoc.z80:100-168` (the `tests:` dispatch tables), run by me | **Empty diff** — genuinely identical dispatch order, confirmed independently. Spot-checked the `adc16` flag-mask byte difference directly (`zexall.z80:195` = `0ffh`, `zexdoc.z80:195` = `0c7h`) — the only per-group difference, exactly as claimed. |
| openMSX A/B bounded-prefix PARITY claim | **Independently re-ran `tools/openmsx-m24-zexall-parity.ps1` myself** end-to-end (WSL openMSX 19.1, confirmed present: `/usr/bin/openmsx`, `openMSX 19.1 flavour: debian`), against a scratch output path so as not to overwrite the developer's committed artifact | Reproduced: `Emulator A (zexall) captured: 'Z80 instruction exerciser'`, `openMSX B (zexall) captured: 'Z80 instruction exerciser'`, `-> PARITY`; identically for zexdoc. **`M24 C3 bounded-prefix A/B RESULT: PARITY (both suites)`** — an independent, from-scratch reproduction of the developer's own committed `docs/m24-parity-trace-diff.md` result, not merely a re-read of that file. |
| Deferred-backlog / 34-row review | Read `agent-protocol/state/deferred-backlog.md` in full | C3 disposed "IN-PROGRESS (M24 implementation complete, pending QA)" with the full evidence summary matching the implementation report exactly, plus the honest DEC-0021 stale-reasoning note. All other 33 rows re-affirmed, matching the implementation report's §7 word-for-word. No silent drift observed. Status-column flip correctly left un-flipped, pending this sign-off. |
| `agent-protocol/state/current-phase.md`, `milestones.md`, `definition-of-done.yaml` | Read directly | M24 correctly shown as `implementation_complete_pending_qa`, not prematurely marked done anywhere. Consistent with the implementation report and RESP-M24-002. |
| Evidence-gate scripts | Ran myself: `tools/validate-assets.ps1`, `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` | `Asset validation result: True` (7 BIOS files + 2 ROM files present); checksum report refreshed successfully. |
| `.com` file size / `top_of_memory` safety margin (R-M24-2) | `ls -la references/zexall/*.com` | Both files are 8704 bytes; `0x0100 + 8704 = 0x2300`, comfortably under the default `0xFF00` guard — confirmed independently, not merely re-read. |
| `references/zexall/LICENSE` | Read directly | Genuine GNU GPL v2 text, matching the claimed YAZE-AG/Frank D. Cringle provenance in `README.md`. |
| CLI dispatch collision check | `grep -n 'argv\[1\]) =='` over `src/main.cpp` | `--cpm-run` is a unique flag string inserted between `--halnote-parity` and `--parity-trace` in the existing sequential `if` chain — no shadowing, no collision. |

---

## 3. Failures and Risk Ranking

**No test failures. No CPU-core defect found or suspected.** The 134/134 genuine PASS claim is
independently confirmed, not merely accepted — three separate from-scratch full-sweep runs
(developer, coordinator, QA) all produced the identical instruction count and marker split.

### Risk 1 (Medium, forward-looking regression-safety design gap) — the system test's ctest-level gate does not hard-fail on a genuine per-group FAIL

Confirmed by direct read of `tests/system/hbf1xv_m24_zexall_system_test.cpp:148-171`: the test
asserts `ok_markers + error_markers == 67` (a marker-COUNT sanity check) and
`unexpected_bdos_calls == 0`, but at no point asserts `error_markers == 0` for either suite. A
future genuine CPU-core regression that flipped, say, all 67 ZEXALL groups from PASS to FAIL would
still produce `ok_markers=0, error_markers=67`, sum `== 67` — **this specific ctest would still
report `Passed`**. The only signal would be a `"SIGNIFICANT FINDING"` line printed to stderr, which
is not surfaced by ctest's default summary output and is easy to miss in a 1400-second test that
ordinarily produces two quiet, unremarkable lines of diagnostic output.

I take this more seriously than a "documentation nuance" because of what this specific test
represents: it is the single most comprehensive, most expensive (~23 minutes, ~11.5 billion real
instructions combined) CPU-correctness oracle this project has ever built, deliberately exceeding
the combinatorial coverage of the existing M9/M10/M12 fixed-trace CPU-timing oracles. Its entire
purpose, going forward, is to be the thing that catches a genuine Z80-core regression in some
future, unrelated milestone. A regression detector whose own pass/fail exit code can stay green
while its core invariant (134/134 group verdicts PASS) is silently violated undermines exactly the
kind of "if `ctest` is green, the invariant holds" evidentiary chain this project's entire operating
model depends on (CLAUDE.md's Determinism/Development-Accuracy non-negotiables; the guardrail
against approving release readiness without regression evidence).

This is **not** a defect in the emulator's current, measured behavior — that is unambiguously
correct, independently confirmed three times over this cycle alone. It is a design gap in the
**permanent regression asset** this milestone commits to the codebase.

### Risk 2 (Low, judgment call, not a defect) — DEC-0021's disk-boot-A/B disposition

See the explicit ruling in §5 below (kept out of the risk-ranking table proper since this is a
scope/process judgment call, not a code or evidence defect).

### No other findings

- License isolation: sound, independently confirmed by direct, full read of every touched `src/`
  file and `tools/zexall-report.py`.
- Zero regression: independently confirmed (123/123 fast + 1/1 slow, clean rebuild).
- Device isolation: independently confirmed (comprehensive snapshot, byte-for-byte identical).
- Adversarial self-check: reasoning independently reviewed and confirmed architecturally sound
  (value-semantics `load_com` signature makes on-disk corruption structurally impossible via this
  mechanism); real file confirmed byte-identical via independently-recomputed SHA-256.
- openMSX A/B: independently re-run from scratch, genuine PARITY reproduced for both suites.
- Backlog/state consistency: independently confirmed, no drift.

---

## 4. Required Fixes

### Required before this milestone can be called a clean, unconditional PASS

1. **Harden `tests/system/hbf1xv_m24_zexall_system_test.cpp`'s pass/fail gate** to hard-assert
   `zexall.error_markers == 0` and `zexdoc.error_markers == 0` (in addition to the existing
   marker-count-sum and unexpected-BDOS-call checks). This is a small, mechanical, purely additive,
   zero-behavioral-risk change — it asserts something already independently confirmed true (`
   error_markers=0` in all three from-scratch runs this cycle) and does not require re-running the
   full ~23-minute sweep from a correctness standpoint (the measured data does not change), though a
   single re-run of the updated test to confirm the new assertions compile and still pass is a
   reasonable, cheap confirmation step before tag. This directly converts the project's single most
   valuable CPU-correctness oracle from an "observe-and-report" test (which can silently stay green
   through a genuine future regression) into a proper hard-gate consistent with every other
   zero-tolerance test in this codebase.

### Recommended, non-blocking follow-ups for the coordinator at closure

2. Fold the DEC-0021 disposition below (§5) into the M24 closure decision text, explicitly, so a
   future planner/developer does not need to re-derive this reasoning from scratch.
3. Record Risk 1's resolution (once applied) as a forward-looking watch-item confirming the
   tightened gate, mirroring this project's existing watch-item-list convention in
   `agent-protocol/state/current-phase.md`.

No other action is required. All of §2's Regression Matrix rows passed independent QA
reproduction with no other gap found.

---

## 5. Explicit Judgment Calls (per REQ-M24-003 items 7 and 8)

### 5.1 DEC-0021 — should M24 redo its disk-boot-A/B BLOCKED sub-claim now that `disks/` exists?

**Ruling: No — leave BLOCKED-as-recorded. Reserve `disks/` for a future, dedicated C5
investigation, not as an M24/C3 closure add-on.**

Reasoning:

- The planner package itself explicitly names and defends against exactly this conflation risk
  (R-M24-8: "a future planner/developer conflates C3's closure with C5's remainder, since both
  involve 'running a program to completion'"), textually distinguishing C3 ("run a CP/M-style
  validation binary via a purpose-built trap harness") from C5 ("reach a real, unattended
  BIOS/MSX-DOS boot prompt"). I independently re-read `agent-protocol/state/deferred-backlog.md`'s
  C5 row and confirmed this distinction is real, not rhetorical: C5's own residual finding, from
  M16, is that the automatic disk-ROM boot handshake is **"genuinely never observed within an
  unattended, keyboard-less cold boot (confirmed up to a 20,000,000-instruction diagnostic run)"**
  — this is a substantial, still-unsolved technical problem in its own right, not a matter of simply
  having the right disk image available now.
- Attempting a genuine MSX-DOS disk-boot-to-prompt A/B for M24 would require solving C5's own
  open problem (the auto-boot trigger) plus building new keystroke-driven automation this project
  has never used, on **both** engines — this is new, non-trivial implementation-adjacent scope that
  was never part of M24's planned slices (S0-S4) and was never routed through the planner-first
  sequencing this project's protocol requires for new work. QA's role is to assess evidence, not
  originate new development work under time pressure at the sign-off step.
- M24's actual, already-gathered evidence for its own real acceptance criteria is not weakened by
  leaving this BLOCKED: the bounded-prefix openMSX A/B already demonstrates the new loading
  convention + BDOS-trap mechanism achieves genuine cross-engine PARITY (independently reproduced
  by me this cycle), and the underlying CPU-instruction-level correctness is covered far more
  thoroughly by the in-process, 5.76-billion-instruction real sweep than any live-Tcl-bounded
  disk-boot fragment ever could be (the same real-time-scheduling infeasibility documented in
  `docs/m23-parity-trace-diff.md` would apply here just as much as to a full live-Tcl ZEXALL sweep).
- This mirrors the project's own established, repeatedly-applied precedent (D7/C5's original
  carry-forward, M23's C1/D4 narrowing) of not force-closing or silently expanding a milestone's
  scope mid-cycle when a genuinely separate, larger piece of work is adjacent to it. DEC-0021's own
  text explicitly anticipates and permits this outcome ("This does NOT retroactively obligate M24 to
  redo its disk-boot A/B sub-claim").
- The `disks/` assets are valuable and should stay visible for whichever future milestone tackles
  C5's real auto-boot-trigger investigation — I recommend the coordinator ensure `deferred-backlog.md`'s
  C5 row (already updated per DEC-0021) remains the pointer to this asset, which it currently is.

### 5.2 The system test's "observe-and-report" ctest gate — acceptable, or should it be tightened?

**Ruling: Should be tightened — see Required Fix #1 in §4. This is the basis for my Conditional
PASS disposition below**, not a mere forward-looking note. My full reasoning is in Risk 1 (§3). To
state the bottom line plainly: a test whose entire purpose is to be a permanent, high-value
regression detector should never be able to report `Passed` while its own documented core
invariant (134/134 genuine PASS) is violated. The fix is cheap, safe, and mechanical, so I am not
recommending this block the milestone's overall technical acceptance (which is fully sound) — but
I am recommending it gate the "clean PASS, proceed without an extra human pause" fast path this
milestone's own continuation directive establishes, precisely because that fast path is the
scenario this gap is most relevant to protecting against.

---

## 6. Sign-off Decision: **Conditional Pass**

**All of this milestone's substantive technical claims are independently verified and sound**:
134/134 genuine ZEXALL/ZEXDOC group-verdict PASS (reproduced three times independently, including
by me from a fully clean rebuild this cycle), zero regression (124/124), genuine license isolation
(every touched file read in full), a genuine and architecturally-sound adversarial self-check,
genuine device isolation, genuine openMSX A/B PARITY for the feasible bounded-prefix technique
(independently re-run by me), and a complete, accurate 34-row backlog review.

**The one gating condition**: apply Required Fix #1 (§4) — harden the system test's ctest-level
gate to hard-assert `error_markers == 0` for both suites — before the coordinator proceeds through
the release-decision/tag step. This is a same-cycle, low-risk, mechanical change; once applied and
confirmed (re-run of the updated system test, plus the fast suite, both green), this milestone is
eligible for a clean PASS and the coordinator's normal no-extra-pause continuation path.

Per this milestone's own standing directive ("on anything short of a clean PASS, STOP and consult
the human"): the coordinator should treat this Conditional Pass as short of a clean PASS and either
(a) route the trivial fix back to the developer and re-confirm before tagging, or (b) explicitly
consult the human if choosing to accept the residual risk and tag as-is. I do not recommend option
(b) given how inexpensive option (a) is relative to the value of the thing it protects.

---

## Independent Verification Detail

### A. Clean rebuild (directly observed, not trusted from any prior report)

```
rm -rf build
cmake -S . -B build -DSONY_MSX_ENABLE_SDL3=OFF     -> configured from scratch
cmake --build build --config Debug                  -> succeeded, zero errors
ctest --test-dir build -C Debug -LE m24_slow_full_sweep --output-on-failure
  100% tests passed, 0 tests failed out of 123
  Total Test time (real) = 6.45 sec
ctest --test-dir build -C Debug -R hbf1xv_m24_zexall_system_test --output-on-failure
  1/1 Test #124: hbf1xv_m24_zexall_system_test ....   Passed  1392.06 sec
  100% tests passed, 0 tests failed out of 1
```

### B. Exact captured output for the slow test (read from `build/Testing/Temporary/LastTest.log`, not the summary line)

```
ZEXALL: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
ZEXDOC: finished=1 instructions_executed=5764169474 ok_markers=67 error_markers=0 unexpected_bdos_calls=0
```

Byte-for-byte identical instruction count and marker split to both the developer's own
(`1453.25 sec`) and the coordinator's own (`1359.34 sec`) independent runs — three separate
from-scratch reproductions now, all identical on the load-bearing numbers, only wall-clock time
differing (expected, given differing machine load).

### C. `git diff v1.0.23` scope (ran myself)

```
git diff v1.0.23 --name-only -- src/devices/ src/peripherals/ src/core/     -> (empty)
git diff v1.0.23 --name-only -- src/                                        -> src/main.cpp only
git diff v1.0.23 -- src/main.cpp                                            -> purely additive (new
    #include, new run_cpm() function, new CLI branch; zero lines of any existing function touched)
git diff v1.0.23 -- CMakeLists.txt tests/CMakeLists.txt                     -> purely additive
```

### D. SHA-256 (independently recomputed, not copy-pasted)

```
sha256sum references/zexall/zexall.com
6e2da55147a04f28d303d5da6a1e6b771557ac244653590a0f24a2d39c8537e8 *references/zexall/zexall.com
```

Matches the implementation report's §4.4 cited digest exactly.

### E. Independent combinatorial-total recount (Python, from scratch)

```
grep -cE "^\s*;.*\([0-9,]+ cycles\)\s*$" references/zexall/zexall.z80   -> 66
(python sum of all matched "(N cycles)" figures)                        -> 1,661,608
<daa,cpl,scf,ccf> group (line 290-296): increment-vector sub-annotation "(65,536 cycles)",
  shift-vector "(1 cycle)"                                              -> group total 65,536
1,661,608 + 65,536 = 1,727,144   -- matches the developer's/coordinator's own figure exactly
```

### F. Independent openMSX A/B re-run (WSL openMSX 19.1, ran myself end-to-end)

```
openMSX: /usr/bin/openmsx (openMSX 19.1 flavour: debian components: ALSAMIDI CORE GL LASERDISC)
Emulator A (zexall) captured: 'Z80 instruction exerciser'
openMSX B (zexall) captured: 'Z80 instruction exerciser'
Emulator A (zexdoc) captured: 'Z80 instruction exerciser'
openMSX B (zexdoc) captured: 'Z80 instruction exerciser'
M24 C3 bounded-prefix A/B RESULT: PARITY (both suites).
```

### G. `tools/zexall-report.py --self-check` (ran myself)

```
SELF-CHECK PASSED
```

(9/9 individual checks passed, including the decoy-substring defense and both malformed-input
`ParseError` cases.)

---

## Summary for the coordinator

- **Full regression independently reproduced by QA from a genuinely clean rebuild**: 123/123 fast
  (6.45s) + 1/1 slow (1392.06s) = 124/124, zero failures.
- **The headline 134/134 genuine PASS claim is now independently reproduced THREE separate times**
  (developer, coordinator, QA), all from-scratch, all producing the identical
  5,764,169,474-instruction count and 67/0 marker split per suite.
- **License isolation, device isolation, adversarial self-check, combinatorial-total arithmetic,
  and openMSX A/B PARITY are all independently re-verified, not merely re-read.**
- **DEC-0021 judgment call**: leave the disk-boot-A/B sub-claim BLOCKED-as-recorded; reserve
  `disks/` for a future, dedicated C5 investigation (§5.1).
- **The one gating item**: the system test's ctest-level gate should be hardened to hard-assert
  `error_markers == 0` for both suites before this milestone is a clean, unconditional PASS (§5.2).

**Recommendation: Conditional Pass.** No blocker-level defect exists in the emulator's measured
behavior — that is thoroughly, independently, and repeatedly confirmed correct. The gating
condition is a small, safe, same-cycle test-hardening fix to the regression harness itself, not a
re-opening of any of this milestone's substantive technical claims. Once applied and reconfirmed,
this milestone is eligible for the standing M24-M25 continuation's no-extra-pause release path.
