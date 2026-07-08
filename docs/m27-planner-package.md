# M27 Planner Package — Production Hardening + Debug/Test Tooling

- Milestone ID: M27
- Title: Production Hardening + Debug/Test Tooling — full state-dump/CPU-trace/event-log CLI
  wiring, real decoded PSG audio capture to file, keystroke-sequencing/scripted-input automation,
  and a genuine, executable replay-determinism proof
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M27-001 (kickoff, planner-first, no production code;
  `agent-protocol/channels/requests.md:1354-1368`)
- Kickoff summary: `agent-protocol/state/milestones.md:755-767` ("M27 (Kickoff 2026-07-08)")
- Decisions/memories in force: DEC-0005 (every planner package restates all 34 backlog rows), the
  project memory `feedback_license_sensitive_scope.md` (zero license-sensitive future work — this
  milestone touches no `references/openmsx-21.0/` data-table content at all, N/A), and the human's
  2026-07-08 directive now recorded as project memory `feedback_slow_test_cadence.md` ("let's not
  do slow test until it's really necessary") — governs the evidence-gate cadence throughout §6.
- Backlog effect this cycle: **this milestone closes no backlog row outright** — it is
  infrastructure/tooling work (debug CLI wiring, an audio-capture tool, a scripted-input mechanism,
  a determinism proof), not a device or presentation-layer deliverable. It does, however, build
  genuinely useful groundwork for two still-OPEN rows: **C5** (full-boot/auto-disk-boot-trigger
  investigation — item 3's scripted-input mechanism plus the disk-loading fix identified in §2.2
  give a future C5 milestone real tools it does not have today) and, more loosely, **E1** (YM2413
  DSP/synthesis depth — item 2 establishes the "dump real device samples to a file" pattern a
  future E1 milestone could mirror once/if a YM2413 `sample()` method ever exists). Neither C5 nor
  E1 is attempted or closed by M27 itself (§3's full 34-row review re-affirms both OPEN/IN-PROGRESS
  exactly as recorded).

> Grounding rule: every API cited below was independently read this cycle from the concrete
> repository file, not assumed from the M26 planner package's own summaries (that package is
> consulted only as a *format/precedent* reference, per the dispatch's explicit instruction — every
> substantive claim about current code state was re-verified by direct read/grep this cycle).
> `src/machine/debug_dump.{h,cpp}`, `debug_event_log.{h,cpp}`, `frame_dump.{h,cpp}`,
> `hbf1xv_machine.h` (the full debug-API block, lines ~280-345, and the accessor block, lines
> ~190-230), `src/main.cpp` (in full, 913 lines), `src/frontend/sdl3_cli.{h,cpp}`,
> `src/frontend/sdl3_app.h`, `src/frontend/sdl3_input_mapper.h`, `src/frontend/psg_audio_pump.{h,cpp}`,
> `src/devices/audio/psg_ym2149.{h,cpp}` (the `sample()`/`resolved_amplitude()`/`write_register()`
> block), `src/peripherals/keyboard_matrix.h`, `tools/mem-to-audio.py` (in full), `CMakeLists.txt`,
> and `tests/CMakeLists.txt` (in full, 984 lines) were all read directly this cycle.

---

## 1. Scope and Assumptions

### 1.1 In scope — the four named work items, each decomposed into concrete deliverables

1. **Full CPU/memory/VRAM state-dump CLI wiring.** `Hbf1xvMachine::write_state_dump()`
   (`hbf1xv_machine.h:326`), `write_cpu_trace()` (`:329`), and `write_event_log()` (`:332`) are
   real, already-implemented, already-unit/integration-tested APIs (M10-S3) — confirmed this cycle
   that **zero** call site exists in `src/main.cpp` or `src/frontend/sdl3_*` for any of the three
   (`grep -n "write_state_dump\|write_cpu_trace\|write_event_log" src/main.cpp
   src/frontend/sdl3_*.{h,cpp}` returns nothing). Wire all three into both executables, via a NEW
   bounded, real `step_cpu_instruction()`-driven headless run mode (§2.2 — the existing headless
   default path does not drive the CPU at all, a genuine prerequisite finding) and additive
   `Sdl3App`/`sdl3_cli` config fields.
2. **Real, decoded PSG audio capture to file.** `tools/mem-to-audio.py` (M10-S5) confirmed this
   cycle, by direct read of its own doc comment (`tools/mem-to-audio.py:6-9`), to be the exact same
   class of insufficient raw-bytes tool the M26 planner found in `tools/mem-to-png.py`: *"This is an
   INERT raw-buffer serialization only... It does NOT synthesize PSG (AY-3-8910) or YM2413/OPLL
   audio and models no sound chip at all."* `src/frontend/psg_audio_pump.{h,cpp}` (M26) is the real,
   already-unit-tested PSG sample-generation wiring — but it is currently consumed only by
   `Sdl3AudioPresenter` for live playback (confirmed: `grep -rn "PsgAudioPump" src/` returns only
   its own header/impl and `sdl3_audio_presenter.*`). Build a new, headless-buildable C++
   sample-dump capability reusing `PsgAudioPump`/`PsgYm2149` genuinely (§2.3), plus a NEW Python
   WAV-writer tool (not extending `mem-to-audio.py`, mirroring the exact `frame_dump`-vs-
   `mem-to-png.py` precedent), with at least one real, committed, audibly-non-trivial example WAV
   under `debug/sounds/`.
3. **Keystroke-sequencing/scripted-input automation.** Confirmed this cycle: `grep -rn "set_key"
   src/` shows `KeyboardMatrix::set_key(int row, int column, bool pressed)`
   (`src/peripherals/keyboard_matrix.h:32`) is a raw, single-call, no-macro API; `grep -rniE
   "script|macro" src/` (excluding `cartridge_cli`/`parity-trace`-style unrelated hits) finds no
   scripting/macro layer anywhere over it in either executable. Design and build a deterministic,
   timed key-event script format (read/write) plus a CLI flag driving it against BOTH executables
   (§2.4) — general-purpose, not itself attempting the C5 auto-boot investigation.
4. **Debug event-log CLI wiring + a genuine replay-determinism proof.** Wire
   `set_event_logging_enabled()`/`write_event_log()` to a CLI flag on both executables (shares the
   §2.2 mechanism with item 1), then build a concrete, adversarially-validated proof that two
   independent, identical runs produce byte-for-byte identical event-log files (§2.5) — a real,
   executable demonstration of this project's own core determinism claim via the debug tooling
   itself.
5. **Full 34-row deferred-backlog review** (§3, DEC-0005).
6. **Zero regression** across the full M1-M26 suite (both build configurations), using the
   fast-subset-by-default evidence-gate discipline named explicitly in §6 per the human's
   `feedback_slow_test_cadence.md` directive.

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M27 | Owning milestone (candidate) |
|---|---|---|
| **Vague/open-ended "stress testing"** (no concrete, testable claim attached) | Explicitly named OUT by REQ-M27-001 itself unless a concrete, well-grounded sub-claim is identified. None was found this cycle that isn't already covered by items 1-4's own concrete acceptance criteria (e.g. item 4's replay-determinism proof IS this milestone's one genuine "production readiness" claim — it is not renamed "stress testing" for effect). | N/A — not a real gap today |
| **"Cross-platform build validation"** (no concrete, testable claim attached) | Same reasoning; this project's build flow is already CMake-portable by construction (no OS-specific `src/` code outside `src/frontend/` SDL3 calls, which are themselves cross-platform by SDL3's own design) and no repository evidence names a concrete cross-platform defect to chase. | N/A |
| **The C5 full-boot/auto-disk-boot-trigger investigation itself** | Item 3 builds the *general-purpose* scripting capability C5 will need; REQ-M27-001 is explicit that "this milestone does NOT need to attempt the C5 investigation itself." | A future dedicated C5 milestone (backlog, still IN-PROGRESS (M16 partial)) |
| **Any YM2413/FM-PAC waveform-synthesis work** (E1) | Out of scope; item 2's audio-dump capability is PSG-only, mirroring M26's own R-M26-5 discipline exactly — `Ym2413Opll` has no `sample()` method (confirmed unchanged this cycle, `grep -n "sample" src/devices/audio/ym2413_opll.h` returns nothing) and this project's culture explicitly disallows fabricating a DSP pipeline. | A future dedicated audio-rendering milestone (E1) |
| **Numeric-keypad rows 9-10 in the new symbolic key-name table** | `Sdl3InputMapper::scancode_map()` (M26) already disclosed rows 9-10 as an out-of-scope simplification (`sdl3_input_mapper.h:33-34`); the new headless key-name table (§2.4) inherits the SAME disclosed boundary for consistency, rather than silently extending coverage the SDL3 side never had. | A future input-depth milestone, if ever needed |
| **Joystick-event scripting** (only keyboard events are a hard requirement) | REQ-M27-001's own item-3 text emphasizes "a simple, deterministic timed key-event script format" — keyboard is the concrete ask. Joystick-event script support is a natural, low-risk, structurally-similar extension the developer MAY add opportunistically if trivial, but is not a required acceptance item (avoids speculative scope with no attached test, per the dispatch's explicit instruction). | Developer's judgment this cycle, or a future cycle |
| **A configuration/remapping layer, multi-window support, or any other M26-named out-of-scope item** | Unrelated to this milestone's four items; re-affirmed out per the M26 precedent (§1.2 of `docs/m26-planner-package.md`), never revisited here. | N/A |
| **Wiring `--dump-audio`/`--input-script`/state-dump flags onto `sony_msx_sdl3` for EVERY item as a hard requirement** | Items 1 and 3 (state-dump/trace/event-log, scripted-input) explicitly require BOTH executables per REQ-M27-001's own text ("driving it against either executable" / "CLI flag on both executables") — these ARE hard, both-executable requirements (§4). Item 2 (audio capture) is explicitly scoped headless-only in REQ-M27-001 ("a new, deterministic C++ sample-dump capability (headless-buildable, no SDL3 dependency, reusing PsgAudioPump)") — SDL3-side audio-dump wiring is optional/developer's-judgment, not a hard AC, since SDL3's live audio path already exists (M26) and the headless capture path is the genuinely new, CI-friendly capability. | N/A — precisely scoped per the dispatch's own wording |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M27-1 (the headless executable's default run path never drives the CPU at all today — a
  genuine, load-bearing finding, not an assumption).** `src/main.cpp:899-911` (read in full this
  cycle): the default path is `cold_boot()` → `load_cartridges_from_args()` →
  **exactly one** `machine.run_frame()` call → print three counters → exit. `run_frame()`
  (confirmed via the M26-era doc comment still governing it, and independently via
  `hbf1xv_machine.cpp`) does `scheduler_.tick(kFrameCycles); on_vsync_boundary();` — it **never**
  calls `cpu_.step()`. This means a state dump/CPU trace/event log taken at the end of today's
  default headless run would show the machine exactly as `cold_boot()` left it (PC=0 or the BIOS
  reset vector, zero instructions retired) — of essentially no debugging value. A genuine,
  CPU-driven bounded run mode (§2.2) is therefore a necessary prerequisite for items 1 and 4 to be
  meaningful in the headless executable, not scope creep. *Verify:* the developer confirms this
  reading is still accurate immediately before implementing (a one-line `grep`/read), since this is
  the single most load-bearing finding this package rests on.
