# M22 Planner Package — Sprites + VDP Command Engine (closes D2/D3, finishes D7)

- Milestone ID: M22
- Title: Sprite Rendering + Collision Detection, VDP Command Engine, D7 Completion
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M22-001 (planner-first, no production code)
- Decisions in force: DEC-0005 (backlog disposition discipline), DEC-0018 (M21 closure; explicitly
  mandates M22 finish D7's remaining command-engine-path piece), the human's 2026-07-07
  M21-M23 pre-authorization (separate tags, per-milestone QA sign-off, full-system cross-check
  every cycle).
- Backlog effect: **closes D2 and D3 in full this cycle; closes D7 in full this cycle**
  (transitions D7 from `IN-PROGRESS (M21 partial)` to `DONE (M22)`) — no row remains partial
  after this milestone. See §1.4/§1.5 for the two required architectural resolutions and the D7
  grounding correction.
- Gate: normal human-release-decision gate applies, but per `agent-protocol/state/current-phase.md`
  the coordinator is pre-authorized to proceed through the release-decision/tag step for this
  specific M21-M23 run without an additional pause request, UNLESS QA does not reach a clean PASS.

> Grounding rule: every behavior-affecting claim below cites a concrete
> `references/openmsx-21.0/...` path with line numbers, independently re-derived (not
> transcribed) by the planner this cycle. openMSX source is the BEHAVIOR reference only (GPL) —
> **never copied into `src/`**.

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) Backlog D2** — sprite rendering + collision/5th-sprite detection: Sprite Mode 1 (32
  sprites, max 4/line, TMS9918-compatible, one color per sprite) and Sprite Mode 2 (32 sprites,
  max 8/line, a color per sprite PER LINE via a separate 512-byte color/attribute table), the
  1-pixel vertical shift quirk, EC (early clock, x-32 shift)/CC (color-cascade OR-merge)/IC
  (collision-exclusion) attribute bits, S#0's 5S (5th/9th-sprite number) and C (collision) flags,
  S#3-S#6's collision X/Y coordinates (with their documented +12/+8 offsets), and pixel
  compositing onto M21's `FrameBuffer` background output.
- **(b) Backlog D3** — the VDP command engine: R#32-46 (SX/SY/DX/DY/NX/NY/COL/ARG/CMD), the full
  command set (ABRT/STOP, POINT, PSET, SRCH, LINE, LMMV, LMMM, LMCM, LMMC, HMMV, HMMM, YMMM,
  HMMC), the 5 logical operations (IMP/AND/OR/XOR/NOT, each with a "T" transparent variant, for
  the 5 commands that actually support them: PSET/LINE/LMMV/LMMM/LMMC), S#2's TR (Transfer
  Ready)/CE (Command Executing)/BD (Border Detected) handshake bits, S#7's color/command-result
  register, S#8/S#9's border-X (SRCH result) coordinates, and R#25's CMD-in-all-modes behavior
  (non-bitmap-mode command execution using G7-style X/Y coordinates over a flat, non-planar
  address space).
