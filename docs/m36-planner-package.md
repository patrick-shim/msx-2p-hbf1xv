# M36 Planner Package: Autonomous Playtest/Live-QA Harness & Playtest Bug Investigation

- Milestone ID: **M36**
- Title: Autonomous Playtest/Live-QA Harness & Playtest Bug Investigation
- Date: 2026-07-10
- Planner: MSX Planner (opus)

## 1. Scope Summary

**Objective:** Deliver a deterministic, autonomous, vision-capable playtest/live-QA harness (`msx-playtest` agent + `/msx-playtest` command) that simulates human player actions by observing on-screen state, then use it to investigate and fix two critical bugs discovered in live human playtesting of YS II.

**Three Slices:**
1. **Phase 1 (S1-S3):** Autonomous playtest harness tooling, agent, command; proven by reaching a non-trivial landmark (proposed: YS II title/save menu).
2. **Phase 2A (S4-S5):** Investigate and fix Bug A (FM-PAC SRAM "NO S-RAM AVAILABLE") — deterministically unlock the SRAM protocol via the playtest harness.
3. **Phase 2B (S6-S7):** Investigate Bug B (black screen on building entry) — use playtest harness to reach the building deterministically, capture the black screen and state dumps, then root-cause (FDC vs VDP).
4. **Ride-Along (S8):** Strengthen R-M35-1 (multi-disk integration test to assert disk-index advances).

**Excluded:**
- Fix Bug B until root-cause is established (planner §2.4.2 — repro-first discipline).
- Audio output/presentation (covered under backlog E1 + C9 SDL3 milestone).
- Keyboard/joystick depth beyond the existing keystroke scripting infrastructure.
- Cartridge types beyond the six MVP types + KonamiSCC (backlog G1-G4 deferred).

**Non-Negotiables:**
- Determinism: every playtest run with an identical input script produces identical emulator state and frame output.
- Vision-capable opus agent (M36-S1) observes PNG frames and decides inputs; no human-in-the-loop required once trained.
- Bounded iteration: if the agent stalls (cannot progress toward the goal after N iterations), it may request a human hint; no infinite loops.
- A/B parity: Phase 2A/2B fixes capture openMSX evidence where applicable (behavior-affecting).
- No ZEXALL: Bug A is device-level (FM-PAC cartridge SRAM mapping); no CPU core edits.

## 2. Spec Boundaries & Architecture

### 2.1 Existing Tooling Reuse (Dependency Map)

| Component | Location | Reused For | Notes |
|-----------|----------|-----------|-------|
| **Input Script Format** | `src/machine/input_script.h` | Accumulating keystroke list | HBF1XV-INPUT-SCRIPT v1 format; T-state deterministic |
| **Input Script Player** | `src/machine/input_script.h:78-91` | Applied during playback | Monotonic cursor; each event applied exactly once |
| **Frame Dump Format** | `src/machine/frame_dump.h` | Screen capture serialization | HBF1XV-FRAME-DUMP v1 format; RGB555 pixels |
| **Frame Dump Parser** | `src/machine/frame_dump.{h,cpp}` | PNG conversion input | Round-trip parse() ↔ serialize() contract |
| **--debug-session Mode** | `src/main.cpp:160-300` | Emulator run orchestration | Headless loop with M27 deterministic stepping |
| **--input-script FLAG** | `src/main.cpp:275-285` | Load + play keystroke script | Exists and functional (M27) |
| **--dump-frame N FLAG** | `src/main.cpp:290-295` | Capture frame at T-state N | Exists and functional (M26) |
| **--dump-state FLAG** | `src/main.cpp:296-300` | Debug state dump | Exists; optional for repro diagnostics |
| **tools/frame-to-png.py** | `tools/frame-to-png.py` | Frame binary → PNG conversion | Takes HBF1XV-FRAME-DUMP v1 input; outputs PNG file |
| **tools/audio-dump-to-wav.py** | `tools/audio-dump-to-wav.py` | Audio binary → WAV (future) | Not required for M36; available for Phase 2+ |
| **peripherals/msx_key_names** | `src/peripherals/msx_key_names.{h,cpp}` | Key name resolution | `key_name_to_row_col()`; maps "UP", "DOWN", etc. |

**Conclusion:** All M27 infrastructure is reusable; only **additive frontend enhancements** are needed (§2.2).

### 2.2 Minimal Additive Enhancements

#### 2.2.1 Multi-Frame Capture in a Single Run (Optional)

**Observation:** The current `--dump-frame N` dumps exactly ONE frame at T-state N. For the observe-act loop, the playtest agent ideally captures frames at regular intervals (e.g. every 262 scanlines = ~73ms) within a single emulator invocation to avoid repeated cold-boots.

**Three Options (pick one per planner judgment):**

- **Option A (Minimal):** Wrap the existing tooling in a PowerShell script (`tools/playtest-runner.ps1`):
  - For each iteration: invoke `sony_msx_headless.exe --debug-session --input-script <script> --dump-frame <list>`
  - Capture multiple frames by running the same script multiple times, each time requesting frame dumps at different T-states
  - Pro: zero src/ edits, fast, proven pattern (mirrors M27/M29 A/B tooling)
  - Con: requires cold-boot per iteration (not free, but acceptable for Phase 1)

- **Option B (Moderate):** Add `--dump-frames-interval N` flag to `src/main.cpp`:
  - Emulator writes a frame dump every N T-states during the run
  - Numbered output: `frame_dump_0.txt`, `frame_dump_1.txt`, etc.
  - Pro: one emulator run per playtest cycle
  - Con: minor src/main.cpp edit; new flag surface

- **Option C (Deferred):** Expose a `dump_frame_on_interval()` API in `Hbf1xvMachine` for direct in-process testing.

**Recommendation:** **Option A** for M36 (zero src/ risk, mirrors M27 precedent of wrapping existing tools). If Phase 2 performance becomes critical, implement **Option B** as a small post-M36 enhancement.

#### 2.2.2 Frame-to-PNG Conversion in Playtest Loop

**Already Available:** `tools/frame-to-png.py` is a complete, working converter. The playtest agent simply calls it:
```
python tools/frame-to-png.py input.txt output.png
```

No changes needed.

#### 2.2.3 Playtest Agent Command Definition (New)

- **Agent Definition:** `.claude/agents/msx-playtest.md`
  - Model: **opus** (vision-capable, per DEC-0049)
  - Tools: Read, Glob, Write, Edit, Bash, TodoWrite, plus vision input (image reading)
  - Role: simulate a human player by observing PNG frames and deciding next keystroke/action
  - Interaction: collaborates with planner/developer/qa for diagnostics, requests human hint if stalled

- **Command Definition:** `.claude/commands/msx-playtest.md`
  - Syntax: `/msx-playtest goal="<goal>" [max-iterations=30] [state-dumps] [debug-log]`
  - Examples:
    - `/msx-playtest goal="reach title screen"` (YS II landing screen)
    - `/msx-playtest goal="reach save menu"` (in-game save dialog)
    - `/msx-playtest goal="enter a building"` (blue screen + interior graphics)
  - Output: saves the derived input script to `debug/playtest_<goal>_<timestamp>.script`, frame sequence to `debug/playtest_<goal>/frame_*.png`, and a playtest report to `debug/playtest_<goal>_report.md`

### 2.3 The Observe-Act Loop Protocol

**Deterministic loop** (no wall-clock nondeterminism; only emulator elapsed cycles):

```
ROUND N:
  1. Emulator: run_cycles with InputScriptPlayer loading script[:N]
  2. Capture: --dump-frame at interval (e.g. every ~73ms of emulated time)
  3. Convert: tools/frame-to-png.py → PNG
  4. Vision: opus agent observes PNG, landmark/anomaly detection
  5. Decision: agent outputs next keystroke(s) + T-state(s)
  6. Append: keystroke(s) → script (script = script + new_event)
  7. Iterate: N += 1, jump to step 1 (DETERMINISTIC REPLAY: prefix stays identical,
              new suffix is appended, so first N frames are byte-for-byte identical)

EXIT: goal reached || max-iterations exceeded || human hint received
OUTPUT: script, frame sequence, report
```

**Guarantees:**
- Determinism: given the same prefix script, the emulator produces the same state/frames (cycle-stamped script as the invariant).
- Non-destructive: each iteration only APPENDS to the script; never rewrites or backtracks.
- Regression-able: saved script can be re-played by any future milestone to reproduce the exact state at each landmark.

### 2.4 Bug A Analysis: FM-PAC SRAM as a System-Wide Memory-Addressing Problem