- **A-M27-2 (the headless executable's default run path never loads BIOS/Kanji/disk assets — a
  second, related finding).** `src/main.cpp:899-903` (same block): `set_asset_root()` is never
  called on the default path (only the dedicated `--bios-boot-trace`/`--halnote-parity` modes call
  it). Combined with A-M27-1, this means today's default headless run is not a realistic "boot the
  machine and capture its state" session at all. §2.2's new mode fixes both gaps together (loads
  real BIOS assets AND drives the CPU), a single, additive, well-justified fix. *Verify:* the
  developer confirms via `grep -n "set_asset_root" src/main.cpp` before relying on this.
- **A-M27-3 (the headless executable has no `--disk` flag at all — SDL3 has one, `sony_msx_headless`
  does not).** `grep -n "\-\-disk\|DiskImage" src/main.cpp` (run this cycle) returns **zero
  matches**. M26's own planner package explicitly left this as developer's discretion
  ("may, at their discretion, symmetrically add the same flag") and it was not taken up. Since
  item 3's scripted-input mechanism is explicitly meant to be useful for a *future* C5
  investigation (which will need disk-image loading on whichever executable drives it), and the
  underlying mechanism (`devices::fdc::DiskImage(std::vector<uint8_t>)`, confirmed still present
  and public per `Hbf1xvMachine::disk_image()`/`disk_drive()` being public non-const accessors) is
  already fully proven safe by M26's own A-M26-6, adding `--disk` to the new headless
  `--debug-session` mode (§2.2) is a small, low-risk, well-grounded, directly-justified fix folded
  into this cycle's scope — not a speculative addition. *Verify:* the developer confirms
  `machine.disk_image()`/`disk_drive()` accessors are unchanged since M26 before relying on this.
