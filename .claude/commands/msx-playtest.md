# /msx-playtest Command

Invoke the autonomous MSX playtest agent to simulate human player actions and reach a specific goal landmark.

## Syntax

```
/msx-playtest 
  goal="<goal_name>"
  [bios_dir="<path>"]
  [disk="<path>"]
  [max_iterations=30]
  [frames_per_iteration=50]
  [existing_script="<path>"]
  [output_dir="<path>"]
```

## Parameters

### goal (required)

Human-readable goal name describing what the playtest should achieve.

- **String**: Alphanumeric + underscores (e.g., "boot_to_basic", "ys2_title", "building_entry")
- **Used in**: Output directory naming, report filename
- **Examples**:
  - `goal="boot_to_basic"` → Reach BIOS startup and MSX-BASIC "Ok" prompt
  - `goal="ys2_title"` → Reach YS II title/menu screen
  - `goal="ys2_building_entry"` → Reach the point where the player enters a building
  - `goal="aleste_demo"` → Reach Aleste demo screen

### bios_dir (optional, default: "bios")

Path to the BIOS/ROM assets directory. Must contain:
- Basic & BIOS ROM (`basic.rom`, `bios.rom`)
- Sub-ROM, Kanji ROM, and Disk ROM variants (as available)

- **Type**: File path (relative or absolute)
- **Default**: `bios` (in repository root)
- **Example**: `bios_dir="bios"` or `bios_dir="/path/to/msx/roms"`

### disk (optional, default: empty)

Path to a floppy disk image (e.g., `.dsk` or 720KB image file) to attach.

- **Type**: File path (relative or absolute)
- **Default**: No disk (emulator boots from BIOS only)
- **Example**: `disk="games/disks/ys2/d1.dsk"`
- **Note**: M35 multi-disk support — if a disk is attached, the playtest agent can use F11 to trigger disk swaps

### max_iterations (optional, default: 30)

Maximum number of observe-act-replay iterations before giving up.

- **Type**: Integer, 1–100
- **Default**: 30 (typically 30–50 frames per iteration × ~16ms per frame ≈ 8-16 seconds real emulation time)
- **Example**: `max_iterations=50` for a more complex goal
- **Behavior**: If agent cannot reach goal after N iterations, it requests a human hint

### frames_per_iteration (optional, default: 50)

Number of frames to execute per iteration (frame-loop shape with VBlank).

- **Type**: Integer, 1–500
- **Default**: 50 (at 60 Hz ≈ 833ms per iteration; ~10 sec to reach goal with 30 iterations)
- **Example**: `frames_per_iteration=100` for slower, more detailed landmark detection
- **Note**: Larger frame budgets reduce iteration count but increase per-iteration runtime

### existing_script (optional, default: empty)

Path to an existing input script to use as the starting point. Useful for "continue from checkpoint" scenarios.

- **Type**: File path (absolute or relative to CWD)
- **Default**: Start from scratch (empty script, cold boot)
- **Example**: `existing_script="debug/playtest_ys2_boot_20260710_123456.script"`
- **Behavior**: Agent loads the script, runs from the end of it, then appends new keystrokes

### output_dir (optional, default: auto-generated)

Override the output directory path. Normally auto-generated as `debug/playtest_<goal>_<timestamp>/`.

- **Type**: File path (absolute or relative)
- **Default**: `debug/playtest_<goal>_<YYYYMMDD_HHMMSS>/`
- **Example**: `output_dir="debug/custom_playtest_run"`
- **Note**: Directory is created if it doesn't exist

## Command Invocation

The coordinator (MSX Master Agent or other) invokes this command to spawn the msx-playtest agent.

**Example 1: Boot to BASIC (most reliable landmark)**
```
/msx-playtest goal="boot_to_basic" bios_dir="bios"
```

**Example 2: Boot YS II and reach title screen**
```
/msx-playtest \
  goal="ys2_title" \
  bios_dir="bios" \
  disk="games/disks/ys2/d1.dsk" \
  max_iterations=50 \
  frames_per_iteration=100
```