**SYSTEM ARCHITECTURE FRAMING:** Bug A is NOT a localized "add SRAM" patch. The FM-PAC SRAM window (0x4000–0x5FFF) **coexists and shares address space** with:
1. **FM-PAC ROM banks** (4-bank window, selected by 0x3FF7 bank register)
2. **OPLL register overlay** (I/O ports #7C/#7D, IF the device is I/O-only, OR memory-mapped at 0x3FF4–0x3FF5 if in a hybrid mode)
3. **Slot decoding** (FM-PAC is a cartridge in slot 1 or 2; slot 0-3 and sub-slot routing apply)
4. **Memory-mapper** (slot 3-0 RAM with #FC–#FF bank-select overlay; may collide in address space if cartridge slots and mapper both address the same page)
5. **Other devices in the same slot/page** (neighboring cartridges, if multi-cartridge is ever supported)

The SRAM **MUST** be decoded as part of the cartridge device's memory-access state machine, not bolted on. The unlock condition (0x4D @ 0x3FFE + 0x69 @ 0x3FFF) is a **memory-write side effect** that changes which physical store (ROM vs SRAM) is mapped into the CPU's address space at 0x4000–0x5FFF. This is a **slot-bus-level routing decision**, analogous to the memory-mapper's #FC–#FF bank-select logic or the VDP's VRAM port #98/#99 multiplexing.

**Hardware Specification:**
- **Reference:** `references/openmsx-21.0/src/sound/MSXFmPac.cc:78-144`, `MSXFmPac.hh:30-35`
- **Memory map (CPU-visible, slot page window 0x4000–0x5FFF):**

| Address Range | ROM Banks (default) | SRAM Window (when unlocked) |
|---|---|---|
| 0x4000–0x3FFD | 16 KB × bank 0–3 (selected by 0x3FF7 bits[1:0]) | 8 KB battery-backed store (when `sramEnabled=true`) |
| 0x3FFE | Register `r1ffe` (read returns latched value; write latches value & checks unlock) | Same address, SRAM location when unlocked |
| 0x3FFF | Register `r1fff` (read returns latched value; write latches value & checks unlock) | Same address, SRAM location when unlocked |
| 0x3FF4–0x3FF5 | YM2413 OPLL register overlay (optional, if device is not I/O-only) | (Usually no OPLL overlay in FM-PAC mode; check openMSX) |
| 0x3FF6 | Enable register (bit-0 I/O, bit-4 force-reset latches) | Enable register (bit-4 also resets latch state when written) |
| 0x3FF7 | Bank register (bits[1:0] select 0–3) | Bank register (same) |

**SRAM Unlock State Machine (`MSXFmPac.cc:137-144`):**
```
void checkSramEnable():
  if (r1ffe == 0x4D && r1fff == 0x69):
    sramEnabled = true
    invalidateDeviceRWCache()  // Memory-mapper-style: invalidate cached page state
  else:
    sramEnabled = false
    invalidateDeviceRWCache()

readMem(0x4000–0x5FFD):
  if sramEnabled:
    return sram[address]
  else:
    return rom[bank * 0x4000 + address]

writeMem(address, value):
  if address == 0x3FFE && !(enable & 0x10):  // bit-4 is "read-only" gate
    r1ffe = value
    checkSramEnable()
  elif address == 0x3FFF && !(enable & 0x10):
    r1fff = value
    checkSramEnable()
  elif sramEnabled && address < 0x1FFE:
    sram.write(address, value)
  else if !sramEnabled:
    // ROM is read-only; writes to ROM window are silent (or may route to OPLL)
```

**Cross-Subsystem Dependencies (MANDATORY REGRESSION TESTS):**

1. **FM-PAC ROM reads** (`readMem` when `sramEnabled=false`):
   - Existing cartridge ROM files (`.rom` images) load at 0x4000–0x5FFF correctly.
   - Bank register (0x3FF7) correctly switches between 4 banks (test: write 0–3 to 0x3FF7, read back, verify ROM content differs between banks).
   - **Neighbor test:** Konami/ASCII*/Generic* cartridges in the same slot still read correctly (no cross-device interference).

2. **OPLL register access** (I/O ports #7C/#7D and any memory-mapped overlay):
   - FM-PAC's YM2413 register writes at #7C/#7D still work when SRAM is locked (default).
   - FM-PAC's YM2413 register writes still work when SRAM is unlocked (should NOT be affected by sramEnabled state).
   - **Neighbor test:** Internal MSXMusic (slot 3-3, #7C/#7D I/O) is unaffected by slot 1/2 FM-PAC cartridge presence.

3. **Slot decoding** (slot 0–3, sub-slot 0–3, page 0–3):
   - FM-PAC cartridge in slot 1, page 2 (typical configuration) correctly appears at CPU addresses 0x4000–0x5FFF.
   - FM-PAC cartridge in slot 2, page 2 (alternate) still works.
   - Slot-expansion logic (slot 0 expanded, slot 3 expanded) does not clobber cartridge routing.
   - **Neighbor test:** Slot 3-0 (main RAM) page 2 still reads/writes via memory-mapper when a cartridge is loaded in slot 1.

4. **Memory-mapper coordination** (slot 3-0, bank register at #FC–#FF):
   - When a cartridge loads into slot 1/2, the memory-mapper (slot 3-0) bank selection (#FC–#FF) for OTHER pages does not interfere.
   - If page 2 is dual-routed (e.g., slot 1 cartridge AND slot 3 mapper both claim page 2), the cartridge takes priority (as per openMSX slot priority rules); verify no address-contention bugs.
   - **Neighbor test:** Switch RAM pages via #FC–#FF while a cartridge is loaded; verify RAM content in unaffected pages.

**Current Emulator Status:**
- Machine has `sram_` (8 KB) allocated but **not wired to any FM-PAC cartridge device** (M17 backlog B4 re-grounded to B6/Halnote).
- **No FM-PAC cartridge mapper type** exists in `CartridgeMapperType` enum (checked `src/devices/cartridge/cartridge_mapper_type.h` — only Mirrored, Generic8kB, Generic16kB, Ascii8kB, Ascii16kB, Konami, KonamiSCC are implemented).
- **Bug diagnosis:** YS II likely loads as an FM-PAC cartridge and probes for SRAM by writing 0x4D/0x69 to 0x3FFE/0x3FFF. Without a FM-PAC cartridge device implementing the unlock state machine, the probe fails and YS II displays "NO S-RAM AVAILABLE".

**Phase 2A Scope (§3 below):** Implement FM-PAC cartridge mapper + SRAM unlock state machine, wired as part of the cartridge device's memory-routing logic (NOT as a standalone SRAM store + separate unlock handler). All MANDATORY REGRESSION TESTS (above) must pass before Phase 2A closure.

### 2.5 Bug B Analysis: Black Screen as a System-Wide Disk-Loading Pipeline Problem

**SYSTEM ARCHITECTURE FRAMING:** Bug B is NOT a simple display state issue. The black screen on building entry is a **DATA-FLOW FAILURE** that must be traced through the ENTIRE PIPELINE from disk → CPU → VDP:

```
Disk Image (FDC sees it)
    ↓ [Sector read command issued]
WD2793 FDC State Machine
    ↓ [Reads bytes, manages DRQ/INTRQ]
Disk-Image Byte Stream → DMA or Programmed I/O
    ↓ [Data transfers to CPU address space or register]
CPU RAM (buffer holds read bytes)
    ↓ [Game code parses graphics data, writes to VDP]
VDP VRAM (pixels/palettes written)
    ↓ [VDP renders next frame]
Display Output
```

**BLACK SCREEN** can occur at ANY of these stages:
1. **Stage 1 (FDC):** Sector read command issued but never completes; INTRQ is never raised; CPU waits forever for INTRQ.
2. **Stage 2 (DMA/I/O):** FDC raises INTRQ but the data never transfers to CPU RAM (DMA bus stalled, I/O port read failing, or DRQ not asserted correctly).
3. **Stage 3 (CPU):** Data transfers OK but the game code hangs/crashes while parsing/buffering the data (unlikely, but possible if a CPU timing bug causes a crash).
4. **Stage 4 (VDP write):** Data arrives but the game's VDP write sequence is corrupted (register writes out of order, VRAM writes to wrong address, palette not updated).
5. **Stage 5 (VDP render):** VDP state is correct but rendering is blanked (display-enable register cleared, or a rendering-depth bug masks the graphics).

**Symptom Analysis:**
- **UI persists, active area black, game unresponsive:** suggests the game is hung (stuck in a CPU wait loop or infinite loop), NOT a VDP blank state. Candidate: stages 1–3 (data pipeline blocked).
- **If the UI is also black:** suggests VDP blanking (register 1 bit-6 = 0 or similar), or a VRAM corruption that affects both UI and playfield. Candidate: stage 4–5.

**Phase 2B Root-Cause Diagnostic Plan (MANDATORY FULL-PIPELINE TRACE):**

1. **Pre-entry baseline** (just before building entry, at "city map" screen):
   - Capture state dump: `debug/ys2_building_pre.txt` (`--dump-state`)
   - Capture frame: `debug/ys2_building_pre.png`
   - Log FDC state: register file, current state machine state, INTRQ status, DRQ status.
   - Log VDP state: register 1 (mode/enable bits), current VRAM read/write address, palette.
   - Log CPU state: PC, registers, halted flag.

2. **Entry transition** (press key to enter building):
   - Run emulator for 1–3 frames past the entry point.
   - Capture state dump and frame at EACH intermediate frame.
   - **Critical:** enable detailed FDC event logging (if available) to record every FDC state transition + INTRQ assertion/de-assertion + DRQ pulse.

3. **Black-screen frame** (when black screen appears):
   - Capture state dump: `debug/ys2_building_black.txt`
   - Capture frame: `debug/ys2_building_black.png`
   - Detailed diagnostics:
     - **FDC:** Is the FDC in an IDLE state or a READING state? Was INTRQ ever raised? Is DRQ asserted? What was the last command?
     - **CPU:** PC value; is CPU halted? If halted, what was the last instruction (was it a HALT opcode)?
     - **VDP:** Register 1; palette values; VRAM content (sample a few addresses where sprites/tiles should be).
     - **Memory:** Check DMA control registers (if any) for stuck/incomplete transfers.

4. **openMSX A/B comparison** (deterministic repro on openMSX 19.1):
   - Run the exact same playtest script on openMSX (same disk, same keystrokes).
   - At the black-screen moment (or the corresponding moment in openMSX):
     - Does openMSX also black out? If yes, this is game behavior, not emulator bug (unexpected, but acceptable).
     - If openMSX continues: capture openMSX state dump at the same frame and diff:
       - Which FDC/DMA state differs first?
       - Which VDP register differs?
       - Which CPU register (PC) differs?

5. **Localization verdict:**
   - If **FDC state differs first:** the bug is FDC-level (sector read not completing, INTRQ timing, DRQ assertion).
   - If **CPU PC differs first:** the bug is CPU-level (usually unlikely unless a core timing bug causes a crash).
   - If **VDP register 1 differs first:** the bug is VDP-level (display was blanked; either the game blanked it intentionally or a VDP bug caused it).
   - If **VRAM content differs:** the bug is VRAM write routing (game writes to wrong address, or VRAM is corrupted).

**MANDATORY EVIDENCE CAPTURE** (no cosmetic patches, no "just re-render"):
- All state dumps MUST include FDC register file + event history (if available).
- All frame captures MUST span from pre-entry to post-black-screen (5+ frames minimum).
- openMSX A/B comparison MUST be completed (if openMSX unavailable, explicitly document as blocker).
- Root-cause conclusion MUST identify the FIRST component in the pipeline that diverges (no hand-waving).

**No fix is in scope** until the pipeline stage is confirmed. A cosmetic VDP re-render or CPU reset would mask the real bug and allow it to surface in future games.

## 3. Slices, Dependencies & Acceptance Criteria

### Slice S1: Playtest Agent Definition & Command Design

**Objective:** Define the `msx-playtest` agent and `/msx-playtest` command in the orchestration layer. No code implementation yet.

**Deliverables:**
- `.claude/agents/msx-playtest.md` — agent specification (name, description, tools, model=opus, dependencies)
- `.claude/commands/msx-playtest.md` — command specification (syntax, examples, goal grammar, output format)
- Architecture design document inline within the command spec covering:
  - Observe-act loop protocol (§2.3 above)
  - Frame capture/conversion pipeline
  - Landmark detection heuristics (how agent recognizes "title screen" vs "save menu" vs "building")
  - Bounded iteration logic + fallback for human hint
  - Output artifacts (script, frames, report)

**Deterministic Test Obligations:** None (orchestration layer only; no executable code).

**Acceptance Criteria:**
1. Agent definition contains all mandatory fields (name, description, tools list, model=opus).
2. Command syntax is unambiguous and includes at least 3 example `goal` values.
3. Loop protocol is documented as pseudocode with T-state/determinism guarantees.
4. Landmark heuristics are described for YS II (title, save menu, building).
5. Human-hint fallback is specified with a timeout/iteration budget.
6. Output artifact paths and formats are fully defined.

**Evidence Gate:** Artifacts checked into `.claude/` pass linting (valid YAML/Markdown frontmatter).

### Slice S2: Playtest Wrapper Tool & Integration with Existing M27 Tooling

**Objective:** Implement the multi-frame capture loop (Option A: PowerShell wrapper) and verify the frame-to-PNG pipeline works end-to-end.

**Deliverables:**
- `tools/playtest-runner.ps1` — orchestrates N iterations of emulator invocation + frame capture + PNG conversion
  - Input: goal, max-iterations, input-script file (or "" for start from scratch)
  - Output: numbered frame PNGs in `debug/playtest_<goal>/`, accumulated input script
- Verify `tools/frame-to-png.py` accepts a HBF1XV-FRAME-DUMP v1 file and produces a valid PNG
- `tools/README.md` updated with playtest-runner usage

**No src/ edits required for this slice.**

**Deterministic Test Obligations:**
- Unit test: `tools/playtest-runner.ps1` with a minimal 2-frame capture on any existing debug script (e.g., `tools/aleste-play-evidence-input.script`) reproduces the same output twice.
- Integration test: frame dump → PNG conversion round-trips (parse → re-serialize → identical PNG).

**Acceptance Criteria:**
1. `playtest-runner.ps1` exists and is executable.
2. Can capture 3+ frames in sequence without hanging or producing invalid output.
3. Frame PNG files are valid (readable by `feh` or any PNG viewer on Windows/WSL).
4. Two runs with the same input produce byte-for-byte identical frame sequences.
5. Frame timestamps/T-states match between the emulator dump and the PNG filename/metadata.

**Evidence Gate:**
```powershell
# Manual smoke test
tools/playtest-runner.ps1 -Goal "aleste_demo" -MaxIterations 2 -InputScript "tools/aleste-play-evidence-input.script"
# Verify debug/playtest_aleste_demo/frame_*.png files are readable PNGs
file debug/playtest_aleste_demo/frame_*.png  # or `identify` on ImageMagick
```

### Slice S3: First Playtest Goal (YS II Title/Save Menu Landmark)

**Objective:** Verify the playtest agent + harness can autonomously reach a non-trivial, visually distinct landmark (YS II's title screen or save menu) and save the derived input script.

**Deliverables:**
- Manual playtest run by the planner or developer:
  - Start YS II from boot (load via `--disk` + auto-boot)
  - Manually capture a frame dump at the title screen / save menu
  - Manually create the baseline frame PNG for landmark recognition
- Placeholder agent role (developer will implement the actual vision-loop in Phase 1 implementation, after this plan is approved):
  - Pseudo-code for "title screen" vs "gameplay" vs "save menu" recognition (pixel area analysis, text detection, etc.)
  - Iteration budget: 50–100 steps (to reach a menu from boot, typically 10–30 seconds of emulated time)

**Deterministic Test Obligations:**
- Test `playtest_goal_ys2_title.script` saved by the harness can be re-played deterministically: two runs with the same script produce byte-for-byte identical frame sequence.

**Acceptance Criteria:**
1. A baseline frame dump for "YS II title screen" exists at `debug/playtest_baseline_ys2_title.txt` (manually captured).
2. The derived input script to reach the title screen (`debug/playtest_ys2_title_<timestamp>.script`) is valid HBF1XV-INPUT-SCRIPT format.
3. Re-playing the script reaches the same visual landmark (frame comparison against baseline, even if not pixel-perfect, should show the same "title" UI).
4. The landmark is reached within the iteration budget (≤50 iterations from cold boot).
5. A playtest report is generated: `debug/playtest_ys2_title_report.md` (summary of iterations, landmarks reached, any anomalies).

**Evidence Gate:**
```powershell
# Verify the script is valid and re-playable
sony_msx_headless.exe --disk disks/games/ys2_disk1.dsk --input-script debug/playtest_ys2_title_*.script --dump-frame 600000 > debug/ys2_title_replay_frame.txt
python tools/frame-to-png.py debug/ys2_title_replay_frame.txt debug/ys2_title_replay.png
# Visually compare against the baseline
```

### Slice S4: Bug A Investigation — FM-PAC SRAM Unlock Protocol Verification

**Objective:** Confirm that YS II's "NO S-RAM AVAILABLE" message is triggered by a failed SRAM unlock attempt, and identify the exact sequence of writes to 0x5FFE/0x5FFF.

**Deliverables:**
- Trace log of I/O and memory-mapped writes during YS II boot, capturing all accesses to FM-PAC-relevant addresses (0x3FFE–0x3FFF / 0x5FFE–0x5FFF, FM-PAC port #7C/#7D if applicable).
  - Use `--dump-state` and optional `--event-log` output (if event logging is available) to capture the sequence.
- Analysis document: `docs/m36-bug-a-investigation.md`, containing:
  - Hypothesis confirmation: YS II does write the magic bytes 0x4D/0x69 to 0x5FFE/0x5FFF (or their slot-relative equivalents).
  - Screenshot/frame capture of the "NO S-RAM AVAILABLE" message.
  - openMSX A/B comparison: identical boot sequence on openMSX 19.1 (WSL), verifying YS II detects SRAM there.
  - Root-cause statement: confirm it's a missing FM-PAC cartridge mapper implementation.

**Deterministic Test Obligations:**
- Trace the YS II boot sequence deterministically (use the playtest script from S3).
- Verify that the trace is reproducible (two runs with the same script produce the same write sequence to 0x5FFE/0x5FFF).

**Acceptance Criteria:**
1. YS II's SRAM-probe sequence is documented (exact addresses, values written, timing in T-states).
2. The sequence matches the FM-PAC unlock protocol: attempts to write 0x4D/0x69 to 0x5FFE/0x5FFF.
3. openMSX A/B evidence confirms YS II successfully detects SRAM on real openMSX (same boot sequence, different outcome).
4. Conclusion: the emulator is missing an FM-PAC cartridge mapper type + device implementation.
5. Investigation report filed at `docs/m36-bug-a-investigation.md`.

**Evidence Gate:**
```powershell
# Capture boot trace with YS II
sony_msx_headless.exe --disk disks/games/ys2_disk1.dsk --dump-state debug/ys2_boot_state.txt > /dev/null
# Check for 0x5FFE/0x5FFF writes in debug logs (manual inspection or grep)
grep -i "5ffe\|5fff" debug/ys2_boot_state.txt

# openMSX A/B comparison (on WSL)
tools/openmsx-ab-smoke.ps1 -Subject "YS2_SRAM_probe"
```

### Slice S5: Bug A Implementation — FM-PAC Cartridge Mapper + SRAM Wiring

**Objective:** Implement the FM-PAC cartridge mapper type and wire the 8 KB SRAM with the unlock protocol into slots 1 & 2, with FULL SYSTEM-LEVEL INTEGRATION and CROSS-SUBSYSTEM REGRESSION TESTING.

**Deliverables:**
- New cartridge mapper type: `devices::cartridge::CartridgeMapperType::FmPac` (add to enum in `src/devices/cartridge/cartridge_mapper_type.h`)
- New cartridge device: `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}`
  - Implements the 4-bank ROM window + 8 KB SRAM + register overlay protocol
  - SRAM unlock state machine: `checkSramEnable()` logic unlocks SRAM when `(r1ffe == 0x4D) && (r1fff == 0x69)`
  - Memory-routing decision: `readMem()`/`writeMem()` ROUTES between ROM and SRAM based on `sramEnabled` state
  - All FM-PAC registers at 0x3FF4–0x3FF7 (enable, bank, r1ffe, r1fff) wired correctly
  - Grounded exactly in `references/openmsx-21.0/src/sound/MSXFmPac.cc:78-144`
- Wiring in `src/machine/hbf1xv_machine.cpp`:
  - When a cartridge of type `FmPac` is loaded into slot 1 or 2, attach the FM-PAC device to the slot bus
  - No changes to the internal MSXMusic device at slot 3-3 (which remains unchanged)
  - Verify slot-priority logic: FM-PAC cartridge at slot 1/2, page 2 takes priority over any other device claiming page 2 in a lower-priority slot
- Unit test: `tests/unit/devices/cartridge/cartridge_fmpac_rom_unit_test.cpp`
  - **Core protocol:** Verify SRAM unlock: write 0x4D to 0x3FFE, 0x69 to 0x3FFF, then SRAM window is readable/writable; relocking (different values) blocks SRAM access
  - **SRAM persistence:** save/load round-trip with `BatteryBackedSram` primitive; verify content survives
  - **Memory routing:** reads from 0x4000–0x5FFF route to ROM when `sramEnabled=false`, to SRAM when `sramEnabled=true`
  - **Register access isolation:** writes to 0x3FF6 (enable), 0x3FF7 (bank) don't clobber SRAM content; bank register changes ROM bank for ROM-read window
  - **Bit-4 force-reset:** writing bit-4 of enable register forcibly resets latches and re-checks unlock condition
  - **100% path coverage:** all lock/unlock transitions, all register access patterns
- **CROSS-SUBSYSTEM REGRESSION TESTS** (MANDATORY per coordinator):
  - `test_fmpac_slot_priority_vs_mapper_ram.cpp`: Load FM-PAC in slot 1, page 2; verify page 2 reads from FM-PAC, not slot 3-0 mapper, even with mapper bank select active
  - `test_fmpac_rom_bank_switching_vs_sram_unlock.cpp`: Bank-switch ROM (0x3FF7) while SRAM unlocked; verify SRAM still accessible (bank register does NOT affect SRAM window)
  - `test_fmpac_io_ports_coexist_with_sram.cpp`: Verify FM-PAC OPLL I/O access (#7C/#7D) works correctly while SRAM is unlocked/locked (should be independent of sramEnabled state)
  - `test_konami_cartridge_still_loads_with_fmpac_present.cpp`: Load Konami cartridge into slot 2 in presence of FM-PAC in slot 1; verify both coexist without interference (each in its own page/sub-slot or different slot)
  - `test_fmpac_vs_msxmusic_io_isolation.cpp`: Verify internal MSXMusic device (slot 3-3, #7C/#7D) is unaffected by FM-PAC cartridge presence (no I/O port collision)
- Integration test: `tests/integration/machine/hbf1xv_m36_fmpac_cartridge_integration_test.cpp`
  - Load YS II as a cartridge with FM-PAC type
  - Boot to the SRAM-probe sequence (first 1000 T-states)
  - Verify that writes to 0x3FFE/0x3FFF land correctly in the FM-PAC device (use `debug_io_write()` or a state dump to confirm)
  - Verify SRAM-detect succeeds: YS II no longer displays "NO S-RAM AVAILABLE"
  - Verify game proceeds to title screen / main menu

**No CPU/core edits required** (device-level only, per DEC-0049).

**Deterministic Test Obligations:**
- Unit: 100% path coverage of SRAM unlock/lock + register access + bank switching + slot priority.
- Cross-subsystem: All 5 neighbor regression tests pass (slot priority, bank switching, OPLL I/O, other cartridges, MSXMusic isolation).
- Integration: YS II boot sequence traces identical write behavior; SRAM-detect succeeds; no player-visible "NO S-RAM" message.
- A/B parity: openMSX A/B harness confirms empty diff on the SRAM-probe sequence (same writes land, same SRAM content after unlock).

**Acceptance Criteria:**
1. `CartridgeMapperType::FmPac` enum value exists and is recognized by cartridge loader.
2. `cartridge_fmpac_rom.{h,cpp}` implements the protocol exactly per openMSX reference (`MSXFmPac.cc:78-144`).
3. SRAM unlock state machine is implemented as part of `readMem()`/`writeMem()` routing (not bolted-on externally).
4. Unit tests: ALL 5 core tests pass; ALL 5 cross-subsystem regression tests pass; 100% path coverage.
5. Integration test passes: YS II successfully detects and uses SRAM; no "NO S-RAM" error.
6. **CROSS-SUBSYSTEM REGRESSION:** Konami, KonamiSCC, Ascii8kB, Ascii16kB, Generic8kB, Generic16kB cartridge tests pass unchanged; slot 3-0 mapper tests pass; MSXMusic (#7C/#7D) tests pass.
7. openMSX A/B parity: empty diff on the SRAM-probe sequence (exact same address/value writes, exact same SRAM unlock condition).
8. **System-wide**: Slot-bus routing integrity verified; no address-space collisions; priority rules respected.

**Evidence Gate:**
```powershell
cmake --build build --config Debug
# Core FM-PAC tests
ctest --test-dir build -C Debug -k "fmpac_rom_unit" --output-on-failure
# Cross-subsystem regression tests
ctest --test-dir build -C Debug -k "fmpac_slot_priority|fmpac_bank_switch|fmpac_io_coexist|konami_with_fmpac|fmpac_vs_msxmusic" --output-on-failure
# Existing cartridge regression (Konami, ASCII*, Generic*)
ctest --test-dir build -C Debug -k "cartridge" --output-on-failure
# Mapper regression
ctest --test-dir build -C Debug -k "mapper" --output-on-failure
# Full suite
ctest --test-dir build -C Debug --output-on-failure
tools/openmsx-ab-smoke.ps1 -Subject "M36_FmPac_probe"
```

### Slice S6: Bug B Investigation — Full-Pipeline Black Screen Root-Cause Analysis

**Objective:** Deterministically reach the building entry state using the playtest harness, capture COMPLETE machine state across the ENTIRE DATA-FLOW PIPELINE (disk → FDC → CPU → VDP → display), and compare against openMSX to localize where the pipeline breaks.

**Deliverables:**

**6A — Playtest Script (Deterministic Landmark):**
- Playtest script to reach "just before building entry": `debug/playtest_ys2_building_entry_<timestamp>.script`
- Frame sequence at transition: `debug/playtest_ys2_building/*.png` (5+ frames: pre-entry, entering, during black-out, post-black-out)
- Verification: re-play the script twice; all frames byte-for-byte identical.

**6B — PIPELINE STAGE 1 (Disk → FDC):**
- State dump: `debug/ys2_building_fdc_stage1.txt` (full FDC register file: status, command, track, sector, data registers)
- FDC diagnostics:
  - Current FDC state machine state (e.g., IDLE, READING, WRITING, SPINUP)
  - Last command issued (e.g., READ SECTOR, READ TRACK, WRITE)
  - INTRQ status (asserted? de-asserted? when?)
  - DRQ status (asserted? pulsing? how many bytes transferred?)
  - Expected: if FDC is working correctly, a READ command should complete with INTRQ asserted after 100–1000 cycles (disk rotation + sector seek time)

**6C — PIPELINE STAGE 2 (FDC → CPU/DMA):**
- State dump: `debug/ys2_building_dma_stage2.txt` (if DMA exists; check S1985 spec for DMA involvement)
- CPU register state: PC, registers A–L, SP, IX, IY, halted flag
- Expected: CPU should NOT be halted; PC should be advancing (in a loop waiting for INTRQ or processing data)
- Evidence: if CPU is halted or PC frozen, FDC data pipeline is stalled (INTRQ never raised)

**6D — PIPELINE STAGE 3 (CPU → VDP):**
- State dump: `debug/ys2_building_vdp_stage3.txt` (VDP register file, read/write pointer state, mode)
- VDP register 1 (display mode / enable bits): check if display is enabled (bit 6 should be 1 for display on)
- VDP VRAM sample: capture a region where sprite/tile graphics should appear (e.g., first 128 bytes of VRAM at 0x0000)
- Expected: if game successfully wrote graphics to VRAM, first 128 bytes should NOT be 0x00 or 0xFF (garbage pattern indicates no write)

**6E — PIPELINE STAGE 4 (VDP Render → Display):**
- Frame PNG at black-screen moment: `debug/ys2_building_black_screen.png`
- Frame PNG at pre-entry (baseline): `debug/ys2_building_pre_entry.png`
- Visual inspection: is the black area a VDP blank (0x0000 black color) or a graphics area that was never written (leaving VRAM uninitialized = 0xFF)?

**6F — openMSX A/B Comparison (MANDATORY — Full Pipeline Trace):**
- Run the exact same playtest script on openMSX 19.1 (WSL)
- At the black-screen frame, capture openMSX state dump with same diagnostics (FDC, CPU, VDP)
- **Diff analysis:**
  - Which component's state FIRST differs from ours?
  - **If FDC differs first:** our FDC is not completing reads (INTRQ not raised, or DRQ stalled).
  - **If CPU differs first:** our CPU diverged during the entry sequence (halted unexpectedly, or PC took a different branch).
  - **If VDP differs first:** our VDP register writes or VRAM writes diverged.
  - **If display output differs:** VDP state is identical but rendering is different (sprite/tile fetch or composition bug in rendering depth).

**6G — Root-Cause Report:**
- File: `docs/m36-bug-b-root-cause.md`
- Content:
  - Pipeline stage-by-stage diagnostics (from 6A–6F above)
  - **Root-cause verdict:** which component first diverges, and the exact symptom (e.g., "FDC INTRQ not raised after 2000 cycles" or "VDP register 1 bit-6 cleared unexpectedly")
  - Evidence: cite the state dump lines that prove the diagnosis
  - **Scope for S7:** describe the minimal fix needed (e.g., "FDC WD2793 INTRQ timing bug in sector-read end-of-file" or "VDP register write in wrong address bus byte")
  - **Honest caveat:** if analysis is inconclusive, document as such; do NOT speculate

**Deterministic Test Obligations:**
- The derived script to reach "just before building entry" must be reproducible: two runs = byte-for-byte identical frames.
- All state dumps must be deterministic (no wall-clock, no RNG); re-run the same script = identical state dumps.
- openMSX A/B comparison must complete successfully (or explicitly note if openMSX unavailable as blocker).

**Acceptance Criteria:**
1. **Playtest landmark reached:** visually confirmed via frame PNG showing building entrance.
2. **Frame sequence complete:** 5+ frames spanning pre-entry → entry → black screen; all identical on re-run of same script.
3. **Full-pipeline diagnostics captured:**
   - FDC register file + state machine state + INTRQ/DRQ timeline (6B)
   - CPU PC + halted status (6C)
   - VDP register 1 + VRAM sample (6D–6E)
   - Frame PNGs for visual confirmation (6E–6F)
4. **openMSX A/B comparison completed:**
   - State dumps at equivalent frames on both emulators captured
   - Diff identifies which component (FDC/CPU/VDP) first diverges
   - If openMSX unavailable, explicitly documented as blocker (still proceed with local analysis)
5. **Root-cause identified:** report in `docs/m36-bug-b-root-cause.md` pinpoints which STAGE OF THE PIPELINE (1–5 from §2.5) is broken and WHY.
6. **No cosmetic patches:** the root-cause analysis is HONEST (no hand-waving); a real FDC/CPU/VDP bug is identified (or inconclusive findings honestly documented).

**Evidence Gate:**
```powershell
# Reach building via playtest (S3 harness + manual playtest agent input)
# Capture state dumps at each frame during entry transition
sony_msx_headless.exe --disk disks/games/ys2_disk1.dsk --input-script debug/playtest_ys2_building_entry_*.script --dump-state debug/ys2_building_stage1.txt
sony_msx_headless.exe --disk disks/games/ys2_disk1.dsk --input-script debug/playtest_ys2_building_entry_*.script --dump-state debug/ys2_building_stage2.txt
# (repeat with different --dump-frame timing to capture intermediate stages)

# openMSX comparison (if available on WSL)
# Run the same script on openMSX, capture frame/state at black-screen moment
# Compare state dumps: diff debug/ys2_building_stage1.txt <(openMSX state dump at same moment)

# Visual frame comparison (developer manual inspection)
# debug/playtest_ys2_building/frame_*.png vs openMSX frame capture at same moment
```

### Slice S7: Bug B Implementation (Conditional on Root-Cause)

**Objective:** Implement the fix for Bug B based on the root-cause analysis from S6.

**Scope:** Conditional on S6 findings. This slice is a **placeholder** in the M36 plan; the exact work will be scoped after S6 completes. Possible targets:
- **If FDC:** Fix a disk-read or INTRQ timing bug; add/update integration test to exercise the building-load sequence.
- **If VDP:** Fix a register write, mode-switch, or VRAM-page bug; add unit test for the specific mode transition.
- **If CPU:** Debug hanging loop or missing interrupt; trace and document the CPU-VDP handshake.

**Deliverables:** Conditional. Developer will scope this after S6 analysis.

**Deterministic Test Obligations:** Conditional.

**Acceptance Criteria:**
- The black screen no longer occurs; the game proceeds past building entry.
- openMSX A/B parity: the repro script produces the same visual outcome (game continues into building).
- Regression: all M1–M35 tests pass unchanged.

**Evidence Gate:** Conditional (depends on fix type).

### Slice S8 (Ride-Along): Strengthen R-M35-1 Multi-Disk Test

**Objective:** Once the playtest harness can drive F11 (disk-change key), enhance the `multi_disk` integration test to verify the disk-index advances after a swap.

**Deliverables:**
- Update `tests/integration/machine/hbf1xv_m35_multi_disk_integration_test.cpp`:
  - Add a new test case that loads a multi-disk game (e.g., YS II disk 1+2) and verifies `disk_manager_.current_disk_index()` advances from 0→1 after an F11 press + disk-swap.
  - Alternatively, if F11 can be encoded in an input script (already proven in M27), use the playtest harness to generate a script that presses F11 and capture the disk-index change.
- Verify the test fails without playtest-ability (adversarial mutation: hard-code `disk_index=0` in the disk manager, verify test fails).

**Deterministic Test Obligations:**
- Test passes with playtest harness input (F11 press in script).
- Test fails if disk swap logic is disabled.

**Acceptance Criteria:**
1. New test case asserts `disk_index` advances after F11.
2. Adversarial mutation kills the test (disk-index hard-coded to 0).
3. Test passes with real playtest harness script (if available by end of Phase 1).
4. Zero regression: existing multi_disk tests pass.

**Evidence Gate:**
```powershell
ctest --test-dir build -C Debug -k "multi_disk" --output-on-failure
```

## 4. Dependency Graph & Sequencing

```
┌─────────────────────────────────────────┐
│  S1: Agent/Command Definition (Planning)│
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  S2: Playtest Wrapper Tool Integration  │
│      (no src/ edits, pure tools)        │
└────────────┬────────────────────────────┘
             │
             ▼
┌─────────────────────────────────────────┐
│  S3: First Playtest Goal (YS II Title)  │
│      (proves observe-act loop)          │
└────────────┬────────────────────────────┘
             │
     ┌───────┴────────┐
     │                │
     ▼                ▼
┌──────────┐    ┌──────────────┐
│ S4: Bug A│    │ S6: Bug B    │
│ Investi- │    │ Investi-     │
│ gation   │    │ gation       │
└────┬─────┘    └──────┬───────┘
     │                 │
     ▼                 ▼
┌──────────┐    ┌──────────────┐
│ S5: Bug A│    │ S7: Bug B    │
│ Fix      │    │ Fix          │
│ (FM-PAC) │    │ (conditional)│
└────┬─────┘    └──────┬───────┘
     │                 │
     └────────┬────────┘
              │
              ▼
    ┌──────────────────────┐
    │  S8: R-M35-1 Test    │
    │  Ride-Along (optional│
    │  if S3/S1 ready)     │
    └──────────────────────┘
```

**Key Dependencies:**
- S1 is a prerequisite for S2–S3 (agent/command must be defined before implementation).
- S2 must complete before S3 (no playtest loop without the runner tool).
- S3 must complete before S4/S6 (prove the harness works before investigating bugs).
- S4 is independent of S5 (investigation informs fix scope; both use the harness from S3).
- S6 is independent of S5 (can be parallelized; S6 findings determine S7 scope).
- S8 depends on S3 (playtest harness must prove F11 capability).

## 5. Test Plan (System-Wide Architecture Focus)

### Unit Tests (Core Behavior)

| Test | Scope | Determinism | Cross-System Impact | Notes |
|---|---|---|---|---|
| `playtest_runner_frame_capture.cpp` (S2) | `--dump-frame` produces valid HBF1XV-FRAME-DUMP v1 | Byte-for-byte replay | None (tooling only) | Two runs, identical script → identical frame dumps |
| `frame_to_png_conversion_unit_test.cpp` (S2) | Frame dump → PNG → frame dump round-trip | Lossless (RGB555 pixels) | None (tooling only) | Parse/serialize contract; pixel accuracy |
| `cartridge_fmpac_rom_unit_test.cpp` (S5) | **SRAM unlock state machine**: lock/unlock transitions, register access, memory routing | Deterministic state machine | **AFFECTS:** slot bus, slot priority, address decoding | 100% path coverage: ROM-read → SRAM-unlock → SRAM-read/write → SRAM-lock → ROM-read |
| `test_fmpac_slot_priority_vs_mapper_ram.cpp` (S5, cross-subsystem) | **SLOT PRIORITY:** FM-PAC page 2 vs mapper page 2 | Deterministic routing | **AFFECTS:** slot 3-0 mapper RAM access, page routing | Verify FM-PAC address takes priority (openMSX priority rules) |
| `test_fmpac_rom_bank_switching_vs_sram_unlock.cpp` (S5, cross-subsystem) | **BANK REGISTER:** 0x3FF7 bank-select does NOT affect SRAM window | Deterministic register writes | **AFFECTS:** ROM/SRAM boundary, address interpretation | Bank-switch while SRAM unlocked; verify SRAM still accessible at same address |
| `test_fmpac_io_ports_coexist_with_sram.cpp` (S5, cross-subsystem) | **I/O INDEPENDENCE:** FM-PAC OPLL #7C/#7D works regardless of SRAM unlock state | Deterministic I/O + SRAM state | **AFFECTS:** OPLL register access, device independence | OPLL register writes land correctly; no I/O interference from SRAM unlock |
| `test_konami_cartridge_still_loads_with_fmpac_present.cpp` (S5, cross-subsystem) | **MULTI-CARTRIDGE:** Konami in slot 2 + FM-PAC in slot 1 coexist | Deterministic slot routing | **AFFECTS:** other cartridge devices, slot decoding | Each cartridge isolated to its own address space; no crosstalk |
| `test_fmpac_vs_msxmusic_io_isolation.cpp` (S5, cross-subsystem) | **BUILT-IN DEVICE ISOLATION:** internal MSXMusic (slot 3-3) unaffected by FM-PAC cartridge | Deterministic I/O | **AFFECTS:** slot 3-3 MSXMusic functionality, I/O bus | MSXMusic #7C/#7D still functional; no port collision with FM-PAC |
| `multi_disk_r_m35_1_unit_test.cpp` (S8) | Disk-index advancement on swap | Input-script driven | None (M35 residual only) | Adversarial mutation: disk_index hard-coded → test fails |

### Integration Tests (System-Level)

| Test | Scope | Determinism | Full-Pipeline Validation | Notes |
|---|---|---|---|---|
| `playtest_goal_ys2_title_integration_test.cpp` (S3) | Boot YS II, reach title screen | Input script deterministic | FDC boot → CPU execute → VDP render | Prove playtest harness works; landmark recognition |
| `hbf1xv_m36_fmpac_cartridge_integration_test.cpp` (S5) | **FM-PAC UNLOCK WORKS END-TO-END:** Load YS II as FM-PAC; SRAM-probe succeeds | Trace YS II boot sequence | **Full path:** CPU writes to 0x5FFE/0x5FFF → FM-PAC device routes writes to r1ffe/r1fff → SRAM unlock fires → SRAM window readable → YS II no "NO S-RAM" error | Verify unlock protocol lands correctly; "NO S-RAM" message gone |
| `hbf1xv_m36_fdc_boot_regression_with_fmpac_present.cpp` (S5, cross-subsystem) | **FDC BOOT STILL WORKS:** BIOS disk-boot path unaffected by FM-PAC slot presence | BIOS boot sector read → disk-ROM INT jump | **Full path:** FDC INTRQ → CPU INT ACK → disk-ROM read from correct slot (3-1) → boot continues | Verify BIOS boot path does not collide with FM-PAC cartridge slot routing |
| `hbf1xv_m36_memory_mapper_coordination_with_fmpac.cpp` (S5, cross-subsystem) | **MAPPER STILL WORKS:** page 3 (or other non-FM-PAC page) RAM bank-switch via #FC–#FF | CPU #FC/#FD/#FE/#FF bank-select writes + RAM read | **Full path:** mapper bank-select → slot-bus routes page to correct RAM bank → CPU reads correct RAM content | Verify memory-mapper's other pages unaffected by FM-PAC presence |
| `hbf1xv_m36_bug_b_building_entry_diagnostics.cpp` (S6) | **FULL-PIPELINE TRACE:** Reach building, capture state at each stage | Playtest script deterministic | **Full pipeline:** disk-load → FDC read → DMA/I/O → CPU RAM buffer → VDP write → VRAM → render → display; identify first divergence from openMSX | 5+ frame sequence; FDC/CPU/VDP diagnostics at each; openMSX A/B comparison |
| `hbf1xv_m35_multi_disk_disk_index_integration_test.cpp` (S8) | F11 press → disk-index advance | Playtest script with F11 | Disk-manager slot-swap logic works; index reflects in next disk-read | Verify R-M35-1: disk swap index actually advances |

### System/A/B Parity Tests (Behavior Reference)

| Test | Scope | Reference | Full-Pipeline Validation | Notes |
|---|---|---|---|---|
| `openmsx-ab-smoke.ps1 -Subject M36_FmPac_probe` (S4/S5) | YS II SRAM unlock probe sequence | openMSX 19.1 | **CPU writes to 0x5FFE/0x5FFF → FM-PAC device routes correctly → SRAM unlocks → game detects SRAM** (empty diff = identical outcome) | No skip; full A/B trace required before S5 closure |
| `openmsx-ab-smoke.ps1 -Subject M36_FmPac_rom_bank` (S5, cross-subsystem) | FM-PAC ROM bank-switching | openMSX 19.1 | Bank-switch writes + ROM read operations | Verify bank routing is identical to openMSX |
| `openmsx-bug-b-building-entry-pipeline.ps1` (S6) | Full building-entry disk-load sequence | openMSX 19.1 | **Trace all 5 pipeline stages:** FDC read command → INTRQ → DMA/I/O → CPU buffer → VDP write → render; identify stage where OUR emulator first diverges | FDC event log, CPU trace, VDP register/VRAM diff; openMSX comparison mandatory |

## 6. Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| **R1: Playtest Agent Vision Stalling** | Medium | Phase 1 unblocked; harness unusable | Bounded iteration (50 max); agent requests human hint if stuck at landmark; planner provides hint via manual screenshot |
| **R2: FM-PAC Implementation Scope Creep** | Low | S5 overruns (bank-register, port mirrors) | Scope strictly to unlock protocol + SRAM window; deferred: FM-PAC audio/synthesis (backlog E1) |
| **R3: Bug B Root-Cause Ambiguity** | Medium | S6 analysis inconclusive | Capture detailed state dumps; compare multiple frames before/during/after; escalate to coordinator if still unclear |
| **R4: YS II Disk Format Incompatibility** | Low | Playtest harness cannot load YS II | Verify disk image format (1.44 MB, standard 3.5" floppy); if wrong, planner provides corrected image or notes as out-of-scope |
| **R5: Frame PNG Conversion Losses** | Low | Vision recognition fails on corrupted frames | Round-trip test frame dump → PNG → frame dump; verify pixel-perfect lossless (RGB555) |
| **R6: ZEXALL Temptation** | Medium | CPU core edits creep in (violates DEC-0049) | Bug A is FM-PAC cartridge (device-level only); ZEXALL withheld unless core defect found (unlikely) |
| **R7: Multi-Disk Test Over-Specification** | Low | S8 adds unexpected complexity | R-M35-1 is a "ride-along"; only implement if time permits; planner can defer to M37 if needed |

## 7. Assumptions & Verification Actions

| Assumption | Verification Action |
|---|---|
| **A1:** YS II loads as an FM-PAC cartridge (or contains FM-PAC-compatible SRAM probe) | S4 investigation: trace the SRAM-probe sequence and compare against openMSX reference |
| **A2:** The 8 KB FM-PAC SRAM unlock magic bytes are 0x4D/0x69 exactly (per openMSX) | Cross-check against `references/fmsx-60/` FM-PAC implementation if available; if divergence found, document and prefer openMSX |
| **A3:** The playtest agent can recognize "title screen" / "save menu" / "building" landmarks visually | S3 delivers baseline frames + heuristic rules; developer tests vision loop on those frames before full integration |
| **A4:** The emulator's frame buffer + PNG conversion preserve pixel colors accurately (no gamma/encoding issues) | S2 unit test: frame dump → PNG → frame dump round-trip is bit-for-byte identical |
| **A5:** YS II multi-disk image is present at `disks/games/ys2_disk1.dsk` and `ys2_disk2.dsk` | Verify file existence; if missing, planner notes as blocker or requests files from human |
| **A6:** openMSX 19.1 on WSL is available for A/B comparison | Fallback: skip A/B parity if openMSX unavailable (downgrade evidence to "developer verified, A/B pending") |

## 8. Acceptance Criteria Summary

### Phase 1 (S1–S3): Autonomous Playtest Harness

| Criterion | Evidence |
|---|---|
| **C1.1** | `.claude/agents/msx-playtest.md` and `.claude/commands/msx-playtest.md` exist with all required fields |
| **C1.2** | `tools/playtest-runner.ps1` executes without error on a sample 2–3 frame capture |
| **C1.3** | Frame PNGs are valid (recognized by image viewers; file command shows `image/png`) |
| **C1.4** | Two runs with identical script produce byte-for-byte identical frame sequence |
| **C1.5** | YS II boot reaches a visually distinct "title screen" or "save menu" via playtest harness (derived script + captured frames) |
| **C1.6** | Playtest report generated: `debug/playtest_ys2_title_report.md` with iteration count + landmarks reached |
| **C1.7** | Landmark reached within budget (≤50 iterations from cold boot) |

### Phase 2A (S4–S5): Bug A (FM-PAC SRAM)

| Criterion | Evidence |
|---|---|
| **C2A.1** | Investigation doc: `docs/m36-bug-a-investigation.md` confirms YS II writes 0x4D/0x69 to 0x5FFE/0x5FFF |
| **C2A.2** | `CartridgeMapperType::FmPac` enum value added |
| **C2A.3** | `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}` implements unlock protocol per openMSX reference |
| **C2A.4** | Unit test `cartridge_fmpac_rom_unit_test.cpp` passes (SRAM unlock/lock, register access, 100% coverage) |
| **C2A.5** | Integration test `hbf1xv_m36_fmpac_cartridge_integration_test.cpp` passes (YS II SRAM-probe succeeds; "NO S-RAM" message gone) |
| **C2A.6** | openMSX A/B parity: smoke test confirms empty diff on SRAM-probe sequence |
| **C2A.7** | Zero regression: all M1–M35 tests pass; existing cartridge tests (Konami, ASCII*, etc.) unchanged |

### Phase 2B (S6–S7): Bug B (Black Screen)

| Criterion | Evidence |
|---|---|
| **C2B.1** | Investigation doc: `docs/m36-bug-b-root-cause.md` identifies root-cause (FDC/VDP/CPU) with diagnostic evidence |
| **C2B.2** | Playtest script to reach building-entry state: `debug/playtest_ys2_building_entry_*.script` |
| **C2B.3** | Frame sequence captures black-screen transition: `debug/playtest_ys2_building/frame_*.png` (visual confirmation) |
| **C2B.4** | State dumps (FDC, VDP, CPU registers) at key moments: `debug/ys2_building_*.txt` |
| **C2B.5** | openMSX A/B comparison on same script completed (shows different outcome or same black-screen if game is behaving) |
| **C2B.6** | Root-cause report is honest (no fabrication; if analysis is inconclusive, documented as such) |
| **C2B.7** | **If root-cause identified:** fix implemented + integration test added; game proceeds past building-entry (C2B.8 below) |
| **C2B.8** | **Post-fix:** black screen no longer occurs; building interior graphics render correctly; zero regression |

### Ride-Along (S8): R-M35-1 Multi-Disk Test

| Criterion | Evidence |
|---|---|
| **C3.1** | New test case in `hbf1xv_m35_multi_disk_integration_test.cpp` asserts disk-index advancement after F11 |
| **C3.2** | Test passes with playtest harness script (F11 press) if harness available; otherwise test implementation is valid and queued for S3 completion |
| **C3.3** | Adversarial mutation (hard-code `disk_index=0`) kills the test (confirms coverage) |
| **C3.4** | Zero regression: existing multi-disk tests pass |

## 9. Proposed Tag Target

**Recommendation: Split into two tags**

- **v1.0.37**: M36 Phase 1 (playtest harness + Bug A fix)
  - Includes: msx-playtest agent, `/msx-playtest` command, `tools/playtest-runner.ps1`, FM-PAC cartridge mapper + SRAM device (S1–S5)
  - Verification: Phase 1 integration test passes; Bug A integration test passes; A/B parity confirmed
  - Rationale: playtest harness is immediately valuable for future milestones (M37+); FM-PAC SRAM is a self-contained device feature

- **v1.0.38**: M36 Phase 2B (Bug B root-cause + fix)
  - Includes: Bug B fix (conditional on S6 root-cause)
  - Rationale: scope is uncertain until S6 analysis; no blocker on v1.0.37 release; M36 can straddle two tags

**Alternative:** Single tag **v1.0.37** after M36 complete (if Bug B is identified and fixed quickly; planner to assess during development).

## 10. References & Grounding

### Hardware Behavior References

- **FM-PAC SRAM Unlock Protocol:** `references/openmsx-21.0/src/sound/MSXFmPac.cc:78-144` (§2.4 above, MSXFmPac::checkSramEnable() + address mapping logic)
- **FM-PAC Memory Map:** `references/openmsx-21.0/src/sound/MSXFmPac.cc:34-56` (readMem/writeMem address ranges and register locations)
- **YM2413 / MSXMusic Device:** `references/openmsx-21.0/src/sound/MSXMusic.hh/.cc` (confirms built-in slot 3-3 device identity)
- **fMSX Cross-Reference (optional):** `references/fmsx-60/source/` (FM-PAC and slot logic; compare for disagreements if discovered)

### Existing Codebase

- **Input Script Mechanism:** `src/machine/input_script.h/.cpp` (M27, §2.1)
- **Frame Dump Format:** `src/machine/frame_dump.h/.cpp` (M26, §2.1)
- **Frame PNG Converter:** `tools/frame-to-png.py` (M26, §2.1)
- **Battery-Backed SRAM Primitive:** `src/devices/memory/battery_backed_sram.h/.cpp` (M17, reusable for FM-PAC)
- **Cartridge Mapper Types:** `src/devices/cartridge/cartridge_mapper_type.h` (MVP types + KonamiSCC; FM-PAC to be added §3.5)
- **Existing Cartridge Devices:** `src/devices/cartridge/cartridge_konami_rom.{h,cpp}` (architectural precedent for new FM-PAC device)

### Decision Records

- **DEC-0049:** M36 Approval & Spec (Bug A, Bug B, playtest agent, phase decomposition)
- **DEC-0012:** FM-PAC vs MSXMusic device grounding (confirms HB-F1XV has MSXMusic, not external FM-PAC)
- **DEC-0010/DEC-0006:** M15/M16 closure + boot checkpoint advancement

## 11. Deliverables Checklist

### Planning Artifacts

- [x] This planner package: `docs/m36-planner-package.md`
- [x] Phase/slice decomposition with acceptance criteria (above, §3)
- [x] FM-PAC SRAM protocol spec grounded in `references/openmsx-21.0/src/sound/MSXFmPac.cc` (§2.4)
- [x] Bug B root-cause investigation plan + diagnostic flow (§2.5)
- [x] Observe-act loop protocol + determinism guarantees (§2.3)

### Implementation Artifacts (Developer Responsibility)

- [ ] `.claude/agents/msx-playtest.md` (S1)
- [ ] `.claude/commands/msx-playtest.md` (S1)
- [ ] `tools/playtest-runner.ps1` (S2)
- [ ] `tests/unit/devices/audio/cartridge_fmpac_rom_unit_test.cpp` (S5)
- [ ] `src/devices/cartridge/cartridge_fmpac_rom.{h,cpp}` (S5)
- [ ] `tests/integration/machine/hbf1xv_m36_fmpac_cartridge_integration_test.cpp` (S5)
- [ ] `docs/m36-bug-a-investigation.md` (S4)
- [ ] `docs/m36-bug-b-root-cause.md` (S6)
- [ ] Updated `tests/integration/machine/hbf1xv_m35_multi_disk_integration_test.cpp` with disk-index advance case (S8)

### QA Artifacts

- [ ] Test evidence: `ctest` output (all slices)
- [ ] A/B parity evidence: `docs/openmsx-ab-smoke.md` (S4/S5/S6)
- [ ] Implementation report: `docs/m36-implementation-report.md`
- [ ] QA sign-off: `docs/m36-qa-signoff.md`

## 12. Open Questions for Developer Review

1. **S2 Multi-Frame Capture:** Recommend **Option A** (PowerShell wrapper, no src/ edits) for Phase 1. Assess if the cold-boot overhead (3–5s per iteration) is acceptable, or if **Option B** (`--dump-frames-interval`) should be fast-tracked.

2. **YS II Disk Format:** Confirm that `disks/games/ys2_disk1.dsk` and `ys2_disk2.dsk` are present in the repo and in standard 720 KB floppy format. If not, planner notes as a blocker and requests the images.

3. **FM-PAC as Cartridge vs Built-In:** The current spec assumes YS II loads as an FM-PAC cartridge in slot 1/2. If YS II is instead a data disk that expects SRAM in the built-in MSXMusic (slot 3-3), the plan must be revised. S4 investigation will clarify this.

4. **Bug B Scope Flexibility:** If S6 root-cause analysis reveals a major architectural issue (e.g., a missing CPU/VDP synchronization mechanism), the fix scope may exceed M36 capacity. In that case, S7 becomes a "diagnosis + recommendation for M37" rather than a full fix. Acceptable?

5. **A/B Parity Dependency:** M36 is the first milestone to use the FM-PAC cartridge mapper. Are openMSX A/B comparisons for this mapper in-scope, or should the planner/developer treat openMSX as the reference without a formal captured diff (acceptable per guardrails)?

---

**End of M36 Planner Package**

---

## A. Detailed Observe-Act Loop Pseudocode (Reference)

```python
# Pseudocode for the msx-playtest agent's main loop

def playtest(goal, max_iterations=50, initial_script=""):
    accumulated_script = parse_input_script(initial_script)
    frame_dir = ensure_directory(f"debug/playtest_{goal}")
    iteration = 0
    
    while iteration < max_iterations:
        # STEP 1: Run emulator with accumulated script
        emulator_run = run_headless_emulator(
            disk=GAME_DISK,
            input_script=accumulated_script,
            dump_frame_at_intervals=[...]  # every ~73ms or N cycles
        )
        
        # STEP 2: Capture frames
        frames = []
        for i, frame_dump_text in enumerate(emulator_run.frame_dumps):
            png_path = f"{frame_dir}/frame_{iteration}_{i}.png"
            convert_frame_dump_to_png(frame_dump_text, png_path)
            frames.append(png_path)
        
        # STEP 3: Vision analysis (agent reads PNGs)
        latest_frame = frames[-1]
        state = analyze_screen(latest_frame, goal)
        #  -> returns: {"landmark": "title_screen", "confidence": 0.95, ...}
        #          or: {"landmark": "black_screen", "anomaly": True, ...}
        #          or: {"landmark": None, "progress": "in_gameplay", ...}
        
        # STEP 4: Landmark detection
        if state["landmark"] == goal:
            log(f"GOAL REACHED: {goal} at iteration {iteration}")
            save_report(...)
            return accumulated_script, frames, "success"
        
        # STEP 5: Decide next input
        if state.get("anomaly"):
            log(f"ANOMALY DETECTED: {state['anomaly']}")
            save_diagnostics(emulator_run)
            return accumulated_script, frames, f"anomaly: {state['anomaly']}"
        
        next_input = decide_next_keystroke(state, goal)
        if next_input is None:
            log(f"STALLED: cannot progress from {state}")
            # Fallback: request human hint
            hint = request_human_hint(state, accumulated_script)
            if hint:
                next_input = hint
            else:
                return accumulated_script, frames, "stalled"
        
        # STEP 6: Append to script (deterministic replay prefix preserved)
        tstate = emulator_run.final_tstate
        new_event = InputScriptEvent(tstate, next_input, pressed=True)
        new_event2 = InputScriptEvent(tstate + 30, next_input, pressed=False)  # release after 30 cycles
        accumulated_script.events.extend([new_event, new_event2])
        
        iteration += 1
    
    log(f"TIMEOUT: reached max_iterations ({max_iterations})")
    return accumulated_script, frames, "timeout"
```

## B. FM-PAC Register Map Reference

| Address | Name | Access | Bits | Purpose |
|---------|------|--------|------|---------|
| 0x3FF4 | YM2413 Address Port | W | 7-0 | YM2413 register address (if OPLL mode) |
| 0x3FF5 | YM2413 Data Port | W | 7-0 | YM2413 register data |
| 0x3FF6 | Enable Register | R/W | Bit-0: I/O enable; Bit-4: SRAM force-reset | Control register |
| 0x3FF7 | Bank Register | R/W | Bits 1-0: ROM bank select (0-3) | Bank selector |
| 0x3FFE | r1ffe (SRAM unlock part 1) | R/W | 7-0 | Must be 0x4D to unlock SRAM |
| 0x3FFF | r1fff (SRAM unlock part 2) | R/W | 7-0 | Must be 0x69 to unlock SRAM |
| 0x4000–0x5FFD | SRAM Window | R/W | 8-bit | 8 KB battery-backed store (when unlocked) |

## C. Mandatory System-Wide Architectural Review Requirement (Per Coordinator Directive)

**CRITICAL RULE:** Both Bug A (FM-PAC SRAM) and Bug B (black screen) fixes MUST be reviewed for SYSTEM-WIDE IMPACT BEFORE CLOSURE. This is NOT a code-review formality; it is an ARCHITECTURAL VALIDATION that prevents LOCAL PATCHES from silently breaking NEIGHBOR SUBSYSTEMS.

### Pre-Closure Checklist (QA + Planner MUST verify):

**Bug A (FM-PAC SRAM) — Mandatory Pre-Closure Validation:**

1. **Slot/Memory Routing Integrity:**
   - [ ] FM-PAC 0x4000–0x5FFF window is routed ONLY through the FM-PAC device (not bypassed to mapper or other device).
   - [ ] Slot priority rules verified: FM-PAC address at slot 1 or 2 takes priority over any conflicting address in slot 3-0 mapper.
   - [ ] Other cartridges (Konami, ASCII*) can still load in parallel slots without FM-PAC interference (cross-cartridge isolation).

2. **ROM/SRAM Multiplexing (State Machine):**
   - [ ] Memory routing correctly switches between ROM and SRAM based on `sramEnabled` state (not partially).
   - [ ] Bank register (0x3FF7) affects ONLY ROM bank, not SRAM access (SRAM always accessible at 0x4000–0x5FFF when unlocked).
   - [ ] No address aliasing or bus contention (two devices claiming the same address).

3. **Unlock Protocol Correctness:**
   - [ ] SRAM only unlocks when BOTH r1ffe == 0x4D AND r1fff == 0x69 are latched (not either-or).
   - [ ] Any other r1ffe/r1fff values lock SRAM (re-check on every write).
   - [ ] Enable register bit-4 force-reset correctly clears both latches.

4. **OPLL Coexistence:**
   - [ ] YM2413 OPLL register writes (#7C/#7D) work correctly when FM-PAC SRAM is unlocked/locked (independent state).
   - [ ] No I/O port collision or interference between FM-PAC and OPLL operations.

5. **Persistence (If Applicable):**
   - [ ] SRAM content loads from disk on boot (using `BatteryBackedSram::load()`).
   - [ ] SRAM content saves to disk on shutdown (using `BatteryBackedSram::save()`).
   - [ ] Persistence path does NOT conflict with MSX-JE Halnote SRAM path (separate save files).

6. **Regression Testing Results:**
   - [ ] ALL existing cartridge tests pass (Konami, ASCII8, ASCII16, Generic8, Generic16, Mirrored).
   - [ ] ALL memory-mapper tests pass (page switching, bank selection).
   - [ ] ALL disk-boot (BIOS) tests pass (FDC reading from correct slot, no interference).
   - [ ] ALL MSXMusic (#7C/#7D) tests pass (internal sound device unaffected).
   - [ ] ctest: 100% pass rate on full suite, zero failures/skips.

7. **A/B Parity Evidence:**
   - [ ] openMSX A/B run COMPLETED and diff reviewed (not skipped, not assumed).
   - [ ] If empty diff: documented as "behavior identical to openMSX."
   - [ ] If non-empty diff: documented as "known divergence XYZ" with explanation (never hide divergences).

---

**Bug B (Black Screen) — Mandatory Pre-Closure Validation:**

1. **Full-Pipeline Root-Cause Identified:**
   - [ ] Root-cause analysis (§2.5, §6) completed and localized to EXACTLY ONE pipeline stage (1–5).
   - [ ] Stage and symptom documented in `docs/m36-bug-b-root-cause.md` (not hand-wavy).
   - [ ] If analysis is inconclusive, explicitly documented as "inconclusive; further investigation required" (do NOT proceed to S7).

2. **Diagnostic Evidence Complete:**
   - [ ] FDC state (register file, state machine, INTRQ/DRQ timeline) captured at black-screen moment.
   - [ ] CPU state (PC, halted flag, registers) captured at black-screen moment.
   - [ ] VDP state (register 1, VRAM sample, address pointer) captured at black-screen moment.
   - [ ] Frame PNG sequence (5+ frames pre-entry → black-screen) captured.
   - [ ] All evidence deterministic (reproducible via playtest script).

3. **openMSX A/B Comparison Completed:**
   - [ ] Same playtest script run on openMSX 19.1 (WSL).
   - [ ] State dumps at equivalent frame captured on openMSX.
   - [ ] Diff analysis identifies which component (FDC/CPU/VDP) first diverges.
   - [ ] If openMSX unavailable: explicitly documented as blocker; root-cause analysis conditional.

4. **Fix Scope Conditional on Root-Cause:**
   - [ ] If FDC: fix targets WD2793 state machine (sector read, INTRQ timing, DRQ logic).
   - [ ] If CPU: fix targets CPU core (halt behavior, interrupt acceptance, timing).
   - [ ] If VDP: fix targets VDP register write routing, VRAM access, or rendering logic.
   - [ ] Scope is MINIMAL (no gratuitous refactor; only the broken stage).

5. **Post-Fix Regression Testing:**
   - [ ] Game proceeds past building-entry; interior graphics render.
   - [ ] openMSX A/B comparison re-run; outcome is now identical (game continues on both).
   - [ ] ALL M1–M35 tests pass unchanged (zero regression).
   - [ ] ctest: 100% pass rate on full suite, zero failures/skips.

6. **No Cosmetic Patches:**
   - [ ] Fix does NOT mask a real bug (no "clear VRAM then render black" workaround).
   - [ ] If a bug cannot be fully fixed in M36, it is documented as deferred (not hidden).

---

**Closure Gate (QA + Coordinator Sign-Off REQUIRED):**

- [ ] Bug A: ALL 7 pre-closure checks passed; regression tests green; A/B parity confirmed.
  - QA sign-off: `docs/m36-qa-signoff.md` section 2A.
  
- [ ] Bug B: ALL 6 pre-closure checks passed; root-cause identified; regression tests green; no cosmetic patches.
  - QA sign-off: `docs/m36-qa-signoff.md` section 2B.

- [ ] Planner review: system-wide impact assessment confirms no silent breakage of neighbors.
  - Planner sign-off: `docs/m36-qa-signoff.md` section 3.

- [ ] Coordinator review: all evidence gates completed; protocol artifacts updated; ready for human release decision or tag.
  - Coordinator sign-off: `docs/m36-qa-signoff.md` section 4.

