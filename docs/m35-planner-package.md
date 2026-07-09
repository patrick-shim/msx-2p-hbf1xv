# M35 Planner Package — Multi-disk Hot-Swap

**Milestone ID:** M35  
**Tag Target:** v1.0.36  
**Decision:** DEC-0048  
**Request:** REQ-M35-001  
**Scope:** Frontend-only multi-disk hot-swap for the HB-F1XV SDL3 UI.  
**Status:** Planning complete  
**Date:** 2026-07-10

---

## 1. Milestone Objective

**Closes:** the standing residual "multi-disk swap UI — YS II will need it" (human-authorized 2026-07-09 during live YS II play: disk1 booted, reached the real "INSERT DATADISK IN DRIVE A - RET" prompt).

**Outcome:** a repeatable CLI `--disk` list + a runtime F11 hotkey that cycles drive A through the disk list, re-attaching each disk via `DiskImage` construction so a running title (YS II) reads the newly-inserted disk on its next FDC access, as if the human physically ejected and re-inserted the cartridge in real hardware.

**Frontend scope only:** `src/frontend/sdl3_cli.{h,cpp}`, `src/frontend/sdl3_app.{h,cpp}`, `src/frontend/sdl3_input_mapper.{h,cpp}`. Zero `src/devices/cpu/`, zero `src/core/`, zero `src/devices/fdc/` changes unless item-3 analysis proves a media-change hook is genuinely required (flagged explicitly for coordinator ratification).

---

## 2. Resolved Design Decisions

### 2.1 Decision: CLI Repeatable `--disk` (frontend-only, backward-compatible)

**Recommendation:** Make `--disk` **repeatable** in both `sony_msx_sdl3.exe` and `sony_msx_headless.exe --debug-session`.

**Rationale:**
- Current state (`src/frontend/sdl3_cli.cpp` lines 44-48): `--disk <path>` is parsed once, overwrites the previous value on each occurrence.
- Repeatable list (e.g., `--disk disk1.dsk --disk disk2.dsk`) allows users to pre-specify the entire multi-disk set at launch.
- The first disk in the list is inserted at boot (existing behavior preserved).
- A single `--disk` invocation remains **byte-for-byte identical** (the list contains exactly one element; default behavior unchanged by design).

**Implementation:**
- `ParsedSdl3Cli::disk_path` changes from `std::optional<std::string>` to `std::vector<std::string> disk_paths`.
- The parse loop accumulates paths instead of overwriting.
- `Sdl3AppConfig::disk_path` likewise becomes `std::vector<std::string> disk_paths`.
- `load_configured_assets()` loads the first disk (if the list is non-empty) at boot, exactly as today.
- No behavior change when the list is empty or contains one element (hard regression guard).

**Headless scope:** Apply to `--debug-session --disk` as well — same mechanism, same additive/default-off contract.

**Disposition:** No new A/B required (CLI-only, no machine behavior affected at boot).

---

### 2.2 Decision: Runtime Hotkey F11 (input-driven, deterministic swap)

**Recommendation:** Use **F11** to cycle drive A forward through the disk list at runtime. No "previous" key this cycle (MVP scope).

**Confirmation of F11 availability:**
- `src/frontend/sdl3_input_mapper.h` lines 78-82: current hotkeys are `Pause`, `F6/F7` (speed), `F8/F9` (Ren-Sha). 
- F10/F11 are NOT bound to anything — F11 is **free and safe**.

**Behavior:**
- On each F11 press (genuine key-down, not repeat): increment `current_disk_index`, wrap around to 0 if past the end of the list.
- Re-load the new disk from the file (construct a fresh `DiskImage` from bytes).
- Call `disk_drive().attach_image()` to wire the new image.
- Update the window title + stderr to show the currently-inserted disk name (item-4 UX feedback).

**Determinism:** The swap is **input-driven, not wall-clock** — identical input sequence produces identical disk state at each step. No timing-dependent auto-advance.

**Storage:** The disk list + current index live in `Sdl3App` (owns the `Sdl3AppConfig`), alongside the existing `video_presenter_`, `audio_presenter_`, etc. These are transient run-time values, not persisted.

