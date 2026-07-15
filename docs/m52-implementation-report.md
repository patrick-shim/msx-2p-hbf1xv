# M52 Implementation Report — SDL3 Master Volume + Hotkey Reshuffle + Disk-Writable Default ON (DEC-0079 / REQ-M52-002)

> Provenance note: the developer agent returned this report inline (its operating guardrail
> returns findings as text rather than files); the coordinator persisted it here VERBATIM on
> 2026-07-15 for the QA handoff, adding only this note and §7. Coordinator independently
> re-verified before persisting: `git status` = exactly the 13 modified files +313/−40 plus
> untracked `src/frontend/master_volume.h`; the §6.5 HARD-EXCLUDE `git diff` = EMPTY.

## 1. Milestone Target
Implement `docs/m52-planner-package.md` slices S1→S4: a post-mix SDL3 master volume control
(helper + presenter scalar + `--volume` CLI + `<volume>` XML + CLI>XML>default resolution +
wiring + Alt+D/Alt+P hotkeys), fast-disk hotkey moved Alt+D→Alt+F, disk-writable default
flipped ON (SDL3 resolver + shipped XML) with `--no-disk-writable` and an Alt+S runtime
bind/unbind toggle, plus docs/round-trip updates. Frontend/config-only; zero
`src/core`/`src/devices/cpu`/device-timing edits.

## 2. Code Changes (per slice, file:line anchors)

**New helper (S1):** `src/frontend/master_volume.h` (NEW, SDL-free header-only) —
`apply_master_gain_sample` (identity at 100, `s*vol/100` clamped), `apply_master_gain`
(in-place, no-op at 100, zero at 0), `step_master_volume` (grid-snap ±step, CLAMP not wrap),
`clamp_master_volume`.

**S1 — master volume core/plumbing:**
- `src/frontend/sdl3_audio_presenter.h` — added `int master_volume_ = 100;`, reusable
  `scaled_buffer_`, `set_master_volume()`/`master_volume()`.