- **A-M27-4 (`PsgYm2149::sample()`'s numeric range is exactly [0,62] per channel-pair sum — read
  directly from source, not assumed).** `src/devices/audio/psg_ym2149.cpp:267-318` (read this
  cycle): `resolved_amplitude()` returns a 5-bit value in `[0,31]` (either the envelope's `volume()
  & 0x1F`, or the fixed-level mapping `2*level+1`, max `2*15+1=31`). `sample()` computes
  `left = level[A] + level[B]`, `right = level[A] + level[C]` (fact-sheet §2's A=Center/B=Left/
  C=Right mixing, already established M15/M26) — each of `left`/`right` is therefore an integer in
  `[0, 62]`. This grounds a real, non-fabricated linear PCM-scaling design (§2.3), not an invented
  range. *Verify:* the developer re-reads this exact block before implementing the scaling formula,
  since an incorrect range would silently clip or under-drive the output.
- **A-M27-5 (`Sdl3AudioPresenter::kSampleRateHz = 44100` is the existing, already-shipped M26
  sample-rate choice — read directly, not assumed).** `src/frontend/sdl3_audio_presenter.h:39`
  (read this cycle). The new headless audio-dump capability should default to the SAME rate for
  consistency with the already-established M26 audio pacing, rather than inventing an unrelated
  default. *Verify:* the developer confirms this constant is unchanged before citing/matching it.
- **A-M27-6 (`Hbf1xvMachine::keyboard()`/`joystick()` are public, both const and non-const,
  confirmed by direct read).** `src/machine/hbf1xv_machine.h:213-216` (read this cycle). Item 3's
  scripted-input player needs only these two ALREADY-PUBLIC accessors — zero new machine-level
  accessor is required. *Verify:* the developer re-confirms before implementing (a one-line read).
- **A-M27-7 (`src/frontend/psg_audio_pump.cpp` and `src/frontend/sdl3_cli.cpp` are ALREADY compiled
  into the headless `sony_msx_core` static library today, with zero `SONY_MSX_ENABLE_SDL3` guard —
  a genuinely important, load-bearing build-graph finding, confirmed by direct read of
  `CMakeLists.txt`, not assumed).** `CMakeLists.txt:13-70` (read this cycle): the `add_library(
  sony_msx_core STATIC ...)` call lists `src/frontend/sdl3_cli.cpp` (line 68) and
  `src/frontend/psg_audio_pump.cpp` (line 69) as ordinary sources, OUTSIDE the
  `if(SONY_MSX_ENABLE_SDL3)` block (lines 77-90, which only adds `sdl3_app.cpp`,
  `sdl3_video_presenter.cpp`, `sdl3_audio_presenter.cpp`, `sdl3_input_mapper.cpp` — all genuinely
  SDL3-API-dependent files — to a SEPARATE `sony_msx_sdl3_frontend` target). This means
  `PsgAudioPump` is ALREADY reachable from a headless (`-DSONY_MSX_ENABLE_SDL3=OFF`) build today,
  confirming §2.3's design (reuse `PsgAudioPump` directly from a new, headless-buildable
  `src/frontend/psg_audio_dump.{h,cpp}` sibling file, added to the SAME unguarded `sony_msx_core`
  source list) is genuinely buildable, not merely hoped-for. *Verify:* the developer confirms the
  new file compiles under BOTH `-DSONY_MSX_ENABLE_SDL3=OFF` and `=ON` before relying on this.
- **A-M27-8 (the `src/machine/` → `src/frontend/` non-dependency boundary rule, `src/CLAUDE.md:15-19`,
  is a hard constraint this package's design must respect).** *"Keep frontend concerns out of
  src/core/, src/devices/, and src/machine/."* This is why §2.3 places the new audio-dump generator
  in `src/frontend/` (which MAY depend on `src/machine/`, the existing, established dependency
  direction — e.g. `Sdl3App` already includes `machine/hbf1xv_machine.h`) rather than in
  `src/machine/` (which must NOT depend on `src/frontend/`) — a deliberate, explicit design choice,
  not an oversight; unlike `frame_dump.*` (M26), which correctly lives in `src/machine/` because it
  depends only on `devices/video/frame_buffer.h` and machine-level `debug_dump.h`, never on
  anything frontend-specific. *Verify:* the developer's final file placement is checked against this
  boundary rule at implementation time, not assumed correct from this package alone.
- **A-M27-9 (this milestone's own work is expected to leave `src/devices/cpu/`, `src/core/`
  genuinely untouched, and `src/peripherals/`/`src/devices/` touched ONLY via wholly NEW, additive
  files with zero edits to any pre-existing file's clock/timing-relevant code — a design GOAL this
  package sets, not yet a proven fact, since no implementation exists yet).** Every one of items
  1-4's designs above (§2.2-§2.5) calls ONLY already-public, already-existing accessor methods
  (`machine.psg()`, `machine.keyboard()`, `machine.joystick()`, `machine.write_state_dump()`, etc.)
  — none require editing `PsgYm2149`, `KeyboardMatrix`, `JoystickPorts`, `Z80aCpu`, or any
  `src/core/` file. The ONLY genuinely new files under `src/peripherals/` (`msx_key_names.*`) and
  `src/devices/` (none) are additive. *Verify (hard, named explicitly in §6):* the developer/QA
  confirm this via `git diff v1.0.26 --stat -- src/devices/cpu/ src/core/` (must be EMPTY) and
  `git diff v1.0.26 --stat -- src/devices/ src/peripherals/` (must show ONLY new files, zero
  modified-existing-file lines) at every gate — this is the exact, precise interpretation of the
  human's `feedback_slow_test_cadence.md` verification instruction (§6 elaborates: the git-diff
  check's purpose is confirming zero edits to *existing* timing-sensitive files, not forbidding any
  new file under those path prefixes ever again).

---

## 2. Spec Summary

### 2.1 Existing seams this milestone binds to (read in full/grepped this cycle)

| Existing API | Source | Role in M27 |
|---|---|---|
| `Hbf1xvMachine::write_state_dump(filename)` / `serialize_state_dump()` | `hbf1xv_machine.h:321,326` | CLI-wired, item 1 |
| `Hbf1xvMachine::write_cpu_trace(filename)` / `set_cpu_trace_enabled(bool)` | `hbf1xv_machine.h:329`; `cpu_trace_sink_` block | CLI-wired, item 1 |
| `Hbf1xvMachine::write_event_log(filename)` / `set_event_logging_enabled(bool)` | `hbf1xv_machine.h:310,332` | CLI-wired, items 1 & 4 |
| `Hbf1xvMachine::set_debug_root(path)` / `debug_root()` | `hbf1xv_machine.h:316-317` | Reused unmodified for the new `sounds/` subfolder convention too |
| `Hbf1xvMachine::set_asset_root(path)` | pre-existing (M13) | New `--debug-session` mode's BIOS loading |
| `devices::fdc::DiskImage(std::vector<uint8_t>)` / `machine.disk_image()`/`disk_drive()` | `disk_image.h:32`; `hbf1xv_machine.h` (A-M26-6 precedent) | New headless `--disk` flag (A-M27-3) |
| `machine::parse_cartridge_cli` | `src/machine/cartridge_cli.h` | Reused, not reimplemented, for the new mode's cartridge flags |
| `PsgYm2149::advance_cycles(delta)` / `sample()` / `write_register(reg, value)` | `psg_ym2149.h:81,83-89,141` | Item 2's real synthesis + demo-tone programming |
| `frontend::PsgAudioPump::pump_one_sample(psg)` / `pump_samples(psg, n)` | `psg_audio_pump.h:36,41` | Item 2's genuine reuse target (A-M27-7) |
| `Hbf1xvMachine::keyboard()` / `joystick()` | `hbf1xv_machine.h:213-216` (A-M27-6) | Item 3's script player target |
| `peripherals::KeyboardMatrix::set_key(row, col, pressed)` / `key(row, col)` | `keyboard_matrix.h:32-33` | Item 3's script player target |
| `Sdl3App` / `Sdl3AppConfig` / `run_one_frame()` / `run_interactive()` | `sdl3_app.h` | Items 1/3/4's SDL3-side wiring point |
| `frontend::parse_sdl3_cli` / `ParsedSdl3Cli` | `sdl3_cli.h` | Extended (additively) for items 1/3/4's SDL3 flags |
| `debug_dump::serialize_region()` | `debug_dump.h:36` | Reused for the new audio-dump PCM byte payload (mirrors `frame_dump.cpp`'s own reuse) |
| `DebugEventLog` / `DebugEvent` / `type_name()` / `format_event()` | `debug_event_log.h` | Item 4's replay-determinism proof reads this format directly |

### 2.2 Architecture decision — item 1: state-dump/CPU-trace/event-log CLI wiring via a new bounded, CPU-driven headless mode

**Decision: add ONE new, self-contained headless mode, `--debug-session`, mirroring the EXACT
established pattern of every other dedicated mode already in `src/main.cpp` (`--bios-boot-trace`,
`--vdp-parity`, `--frame-dump-demo`, etc.) — a wholly additive new `if` branch in `main()`, never
touching the pre-existing default run path's behavior.** This resolves A-M27-1/A-M27-2/A-M27-3 in
one place:

```
--debug-session <bios_dir> <max_steps>
    [--disk <path>] [--cart1 <path>] [--cart1-type <T>] [--cart2 ...] [--cart2-type ...]
    [--debug-root <path>]
    [--dump-state <name>] [--trace-cpu <name>] [--event-log <name>]
    [--input-script <path>]     (item 3, §2.4)
```

Implementation shape (mirrors `run_bios_boot_trace`'s existing `while (steps < max_steps &&
!machine.cpu().state().halted())` loop exactly, `src/main.cpp:708-712`, extended additively):

```cpp
// (illustrative; developer's exact shape per src/CLAUDE.md)
int run_debug_session(const std::string& bios_dir, std::uint32_t max_steps,
                       const DebugSessionOptions& opts, const std::vector<std::string>& cli_args) {
    Hbf1xvMachine machine;
    if (!opts.debug_root.empty()) machine.set_debug_root(opts.debug_root);
    if (opts.event_log_name)  machine.set_event_logging_enabled(true);   // BEFORE cold_boot (A-M27-9-adjacent ordering requirement, see risk R-M27-2)
    machine.set_asset_root(bios_dir);
    machine.cold_boot();
    if (const int rc = load_cartridges_from_args(machine, cli_args); rc != 0) return rc;
    if (opts.disk_path) { /* A-M27-3: mirrors A-M26-6 verbatim */ }
    if (opts.trace_cpu_name) machine.set_cpu_trace_enabled(true);

    InputScriptPlayer script_player = opts.input_script ? load_script(*opts.input_script) : InputScriptPlayer{};

    std::uint32_t steps = 0;
    while (steps < max_steps && !machine.cpu().state().halted()) {
        machine.step_cpu_instruction();
        script_player.apply_due(machine.elapsed_cycles(), machine.keyboard(), machine.joystick());
        ++steps;
    }

    if (opts.dump_state_name) machine.write_state_dump(*opts.dump_state_name);
    if (opts.trace_cpu_name)  machine.write_cpu_trace(*opts.trace_cpu_name);
    if (opts.event_log_name)  machine.write_event_log(*opts.event_log_name);
    return 0;
}
```

This is the SAME "stop stepping exactly at the halt boundary" convention every other call site in
this project uses (`tests/CLAUDE.md`'s established discipline, and explicitly the SAME loop shape
as `run_bios_boot_trace`/`run_parity_trace`) — zero new CPU-stepping semantics invented.

**SDL3 side:** additive `Sdl3AppConfig` fields (`std::optional<std::string> dump_state_filename,
trace_cpu_filename, event_log_filename`), all `std::nullopt` by default (a hard regression guard —
every pre-existing M26 `Sdl3App`/`Sdl3AppConfig` test must remain green unchanged, since the new
fields are additive-only with no-op defaults). `Sdl3App::init()` calls
`set_event_logging_enabled(true)` BEFORE `cold_boot()` when `event_log_filename` is set (mirrors
the ordering requirement above); the three `write_*` calls happen once, at the end of
`run_interactive()`'s bounded loop (when `max_frames` is reached) or on `SDL_EVENT_QUIT` — whichever
comes first, added to the EXISTING loop-exit path, not a new one.

### 2.3 Architecture decision — item 2: headless PSG audio-dump capability + new WAV tool

**Decision: a new `src/frontend/psg_audio_dump.{h,cpp}` (headless-buildable per A-M27-7, added to
the unguarded `sony_msx_core` source list — NOT the `SONY_MSX_ENABLE_SDL3`-gated
`sony_msx_sdl3_frontend` target), genuinely reusing `PsgAudioPump`/`PsgYm2149` (per REQ-M27-001's
explicit instruction), zero new machine-level method (a deliberate risk-reduction choice — unlike
`frame_dump.*`, this capability needs no new `Hbf1xvMachine` method at all, since it only needs the
ALREADY-public `machine.psg()` accessor).**

```cpp
// src/frontend/psg_audio_dump.h (illustrative; mirrors frame_dump.h's exact doc-comment/format
// discipline)
namespace sony_msx::frontend {

inline constexpr const char* kAudioDumpFormatTag = "HBF1XV-AUDIO-DUMP v1";

// Deterministic decoded-PSG-audio dump: pumps `sample_count` real StereoSample
// values from `psg` via a NEW PsgAudioPump(cycles_per_sample) (genuine reuse of
// the M26 wiring, A-M27-7/A-M27-5's kSampleRateHz=44100 default), converts each
// to signed 16-bit PCM per the DOCUMENTED linear scale grounded in A-M27-4's
// real [0,62] resolved_amplitude() sum range, and serializes via the EXISTING
// machine::debug_dump::serialize_region() folded-hex routine (genuine reuse,
// mirrors frame_dump.cpp's own reuse precedent exactly -- src/frontend/ MAY
// depend on src/machine/, per A-M27-8).
[[nodiscard]] std::string serialize_psg_audio_dump(devices::audio::PsgYm2149& psg,
                                                    std::uint64_t sample_rate_hz,
                                                    std::size_t sample_count);

// Writes to <debug_root>/sounds/<filename> (a NEW debug/ subfolder, matching the
// existing traces//logs//frames/ convention -- CLAUDE.md's own explicit
// "organize sub-directories if needed" license). Takes debug_root explicitly
// (not a Hbf1xvMachine method) so this stays a pure src/frontend/ utility with
// zero new machine-level surface (A-M27-9's risk-reduction goal).
bool write_psg_audio_dump(const std::filesystem::path& debug_root, const std::string& filename,
                           devices::audio::PsgYm2149& psg, std::uint64_t sample_rate_hz,
                           std::size_t sample_count);

}  // namespace sony_msx::frontend
```

Dump format (mirrors `frame_dump.h`'s exact discipline):

```
HBF1XV-AUDIO-DUMP v1
[AUDIO] sample_rate=44100 channels=2 bits=16 samples=<N>
[PCM] size=<N*4>            (folded canonical hex, REUSING debug_dump::serialize_region())
[END]
```

**Linear PCM-scaling formula (a documented, disclosed, first-principles modeling choice — mirrors
this project's established "documented simplification, not a fabricated fact" standard, e.g. the
M21 renderer's own disclosed simplifications, M26's disclosed nearest-neighbor audio sampling):**
grounded in A-M27-4's real `[0, 62]` range, e.g. `pcm16 = static_cast<int16_t>((raw_sum * 65535 /
62) - 32768)` (maps 0 → −32768, 62 → +32767). The exact literal formula/rounding is the developer's
implementation detail as long as it is linear, deterministic, and documented in a code comment
citing A-M27-4's grounded range — no anti-aliasing/dithering is required (mirrors M26's own
disclosed audio-fidelity simplification).

**New `tools/audio-dump-to-wav.py`** (sibling of `tools/frame-to-png.py`, NOT extending
`tools/mem-to-audio.py` — same reasoning as why `frame-to-png.py` had to be new rather than
extending `mem-to-png.py`, per REQ-M27-001's explicit instruction): parses the new `[AUDIO]`/`[PCM]`
dump format (reusing/adapting the ALREADY-proven `parse_region_from_dump()` folding logic present
in BOTH `tools/mem-to-png.py` and `tools/mem-to-audio.py`), decodes the PCM bytes as little-endian
signed 16-bit stereo interleaved samples, and emits a real WAV file. The developer SHOULD factor a
small shared `tools/wav_encode_common.py` containing the already-hermetically-tested `encode_wav()`
function extracted verbatim from `tools/mem-to-audio.py` (a zero-behavior-change refactor — `
mem-to-audio.py` re-imports it, its own `--self-check` must still pass byte-for-byte identically),
mirroring the M26 `tools/png_encode_common.py` precedent exactly; OR, if the developer judges the
coupling not worth it, duplicate the small, already-proven encoder locally — either is acceptable,
developer's call (mirrors M26's own explicit "either is acceptable" framing).

**At least one real, committed, audibly-non-trivial example WAV under `debug/sounds/`** (hard
acceptance criterion, §4): produced by a new `--audio-dump-demo` headless mode (mirrors
`--frame-dump-demo`'s exact precedent) that programs a known, fixed tone on PSG channel A via
`machine.psg().write_register(reg, value)` (the ALREADY-existing, already-tested public API,
`psg_ym2149.h:141`) — e.g. a fixed tone-period + max fixed volume level (`R_AVOL`, level 15 →
`resolved_amplitude()`'s documented `2*15+1=31` maximum, per A-M27-4) — then calls
`write_psg_audio_dump()` for a fixed sample count. This is a real, non-silent, deterministically
hand-computable square-wave oracle (the tone generator's period/toggle logic is ALREADY unit-tested
since M15), not a blank/near-silent example — mirroring the M26 frame-dump-demo's own "vivid,
recognizable, not blank" evidence-design philosophy.

### 2.4 Architecture decision — item 3: scripted-input automation

**Decision: two new, small, additive files — `src/peripherals/msx_key_names.{h,cpp}` (a symbolic
key-name → (row, column) lookup table) and `src/machine/input_script.{h,cpp}` (the deterministic
timed script format + parser + player) — zero edits to any existing `KeyboardMatrix`/`JoystickPorts`
file.**

**`src/peripherals/msx_key_names.h`** (placed under `src/peripherals/` per `src/CLAUDE.md`'s own
boundary rule — "keyboard matrix, joystick, and slot-side peripheral adapters" is the textbook-
correct home for a keyboard key-name table, not `src/machine/`, per A-M27-8's boundary discipline):

```cpp
namespace sony_msx::peripherals {

// A symbolic, human-readable key-name -> (row, column) table for the MSX 11x8
// keyboard matrix, rows 0-8 (the SAME disclosed rows-9-10-excluded boundary
// Sdl3InputMapper::scancode_map() (M26) already established -- inherited for
// consistency, not silently extended). The row/column VALUE for each named key
// MUST be derived by directly reading the actual, already-tested
// Sdl3InputMapper::scancode_map() array (src/frontend/sdl3_input_mapper.cpp)
// and re-keying it by a descriptive string name instead of an SDL_Scancode --
// this is a re-EXPRESSION of already-verified facts, not a fresh
// re-derivation from any external source, and a dedicated SDL3-gated
// cross-consistency test (M27-S7) proves the two tables agree wherever a name
// logically corresponds to a scancode (e.g. "SPACE" here vs SDL_SCANCODE_SPACE
// there resolve to the identical (row, column)).
[[nodiscard]] std::optional<std::pair<int, int>> key_name_to_row_col(std::string_view name);

}  // namespace sony_msx::peripherals
```

**`src/machine/input_script.h`** (placed under `src/machine/`, mirroring `debug_dump.h`/
`frame_dump.h`'s own precedent of housing deterministic debug/test-tooling formats there; the
PLAYER class depends only on `peripherals::KeyboardMatrix`/`JoystickPorts`, both already
`#include`-able from `src/machine/` since `Hbf1xvMachine` itself already depends on them — no
boundary violation):