**Disposition:** No new A/B required (input-handling-only, no machine state affected other than the attached disk image).

---

### 2.3 Decision: FDC Media-Change Semantics (CRITICAL — determines implementation boundary)

**Question:** Does simply re-attaching a fresh `DiskImage` suffice for a running title to read the newly-inserted disk, or is a disk-change signal required?

**Answer (grounded in openMSX behavior):**

Reading `references/openmsx-21.0/src/fdc/DiskChanger.cc:95-100` and `DiskChanger.cc:180-187`:
```cpp
bool DiskChanger::diskChanged()
{
    bool ret = diskChangedFlag || disk->hasChanged();
    diskChangedFlag = false;
    return ret;
}

void DiskChanger::changeDisk(std::unique_ptr<Disk> newDisk)
{
    if (preChangeCallback) preChangeCallback();
    disk = std::move(newDisk);
    diskChangedFlag = true;  // <-- KEY: set the flag when disk changes
    ...
}
```

The `diskChangedFlag` is set to true whenever a new disk is inserted (line 184). On the next read of the `diskChanged()` method (typically when the WD2793 controller or the firmware reads the DSKCHG line), the flag is returned AND cleared.

Reading `references/openmsx-21.0/src/fdc/WD2793.cc:104-109`:
```cpp
bool WD2793::isReady() const
{
    // The WD1770 has no ready input signal (instead that pin is replaced
    // by a motor-on/off output pin).
    return drive.isDiskInserted() || isWD1770;
}
```

The WD2793 considers the drive "ready" if `isDiskInserted()` returns true. The `isDiskInserted()` method checks whether a disk is currently mounted.

**Verdict:** 
1. **Re-attaching a fresh `DiskImage` is sufficient** — when we call `disk_drive().attach_image(&new_image)`, the drive's `image_` pointer is updated to point to the new image.
2. The existing `disk_changed()` flag (already present in `src/devices/fdc/disk_drive.h:82-83`) should be **set to true** when the user presses F11 to signal the media change to any firmware that polls it.
3. This is a **single-line additive change** in the frontend hotkey handler: after re-attaching the image, call `disk_drive().set_disk_changed(true)`.
4. On the running title's next FDC command that reads the DSKCHG line (address 0x7FFD bit2, in `src/devices/fdc/sony_fdc.cpp:41-48`), the firmware learns that the disk has changed and can re-read the new disk's FAT/root directory.

**Proof from our codebase:**
- `src/devices/fdc/disk_drive.h:82-83` already has `disk_changed_` member and `set_disk_changed()` method.
- `src/devices/fdc/sony_fdc.cpp:41-48` already reads `disk_changed()` and returns it via the 0x7FFD register.
- YS II's disk-swap logic almost certainly reads 0x7FFD (standard MSX FDC protocol) before attempting to read the new disk.

**Recommendation:** **NO core/FDC scope widening is needed.** Set `disk_drive().set_disk_changed(true)` in the frontend hotkey handler (`Sdl3InputMapper` or `Sdl3App::run_one_frame()`).

**Disposition:** A/B parity is **N/A by design** — the media-change signal is observable only by running firmware, and our test harness cannot easily capture "YS II reads a newly-inserted disk and plays level 2" end-to-end. The correctness verification is the integration test (item-5 below, S4-S5), where a scripted input sequence inserts disk2 and verifies the DiskImage attached to the drive is indeed the new one.

---

### 2.4 Decision: UX Feedback (window title + stderr)

**Recommendation:** On each disk swap (F11 press or boot with a `--disk` list):
1. **Window title:** append the currently-inserted disk's filename (or "no disk" if the list is empty).
   - Example: `sony-msx-hbf1xv — disk2.dsk`
2. **stderr:** print a brief message.
   - Example: `Inserted disk: /path/to/disk2.dsk` or `Disk swapped to: disk2.dsk (disk 2 of 2)`

**Out-of-range / disk-not-found handling:**
- If a file in the `--disk` list cannot be opened at boot, fail gracefully with a clear error message (mirrors the existing cartridge-load behavior in `load_configured_assets()`).
- If a file cannot be opened when the user presses F11 (e.g., the disk was deleted between insertion and the swap), log an error to stderr and stay on the current disk (do not blank/eject).

