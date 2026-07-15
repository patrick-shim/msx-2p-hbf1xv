# M52 QA Sign-off — SDL3 Master Volume + Hotkey Reshuffle + Disk-Writable Default ON

- Milestone ID: **M52** (DEC-0079, REQ-M52-003)
- Reviewer: MSX QA Agent
- Date: 2026-07-15
- Tree state assessed: **UNCOMMITTED** working tree on `main @ 1885345` — 13 modified tracked
  files (+313 / −40) + 1 new untracked `src/frontend/master_volume.h` + gitignored `tests/`
  edits (3 new test cpp + extended suites + `tests/CMakeLists.txt`).
- Blast radius: **MED** (default-behavior change on the disk write-back path) → NORMAL
  human-release gate, no auto-close.
- Handoffs read: `docs/m52-planner-package.md`, `docs/m52-implementation-report.md`, DEC-0079,
  REQ-M52-002 / REQ-M52-003.
- Sign-off Decision: **PASS.**

---

## 1. Regression Scope

Frontend/config usability slice — no silicon behavior touched. In scope for regression:

- **Audio presentation:** post-mix master-gain scalar in `Sdl3AudioPresenter`; byte-identity
  at full volume (100); DEC-0039 mixer balance and `kAmplitudeScale` must stay untouched.
- **CLI/XML/resolution plumbing (M50 seam):** `--volume <0..100>`, `<volume percent>` XML,
  `--no-disk-writable`, CLI > XML > built-in-default precedence, anti-drift struct defaults.
- **Hotkey dispatch:** fast-disk Alt+D → Alt+F; new Alt+D/Alt+P volume steppers; new Alt+S
  disk-writable toggle; host-hotkey discipline (only the letter keydown swallowed).
- **Disk write-back path:** disk-writable default flip false → true (SDL3 resolver + shipped
  XML); Alt+S bind/unbind flush semantics; headless default stays OFF; M45/M45b/M47 WD2793
  write-integrity preserved.
- **Determinism guard:** every pre-M52 emulation-output fixture byte-identical; only the 3
  enumerated §4.5 config-policy assertions flip.
- **HARD-EXCLUDE timing set:** `src/core`, `src/devices/cpu`, VDP/sprite/WD2793/S1985 files —
  must show an EMPTY diff.

Out of scope (correctly): device/timing behavior, openMSX A/B, ZEXALL exerciser (frontend-only,
DEC-0077/M50 precedent), and the DEC-0039 audio balance.

## 2. Regression Matrix Status

Reproduced from the ONE canonical `build/` tree (SDL3=ON, DEC-0041 single-build policy).