**Example 3: Continue from a checkpoint script**
```
/msx-playtest \
  goal="ys2_building_entry" \
  bios_dir="bios" \
  disk="games/disks/ys2/d1.dsk" \
  existing_script="debug/playtest_ys2_title_20260710_120000.script" \
  max_iterations=30
```

## Agent Behavior

When invoked, the msx-playtest agent (opus, vision-capable):

1. **Validates inputs**: Checks bios_dir exists, disk (if provided) is readable, frames_per_iteration is sane
2. **Initializes**: Sets up output directory structure
3. **Runs the observe-act-replay loop** (see `.claude/agents/msx-playtest.md`):
   - Iteration 1: Run emulator with empty (or existing) script, capture frames, observe goal/landmarks
   - Iteration 2+: Append keystroke(s), replay (deterministic), observe, decide
4. **Produces artifacts**:
   - `playtest_<goal>_<timestamp>.script` (reusable input script)
   - `frames/frame_*.png` (evidence frames)
   - `playtest_<goal>_<timestamp>_report.md` (summary report)
5. **Reports to coordinator**: Success (goal reached), stalled (awaiting human hint), or error

## Output Artifacts

All outputs are in `debug/playtest_<goal>_<timestamp>/` (or custom output_dir if specified).

### Generated Files

| File | Purpose | Format |
|------|---------|--------|
| `playtest_<goal>_<timestamp>.script` | Reusable cycle-stamped input script | HBF1XV-INPUT-SCRIPT v1 ASCII |
| `frames/frame_0000.png` | Frame 0 (starting state) | PNG (RGB truecolor) |
| `frames/frame_0001.png` | Frame 1 (after 1 frame of emulation) | PNG (RGB truecolor) |
| `frames/frame_NNNN.png` | Frame N (end of current iteration) | PNG (RGB truecolor) |
| `playtest_<goal>_<timestamp>_report.md` | Markdown summary | Markdown |
| `frames_raw/frame_dump_*.txt` | Raw frame dumps (intermediate) | HBF1XV-FRAME-DUMP v1 ASCII |

### Report Contents

The `.md` report includes:
- Goal name and status (REACHED / STALLED / ERROR)
- Iteration count and frames evaluated
- Landmarks detected (visual states observed)
- Key sequence applied (summary of keystroke events)
- Evidence links (references to PNG frames)
- Any anomalies or observations
- Instructions for reuse (how to replay the script)

## Determinism & Replayability

All artifacts are deterministic:

- **Script**: Cycle-stamped, so `sony_msx_headless --input-script <script> --frames N` produces byte-for-byte identical frame sequence
- **Frames**: Identical scripts produce identical PNGs
- **Report**: Deterministic (no timestamps, only facts)

**Example reuse**:
```bash
# Original playtest run created:
# debug/playtest_ys2_title_20260710_120000/playtest_ys2_title_20260710_120000.script

# Future regression test replays it identically:
sony_msx_headless.exe --debug-session bios 999999999 \
  --frames 100 \
  --disk games/disks/ys2/d1.dsk \
  --input-script debug/playtest_ys2_title_20260710_120000/playtest_ys2_title_20260710_120000.script \
  --dump-frame replay_test
```

## Landmarks & Detection Examples

### boot_to_basic

**Description**: BIOS startup, MSX-BASIC interpreter at "Ok" prompt.

**Visual landmarks**:
1. Frame 0–10: Black screen (cold boot, BIOS load)
2. Frame 10–80: Blue border, "MSX2+" or startup logo
3. Frame 80–120: MSX-BASIC welcome banner, text-mode screen
4. Frame 100+: "Ok" prompt visible at bottom-left

**Expected keystrokes**: None (automatic boot)

**Success criteria**: Frame shows "Ok" prompt, text cursor blinking

### ys2_title