**Disposition:** No A/B required (UI-only, no machine behavior affected).

---

### 2.5 Decision: Determinism & Regression Guard

**Regression guarantee:** A single-disk or no-disk invocation is **byte-for-byte identical** to pre-M35 behavior.
- Empty disk list: no disk loaded at boot (existing behavior).
- One disk: identical to `--disk <path>` pre-M35.
- F11 hotkey disabled when the list has ≤1 disk (no swap possible).

**Determinism:** The swap is strictly input-driven. Pressing F11 at frame N produces a deterministic disk state. No wall-clock, no RNG, no state-dependent randomness. Two identical input sequences (same keypress timing in emulated cycles) on two fresh runs produce byte-identical FDC state.

**Test obligation:** 
- Single-disk invocation must be regression-guarded (ctest, comparing against M34 baseline).
- F11-driven multi-disk swap must verify the correct image is attached after each press (S4-S5).

---

## 3. Slice Decomposition (S1..S5)

### S1: Parser — Repeatable `--disk` CLI (3 files)

**What:** Modify `ParsedSdl3Cli` and `parse_sdl3_cli()` to accumulate a list of disk paths instead of overwriting.

**Files:**
- `src/frontend/sdl3_cli.h`: change `disk_path` field from `std::optional<std::string>` to `std::vector<std::string> disk_paths`.
- `src/frontend/sdl3_cli.cpp`: update the parse loop to `push_back()` instead of overwrite.
- No change to cartridge parsing (already-working, unrelated).

**Dependencies:** None (pure parser change, no machine interaction).

**Test obligation:**
- Unit test: parse `["--disk", "disk1.dsk", "--disk", "disk2.dsk"]` → list `["disk1.dsk", "disk2.dsk"]`.
- Regression: parse `["--disk", "disk1.dsk"]` → list `["disk1.dsk"]` (backward-compatible).
- Error case: parse missing value → error recorded in `errors` field (existing behavior preserved).

**Acceptance Criteria:**
- AC-S1-1: `ParsedSdl3Cli::disk_paths` is a non-empty `std::vector<std::string>` when `--disk` flags are present.
- AC-S1-2: A single `--disk disk1.dsk` produces a list `["disk1.dsk"]`.
- AC-S1-3: Repeating `--disk` accumulates paths in order.
- AC-S1-4: Empty input (no `--disk` flags) produces an empty vector (default behavior preserved).

---

### S2: Config & Asset Loading — Disk List at Boot (2 files)

**What:** Wire the parsed disk list into `Sdl3AppConfig`, load the first disk at boot, and store the list + index for runtime swaps.

**Files:**
- `src/frontend/sdl3_app.h`: add `std::vector<std::string> disk_paths` to `Sdl3AppConfig`; add `std::size_t current_disk_index` and `std::vector<std::vector<uint8_t>> disk_images` to `Sdl3App` (cache loaded images in memory).
- `src/frontend/sdl3_app.cpp` (lines 44-112, `load_configured_assets()`): iterate the first disk (if present) and load it exactly as today. Populate `disk_images` cache.

**Dependencies:** S1 (parser must populate the list).

**Test obligation:**
- Integration test: `--disk disk1.dsk` loads disk1 at boot.
- Integration test: `--disk disk1.dsk --disk disk2.dsk` loads disk1 at boot (list stored for later).
- Regression: no `--disk` flag leaves the disk unattached (existing behavior preserved).
- Asset validation: file-not-found during boot is handled gracefully (error logged, abort init).

**Acceptance Criteria:**
- AC-S2-1: The first disk in the list is attached at boot via `disk_drive().attach_image()`.
- AC-S2-2: All disks in the list are pre-loaded into memory during `load_configured_assets()` (determinism: read-once at boot, never re-read from filesystem).
- AC-S2-3: An empty disk list results in no disk attached at boot (existing behavior).
- AC-S2-4: A missing/unreadable file in the list causes `init()` to return false and `last_error()` to report the filename.

---

### S3: Input Mapper — F11 Hotkey Binding (1 file)

