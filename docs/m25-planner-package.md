# M25 Planner Package — Sony Speed-Controller + Hardware PAUSE + Ren-Sha Turbo (closes C8)

- Milestone ID: M25
- Title: Sony Speed-Controller (MB670836) + Hardware PAUSE + Ren-Sha Turbo Autofire
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M25-001 (kickoff, planner-first, no production code;
  `agent-protocol/channels/requests.md:1241-1249`)
- Decisions in force: DEC-0005 (backlog disposition discipline — every planner package restates all
  34 rows), the human's 2026-07-08 "zero license-sensitive future work" standing directive
  (`agent-protocol/state/current-phase.md:3`, `feedback_license_sensitive_scope.md` — never
  reproduce openMSX's own *large* data tables under `src/`, even "independently re-derived"; small
  discrete facts/constants remain the established, accepted pattern), the M24-M25 continuation
  directive (planner → developer → QA → release decision/tag, proceeding without an extra pause on
  a clean QA PASS, STOP-and-consult otherwise — this condition already fired once this run for M24,
  DEC-0022).
- Backlog effect this cycle: **C8 (Sony speed-controller + hardware PAUSE (MB670836); Ren-Sha
  Turbo autofire) → target: closes IN FULL**, subject to the precise, non-negotiable A/B
  disposition rule in §4 Acceptance Criterion 9 (PAUSE/Speed-Controller A/B is honestly BLOCKED —
  no comparable reference engine behavior exists; this does not block closure, exactly mirroring
  the C3/M24 disk-boot-A/B and M21 computed-pixel-color precedents of an honestly-disclosed BLOCKED
  sub-claim not gating an otherwise-complete milestone). No other backlog row is touched. All 34
  rows re-affirmed in §3.5.
- Gate: normal human-release-decision gate; per the M24-M25 continuation directive the coordinator
  is pre-authorized to proceed through the release-decision/tag step without an additional pause,
  UNLESS QA does not reach a clean PASS (real blocker → stop and consult the human).

> Grounding rule: every claim below about the Sony hardware was independently verified this cycle
> by directly reading `references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md` §9 (full file
> read) and `references/fact-sheets/Zilog Z80A CPU.md` §6 (full file read), plus a targeted,
> evidence-based search of `references/openmsx-21.0/` for ANY Sony-specific PAUSE/speed-controller
> modeling (§2.2 documents exactly what was found and what was honestly NOT found, with concrete
> file:line citations — nothing is asserted about a reference file's contents without having read
> it). `src/machine/hbf1xv_machine.{h,cpp}` was read in full to ground the architecture decision in
> §2.3 against the actual, already-working `run_frame()`/`step_cpu_instruction()`/vsync design,
> per the task's explicit instruction to do so before proposing a design.

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) Hardware PAUSE** — a machine-level CPU-execution gate driven by a physical PAUSE button,
  modeling the Sony **MB670836** companion chip's documented behavior: it "physically halts the
  CPU and cannot be bypassed in software" (S1985 fact-sheet §9). Genuinely distinct from the Z80's
  own `HALT` instruction (CPU-internal state, already modeled M9/M23/C2) — see §2.3/§2.4 for the
  full architectural differentiation.
- **(b) Speed Controller** (slow-motion slider) — per the Z80A fact-sheet §6 item 3 (quoted
  verbatim below), NOT a clock-speed change; implemented as an automatic, VBlank-synced generator
  that periodically engages the SAME hardware-PAUSE gate as (a), producing an intermittent-pause
  slow-motion effect.
- **(c) Ren-Sha Turbo** — a separate, simpler autofire feature (rapid button-press synthesis) on
  the space bar and joystick trigger-A of both ports, grounded concretely in real openMSX behavior
  + real per-machine HB-F1XV configuration data (§2.2/§2.5) — unlike (a)/(b), this one HAS a
  genuine, usable, cross-checkable reference implementation.
- **(d) Deterministic unit/integration tests** for both new classes in isolation, plus integration
  tests wiring them into `KeyboardMatrix`/`JoystickPorts`/`Hbf1xvMachine` with explicit
  regression-guard assertions (default/unattached state byte-for-byte unchanged from pre-M25).
- **(e) A dedicated system integration test** exercising all three features together against a real
  stepped CPU program (per the milestone kickoff's explicit requirement for "a dedicated system
  integration test").
- **(f) openMSX A/B evidence**, explicitly bounded per the honest feasibility analysis in §2.7 — a
  genuine, feasible technique exists for Ren-Sha Turbo; PAUSE/Speed-Controller is explicitly,
  honestly marked BLOCKED (no comparable reference engine behavior exists to diff against — this
  is real first-principles design work, not a shortcut).
- **(g) Full deferred-backlog review** — all 34 rows re-affirmed (§3.5).
- **(h) Zero regression** across the FULL M1-M24 suite (124 tests, noting
  `hbf1xv_m24_zexall_system_test` alone takes ~24-27 minutes).

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M25 | Owning milestone (candidate) |
|---|---|---|
| **A CLI flag / SDL3 input binding for the PAUSE button, Speed Controller slider, or Ren-Sha Turbo slider** | No frontend exists yet (backlog C9, still OPEN) and no other human-input peripheral in this project has CLI wiring today — `KeyboardMatrix::set_key()`/`JoystickPorts::set_port()` are pure API surfaces exercised only by tests, exactly like the two new classes this milestone adds. Adding a CLI surface ahead of the frontend would be premature scope. | C9 (SDL3 frontend) — input mapping is explicitly named in its own scope. |
| **Modeling the MB670836's DRAM address-multiplexing function** | The fact-sheet (§9, §4) names this as the chip's OTHER function, alongside PAUSE/speed-controller. DRAM refresh/multiplexing is already treated project-wide as "real but transparent to software" (only the Z80 `R` register value is modeled, not actual refresh cycles — Z80A fact-sheet §7 "Contended memory — NOT applicable"; §7 "the refresh function itself is real ... but transparent to software, you only need to model the R register value"). Modeling DRAM muxing would add zero observable behavior. | N/A — permanently out of scope, matches the project-wide refresh-transparency precedent. |
| **A literal PAUSE key on the `KeyboardMatrix` row/column grid** | §2.4 argues this explicitly: the fact-sheet's own framing — "physically halts the CPU and **cannot be bypassed in software**" — is incompatible with PAUSE being an ordinary matrix key (software CAN always ignore/mask a matrix key read). PAUSE is modeled as a dedicated machine-level API call, not a matrix cell. | N/A — a deliberate, grounded design decision, not a deferral. |
| **Numeric audio/DSP synthesis for anything this milestone touches** | Neither PAUSE, Speed Controller, nor Ren-Sha Turbo produce audio; this is unrelated to backlog E1 (YM2413 DSP depth). | N/A |
| **Any change to `src/devices/cpu/*`** | The Z80 core itself is untouched — hardware PAUSE is modeled entirely at the machine-composition level (§2.3/§2.4), never inside `Z80aCpu`. This is a hard, explicit constraint protecting the M9/M12/M23 zero-tolerance CPU-timing oracles. | N/A |
| **Full-suite live-Tcl or any wall-clock-based A/B technique for the Speed Controller's toggle cadence** | Per M23's own hard-won finding (`docs/m23-parity-trace-diff.md`), live openMSX Tcl-session timing diverges from this project's deterministic engine once meaningful wall-clock time elapses — and this sub-claim is doubly moot since openMSX does not even model the Speed Controller (§2.2). | N/A — structurally infeasible, not merely undesirable. |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M25-1 (PAUSE button press-toggle semantics — a first-principles design choice, NOT a Sony
  hardware fact).** Neither fact-sheet states whether the physical PAUSE button is a momentary
  (pause-while-held) or toggle (press-once-to-pause, press-again-to-resume) switch. The closest
  openMSX precedent, `MSXTurboRPause` (`references/openmsx-21.0/src/MSXTurboRPause.cc:20`,
  read this cycle), is EXPLICITLY a different chipset/machine (MSX turboR's S1990, not the Sony
  S1985/MB670836) and uses toggle (flip-flop) semantics: *"Whenever the pause key is pressed a
  flip-flop is toggled"* — cited for design-inspiration only, not as grounding evidence for the
  Sony chip's actual circuit. This package adopts toggle semantics (`press_pause_button()` flips
  an internal `engaged_` bool) because (a) it matches common 1980s-consumer-hardware PAUSE-button
  UX, (b) it is the only architecturally-analogous behavior this project's own reference material
  documents anywhere, and (c) it produces clean, deterministic, easily-testable semantics. *Verify:*
  if a genuine Sony MB670836/HB-F1XV service-manual or community-measured description of the real
  button's electrical behavior ever becomes available, re-verify this choice.