**Description**: YS II game boots from disk, reaches title screen/main menu.

**Visual landmarks**:
1. Frame 0–50: Disk boot (black screen, activity)
2. Frame 50–150: Game initialization, graphics loading
3. Frame 150+: Title screen visible (game logo, menu text "New Game", "Load Game", etc.)

**Expected keystrokes**:
- T=0: (none, auto-boot from disk)
- Can press DOWN/UP to navigate menu (if desired)

**Success criteria**: Frame shows title screen UI with menu options visible

### ys2_building_entry

**Description**: YS II gameplay progresses to the point where player enters a building and sees interior graphics.

**Visual landmarks**:
1. Frame 0–150: Disk boot + title screen (from ys2_title)
2. Frame 150–300: Menu navigation (press DOWN/RETURN to start new game)
3. Frame 300–500: Gameplay, city map screen visible
4. Frame 500+: Player-controlled avatar visible, can navigate to building

**Expected keystrokes**:
- RETURN or SPACE to select "New Game"
- Arrow keys (UP/DOWN/LEFT/RIGHT) to navigate map
- RETURN to enter building

**Success criteria**: Frame shows building interior graphics (different color palette, indoor tiles)

### aleste_demo

**Description**: Aleste (arcade-style shmup) cartridge boots and reaches the demo/title screen.

**Visual landmarks**:
1. Frame 0–30: Cartridge boot (black screen)
2. Frame 30–60: Game initialization, graphics loading
3. Frame 60+: Demo screen visible (game logo, copyright, menu)

**Expected keystrokes**: None or RETURN to proceed

**Success criteria**: Frame shows Aleste demo/title screen

## Error Handling & Fallbacks

### Emulator Failure

If `tools/playtest-capture.ps1` fails:
- Agent logs the exit code and stderr
- Attempts 1 retry (common transients)
- If persistent, reports error and requests human hint

### Disk Not Found

If disk path is invalid:
- Agent logs "disk not found" warning
- Proceeds with headless boot (BIOS only) if appropriate
- May not reach the goal (e.g., YS II requires disk)

### Vision Stall

If agent cannot detect progress after 3+ iterations:
- Agent outputs a human-readable prompt requesting a hint
- Example: "Unable to detect visual change. Is frame visually at goal? What key next?"
- Awaits human response (coordinator relays hint)

### Iteration Budget Exceeded

If max_iterations reached without goal:
- Agent reports "STALLED" in final report
- Provides evidence (last frames, script so far)
- Coordinator may increase budget or ask human for hint

## Future Enhancements (Out of Scope for M36 Phase 1)

- Joystick input support (currently keyboard-only)
- SDL3 real-window observation (Phase 1 is headless PNG only)
- Audio feedback processing (Phase 1 is visual only)
- Advanced RL-based navigation (Phase 1 is rule-based heuristics)
- Automatic frame-rate tuning based on game responsiveness
- Playtest result caching/deduplication (to skip redundant runs)

## Reference Precedence (Correctness Judgments)

When the agent judges whether an observed frame is correct vs. an anomaly/bug, the mission is a
machine as genuine to real HB-F1XV hardware as possible: apply the STRICT reference-precedence rule
in `CLAUDE.md` — real hardware spec (fact-sheets or primary hardware docs) > openMSX > fMSX, with
the spec winning on a confirmed conflict. openMSX A/B stays the parity harness, but "differs from
openMSX" is not automatically a defect when the real-hardware spec says otherwise; flag such
divergences for developer/QA rather than resolving them in-agent.

## See Also

- `.claude/agents/msx-playtest.md` — Agent implementation details
- `CLAUDE.md` — Reference precedence (authority order — STRICT) rule
- `tools/playtest-capture.ps1` — Frame capture wrapper script
- `docs/m36-planner-package.md` — Full M36 specification
- `src/machine/input_script.h` — Input script format reference
- `src/machine/frame_dump.h` — Frame dump format reference