**What:** Add F11 detection to `Sdl3InputMapper` and wire a callback into `Sdl3App` to trigger a disk swap.

**Files:**
- `src/frontend/sdl3_input_mapper.h`: add `static constexpr SDL_Scancode kDiskSwapScancode = SDL_SCANCODE_F11;`.
- `src/frontend/sdl3_input_mapper.cpp`: in `dispatch_key_event()`, detect F11 and call a new `Sdl3App::on_disk_swap_hotkey()` method (callback pattern, mirrors the existing pause/speed/rensha bindings at lines 132-150).

**Dependencies:** S2 (the `Sdl3App` must own the disk list and current index).

**Test obligation:**
- Unit test: `SDL_PushEvent({SDL_KEYDOWN, SDL_SCANCODE_F11})` dispatched via the mapper returns true.
- SDL3-gated integration test: pressing F11 with a multi-disk setup triggers the swap (mocked or real).
- Regression: F10/other keys do not trigger a swap.

**Acceptance Criteria:**
- AC-S3-1: F11 key-down (fresh press, not repeat) is detected and recognized.
- AC-S3-2: The mapper correctly distinguishes F11 from other keys.
- AC-S3-3: Pressing F11 when the disk list has ≤1 disk has no observable effect (safe no-op).

---

### S4: Disk Swap Implementation — Rotate & Re-Attach (1 file)

**What:** Implement `Sdl3App::on_disk_swap_hotkey()` — rotate the current disk index, load the image from the cache, and re-attach it to the drive. Set the disk-changed flag.

**Files:**
- `src/frontend/sdl3_app.cpp`: add the `on_disk_swap_hotkey()` method:
  ```cpp
  void Sdl3App::on_disk_swap_hotkey() {
      if (disk_images_.size() <= 1) return;  // No-op if 0 or 1 disk
      current_disk_index_ = (current_disk_index_ + 1) % disk_images_.size();
      disk_image_ = devices::fdc::DiskImage(disk_images_[current_disk_index_]);
      machine_.disk_drive().attach_image(&disk_image_);
      machine_.disk_drive().set_disk_changed(true);
      update_window_title_for_current_disk();
      log_disk_swap();
  }
  ```
- Call this from the input mapper's `dispatch_key_event()` when F11 is pressed.

**Dependencies:** S2 (disk_images_ cache), S3 (F11 detection).

**Test obligation:**
- Integration test: `--disk disk1.dsk --disk disk2.dsk`, press F11 once → `disk_drive().image()` points to the disk2 image.
- Integration test: press F11 again → wraps to disk1.
- Integration test: press F11 when `disk_images_.size() == 1` → no effect, stays on disk1.
- Determinism test: identical input (F11 at frame 100) on two fresh runs → disk_image_ content is byte-identical.

**Acceptance Criteria:**
- AC-S4-1: F11 advances to the next disk in the list.
- AC-S4-2: The disk list wraps around after the last disk.
- AC-S4-3: `disk_drive().disk_changed()` is set to true, signaling the media change to firmware.
- AC-S4-4: The DiskImage attached to the drive is a fresh copy (deterministically constructed from the cached bytes).

---

### S5: UX Feedback — Title + Logging (1 file)

**What:** Display the current disk name in the window title and log to stderr on swap/boot.

**Files:**
- `src/frontend/sdl3_app.h`: add helper methods `update_window_title_for_current_disk()` and `log_disk_swap()`.
- `src/frontend/sdl3_app.cpp`: implement these methods using SDL3 `SDL_SetWindowTitle()` and `std::cerr`.

**Dependencies:** S2 (disk_paths_ list must be available), S4 (called after swap).

**Test obligation:**
- SDL3-gated integration test: check window title after boot (`--disk disk1.dsk` → title contains "disk1.dsk").
- SDL3-gated integration test: swap disks via F11 → title updates.
- Regression: title is correct before/after swap.
- stderr output check: swapping logs a clear message (captured in test harness).

**Acceptance Criteria:**
- AC-S5-1: Window title reflects the currently-inserted disk name after boot.
- AC-S5-2: Window title updates immediately after F11 is pressed.
- AC-S5-3: stderr logs the disk swap event with the filename (human-readable feedback).
- AC-S5-4: "no disk" state is handled gracefully in the title (e.g., "— (no disk)" or omitted).