- **A-M25-2 (hardware PAUSE freezes the CPU with ZERO R-register/PC/state progress — grounded in
  the fact-sheet's own words, not assumed).** S1985 fact-sheet §9: *"The hardware PAUSE physically
  halts the CPU and cannot be bypassed in software."* This is read literally: no M1/opcode-fetch
  cycle occurs at all while engaged, so `PC`, `R`, and every register are frozen — a stronger,
  externally-imposed condition than the Z80's own `HALT` instruction (§2.4 explains the
  differentiation in full, including why `R` behaves oppositely in the two cases). *Verify:*
  the developer's integration test explicitly asserts byte-for-byte register/PC/R stasis across
  multiple paused `step_cpu_instruction()` calls (§4 Acceptance Criterion 2).
  Verify:* the developer's integration test explicitly asserts byte-for-byte register/PC/R stasis
  across multiple paused `step_cpu_instruction()` calls (§4 Acceptance Criterion 2).
- **A-M25-3 (Speed-Controller numeric duty-cycle range — a first-principles design default, NOT a
  hardware fact, unlike Ren-Sha Turbo's grounded 47/221 defaults — §A-M25-5).** No datasheet or
  community measurement of the actual Sony slider's numeric pause-duty-cycle range was found in
  either fact-sheet or `references/openmsx-21.0/`. §2.4 proposes a documented default
  (`kPeriodFrames = 8`, levels `0..7`) as a clean, testable, first-principles design choice. *Verify:*
  if real hardware measurement of the Speed-Controller slider's actual slow-motion ratio ever
  becomes available, re-verify/re-calibrate this constant; until then it is explicitly labeled a
  design default in code comments, never asserted as a hardware fact.
- **A-M25-4 (the Speed-Controller autofire and the manual PAUSE button share ONE observable CPU
  gate — grounded in the fact-sheet's own phrasing).** S1985 fact-sheet §9 / Z80A fact-sheet §6
  item 3: the Speed Controller is *"implemented as an autofire ON THE PAUSE BUTTON synced to
  BLANK"* (emphasis added) — i.e. the slider automatically, repeatedly drives the SAME underlying
  PAUSE mechanism the manual button drives, rather than being a wholly independent second circuit.
  This package therefore models both as two independent trigger SOURCES that logically OR into one
  combined `cpu_should_pause()` gate (§2.4) — behaviorally identical to "one shared line, two
  drivers" from the CPU's observable perspective, and simpler to implement/test than modeling an
  undocumented internal Sony circuit topology we have no datasheet for. *Verify:* no action needed
  — this is an explanatory design-equivalence argument, not a load-bearing numeric claim.
- **A-M25-5 (Ren-Sha Turbo's real per-machine calibration IS a genuine, citable hardware fact — TWO
  scalar integers, not a "large data table," so citing them is safe under the standing
  license-sensitivity directive).** `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml:16-19`
  (read this cycle):
  ```
  <RenShaTurbo>
    <min_ints>47</min_ints>
    <max_ints>221</max_ints>
  </RenShaTurbo>
  ```
  with the file's own header comment attributing the machine definition to a specific real,
  serial-numbered HB-F1XV unit (`Sony_HB-F1XV.xml:14`: *"serialnumber Meits's machine: 225891"*).
  This is two small, real, per-machine-measured scalar constants (analogous to how this project
  has already cited SHA1 hashes, port numbers, wait-state counts, and ROM sizes from openMSX
  throughout M11-M24 without issue) — categorically different from the ~340-entry VDP access-slot
  timing TABLE the standing memory note explicitly warns against (`VDPAccessSlots.cc`). *Verify:*
  QA should independently confirm this file:line citation and independently judge whether it
  crosses the "large data table" line (this planner's judgment: it does not — two scalars, not an
  array).
- **A-M25-6 (Ren-Sha Turbo's toggle-frequency FORMULA is independently derived from the config
  data's documented MEANING, not transcribed from openMSX's own code).** openMSX's own doc comment
  (`references/openmsx-21.0/src/Autofire.hh:66-76`, read this cycle) explains what `min_ints`/
  `max_ints` MEAN: *"Number of interrupts... for 50 periods, measured in ntsc mode (which gives 60
  interrupts per second)."* §2.5 independently re-derives a frequency formula from this documented
  MEANING (50 on/off periods occur over `ints` VBlank interrupts, i.e. `ints/60` seconds, so
  `freq_hz = 3000/ints`) using this project's own already-established `kSystemClockHz = 3579545`
  constant (already independently declared per-file in `wd2793.h`, `disk_drive.h`, `rp5c01.h`) —
  NOT openMSX's own `setClock()` code shape (`references/openmsx-21.0/src/Autofire.cc:79-87`,
  `(2 * 50 * 60) / (max_ints - ...)`), which is a different (but dimensionally equivalent)
  expression this package deliberately does not transcribe. *Verify:* the developer independently
  re-derives and sanity-checks this formula at implementation time (a two-line dimensional-analysis
  check is sufficient) before relying on it.