- **(c) Backlog D7's remaining piece (mandatory, DEC-0018)** — the command engine's OWN
  coordinate-to-VRAM-address resolution for every scrMode the command engine supports (G4/G5
  non-planar, G6/G7 planar, and the R#25-CMD-bit-gated NonBitmap mode), independently re-derived
  and cross-verified for physical-bank-placement consistency with M21's already-closed
  CPU-port/display-path rotate-right-by-1 model (§1.5). This closes D7 to **DONE (M22)** — no
  longer partial.
- **(d) Both architectural resolutions required by the task** (§1.4): sprite-compositing
  integration point, and command-engine execution model (atomic vs. stateful).
- **(e) Deterministic unit + integration + a dedicated system integration test**, zero regression
  across the FULL M1-M21 suite (106 tests), independently re-verified by both the developer and
  QA per the human's explicit "deliberate cross-check across the entire system" directive.
- **(f) Real openMSX A/B evidence** — sprite collision/5th-sprite status and command-engine VRAM
  writes/status bits are raw-byte/register-comparable via the same Tcl-debugger technique M11-M21
  established (§2.7); any sub-claim with no feasible headless equivalent is reported BLOCKED, not
  fabricated.
- **(g) Full deferred-backlog review** — all 34 rows re-affirmed (§4).

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M22 | Owning milestone (candidate) |
|---|---|---|
| **Cycle-accurate VDP access-slot/command timing** (backlog D4) — 1368 VDP-cycles/line, slot tables (154/88/31), 16-cycle request latency, CPU-access priority, the command engine's real per-pixel/per-line VDP-cycle cost (fact-sheet §8's 48/64/72/88-cycle table) | M22's command engine executes ATOMICALLY (§1.4 resolution 2) for the 10 non-transfer commands and via an event-driven (not wall-clock) state machine for the 3 transfer commands — mirrors M21's own explicit deferral of cycle-accurate VDP timing. No VDP-cycle countdown, access-slot scheduling, or `EmuTime`-equivalent is introduced. | M23 (paired with C1/C2, already the indicative next milestone in this same pre-authorized run) |
| **CPU-access-steals-command-engine-slot interaction** (fact-sheet §8: "CPU VRAM access takes priority... with sprites on, a HMMV can be cut ~2x by simultaneous OUT (#98),A") | Depends entirely on D4's access-slot model; this milestone's atomic/event-driven execution has no notion of a "slot" to steal. | M23 |
| **Sprite fetch pattern always running when sprites enabled regardless of visible count** (fact-sheet §6 emulation pitfall (a); a real-hardware VRAM-bandwidth/timing artifact) | A cycle-timing-visible side effect (affects command-engine speed), not a CPU/status-visible behavior; out of scope absent D4. | M23 |
| **Extended/expansion VRAM** (MXS/MXD "select expansion RAM" bits, R#45 bit6 MXC/`cpuExtendedVram`, the `0x20000`+ addressing branch in every `Graphic*Mode::addressOf`) | HB-F1XV has 128 KB VRAM, fixed, no expansion socket (fact-sheet §2, already an established M14/M21 scope boundary). MXS/MXD/R#45 bit6 are STORED for register-contract completeness but their "switch to expansion bank" effect is a non-goal — commands always address the fixed 128 KB store. | N/A — no hardware to model |
| **Command engine "too-fast" self-overlap streak artifact** (fact-sheet §10: LMMM copies with too-small overlap distance) | An artifact of D4's real per-pixel VRAM-access pipeline timing; atomic execution has no pipeline to model. | M23 |
| **Command modifying R#23/R#18 mid-command corrupting a byte** (fact-sheet §10) | A cycle-accurate, mid-command-interruption artifact; M22's atomic model has no "mid-command" moment for the 10 atomic commands to observe register changes during, and the 3 transfer commands only pause at well-defined, event-driven transfer boundaries (not raster/cycle boundaries). | M23 |
| **`/WAIT` generator (R#25 WTE)** | Fact-sheet §7/§9: "Not wired on HB-F1XV — irrelevant for HB-F1XV emulation but must exist as a register bit." Already stored (M14's R#25 handling); no CPU-stall behavior to add since this machine never asserts it. | N/A — this machine never wires `/WAIT` |
| **A real SDL3 presentation surface** | Backlog C9 (SDL3 frontend), still OPEN, not yet built (M26). `FrameBuffer` remains a pure in-memory contract. | M26 |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M22-1 (the M14 two-write register-latch protocol, `#99`/`#9B`, is ALREADY forward-compatible
  with R#32-46 — no port-protocol change needed).** Read `src/devices/video/v9958_vdp.cpp` this
  cycle: `port1_write`'s register-write branch calls `change_register(value & 0x3F, data_latch_)`
  and `indirect_write` calls `change_register(reg_nr & 0x3F, value)` — BOTH already mask to 6 bits
  (0-63), which already includes R#32-46 (indices 32-46 fit inside 0x3F). The ONLY change needed is
  inside `change_register()` itself: today `if (reg >= kNumControlRegs) { return; }` (`kNumControlRegs
  = 32`) silently drops writes to R#32-46. M22 replaces this drop with a dispatch to the new command
  engine (`if (reg >= 32) { if (reg < 47) cmd_engine_.write_register(reg - 32, value); return; }`),
  grounded 1:1 against `references/openmsx-21.0/src/video/VDP.cc:1020-1033`'s own
  `changeRegister()` (`if (reg >= 32) { ...; if (reg < 47) cmdEngine->setCmdReg(reg - 32, val,
  time); return; }`). *Verify:* a unit test writes R#46 (CMD) via BOTH the `#99` two-write path and
  the `#9B` indirect-R#17 path and confirms both reach the command engine identically.
- **A-M22-2 (the reference's own command-register storage lives OUTSIDE `VDP::controlRegs`, in
  `VDPCmdEngine`'s own fields — mirrored here as `VdpCommandEngine` owning R#32-46 storage, NOT
  `V9958Vdp::control_regs_` growing to 47 entries).** `VDPCmdEngine.hh:264-267`: `unsigned SX{0},
  SY{0}, DX{0}, DY{0}, NX{0}, NY{0}; unsigned ASX{0}, ADX{0}, ANX{0}; uint8_t COL{0}, ARG{0},
  CMD{0};` — a completely separate register file from `VDP::controlRegs[0..31]`. This project
  mirrors that separation: a NEW `VdpCommandEngine` class (§2.1) owns SX/SY/DX/DY/NX/NY/COL/ARG/CMD
  (+ temp ASX/ADX/ANX), NOT an expansion of `V9958Vdp::control_regs_`. *Verify:* `V9958Vdp::
  kNumControlRegs` stays 32 (unchanged); no existing R#0-31 test is touched.
- **A-M22-3 (SX/SY/DX/DY/NX/NY bit widths, independently re-derived from `setCmdReg`).**
  `VDPCmdEngine.cc:1808-1874`: SX is 9-bit (`(SX&0x100)|value`, `(value&0x01)<<8`); SY/DY/NY are
  10-bit (`(value&0x03)<<8`); DX is 9-bit (mirrors SX); NX is 10-bit (comment at `:1841-1842`
  explicitly notes "current implementation needs 10 bits...otherwise texts in UR are screwed" even
  though the datasheet nominally documents fewer). *Verify:* unit test writes the high-byte
  registers with out-of-range bit patterns and confirms exactly the masked bit width is retained
  (e.g. SX high byte only bit0 sticks, SY/DY/NY high bytes only bits1-0 stick).
- **A-M22-4 (only 5 of the 13 commands consult the low-nibble logical-op code — a genuine risk,
  R-M22-1).** `VDPCmdEngine.cc:1940-2030`'s `executeCommand` dispatch keys SOLELY on `CMD>>4` to
  select `startPoint`/`startSrch`/`startLmcm`/`startHmmv`/`startHmmm`/`startYmmm`/`startHmmc`
  (no `LogOp` template parameter at all for these 7) versus `startPset`/`startLine`/`startLmmv`/
  `startLmmm`/`startLmmc` (which DO carry a `LogOp` and whose `sync2()` dispatch additionally keys
  on the full low nibble, `:2126-2185` for PSET's 16 sub-cases). Concretely: POINT, SRCH, LMCM,
  HMMV, HMMM, YMMM, HMMC (the "high-speed"/read-only commands) ALWAYS perform a plain/implicit
  copy regardless of the CMD register's low 4 bits — the low nibble is simply never consulted for
  them. A naive implementation might wrongly apply a logical-op table to all 13 commands.
  *Verify:* unit test sets a non-zero, non-IMP low nibble on e.g. HMMV and confirms the fill still
  behaves as a plain overwrite (not AND/OR/XOR/NOT), while the SAME low-nibble value on LMMV
  produces the documented logical-op result.
- **A-M22-5 (CMD>>4 command-code table, independently re-derived from the `executeCommand` switch
  keys, not transcribed from memory or the fact-sheet alone).** `VDPCmdEngine.cc:1963-2028`:
  `(scrMode<<4)|(CMD>>4)` keyed dispatch — recomputing the low-nibble-independent part: `CMD>>4 ∈
  {0,1,2,3}` → ABRT (4 reserved/no-op code-points all alias to the same abort behavior);
  `4`→POINT; `5`→PSET; `6`→SRCH; `7`→LINE; `8`→LMMV; `9`→LMMM; `0xA`→LMCM; `0xB`→LMMC; `0xC`→HMMV;
  `0xD`→HMMM; `0xE`→YMMM; `0xF`→HMMC. Cross-checked against the fact-sheet §8 table (which lists
  `HMMC=1111, YMMM=1110, HMMM=1101, HMMV=1100, LMMC=1011, LMCM=1010, LMMM=1001, LMMV=1000,
  LINE=0111, SRCH=0110, PSET=0101, POINT=0100, STOP=0000`) — MATCHES exactly (both independently
  agree, source-code-derived and fact-sheet-derived). *Verify:* unit test drives each of the 13
  CMD>>4 values through `write_register(14, cmd_byte)` and asserts the correct command's
  observable effect (a VRAM write, a COL read-back, or CE clearing immediately for ABRT).
- **A-M22-6 (scrMode determination — independently re-derived, governs BOTH which coordinate
  address formula AND which commands are legal).** `VDPCmdEngine.cc:1902-1938`
  (`updateDisplayMode`): `scrMode = 0` for base GRAPHIC4, `1` for GRAPHIC5, `2` for GRAPHIC6, `3`
  for GRAPHIC7 (this covers YJK/YJK+YAE too, since `mode.getBase()` strips the YJK/YAE bits —
  independently confirmed against this project's OWN `VdpModeState.base` field, which already
  encodes GRAPHIC7's base as `0x1C` regardless of YJK/YAE per `vdp_mode.h:101-104`); `scrMode = 4`
  ("like GRAPHIC7, but non-planar") for any OTHER mode when R#25 CMD bit is set; `scrMode = -1` (no
  commands at all — writing R#46 immediately calls `commandDone()`, i.e. behaves as ABRT) when
  CMD bit is clear and the mode isn't G4-G7. *Verify:* unit test switches through TEXT1/GRAPHIC1/
  GRAPHIC3/GRAPHIC4-7/YJK modes with R#25 CMD bit both clear and set, confirming command
  legality/address-formula selection matches this table exactly, including the -1 (silently
  no-op) case.
- **A-M22-7 (the command engine's coordinate-to-address formulas are a DIFFERENT function domain
  from D7's existing CPU-port rotate-right-by-1 — NOT literally reused — but independently
  verified mathematically CONSISTENT for the two shared modes, G6/G7; this closes D7 in full, §1.5
  has the full derivation).** See §1.5.
- **A-M22-8 (HB-F1XV never has extended VRAM — MXS/MXD/R#45-bit6 are stored but their "select
  expansion bank" effect is a non-goal, consistent with the established M14/M21 128 KB-fixed
  scope boundary).** `VDPCmdEngine.cc:175-410`'s every `addressOf(x,y,extVRAM)` has an `if
  (!extVRAM) [[likely]] { ... } else { ... | 0x20000 }` branch; `VDP.cc:1025-1027`'s
  `cpuExtendedVram = (val & 0x40) != 0` (R#45 bit6, MXC) is a CPU-port-only concern, separate from
  the command engine's own MXS/MXD (ARG register bits). M22 implements ONLY the `!extVRAM` branch
  of every `addressOf` formula; MXS/MXD/R#45-bit6 are stored (register-contract completeness) but
  always treated as if false. *Verify:* unit test sets MXS/MXD=1 (ARG bits 0x10/0x20) and confirms
  command addressing is UNCHANGED (never reaches for a `0x20000`+ address, which is out of this
  VRAM's 128 KB range and would silently wrap/corrupt if attempted).
- **A-M22-9 (the 1-pixel vertical sprite shift is the SAME mechanism as `SpriteChecker::getSprites`'s
  own `line--` "checked one line earlier than displayed" convention — independently re-derived,
  not two separate effects, R-M22-2).** `SpriteChecker.hh:255-265`: `getSprites(line)` decrements
  `line` before indexing `spriteBuffer`, with the comment "Compensate for the fact sprites are
  checked one line earlier than they are displayed," and explicitly returns EMPTY for `line < 0`
  ("Is there ever a sprite on absolute line 0? ... it is never displayed"). Cross-referencing the
  fact-sheet §6's two DISTINCT-sounding pitfalls — "(b) sprite data for line N is fetched during
  line N-1" and "(c) a sprite at Y=0 appears on the 2nd displayed line" — independently confirmed
  this cycle to be the SAME underlying off-by-one viewed from two angles (the fetch-ahead
  scheduling artifact (b) is the CAUSE; the "Y=0 shows at output line 1" (c) is its OBSERVABLE
  effect), not two separate mechanisms to model. Consequence for this project's frame-wide,
  non-cycle-scheduled sprite engine: for OUTPUT line `L` (0-based active-display line, matching
  `VdpFrameRenderer`'s existing line-index convention), the sprite-visibility test must use
  `sprite_line_in_pattern = ((L - 1) + R23 - Y) & 0xFF`, and **output line 0 always has zero
  sprites, by construction** (there is no "check line -1"). *Verify:* unit test places a sprite at
  Y=0 and confirms it is NOT visible on output line 0 but IS visible starting at output line 1;
  a second test confirms output line 0 is unconditionally sprite-free regardless of any sprite's Y
  value.
- **A-M22-10 (EC/CC/IC bit positions, independently re-derived — NOT assumed from the task's or
  fact-sheet's prose alone, a genuine risk R-M22-3).** `SpriteChecker.cc:150,320,370` (EC):
  `if (colorAttrib & 0x80) sip.x -= 32;` → **EC = bit7 (0x80)**, applies in BOTH sprite modes 1 and
  2. `SpriteConverter.hh:159,179` (CC, the source's OWN comment/variable naming, not inferred):
  `if ((visibleSprites[first].colorAttrib & 0x40) == 0)` / `if (!(info2.colorAttrib & 0x40))
  break;`, both explicitly discussing "CC" → **CC = bit6 (0x40)**. By elimination against
  `SpriteChecker.cc:438,446`'s `colorAttrib1 & 0x60` collision-exclusion mask (`0x60 = 0x40|0x20`)
  → **IC = bit5 (0x20)**. Color index = bits3-0 (`colorAttrib & 0x0F`); bit4 unused. *Verify:* unit
  test sets each bit individually and confirms exactly the documented effect (EC: x-shift only; CC:
  merge-chain + collision-exclusion; IC: collision-exclusion only, see A-M22-11).
- **A-M22-11 (IC does NOT suppress pixel rendering, despite its "Invisible Code" name — it ONLY
  excludes the sprite from COLLISION detection; a genuine, easy-to-mis-implement risk, R-M22-4).**
  Direct read of `SpriteConverter.hh:144-205` (`drawMode2`, the actual pixel-compositing function):
  the drawing loop never tests bit `0x20` (IC) at all — it tests bit `0x40` (CC, for the
  merge-chain) and the color-index-0/transparency rule, but nothing gates a pixel's VISIBILITY on
  IC. Meanwhile `SpriteChecker.cc:438,446` (`checkSprites2`, the collision-detection function)
  explicitly excludes `colorAttrib & 0x60` (CC-or-IC) sprites from collision testing. Conclusion:
  **IC=1 sprites are still fully drawn/visible on screen; they are merely never counted toward a
  collision.** A naive implementation, going by the name, would incorrectly ALSO hide IC=1
  sprites from the rendered output. *Verify:* dedicated unit test places two overlapping sprites,
  one with IC=1, confirms (a) the IC=1 sprite's pixels ARE drawn in `FrameBuffer` output, and (b)
  no collision (S#0 bit5 C) is raised for the overlap.
- **A-M22-12 (color-0 sprite-pixel transparency is conditioned on R#8 TP, contradicting the
  fact-sheet's OWN blanket claim — ground truth is the source, a genuine risk R-M22-5, exactly the
  kind of fact-sheet-vs-source discrepancy this project's discipline requires catching).** The
  fact-sheet §6 states: "Colour-0 pixels in a sprite pattern are always transparent regardless of
  TP (a documented erratum in some guides that claim TP affects them)." But the ACTUAL reference
  source, read directly this cycle, `SpriteConverter.hh:110-114` (mode 1) and `:170-171` (mode 2):
  `Pixel colIndex = si.colorAttrib & 0x0F; if (colIndex == 0 && transparency) continue;` —
  `transparency` IS the R#8 TP setting (`SpriteConverter::setTransparency`, driven by
  `vdp.getTransparency()`, `VDP.hh:186-191`, `(controlRegs[8]&0x20)==0`). This means color-0
  sprite pixels are skipped (background shows through) ONLY when TP is enabled; when TP is
  DISABLED, color-0 sprite pixels ARE drawn using palette entry 0. This directly CONTRADICTS the
  fact-sheet's blanket "always transparent regardless of TP" prose. Per this project's standing
  discipline (ground truth is the read source, not a summarizing fact-sheet), M22 implements the
  CODE behavior (conditional on TP), not the fact-sheet's stronger claim. *Verify:* two unit tests:
  TP enabled + color-0 sprite pixel → background shows through; TP disabled + color-0 sprite pixel
  → palette entry 0 is drawn (not background).
- **A-M22-13 (S#6 bit1's documented "EO" (even/odd) copy is honestly UNIMPLEMENTED even in the
  reference itself — inherit the SAME disclosed gap, do not fabricate a "more correct" model,
  mirrors the M21 A-M21-7 precedent for exactly this kind of honest-hedge situation).**
  `SpriteChecker.hh:378-383`'s own doc comment: "Bit 9 [of the Y coord] contains EO, I guess that's
  a copy of the even/odd flag... [TODO] not yet implemented." Confirmed via
  `VDP.cc:948-949`: `case 6: return uint8_t(spriteChecker->getCollisionY(time) >> 8) | 0xFC;` — no
  EO bit is OR'd in; the mask `0xFC` simply forces bits7-2 to 1, leaving bits1-0 as `(collisionY>>8)
  & 0x03`, which for a 9-bit-max `collisionY` is always 0 for bit1. M22 reproduces this EXACT,
  disclosed-incomplete behavior (S#6 bit1 always 0), not an independently "fixed" EO-aware
  implementation beyond what even openMSX itself models. *Verify:* unit test confirms S#6 bit1 is
  always 0 regardless of which `Field` (if any Field concept is even threaded through) is active.
- **A-M22-14 (S#0 read clears ONLY the F/5S/C bits, NOT the 5th-sprite-number field — a genuine,
  easy-to-over-clear risk, R-M22-6).** `SpriteChecker.hh:104-110` (`resetStatus`):
  `vdp.setSpriteStatus(vdp.getStatusReg0() & 0x1F)` — masks OFF bits 6 (5S) and 5 (C) only, KEEPING
  bits 4-0 (the 5th/9th-sprite number) unchanged until the next frame's sprite check overwrites it.
  Separately, `VDP.cc:965-969` clears bit7 (F) via `statusReg0 &= ~0x80`. So a single S#0 read
  clears bits 7/6/5 but leaves bits 4-0 stale. *Verify:* unit test triggers a 5th-sprite condition,
  reads S#0 twice, and confirms the sprite-number field's value survives the first read unchanged
  (only bits 7/6/5 clear).
- **A-M22-15 (sprite mode 2's per-line color/attribute table sits at a FIXED +512-byte offset
  within the SAME sprite-attribute-table window as the Y/X/pattern table, not a separately
  register-based address — independently re-derived).** `SpriteChecker.cc:284-285,329-330`:
  `vram.spriteAttribTable.getReadAreaPlanar<32*4>(512)` / `getReadArea<32*4>(512)` for the Y/pattern
  table, while the color/attribute reads use `colorIndex = (~0u<<10) | (sprite*16+spriteLine)`
  (an "AND-mask forced-1" address relative to the SAME table's base, offset 0). This matches the
  fact-sheet §6's "the colour table (512 bytes...) sits 512 bytes below the attribute table."
  *Verify:* unit test computes both table addresses from the SAME R#5/R#11-derived base
  (`(R11<<15)|(R5<<7)`, independently re-derived from `VDP.hh:266-268`) and confirms the +512
  relationship directly, not two independently-configured bases.
- **A-M22-16 (sprite pattern/attribute table base formulas — independently re-derived, NOT
  transcribed).** `VDP.hh:262-268`: `getSpritePatternTableBase() = controlRegs[6] << 11`;
  `getSpriteAttributeTableBase() = (controlRegs[11] << 15) | (controlRegs[5] << 7)`. Same
  disclosed simplification this project already carries from M21 (A-M21's own note): the
  "forced-1 low bits" hardware AND-mask nuance (`VRAMWindow` masking) is NOT reproduced; tests use
  canonical/valid base register values, consistent with the established VRAM-addressing depth
  level. *Verify:* unit test drives R#5/R#6/R#11 through representative values and confirms the
  base addresses match these formulas exactly.
- **A-M22-17 (command execution model — the two required architectural resolutions).** See §1.4.

### 1.4 The two required architectural resolutions

#### Resolution 1 — Sprite compositing integration point: EXTEND `VdpFrameRenderer::render_frame()`

**Decision: sprite pixel compositing is added AS PART OF `VdpFrameRenderer::render_frame()`'s
existing per-line pipeline** (an internal `composite_sprites(line, field, out)` step called after
`dispatch_content()` populates the background row, mirroring `render_line()`'s existing
post-background border-mask step), returning a SINGLE, already-composited `FrameBuffer` — NOT a
separate, consumer-visible "sprite plane" output.

**Grounding, independently read this cycle, not assumed:** `references/openmsx-21.0/src/video/
SpriteConverter.hh:99-132` (`drawMode1`) and `:144-205` (`drawMode2`) take a `std::span<Pixel>
pixelPtr` parameter and write DIRECTLY into it via overdraw (`*p = color;` / `pixelPtr[x] = pix;`)
— this is the SAME per-line pixel buffer the background rasterizer (`CharacterConverter`/
`BitmapConverter`, called immediately before, in the real renderer's `PixelRenderer`-family
drawDisplay loop) already wrote for that scanline. There is NO separate "sprite surface" anywhere
in the reference architecture; sprites are a pure overdraw pass onto the SAME output buffer, using
the SAME palette. This is a direct, load-bearing architectural fact (not an inference from
"convenience"): the reference literally has no other place a sprite pixel could go.

**Why this is the right call for THIS project, not merely the easy option:**
1. It matches the reference's own architecture exactly (no invented abstraction).
2. `FrameBuffer` (M21) is already this project's single canonical "what appears on screen"
   contract; a second, separate sprite-plane output type would force every future consumer
   (SDL3, M26) to re-implement compositing (background+sprite layering, including the CC
   color-cascade merge and priority-overdraw ordering) itself — duplicating logic this project
   already centralizes in `VdpFrameRenderer`.
3. `VdpFrameRenderer` remains a PURE, read-only, on-demand consumer (unchanged architectural
   character from M21) — it never mutates `V9958Vdp` state; it only gains a new READ dependency
   (querying the sprite engine's per-line visible-sprite list, §2.2).

**Important separation preserved:** the CPU-VISIBLE STATUS side effects of sprite checking (S#0
5S/C, S#3-S#6 collision X/Y) are **independent of whether any frame is ever rendered** — real
software polls these via `IN (#99)` without ever needing a pixel output. These MUST be computed by
`V9958Vdp` itself (via a new, internally-owned sprite-check component, §2.2), driven by the
EXISTING `on_vsync()` frame-boundary hook (mirroring M21's blink-countdown precedent exactly — no
new clock consumer), NOT lazily deferred until `VdpFrameRenderer::render_frame()` happens to be
called. `VdpFrameRenderer`'s sprite-pixel compositing pass then QUERIES this SAME, already-computed
per-line visible-sprite buffer (a read-only accessor) rather than re-deriving it — avoiding a
second, drift-prone implementation of the sprite-selection algorithm.

#### Resolution 2 — Command engine execution model: HYBRID (atomic for 10 commands, event-driven stateful for 3)

**Decision:**
- **ABRT/STOP, POINT, PSET, SRCH, LINE, LMMV, LMMM, HMMV, HMMM, YMMM (10 commands)** execute
  **ATOMICALLY**: the entire operation (all NX*NY pixels/bytes) completes synchronously within the
  single call that writes R#46 (CMD). CE (S#2 bit0) is observably 0 immediately after that call
  returns — a "degenerate but technically correct" handshake, since real software's poll loop
  (`IF VDP(-2) AND 1 THEN loop`) sees CE already cleared on its very first check. BD (S#2 bit4,
  SRCH's border-detected flag) persists as REAL state until an S#9 read clears it (matching
  `resetBD()` — this is NOT atomically cleared, since real software depends on reading it after
  the fact).
- **LMCM, LMMC, HMMC (3 commands)** require a genuine, EXPLICIT, multi-step state machine —
  mirroring this project's own FDC precedent (WD2793 DRQ/INTRQ, M16): CE stays 1 across the WHOLE
  transfer; TR toggles 0→1 to signal "ready for the next byte/pixel," is cleared transiently at the
  moment of an actual transfer step, and the command completes (CE→0) only after NX*NY individual
  CPU-port interactions (writes to R#44/COL for CPU→VRAM commands, reads of S#7 for VRAM→CPU) have
  each been serviced. This is **event-driven** (advanced by real I/O port accesses the CPU actually
  performs), **never wall-clock/cycle-scheduled** — identical in spirit to how the FDC's own
  read/write-triggered state transitions work, and requires ZERO new clock-consumer axis.

**Justification, weighed explicitly against the real S#2 TR/CE semantics and this project's own
established precedent (not picked for convenience):**

1. **Why NOT purely atomic for all 13 commands.** Directly re-reading `VDPCmdEngine.cc:1291-1350`
   (`startLmmc`/`executeLmmc`) and `:1720-1771` (`startHmmc`/`executeHmmc`): both gate the ACTUAL
   VRAM write on a `transfer` boolean that only becomes true when the CPU writes R#44 (COL) AGAIN
   AFTER the command has started (`setCmdReg` case `0x0C`: `COL = value; ...; transfer = true;`).
   Real software's control flow for LMMC/HMMC is: write R#46 to start → **loop**: poll S#2 TR,
   write R#44 with the next pixel/byte's data, (repeat NX*NY times) → poll CE=0. Similarly
   `startLmcm`/`executeLmcm` (`:1240-1289`) requires the CPU to READ S#7 (`readColor`/`resetColor`)
   once per pixel to both retrieve the transferred value AND arm the next `transfer`. **This is a
   REAL, load-bearing, software-visible multi-step protocol spanning MULTIPLE, SEPARATE I/O
   accesses** — a single atomic call at R#46-write time cannot honor it: there is no way to
   "atomically" perform an operation that depends on future CPU actions that haven't happened yet
   (the pixel/byte DATA for LMMC/HMMC literally arrives via SEPARATE, subsequent port writes; for
   LMCM the CPU must physically read each transferred pixel via S#7 before the NEXT one is
   produced). Collapsing this to "instant completion on R#46 write" would silently DROP every
   pixel/byte after the first (or require fabricating CPU input the CPU never supplied) — an
   outright correctness defect, not merely a timing simplification.
2. **Why this DOES match this project's own established precedent, not a bespoke exception.** The
   FDC (M16, `src/devices/fdc/wd2793.*`) already establishes the exact template this project uses
   for "a device with a real busy/ready handshake that real software polls before/between
   multi-step data-transfer operations": an explicit, deterministic, NON-wall-clock state machine
   (INTRQ/DRQ) advanced by actual CPU register reads/writes, not simulated elapsed time. LMCM/LMMC/
   HMMC are the VDP's structural analog of the FDC's sector read/write data-transfer loop; the
   OTHER 10 commands have no such per-step CPU dependency (their entire NX*NY operation only
   touches VRAM/VDP-internal registers, exactly like the FDC's own Type I "seek" commands, which
   this project's FDC model already treats as completing without a multi-step data-phase state
   machine).
3. **Why atomic IS the right, honestly-scoped choice for the OTHER 10 commands, not a shortcut.**
   Since D4 (cycle-accurate VDP timing, including the command engine's REAL per-pixel/per-line
   VDP-cycle cost, fact-sheet §8) is explicitly, separately scoped to M23, any attempt to model
   "CE stays 1 for N cycles then clears" for these 10 commands would require EITHER (a) fabricating
   an arbitrary, non-grounded elapsed-time model this project doesn't otherwise have for the VDP, or
   (b) prematurely importing D4's real access-slot/cycle-cost tables before M23 exists to própertly
   own that entire subsystem — both would be scope creep and/or fabrication. Atomic execution is the
   accurate reflection of "this milestone does not model VDP cycle timing," matching M21's own
   frozen-snapshot philosophy precisely, and it is STILL technically correct: real software's poll
   loop for these commands ALWAYS eventually observes CE=0 (this model just makes that observable
   on the very next status read, rather than after N real VDP cycles) — no software-visible
   CORRECTNESS property is violated, only the exact WAIT DURATION (a D4 concern) differs.
4. **BD persists as real, non-atomic state deliberately.** Unlike CE (execution-in-progress), BD
   (border/search-target found) is a RESULT flag real software reads well after SRCH completes
   (`resetBD()` is only invoked on an explicit S#9 read, `VDP.cc:978-982`) — collapsing it to
   "always 0 after the atomic call" would erase a real, useful, CPU-visible result. M22 keeps BD as
   persistent status, cleared only on the documented S#9-read trigger.

### 1.5 The D7 closing-piece grounding (required by the task; resolved by direct source reading, correcting the carried-forward M21 assumption where warranted)

The task explicitly warns against assuming the command engine reuses D7's existing rotate-right-by-1
CPU-port formula (`V9958Vdp::effective_address()`) without confirming. Direct, independent reading
of `references/openmsx-21.0/src/video/VDPCmdEngine.cc:155-410` (the `Graphic4Mode`/`Graphic5Mode`/
`Graphic6Mode`/`Graphic7Mode`/`NonBitmapMode` structs' `addressOf(x, y, extVRAM)` static functions)
CONFIRMS the warning was warranted: **the command engine does NOT call `effective_address()` at
all, and cannot — that function's domain is a linear 17-bit CPU-port pointer address with no X/Y
concept whatsoever, whereas the command engine addresses VRAM directly in X/Y pixel-coordinate
space.** These are genuinely different functions operating on different inputs; M22 must — and
does — implement NEW, dedicated coordinate-based address-resolution functions.

**However**, independently re-deriving and hand-computing both formulas this cycle shows they ARE
mathematically CONSISTENT with (not merely coincidentally similar to) the SAME physical-VRAM-bank
placement rule M21 already established for G6/G7 (an important distinction: same underlying
hardware interleave, different-domain expression of it — not "the same formula," but not an
unrelated one either):

- **`Graphic7Mode::addressOf(x, y, false)`** (`VDPCmdEngine.cc:333-341`) = `((x & 1) << 16) |
  ((y & 511) << 7) | ((x & 255) >> 1)`. Setting `col = x` (0-255) and treating `y` (0-511) as
  DIRECTLY encoding `page*256 + line` (see below): bank selection = `x & 1` (matches M21's
  "even/odd LOGICAL byte" bank rule at byte granularity, since G7 is 1 byte/pixel); within-bank
  offset = `y*128 + (x>>1)` = `page*32768 + line*128 + (x>>1)`. Independently re-derived from
  M21's OWN `planar_row_spans(row_base = page*0x10000 + line*256, 256)` (`vdp_frame_renderer.cpp:
  32-42`): `bank0_base = row_base >> 1 = page*32768 + line*128`, and a column's bank-half-index is
  `col >> 1` — **algebraically IDENTICAL** to the command engine's own `y*128 + (x>>1)` once `y` is
  understood as `page*256+line`. Hand-verified for two concrete cases: `(x=0,y=0)` → command-engine
  formula gives address `0` (bank0, offset0); M21's model for `page=0,line=0,col=0` gives
  `bank0_base+0 = 0`. Match. `(x=1,y=256)` (page1, line0, col1, odd column → bank1) → command-engine
  formula gives `((1&1)<<16)|((256&511)<<7)|((1&255)>>1)` = `0x10000 | (256<<7) | 0` = `0x10000 +
  32768 + 0 = 0x18000`; M21's model for `page=1,line=0` gives `bank1_base = 0x10000 +
  (page*0x10000+line*256)>>1 = 0x10000 + (0x10000>>1) = 0x10000+0x8000 = 0x18000`. Match.
- **`Graphic6Mode::addressOf(x, y, false)`** (`VDPCmdEngine.cc:281-289`) = `((x & 2) << 15) |
  ((y & 511) << 7) | ((x & 511) >> 2)`. Here the bank-selection bit is `x & 2` (bit1 of `x`, NOT
  bit0) — because G6 packs 2 pixels/byte, so the relevant "byte index" is `i = x >> 1`, and it is
  `i`'s OWN LSB (`i & 1`, algebraically `(x>>1)&1`, i.e. bit1 of `x`) that determines the bank —
  matching M21's byte-level bank-split rule exactly once re-expressed in byte-index terms. Within-
  bank offset = `y*128 + (x>>2)` = `page*32768+line*128+(i>>1)`, again algebraically identical to
  `planar_row_spans`'s own `bank0_base+i>>1` for 256-byte rows. Hand-verified similarly (byte index
  `i=0` at page0/line0 → address 0 in both models; `i=1` at page0/line0 → command-engine gives
  `((1<<1)&2)... ` — recomputed: for byte index `i=1`⇒`x∈{2,3}`; taking `x=2`: `((2&2)<<15)|
  (0<<7)|((2&511)>>2)` = `0x10000|0|0` = `0x10000`, matching M21's `bank1_base+0 = 0x10000+0`).
- **The genuinely different, worth-flagging nuance (independently discovered this cycle, a real
  risk, R-M22-7): the command engine addresses BOTH pages DIRECTLY through the Y-coordinate's own
  range — `y & 511` for G6/G7 (2 pages × 256 lines = 512), `y & 1023` for G4/G5
  (`Graphic4Mode`/`Graphic5Mode::addressOf`, `:175-183/227-235`, 4 pages × 256 lines = 1024) —
  COMPLETELY BYPASSING R#2's display-page-select bits.** This is DIFFERENT from the DISPLAY path
  (M21's `resolve_bitmap_page()`, which selects a page via R#2 bits 5-6, since the raster Y
  coordinate alone only spans one page's worth of lines and needs an explicit register to pick
  WHICH physical bank is shown). **R#2 has NO effect whatsoever on where VDP commands read or
  write** — a command's DY/SY register value alone determines which of the 2 (or 4) physical pages
  it touches. A naive implementation might incorrectly gate command addressing on R#2's page bits
  (copying the display-path's own page-resolution logic) — this would be a genuine defect.
  *Verify:* dedicated unit test sets R#2's page-select bits to page 0, then issues an HMMV command
  with `DY=300` (which is `page 1, line 44` since `300 = 256+44`) and confirms the fill lands in
  PHYSICAL BANK 1 (0x10000+...) regardless of R#2's setting — i.e., command addressing is
  independently confirmed NOT to consult R#2 at all.
- **`NonBitmapMode::addressOf(x, y, false)`** (`VDPCmdEngine.cc:383-391`) = `((y & 511) << 8) |
  (x & 255)` — a flat, non-planar, linear formula (no bank split at all), used when R#25's CMD bit
  enables commands in non-bitmap (text/tile) modes. Maps directly onto this project's existing flat
  `VdpVram` store with zero transform, the simplest of the five formulas.
- **`Graphic4Mode`/`Graphic5Mode::addressOf`** (`:175-183/227-235`) are already non-planar (no bank
  split), directly analogous to M21's existing `bitmap_row_base_nonplanar` but again addressed
  DIRECTLY via the full `y & 1023` range rather than via R#2 page bits (same R#2-bypass nuance as
  above, generalized to 4 pages).

**Resolution: D7 closes IN FULL this cycle.** M22 implements five new, dedicated, coordinate-based
address-resolution functions (§2.1) — grounded directly in the five `Graphic*Mode`/`NonBitmapMode::
addressOf` functions, NOT a call to `effective_address()` — independently hand-verified for
physical-bank-placement consistency with M21's existing rotate-right-by-1/`planar_row_spans` model
for the two shared modes (G6/G7), and explicitly, correctly modeling the R#2-bypass nuance for all
four bitmap-family formulas. This corrects (rather than blindly confirms) the carried-forward M21
assumption: it is **consistent with, but not literally the same function as**, the existing D7
CPU-port piece — an important, precise distinction now recorded for the ledger.

---

## 2. Spec Summary

### 2.1 `src/` placement (per `src/CLAUDE.md`)

New files, all in the EXISTING `src/devices/video/` family (mirrors the M14/M21 precedent — the
whole V9958 subsystem, contract + rendering depth + sprites + command engine, stays together; no
new top-level device family is warranted):

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/video/sprite_engine.h` / `.cpp` (**new**) | `class SpriteEngine` — owned INSIDE `V9958Vdp` (private member, mirrors `blink_countdown_`/`blink_state_`'s ownership style). `void recompute_frame(const VdpVram& vram, std::span<const std::uint8_t, 32> control_regs, const VdpModeState& mode, int height);` (frame-boundary-triggered, called from `V9958Vdp::on_vsync()`); `std::span<const VisibleSprite> visible_sprites(int output_line) const;` (for `VdpFrameRenderer`'s compositing pass, §2.2); status accessors: `std::uint8_t status_bits() const;` (S#0 bits6-0, the 5S/C/sprite-number composite), `int collision_x() const;`, `int collision_y() const;`; `void reset_status();` (S#0-read side effect, A-M22-14's `&0x1F` semantics), `void reset_collision();` (collision X/Y read-side reset). `struct VisibleSprite { std::uint32_t pattern; int x; std::uint8_t color_attrib; };` (mirrors `SpriteChecker::SpriteInfo`). | `SpriteChecker.hh/.cc` (behavior reference; independently re-expressed, never copied) |
| `src/devices/video/vdp_command_address.h` (**new**, header-only, pure functions) | Five coordinate-to-address functions, independently re-derived per §1.5: `graphic4_command_address(x,y)`, `graphic5_command_address(x,y)`, `graphic6_command_address(x,y)` (planar), `graphic7_command_address(x,y)` (planar; also used for YJK/YJK+YAE, which share GRAPHIC7's `scrMode`), `non_bitmap_command_address(x,y)` (flat, R#25-CMD-bit path). | `VDPCmdEngine.cc:175-410` (`Graphic4Mode`/`Graphic5Mode`/`Graphic6Mode`/`Graphic7Mode`/`NonBitmapMode::addressOf`, `!extVRAM` branch only, A-M22-8) |
| `src/devices/video/vdp_command_engine.h` / `.cpp` (**new**) | `class VdpCommandEngine` — owned INSIDE `V9958Vdp` (private member, constructed with a reference to the SAME `VdpVram& vram_` `V9958Vdp` owns — command-engine writes land directly in the shared VRAM store, so `VdpFrameRenderer`'s EXISTING background-rendering code paths see command-engine-written content with ZERO changes, a low-risk integration strength). `void write_register(int index, std::uint8_t value);` (index 0-14 = R#32-46, A-M22-1/A-M22-2); `std::uint8_t read_register(int index) const;` (debug peek, mirrors `peekCmdReg`); `void notify_mode_change(const VdpModeState& mode, bool cmd_bit);` (scrMode recompute, A-M22-6, called from `V9958Vdp::recompute_mode()`); status: `bool tr() const;`, `bool ce() const;`, `bool bd() const;`, `std::uint8_t color() const;` + `void on_color_register_read();` (S#7-read side effect, arms the next LMCM transfer, mirrors `resetColor()`), `void on_border_x_register_read();` (S#9-read side effect, mirrors `resetBD()`), `unsigned border_x() const;` (ASX, for S#8/S#9). | `VDPCmdEngine.hh/.cc` (behavior reference; independently re-expressed, never copied) |

Edits to already-shipped M21/M14 files (additive; flagged explicitly):

- `src/devices/video/v9958_vdp.h` / `.cpp`:
  - New private members `VdpCommandEngine cmd_engine_{vram_};` and `SpriteEngine sprite_engine_{};`
    (declared after `vram_`, mirroring the existing member-ordering discipline).
  - `change_register()`: the `if (reg >= kNumControlRegs) { return; }` early-return becomes a
    dispatch to `cmd_engine_.write_register(reg - 32, value)` for `reg` in `[32, 47)` (A-M22-1).
  - `recompute_mode()`: additionally calls `cmd_engine_.notify_mode_change(mode_, (control_regs_[25]
    & 0x40) != 0)` (R#25 CMD bit, A-M22-6) whenever the recomputed mode or CMD bit changes.
  - `on_vsync()`: additionally calls `sprite_engine_.recompute_frame(vram_, control_regs_, mode_,
    <active-display height>)` — mirrors the EXISTING blink-countdown call site exactly (frame-
    boundary-triggered, no new clock consumer).
  - `peek_status_register()`/`read_status()`: replace the M14/M21 STUB values for cases 3-9 (and
    extend case 0/2's composition) with LIVE values sourced from `sprite_engine_`/`cmd_engine_`,
    following the EXACT mask structure already scaffolded in M14 (§2.4 below has the full mapping).
  - New const accessors: `const SpriteEngine& sprite_engine() const;` (for `VdpFrameRenderer` and
    tests), `const VdpCommandEngine& cmd_engine() const;` (for tests/debug).
- `src/devices/video/vdp_frame_renderer.h` / `.cpp`:
  - New private method `composite_sprites(int line, Field field, std::span<std::uint16_t> out)
    const;`, called from `render_line()` immediately after `dispatch_content()` and BEFORE the
    existing border-mask step (mirrors the reference's own "background then sprite overdraw" order,
    §1.4 Resolution 1) — queries `vdp_->sprite_engine().visible_sprites(line)`.
  - Implements the mode1 (`drawMode1`-equivalent, simple overdraw) and mode2 (`drawMode2`-
    equivalent, CC color-cascade merge, A-M22-10/11) compositing rules, plus the R#8-TP-conditioned
    color-0 transparency rule (A-M22-12) and the EC x-32 shift (already baked into `VisibleSprite.x`
    by `SpriteEngine`, mirroring the reference's own `sip.x -= 32` at check time, not render time).
- `src/devices/video/vdp_mode.h`: no changes needed — `VdpModeState.base` already carries
  everything `scrMode`/sprite-mode determination needs (A-M22-6).

Machine wiring (`src/machine/hbf1xv_machine.h/.cpp`): **NO changes needed.** `vdp()` (M14) and
`render_frame()` (M21) already exist and are sufficient; sprite/command-engine state is reached via
`machine.vdp().sprite_engine()`/`machine.vdp().cmd_engine()` for tests, and pixel output is already
composited transparently into the existing `render_frame()` call. This is a deliberately low-risk
integration point worth flagging explicitly: unlike M18/M19/M20 (new device families needing new
slot/bus wiring), M22 adds ZERO new machine-level surface area.

`CMakeLists.txt`: add `src/devices/video/sprite_engine.cpp` and
`src/devices/video/vdp_command_engine.cpp` to `sony_msx_core`'s source list.

Boundary compliance: both new classes carry no slot/bus knowledge (matches `V9958Vdp`/
`VdpFrameRenderer` themselves), placed under `src/devices/` per `src/CLAUDE.md`.

### 2.2 D2 — Sprite engine spec

**Sprite Mode determination** (independently re-derived from `DisplayMode.hh:158-174`, cross-
checked against this project's OWN `VdpModeState.base`): mode 1 for base `0x00`(G1)/`0x02`(MC)/
`0x04`(G2); mode 2 for base `0x08`(G3)/`0x0C`(G4)/`0x10`(G5)/`0x14`(G6)/`0x1C`(G7, incl. YJK/
YJK+YAE); mode 0 (no sprites) for TEXT1/TEXT1Q/TEXT2/MulticolorQ/Unknown — confirmed this project's
V9958 is never `isMSX1VDP()`, so MULTIQ's MSX1-only mode-1 case (`DisplayMode.hh:162-163`) never
applies here.

**Per-output-line visible-sprite selection** (`recompute_frame`, run once per `on_vsync()` over
ALL output lines `0..height-1` in one deterministic pass — mirrors `checkSprites1`/`checkSprites2`'s
own "process the whole frame, tracking the FIRST line where 5th/9th-sprite occurs" requirement,
adapted to this project's frame-wide, non-incremental architecture):
- For each sprite `s` (0-31, stop early at Y=208 mode1 / Y=216 mode2 sentinel), for each output line
  `L` in `1..height-1` (line 0 is unconditionally sprite-free, A-M22-9): `sprite_line =
  ((L - 1) + R23 - Y[s]) & 0xFF`; visible iff `sprite_line < magSize` (`magSize = (mag+1)*size`,
  `size` from R#1 bit1 (8/16), `mag` from R#1 bit0).
- Mode 1: max 4 visible/line; 5th sprite (first sprite index exceeding 4 on the EARLIEST such
  line) recorded into S#0 bits4-0 with bit6 set (only when F/C not both zero-masked per
  `SpriteChecker.cc:163`'s `(status&0xC0)==0` gate); pattern from `calculatePatternNP`-equivalent
  (flat sprite-pattern-table read, `patternNr*8+spriteLine` [+16 for 16x16]); color = attribute
  byte's bits3-0 (one color per WHOLE sprite, not per-line).
- Mode 2: max 8 visible/line; 9th-sprite condition ALSO stored in the SAME S#0 bits4-0/bit6 field
  (fact-sheet §6: "5S bit means ≥5 (or ≥9 in G-modes)" — same register field, different trigger
  count depending on mode, a naming quirk to preserve exactly, not "fix"); pattern from
  `calculatePatternPlanar`-equivalent (even/odd pattern-table-index bank split, mirrors this
  project's OWN `planar_row_spans` bank-split concept applied to the sprite-pattern table instead
  of the bitmap tables); per-LINE color/attribute byte at `spriteAttribTableBase + (sprite*16 +
  spriteLine)` (A-M22-15), EC (bit7, A-M22-10) applied to X at check time (`x -= 32`).
- Magnification (`doublePattern`, `SpriteChecker.hh:52-63`): independently re-derive the documented
  bit-doubling algorithm ("abcd...." → "aabbccdd") for `MAG=1`.
- Collision detection (per output line, checking every VISIBLE-sprite pair, max 4 mode1/8 mode2 →
  ≤6/≤28 pairs): color-0 collision eligibility gated by `canSpriteColor0Collide`-equivalent
  (`isMSX1VDP() || !TP` — since this machine is never MSX1, effectively `!TP`, i.e. color-0 sprites
  can collide ONLY when R#8 TP is disabled); mode2 additionally excludes CC-or-IC sprites from
  collision eligibility (A-M22-10/11, bits `0x60`); overlap test via 32-bit pattern AND with a
  distance-based shift (`SpriteChecker.cc:210-228`); first collision found (lowest output line)
  sets S#0 bit5 (C) and records `collision_x = min_x_collision + 12`, `collision_y = output_line +
  8` (the `+12`/`+8` offsets are fixed, border-geometry-derived constants baked directly into the
  STATUS-REGISTER CONTRACT — independent of any absolute-raster-line model this project does not
  build, A-M22-derived; `getLineZero()`'s subtraction in the reference is a no-op for this
  project's already-0-based active-display-line convention).
- S#0 composition: bit6=5S, bit5=C, bits4-0=sprite-number (A-M22-14's stale-field nuance).
- S#3/S#4: collision-X low/high, high byte `| 0xFE` (bits7-1 forced 1, matches the EXISTING M14
  stub mask — confirmed unchanged). S#5/S#6: collision-Y low/high, high byte `| 0xFC` with bit1
  ALWAYS 0 (A-M22-13's inherited EO-not-implemented gap).

**Pixel compositing** (`VdpFrameRenderer::composite_sprites`, queries `sprite_engine().
visible_sprites(line)`):
- Mode 1: reverse-priority overdraw (`std::views::reverse`-equivalent — draw lowest-priority
  (highest sprite index) FIRST, highest-priority (sprite 0) LAST, so sprite 0 visually wins
  overlaps); skip color-index-0 pixels ONLY when TP enabled (A-M22-12); clip pattern to
  `[0, width)`.
- Mode 2: find the first sprite (lowest index) with CC=0 (`first`); draw from lowest-priority index
  down to `first`; for each drawn pixel, OR-merge in any immediately-following (higher index,
  contiguous, same-pixel-overlap) CC=1 sprites' color bits (A-M22-10's cascade rule,
  independently re-derived from `SpriteConverter.hh:155-204`); IC=1 sprites ARE drawn normally
  (A-M22-11 — IC never gates visibility, only collision).
- GRAPHIC5 (SCREEN6, 2bpp/4-palette-index background): sprite pixels use the SAME 16-color palette
  as every other mode 2 case (the reference's `SpriteConverter`'s `MODE==GRAPHIC5` branch splits a
  4-bit merged color into two 2-bit half-pixel writes — `palette[color>>2]`/`palette[color&3]`);
  independently re-derive and reproduce this specific sub-case explicitly (a disclosed, testable
  detail, not silently generalized from the G6/G7 single-pixel-write case).

### 2.3 D3 — VDP command engine spec

**Register file** (owned by `VdpCommandEngine`, NOT `V9958Vdp::control_regs_`, A-M22-2): SX(9-bit)/
SY(10-bit)/DX(9-bit)/DY(10-bit)/NX(10-bit)/NY(10-bit)/COL(8-bit)/ARG(8-bit)/CMD(8-bit), plus
internal working state ASX/ADX/ANX (temp X/Y trackers exposed read-only via S#8/S#9's `border_x()`)
— bit widths independently re-derived (A-M22-3).

**scrMode** (A-M22-6): 0=G4, 1=G5, 2=G6, 3=G7(+YJK/YJK+YAE), 4=NonBitmap (any other mode, gated by
R#25 CMD bit), -1=no commands (writing CMD immediately no-ops as if ABRT).

**Command dispatch** (A-M22-4/A-M22-5): 13 CMD>>4 code-points — ABRT/STOP(0-3, aliased),
POINT(4), PSET(5), SRCH(6), LINE(7), LMMV(8), LMMM(9), LMCM(0xA), LMMC(0xB), HMMV(0xC), HMMM(0xD),
YMMM(0xE), HMMC(0xF). Only PSET/LINE/LMMV/LMMM/LMMC consult the low nibble for a logical-op
selection (IMP=0000/AND=0001/OR=0010/XOR=0011/NOT=0100, T-variants at 1000-1100, undefined
low-nibbles behave as a no-op "DummyOp" — `VDPCmdEngine.cc:2126-2185`'s own dispatch table,
independently re-derived); the other 8 commands always perform their fixed, implicit operation
(plain copy/fill/read) regardless of the low nibble.

**Address resolution** (§1.5, `vdp_command_address.h`): dispatch on `scrMode` to the matching
coordinate-address function; `!extVRAM` branch only (A-M22-8).

**Execution model** (§1.4 Resolution 2): atomic for 10 commands; event-driven stateful (TR/CE/
`transfer`) for LMCM/LMMC/HMMC.

**Logical-op semantics** (fact-sheet §8, independently cross-checked against
`VDPCmdEngine.cc:665-720`'s `ImpOp`/`AndOp`/`OrOp`/`XorOp`/`NotOp`/`TransparentOp<Op>` functors):
`IMP: DC=SC`, `AND: DC=SC&(DC|mask)` (mode-specific nibble/2-bit masking for G4/G5/G6's packed
pixels, G7/NonBitmap have no sub-byte mask), `OR: DC=SC|DC`, `XOR: DC=SC^DC`, `NOT: DC=(SC&mask)|
~(DC|mask)`; T-variants: apply the op ONLY when the source pixel value is non-zero, else leave the
destination unchanged (`TransparentOp<Op>::operator()`'s `if (color) Op::operator()(...)`) — the
masked-blit/animation idiom.

**S#2 status composition** (independently re-derived from `VDP.cc:928-941`): bits3-2 stay the
EXISTING M14 "undocumented, always 1" constant (`kStatusReg2Base`, unchanged); bit1 stays the
EXISTING EO-field toggle (unchanged, D6/M21); bit0=CE, bit4=BD, bit7=TR — sourced live from
`cmd_engine_.ce()`/`bd()`/`tr()`. Bits5/6 (HR/VR, raster-timing-derived) remain idle 0, correctly
deferred to D4/M23 (unchanged M14 disposition — NOT this milestone's scope).

**S#7/S#8/S#9**: S#7 = `cmd_engine_.color()` (live COL), read-side calls `on_color_register_read()`
(arms next LMCM transfer + conditionally clears TR, mirrors `resetColor()`'s `if (!CMD) status &=
~TR;`). S#8/S#9 = `cmd_engine_.border_x()` (ASX) low/high, high byte mask `| 0xFE` (matches the
EXISTING M14 stub mask, confirmed unchanged); S#9 read additionally calls
`on_border_x_register_read()` (clears BD, mirrors `resetBD()`).

**R#25 CMD-bit-gated non-bitmap commands**: when CMD bit set and the current display mode is
anything other than G4-G7 (e.g. TEXT1/TEXT2/G1/G2/G3/MULTICOLOR), commands execute using
`NonBitmapMode`'s flat, non-planar X/Y→address formula (§1.5) over the SAME physical VRAM the
background renderer reads — meaning a command run in, say, GRAPHIC1 mode writes bytes that the
EXISTING M21 `render_graphic1`/name-table/pattern-table logic will subsequently read and display
unmodified (no special-casing needed in the renderer — a low-risk integration strength, mirroring
the same "commands write into the SAME shared `VdpVram` the renderer already reads" property
noted in §2.1 for G4-G7).

### 2.4 D7 closure

Fully resolved and grounded in §1.5. `vdp_command_address.h`'s five functions close D7's remaining
piece; the backlog transitions to `DONE (M22)`.

### 2.5 Determinism (hard requirement, unchanged philosophy from M21)

- `SpriteEngine::recompute_frame()` is driven ONLY by the EXISTING `on_vsync()` hook (no new clock
  consumer) — a pure function of the frozen VRAM/register snapshot at that moment, mirroring the
  blink-countdown precedent exactly.
- `VdpCommandEngine`'s atomic commands are pure, synchronous functions of VRAM/register state at
  call time (no wall-clock, no hidden scheduling). Its 3 event-driven (transfer) commands advance
  ONLY on real I/O-port accesses (`write_register`/`on_color_register_read`) — never on elapsed
  cycles/time — preserving "no new clock consumer" exactly as the FDC precedent already
  established for THIS project.
- Two-run determinism: identical VRAM/register/command-register write sequences on two independent
  `Hbf1xvMachine` (or bare `V9958Vdp`) instances produce byte-identical VRAM content, status-
  register values, and `FrameBuffer` content.
- *Verify:* the M9/M12 CPU-timing oracle suites and the M14/M21 VRAM-pointer/planar-transform unit
  tests remain green unmodified (no CPU-visible timing change from this milestone at all).

### 2.6 openMSX A/B acceptance plan (raw-byte/register comparison — genuinely more direct than M21's own technique)

Mirrors the established M11-M21 Tcl-debugger methodology, extended to the NEW sprite/command
observable state, which is MORE directly comparable than M21's own computed-color content (sprite
collision flags, 5S/C bits, and command-engine VRAM writes are exactly the kind of raw-byte/
register state this project's technique already handles well):

1. **Sprite collision/5th-sprite status**: drive an identical sprite-attribute-table + register
   setup on both sides (via each engine's own VRAM/port-write path); read back S#0 (5S/C/sprite-
   number), S#3-S#6 (collision X/Y) via each side's own status-register read/Tcl-debugger
   equivalent; compare raw byte values directly. Include: (a) a straightforward 5-sprites-on-one-
   line case (mode 1); (b) a 9-sprites-on-one-line case (mode 2); (c) a deliberate two-sprite
   overlap producing a collision, including one case with TP disabled and color-0 sprites (A-M22-12
   interacts with `canSpriteColor0Collide`); (d) an IC=1 sprite overlap that must NOT register a
   collision (A-M22-11).
2. **Command engine VRAM writes**: drive an identical R#32-46 sequence on both sides for a
   representative case per command family — a fill (HMMV), a logical-op copy (LMMM with XOR), a
   line draw (LINE), a search (SRCH, reading back S#8/S#9 border-X and S#2 BD) — then read back raw
   VRAM bytes at the destination address on both sides; compare directly. Include a G6/G7-planar
   destination case specifically to cross-validate §1.5's D7-closure claim (the command-engine
   write must land at the SAME physical bank/offset on both engines).
3. **Command engine status handshake**: drive an LMMC transfer (a few bytes) on both sides, reading
   S#2 TR/CE between each R#44 write, confirming the SAME transfer-count-driven TR/CE sequence
   (this engine's event-driven model vs. openMSX's own cycle-scheduled model — the comparison is on
   the FINAL, settled register values after each discrete CPU action, not on wall-clock timing,
   which is explicitly out of scope, §1.2).
4. **Adversarial comparator self-check** (as every prior milestone): empty-side input → BLOCKED;
   corrupted field → DIVERGENCE.
5. **Mechanics**: `tools/gen-m22-sprite-cmd-probe.py` (new, synthetic VRAM/register fixtures) and
   `tools/openmsx-m22-sprite-cmd-parity.ps1` (new); output `docs/m22-parity-trace-diff.md`.
6. **Honest BLOCKED disposition, if warranted**: if the live openMSX Tcl debugger turns out NOT to
   expose a directly-comparable introspection point for any specific sub-claim (verify via a live
   `debug list` query before claiming either way, exactly as M21 did for computed pixel color) —
   report that sub-claim BLOCKED, not fabricated. Candidates to check explicitly: whether
   `VDPCmdEngine`'s internal ASX/ADX/ANX working registers are exposed distinctly from S#8/S#9's
   already-masked byte view (if not, the comparison stays at the S#8/S#9 byte level, which is still
   a genuine, real comparison, just coarser).
7. **Hard rule (unchanged)**: no parity claim without a genuine captured diff.

---

## 3. Milestones (Slice Plan M22-S1 … S8)

Every slice runs the full evidence-gate set (§5 item 11) and leaves `ctest` green, including the
**entire M1-M21 suite (106 tests)**, per the human's explicit "deliberate cross-check" directive.

### M22-S1 — Sprite engine infrastructure + Sprite Mode 1
- **Goal:** `SpriteEngine` skeleton (`sprite_engine.h/.cpp`); sprite-mode determination
  (A-M22-6-analog for sprites, from `DisplayMode.hh:158-174`); Sprite Mode 1 check/collision/
  5th-sprite algorithm (A-M22-9's 1-pixel-shift, EC), status composition (S#0/S#3-S#6), wired into
  `V9958Vdp::on_vsync()` and `peek_status_register()`/`read_status()` (cases 0/3-6).
- **Files:** `src/devices/video/sprite_engine.{h,cpp}` (new); `v9958_vdp.{h,cpp}` (extend).
- **Unit tests:** 1-pixel vertical shift (Y=0 sprite absent on line 0, present from line 1,
  A-M22-9); EC x-32 shift; 4-sprites-then-5th-sprite detection with the EARLIEST-line rule; S#0
  read clearing ONLY bits7/6/5 (A-M22-14); collision detection + the `+12`/`+8` coordinate offsets;
  color-0/TP collision-eligibility interaction; two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S2 — Sprite Mode 2 + pixel compositing
- **Goal:** Sprite Mode 2 (8/line, 9th-sprite→5S field, per-line color/attribute table at
  +512-byte offset, A-M22-15/16); CC/IC bit semantics (A-M22-10/11); `VdpFrameRenderer::
  composite_sprites()` for both modes, including the CC color-cascade merge and the GRAPHIC5
  half-pixel special case (§2.2).
- **Files:** `sprite_engine.{h,cpp}` (extend); `vdp_frame_renderer.{h,cpp}` (extend).
- **Unit tests:** 8-sprites-then-9th-sprite; per-line (not per-sprite) color; IC=1 sprite drawn but
  excluded from collision (A-M22-11's precise distinction); CC=1 color-cascade merge chain (at
  least 2 consecutive CC=1 sprites merging into a preceding CC=0 sprite); color-0/TP transparency
  in BOTH modes (A-M22-12, both TP states tested); GRAPHIC5 half-pixel sprite color split; overdraw
  priority order (sprite 0 wins overlaps in both modes); two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S3 — Command engine register file + dispatch + address resolution (closes D7)
- **Goal:** `VdpCommandEngine` skeleton; R#32-46 register file (A-M22-2/3); `change_register()`
  dispatch extension (A-M22-1); scrMode determination (A-M22-6, wired from `recompute_mode()`);
  the five `vdp_command_address.h` coordinate functions (§1.5) — THIS is the D7-closing slice.
- **Files:** `vdp_command_engine.{h,cpp}` (new); `vdp_command_address.h` (new); `v9958_vdp.{h,cpp}`
  (extend: dispatch + mode-change notification).
- **Unit tests:** register bit-width masking (A-M22-3); dispatch reaches the command engine via
  BOTH `#99` and `#9B` (A-M22-1); scrMode table for every mode × R#25-CMD-bit combination
  (A-M22-6); each of the 5 address functions cross-checked against a hand-computed table INCLUDING
  the two G6/G7 hand-verified cases from §1.5; the R#2-bypass nuance explicitly tested (R-M22-7);
  extended-VRAM bits stored but inert (A-M22-8); two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S4 — Atomic commands: fill/copy/line/search/point (no logical op)
- **Goal:** ABRT/STOP, POINT, SRCH, HMMV, HMMM, YMMM, LMCM's non-transfer skeleton (see S6 for the
  transfer-completing half) implemented atomically; BD persistence (S#9-read-cleared only).
- **Files:** `vdp_command_engine.{h,cpp}` (extend).
- **Unit tests:** HMMV fill across NX×NY with DIX/DIY direction bits; HMMM/YMMM VRAM→VRAM copy;
  SRCH finding a target color and setting BD (persisting across an unrelated status read, clearing
  only on S#9 read); POINT reading a pixel into COL/S#7; ABRT/STOP immediately clearing CE; the
  low-nibble-ignored property for these commands (A-M22-4); two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S5 — Atomic commands with logical ops: PSET/LINE/LMMV/LMMM
- **Goal:** the 5 logical operations (IMP/AND/OR/XOR/NOT + T-variants) applied to PSET/LINE/LMMV/
  LMMM, including mode-specific sub-byte pixel masking for G4/G5/G6.
- **Files:** `vdp_command_engine.{h,cpp}` (extend).
- **Unit tests:** each of the 5 ops + its T-variant, for at least one packed mode (G4, verifying the
  nibble mask) and one byte mode (G7); undefined low-nibble → no-op (DummyOp); LINE's Bresenham
  major-axis (MAJ bit) behavior for at least one octant; two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S6 — Transfer commands: LMCM/LMMC/HMMC (event-driven TR/CE state machine)
- **Goal:** the stateful, event-driven handshake (§1.4 Resolution 2) for the 3 CPU-transfer
  commands: `transfer` flag, TR set/clear timing, CE persistence across the whole NX×NY transfer,
  S#7 read/write side effects (`on_color_register_read()`).
- **Files:** `vdp_command_engine.{h,cpp}` (extend).
- **Unit tests:** LMMC — write R#46 to start, confirm TR=1/CE=1, write R#44 repeatedly and confirm
  each write completes exactly one pixel's transfer (VRAM updated), confirm CE clears exactly after
  the NX×NYth write; LMCM — read S#7 repeatedly, confirm COL advances correctly and CE clears at
  the end; HMMC analogous at byte granularity; a test confirming NO wall-clock/cycle dependency
  (advancing zero simulated cycles between transfer steps still works, since it's event-, not
  time-, driven); two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S7 — R#25 CMD-bit non-bitmap commands + final status wiring
- **Goal:** `NonBitmapMode` command execution gated by R#25 CMD bit in non-G4-G7 modes; confirm
  written bytes are visible unmodified through the EXISTING M21 renderer paths (the "shared VRAM"
  integration strength, §2.1/§2.3); complete S#2/S#7/S#8/S#9 status wiring if any piece remains.
- **Files:** `vdp_command_engine.{h,cpp}` (extend if needed); `v9958_vdp.{h,cpp}` (status wiring
  completion).
- **Unit tests:** a command run in GRAPHIC1 mode with CMD bit set, confirmed to write bytes the
  EXISTING `render_graphic1` path subsequently displays correctly (a genuine cross-path test,
  mirroring M21's own D7 cross-path-test precedent); CMD bit clear in a non-G4-G7 mode → command
  silently no-ops (scrMode=-1); two-run determinism.
- **Gates:** build + ctest green (full suite).

### M22-S8 — System integration + A/B evidence + backlog finalization
- **Goal:** a dedicated SYSTEM integration test exercising a real CPU program (via the M11 bus,
  `OUT (#98)/(#99)/(#9B)`) that (a) sets up sprites and reads back S#0/S#3-S#6 through a real
  collision scenario, and (b) issues a representative command sequence (at least one atomic command
  and one transfer command) and confirms the resulting VRAM content via `render_frame()`; the A/B
  evidence capture (§2.6); full re-verification of the ENTIRE M1-M21 suite (106 tests) by BOTH the
  developer AND (independently) QA; backlog finalization (§4) — D2/D3 close in full, D7 transitions
  to DONE.
- **Files:** `tests/integration/machine/hbf1xv_m22_sprites_integration_test.cpp` (new);
  `tests/integration/machine/hbf1xv_m22_command_engine_integration_test.cpp` (new);
  `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp` (new); `tools/gen-m22-sprite-
  cmd-probe.py` (new); `tools/openmsx-m22-sprite-cmd-parity.ps1` (new); `docs/m22-parity-trace-
  diff.md`; refreshed `docs/asset-checksums.txt`.
- **Gates:** all evidence gates (validate-assets, checksum, Debug build, ctest — FULL suite) plus
  the A/B gate.
- **Backlog effect:** this is the slice that finalizes D2/D3 → DONE and D7 → DONE (no longer
  partial) in the ledger.

---

## 4. Full Deferred-Backlog Review (mandatory — every row across all six registry sections, per DEC-0005)

Source of truth: `agent-protocol/state/deferred-backlog.md`, read in FULL this cycle (all 34 rows
across Sections A-F). Every row re-affirmed below with a one-line justification.

### Section A (human-directive-tracked rows)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| B1 | PSG/YM2149 internals | DONE (M15) — unchanged | Closed at M15; unrelated to M22 (audio device). |
| B2 | RTC/RP5C01 internals | DONE (M15) — unchanged | Closed at M15; unrelated to M22. |
| B3 | FM-PAC (OPLL YM2413) device internals | DONE (M17) — unchanged | Closed at M17; unrelated to M22. |
| B4 | MSX-JE 16 KB SRAM | DONE (M20) — unchanged | Closed at M20; unrelated to M22. |
| B5 | Kanji-font I/O `#D8-DB` | DONE (M18) — unchanged | Closed at M18; unrelated to M22. |
| B6 | Halnote/MSX-JE firmware mapping | DONE (M20) — unchanged | Closed at M20; unrelated to M22. |
| B7 | Cartridge loading | DONE (M19) — unchanged | Closed at M19; unrelated to M22. |
| B8 | FDC drive mechanics | DONE (M16) — unchanged | Closed at M16; unrelated to M22 (though its DRQ/INTRQ state-machine PATTERN is the direct precedent M22 follows for the 3 command-engine transfer commands, §1.4). |
| B9 | VRAM/V9958 VDP (register/status/interrupt contract) | DONE (M14) — unchanged | Closed at M14; M22 EXTENDS the same device family (sprites + command engine) but does not reopen B9's own closed acceptance criteria — the new `change_register()`/status-register edits are additive extensions of an already-forward-compatible M14 design (A-M22-1). |

### Section B (other tracked deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| C1 | Exact cycle/T-state timing parity | OPEN — unchanged | M22 introduces no CPU-visible timing behavior; the command engine's atomic/event-driven model has no cycle cost. Candidate owner remains M23. |
| C2 | Z80 HALT-R increment | OPEN — unchanged | Per DEC-0004; unrelated to M22. |
| C3 | ZEXDOC/ZEXALL full sweep | OPEN — unchanged | `references/zexall/` remains present, unactioned; unrelated to M22. |
| C4 | S1985 backup-RAM `.sram` persistence | DONE (M15) — unchanged | Closed at M15; unrelated to M22. |
| C5 | Full boot past first device read | IN-PROGRESS (carried from M16) — unchanged | M22 adds no new boot-path interaction (sprites/command engine are on-demand/CPU-triggered, not wired into the boot sequence); the M16 residual (auto-disk-boot trigger investigation) remains untouched. |
| C6 | Keyboard matrix + joystick | DONE (M15) — unchanged | Closed at M15; unrelated to M22. |
| C7 | Printer + cassette | DONE (M18) — unchanged | Closed at M18; unrelated to M22. |
| C8 | Sony speed-controller + hardware PAUSE + Ren-Sha Turbo | OPEN — unchanged | HB-F1XV-specific companion-chip behavior; unrelated to M22. |
| C9 | SDL3 frontend | OPEN — unchanged | Presentation layer; M22's sprite compositing stays inside the existing SDL3-independent `FrameBuffer` contract (§1.4 Resolution 1), by design — still unbuilt itself, unchanged disposition. |
| C10 | FDC flux-level/DMK fidelity | OPEN — unchanged | Unrelated to M22 (FDC, not VDP) — though B8's DRQ/INTRQ pattern is reused conceptually (Section A note above). |
| D1 | Pixel-accurate raster rendering pipeline | DONE (M21) — unchanged | Closed at M21; M22 EXTENDS `VdpFrameRenderer` with sprite compositing (§1.4) but does not reopen D1's own closed background-plane acceptance criteria. |
| D2 | Sprite rendering + collision/5th-sprite detection | **CLOSES this cycle (M22)** | Full sprite mode 1 & 2 check/collision/compositing delivered (§2.2, S1-S2), grounded byte-exact per `SpriteChecker.cc/.hh`/`SpriteConverter.hh`, including the independently-verified 1-pixel vertical shift (A-M22-9), EC/CC/IC bit semantics (A-M22-10/11), the fact-sheet-vs-source color-0/TP discrepancy resolved in favor of source (A-M22-12), and the honestly-inherited S#6-bit1/EO gap (A-M22-13). |
| D3 | VDP command engine | **CLOSES this cycle (M22)** | Full R#32-46 register file, all 13 commands (10 atomic + 3 event-driven-stateful, §1.4 Resolution 2), the 5 logical operations + T-variants, S#2 TR/CE/BD, S#7 color, S#8/S#9 border-X, and R#25 CMD-in-all-modes delivered (§2.3, S3-S7), grounded byte-exact per `VDPCmdEngine.cc/.hh`. |
| D4 | Cycle-accurate VDP access-slot/command timing | OPEN — unchanged | Explicitly named out of M22 (§1.2); the command engine's atomic/event-driven execution model is a DELIBERATE, honestly-scoped stand-in, not a partial attempt at D4. Candidate owner M23. |
| D5 | YJK/YJK+YAE color decode + 15-bit DAC | DONE (M21) — unchanged | Closed at M21; unrelated to M22 (M22's command-engine address resolution for G7/YJK modes reuses the SAME `scrMode=3` bucket as plain GRAPHIC7, §1.5, but does not touch the color-decode formulas themselves). |
| D6 | Horizontal scroll/interlace/blink/superimpose | DONE (M21) — unchanged | Closed at M21; unrelated to M22. |
| D7 | G6/G7 VRAM address interleave in the display/command path | **CLOSES IN FULL this cycle (M22)** — was IN-PROGRESS (M21 partial) | The command-engine-path piece (five new coordinate-based address functions, `vdp_command_address.h`) is delivered and independently hand-verified for physical-bank-placement consistency with the ALREADY-closed CPU-port/display-path pieces (§1.5) — correcting, not blindly confirming, the carried-forward M21 assumption that the SAME function would be reused (it is a DIFFERENT function, in a different domain, that is mathematically consistent with the same underlying hardware rule). No piece of D7 remains open. |

### Section C (M14 VDP-depth deferrals) — covered as D1-D7 above; no separate additional rows.

### Section D (M17 YM2413 DSP/timing deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| E1 | YM2413 FM-synthesis DSP/audio depth | OPEN — unchanged | Unrelated to M22 (audio device). |
| E2 | YM2413 register-write timing constraint | OPEN — unchanged | Unrelated to M22. |

### Section E (M18 printer/cassette depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| F1 | Cassette tape image-format/signal fidelity | OPEN — unchanged | Unrelated to M22. |
| F2 | Printer image/ESC-sequence rendering depth | OPEN — unchanged | Unrelated to M22 (a different device's rendering depth, not the VDP). |

### Section F (M19 cartridge-mapper-depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| G1 | KonamiSCC mapper + embedded SCC/SCC+ sound chip | OPEN — unchanged | Unrelated to M22 (cartridge/audio). |
| G2 | ROM-database/heuristic mapper-type auto-detection | OPEN — unchanged | Unrelated to M22. |
| G3 | Full `CartridgeSlotManager`-style runtime hot-plug | OPEN — unchanged | Unrelated to M22. |
| G4 | The long tail of ~80 other specialized/vendor mapper types | OPEN — unchanged | Unrelated to M22. |

**Backlog bookkeeping note (executed at ledger-update time by the coordinator, per the exact
M14/M17/M18/M19/M20/M21 precedent):** on M22 closure, mark D2/D3 `DONE (M22)` and D7 `DONE (M22)`
(no longer partial) in the same cycle. No new backlog row is proposed this cycle — every named
piece of D2/D3/D7 closes without discovering further out-of-scope depth beyond D4 (already a
separately-named, already-scheduled row).

---

## 5. Acceptance Criteria (M22 exit)

1. `SpriteEngine` (§2.2) implemented: Sprite Mode 1 (4/line) and Sprite Mode 2 (8/line) check,
   collision detection, and 5th/9th-sprite status computed once per frame at the EXISTING
   `on_vsync()` hook (no new clock consumer); S#0/S#3-S#6 wired live, replacing the M14/M21 idle
   stubs. *(S1-S2)*
2. The 1-pixel vertical sprite shift (A-M22-9), EC/CC/IC bit semantics INCLUDING the IC-does-not-
   suppress-rendering distinction (A-M22-11) and the fact-sheet-vs-source color-0/TP resolution in
   favor of the read source (A-M22-12), are implemented and independently tested exactly as
   grounded, not as naively assumed from names/prose alone. *(S1-S2)*
3. Sprite pixel compositing (mode 1 overdraw-priority, mode 2 CC color-cascade merge, the GRAPHIC5
   half-pixel special case) is implemented AS PART OF `VdpFrameRenderer::render_frame()`'s existing
   pipeline (§1.4 Resolution 1), returning a single, already-composited `FrameBuffer` — no separate
   sprite-plane output type. *(S2)*
4. `VdpCommandEngine` (§2.3) implemented: the full R#32-46 register file (A-M22-2/3); all 13
   commands (ABRT/STOP, POINT, PSET, SRCH, LINE, LMMV, LMMM, LMCM, LMMC, HMMV, HMMM, YMMM, HMMC);
   the 5 logical operations + T-variants for the 5 commands that support them, with the other 8
   commands' low-nibble-ignored property explicitly tested (A-M22-4); R#25 CMD-in-all-modes
   support via `NonBitmapMode` addressing. *(S3-S7)*
5. The command-execution model is the explicitly-justified HYBRID from §1.4 Resolution 2: atomic
   completion for the 10 non-transfer commands (CE observably 0 immediately after the triggering
   write; BD persists until an explicit S#9 read); an event-driven (never wall-clock), multi-step
   TR/CE/`transfer` state machine for LMCM/LMMC/HMMC, mirroring this project's own FDC precedent.
   *(S4-S6)*
6. D7 closes IN FULL: five new, dedicated coordinate-based address-resolution functions
   (`vdp_command_address.h`) are implemented, independently hand-verified for physical-bank-
   placement CONSISTENCY with (not identity to) the existing CPU-port/display-path rotate-right-by-
   1 model for G6/G7 (§1.5), and the R#2-bypass nuance (commands address BOTH pages directly via
   Y, never via R#2's display-page-select bits) is explicitly implemented and tested (R-M22-7).
   *(S3)*
7. **D2 and D3 close in full this cycle; D7 transitions from IN-PROGRESS (M21 partial) to DONE
   (M22)** — no row remains partial after this milestone (§4).
8. Deterministic unit + integration + a dedicated SYSTEM integration test (per `tests/CLAUDE.md`'s
   three-tier convention) cover every new behavior in §2.2-§2.4; two identical runs produce byte-
   identical VRAM content, status-register values, and `FrameBuffer` content for identical inputs.
   *(S1-S8)*
9. **Zero regression across the FULL M1-M21 suite (106 tests)** — not merely sprite/command-
   adjacent tests — independently re-run and confirmed by BOTH the developer AND (separately) QA,
   per the human's explicit "deliberate cross-check across the entire system" directive. In
   particular: the M9/M12 CPU-timing oracles remain untouched (no new clock consumer); the M14/M21
   VRAM-pointer/planar-transform unit tests remain green UNMODIFIED. *(S8)*
10. Real openMSX A/B evidence captured via the raw-byte/register comparison technique (§2.6) for:
    sprite collision/5th-sprite status (incl. an IC=1 non-collision case and a TP-disabled color-0
    collision case), command-engine VRAM writes (incl. a G6/G7-planar destination cross-validating
    D7's closure), and the command-engine status handshake (TR/CE sequence for a transfer command).
    Any individual sub-claim with no feasible headless equivalent is reported BLOCKED, not
    fabricated. *(S8)*
11. Every deferred-backlog row (all 34, across all six registry sections) re-affirmed with
    justification (§4); D2/D3 → DONE, D7 → DONE (no longer partial).
12. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest
    — full suite).
13. QA sign-off recorded before closure (`docs/m22-qa-signoff.md`), including QA's OWN independent
    re-run of the full M1-M21 regression suite (not a rubber-stamp of the developer's own claim).
    Per `agent-protocol/state/current-phase.md`, the coordinator is pre-authorized to proceed
    through the release-decision/tag step for this specific M21-M23 run on a clean QA PASS, without
    an additional pause request — but a real blocker (QA anything less than PASS) stops the run for
    human consultation.

---

## 6. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M22-1 | The command engine's logical-op dispatch is wrongly applied to ALL 13 commands instead of only the 5 that actually consult the CMD low nibble (POINT/SRCH/LMCM/HMMV/HMMM/YMMM/HMMC always perform a fixed, implicit operation, A-M22-4). | Unit test sets a non-zero, non-IMP low nibble on HMMV/HMMM/YMMM/HMMC/POINT/SRCH/LMCM and confirms the fixed/implicit behavior is unaffected, while the SAME low nibble on LMMV/LMMM/PSET/LINE/LMMC produces the documented op result. |
| R-M22-2 | The 1-pixel vertical sprite shift and the "sprite data fetched one line early" pitfall are treated as two SEPARATE mechanisms to model (potentially double-applying an offset, or modeling neither correctly), instead of the SAME off-by-one (A-M22-9). | Unit test places a sprite at Y=0 and confirms it is invisible on output line 0 and first visible on output line 1 (not line 2, ruling out a double-applied offset); a second test confirms output line 0 is unconditionally sprite-free. |
| R-M22-3 | EC/CC/IC bit positions are guessed/transposed (e.g. assuming CC=bit5/IC=bit6 instead of the source-code-confirmed CC=bit6/IC=bit5, A-M22-10). | Unit test sets each bit individually (0x80/0x40/0x20) and confirms exactly the documented, source-grounded effect for each, with no cross-contamination. |
| R-M22-4 | IC is implemented as suppressing sprite RENDERING (matching its "Invisible Code" name) instead of ONLY excluding the sprite from collision detection, contradicting the actual `SpriteConverter.hh` drawing code which never checks bit 0x20 (A-M22-11). | Dedicated unit test confirms an IC=1 sprite's pixels ARE present in `FrameBuffer` output while simultaneously confirming it does NOT participate in a collision. |
| R-M22-5 | Color-0 sprite-pixel transparency is implemented per the fact-sheet's blanket "always transparent regardless of TP" claim instead of the actual source behavior (conditioned on R#8 TP, A-M22-12) — a genuine fact-sheet-vs-source discrepancy this project's discipline requires resolving in favor of the read source. | Two unit tests: TP enabled + color-0 sprite pixel → background shows through; TP disabled + color-0 sprite pixel → palette entry 0 is drawn (not background) — both must pass, proving the implementation follows the CODE, not the prose. |
| R-M22-6 | S#0 read is implemented as clearing the ENTIRE byte (including the stale 5th/9th-sprite-number field in bits4-0) instead of only bits7/6/5 (A-M22-14). | Unit test triggers a 5th-sprite condition, reads S#0 twice, and confirms the sprite-number field survives the FIRST read unchanged (only bits7/6/5 clear); a subsequent frame's recompute is the only thing that changes it. |
| R-M22-7 | The command engine's coordinate-to-address resolution incorrectly consults R#2's display-page-select bits (copying the DISPLAY path's page-resolution logic), when real hardware addresses BOTH pages directly through the Y-coordinate's own range, bypassing R#2 entirely (§1.5's genuinely-different-from-M21 nuance). | Unit test sets R#2's page-select bits to page 0, issues a command with a DY/SY value in the SECOND page's Y range (e.g. DY=300 for G6/G7's 512-line range), and confirms the write lands in the correct SECOND PHYSICAL BANK regardless of R#2's setting. |
| R-M22-8 | D7's command-engine-path piece is implemented as a literal call to (or a copy-paste reuse of) `V9958Vdp::effective_address()`, which is architecturally impossible (that function has no X/Y parameters) but a careless implementation might instead force an awkward, incorrect translation attempting to route through it, rather than implementing the genuinely separate, independently-derived coordinate-based formulas in `vdp_command_address.h` (§1.5). | Implementation report explicitly names `vdp_command_address.h` as new, separate functions; QA independently confirms `effective_address()` itself is UNCHANGED (`git diff` against the M21 commit for `v9958_vdp.cpp`'s `effective_address()`, empty diff) and that the new functions are independently hand-verified against `Graphic4Mode`-`NonBitmapMode::addressOf` directly. |
| R-M22-9 | The 3 CPU-transfer commands (LMCM/LMMC/HMMC) are collapsed into the SAME atomic-completion model as the other 10 commands, silently dropping all but the first pixel/byte of a multi-pixel transfer (a genuine correctness defect, not merely a timing simplification, §1.4 Resolution 2 point 1). | Unit test issues an LMMC/HMMC transfer of NX×NY > 1 pixels/bytes, feeding each one via a SEPARATE `write_register` call to R#44, and confirms ALL NX×NY destination bytes/pixels are correctly written (not just the first), with CE clearing only after the LAST one. |
| R-M22-10 | Sprite compositing is implemented as a SEPARATE, consumer-visible output (a second "sprite plane" `FrameBuffer`-like type) rather than folded into `VdpFrameRenderer::render_frame()`'s existing single-buffer pipeline (§1.4 Resolution 1), diverging from the reference's own single-buffer-overdraw architecture and creating a future compositing-duplication burden for the SDL3 frontend (M26). | Implementation report confirms `FrameBuffer`'s public shape is UNCHANGED from M21 (still one `width`/`height`/`pixels`/`border_color`); sprite pixels are visible in the SAME `pixels` array `render_frame()` already returns, with no new output type introduced. |
| R-M22-11 | The FULL M1-M21 regression suite (106 tests) is not actually independently re-run by both the developer AND QA — only the "obviously sprite/command-adjacent" subset is checked, missing the human's explicit "deliberate cross-check across the entire system" requirement. | Both the developer's implementation report and the QA sign-off MUST record a fresh, full `ctest` run (all 106 prior tests plus new M22 tests) with an explicit pass count, not a claim of "no sprite/command-adjacent test failed." |
| R-M22-12 | The A/B evidence plan claims parity for a sub-feature (e.g. the command engine's internal ASX/ADX/ANX working registers, or the exact TR/CE cycle-by-cycle timing) that openMSX's Tcl debugger genuinely does not expose comparably — fabricating parity evidence for an unverified technique. | §2.6 explicitly requires a live `debug list`-style feasibility check BEFORE claiming a technique works; any genuinely-infeasible sub-claim is reported BLOCKED in `docs/m22-parity-trace-diff.md`, never silently upgraded to PASS. |
| R-M22-13 | S#6 bit1 (the documented, but even-in-the-reference-itself UNIMPLEMENTED, EO copy) is "fixed" beyond what openMSX's own source models, creating an unwarranted independent-ground-truth claim (A-M22-13, mirroring the M21 A-M21-7 precedent). | Implementation report and QA sign-off explicitly confirm S#6 bit1 is modeled as always 0 (inherited gap, not independently resolved), citing `SpriteChecker.hh:378-383`'s own disclosed uncertainty. |

---

## 7. Developer Handoff

- **Start at M22-S1** (sprite engine infrastructure + Sprite Mode 1) — grounded per §2.2; cite
  `references/openmsx-21.0/src/video/{SpriteChecker,SpriteConverter}.{hh,cc}` line ranges in code
  comments (never copy the code itself — GPL isolation).
- **Sequence S1→S8 in order**; each runs the full evidence-gate set INCLUDING the full M1-M21
  regression (106 tests), not a subset; keep `ctest` green every cycle.
- **`src/` placement fixed by §2.1**: new files stay in the EXISTING `src/devices/video/` family
  (no new top-level device family). `SpriteEngine`/`VdpCommandEngine` are owned INSIDE `V9958Vdp`
  as private members (mirrors the existing `blink_countdown_`/`blink_state_` ownership style, NOT
  new machine-level siblings like `VdpFrameRenderer`) — this is a deliberate architectural choice
  (§1.4), not the developer's to relitigate without a documented reason. NO machine-level wiring
  changes are needed (`vdp()`/`render_frame()` already suffice).
- **Critical architectural findings to honor:**
  - Sprite pixels composite INTO the SAME `FrameBuffer` `render_frame()` already returns — never a
    separate sprite-plane output (§1.4 Resolution 1, R-M22-10).
  - The command engine's execution model is HYBRID: atomic for 10 commands, event-driven stateful
    (never wall-clock) for LMCM/LMMC/HMMC (§1.4 Resolution 2, R-M22-9). Do not collapse the
    transfer commands into the atomic model — that silently drops data, a real defect.
  - D7's command-engine-path piece needs FIVE NEW, coordinate-based address functions
    (`vdp_command_address.h`) — NOT a call to `effective_address()`, which cannot even accept X/Y
    parameters. They ARE mathematically consistent with the existing rotate-right-by-1 model for
    G6/G7 (independently hand-verified in §1.5) but are genuinely separate functions (R-M22-8).
  - Commands address BOTH pages of a bitmap mode DIRECTLY through the Y-coordinate's own range
    (`y&511` for G6/G7, `y&1023` for G4/G5) — R#2's display-page-select bits have NO effect on
    command addressing (R-M22-7). Do not copy the display path's page-resolution logic here.
  - EC=bit7(0x80), CC=bit6(0x40), IC=bit5(0x20) (A-M22-10, R-M22-3) — independently re-verify, do
    not transpose.
  - IC does NOT suppress rendering, only collision eligibility (A-M22-11, R-M22-4) — a real,
    easy-to-mis-implement distinction.
  - Color-0 sprite-pixel transparency is conditioned on R#8 TP (matching the SOURCE), NOT an
    unconditional "always transparent" (contradicting the fact-sheet's own prose, A-M22-12,
    R-M22-5) — ground truth is the read source code.
  - S#0 read clears ONLY bits7/6/5, leaving the 5th/9th-sprite-number field (bits4-0) stale until
    the next frame recompute (A-M22-14, R-M22-6).
  - S#6 bit1 (EO copy) is inherited as always-0, matching openMSX's OWN disclosed "not yet
    implemented" gap — do not over-claim a fix beyond the reference (A-M22-13, R-M22-13).
  - Only 5 of 13 commands (PSET/LINE/LMMV/LMMM/LMMC) consult the CMD low nibble for a logical op;
    the other 8 always perform a fixed, implicit operation (A-M22-4, R-M22-1).
  - HB-F1XV has no extended VRAM; MXS/MXD/R#45-bit6 are stored but always treated as false
    (A-M22-8).
- **No new clock consumer (§2.5):** `SpriteEngine::recompute_frame()` hooks the EXISTING
  `on_vsync()` call; the command engine's transfer commands advance only on real I/O accesses.
- **Full regression discipline (the human's explicit standing instruction for this M21-M23 run):**
  every slice's evidence gate must include a FRESH, FULL `ctest` run (all M1-M21 tests plus new M22
  tests) — not an assumption that "untouched files can't have regressed." QA must independently
  reproduce this, not rubber-stamp the developer's count.
- **A/B (§2.6):** build `tools/gen-m22-sprite-cmd-probe.py` + `tools/openmsx-m22-sprite-cmd-
  parity.ps1` using the raw-byte/register comparison technique; verify feasibility (live `debug
  list` query) before claiming any specific introspection point works; report BLOCKED honestly for
  any sub-claim that turns out infeasible. Cover: sprite collision/5th-sprite status (incl. IC=1
  non-collision and TP-disabled color-0 collision), command-engine VRAM writes (incl. a G6/G7
  destination cross-validating D7's closure), and the transfer-command TR/CE sequence.
- **Ledger discipline (DEC-0005):** on closure, mark D2/D3 `DONE (M22)` and D7 `DONE (M22)` (no
  longer partial) in `agent-protocol/state/deferred-backlog.md`; update `state/current-phase.md`
  and `state/milestones.md`.
- **Gate:** per `agent-protocol/state/current-phase.md`, the coordinator is pre-authorized to
  proceed through the release-decision/tag step for this specific M21-M23 run on a clean QA PASS —
  but STOP and consult the human if QA does not reach a clean PASS.
- Deliverables: source per §2.1; tests per §3 (unit + integration + a dedicated system test);
  `docs/m22-parity-trace-diff.md`; refreshed `docs/asset-checksums.txt`; an implementation report
  `docs/m22-implementation-report.md`; then hand to QA for `docs/m22-qa-signoff.md`.