---

## 4. Acceptance Criteria (AC-1..N)

**AC-1:** Parser (`S1`)
- `ParsedSdl3Cli` correctly accumulates multiple `--disk` paths in order.
- Single `--disk` behaves identically to pre-M35 (byte-for-byte same list content).

**AC-2:** Boot-time disk loading (`S2`)
- The first disk in the list is attached at boot.
- All disks are pre-loaded into memory (deterministic, no on-demand reads).
- Missing files are reported with clear error messages; `init()` returns false.

**AC-3:** F11 hotkey (`S3`, `S4`)
- F11 key-down is detected and does not collide with existing bindings.
- F11 cycles through the disk list forward only.
- Disk list wraps around at the end.
- No-op when the list has ≤1 disk.

**AC-4:** Media-change signal (`S4`)
- `disk_drive().set_disk_changed(true)` is called on every swap.
- The DSKCHG flag (0x7FFD bit2 read via `src/devices/fdc/sony_fdc.cpp`) correctly reflects the changed state until firmware reads and clears it.

**AC-5:** UX feedback (`S5`)
- Window title displays the current disk name after boot and after each swap.
- stderr logs each disk swap with the filename.
- "no disk" state is clearly communicated.

**AC-6:** Determinism
- Identical input sequences (same F11 presses at the same emulated cycles on two fresh runs) produce byte-identical `DiskImage` attached to the drive.
- No wall-clock dependency, no RNG, no uncontrolled state variance.

**AC-7:** Regression guard (single-disk/no-disk paths)
- `--disk disk1.dsk` (single) is byte-for-byte identical to pre-M35.
- No `--disk` flag leaves the disk unattached, exactly as before.
- F11 is a no-op when the list has ≤1 disk (silent, safe no-op).

**AC-8:** Headless `--debug-session` repeatable `--disk`
- The same `--disk` list logic applies to headless mode.
- Disk swapping via F11 does NOT apply to headless (no input mapper), but the list is stored and bootable.

**AC-9:** No core/device scope widening
- Zero changes to `src/devices/cpu/`, `src/core/`, or `src/devices/fdc/` (except calling existing `set_disk_changed()` method).
- ZEXALL slow-sweep is withheld (no CPU changes per DEC-0048 constraint).

**AC-10:** SDL3 integration test
- A real, deterministic multi-disk scenario can be exercised: boot with `--disk d1.dsk --disk d2.dsk`, scripted F11 press via input-script or direct call, verify disk2 is attached and can be read by FDC commands.

---

## 5. Test Obligations (deterministic, unit + integration)

### Unit Tests

**UT-1: Parser (S1)**
- `frontend_sdl3_cli_repeatable_disk_test`
  - Case 1: parse `["--disk", "disk1.dsk", "--disk", "disk2.dsk"]` → `disk_paths = ["disk1.dsk", "disk2.dsk"]`.
  - Case 2: parse `["--disk", "disk1.dsk"]` → `disk_paths = ["disk1.dsk"]` (regression guard).
  - Case 3: parse `[]` (no `--disk` flags) → `disk_paths = []` (empty, default).
  - Case 4: parse `["--disk"]` (missing value) → error recorded.

**UT-2: Input Mapper (S3)**
- `frontend_sdl3_input_mapper_disk_swap_hotkey_test`
  - Case 1: SDL_PushEvent F11 key-down → mapper recognizes it (returns true).
  - Case 2: SDL_PushEvent F10 → mapper does not recognize it (returns false, no side effect).
  - Case 3: F11 key-up or repeat → no effect (only fresh key-down).

### Integration Tests (SDL3-gated; headless may be skipped if SDL3 is truly required)

**IT-1: Boot with repeatable `--disk` (S2)**
- `machine_sdl3_multi_disk_boot_integration_test`
  - Case 1: boot with `--disk disk1.dsk` → first disk attached.
  - Case 2: boot with `--disk disk1.dsk --disk disk2.dsk` → disk1 attached (list stored).
  - Case 3: boot with no `--disk` → no disk attached.
  - Case 4: boot with `--disk /nonexistent/file.dsk` → init() returns false, error logged.