```
HBF1XV-INPUT-SCRIPT v1
T=<tstate-dec> KEY=<name> DOWN
T=<tstate-dec> KEY=<name> UP
[END]
```

`T` is the cumulative machine T-state (`Hbf1xvMachine::elapsed_cycles()`'s own deterministic clock
basis — the SAME field convention `DebugEvent::tstate` already uses, `debug_event_log.h:23`, a
genuine, deliberate cross-item consistency choice, not a coincidence). Events MUST be strictly
non-decreasing in `T` in the file (the parser rejects out-of-order scripts with a clear error,
mirroring `parse_frame_dump()`'s "throws on malformed input, never silently returns garbage"
discipline, `frame_dump.h:44-45`).

```cpp
// src/machine/input_script.h (illustrative)
struct InputScriptEvent {
    std::uint64_t at_tstate;
    std::string key_name;
    bool pressed;
};

[[nodiscard]] std::vector<InputScriptEvent> parse_input_script(const std::string& text);
[[nodiscard]] std::string serialize_input_script(const std::vector<InputScriptEvent>& events);

// A monotonic-cursor player -- never re-scans from the start, applies each due
// event EXACTLY once, in file order (mirrors this project's established
// event-driven, monotonic-cursor architectural precedent: the M16 FDC
// DRQ/INTRQ state machine and the M22 VDP command engine's LMCM/LMMC/HMMC
// event-driven commands -- an architectural PATTERN citation, not identical
// code).
class InputScriptPlayer {
public:
    explicit InputScriptPlayer(std::vector<InputScriptEvent> events = {});
    // Applies every event with at_tstate <= current_tstate not yet applied, in
    // order, via peripherals::msx_key_names::key_name_to_row_col() + KeyboardMatrix::set_key().
    void apply_due(std::uint64_t current_tstate, peripherals::KeyboardMatrix& keyboard);
private:
    std::vector<InputScriptEvent> events_;
    std::size_t cursor_ = 0;
};
```

**CLI wiring:** `--input-script <path>` on BOTH executables (headless: a new flag on
`--debug-session`, §2.2; SDL3: a new additive `Sdl3AppConfig::input_script_path` field). The
driving hook is IDENTICAL in shape on both sides — call `apply_due(machine.elapsed_cycles(), ...)`
once per `step_cpu_instruction()` call, inside the SAME CPU sub-loop both executables already run
(§2.2's new headless loop; `Sdl3App::run_one_frame()`'s EXISTING CPU sub-loop, additively extended)
— a deliberate design choice enabling a genuine, testable cross-executable claim: **the same
script, against the same asset root/cartridge/disk, produces identical final machine state
regardless of which executable drives it** (§4 Acceptance Criterion 12).