- `src/frontend/sdl3_audio_presenter.cpp` — post-mix gain applied in `pump_and_push` (:57),
  `pump_and_push_paced` batch (:133), and the LIVE `push_produced_paced` (:201-207) with the
  **byte-identity short-circuit at 100** (pushes original bytes; below 100 copies the pushed
  region into `scaled_buffer_` so the caller's buffer is never mutated); `set_master_volume()`
  (:213).
- `src/frontend/sdl3_cli.{h,cpp}` — `std::optional<int> volume;`; `--volume` parse mirroring
  `--persistence` (range `[0..100]`, "not a valid integer"/"out of range" errors).
- `src/machine/emulator_config.{h,cpp}` — `int master_volume = 100;` field + `<volume percent>`
  parse (mirrors `persistence@percent`; out-of-range warns + keeps 100).
- `src/frontend/config_runtime.{h,cpp}` — `ResolvedRuntimeConfig::master_volume` + resolver line
  `r.master_volume = parsed.volume.has_value() ? *parsed.volume : cfg.master_volume;`.
- `src/frontend/sdl3_app.h` — `Sdl3AppConfig::master_volume = 100` (struct default stays unity —
  anti-drift); `master_volume_` member + `kVolumeStep=10`; `on_volume_step_hotkey`/
  `on_disk_writable_toggle_hotkey` decls.
- `src/frontend/sdl3_main.cpp` — `config.master_volume = runtime.master_volume;`;
  `Sdl3App::init()` applies `set_master_volume` + caches `master_volume_` after
  `audio_presenter_->init()`.

**S2 — hotkeys** (`src/frontend/sdl3_app.cpp` :516-562): Alt+F fast-disk (moved from Alt+D,
fresh-keydown, message `(Alt+F)`); Alt+D volume-down / Alt+P volume-up (**key-repeat honored**
— `!repeat` guard dropped); Alt+S disk-writable toggle (fresh-keydown). Handlers
`on_volume_step_hotkey` and `on_disk_writable_toggle_hotkey` added after
`on_persistence_mode_hotkey`. Only the letter keydown is swallowed; standalone Alt still
reaches the matrix.

**S3 — disk-writable default ON + Alt+S semantics:** `emulator_config.h` `disk_writable`
default `false→true` (XML base default); `Sdl3AppConfig`/`ParsedSdl3Cli` struct defaults
**stay false** (anti-drift); `--no-disk-writable` added (sets false + specified, last-wins);
Alt+S handler binds `DiskImage::set_host_path(current path)` on ON (late-ON flushes whole
image) and `set_host_path({})` on OFF (unbind → future flushes no-op; in-memory writes kept but
discarded at exit). **Zero `src/devices/fdc/` edits.** Headless `src/main.cpp` disk-writable
default unchanged (stays OFF).

**S4 — docs/config:** `sony_msx_hbf1xv.xml` — `<disk-writable enabled="true"/>` + new
`<volume percent="100"/>` with comments; `sdl3_main.cpp` banner + `print_usage`
(Alt+F/Alt+D/Alt+P/Alt+S, `--volume`, `--no-disk-writable`, new Sound section); `README.md`
(feature bullets + flags list + config-field list).

Diff stat (tracked): 13 files, **+313 / −40**; plus new `src/frontend/master_volume.h`.

## 3. Unit Test Results
Test count **245 → 248** (+3 new executables). New/extended unit suites (all Passed in the
full run):
- `frontend_master_volume_unit_test` (#182, NEW) — identity@100 over every int16, silence@0,
  mid attenuation/truncation, extreme no-overflow, buffer no-op@100/zero@0, and step
  CLAMP-not-wrap incl. S2-1 values.
- `frontend_sdl3_cli_unit_test` (#151) — `--volume` valid/out-of-range/non-numeric/missing +
  `--no-disk-writable` last-wins.
- `machine_emulator_config_unit_test` (#178) — `<volume>` valid/out-of-range/bad/omitted;
  **§4.5 flip** applied.
- `frontend_config_runtime_unit_test` (#180) — volume 5-case precedence matrix + disk-writable
  flipped-default precedence; **§4.5 flip** applied.

## 4. Integration Test Results
- `frontend_master_volume_audio_identity_integration_test` (#183, NEW) — MachineAudioMixer PCM:
  gain@100 byte-identical to un-gained mix, @0 all-zero, mid deterministic + attenuation-only;
  two-run determinism.
- `frontend_disk_writable_toggle_flush_integration_test` (#184, NEW) — ON persists+reload
  byte-identical; OFF unbind → flush no-op + file unwritten + in-memory write kept (no
  rollback) + discarded at exit; late-ON flushes whole image; two-run determinism.
- `frontend_config_runtime_wiring_integration_test` (#181) — volume + disk-writable reach
  resolved config with correct precedence + determinism gate.
- `machine_hbf1xv_shipped_config_roundtrip_integration_test` (#187) — `DIFF(master_volume)` +
  `<volume`/`percent="100"` tokens; shipped == compiled defaults, zero warnings.
- FDC write-integrity preserved (S3-7): `devices_fdc_wd2793_write_{earlybyte,firstbyte,stall}`,
  `machine_hbf1xv_m36_disk_save_persist` all Passed.

**Full ctest: 247/247 = 100% passed, 0 failed** (Total 50.58 s), excluding only the one slow
CPU-exerciser #141 (see §5). All M52 tests green.

## 5. Known Issues / Residuals
- **ZEXALL (`hbf1xv_m24_zexall_system_test`, #141) time-boxed out of the reported run.** It is
  a 20-30 min Debug Z80 exerciser, explicitly NOT required for this frontend/config milestone
  (DEC-0077 precedent), and **provably CPU-unaffected**: the mandatory empty-`git diff`
  accuracy gate confirms `src/core` and `src/devices/cpu` are byte-identical to pre-M52, so
  its result is unchanged from the green baseline. A standalone confirmation run was left
  executing at report time; it did not surface a failure (it was mid-execution, not failed).
- **Build operational note:** the first build hit `LNK1168` because a stale
  `sony_msx_sdl3.exe` (PID 63872) held the output; cleared it and relinked cleanly (both exes
  built, 0 compile errors). Not a code issue.
- **Deviation from package prose (documented in `master_volume.h`):** `step_master_volume`
  uses a direction-aware grid snap (up→next grid above, down→next grid below), which satisfies
  **all** enumerated §4.1 S2-1 acceptance values (incl. `step(55,+1,10)=60` and
  `step(55,-1,10)=50`). The package's §2.3 prose formula `round_to_grid(current)+dir*step`
  would yield 70 for the off-grid +1 case, contradicting its own S2-1 example; the developer
  followed the concrete S2-1 acceptance values (the testable oracle). CLAMP-not-wrap preserved
  exactly.
- Host-hotkey event wiring (which scancode+mod is swallowed) has no direct ctest by
  established discipline (needs a live SDL pump) — the decidable logic (`step_master_volume`,
  DiskImage bind/unbind flush) is fully unit/integration-tested; the rest is QA manual smoke
  per package §6.

## 6. Evidence Gates
1. `validate-assets.ps1` — **PASS** (7 BIOS + `fmpac.rom`).
2. `checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — **written** (SHA256).
3. `cmake --build build --config Debug` — **0 errors**, `sony_msx_headless.exe` +
   `sony_msx_sdl3.exe` built.
4. `ctest -C Debug` — **247/247 (100%) pass**; ZEXALL #141 excluded (see §5).
5. Empty-diff accuracy gate over the HARD-EXCLUDE timing set — **EMPTY (PASS)**;
   `src/devices/fdc` also untouched.
6. openMSX A/B + ZEXALL not required (frontend/config-only) — correctly not run.

### §4.5 enumerated intended flips (the ONLY expectation changes; each carries a DEC-0079 comment)
1. `tests/unit/machine/emulator_config_unit_test.cpp` — `kDefaults.disk_writable == false` →
   `== true` (+ added `master_volume == 100`).
2. `tests/unit/frontend/config_runtime_unit_test.cpp` — `NoConfig_DiskWritableOff`
   (`!r.disk_writable`) → `NoConfig_DiskWritableOn` (`r.disk_writable`).
3. `tests/integration/machine/hbf1xv_shipped_config_roundtrip_integration_test.cpp` — added
   `<volume`/`percent="100"` tokens + `DIFF(master_volume)`; shipped
   `<disk-writable enabled="true"/>`.
No other oracle weakened; the added disk-writable/volume precedence cases only strengthen
coverage.

## 7. QA Handoff (coordinator-appended from the developer's summary)
- Slices S1–S4 all implemented; acceptance S1-1…S4-2 and M-1…M-6 met (M-1 full-volume
  byte-identity proven by test #183; M-5 emulation fixtures byte-identical, only the 3 §4.5
  flips changed).
- Re-verify from the single `build/` tree: clean rebuild → `ctest -C Debug`. ZEXALL optional
  (~20-30 min, empty-diff-proven unchanged).
- **NOT committed** — coordinator owns landing. `tests/` and `docs/` are gitignored
  (pre-existing convention), so the new/edited test files and `docs/asset-checksums.txt` are
  on disk but untracked; only the 13 tracked files + new `src/frontend/master_volume.h` appear
  in `git status`.
- Blast radius MED (disk-writable default change) → NORMAL human-release gate, no auto-close.