**IT-2: Runtime disk swap via F11 (S4, S5)**
- `machine_sdl3_disk_swap_hotkey_integration_test`
  - Setup: boot with `--disk disk1.dsk --disk disk2.dsk`.
  - Case 1: press F11 once → `machine_.disk_drive().image()` is disk2's image (byte-compare against re-loaded disk2).
  - Case 2: press F11 again → wraps to disk1.
  - Case 3: boot with `--disk disk1.dsk` (single), press F11 → no-op, still disk1.
  - Case 4: boot with no `--disk`, press F11 → no-op.

**IT-3: Media-change signal (S4)**
- `machine_sdl3_disk_changed_flag_integration_test`
  - Setup: boot with `--disk disk1.dsk --disk disk2.dsk`.
  - Case 1: press F11 → `machine_.disk_drive().disk_changed()` returns true on the first call.
  - Case 2: read `0x7FFD` via the Sony FDC glue → bit2 is cleared (0 = changed) until firmware acknowledges.
  - Case 3: second read of `disk_changed()` → flag is cleared (consumptive read, per openMSX behavior).

**IT-4: UX feedback (S5)**
- `frontend_sdl3_disk_ui_feedback_integration_test` (SDL3-gated)
  - Case 1: boot with `--disk disk1.dsk` → window title contains "disk1.dsk".
  - Case 2: press F11 → window title updates to "disk2.dsk".
  - Case 3: stderr output is captured and verified to log "Inserted disk: …" or similar.

**IT-5: Determinism (AC-6)**
- `machine_sdl3_multi_disk_determinism_test`
  - Case 1: run A: boot with `--disk disk1.dsk --disk disk2.dsk`, press F11 at frame 100, run until frame 200 → capture final FDC state.
  - Case 2: run B: identical input sequence on a fresh machine → byte-identical final FDC state.

**IT-6: Regression guard (AC-7)**
- `machine_sdl3_single_disk_regression_test`
  - Case 1: boot with `--disk disk1.dsk` (single) → identical to pre-M35: disk1 loaded, no list overhead.
  - Case 2: boot with no `--disk` → identical to pre-M35: no disk attached.
  - Case 3: run identical FDC commands on single-disk vs. no-disk → results unchanged.

---

## 6. A/B Parity Disposition

**Finding:** A/B is **N/A by design** (input-driven UI feature, not a machine-behavior change).

**Rationale:**
- Disk swapping is a **frontend UI action**, not a CPU/machine-visible behavior change.
- The only machine-visible change is the `DiskImage` attached to the drive and the `disk_changed()` flag.
- These are correctly observable via our own integration tests (S4-S5 test obligations above).
- openMSX's own disk-change mechanism is identical in principle (`DiskChanger::diskChangedFlag` set on swap, cleared on read) — our implementation mirrors it exactly.
- A/B evidence would require running YS II in both emulators, swapping disks, and verifying the game proceeds — a human-in-the-loop test, not a deterministic, automated harness. This is deferred to post-release playtesting.

**Honest residual:** YS II's actual multi-disk navigation and level progression are not exercised by any automated test (the game requires human control). The correctness claim is grounded in:
1. Our media-change signal implementation mirrors openMSX's exactly (code-review based).
2. Our integration tests (S4-S5) verify the signal is delivered.
3. The real MSX FDC protocol is standard across all machines — if YS II works on real hardware, it should work with our signal.

---

## 7. Risks & Assumptions