| # | Item | Method | Result |
|---|------|--------|--------|
| a | Build | `cmake --build build --config Debug` | **PASS** — exit 0, 0 compile/0 link errors/0 warnings; `sony_msx_headless.exe` + `sony_msx_sdl3.exe` present |
| a | Full ctest | `ctest --test-dir build -C Debug --output-on-failure -E zexall` | **PASS** — **247/247 (100%), 0 failed**, 45.47 s (matches report's 247/247) |
| b | §4.5 flips only | diff-level read of gitignored `tests/` | **PASS** — exactly the 3 enumerated flips; all other changes additive |
| c | M-1 byte-identity | read test #183 (non-tautological) + presenter short-circuit + anti-drift greps | **PASS** |
| d | Alt+S flush | read test #184 + FDC empty-diff + write-integrity suite | **PASS** |
| e | HARD-EXCLUDE empty-diff | `git diff --exit-code` over the timing set | **PASS — EMPTY** (re-run twice, start and end of QA) |
| f | Shipped-XML round-trip | test #187 + spot-read `sony_msx_hbf1xv.xml` + README/banner | **PASS** |
| g | SDL3 hotkey wiring | headless `--hidden-window` smoke (`--volume 50`, copied disk) | **PASS (wiring)** — live keypress dispatch deferred to owner live check |
| h | ZEXALL #141 | disposition | **Skipped (justified)** — empty-diff-proven CPU-unchanged; DEC-0077/M50 precedent |

### Detailed evidence

**(a) Build + ctest.** Clean rebuild from `build/` (exit 0, no warnings/errors, both exes
built). Full ctest excluding only `hbf1xv_m24_zexall_system_test` (#141): **247 run, 247
passed, 0 failed**. Every M52-relevant test green:
`frontend_master_volume_unit_test` (#182), `frontend_master_volume_audio_identity_integration_test`
(#183), `frontend_disk_writable_toggle_flush_integration_test` (#184),
`machine_hbf1xv_shipped_config_roundtrip_integration_test` (#187),
`machine_emulator_config_unit_test` (#178), `frontend_config_runtime_unit_test` (#180),
`frontend_config_runtime_wiring_integration_test` (#181), `frontend_sdl3_cli_unit_test` (#151),
and the FDC write-integrity set `devices_fdc_wd2793_write_{stall,firstbyte,earlybyte}`
(#77/#78/#79) + `machine_hbf1xv_m36_disk_save_persist_integration_test` (#174).

**(b) §4.5 test-expectation flips.** The `tests/` tree is gitignored (no committed baseline to
diff), so each expectation was read directly. The ONLY changed pre-existing oracle assertions
are exactly the enumerated three:
- `tests/unit/machine/emulator_config_unit_test.cpp:369` — `kDefaults.disk_writable == true`
  (was `== false`) with an added `kDefaults.master_volume == 100`. Carries a DEC-0079 §4.5 comment.
- `tests/unit/frontend/config_runtime_unit_test.cpp:113` — `NoConfig_DiskWritableOn`
  (`r.disk_writable`), was `NoConfig_DiskWritableOff` (`!r.disk_writable`). DEC-0079 §4.5 comment.
- `tests/integration/machine/hbf1xv_shipped_config_roundtrip_integration_test.cpp` — added
  `DIFF(master_volume)` (line 74) + `<volume`/`percent="100"` to `contains_all`; shipped
  `<disk-writable enabled="true"/>`.

Everything else is **additive** (strengthening coverage), NOT a weakening: new `<volume>` parse
cases (valid boundaries / out-of-range warn+default / bad type / omitted), the FullValid
`master_volume == 55` add, the volume 5-case precedence matrix (Part G) and the disk-writable
precedence matrix incl. the forced-OFF case (Part H). The FullValid `disk_writable == true`
assertion (line 107) was already true pre-M52 (§4.5/A5) — unchanged. No other oracle weakened.

**(c) M-1 full-volume byte-identity + anti-drift.**
- Test #183 (`master_volume_audio_identity_integration_test`) is **non-tautological**: it
  produces a real interleaved-stereo mix from `MachineAudioMixer` (a genuine PSG+SCC program),
  asserts the mix is non-zero (`Mix_IsNonTrivial_OracleNotVacuous`), then asserts
  `apply_master_gain(buf, 100)` is **byte-for-byte equal to the un-gained reference mix**
  (`FullVolume100_ByteIdenticalToUnGainedMix_HardOracle`), plus @0 all-zero and @37 attenuation-only.
- Presenter live path: `push_produced_paced` (`sdl3_audio_presenter.cpp:191-207`) short-circuits
  at `master_volume_ == 100` and pushes the **original `produced` bytes** (no copy, no scaling);
  below 100 it scales into a reusable `scaled_buffer_` so the caller's buffer is never mutated.
- `machine_audio_mixer.{h,cpp}` diff is **EMPTY**; the only `kAmplitudeScale` string in the diff
  is a documentation comment, not a constant change.
- Anti-drift confirmed: `Sdl3AppConfig::disk_writable = false` (`sdl3_app.h:66`),
  `ParsedSdl3Cli::disk_writable = false` + `disk_writable_specified = false` (`sdl3_cli.h:72,209`),
  `ResolvedRuntimeConfig::disk_writable = false` (`config_runtime.h`), `Sdl3AppConfig::master_volume
  = 100` all stay at their pre-M52 struct defaults. The disk-writable flip to `true` lives ONLY in
  `EmulatorConfig::disk_writable` (`emulator_config.h:71`) + shipped `sony_msx_hbf1xv.xml`. The two
  other `disk_writable = true` sites in `src/` (`sdl3_cli.cpp:136`, `main.cpp:935`) are the explicit
  `--disk-writable` FLAG handlers, not defaults.

**(d) Alt+S flush semantics.**
- Test #184 (`disk_writable_toggle_flush_integration_test`) models the exact §2.4 contract via
  the SAME `DiskImage::set_host_path` bind/unbind primitive the handler uses: (a) ON → write →
  `flush()` true → reload byte-identical; (b) ON → write → OFF (`set_host_path({})`) → `flush()`
  no-op, file never written, in-memory write KEPT (no rollback), discarded at exit; (c) write-
  while-OFF → late-ON bind → whole image flushed (early + later writes persisted); + two-run
  determinism.
- `src/devices/fdc` diff is **EMPTY** (no device edit — the Alt+S unbind reuses the existing
  `set_host_path` per A3). The handler `on_disk_writable_toggle_hotkey` (`sdl3_app.cpp:857-882`)
  keeps `config_.disk_writable` and the current image's host-path binding in lockstep (the two
  flush gates A4 agree).
- FDC write-integrity preserved (S3-7): #77/#78/#79/#174 all green (see (a)).

**(e) HARD-EXCLUDE empty-diff accuracy gate.** `git diff --exit-code` over
`src/core src/devices/cpu src/devices/video/vdp_access_timing.h {v9958_vdp,sprite_engine}.{h,cpp}
src/devices/fdc/wd2793.cpp src/devices/chipset/s1985_engine.{h,cpp}` is **EMPTY** (exit 0),
independently re-run at both the start and end of QA. No HARD-EXCLUDE timing file touched.

**(f) Shipped-XML round-trip + docs.** Test #187 adds `DIFF(master_volume)` (proving
parse(shipped) == compiled default for both fields) and the `<volume`/`percent="100"` tokens,
and asserts `ShippedConfig_ZeroWarnings` + `ShippedConfig_EqualsCompiledDefaults` (with a gutted-
file guard). Spot-read of `sony_msx_hbf1xv.xml`: `<disk-writable enabled="true"/>` and
`<volume percent="100"/>` present with accurate comments. Banner/usage
(`sdl3_main.cpp`) + README correctly document Alt+F (fast-disk), Alt+D/Alt+P (volume), Alt+S
(disk-writable), `--volume`, `--no-disk-writable`, disk-writable default ON, and the master-volume
XML field.

**(g) SDL3 hotkey wiring — headless smoke.** `sony_msx_sdl3.exe --hidden-window --max-frames 3
--volume 50` boots and exits clean (exit 0); the startup summary prints `Volume : 50%
(Alt+D -10 / Alt+P +10)` and the reshuffled hotkey banner (Alt+F fast-disk, Alt+D/Alt+P volume,
Alt+S disk-writable) — confirming the `--volume` → resolver → `config.master_volume` → `init()` →
`set_master_volume` wiring end-to-end. A second run against a COPIED disk in scratchpad printed
`[writable -- saves persist on swap/exit; Alt+S toggles]` (disk-writable resolves ON with no
config/flag) and the disk SHA256 was **identical before and after** the boot run — a boot-only
run under the flipped default does NOT spuriously mutate the host disk (directly de-risks R1).
The live interactive keypress dispatch (Alt+D/Alt+P/Alt+S/Alt+F in a real window) and audible
attenuation remain the established manual-smoke boundary — NOT fabricated here.

**(h) ZEXALL #141.** Skipped. The mandatory empty-diff gate proves `src/core` and
`src/devices/cpu` are byte-identical to pre-M52, so the ~20-30 min Debug exerciser's result is
unchanged from the green baseline; frontend/config-only milestones do not run it (DEC-0077/M50
precedent). Optional and non-blocking.

## 3. Failures and Risk Ranking

**No test failures. No blocker-level gaps.** Residual observations (all Low):

| Sev | Item | Assessment |
|-----|------|------------|
| Low | Live hotkey dispatch + audible volume + real game-save persistence have no ctest | Inherent, pre-existing manual-smoke boundary for every host hotkey (Alt+D/Alt+B/Alt+M never had a direct ctest — needs a live SDL pump). The decidable logic (`step_master_volume` clamp/grid, `DiskImage` bind/unbind flush) IS fully unit/integration-tested; the headless smoke confirmed the wiring + banner. Deferred to the owner live check (planner §6). |
| Low | `step_master_volume` documented deviation from planner §2.3 prose | The developer implemented a direction-aware grid snap instead of the prose formula `round_to_grid + dir*step`. Verified it satisfies **every** §4.1 S2-1 acceptance value (step(70,-1)=60, step(0,-1)=0 clamp, step(100,+1)=100 clamp, step(55,-1)=50, step(55,+1)=60); the planner prose formula would yield 70 for the off-grid +1 case, contradicting its own S2-1 example. The developer correctly followed the testable oracle. CLAMP-not-wrap preserved. Acceptable — not a defect. |
| Low | R1 — disk-writable default flip could mutate `.dsk` files | Well-mitigated: SDL3-only flip (headless stays OFF), struct-default anti-drift, `--no-disk-writable` escape hatch, empty-diff + fixture byte-identity gates, the OFF-discards flush test, AND the live smoke showing a boot-only run leaves the disk SHA256 unchanged. Owner-acknowledged consequence (a real MSX writes its floppies). |

Risks R2–R7 from the planner are covered by the evidence above (R2 byte-identity #183 + empty
mixer diff; R3 hotkey discipline read + banner smoke; R4 anti-drift greps + precedence matrices;
R5 presentation-only by construction; R6 flush test #184; R7 §4.5 flips confirmed as the only
policy changes).

## 4. Required Fixes

**None.** No blocker or major issue. No fix is required for release readiness. The residuals in
§3 are Low and inherent/acceptable; none is a QA-imposed condition.

## 5. Sign-off Decision

**PASS.**

All §4 acceptance criteria (S1-1…S4-2, M-1…M-6) are met with reproduced evidence; all §6 evidence
gates are green; the HARD-EXCLUDE empty-diff accuracy gate is EMPTY; the only test-expectation
changes are the 3 enumerated §4.5 config-policy flips (no other oracle weakened); full-volume
byte-identity, anti-drift confinement, Alt+S flush semantics, and WD2793 write-integrity are all
independently confirmed; and a headless smoke verifies the volume wiring, the disk-writable
default-ON resolution, and no spurious host-disk mutation on boot. The tree was byte-stable
throughout QA (no foreign writer; no QA-induced mutation).

### Recommended release path (standard NORMAL human-release gate — not a QA condition)

Owner live check in an interactive SDL3 window before release:
1. **Alt+D / Alt+P** audibly step the master volume in 10% grid steps and saturate (not wrap) at
   0% (muted) / 100% (full); `--volume 0` silent, `--volume 100` == pre-M52 audio.
2. **Alt+F** still toggles fast-disk (moved off Alt+D); stderr says `(Alt+F)`.
3. **Alt+S ON** then a real game save persists to the host `.dsk` on swap/exit; **Alt+S OFF**
   then the write is discarded at exit (not written to host).
4. A real game save persists **by default** now (disk-writable ON) with no flag; **`--no-disk-writable`**
   is the read-only escape hatch.