- **A-M25-7 (Ren-Sha Turbo's OR-into-read wiring points are read directly from openMSX's own
  behavior, not guessed).** `references/openmsx-21.0/src/MSXPPI.cc:90-93` (read this cycle):
  ```cpp
  auto row = keyboard.getKeys()[selectedRow];
  if (selectedRow == 8) {
      row |= renshaTurbo.getSignal(time) ? 1 : 0;
  }
  ```
  and `references/openmsx-21.0/src/sound/MSXPSG.cc:90-93` (read this cycle):
  ```cpp
  byte joystick = ports[selectedPort]->read(time) |
                  ((renShaTurbo.getSignal(time)) ? 0x10 : 0x00);
  ```
  This independently confirms: (a) keyboard row **8, bit 0** = SPACE (matching the standard MSX
  keyboard-matrix chart the S1985 fact-sheet itself cites as a primary source, "MSX Assembly Page
  (Grauw) ... Keyboard matrices"); (b) PSG **R14 bit 4** = joystick trigger A; (c) the combine
  operation is bitwise-OR, applied AFTER the normal (possibly-pressed) row/port read, meaning the
  autofire signal can only ever force a bit from 0→1 (i.e. force a periodic RELEASE), never force
  a 0 (a press) — a critical correctness invariant §2.5/§4 make an explicit, dedicated test
  requirement (R-M25-6). *Verify:* the developer independently re-reads both cited call sites at
  implementation time before finalizing the OR-mask wiring.
- **A-M25-8 (openMSX genuinely does not model the Sony PAUSE/Speed-Controller — confirmed by
  direct, exhaustive search, not inferred from absence of a quick grep hit).** §2.2 documents the
  full search methodology and findings. *Verify:* re-confirm via a fresh search if this package is
  ever revisited against a newer openMSX version.

---

## 2. Spec Summary

### 2.1 Grounding facts (verbatim, re-read this cycle — not trusted from the kickoff summary)

**S1985 fact-sheet §9** (`references/fact-sheets/Yamaha S1985 MSX-ENGINE Chipset.md:173-179`):
> The **Sony HB-F1XV**... hardware PAUSE, speed-controller, Ren-Sha Turbo autofire.
> **Sony-specific companion chip:** a second Sony custom LSI (**MB670836**) sits beside the S1985
> and handles DRAM address multiplexing plus the speed-controller (slow-motion) and
> hardware-PAUSE circuitry; the earlier HB-F1II used a similar chip. The hardware PAUSE physically
> halts the CPU and cannot be bypassed in software.

**Zilog Z80A fact-sheet §6** (`references/fact-sheets/Zilog Z80A CPU.md:213-216`), the
"MSX-specific wait states" subsection, item 3:
> 3. The HB-F1XV has **no CPU turbo mode** (unlike the Panasonic FS-A1WX/WSX which can switch the
>    Z80 to ~5.37 MHz via an I/O port). The "Speed Controller" slider is *not* a clock change — it
>    is implemented as an autofire on the PAUSE button synced to BLANK, slowing games by pausing
>    them intermittently.

Both independently confirm the kickoff summary's characterization; nothing was found to contradict
it. Note the Zilog Z80A fact-sheet's own §7 (`Zilog Z80A CPU.md:246-249`) attributes the
memory-mapper-address-expansion function to a chip it names **MB64H444**, while the S1985
fact-sheet §9/§4 names the DRAM-mux/PAUSE/speed-controller chip **MB670836** — two DIFFERENT part
numbers for two different Sony companion functions (mapper-address-expansion vs.
DRAM-mux+PAUSE+speed-controller). This planner package only implements the MB670836 PAUSE/
speed-controller function; the MB64H444 mapper-address-expansion function is already fully covered
by the existing M11/M13 `MapperIo`/memory-mapper work (unrelated to this milestone).

### 2.2 openMSX grounding search — full, honest findings (not a quick guess)

Search performed this cycle across `references/openmsx-21.0/` for `PAUSE`, `RenSha`/`autofire`,
and `MB670836`/`speed.?controller` (case-insensitive), then every hit was read directly.

**Ren-Sha Turbo: genuinely, fully modeled.**
- `references/openmsx-21.0/src/RenShaTurbo.{hh,cc}` — a small wrapper owning one `Autofire`
  instance, config-driven from a `<RenShaTurbo>` XML block (`min_ints`/`max_ints`, defaults 47/221
  if absent).
- `references/openmsx-21.0/src/Autofire.{hh,cc}` — the actual signal generator: a `DynamicClock`
  running at a speed-derived frequency; `getSignal(time)` returns whether the elapsed tick count
  is odd (a square wave); speed 0 = disabled (special-cased, not "very slow").
- Wired into `MSXPPI.cc:90-93` (keyboard row 8 bit 0) and `sound/MSXPSG.cc:90-93` (PSG R14 bit 4)
  — full call-sites quoted in A-M25-7 above.
- **`Sony_HB-F1XV.xml:16-19`** carries a real, machine-specific `<RenShaTurbo>` block with the
  47/221 defaults, attributed to a real serial-numbered unit (A-M25-5).

**Hardware PAUSE / Speed Controller: genuinely, demonstrably NOT modeled for this chip/machine
family — checked, not assumed.**
- `references/openmsx-21.0/src/SG1000Pause.hh` — Sega SG-1000/SC-3000 "hold"/"pause"/"reset"
  button, **triggers an NMI**. Different machine family entirely (not MSX), different mechanism
  (NMI, not a CPU-halting WAIT-line gate).
- `references/openmsx-21.0/src/MSXTurboRPause.{hh,cc}` — MSX **turboR** (S1990 chipset) pause key,
  a **flip-flop status bit at I/O port `0xA7`**, which — critically — is implemented by calling
  `getMotherBoard().pause()`/`unpause()` (`MSXTurboRPause.cc:66-69`), i.e. openMSX's own
  **whole-session engine pause** (stops the entire emulation loop), not a CPU-execution gate at
  all. This is architecturally INCOMPATIBLE with this project's atomic, deterministic
  per-instruction `step_cpu_instruction()` stepping model (there is no "pause the whole host
  session" concept here, nor should there be — see §2.3). Also a different chipset (S1990, not
  S1985/MB670836) and a different machine family (turboR, not the HB-F1XV's plain MSX2+).
  Cited for design-inspiration on the PAUSE-button toggle semantics only (A-M25-1), not for any
  numeric or mechanistic grounding.
- **None of the six real Sony MSX machine XML definitions wire a Pause or SpeedController
  device**, and **four of the six explicitly say so in their own `<description>` text** (read this
  cycle, direct quotes):
  - `Sony_HB-F1.xml:9`: *"Japanese MSX2 with built in software and speed controller (**not
    emulated**)."*
  - `Sony_HB-F1II.xml:9`: *"Cost reduced version of the HB-F1, with Ren-sha turbo slider, speed
    controller (**not emulated**) and software built in."*
  - `Sony_HB-F1XD.xml:9`: *"Japanese MSX2 with built in disk drive, Ren-sha turbo slider and speed
    controller (**not emulated**)."*
  - `Sony_HB-F1XDJ.xml:9`: *"Japanese MSX2+ with built in disk drive, MSX-MUSIC, Ren-sha turbo
    slider and speed controller (**not emulated**)."*
  - `Sony_HB-F1XDmk2.xml:9`: *"A newer version of the Sony HB-F1XD, but in emulation it's identical
    to its predecessor"* — the predecessor (`Sony_HB-F1XD.xml`) explicitly says "not emulated"
    (found this cycle via QA's own fresh `find` for every `Sony_HB-F1*.xml` file, RESP-M25-003;
    independently confirmed to wire no Pause/SpeedController device and to carry the identical
    real `<RenShaTurbo min_ints=47 max_ints=221>` calibration as `Sony_HB-F1XV.xml`).
  - `Sony_HB-F1XV.xml:9` (this project's actual target machine) does not even mention the speed
    controller in its own (shorter) description — consistent with "not emulated, not even
    discussed" rather than a silent omission of something that IS emulated.
- **Conclusion, stated plainly for §2.7/§4:** there is NO Sony-specific PAUSE/speed-controller
  reference behavior anywhere in openMSX 21.0 to diff against. This is genuine, honest,
  first-principles design work grounded solely in the two project fact-sheets, not a corner cut.

### 2.3 Architecture decision — where PAUSE/Speed-Controller lives

Per the task's explicit instruction, `src/machine/hbf1xv_machine.{h,cpp}` was read in full before
proposing this design (already summarized in §2.2's `MSXTurboRPause` finding above, which
ELIMINATES the most tempting shortcut — "just call some session-pause API" — as architecturally
wrong for this project).

**The existing, working VBlank/vsync architecture** (`run_frame()`,
`src/machine/hbf1xv_machine.cpp:346-362`):
```cpp
void Hbf1xvMachine::run_frame() {
    scheduler_.tick(kFrameCycles);
    ++frame_count_;
    vdp_.on_vsync();               // <- the exact hook point this milestone reuses
    last_vsync_cycle_ = elapsed_cycles();
}
```
`run_frame()` is a **coarse, CPU-agnostic** primitive: it advances the scheduler clock a whole
frame atomically and fires the VDP's own `on_vsync()` hook — it never touches the CPU at all. The
**actual CPU-execution primitive** used everywhere else in this codebase (every M9-M24 test, and
the only place any CPU instruction is ever decoded) is `step_cpu_instruction()`
(`hbf1xv_machine.cpp:384-422`), which already has an established pattern for a machine-level
external gate/signal being sampled at the top of the function, unconditionally, every call:
```cpp
if (vdp_.irq_active()) {
    cpu_.request_maskable_interrupt();
}
```

**Decision: hardware PAUSE is modeled as a NEW, analogous machine-level gate consulted at the very
TOP of `step_cpu_instruction()`, BEFORE any opcode decode — mirroring the existing VDP-IRQ-sample
line's position and style exactly.** Concretely:

1. A new device, `devices::chipset::Mb670836PauseController` (§2.4), owns the combined
   button/speed-controller PAUSE gate state. It is a machine-composed device exactly like
   `S1985Engine` — NOT part of `Z80aCpu` (zero change to `src/devices/cpu/*`, protecting the
   M9/M12/M23 zero-tolerance CPU-timing oracles), and NOT session-level like openMSX's
   `MSXTurboRPause`/`getMotherBoard().pause()` (architecturally wrong for this project — see
   above).
2. `step_cpu_instruction()` checks `pause_controller_.cpu_should_pause()` FIRST. If true: it
   **skips `cpu_.step()` entirely** (no opcode fetch/decode/execute — PC, R, and every register
   stay frozen, matching A-M25-2's literal reading of "physically halts the CPU"), ticks the
   scheduler by a small idle quantum (§2.4), and returns early — a small, surgically-confined,
   additive, early-return-only insertion at the top of an already-well-understood function,
   mirroring the "surgically confined" precedent language used to describe M20/M23's own
   minimal-footprint CPU-adjacent edits.
3. `run_frame()` is **completely unaffected** by hardware PAUSE — it never calls `cpu_.step()` in
   the first place, matching real hardware (the VDP's own crystal/raster continues regardless of
   CPU state; only the Z80's own WAIT-gated bus cycles freeze). This is a genuinely clean
   architectural fit, not a workaround: the project's ALREADY-EXISTING split between a
   CPU-agnostic `run_frame()` and a CPU-driving `step_cpu_instruction()` happens to be exactly the
   right seam for a PAUSE mechanism that (per real hardware) affects only the CPU's own clock
   domain.
4. The Speed Controller's VBlank-synced duty cycle is driven by ONE new line added to
   `run_frame()`, immediately alongside the EXISTING `vdp_.on_vsync()` call (mirroring the M23-S2
   `last_vsync_cycle_` bookkeeping-only precedent — an additive, non-perturbing single-line
   insertion into an already-shipped function):
   ```cpp
   vdp_.on_vsync();
   pause_controller_.on_vsync();   // <- new; advances the speed-controller duty-cycle window
   last_vsync_cycle_ = elapsed_cycles();
   ```
   `Mb670836PauseController::on_vsync()` is ALSO a public method a test can call directly
   (independent of `run_frame()`'s scheduler-ticking side effect) — needed because this project's
   established testing precedent (A-M24-8: *"this harness never calls `run_frame()`... mirrors the
   established, already-safe M21/M22 system-test pattern of driving the CPU purely via
   `step_cpu_instruction()`"*) deliberately never mixes `run_frame()`'s atomic frame-tick with
   `step_cpu_instruction()`'s per-instruction tick in the same test (that WOULD double-count
   elapsed cycles). A test that wants "N simulated VBlanks interleaved with real CPU stepping"
   therefore calls `machine.pause_controller().on_vsync()` directly at computed T-state boundaries
   while driving the CPU purely through `step_cpu_instruction()` — see §3.2/§4.

**Why this is NOT the same as the Z80's own `HALT` (a critical, explicit differentiation the
kickoff itself flagged):**

| | Z80 `HALT` instruction (M9/M23/C2, already modeled) | MB670836 hardware PAUSE (this milestone) |
|---|---|---|
| Trigger | CPU voluntarily executes the `HALT` opcode | External signal (button press / speed-controller autofire) |
| Where modeled | INSIDE `Z80aCpu` (`z80a_cpu.cpp`, `state().halted()`) | OUTSIDE the CPU entirely, at machine composition |
| `R` register | KEEPS incrementing (C2/M23: HALT internally re-executes NOP-equivalent M1 fetches) | FROZEN — no M1 cycle occurs at all while the external gate holds |
| Released by | ANY interrupt (maskable or NMI) or reset | ONLY the PAUSE gate itself releasing (button pressed again / speed-controller window ends) — "cannot be bypassed in software" |
| Software visibility | Software chose to enter it | Software has NO visibility or agency — the CPU's own logic never runs during it |

### 2.4 Design — `devices::chipset::Mb670836PauseController`

Placement: `src/devices/chipset/mb670836_pause.{h,cpp}` — `src/devices/` per `src/CLAUDE.md`
("chip and controller implementations"), `chipset/` alongside `S1985Engine` (the other
non-CPU/non-memory/non-peripheral Sony/Yamaha glue-logic device this project already models this
way).

```cpp
namespace sony_msx::devices::chipset {

// Sony MB670836 residual PAUSE/speed-controller layer (M25).
//
// Per S1985 fact-sheet §9, the MB670836 ALSO does DRAM address multiplexing --
// NOT modeled here (project-wide precedent: DRAM refresh/muxing is real but
// transparent to software, Z80A fact-sheet §7; only the R-register VALUE is
// modeled, never actual refresh cycles). This class models ONLY the
// PAUSE/speed-controller function.
class Mb670836PauseController {
public:
    // Speed-Controller duty-cycle period, in VBlank frames. NOT a Sony hardware
    // fact (no datasheet/measurement exists) -- a documented, first-principles
    // DESIGN DEFAULT (A-M25-3). level=0 => never paused (full speed);
    // level=kMaxSpeedLevel => paused kMaxSpeedLevel of every kPeriodFrames
    // VBlanks (maximum slow-motion).
    static constexpr int kPeriodFrames = 8;
    static constexpr int kMaxSpeedLevel = kPeriodFrames - 1;  // 0..7

    void reset();  // engaged_=false, speed_level_=0, frame_index_=0 -- idle default

    // Physical PAUSE button: one press toggles engaged/disengaged (A-M25-1).
    void press_pause_button();
    [[nodiscard]] bool button_engaged() const;

    // Speed Controller slider (0 = full speed/disabled .. kMaxSpeedLevel).
    void set_speed_level(int level);
    [[nodiscard]] int speed_level() const;

    // VBlank hook (A-M25-4/§2.3): advances the internal frame counter used by
    // the speed-controller duty cycle. Called from run_frame() OR directly by
    // tests driving the CPU purely via step_cpu_instruction().
    void on_vsync();
    [[nodiscard]] bool speed_controller_paused_this_frame() const;  // introspection

    // Combined CPU-execution gate, consulted by step_cpu_instruction() BEFORE
    // any opcode decode. True if EITHER source is active (A-M25-4).
    [[nodiscard]] bool cpu_should_pause() const;

private:
    bool engaged_ = false;
    int speed_level_ = 0;
    std::uint64_t frame_index_ = 0;
};

}  // namespace sony_msx::devices::chipset
```

Duty-cycle formula (frame `f` is paused iff `(f % kPeriodFrames) < speed_level_`) — a clean,
deterministic, directly-computable oracle for tests.

**`step_cpu_instruction()` gating** (the ONLY change to `hbf1xv_machine.cpp`'s existing function
body — everything after this early-return block is completely unmodified):
```cpp
std::uint32_t Hbf1xvMachine::step_cpu_instruction() {
    if (pause_controller_.cpu_should_pause()) {
        // Hardware PAUSE: no M1/opcode-fetch cycle occurs -- PC/R/registers are
        // NOT touched (A-M25-2). One idle T-state per call is the finest-grained
        // unit this whole-instruction-atomic engine can charge for an
        // indefinitely-held external WAIT condition (no datasheet quantizes
        // this -- a modeling choice, not a hardware fact). VDP/RTC/FDC clock
        // sources are UNAFFECTED (their crystal is not gated by the Z80 WAIT
        // pin on real hardware) -- only CPU decode/execute is suppressed.
        constexpr std::uint32_t kPausedIdleTStates = 1;
        scheduler_.tick(kPausedIdleTStates);
        return kPausedIdleTStates;
    }
    // ... existing body, completely unmodified from pre-M25 ...
}
```
The function's return-value CONTRACT ("T-states this call consumed") is preserved exactly, so any
caller summing return values to track elapsed time remains correct whether or not PAUSE is
engaged.

### 2.5 Design — `peripherals::RenshaTurbo`

Placement: `src/peripherals/rensha_turbo.{h,cpp}` — `src/peripherals/` per `src/CLAUDE.md`
("keyboard matrix, joystick, and slot-side peripheral adapters"); this is an autofire adapter
feeding keyboard/joystick reads, the same category as the existing `CassetteInterface` precedent.

```cpp
namespace sony_msx::peripherals {

// X-pattern clock source (mirrors RtcClockSource/rp5c01.h, FdcClockSource/
// fdc_clock_source.h, CassetteClockSource/cassette_interface.h exactly).
class RenshaTurboClockSource {
public:
    virtual ~RenshaTurboClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_cycles() const = 0;
};

// Ren-Sha Turbo autofire (M25, backlog C8 sub-item). Grounded concretely in
// openMSX's RenShaTurbo.{hh,cc}/Autofire.{hh,cc} (behavior reference, A-M25-7)
// and Sony_HB-F1XV.xml's real min_ints=47/max_ints=221 (A-M25-5) -- NOT
// transcribed code (A-M25-6 independently re-derives the frequency formula).
//
// Deliberately a CONCRETE class held by direct pointer (not an abstract
// interface like CassetteInputSource) -- unlike CassetteInterface, this class
// has no dependency of its own that would create coupling pressure on its
// consumers, so the extra interface layer buys nothing here.
class RenshaTurbo {
public:
    static constexpr std::uint64_t kSystemClockHz = 3579545;
    // Sony_HB-F1XV.xml:17-18 (A-M25-5) -- real per-machine calibration.
    static constexpr unsigned kDefaultMinInts = 47;   // fastest (speed=100)
    static constexpr unsigned kDefaultMaxInts = 221;  // slowest (speed=1)

    void reset();  // speed_=0 (disabled) -- regression-safe idle default
    void attach_clock_source(RenshaTurboClockSource* source);

    void set_speed(int speed);   // 0..100, 0 = disabled (Autofire.hh precedent)
    [[nodiscard]] int speed() const;

    // Negative-logic autofire pulse at the current cpu_cycles(). false when
    // disabled/unattached -- regression-safe default.
    [[nodiscard]] bool signal() const;

    // Consumer-facing OR masks (A-M25-7): 0x01 (keyboard row8 bit0 = SPACE) /
    // 0x10 (PSG R14 bit4 = trigger A) when signal() is true, else 0x00. Callers
    // OR these into an already-computed read value -- NEVER forces a 0 bit,
    // only ever forces a bit from 0->1 (a periodic RELEASE pulse, matching
    // real hardware -- R-M25-6).
    [[nodiscard]] std::uint8_t keyboard_row8_or_mask() const;
    [[nodiscard]] std::uint8_t joystick_trigger_a_or_mask() const;

private:
    [[nodiscard]] std::uint64_t half_period_cycles() const;  // A-M25-6 derivation

    int speed_ = 0;
    RenshaTurboClockSource* clock_source_ = nullptr;
};

}  // namespace sony_msx::peripherals
```

**Frequency derivation (A-M25-6), fully worked, no openMSX table/formula transcribed:**
`ints` = interpolated linearly between `kDefaultMaxInts` (speed=1, slowest) and `kDefaultMinInts`
(speed=100, fastest); `ints` represents 50 on/off periods over `ints` VBlank interrupts at 60 Hz
(NTSC), i.e. `ints/60` seconds → `freq_hz = 50 / (ints/60) = 3000/ints`. A full square-wave cycle
is 2 toggles, so `half_period_cycles = kSystemClockHz / (2 * freq_hz) = kSystemClockHz * ints /
6000`, computed in integer `uint64_t` arithmetic (no floating point — full determinism).
`signal() = ((clock_source_->cpu_cycles() / half_period_cycles()) & 1) == 1` when `speed_ != 0`,
else `false`.

**Wiring into `KeyboardMatrix`/`JoystickPorts`** (both additive, both default nullptr = byte-for-
byte pre-M25 behavior, mirroring the `attach_cassette_input_source(nullptr)` regression-guard
pattern used since M18):
```cpp
// keyboard_matrix.h/.cpp
void attach_rensha_turbo(const RenshaTurbo* source);   // default nullptr
// inside keyboard_row(int row) const override:
std::uint8_t value = rows_[row];
if (row == 8 && rensha_ != nullptr) {
    value |= rensha_->keyboard_row8_or_mask();
}
return value;
```
```cpp
// joystick.h/.cpp
void attach_rensha_turbo(const RenshaTurbo* source);   // default nullptr
// inside read_port_a() override, applied to the SELECTED port's already-
// computed value (matches openMSX's ports[selectedPort]-after-mux wiring,
// A-M25-7):
std::uint8_t value = encode(ports_[selected_]) | kLayoutBit | cassette_bit;
if (rensha_ != nullptr) {
    value |= rensha_->joystick_trigger_a_or_mask();
}
return value;
```

### 2.6 `src/` placement summary

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/chipset/mb670836_pause.h`/`.cpp` (**new**) | `Mb670836PauseController` — combined button/speed-controller CPU-execution gate. | S1985 fact-sheet §9; Z80A fact-sheet §6 item 3; A-M25-1..4. |
| `src/peripherals/rensha_turbo.h`/`.cpp` (**new**) | `RenshaTurbo` — autofire signal generator + consumer OR-masks. | openMSX `RenShaTurbo.{hh,cc}`/`Autofire.{hh,cc}` (behavior reference); `Sony_HB-F1XV.xml:16-19`; A-M25-5..7. |
| `src/peripherals/keyboard_matrix.h`/`.cpp` (**edit, additive**) | `attach_rensha_turbo()` + row-8-bit0 OR. | A-M25-7. |
| `src/peripherals/joystick.h`/`.cpp` (**edit, additive**) | `attach_rensha_turbo()` + trigger-A OR. | A-M25-7. |
| `src/machine/hbf1xv_machine.h`/`.cpp` (**edit, additive**) | Owns `Mb670836PauseController`, `RenshaTurbo`, a new `RenshaTurboClock` (5th X-pattern class, mirrors `RtcClock`/`FdcClock`/`CassetteClock`); gates `step_cpu_instruction()`; adds the `on_vsync()` hook line to `run_frame()`; adds `.reset()` calls to `cold_boot()`; adds accessors (`pause_controller()`, `rensha_turbo()`) mirroring the existing `s1985()`/`ppi()` accessor style. | §2.3/§2.4/§2.5. |

Nothing under `src/devices/cpu/`, `src/devices/video/`, `src/devices/audio/`, `src/devices/rtc/`,
`src/devices/fdc/`, `src/devices/cartridge/`, `src/devices/memory/`, `src/devices/halnote/`,
`src/devices/kanji/`, or `src/core/` is touched.

### 2.7 openMSX A/B evidence plan — honest, per-feature

- **Ren-Sha Turbo: FEASIBLE.** PRIMARY technique — an architectural/wiring-level check using the
  established Tcl `debug write ioports`/`reg`/`debug read` toolkit (`tools/openmsx-trace-parity.ps1`
  mechanics, proven since M10): confirm openMSX's own keyboard-row-8/PSG-R14 OR-combine wiring
  points and bit positions match this project's implementation (A-M25-7), and confirm the
  regression-safety invariant that with the SPACE key/trigger genuinely NOT held, RenSha produces
  ZERO observable difference on either engine (since it only ever forces a release, never a press
  — R-M25-6). A SECONDARY, best-effort attempt at a bounded live-session toggle-cadence comparison
  (matched `min_ints`/`max_ints` config on both sides) may be attempted but must be honestly
  reported as PARITY/DIVERGENCE/BLOCKED per what is ACTUALLY observed, not pre-guaranteed —
  explicitly caveated given M23's own finding that live-Tcl-session timing can diverge from this
  project's deterministic engine once meaningful wall-clock time elapses.
- **Hardware PAUSE / Speed Controller: BLOCKED, and this is the RIGHT, honest disposition, not a
  shortfall.** §2.2 establishes concretely that openMSX 21.0 has ZERO Sony-specific PAUSE/
  speed-controller modeling anywhere — not "hard to find," genuinely absent, with the reference
  project's OWN machine-description text saying so explicitly for four of five sibling Sony
  machines. There is no reference engine behavior to diff against. This sub-claim MUST be reported
  as BLOCKED in `docs/m25-parity-trace-diff.md` with the exact citations from §2.2, mirroring the
  established, accepted pattern of an honestly-disclosed BLOCKED sub-claim (M21's "no
  computed-pixel-color introspection point," C3/M24's disk-boot-A/B) that does NOT gate an
  otherwise-complete milestone's closure (§4 Acceptance Criterion 9).

---

## 3. Milestones — Slice Plan (M25-S1 … S5)

Every slice runs the full evidence-gate set (§4 Acceptance Criterion 12) and leaves `ctest` green
across the entire M1-M24 suite (124 tests; the slow `hbf1xv_m24_zexall_system_test` may be excluded
from PER-SLICE inner-loop runs via `ctest -LE m24_slow_full_sweep` for iteration speed, but MUST
be included at least once before requesting QA, per §4 Acceptance Criterion 6).

### M25-S1 — `Mb670836PauseController` (isolated unit tests, zero machine wiring yet)

- New `src/devices/chipset/mb670836_pause.{h,cpp}` per §2.4.
- New `tests/unit/devices/chipset/mb670836_pause_unit_test.cpp`: `reset()` idle defaults;
  `press_pause_button()` toggles `button_engaged()` true→false→true; `set_speed_level()`
  clamping/range; `on_vsync()`+duty-cycle formula exhaustively verified for at least 2 distinct
  speed levels across a full `kPeriodFrames`-length window (e.g. `speed_level=3` over 8 calls to
  `on_vsync()` produces the exact expected true/false sequence, hand-computed and asserted);
  `cpu_should_pause()` truth table (button-only, speed-controller-only, both, neither — all 4
  combinations).
- **Gates:** build + ctest green (full suite, 124 prior + new).

### M25-S2 — `RenshaTurbo` (isolated unit tests, zero peripheral wiring yet)

- New `src/peripherals/rensha_turbo.{h,cpp}` per §2.5.
- New `tests/unit/peripherals/rensha_turbo_unit_test.cpp`: `reset()`/unattached-clock idle default
  (`signal()` always false); `speed()==0` → always false regardless of cycles; the `half_period_
  cycles()` derivation independently re-verified by the TEST ITSELF at 2-3 concrete speed values
  (hand-computed expected periods, per A-M25-6's formula, asserted against the class's own
  `signal()` toggling at the expected cycle boundaries); `keyboard_row8_or_mask()`/
  `joystick_trigger_a_or_mask()` return the correct bit values (0x01/0x10) exactly when `signal()`
  is true, 0x00 otherwise.
- **Gates:** build + ctest green (full suite).

### M25-S3 — `KeyboardMatrix`/`JoystickPorts` wiring + regression guards

- Additive edits to `src/peripherals/keyboard_matrix.{h,cpp}` and `src/peripherals/joystick.{h,cpp}`
  per §2.5.
- New `tests/integration/peripherals/rensha_turbo_integration_test.cpp`: (a) REGRESSION GUARD —
  with `attach_rensha_turbo(nullptr)` (the default), `keyboard_row(8)`/`read_port_a()` are
  byte-for-byte identical to pre-M25 for every existing M15-M18 test scenario re-exercised; (b) with
  a live `RenshaTurbo` attached and speed>0 but the SPACE key/trigger-A NOT pressed, the OR'd bit
  never forces a spurious "pressed" reading at ANY sampled cycle (R-M25-6, exhaustively checked
  across a full toggle period); (c) with the key/trigger genuinely pressed AND RenSha engaged,
  the read value alternates between "pressed" and "released" at the expected computed cadence.
- **Gates:** build + ctest green (full suite).

### M25-S4 — Machine wiring: hardware PAUSE gate + Speed-Controller `on_vsync()` hook

- Additive edits to `src/machine/hbf1xv_machine.{h,cpp}` per §2.3/§2.4/§2.6: new owned members
  (`pause_controller_`, `rensha_turbo_`, `RenshaTurboClock rensha_clock_{scheduler_}`), the
  `step_cpu_instruction()` early-return gate, the `run_frame()` one-line `on_vsync()` hook, the new
  `.reset()` calls in `cold_boot()`, and the new public accessors (`pause_controller()`,
  `rensha_turbo()`).
- New `tests/integration/machine/hbf1xv_m25_pause_integration_test.cpp`: (a) REGRESSION GUARD — with
  the pause controller in its default (post-`cold_boot()`) state, `step_cpu_instruction()`'s
  observable behavior (return value, PC/register progress, `elapsed_cycles()` delta) is
  byte-for-byte identical to pre-M25 for a representative existing CPU test program; (b) load a
  small, deterministic counter-increment loop via `map_flat_ram()`+`load_memory()`, step it
  forward N instructions, call `press_pause_button()`, step forward M MORE times — assert PC,
  every register, R, and the counter's memory location are IDENTICAL before and after those M
  steps, and `elapsed_cycles()` advanced by EXACTLY M (1 T-state/paused call); (c) call
  `press_pause_button()` again (release) and confirm execution resumes from the exact frozen PC
  and the counter continues incrementing correctly on the next real step.
- New `tests/integration/machine/hbf1xv_m25_speed_controller_integration_test.cpp`: drive the same
  counter-loop program purely via `step_cpu_instruction()` while calling
  `machine.pause_controller().on_vsync()` directly at computed `kFrameCycles`-multiple T-state
  boundaries (NEVER calling `run_frame()` in the same test, per A-M24-8's established precedent —
  avoids double-counting elapsed cycles); at a known `speed_level`, assert the counter's growth
  over K total simulated VBlank windows matches the exact expected duty-cycle fraction (a
  deterministic, hand-computable oracle per §2.4's formula).
- **Gates:** build + ctest green (full suite); explicit confirmation (via `git diff`) that every
  named M9/M12/M23 zero-tolerance CPU-timing-oracle test file is byte-for-byte unchanged.

### M25-S5 — Dedicated system integration test + A/B evidence + backlog/documentation closure

- New `tests/system/hbf1xv_m25_speed_pause_rensha_system_test.cpp`: the milestone's required
  "dedicated system integration test" — combines all three features against one stepped CPU
  program in a single deterministic scenario (hardware PAUSE engage/release mid-program; Speed
  Controller duty-cycle slow-motion across several simulated VBlank windows; Ren-Sha Turbo
  observed toggling the SPACE-key row and a joystick trigger over many cycles, both while
  held and — as a negative-control assertion — while NOT held).
- `tools/openmsx-m25-rensha-parity.ps1` (new) → `docs/m25-parity-trace-diff.md` (new), per §2.7:
  Ren-Sha Turbo's architectural/wiring check (feasible), PAUSE/Speed-Controller's honest BLOCKED
  disposition (with the §2.2 citations reproduced in the artifact itself, not merely referenced).
- `docs/m25-implementation-report.md`: full slice-by-slice summary, the exact duty-cycle/
  frequency arithmetic verified, the regression-guard confirmations, and the A/B disposition.
- Full 34-row backlog review written into `agent-protocol/state/deferred-backlog.md` (§3.5's
  disposition).
- **Gates:** all of the above green; the FULL M1-M24 suite including
  `hbf1xv_m24_zexall_system_test` run at least once (~24-27 min budgeted); QA sign-off required
  before closure.

---

### 3.5 Full Deferred-Backlog Review (all 34 rows re-affirmed)

Per DEC-0005, every planner package must consult `agent-protocol/state/deferred-backlog.md` in
full (read in full this cycle, 404 lines) and restate every row.

**Section A (human-directive-tracked, 2026-07-06):**
- **B1** PSG/YM2149 internals — DONE (M15), re-affirmed unchanged; M25 touches no PSG register/
  generator logic (RenshaTurbo ORs into the PSG's PORT SOURCE read value, at the `JoystickPorts`
  layer, never inside `PsgYm2149` itself).
- **B2** RTC/RP5C01 internals — DONE (M15), re-affirmed unchanged; M25 touches no RTC device.
- **B3** FM-PAC/YM2413 device — DONE (M17), re-affirmed unchanged; M25 touches no audio device.
- **B4** MSX-JE 16 KB SRAM — DONE (M20), re-affirmed unchanged.
- **B5** Kanji font ROM I/O — DONE (M18), re-affirmed unchanged.
- **B6** Halnote/MSX-JE firmware mapping — DONE (M20), re-affirmed unchanged.
- **B7** Cartridge loading — DONE (M19), re-affirmed unchanged.
- **B8** FDC drive mechanics — DONE (M16), re-affirmed unchanged.
- **B9** VRAM/V9958 VDP contract — DONE (M14), re-affirmed unchanged; M25's `run_frame()` edit is a
  single additive line calling the NEW `pause_controller_.on_vsync()`, immediately alongside the
  pre-existing, UNCHANGED `vdp_.on_vsync()` call — zero edit to any VDP file.

**Section B (other known deferrals):**
- **C1** Exact cycle/T-state timing parity — re-affirmed **IN-PROGRESS (M23 partial)**, unchanged;
  M25 adds no CPU-timing-affecting code inside `Z80aCpu` itself — the new PAUSE gate is a
  machine-level early-return BEFORE `cpu_.step()` is ever called, not a CPU-core timing change.
- **C2** Z80 HALT-R increment — DONE (M23), re-affirmed unchanged; M25 touches no CPU-core file, and
  the PAUSE mechanism explicitly, deliberately does NOT increment `R` (the opposite of HALT-R,
  §2.3's differentiation table) — this is a genuinely different mechanism, not a regression risk to
  C2's own closed scope.
- **C3** ZEXDOC/ZEXALL full parity sweep — DONE (M24), re-affirmed unchanged; M25 touches no CPU-core
  file, so the M24 sweep's own findings remain valid without re-running the full sweep's *content*
  (though the system test itself must still pass as part of the full-suite regression gate, §4
  Acceptance Criterion 6).
- **C4** S1985 backup-RAM `.sram` persistence — DONE (M15), re-affirmed unchanged.
- **C5** Full boot past first device read — re-affirmed **IN-PROGRESS (M16 partial)**, unchanged;
  unrelated to M25 (no boot-sequence code touched).
- **C6** Peripherals (keyboard/joystick) — DONE (M15), re-affirmed unchanged; M25 ADDS to
  `KeyboardMatrix`/`JoystickPorts` (attach points for `RenshaTurbo`) but the addition is a
  regression-guarded, default-nullptr, additive extension — C6's own already-closed contract
  (matrix read/joystick-port semantics with no RenSha attached) is explicitly re-verified
  byte-for-byte unchanged by M25-S3's regression-guard test.
- **C7** Printer + cassette — DONE (M18), re-affirmed unchanged; M25 touches no printer/cassette file.
- **C8** Sony speed-controller/PAUSE + Ren-Sha Turbo — **THIS MILESTONE.** Target: closes IN FULL per
  §4 Acceptance Criterion 9's precise disposition rule (implementation complete for all three
  sub-items; the PAUSE/Speed-Controller A/B sub-claim is honestly BLOCKED for lack of a comparable
  reference engine, which does not gate closure).
- **C9** SDL3 frontend — re-affirmed OPEN; unrelated to this milestone (still indicative M26 per
  `agent-protocol/state/current-phase.md:67`). Note for the FUTURE C9 milestone: PAUSE/Speed-
  Controller/Ren-Sha Turbo now have a complete machine/peripheral-level API surface
  (`press_pause_button()`, `set_speed_level()`, `RenshaTurbo::set_speed()`) ready for a future
  keyboard/input binding — this milestone deliberately adds no CLI/SDL3 wiring itself (§1.2).
- **C10** FDC flux-level/DMK disk fidelity — re-affirmed OPEN; unrelated to this milestone.

**Section C (M14 VDP-depth deferrals):**
- **D1** Pixel-accurate raster rendering pipeline — DONE (M21), re-affirmed unchanged.
- **D2** Sprite rendering + collision — DONE (M22), re-affirmed unchanged.
- **D3** VDP command engine — DONE (M22), re-affirmed unchanged.
- **D4** Cycle-accurate VDP access-slot/command timing — re-affirmed **IN-PROGRESS (M23 partial)**,
  unchanged; M25 touches no VDP-timing file.
- **D5** YJK/YJK+YAE color decode — DONE (M21), re-affirmed unchanged.
- **D6** Scroll/interlace/blink/superimpose — DONE (M21), re-affirmed unchanged.
- **D7** G6/G7 planar interleave — DONE (M22, closed in full), re-affirmed unchanged.

**Section D (M17 YM2413 depth deferrals):**
- **E1** YM2413 FM-synthesis DSP/audio depth — re-affirmed OPEN; unrelated to this milestone.
- **E2** YM2413 register-write timing constraint — re-affirmed OPEN; unrelated to this milestone.

**Section E (M18 printer/cassette depth deferrals):**
- **F1** Cassette tape image-format/signal fidelity — re-affirmed OPEN; unrelated to this milestone.
- **F2** Printer image/ESC-sequence rendering depth — re-affirmed OPEN; unrelated to this milestone.

**Section F (M19 cartridge-mapper depth deferrals):**
- **G1** KonamiSCC + embedded SCC chip — re-affirmed OPEN; unrelated to this milestone.
- **G2** ROM-database/heuristic mapper auto-detection — re-affirmed OPEN; unrelated to this
  milestone.
- **G3** Full runtime cartridge hot-plug — re-affirmed OPEN; unrelated to this milestone.
- **G4** Long tail of ~80 other mapper types — re-affirmed OPEN; unrelated to this milestone.

No new backlog rows are proposed this cycle — every sub-item this milestone touches was already
named inside the existing C8 row.

---

## 4. Acceptance Criteria

1. **A genuine, fact-sheet-grounded design for all three mechanisms** (§2.3/§2.4/§2.5), with every
   numeric/behavioral choice either directly cited to a fact-sheet/openMSX source or explicitly
   labeled an Assumption with a verification action (§1.3) — no behavior fabricated.
2. **Hardware PAUSE genuinely, verifiably halts CPU execution**: a dedicated integration test
   (M25-S4) proves PC/registers/R/memory state are byte-for-byte FROZEN across multiple paused
   `step_cpu_instruction()` calls, `elapsed_cycles()` still advances (1 T-state/call), and
   execution resumes correctly from the exact frozen PC on release.
3. **Hardware PAUSE cannot be "bypassed in software" within this emulator's own API surface**: no
   method on `Z80aCpu`, `CpuBusClient`, or any bus-visible I/O port can clear the gate — only
   `Mb670836PauseController::press_pause_button()` (called at the machine-composition/test/future-
   frontend layer, never from CPU-visible code) can. Verified by inspection (no I/O port is wired
   to the pause controller) and a passing test that a CPU program alone (any instruction sequence)
   cannot self-release PAUSE.
4. **Speed Controller's VBlank-synced duty cycle is genuinely deterministic and independently
   computable**: the integration test's oracle is HAND-COMPUTED from §2.4's stated formula, not
   merely "whatever the code produces."
5. **Ren-Sha Turbo never forces a spurious press** (R-M25-6): with the underlying key/trigger not
   held, the autofire signal has ZERO observable effect at any sampled cycle — an explicit,
   dedicated, exhaustively-checked test assertion (M25-S3).
6. **Zero regression**: the FULL M1-M24 suite (124 tests, including the ~24-27-minute
   `hbf1xv_m24_zexall_system_test`) passes 100%, independently reproduced by the developer and QA
   from clean rebuilds; `git diff` against `v1.0.24` shows NO change to `src/devices/cpu/`,
   `src/devices/video/`, `src/devices/audio/`, `src/devices/rtc/`, `src/devices/fdc/`,
   `src/devices/cartridge/`, `src/devices/memory/`, `src/devices/halnote/`, `src/devices/kanji/`,
   or `src/core/` — only new `src/devices/chipset/mb670836_pause.*`, new
   `src/peripherals/rensha_turbo.*`, and additive edits to `src/peripherals/keyboard_matrix.*`,
   `src/peripherals/joystick.*`, and `src/machine/hbf1xv_machine.*`.
7. **Every named M9/M12/M23 zero-tolerance CPU-timing-oracle test file is confirmed byte-for-byte
   UNCHANGED** via direct `git diff`, independently re-confirmed by both the developer and QA
   (mirrors the M23 precedent of this exact confirmation style).
8. **Default/unattached behavior is byte-for-byte unchanged from pre-M25** for `step_cpu_instruction()`
   (pause controller idle), `KeyboardMatrix::keyboard_row()`, and `JoystickPorts::read_port_a()`
   (RenSha unattached) — explicit regression-guard tests, not merely "no test failed."
9. **The precise, non-negotiable A/B disposition rule:** Ren-Sha Turbo's architectural/wiring-level
   A/B check is genuinely attempted and its result (PARITY/DIVERGENCE) is honestly reported in
   `docs/m25-parity-trace-diff.md`. Hardware PAUSE / Speed Controller's A/B sub-claim is reported
   as **BLOCKED**, with the exact §2.2 citations (four Sony machine XML `"(not emulated)"` quotes,
   the `MSXTurboRPause`/`SG1000Pause` architectural-mismatch findings) reproduced in the artifact —
   this BLOCKED disposition does NOT gate C8's closure (mirrors the M21 computed-pixel-color and
   C3/M24 disk-boot-A/B precedents of an honest, non-fabricated BLOCKED sub-claim not blocking an
   otherwise-complete milestone).
10. **The dedicated system integration test** (M25-S5) exercises all three features together against
    one real stepped CPU program, per the milestone kickoff's explicit requirement.
11. **Full 34-row deferred-backlog review** completed and written into `deferred-backlog.md` (§3.5)
    — C8 dispositioned per Acceptance Criterion 9's exact rule; all other 33 rows re-affirmed with a
    one-line justification, no silent drift.
12. **Evidence gates executed and captured**: `tools/validate-assets.ps1`;
    `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`; `cmake --build build --config
    Debug`; `ctest --test-dir build -C Debug --output-on-failure` (full suite, including the slow
    zexall system test at least once); `tools/openmsx-m25-rensha-parity.ps1` →
    `docs/m25-parity-trace-diff.md`.

---

## 5. Risks (each with a verification action)

- **R-M25-1 (no Sony MB670836 datasheet exists — the PAUSE-button toggle-vs-hold semantics and the
  Speed-Controller's numeric duty-cycle range are first-principles design defaults, not hardware
  facts).** *Verification:* A-M25-1/A-M25-3 explicitly label these as design defaults in both this
  package and the eventual code comments; if a genuine Sony service-manual/community measurement
  ever surfaces, re-verify/re-calibrate.
- **R-M25-2 (openMSX genuinely cannot serve as an A/B reference for PAUSE/Speed-Controller).**
  *Verification:* §2.2's exhaustive, cited search stands as the record; QA independently re-runs
  the same searches before accepting the BLOCKED disposition, per Acceptance Criterion 9.
- **R-M25-3 (license-sensitivity: citing RenShaTurbo's real 47/221 config values).** *Verification:*
  A-M25-5 explicitly argues why this is safe (two scalars, not a large table) — QA/coordinator
  should independently judge this reasoning given the standing "zero license-sensitive future work"
  directive's heightened sensitivity around this exact topic; if judged unsafe, the developer falls
  back to a documented, non-openMSX-sourced first-principles default (mirroring A-M25-3's own
  approach for the Speed Controller) with zero loss of correctness (Ren-Sha Turbo's actual toggle
  rate is not a Target-Machine-Specification-mandated exact value).
- **R-M25-4 (the PAUSE gate must be proven to have EXACTLY ZERO regression risk on the CPU-timing
  oracles — the single highest-sensitivity surface this milestone touches).** *Verification:* §4
  Acceptance Criteria 6/7's explicit `git diff` confirmations, independently re-run by both the
  developer and QA, mirroring the M23 precedent's rigor exactly for this exact class of risk.
- **R-M25-5 (double-counting risk if a test naively mixes `run_frame()`'s atomic scheduler tick with
  `step_cpu_instruction()`'s per-instruction tick while testing the Speed Controller).**
  *Verification:* M25-S4's Speed-Controller test explicitly calls `pause_controller().on_vsync()`
  DIRECTLY rather than `run_frame()`, mirroring the established A-M24-8 "never call `run_frame()`
  when driving via `step_cpu_instruction()`" precedent; the developer confirms this discipline is
  followed before requesting QA.
- **R-M25-6 (Ren-Sha Turbo's OR-combine wiring, if implemented backward, would force spurious
  PRESSES rather than periodic RELEASES — a genuine, user-visible correctness defect, e.g. games
  firing uncommanded).** *Verification:* §4 Acceptance Criterion 5's dedicated, exhaustive
  negative-control test (M25-S3) is a hard, non-negotiable gate, not an optional nicety.
- **R-M25-7 (the exact idle T-state charge for a paused `step_cpu_instruction()` call — 1 T-state,
  §2.4 — is a modeling choice, not a hardware-quantized fact, since real hardware's WAIT-line hold
  is not naturally discretized).** *Verification:* explicitly documented as a design choice in code
  comments (not asserted as measured fact); the choice is defensible (finest-grained, no overshoot
  risk) and does not affect any OTHER already-closed milestone's timing oracle, confirmed via
  R-M25-4's `git diff` check.
- **R-M25-8 (scope creep risk: this is the first genuinely novel, unscoped-before hardware feature
  in the project — temptation to over-build, e.g. inventing a CLI/frontend surface prematurely).**
  *Verification:* §1.2's explicit out-of-scope table is a hard boundary; the developer does not add
  any CLI flag, SDL3 binding, or session-pause concept this cycle.

---

## 6. Developer Handoff

**Build order:** M25-S1 (`Mb670836PauseController` isolated) → M25-S2 (`RenshaTurbo` isolated) →
M25-S3 (peripheral wiring + regression guards) → M25-S4 (machine wiring: the highest-risk slice,
touches `step_cpu_instruction()`/`run_frame()`/`cold_boot()` — run the FULL M9/M12/M23 CPU-timing-
oracle `git diff` check immediately after this slice, before proceeding) → M25-S5 (dedicated system
test + A/B evidence + backlog/documentation closure).

**Hard constraints (do not deviate without a `decisions.md` entry):**
- Do NOT touch `src/devices/cpu/*` — hardware PAUSE is machine-level only (§2.3).
- Do NOT model the Sony PAUSE key as a `KeyboardMatrix` row/column cell (§1.2/§2.3's explicit,
  grounded reasoning: "cannot be bypassed in software" is incompatible with an ordinary,
  software-maskable matrix key).
- Do NOT add any CLI flag or SDL3/frontend binding this cycle (§1.2).
- Do NOT mix `run_frame()`'s atomic scheduler tick with `step_cpu_instruction()`'s per-instruction
  tick inside the same test (R-M25-5) — call `pause_controller().on_vsync()` directly instead when
  a test needs simulated VBlank boundaries alongside CPU stepping.
- Do NOT fabricate an A/B PASS for PAUSE/Speed-Controller — Acceptance Criterion 9 requires an
  honest BLOCKED disposition with the exact citations from §2.2.
- Run the FULL M1-M24 suite (124 tests, including the slow zexall system test at least once before
  requesting QA), not a subset.

**Files to create:** `src/devices/chipset/mb670836_pause.h`, `src/devices/chipset/mb670836_pause.cpp`,
`src/peripherals/rensha_turbo.h`, `src/peripherals/rensha_turbo.cpp`,
`tests/unit/devices/chipset/mb670836_pause_unit_test.cpp`,
`tests/unit/peripherals/rensha_turbo_unit_test.cpp`,
`tests/integration/peripherals/rensha_turbo_integration_test.cpp`,
`tests/integration/machine/hbf1xv_m25_pause_integration_test.cpp`,
`tests/integration/machine/hbf1xv_m25_speed_controller_integration_test.cpp`,
`tests/system/hbf1xv_m25_speed_pause_rensha_system_test.cpp`,
`tools/openmsx-m25-rensha-parity.ps1`, `docs/m25-implementation-report.md`,
`docs/m25-parity-trace-diff.md`.

**Files to edit (additive only):** `src/peripherals/keyboard_matrix.h`/`.cpp`,
`src/peripherals/joystick.h`/`.cpp`, `src/machine/hbf1xv_machine.h`/`.cpp`, `tests/CMakeLists.txt`
(register new test executables — no `SONY_MSX_BIOS_DIR`-style asset-root wiring is needed this
milestone, since nothing new reads a ROM/BIOS asset, a genuine risk-reduction relative to M24's own
DEC-0016-class exposure), `agent-protocol/state/deferred-backlog.md` (§3.5 dispositions),
`agent-protocol/state/milestones.md`, `agent-protocol/state/definition-of-done.yaml`,
`agent-protocol/state/current-phase.md`. Optional, developer's judgment (not a hard acceptance
criterion): a new `DebugEventType::Pause`/`Resume` case in `src/machine/debug_event_log.{h,cpp}`
and a small PAUSE/RenSha section in `Hbf1xvMachine::serialize_state_dump()`, extending this
project's existing debug-tooling ethos to the two genuinely new pieces of observable machine state
this milestone introduces.

**Evidence to capture before requesting QA:** full `ctest` output (124 prior + new, 0 failed, with
the slow zexall system test's inclusion called out explicitly); the hand-computed duty-cycle/
frequency oracle values used by each integration test, cross-checked against the actual test
output; the regression-guard confirmations (default state byte-for-byte unchanged); the
`git diff v1.0.24` confirmation that no CPU/VDP/audio/RTC/FDC/cartridge/memory/halnote/kanji/core
file changed; the Ren-Sha Turbo A/B result and the honestly-BLOCKED PAUSE/Speed-Controller
disposition (both in `docs/m25-parity-trace-diff.md`); the four asset/build/test evidence-gate
script outputs.