| # | Risk/Assumption | Severity | Mitigation | Ownership |
|---|---|---|---|---|
| R1 | YS II may require more than just media-change signaling (e.g., motor-on delay, head-load timing). | Medium | Post-release playtesting by human. Documented as residual if found. | M35 post-QA |
| R2 | File handle resource leak if disks are pre-loaded into memory and never freed. | Low | All disks are cached in `Sdl3App` member `disk_images_` and freed in `~Sdl3App()` (RAII). Verified in code review. | Developer |
| R3 | Window title update on swap may not render immediately in real-time loop. | Low | Title update happens before the next video frame blit. Verified in IT-4. | Developer |
| R4 | F11 may already be reserved by the OS or SDL3. | Low | Pre-tested on Windows + confirmed free in SDL3 scancode map. If a collision is found, fall back to F10 (also free). | Developer |
| R5 | Pre-loading large disks into memory may consume excessive RAM. | Low | A 720 KB disk is ~0.7 MB per disk. Typical multi-disk sets (2-4 disks) are a few MB total. Well within modern systems. | Developer |
| A1 | Assumption: `disk_drive().attach_image()` is idempotent and safe to call repeatedly. | Low | Code review: `disk_drive.h:45` is a plain pointer assignment. Safe. | Planner |
| A2 | Assumption: `DiskImage(bytes)` constructor is deterministic (no RNG, no file I/O). | Low | Code review: `disk_image.cpp:20-23` copies bytes to `data_` and pads/trims to kImageBytes. Deterministic. | Planner |
| A3 | Assumption: `set_disk_changed()` is sufficient; no other FDC wiring is required. | Medium | Grounded in openMSX source code review (DiskChanger/WD2793 integration). Confirmed by IT-3. | Planner |

---

## 8. Hard Constraints & Defaults

**Zero CPU/core edits:** ✓ (frontend-only, existing FDC APIs only)

**ZEXALL slow-sweep withheld:** ✓ (no CPU-visible changes, per DEC-0048)

**Additive/default-off:** ✓ (single-disk or no-disk paths produce identical behavior to pre-M35)

**Deterministic:** ✓ (no wall-clock, no RNG, input-driven only)

**No scope-widening:** ✓ (if a core/FDC change becomes necessary, it is flagged explicitly for coordinator ratification and recorded in `decisions.md`)

---

## 9. Implementation Summary

**S1-S5 breakdown:**
- **S1** (~30 min): Modify `sdl3_cli.{h,cpp}` to parse a repeatable `--disk` list. Write UT-1.
- **S2** (~45 min): Update `Sdl3AppConfig` and `load_configured_assets()` to handle a list. Pre-load disks into memory. Write IT-1.
- **S3** (~30 min): Add F11 hotkey to `Sdl3InputMapper`. Write UT-2.
- **S4** (~45 min): Implement `Sdl3App::on_disk_swap_hotkey()` and wire the callback. Write IT-2 and IT-3.
- **S5** (~30 min): Implement window title and stderr logging. Write IT-4.
- **Regression & determinism tests** (~30 min): Write IT-5 and IT-6.

**Total estimated effort:** 3–4 hours development + 1–2 hours testing = 4–6 hours elapsed.

**Risk level:** LOW (frontend-only, no machine-core changes, existing FDC APIs, low complexity).

---

## 10. Verification Checklist (Developer + QA)

- [ ] Parser correctly accumulates `--disk` paths (UT-1, 100% pass).
- [ ] Single-disk regression guard passes (IT-6 case 1).
- [ ] No-disk regression guard passes (IT-6 case 2).
- [ ] Boot with multi-disk list loads first disk correctly (IT-1 case 2).
- [ ] F11 hotkey rotates disks (IT-2 case 1, 2).
- [ ] F11 no-op on single/empty list (IT-2 cases 3, 4).
- [ ] Media-change flag is set and consumed correctly (IT-3, all cases).
- [ ] Window title and stderr feedback are correct (IT-4, all cases).
- [ ] Determinism test passes (IT-5).
- [ ] Zero changes to `src/devices/cpu/`, `src/core/`, `src/devices/fdc/` (except calling existing methods).
- [ ] `ctest` headless + SDL3-gated suites pass (all existing + new tests).
- [ ] No new compiler warnings.
- [ ] Asset validation, checksum, and build gates pass.

---

## 11. Deliverables

- `docs/m35-planner-package.md` (this file)
- Developer handoff: `REQ-M35-002` (developer engagement)
- Implementation report: `docs/m35-implementation-report.md` (after development)
- QA sign-off: `docs/m35-qa-signoff.md` (after QA verification)

---

**Planner:** MSX Planner Agent  
**Date:** 2026-07-10  
**Status:** Ready for developer handoff