Joystick-event scripting (`JOY=<port>,<button> DOWN|UP`) is a natural, structurally similar
extension the developer MAY add opportunistically if trivial (§1.2) — NOT a required acceptance
item this cycle.

### 2.5 Architecture decision — item 4: event-log CLI wiring (shares §2.2) + the replay-determinism proof

The CLI wiring itself is entirely covered by §2.2's `--event-log <name>` flag (shared with item 1)
— no separate mechanism is needed. The genuinely NEW deliverable for item 4 is the **proof**:

**A new, dedicated, headless `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp`**
(mirrors the M24/M25/M26 "one dedicated system test per milestone's headline claim" precedent):

1. Construct TWO wholly independent, freshly-constructed `Hbf1xvMachine` instances (never the same
   object re-run — genuine cross-instance independence, mirroring the M16 "real-boot max PC
   ... deterministic, two-run identical" precedent for what counts as a real determinism proof in
   this project).
2. Configure BOTH identically: the SAME `SONY_MSX_BIOS_DIR` asset root (the established
   `target_compile_definitions(... PRIVATE SONY_MSX_BIOS_DIR=...)` pattern, per DEC-0016's standing
   watch-item), the SAME cartridge (`roms/aleste.rom`, the project's own established, disclosed,
   non-identity-claiming `Generic8kB` smoke fixture — real committed asset, mirrors M19's
   precedent), `set_event_logging_enabled(true)` BEFORE `cold_boot()` on both.
3. Run BOTH through an IDENTICAL, fixed, bounded instruction count via `step_cpu_instruction()`
   (mirrors every other bounded-run precedent in this project).
4. `write_event_log()` on each to two DIFFERENT filenames; read BOTH files back from disk and
   assert **byte-for-byte identical content** — a real file-content comparison (`std::ifstream`
   read + `std::string` equality), not a same-object/same-pointer triviality.
5. **Adversarial negative control** (mirrors this project's established "prove the check actually
   discriminates" discipline — the M24 ZEXALL corruption self-check, the M10/M11/M13 comparator
   failure-mode checks): construct a THIRD machine, identical setup, but inject ONE extra/different
   `keyboard().set_key()` call partway through the run before continuing; assert its event log
   **differs** from the first two. This proves the byte-for-byte equality check is a real,
   discriminating test, not a vacuous no-op that would pass even if `write_event_log()` were
   silently broken.

**Complementary, non-`ctest` evidence-gate step (recommended, mirrors M26's own §6 "human-observed
real-launch" evidence-category split):** the developer/QA should ALSO directly launch the real,
compiled `sony_msx_headless.exe --debug-session ... --event-log run1.log` and
`... --event-log run2.log` TWICE with identical arguments (two real, separate process invocations,
not the same in-process `ctest` harness), then compare the two resulting files
(`Compare-Object`/`fc` in PowerShell, or a trivial Python byte-compare) — a genuine, real-executable
proof of the CLI wiring itself, documented in the implementation report, not claimed as a new
`ctest` case (mirrors the established ctest-vs.-manual-launch evidence split, §6).

### 2.6 CMake plan

- `CMakeLists.txt`: add `src/machine/input_script.cpp`, `src/peripherals/msx_key_names.cpp`,
  `src/frontend/psg_audio_dump.cpp` to the EXISTING, unguarded `sony_msx_core` source list (A-M27-7
  precedent) — all three remain headless-buildable.
- `tests/CMakeLists.txt`: new headless test executables registered OUTSIDE the
  `if(SONY_MSX_ENABLE_SDL3)` guard (mirrors the M26-S1/S4 precedent of registering headless-
  buildable new tests outside the guard even in an otherwise SDL3-flavored milestone); new SDL3-
  gated tests (the key-name cross-consistency check, the SDL3-side debug-session/input-script
  integration tests) registered INSIDE the existing guard block, appended after the M26 block.

---

## 3. Milestones — Slice Plan (M27-S1 … S8)

Every slice runs the full evidence-gate set (§6) and leaves the **headless**
(`-DSONY_MSX_ENABLE_SDL3=OFF`) `ctest` suite green across the entire M1-M26 suite (134 tests) at
every step.

### M27-S1 — Headless `--debug-session` skeleton (bounded CPU-driven run + asset loading + `--disk`)

- New mode in `src/main.cpp` per §2.2: BIOS asset loading, cartridge loading (reused parser), the
  new `--disk` flag (A-M27-3, mirrors A-M26-6), the bounded `step_cpu_instruction()` loop. NO
  dump/trace/log/script flags wired yet — this slice proves the loop/asset-loading skeleton alone.
- New `tests/integration/machine/hbf1xv_m27_debug_session_integration_test.cpp`: asserts a
  deterministic, hand-computable final PC/`elapsed_cycles()`/`frame_count()`-equivalent state after
  a fixed `max_steps` real-BIOS-boot run (mirrors the M13/M15/M16 boot-checkpoint precedent); a
  second case asserts `--disk`-loaded bytes are byte-readable through the FDC (mirrors A-M26-6's own
  verification action).
- **Gates:** headless build + `ctest` green (134 prior + new).

### M27-S2 — State-dump/CPU-trace CLI wiring (item 1, headless half)

- `--dump-state <name>`, `--trace-cpu <name>`, `--debug-root <path>` flags added to `--debug-session`
  per §2.2. Zero edits to `hbf1xv_machine.{h,cpp}` (both underlying methods already exist).
- New test case(s): after a fixed bounded run, assert the written `<debug_root>/traces/<name>`
  file's `[CPU]` section matches the EXACT expected register values (a deterministic oracle,
  mirrors `machine_hbf1xv_debug_dump_unit_test`'s own existing discipline) and the written CPU
  trace's step count matches `max_steps` exactly.
- **Gates:** headless `ctest` green.

### M27-S3 — Event-log CLI wiring (item 1/4, headless half) + the replay-determinism proof

- `--event-log <name>` flag added; the CRITICAL ordering fix (§2.2's `run_debug_session` sketch —
  `set_event_logging_enabled(true)` called BEFORE `cold_boot()` when the flag is present, so the
  Reset event is genuinely captured, per `set_event_logging_enabled()`'s own doc comment,
  `hbf1xv_machine.h:306-309`).
- New `tests/system/hbf1xv_m27_replay_determinism_system_test.cpp` per §2.5 (the item-4 headline
  proof, including the adversarial negative control).
- **Gates:** headless `ctest` green; the new system test is fast (bounded instruction count, not
  the slow zexall class) — confirm and document its runtime.

### M27-S4 — SDL3-side wiring for items 1 & 4 (state-dump/CPU-trace/event-log, `Sdl3App`/`sdl3_cli`)

- Additive `Sdl3AppConfig`/`ParsedSdl3Cli` fields per §2.2; flush at bounded-run exit or on quit.
- New SDL3-gated `tests/integration/frontend/sdl3_app_debug_session_integration_test.cpp`: mirrors
  S2/S3's headless assertions, under the dummy drivers, via `run_one_frame()` called a bounded
  number of times (never `run_interactive()`, per the established A-M26-8 discipline).
- A REGRESSION-GUARD case confirms every pre-existing M26 `Sdl3App`/`sdl3_cli` test remains
  unaffected when the new fields are left unset (their default, no-op state).
- **Gates:** headless unaffected; SDL3-ON `ctest` green (prerequisite: an SDL3-discoverable build,
  per A-M26-1's already-established environment finding).

### M27-S5 — Headless PSG audio-dump capability + WAV tool (item 2)

- New `src/frontend/psg_audio_dump.{h,cpp}` per §2.3 (headless-buildable, added to `sony_msx_core`).
- New `tools/audio-dump-to-wav.py` (+ optional shared `tools/wav_encode_common.py`).
- New `tests/unit/frontend/psg_audio_dump_unit_test.cpp` (headless): a known PSG register program
  (fixed tone period + volume) → a hand-computed expected square-wave amplitude sequence → assert
  the serialized dump's `[AUDIO]`/`[PCM]` content matches exactly; a round-trip assertion (encode →
  parse the folded-hex region back → byte-identical PCM recovery, mirroring `frame_dump`'s own
  round-trip test precedent).
- New `--audio-dump-demo` headless mode per §2.3 point 3; at least one real, committed WAV under
  `debug/sounds/`, referenced by path in the implementation report.
- **Gates:** headless `ctest` green (134/135 prior + new); `python tools/audio-dump-to-wav.py ...
  --self-check` hermetic pass documented as evidence (mirrors `mem-to-png.py`/`frame-to-png.py`'s
  own `--self-check` precedent).

### M27-S6 — Scripted-input automation: key-name table + script parser/player (item 3)

- New `src/peripherals/msx_key_names.{h,cpp}` per §2.4.
- New `src/machine/input_script.{h,cpp}` per §2.4.
- New `tests/unit/peripherals/msx_key_names_unit_test.cpp`: every mapped name resolves to its
  documented (row, column); an unmapped name returns `std::nullopt` (regression guard).
- New `tests/unit/machine/input_script_unit_test.cpp`: parse/serialize round-trip (deterministic
  oracle); malformed-input rejection (missing format tag, out-of-order `T`, unknown `KEY` name);
  `InputScriptPlayer::apply_due()`'s monotonic-cursor behavior against a synthetic `KeyboardMatrix`
  — every due event applied exactly once, in order, never re-applied, never skipped, even across
  multiple `apply_due()` calls with the same or a stale `current_tstate`.
- **Gates:** headless `ctest` green.

### M27-S7 — Scripted-input CLI wiring (both executables) + cross-consistency + cross-executable equivalence

- `--input-script <path>` wired into `--debug-session` (headless, §2.2's sketch) and into
  `Sdl3App::run_one_frame()`'s existing CPU sub-loop (additive `Sdl3AppConfig::input_script_path`).
- New `tests/integration/machine/hbf1xv_m27_input_script_integration_test.cpp` (headless): drives a
  real bounded run with a real script, asserts `machine.keyboard().key(row,col)` transitions at the
  EXACT expected T-state boundaries (a deterministic, hand-computed oracle).
- New SDL3-gated `tests/integration/frontend/sdl3_input_mapper_key_names_consistency_integration_test.cpp`:
  for a sampled/full set of logically-corresponding names, asserts
  `peripherals::key_name_to_row_col(name)` equals `Sdl3InputMapper::map_scancode(corresponding_scancode)`
  — the cross-consistency proof named in §2.4.
- New SDL3-gated `tests/integration/frontend/sdl3_app_input_script_integration_test.cpp`: the SAME
  script/asset-root/cartridge combination as the headless S7 test, run through `Sdl3App::
  run_one_frame()` a bounded number of times, asserting the SAME documented, hand-computed final
  keyboard-matrix state — an independent proof (two separate `ctest` cases in two separate build
  configurations, each asserting the SAME oracle) rather than a single test requiring both
  configurations compiled together, per §4 Acceptance Criterion 12's precise framing.
- **Gates:** headless unaffected by the SDL3-gated additions; SDL3-ON `ctest` green.

### M27-S8 — Dedicated system test + evidence/documentation/backlog closure

- New `tests/system/hbf1xv_m27_debug_tooling_system_test.cpp` (headless): combines
  `--debug-session`-equivalent state-dump + CPU-trace + event-log + input-script driving in ONE
  deterministic, bounded scenario (mirrors the M25/M26 "everything together" dedicated-system-test
  precedent).
- `docs/m27-implementation-report.md`: full slice-by-slice summary, including the manual
  two-process replay-determinism evidence-gate step (§2.5).
- Full 34-row deferred-backlog review written into `agent-protocol/state/deferred-backlog.md` (§3.9
  below); no row's status changes this cycle (this milestone closes no backlog row), but C5's row
  gains a factual cross-reference note (the new `--disk`/scripted-input tools now exist) without
  altering its IN-PROGRESS status.
- **Gates:** all of the above green; the FULL headless M1-M27 suite re-confirmed green one final
  time (fast subset per §6, PLUS a closure-time judgment call on the slow sweep, §6); the FULL
  SDL3-ON suite green; QA sign-off required before closure.

### 3.9 Full Deferred-Backlog Review (all 34 rows re-affirmed)

Per DEC-0005, `agent-protocol/state/deferred-backlog.md` was read in full this cycle (492 lines).
This milestone closes **zero** backlog rows (it is infrastructure/tooling scope, not a device or
presentation-layer deliverable) — every row below is re-affirmed EXACTLY as currently recorded,
with a one-line note on M27's relationship (if any).

**Section A (human-directive-tracked, 2026-07-06):**
- **B1** PSG/YM2149 internals — DONE (M15), re-affirmed unchanged. M27 adds a NEW CONSUMER (the
  audio-dump capability, §2.3) of the already-existing, unmodified `advance_cycles()`/`sample()`/
  `write_register()` — zero edit to `psg_ym2149.{h,cpp}`.
- **B2** RTC/RP5C01 internals — DONE (M15), re-affirmed unchanged; M27 touches no RTC file.
- **B3** FM-PAC/YM2413 device — DONE (M17), re-affirmed unchanged; M27 touches no `ym2413_opll.*`
  file (confirmed this cycle, §1.2 — no waveform-synthesis work added anywhere).
- **B4** MSX-JE 16 KB SRAM — DONE (M20), re-affirmed unchanged.
- **B5** Kanji font ROM I/O — DONE (M18), re-affirmed unchanged.
- **B6** Halnote/MSX-JE firmware mapping — DONE (M20), re-affirmed unchanged.
- **B7** Cartridge loading — DONE (M19), re-affirmed unchanged; M27's replay-determinism proof
  (§2.5) reuses the existing `roms/aleste.rom` fixture and `cartridge_cli` parser, unmodified.
- **B8** FDC drive mechanics — DONE (M16), re-affirmed unchanged; M27 adds the headless `--disk`
  flag (A-M27-3) using the EXISTING, unmodified `DiskImage(bytes)` constructor — zero edit to any
  FDC device file.
- **B9** VRAM/V9958 VDP contract — DONE (M14), re-affirmed unchanged; M27 touches no VDP file.

**Section B (other known deferrals):**
- **C1** Exact cycle/T-state timing parity — re-affirmed **IN-PROGRESS (M23 partial)**, unchanged;
  M27 adds zero CPU-timing-affecting code anywhere (confirmed, §1.3 A-M27-9's verification gate).
- **C2** Z80 HALT-R increment — DONE (M23), re-affirmed unchanged; M27 touches no CPU-core file.
- **C3** ZEXDOC/ZEXALL full parity sweep — DONE (M24), re-affirmed unchanged; the slow
  `hbf1xv_m24_zexall_system_test` itself is untouched this cycle (§6 addresses cadence, not
  content).
- **C4** S1985 backup-RAM `.sram` persistence — DONE (M15), re-affirmed unchanged.
- **C5** Full boot past first device read — re-affirmed **IN-PROGRESS (M16 partial)**, unchanged in
  substance. NOTE (new this cycle, factual only, no status change): M27 delivers two genuinely new
  tools a future C5 investigation did not have before — a headless `--disk` flag (A-M27-3, closing
  the "SDL3 has one, headless doesn't" gap) and the general-purpose scripted-input mechanism (§2.4)
  REQ-M27-001 explicitly named as useful for C5. M27 does NOT attempt the auto-boot-trigger
  investigation itself.
- **C6** Peripherals (keyboard/joystick) — DONE (M15), re-affirmed unchanged; M27 adds a NEW
  CONSUMER (`InputScriptPlayer`, §2.4) of the EXISTING, unmodified `set_key()`/`key()` APIs — zero
  edit to `keyboard_matrix.*`/`joystick.*`.
- **C7** Printer + cassette — DONE (M18), re-affirmed unchanged; M27 touches neither device.
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — DONE (M25), re-affirmed unchanged; M27
  touches neither `mb670836_pause.*` nor `rensha_turbo.*`.
- **C9** SDL3 frontend — DONE (M26), re-affirmed unchanged in substance; M27 makes ADDITIVE-ONLY
  extensions to `Sdl3App`/`sdl3_cli`/`Sdl3AppConfig` (§2.2/§2.4/§2.6) with every new field defaulting
  to a no-op state — every pre-existing M26 SDL3 test must remain green unchanged (a hard,
  explicitly tested regression guard, §3 M27-S4).
- **C10** FDC flux-level/DMK disk fidelity — re-affirmed OPEN; unrelated to this milestone.

**Section C (M14 VDP-depth deferrals):** D1-D7 all re-affirmed exactly as currently recorded
(D1/D2/D3/D5/D6/D7 DONE; D4 IN-PROGRESS (M23 partial)) — M27 touches no VDP file of any kind.

**Section D (M17 YM2413 depth deferrals):**
- **E1** YM2413 FM-synthesis DSP/audio depth — re-affirmed **OPEN**, unchanged in substance. NOTE
  (new this cycle, factual only): M27's item-2 audio-dump capability is explicitly, deliberately
  PSG-only (§1.2) — it establishes a "dump real device samples to file" PATTERN a future E1
  milestone could structurally mirror once/if a YM2413 `sample()` method is ever grounded and built,
  but M27 adds zero waveform/DSP-shaped code to `ym2413_opll.*` or anywhere else (confirmed via
  `grep -n "sample" src/devices/audio/ym2413_opll.h` returning nothing, this cycle).
- **E2** YM2413 register-write timing constraint — re-affirmed OPEN; unrelated to this milestone.

**Section E (M18 printer/cassette depth deferrals):** F1/F2 both re-affirmed OPEN, unchanged;
unrelated to this milestone (no printer/cassette scripting or capture is in scope).

**Section F (M19 cartridge-mapper depth deferrals):** G1-G4 all re-affirmed OPEN, unchanged;
unrelated to this milestone.

No new backlog rows are proposed this cycle. No row's status changes.

---

## 4. Acceptance Criteria

1. **Item 1 is genuinely wired on BOTH executables**: `--dump-state`, `--trace-cpu`, `--event-log`
   are real, working CLI flags on `sony_msx_headless` (via the new `--debug-session` mode) and on
   `sony_msx_sdl3` (via additive `Sdl3AppConfig`/`sdl3_cli` fields); each produces the documented,
   deterministic output file, verified by direct content assertions against a hand-computed oracle
   (not merely "the file exists").
2. **Zero new machine-level method was needed or added for item 1** (`write_state_dump()`/
   `write_cpu_trace()`/`write_event_log()` already existed) — `git diff v1.0.26 --stat --
   src/machine/hbf1xv_machine.h src/machine/hbf1xv_machine.cpp` shows either NO change, or ONLY the
   minimal, disclosed change (if any) the developer judges genuinely necessary; any such change
   must be explicitly justified in the implementation report.
3. **Item 2 delivers a real, decoded, headless-buildable PSG audio-capture capability** genuinely
   reusing `PsgAudioPump`/`PsgYm2149` (grep-verified: no parallel, duplicate synthesis logic added
   anywhere), a NEW `tools/audio-dump-to-wav.py` (not an extension of `tools/mem-to-audio.py`'s own
   responsibilities — confirmed via `git diff v1.0.26 -- tools/mem-to-audio.py` showing either no
   change or ONLY the disclosed, zero-behavior-change `encode_wav()`-extraction refactor, §2.3),
   and at least one real, committed, audibly-non-trivial (non-silent, deterministically
   hand-computable) example WAV under `debug/sounds/`, referenced by path in the implementation
   report.
4. **Item 3 delivers a genuine, deterministic, timed key-event scripting mechanism** usable
   identically from both executables: a documented text format, a parser that rejects malformed
   input (never silently returns a partial/garbage result), a monotonic-cursor player proven to
   apply every due event exactly once, and a CLI flag on both executables.
5. **Item 4 delivers a real, adversarially-validated, byte-for-byte replay-determinism proof**: two
   independently constructed `Hbf1xvMachine` instances, identical configuration, produce
   byte-identical event-log files after an identical bounded run; a deliberately-diverged third run
   produces a DIFFERENT file, proving the check genuinely discriminates (not a vacuous no-op).
6. **The `Sdl3InputMapper::scancode_map()`-vs-`msx_key_names` cross-consistency claim is
   independently tested**, not merely asserted (§3 M27-S7) — every logically-corresponding key name
   resolves to the identical (row, column) in both tables.
7. **The "same script drives both executables to the same final state" claim is independently
   tested in BOTH build configurations** against the SAME documented, hand-computed oracle (§3
   M27-S7) — not merely asserted in prose.
8. **Zero regression on the headless (`-DSONY_MSX_ENABLE_SDL3=OFF`) configuration**: the full
   M1-M26 suite (134 tests) plus all new M27 tests pass 100%, independently reproduced by the
   developer and QA from clean rebuilds; `git diff v1.0.26` confirms NO change to any
   `src/devices/cpu/` or `src/core/` file, and that any change under `src/devices/` or
   `src/peripherals/` is confined to wholly NEW, additive files (zero modified-existing-file
   content) — per A-M27-9's precise verification gate.
9. **`ctest` runs entirely headlessly under `-DSONY_MSX_ENABLE_SDL3=ON` too** — every new SDL3-
   gated test succeeds under `SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`, with zero
   `SDL_Delay`-based pacing anywhere in the `ctest` execution path (mirrors A-M26-8 exactly).
10. **Every pre-existing M26 `Sdl3App`/`sdl3_cli`/`Sdl3AudioPresenter`/`Sdl3VideoPresenter`/
    `Sdl3InputMapper` test remains green, unmodified in its own assertions**, proving the new,
    additive `Sdl3AppConfig` fields are genuinely no-op when unset.
11. **The dedicated system test** (M27-S8) exercises state-dump + CPU-trace + event-log +
    input-script driving together in one deterministic, bounded scenario.
12. **Full 34-row deferred-backlog review** completed and written into `deferred-backlog.md` (§3.9)
    — zero rows change status this cycle; C5's row gains a factual, non-status-changing
    cross-reference note only.
13. **The evidence-gate cadence discipline (§6) is followed and its `git diff` verification actions
    are actually executed and their output captured** — not assumed or silently skipped.
14. **Evidence gates executed and captured**: `tools/validate-assets.ps1`;
    `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; `cmake --build build --config
    Debug` (headless AND SDL3-ON, each documented separately); `ctest --test-dir build -C Debug
    --output-on-failure -LE m24_slow_full_sweep` (the default fast-subset cadence, both
    configurations, per §6); `python tools/audio-dump-to-wav.py ... --self-check`.
15. **No out-of-scope item from §1.2 is added** (no vague stress-testing/cross-platform-validation
    scope, no C5 investigation attempt, no YM2413 synthesis work, no numeric-keypad key-name
    coverage beyond the disclosed M26-inherited boundary).

---

## 5. Risks (each with a verification action)

- **R-M27-1 (the new `--debug-session` headless mode is genuinely new CPU-driving surface in
  `sony_msx_headless` — the FIRST time this executable's own code, not just a test, drives the CPU
  in a bounded, realistic BIOS-boot loop).** *Verification:* mirrors the ALREADY-established
  `run_bios_boot_trace`/`run_parity_trace` loop shape exactly (§2.2) — zero new CPU-stepping
  semantics invented; the developer confirms the new mode is a wholly ADDITIVE new `if` branch,
  never touching the pre-existing default run path's own behavior (`git diff` on the default path's
  own code block must be empty).
- **R-M27-2 (the event-logging-must-be-enabled-before-`cold_boot()` ordering requirement is a real,
  easy-to-get-wrong sequencing constraint — silently missing it would not fail loudly, it would just
  silently omit the Reset event from the captured log, undermining item 4's own replay-determinism
  proof if it silently diverged between the two runs being compared).** *Verification:* the
  developer writes a DEDICATED test asserting the Reset event IS present at sequence 0 of a captured
  log when `--event-log` is combined with a fresh `cold_boot()`-driven session (both executables);
  QA specifically checks this ordering in both `src/main.cpp`'s new mode and `Sdl3App::init()`.
- **R-M27-3 (the audio-dump PCM-scaling formula, §2.3, is a first-principles, disclosed modeling
  choice — an incorrect or undisclosed formula could silently misrepresent the real PSG output,
  undermining item 2's "real, decoded" claim).** *Verification:* the formula must be documented in a
  code comment citing A-M27-4's grounded `[0,62]` range; the unit test (M27-S5) asserts against a
  HAND-COMPUTED expected sequence derived from the SAME documented formula, not merely "some
  non-zero output."
- **R-M27-4 (the `msx_key_names` table risks silently diverging from `Sdl3InputMapper::
  scancode_map()`'s own already-tested row/column assignments if hand-typed independently rather
  than genuinely cross-checked — a real regression-introduction risk for anyone who later relies on
  BOTH tables agreeing).** *Verification:* the M27-S7 cross-consistency test (§2.4/§3) is a HARD
  gate, not optional; the developer must derive every table entry by directly reading
  `sdl3_input_mapper.cpp`'s actual array literal, never by independently re-deriving from the
  fact-sheet or memory.
- **R-M27-5 (touching `Sdl3App`/`Sdl3AppConfig`/`sdl3_cli.{h,cpp}` — all M26-authored, already
  QA-passed files — for items 1 & 3's SDL3-side wiring carries real regression risk to the
  cleanest-ever QA cycle's own surface).** *Verification:* every new field is additive with a
  no-op default (§2.2/§2.4); M27-S4's regression-guard test is a HARD, explicitly named gate (§4
  Acceptance Criterion 10), not merely implied by "existing tests still pass."
- **R-M27-6 (the git-diff-based slow-test-skip verification, per the human's
  `feedback_slow_test_cadence.md` directive, must be interpreted PRECISELY — new, additive files
  under `src/peripherals/`/`src/devices/`/`src/frontend/` are EXPECTED and FINE this cycle; the
  check's real purpose is confirming zero edits to EXISTING timing-sensitive files, not forbidding
  any new file under those path prefixes ever again — a naive, literal reading of "empty diff under
  src/peripherals/" would be FALSE this cycle by design, since `msx_key_names.*` is a genuinely new
  file there).** *Verification (§6 elaborates further):* run `git diff v1.0.26 --stat --
  src/devices/cpu/ src/core/` (must be EMPTY — the hard, zero-tolerance part) AND `git diff v1.0.26
  --stat -- src/devices/ src/peripherals/` (must show ONLY newly-added files, zero modified-line
  counts against any pre-existing file) at every gate where the slow test is being skipped;
  document both outputs, not just a pass/fail conclusion.
- **R-M27-7 (scope-creep risk: the four items' natural connections to C5/E1 create a real
  temptation to attempt "just a little" of either — explicitly disallowed, §1.2).** *Verification:*
  §1.2's explicit out-of-scope table is a hard boundary; the developer does not attempt the C5
  auto-boot-trigger investigation or any YM2413 synthesis work this cycle, regardless of how close
  the new tooling brings either.
- **R-M27-8 (the replay-determinism proof's own construction risk: if the "two independent machine
  instances" are accidentally not genuinely independent — e.g. sharing static/global state
  somewhere — the proof could pass vacuously even with a real, undetected non-determinism bug
  elsewhere in the codebase).** *Verification:* the adversarial negative control (§2.5 point 5) is
  the hard mitigation — if it fails to produce a genuine divergence, that is itself evidence the
  comparison technique is broken and must be fixed before the milestone can claim item 4 complete.

---

## 6. Developer Handoff

**Build order:** M27-S1 (headless `--debug-session` skeleton — do this FIRST, since S2/S3/S6/S7 all
depend on it) → M27-S2 (state-dump/CPU-trace, headless) → M27-S3 (event-log + the replay-
determinism proof, headless — the item-4 headline claim, worth landing early while the loop's
exact semantics are freshest) → M27-S4 (SDL3-side wiring for items 1/4 — run the FULL M26 SDL3-ON
regression suite immediately after, before proceeding, mirroring the M26-S1 "verify the
highest-blast-radius touch point immediately" discipline) → M27-S5 (audio-dump, independent of
S2-S4, may proceed in parallel if useful) → M27-S6 (key-name table + script parser/player) →
M27-S7 (scripted-input CLI wiring + the two cross-consistency/cross-executable tests) → M27-S8
(dedicated system test + evidence + backlog closure).

**Hard constraints (do not deviate without a `decisions.md` entry):**
- Do NOT touch `src/devices/cpu/*` or `src/core/*` — confirmed via `git diff v1.0.26` at every gate
  (R-M27-6's precise interpretation).
- Do NOT edit any PRE-EXISTING file under `src/devices/` or `src/peripherals/` — every touch to
  those directories this cycle must be a wholly NEW, additive file (A-M27-9).
- Do NOT add any waveform/DSP synthesis for YM2413 anywhere (mirrors R-M26-5 exactly, still in
  force).
- Do NOT attempt the C5 auto-boot-trigger investigation this cycle (§1.2/R-M27-7).
- Do NOT use `SDL_Delay`/real-time pacing anywhere inside `ctest`'s own execution path for any new
  SDL3-gated test (mirrors A-M26-8, still in force).
- Do NOT extend `tools/mem-to-audio.py`'s own responsibilities (parsing new dump formats, PSG-aware
  decoding) — build the new capability as a genuinely separate tool (§2.3); a pure, zero-behavior-
  change `encode_wav()`-extraction refactor into a shared module is the ONLY acceptable touch to
  that file, and only if the developer judges the shared-module design worth the coupling.
- Do NOT run the slow `hbf1xv_m24_zexall_system_test` at every gate by default (see the cadence
  discipline below) — the DEFAULT evidence-gate command throughout this milestone is:
  ```powershell
  ctest --test-dir build -C Debug --output-on-failure -LE m24_slow_full_sweep
  ```
  Before EVERY gate where the slow test is skipped, run and record:
  ```powershell
  git diff v1.0.26 --name-only -- src/devices/ src/peripherals/ src/core/
  ```
  and confirm (per R-M27-6's precise interpretation) that any listed path is either (a) a file that
  did not exist at `v1.0.26` (a genuinely new, additive file this milestone adds) or (b) absent
  entirely. If EVERY listed path satisfies (a) or the list is empty, the fast subset is sufficient
  and the slow test does not need to run at that gate. Given every one of items 1-4's designs above
  is machine-introspection/frontend/peripheral-adjacent (never an edit to `Z80aCpu`, `Scheduler`, or
  any pre-existing device's clock-consuming logic), the slow test is NOT expected to be needed
  anywhere in this milestone — but this expectation must be CONFIRMED via the exact command above at
  each relevant gate, never silently assumed. A full, unfiltered `ctest` run (including the slow
  test) at the final closure gate is a coordinator/QA judgment call, not a hard per-slice
  requirement, GIVEN the git-diff confirmation holds throughout — this is the coordinator's/QA's
  call to make at closure time, not bound in advance by this planner package.

**Files to create (indicative, developer's judgment on exact shape per `src/CLAUDE.md`):**
`src/peripherals/msx_key_names.h`/`.cpp`, `src/machine/input_script.h`/`.cpp`,
`src/frontend/psg_audio_dump.h`/`.cpp`, `tools/audio-dump-to-wav.py` (+ optional
`tools/wav_encode_common.py`), `tests/integration/machine/
hbf1xv_m27_debug_session_integration_test.cpp`, `tests/integration/machine/
hbf1xv_m27_input_script_integration_test.cpp`, `tests/system/
hbf1xv_m27_replay_determinism_system_test.cpp`, `tests/system/hbf1xv_m27_debug_tooling_system_test.cpp`,
`tests/unit/peripherals/msx_key_names_unit_test.cpp`, `tests/unit/machine/input_script_unit_test.cpp`,
`tests/unit/frontend/psg_audio_dump_unit_test.cpp`, `tests/integration/frontend/
sdl3_app_debug_session_integration_test.cpp`, `tests/integration/frontend/
sdl3_input_mapper_key_names_consistency_integration_test.cpp`, `tests/integration/frontend/
sdl3_app_input_script_integration_test.cpp`, at least one committed WAV under `debug/sounds/`,
`docs/m27-implementation-report.md`.

**Files to edit (additive only):** `src/main.cpp` (new `--debug-session`/`--audio-dump-demo` modes
— new `if` branches only, zero change to the pre-existing default path or any other existing mode),
`src/frontend/sdl3_app.h`/`.cpp` (additive `Sdl3AppConfig` fields, additive flush logic),
`src/frontend/sdl3_cli.h`/`.cpp` (additive `ParsedSdl3Cli` fields), `CMakeLists.txt` (new source
files added to the unguarded `sony_msx_core` list), `tests/CMakeLists.txt` (new test registrations,
headless ones outside the SDL3 guard, SDL3-gated ones inside it), possibly `tools/mem-to-audio.py`
(the OPTIONAL, zero-behavior-change `encode_wav()`-extraction refactor only),
`agent-protocol/state/deferred-backlog.md` (§3.9's factual, non-status-changing C5 cross-reference
note), `agent-protocol/state/milestones.md`, `agent-protocol/state/definition-of-done.yaml`,
`agent-protocol/state/current-phase.md`.

**Evidence to capture before requesting QA:** full headless `ctest` output (fast subset by default,
per the cadence discipline above; 134 prior + all new M27 tests, 0 failed); full SDL3-ON `ctest`
output (fast subset, all new SDL3-gated tests, 0 failed, explicitly confirmed run under
`SDL_VIDEO_DRIVER=dummy`/`SDL_AUDIO_DRIVER=dummy`); the `git diff v1.0.26` outputs named in the
cadence discipline above, captured verbatim (not summarized as "empty, trust me"); the committed
example WAV's path and the `tools/audio-dump-to-wav.py --self-check` output; the committed input
script example (if the developer creates one for the demo/system test) and its parse/round-trip
evidence; the replay-determinism proof's actual byte-comparison output (both the `ctest` case AND
the recommended manual two-process launch, §2.5); the manual `sony_msx_headless.exe
--debug-session ...` and (if wired) `sony_msx_sdl3.exe ...` real-launch evidence (mirrors M26's own
"launch the real executable, don't just trust ctest" discipline); the four standard asset/build/test
evidence-gate script outputs for BOTH build configurations.

**Evidence-category split (mirrors M26 §6's precedent):**

**`ctest`-gated (automated, reproducible, required before QA sign-off):** debug-session bounded-run
determinism; state-dump/CPU-trace content correctness; event-log Reset-event-present-ness and the
replay-determinism byte-comparison + adversarial negative control; audio-dump numeric/format
correctness; scripted-input parse/round-trip/monotonic-application correctness;
key-name-vs-scancode cross-consistency; cross-executable same-script-same-final-state equivalence
(two independent `ctest` cases, one per configuration); the dedicated system test.

**Human-observed / directly-launched-executable only (NOT `ctest`-gated):** does the committed
example WAV actually sound like a plausible PSG tone when played on real speakers (mirrors M26's own
disclosed "does real audio actually play and sound plausible" human-observed category exactly — this
project has never claimed to `ctest`-verify subjective audio quality, and M27 does not start now);
the manual two-process replay-determinism launch (§2.5) is a real, directly-executed piece of
evidence but is documented as a manual verification step, not fabricated as a `ctest` case.
