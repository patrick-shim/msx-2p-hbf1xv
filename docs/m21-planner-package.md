# M21 Planner Package — VDP Rendering Depth: Pixel Pipeline, YJK/YAE Color Decode, Scroll/Interlace/Blink, G6/G7 Planar Interleave

- Milestone ID: M21
- Title: VDP Rendering Depth — Pixel Pipeline, YJK/YAE Color Decode, Scroll/Interlace/Blink, G6/G7 Planar Interleave
- Spec Owner: MSX Planner Agent
- Developer Owner: MSX Developer Agent
- QA Owner: MSX QA Agent
- Request: REQ-M21-001 (planner-first, no production code)
- Decisions in force: DEC-0005 (backlog disposition discipline — every planner package restates all
  rows), DEC-0009 (indicative order: "M21 VDP rendering (D1/D5/D6/D7)"). No new decision entry is
  required to open M21 (already logged, REQ-M21-001, `agent-protocol/state/current-phase.md`).
- Backlog effect: **closes D1 and D5 in full this cycle; closes D6 in full (with one explicit,
  grounded scope note on superimpose, §1.2); advances D7 PARTIALLY** — the CPU-port and
  display-path planar-interleave pieces close this cycle, the command-engine-path piece is
  explicitly carried to M22 (D3). See §1.4 for the full resolution of the scope-boundary question
  the task requires, and §4 for the backlog-registry disposition.
- Gate: normal human-release-decision gate applies, but per `agent-protocol/state/current-phase.md`
  the coordinator is pre-authorized to proceed through the release-decision/tag step for this
  specific M21-M23 run without an additional pause request, UNLESS QA does not reach a clean PASS
  (real blocker → stop and consult the human).

> Grounding rule: every behavior-affecting claim below cites a concrete `references/openmsx-21.0/...`
> path with line numbers, independently re-derived (not transcribed) by the planner this cycle,
> per the task's explicit instruction and this project's demonstrated transcription-risk history
> (DEC-0016, DEC-0012, the M19 Konami mirror-quirk correction). openMSX source is the BEHAVIOR
> reference only (GPL) — **never copied into `src/`**.

---

## 1. Scope and Assumptions

### 1.1 In scope

- **(a) A deterministic, SDL3-independent rendering-output architecture** (`VdpFrameRenderer` +
  `FrameBuffer`, §2.1) — an in-memory RGB555 pixel buffer that unit/integration tests construct by
  driving VRAM + registers through the EXISTING M14 `V9958Vdp` port contract (`#98/#99/#9A/#9B`) and
  then asking the renderer to produce a frame, asserting on raw pixel bytes (never PNG files).
- **(b) Backlog D1** — pixel-accurate raster decode for every Target-Spec text/graphics mode:
  TEXT1, TEXT1Q (V9958 renders this as blank, §1.3 A-M21-6), TEXT2, GRAPHIC1, GRAPHIC2, GRAPHIC3,
  MULTICOLOR, MULTIQ (blank, same reason), GRAPHIC4 (SCREEN5), GRAPHIC5 (SCREEN6), GRAPHIC6
  (SCREEN7), GRAPHIC7 (SCREEN8); border/backdrop color; blink (TEXT2 flip-color + bitmap-mode
  page-flip blink); per-scanline output granularity.
- **(c) Backlog D5** — YJK (SCREEN12) and YJK+YAE (SCREEN10/11) color decode: Y/J/K unpack, the
  documented `R=clamp(Y+J)`/`G=clamp(Y+K)`/`B=clamp((5Y−2J−K+2)/4)` formulas (independently
  re-derived and cross-checked against `BitmapConverter.cc`, §2.3), and the 3-bit→5-bit palette
  expansion table used for every non-YJK palette color output too.
- **(d) Backlog D6** — horizontal scroll (R#25/26/27), vertical scroll (R#23), blink timing (R#12/13),
  interlace/EO field addressing for bitmap modes (R#9 IL/EO, hedged per §1.3 A-M21-7), and an
  explicit, grounded disposition of superimpose/digitize (§1.2).
- **(e) Backlog D7, PARTIAL** — the G6/G7 (and YJK, since YJK overlays GRAPHIC7's base) VRAM
  address-interleave transform, for BOTH: (i) the CPU-port `#98` access path (an edit to the
  already-shipped M14 `V9958Vdp::effective_address()`), and (ii) the display-path row-pointer
  resolution the new renderer needs for planar modes. The command-engine-path interleave (needed
  once M22's VDPCmdEngine-equivalent exists) is explicitly OUT — see §1.4 for the full resolution.
- **(f) Deterministic unit + integration + a dedicated system integration test**, zero regression
  across the FULL M1-M20 suite (95 tests), independently re-verified by both the developer and QA
  per the human's explicit "deliberate cross-check across the entire system" directive.
- **(g) Real openMSX A/B evidence** using a derived-value comparison technique (§2.7) — genuinely
  headless and deterministic, since a literal screenshot-pixel diff is not a clean fit for this
  milestone's content (explained and honestly scoped, not fabricated).
- **(h) Full deferred-backlog review** — all 34 rows re-affirmed (§4).

### 1.2 Out of scope (named explicitly, each with justification)

| Item | Why OUT of M21 | Owning milestone (candidate) |
|---|---|---|
| **Sprite rendering + collision** (backlog D2) | Named as a separate backlog row with its own grounding (`SpriteChecker.cc/.hh`); a materially different subsystem (OBJ-plane compositing, 5th-sprite/collision detection) from the VRAM→background-plane pixel decode this milestone covers. | M22 (paired with D3, per the indicative order) |
| **VDP command engine** (backlog D3) | A completely separate register/handshake subsystem (R#32-46, `VDPCmdEngine.cc`) that WRITES VRAM under program control; D1's renderer only READS whatever bytes are already resident, regardless of how they got there (CPU port or, later, the command engine). | M22 |
| **Cycle-accurate VDP access-slot/command timing, exact sub-frame IRQ raster position** (backlog D4) | M21's renderer is a **pull-model, frozen-register-snapshot** renderer (§2.2) — it decodes "whatever the current register/VRAM state says" into a full frame in one call, not a per-VDP-cycle-scheduled raster generator. Raster-split effects from register changes MID-FRAME, and the odd field's "half a line lower" TIMING position (`VDP.hh:396-397`, a positioning/sync concern, not a color-decode concern) are explicitly deferred to D4. | M23 (paired with C1/C2) |
| **Superimpose/digitize/external-video compositing** (part of D6's original wording) | The HB-F1XV **has no digitizer/genlock hardware** — confirmed by the Target Machine Specification's I/O-ports list (no digitizer/composite-in port) and the fact-sheet §9: "HB-F1XV is not a digitizer/superimpose model, so these are typically unused there." M21 correctly models this by (i) keeping R#0 TP already-stored (M14) and (ii) NOT fabricating an external-video compositing path this real machine cannot have. This is a grounded closure, not a shortcut. | N/A — not buildable without hardware this machine does not have |
| **A real SDL3 presentation surface** | Backlog C9 (SDL3 frontend) is OPEN, not yet built (M26). `FrameBuffer` is a pure in-memory contract; converting it to an actual displayed/host-scaled image is frontend scope. | M26 |
| **PNG/visual debug-export tooling for the new framebuffer** | `tools/mem-to-png.py` (M10) already exists for RAW byte visualization; a proper per-mode-aware PNG dump of `FrameBuffer` content would be a nice-to-have debug convenience, not a test-suite requirement (the task brief is explicit: "tests must assert on raw pixel-buffer bytes, not rendered PNG files"). Left as an optional, non-blocking developer convenience. | Optional, non-blocking this cycle |

### 1.3 Assumptions (each grounded, each with a verification action)

- **A-M21-1 (rendering must be exercisable purely through the EXISTING M14 port contract — no new
  VRAM/register write path).** Confirmed by reading `src/devices/video/v9958_vdp.h/.cpp` in full
  this cycle: `io_write`/`io_read` already dispatch `#98` (VRAM data), `#99` (register/address +
  status), `#9A` (palette), `#9B` (indirect register); `vram()` and `control_register(i)` and
  `palette_entry(i)` and `mode()` are already exposed, const, side-effect-free accessors. The new
  renderer needs ZERO new mutators on `V9958Vdp` (only one internal edit, §1.4, to
  `effective_address()`) — it is a pure CONSUMER of the already-shipped M14 surface. *Verify:* the
  renderer's constructor/render call takes `const V9958Vdp&` (or an equivalent read-only view) with
  no additional friend/mutator access required beyond the one D7 edit.
- **A-M21-2 (pixel format: native 15-bit 5:5:5 RGB, not a host 32-bit ARGB).** The fact-sheet §9
  states the V9958 DAC is physically "15-bit (5:5:5)". Choosing `std::uint16_t` RGB555 (bits
  14-10=R, 9-5=G, 4-0=B, bit15 reserved as a transparency/keying metadata flag, §2.1) as the
  canonical `FrameBuffer` pixel type means the renderer never needs openMSX's own precomputed
  32768-entry `palette32768`/`V9958_COLORS` host-pixel lookup table (`SDLRasterizer.cc:263-282`) —
  YJK's R/G/B values ARE already 0-31 (5-bit) after the documented clamp, so they can be packed
  directly with no LUT. This is a deliberate simplification, not a divergence: any future SDL3
  frontend (M26) widens RGB555→host format as a separate, trivial, one-line-per-pixel step.
  *Verify:* unit test asserts `pack_rgb555`/`unpack` round-trips exactly for all 32768 values.
- **A-M21-3 (3-bit→5-bit palette expansion table, independently re-derived, not transcribed).**
  `references/openmsx-21.0/src/video/SDLRasterizer.cc:286-291`: `r5 = (r3 << 2) | (r3 >> 1)`.
  Re-computed this cycle for all 8 inputs: `0→0, 1→4, 2→9, 3→13, 4→18, 5→22, 6→27, 7→31` — matches
  the fact-sheet §5's stated table exactly (`references/fact-sheets/Yamaha V9958 VDP.md:103`).
  Applies to: (a) the 16-entry palette register file for every non-G7/non-YJK color mode, and (b)
  G7's own fixed 256-color decode (next item) which reuses the SAME expansion after re-deriving a
  pseudo-3-bit value from its 2-bit blue field.
- **A-M21-4 (GRAPHIC7/SCREEN8 fixed-color byte layout is `GGGRRRBB`, NOT the naively-expected
  `RRRGGGBB` — independently re-derived, a genuine risk this cycle's own grounding surfaced,
  R-M21-1).** `SDLRasterizer.cc:330-336`:
  ```
  PALETTE256[i] = V9938_COLORS[(i & 0x1C) >> 2][(i & 0xE0) >> 5][(i & 0x03)==3 ? 7 : (i&0x03)*2];
  ```
  `V9938_COLORS` is indexed `[r3][g3][b3]` (confirmed by its construction loop at
  `SDLRasterizer.cc:304-314`, `V9938_COLORS[r][g][b] = ...`). Substituting: the FIRST index
  (bits `(i&0x1C)>>2` = bits 4-2 of the byte) is **R**; the SECOND index (bits `(i&0xE0)>>5` = bits
  7-5) is **G**; the THIRD index is a pseudo-3-bit **B** derived from the byte's low 2 bits via the
  mapping `{0→0, 1→2, 2→4, 3→7}` (re-derived: `(i&3)==3 ? 7 : (i&3)*2`). So the byte layout is
  **G7,G6,G5, R4,R3,R2, B1,B0** (`GGG RRR BB`), i.e. green occupies the TOP 3 bits, not red — the
  opposite of what a naive "RGB reading order" assumption would produce. *Verify:* a dedicated unit
  test asserts `render(byte=0b111_000_00)` (green=7,red=0,blue=0) produces max-green, zero-red,
  zero-blue output — NOT max-red.
- **A-M21-5 (YJK's B-channel formula must use C++ truncating integer division, not
  `floor()`/`std::floor` — a genuine, independently-identified rounding risk, R-M21-2).**
  `BitmapConverter.cc:217-228` (`yjk2rgb`): `b = std::clamp((5*y - 2*j - k + 2) / 4, 0, 31)` where
  `y,j,k` are plain `int`. C++'s `/` on `int` truncates TOWARD ZERO, not toward negative infinity;
  for negative numerators (achievable here since `j,k` are signed 6-bit values in `-32..31` and `y`
  is `0..31`, so `5y-2j-k+2` can be negative) truncation-toward-zero and mathematical floor DIVERGE
  by exactly one for negative-but-not-multiple-of-4 numerators. The fact-sheet's own prose
  ("`B = clamp(floor((5·Y−2·J−K+2)/4), 0, 31)`", `Yamaha V9958 VDP.md:109`) uses the word "floor",
  which is a natural-language description of what plain C `/` already does for this specific
  reference implementation — it does NOT mean "the developer should call `std::floor()`". Since our
  emulator is also C++ with `int` operands, using plain `/` produces the IDENTICAL bit pattern to
  openMSX automatically; the risk is a developer "clarifying" the code with an explicit
  floating-point `floor()` call, which would silently diverge for negative numerators before
  clamping. *Verify:* unit test picks Y/J/K values that make the raw (pre-clamp) numerator negative
  and not a multiple of 4, and asserts the truncating-`/` result, not the floor-of-a-more-negative
  value.
- **A-M21-6 (TEXT1Q/MULTIQ and any other undefined mode-byte render V9958 "blank" fill, never
  TMS9918-compatible content — independently re-derived, a genuine risk, R-M21-6).**
  `CharacterConverter.cc:64-77`: `if (vdp.isMSX1VDP()) { renderText1Q/renderMultiQ(...) } else {
  renderBlank(...) }`. The HB-F1XV is a V9958-based MSX2+ machine — `isMSX1VDP()` is always FALSE
  for this Target Machine Spec — so TEXT1Q (base `0x05`) and MULTIQ (base `0x06`), and (per
  `CharacterConverter.cc:78-84`) every OTHER undefined base combination, all resolve to
  `renderBlank()`: `std::ranges::fill(buf, palFg[15])` (`CharacterConverter.cc:368-373`) — a flat
  fill of palette entry 15, NOT the striped `renderBogus()` pattern (which is MSX1-only,
  `:352-366`, and never reachable on this Target-Spec machine either). `VdpMode::Text1Q`,
  `MulticolorQ`, and `Unknown` (already-decoded identities in the EXISTING M14
  `src/devices/video/vdp_mode.h`) must all render via the SAME flat-fill path. *Verify:* unit test
  sets R#0/R#1 to the TEXT1Q bit pattern and asserts a flat, single-color output (palette entry 15),
  not a striped pattern and not TMS9918 character-cell content.
- **A-M21-7 (interlace/EO bitmap-mode page-alternation addressing is hedged, matching openMSX's own
  disclosed uncertainty — an honest scope statement, not a fabricated ground truth).**
  `VDP.hh:420-430` (`isEvenOddEnabled`) carries the reference project's OWN comment: `"TODO: Find
  out how real VDP does it... If it handles it dynamically (my guess), then an update method should
  be added"` and `getEvenOddMask()` (`:443-459`) says `"TODO: Verify which page is displayed on even
  fields."` Since even the openMSX authors — the best-available behavior reference for this project
  — flag this as unverified, M21's acceptance bar for this ONE narrow sub-feature (bitmap-mode
  even/odd VRAM-page alternation, used together with interlace) is **parity with openMSX's own
  modeled behavior** (reproducing `getEvenOddMask()`'s exact bit formula, independently re-derived at
  implementation time), not an independent, higher-confidence ground-truth claim beyond what the
  reference itself asserts. This is disclosed explicitly here, per the task's "honest BLOCKED"
  discipline extended to "honest, hedged confidence level" for a sub-feature that is not fully
  blocked (it IS testable/buildable, just inherits the reference's own documented uncertainty).
  *Verify:* the A/B evidence for this specific sub-claim is framed as "matches openMSX's own
  computed value," not "independently proven correct against real hardware."
- **A-M21-8 (horizontal scroll is applied in DIFFERENT places for character modes vs. bitmap modes —
  must not be assumed uniform, R-M21-8).** `CharacterConverter.cc`'s `renderGraphic1`/`renderGraphic2`/
  `renderMultiHelper` (:255-350) already take `vdp.getHorizontalScrollHigh()` (R#26,
  `VDP.hh:344-351`) as an internal parameter and apply it to NAME-TABLE indexing directly inside the
  per-mode converter. `BitmapConverter`'s `convertLine`/`convertLinePlanar` signatures (`.hh:45-66`)
  have NO scroll parameter at all — for bitmap modes (G4-G7/YJK), horizontal-scroll windowing is
  applied by the RASTERIZER at a layer this project has not yet built (`SDLRasterizer.cc`'s own
  `drawDisplay`, not read in full this cycle — deliberately left for the developer to independently
  ground during S6, rather than the planner asserting an unread formula). *Verify:* the S6
  implementation report cites the specific `SDLRasterizer.cc` line range it derived the bitmap-mode
  scroll-window formula from, independently re-computed, not assumed-uniform with the character-mode
  mechanism.
- **A-M21-9 (MULTICOLOR's fact-sheet "64×48" is a COLOR-CELL grid, not the host pixel-canvas
  dimensions — independently re-derived, a genuine risk, R-M21-5).** `Yamaha V9958 VDP.md:55`
  lists MULTICOLOR's "Resolution" as "64×48". But `CharacterConverter.cc:318-350`
  (`renderMultiHelper`/`renderMulti`) draws 8 HOST pixels per iteration (`pixelPtr[0..7]`, 2 colors
  of 4px each) × 32 iterations/line = 256 host pixels per line, over 192 lines (`line/4` selects one
  of 48 block-rows) — i.e., the actual pixel CANVAS is 256×192, identical in raster size to
  GRAPHIC1/2/3; "64×48" describes the BASIC-visible 4×4-pixel COLOR-CELL grid (64 cells × 48
  cell-rows), not a literal 64×48-pixel image. A naive read of the fact-sheet table could lead to
  rendering a genuinely 64×48-pixel image, which would be WRONG. *Verify:* MULTICOLOR unit test
  asserts a 256-wide row buffer with each of the 64 4-pixel color groups holding one of 2 possible
  colors per group (matching the block structure), not a 64-wide buffer.
- **A-M21-10 (the G6/G7 planar transform is a 17-bit ROTATE-RIGHT-BY-ONE, independently re-derived
  bit-for-bit — not a naive "swap nibbles"/byte-interleave guess, R-M21-3).**
  `references/openmsx-21.0/src/video/VDP.cc:849-857` (`executeCpuVramAccess`):
  ```
  int addr = (controlRegs[14] << 14) | vramPointer;
  if (displayMode.isPlanar()) {
      addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF;
  }
  ```
  Independently re-derived this cycle: for a 17-bit `addr` (range `0..0x1FFFF`), `(addr << 16) &
  0x1FFFF` keeps ONLY `addr`'s bit 0, relocated to bit 16 (every other shifted-up bit lands at bit
  17+ and is masked away) — i.e. `(addr << 16) & 0x1FFFF == (addr & 1) << 16`. Therefore the whole
  expression reduces EXACTLY to `physical = (addr >> 1) | ((addr & 1) << 16)`, i.e. a **17-bit
  rotate-right-by-1**: even logical addresses (bit0=0) map to physical `0x00000-0x0FFFF` (bank 0);
  odd logical addresses (bit0=1) map to physical `0x10000-0x1FFFF` (bank 1), with the same
  `addr >> 1` value in both banks. This is confirmed by TWO independent call sites computing the
  IDENTICAL formula: `VDPVRAM.cc:47-53` (`LogicalVRAMDebuggable::transform`, byte-for-byte the same
  expression) and `VDPVRAM.hh:244-261`/`271-279` (`getReadAreaPlanar`/`readPlanar`, same rotate
  expressed as `(index & 1) << 16 | (index & 0x1FFFE) >> 1`, algebraically identical for the masked
  index domain). Extended/expansion VRAM interleaved-with-itself (`VDP.cc:853-855`'s comment) is
  **not applicable** to HB-F1XV (128 KB fixed, no expansion socket, fact-sheet §2) and is explicitly
  a non-goal (§1.2 already lists no other 192KB-mode features; this is reiterated here to prevent
  scope creep into modeling a socket this machine does not have). *Verify:* unit test writes to
  logical address 1 (odd) and asserts the byte physically lands at `vram().read(0x10000)`, and
  logical address 0 (even) lands at `vram().read(0x00000)` — both via the device's OWN raw-physical
  debug accessor, independent of the CPU-port read-back path (which would trivially round-trip
  either way, per §1.4's own analysis).
- **A-M21-11 (the renderer's own planar row-pointer resolution for a CONTIGUOUS row is a direct
  bank-split, not a byte-by-byte rotate loop — proven equivalent, not merely convenient).** For a
  display-line's LOGICAL byte range `[base, base+n)` where `base` is always even (G6/G7 table
  geometry: 256 logical bytes/row, `base = line * 256`, always even), the rotate-right-by-1 formula
  applied to each byte in the range degenerates to two CONTIGUOUS half-length spans:
  `physical_bank0[i] = vram[(base >> 1) + i]` (even logical bytes) and
  `physical_bank1[i] = vram[0x10000 + (base >> 1) + i]` (odd logical bytes), for `i` in
  `0..(n/2)-1`. Independently re-derived this cycle AND cross-confirmed against openMSX's own
  identical helper, `VDPVRAM.hh:236-261` (`getReadAreaPlanar`): `addr = effectiveBaseMask &
  (indexMask | (index >> 1)); ptr0 = &data[addr | 0x00000]; ptr1 = &data[addr | 0x10000];` — the
  SAME `index >> 1` bank-split-with-half-length-run structure. This lets the renderer reuse
  `BitmapConverter`'s own `convertLinePlanar(buf, vramPtr0, vramPtr1)`-style two-span API
  (`BitmapConverter.hh:64-66`) without a per-byte transform loop. *Verify:* unit test cross-checks
  the bank-split helper against a byte-by-byte application of A-M21-10's rotate formula for a full
  256-byte synthetic row, asserting identical results.
- **A-M21-12 (the CPU-port `advance_vram_pointer()`/R#14-carry logic is UNCHANGED by the D7 edit —
  only the STORAGE address computation changes, a low-risk, surgical edit to M14's existing
  `V9958Vdp::effective_address()`).** In `VDP.cc:849-887`, the planar transform (lines 851-857) is
  applied to a LOCAL variable (`addr`) used only to index `vram->cpuRead/cpuWrite`; the
  pointer-increment/R#14-carry logic at lines 883-887 operates on the ORIGINAL `vramPointer`
  variable, never reassigned by the planar branch. This means M21's edit to
  `V9958Vdp::effective_address()` (adding a conditional planar transform) needs ZERO changes to
  `advance_vram_pointer()` — the already-QA-verified M14 auto-increment/carry unit tests remain
  valid unmodified. *Verify:* the existing M14 VRAM-pointer-carry unit tests
  (`tests/unit/devices/video_v9958_ports_unit_test.cpp`) are re-run and confirmed green, unedited,
  after the D7 change.
- **A-M21-13 (the D7 edit touches an already-shipped, QA-signed M14 file — flagged explicitly for
  coordinator visibility, mirroring the R-M20-7 precedent of surfacing non-obvious architectural
  calls even when not strictly decision-gated).** `src/devices/video/v9958_vdp.cpp`'s
  `effective_address()` is edited to add the D7 planar transform. This is squarely WITHIN M21's own
  authorized scope (REQ-M21-001 names D7 explicitly, and the backlog's own "Candidate owner" column
  for D7 already reads "Video-rendering / command milestone" — i.e., THIS milestone), not a
  surprise cross-milestone rearrangement requiring a new `decisions.md` entry (contrast DEC-0008,
  which authorized BROADER M1-M14 rearrangement for M15) — but it is called out explicitly here so
  the coordinator/human is never surprised that an M14 file changed during M21. *Verify:* the M21
  implementation report names this file/diff explicitly as a "small, additive, single-function
  edit," and the regression suite re-confirms every M14 unit test green.

### 1.4 The D7 scope-boundary resolution (required by the task; resolved by direct source reading, not assumed)

The task requires an explicit answer to: does M21 close D7 in full, partially, or leave it
OPEN/IN-PROGRESS? The backlog's own text says the interleave is "only observable once D1/D3
exist." Direct source reading (§1.3 A-M21-10/A-M21-11) shows this framing is **incomplete**: the
interleave transform is applied by openMSX at (at least) THREE independent call sites, not one:

1. **The CPU-port access itself** (`VDP.cc:849-857`, `executeCpuVramAccess`, the function directly
   behind the `#98` data port) — this has **zero dependency on a renderer (D1) or a command engine
   (D3)**. It is observable TODAY, purely through the already-shipped M14 contract: write a byte at
   a logical VRAM address while in G6/G7 mode, then use the VDP's own raw-physical debug accessor
   (`vram().read(physical_address)`, already exported by M14) to confirm it landed at the
   ROTATED physical address, not the flat logical one. This is squarely **M14/M21 CPU-port-contract
   depth**, independently closable with no dependency on M22.
2. **The display/renderer's own row-pointer resolution** (`VDPVRAM.hh:236-261`/`271-279`,
   consumed by `BitmapConverter::convertLinePlanar`) — this is exactly D1's own new scope THIS
   milestone (M21 is building the renderer). Also independently closable in M21, since the
   renderer reads whatever is ALREADY in the (correctly interleaved, per point 1) flat VRAM store —
   it does not need a command engine to exist.
3. **The VDP command engine's own coordinate-to-VRAM-address resolution** (`VDPCmdEngine.cc`, not
   read in depth this cycle — correctly left for the M22 planner to ground) — this is the ONLY
   piece that genuinely depends on D3 (the command engine itself doesn't exist yet in this
   codebase), since it is the command engine's OWN internal address computation for commands like
   HMMM/LMMM/LINE/PSET operating in G6/G7 coordinate space.

**Resolution: M21 closes D7's CPU-port piece (1) and display-path piece (2) — both are genuinely
in-scope for this milestone with no dependency on M22. D7's command-engine-path piece (3) remains
carried forward, to be closed together with D3 when M22 lands.** This is NOT a force-close (per
DEC-0005 discipline / the M14-M20 contract-vs-depth precedent): D7 stays as ONE tracked row, marked
`IN-PROGRESS (M21 partial)` in the backlog (§4), mirroring the C5 precedent (a single row tracked
IN-PROGRESS across M15→M16, not force-closed and not split into a brand-new letter-row, because the
remainder is not "newly discovered depth" but the ALREADY-NAMED other half of D7's own original
definition). The M22 planner must explicitly close D7 fully alongside D3.

Two consequences flow from this:
- Because the CPU-port piece (1) is now correctly understood as its OWN closable slice of work
  (not gated on D1's renderer existing at all), M21's slice plan places it early (S4, layered onto
  the G6/G7 bitmap-mode slice) rather than treating it as a trailing afterthought.
- Because BOTH the CPU-port write path (1) and the renderer's read path (2) must apply the
  IDENTICAL transform to the SAME flat physical store, any test that writes via the CPU port and
  reads back via the renderer (a genuine end-to-end G6/G7 test) is a strong correctness signal: if
  either side's transform has an off-by-one/rotate-direction bug, the round-trip will visibly
  produce garbled/mirrored pixel content, not silently pass (unlike a CPU-port-only round-trip test,
  which — per A-M21-10's own analysis — would pass even with the transform OMITTED entirely on both
  sides, since composing the same bijective map with itself at write and read is self-consistent
  regardless of which map is used). The S4/S7 test plan (§3) includes this specific style of
  cross-path assertion deliberately.

---

## 2. Spec Summary

### 2.1 `src/` placement (per `src/CLAUDE.md`)

New files, all in the EXISTING `src/devices/video/` family (mirrors the M14 precedent of keeping
the whole V9958 subsystem — contract AND now rendering depth — together; no new top-level device
family is warranted, unlike M18/M19/M20's genuinely new device classes):

| File | Responsibility | Grounding |
|---|---|---|
| `src/devices/video/vdp_palette.h` (**new**, header-only, pure functions) | `expand3to5(std::uint8_t c3) -> std::uint8_t` (the fixed table, A-M21-3); `pack_rgb555(r5,g5,b5) -> std::uint16_t`; `palette_entry_to_rgb555(std::uint16_t grb9) -> std::uint16_t` (unpacks the VDP's stored 9-bit GRB palette format — bits 10-8=G,6-4=R,2-0=B per `VDP.hh:274-278`'s documented format, expands each 3-bit component, repacks as RGB555). | `SDLRasterizer.cc:286-296`; `VDP.hh:274-278` |
| `src/devices/video/frame_buffer.h` (**new**) | `enum class Field { Progressive, Even, Odd };` and `struct FrameBuffer { int width; int height; std::vector<std::uint16_t> pixels; std::uint16_t border_color; }` — the deterministic, SDL3-independent output contract (§2.2). | New design, grounded in the fact-sheet's DAC width (§9) and Target-Spec mode resolutions |
| `src/devices/video/vdp_frame_renderer.h` / `.cpp` (**new**) | `class VdpFrameRenderer` — takes a `const V9958Vdp&`; `render_line(int line, Field field, std::span<std::uint16_t> out) const` (variable width per mode, mirrors the per-line unit-testability of `CharacterConverter`/`BitmapConverter`); `render_frame(Field field = Field::Progressive) const -> FrameBuffer` (full active-display area, one call, frozen-snapshot, §2.2); internal per-mode `render_text1/text2/graphic1/graphic2_or_3/multicolor/graphic4/graphic5/graphic6/graphic7/yjk/yjk_yae/blank` helpers (recommended split mirroring openMSX's `CharacterConverter`/`BitmapConverter` two-family precedent — the developer's call on internal class/file organization, per `src/CLAUDE.md`'s "best judgment" latitude, as long as the public `VdpFrameRenderer` facade and `FrameBuffer`/`Field` types are as specified here). | `CharacterConverter.{hh,cc}`, `BitmapConverter.{hh,cc}`, `DisplayMode.hh` (mode dispatch) |

Edits to already-shipped M14 files (additive, surgical, flagged per A-M21-13):

- `src/devices/video/vdp_mode.h`: add `constexpr bool vdp_base_is_planar(std::uint8_t base)` next
  to the existing `vdp_base_is_v9938_mode` — `(base & 0x14) == 0x14`, independently re-derived from
  `DisplayMode.hh:140-143` and verified to correctly identify GRAPHIC6 (`0x14`), GRAPHIC7 (`0x1C`),
  and (since YJK/YAE only set bits 5/6, never touching bits 2/4) both YJK overlay modes too, while
  excluding GRAPHIC5 (`0x10`).
- `src/devices/video/v9958_vdp.h` / `.cpp`: `effective_address()` gains a conditional planar
  transform (D7 CPU-port piece, A-M21-10/A-M21-12) — the ONLY behavior-affecting edit to the M14
  contract this milestone makes. `advance_vram_pointer()` is UNCHANGED (A-M21-12). Add one new
  const accessor if needed for the renderer's direct physical-VRAM-bank access (likely none needed
  beyond the already-public `vram()`).

Machine wiring (`src/machine/hbf1xv_machine.h/.cpp`, additive only):

- `+devices::video::VdpFrameRenderer vdp_frame_renderer_` member (constructed referencing `vdp_`), or
  a lazily-constructed renderer wrapping `vdp_` — developer's call.
- `+FrameBuffer render_frame(Field field = Field::Progressive) const;` accessor, mirroring the
  existing `vdp()` accessor pattern (a thin, additive pass-through — no change to `wire_bus()` or
  `cold_boot()` is required, since the renderer is a pure, on-demand consumer, not a wired bus
  device).

`CMakeLists.txt`: add `src/devices/video/vdp_frame_renderer.cpp` to `sony_msx_core`'s source list.

Boundary compliance: `VdpFrameRenderer` carries no slot/bus knowledge (matches `V9958Vdp` itself);
it is a pure function of VDP state, placed under `src/devices/` per `src/CLAUDE.md` (device-family
logic), with the machine only exposing an accessor (no new frontend concerns leak into
`src/machine/`, matching the boundary rule "keep frontend concerns out of `src/core/`,
`src/devices/`, and `src/machine/`").

### 2.2 Rendering-output architecture (the "hard architectural problem," resolved)

**Model: a deterministic, pull-based, frozen-register-snapshot renderer** — not a per-VDP-cycle
scheduled raster generator (that is D4/C1/C2, explicitly M23's scope, §1.2). A test/tool:

1. Drives VRAM content and mode/palette registers through the EXISTING M14 port API
   (`io_write(#98/#99/#9A/#9B, ...)`, exactly as the existing M14 integration test already does,
   `tests/integration/machine/hbf1xv_vdp_io_integration_test.cpp`).
2. Calls `VdpFrameRenderer::render_frame(field)` (or `render_line(...)` for narrower, faster unit
   assertions) — a SINGLE, synchronous call that decodes the CURRENT (frozen) register/VRAM state
   into a full frame, as if that state held constant for the frame's whole duration.
3. Asserts directly on `FrameBuffer.pixels` (raw `std::uint16_t` RGB555 values) — no PNG, no host
   window, no SDL3.

This is deterministic by construction (pure function of stored state, no wall-clock, no hidden
scheduling) and fully exercises the real M14 contract end-to-end. Mid-frame register-change/raster-
split accuracy is explicitly NOT modeled (a disclosed non-goal, §1.2) — this is the honest
"contract vs. depth" line for D1 vs. D4, exactly mirroring the M14 precedent of shipping a
CONTRACT now and deferring cycle-accurate depth to a later, explicitly-named milestone.

**`FrameBuffer` dimensions** (independently derived from `DisplayMode::getLineWidth()`,
`DisplayMode.hh:176-186`, and `CharacterConverter.cc`'s own draw-call counts — NOT from the
fact-sheet's mode table alone, which conflates cell-grid and pixel-canvas resolution for some modes,
A-M21-9):

| Mode | Width (px) | Height (px, LN=0/LN=1) | Derivation |
|---|---|---|---|
| TEXT1 | 240 | 192 only (text modes ignore LN) | `CharacterConverter.cc:157-161`: 40 chars × 6px |
| TEXT2 | 480 | 192 only | `:208-245`: 80 chars × 6px |
| GRAPHIC1/2/3, MULTICOLOR | 256 | 192 / 212 | `DisplayMode.hh:183-185` (not in the 512 list) |
| GRAPHIC4 (SCREEN5) | 256 | 192 / 212 | `BitmapConverter.cc:108-143` (256 px/line) |
| GRAPHIC5 (SCREEN6) | 512 | 192 / 212 | `DisplayMode.hh:183` (512 list); `BitmapConverter.cc:145-157` |
| GRAPHIC6 (SCREEN7) | 512 | 192 / 212 | `DisplayMode.hh:183`; `BitmapConverter.cc:159-203` |
| GRAPHIC7 (SCREEN8), YJK, YJK+YAE | 256 | 192 / 212 | `DisplayMode.hh:181-182` ("Testing mode instead of base ensures YJK modes are 256"); `BitmapConverter.cc:205-285` |
| TEXT1Q, MULTIQ, Unknown | 256 (flat fill) | 192 | A-M21-6 |

Height doubles to 424 only via the two-field interlace convenience (`Field::Even`/`Field::Odd`,
each still 192/212 tall) — combining two fields into one interleaved 424-line raster (with the
odd field's documented "half a line lower" positioning, `VDP.hh:396-397`) is explicitly OUT of
scope (a raster-positioning/timing concern, §1.2), so M21 does not offer a single
`render_interlaced_frame() -> 424-line buffer` call; it offers the two field buffers as first-class,
separately-assertable output.

**Border/backdrop**: NOT modeled as extra canvas rows/columns (a decision that sidesteps needing
unbuilt raster-geometry timing, e.g. TEXT1's real hardware left/right border width in V9958 dot-
clock units — a D4 concern). Instead, `FrameBuffer.border_color` is a single RGB555 value per
frame, computed via the mode-aware backdrop accessor already groundable at `VDP.hh:211-226`
(`getBackgroundColor`: R#7 bits 3-0 for most modes, all 8 bits for GRAPHIC7, split nibbles for
GRAPHIC5) — satisfying D1's named "border color" acceptance item without fabricating unbuilt raster
geometry.

**Transparency metadata (bit 15 of the RGB555 word)**: reserved, set when R#0's TP bit is active
AND the resolved color index is palette entry 0 — pure bookkeeping for a future compositing
consumer (SDL3/M26); never affects the visible R/G/B value on THIS machine (§1.2, no digitizer
hardware to composite against).

### 2.3 D5 — YJK / YJK+YAE color decode (byte-exact, independently re-derived)

Grounded in `BitmapConverter.cc:217-285` (full re-derivation in A-M21-5 above for the rounding
risk). Per 4-pixel group (2 bytes from each of the two planar banks, `p[0..3]` = plane0-even,
plane1-even, plane0-odd, plane1-odd):

```
k = (p[0] & 7) + ((p[1] & 3) << 3) - ((p[1] & 4) << 3)   // 6-bit signed, range -32..31
j = (p[2] & 7) + ((p[3] & 3) << 3) - ((p[3] & 4) << 3)   // 6-bit signed, range -32..31
for each of the 4 pixels n in 0..3:
    y = p[n] >> 3                                          // 5-bit unsigned (YJK) / 4-bit even-only (YAE, bit3 stolen)
    if YAE and (p[n] & 0x08):                               // attribute bit set -> palette color
        pixel = expand_palette16(p[n] >> 4)
    else:
        r = clamp(y + j, 0, 31)
        g = clamp(y + k, 0, 31)
        b = clamp((5*y - 2*j - k + 2) / 4, 0, 31)           // PLAIN C++ int division (A-M21-5)
        pixel = pack_rgb555(r, g, b)
```

No 32768-entry LUT is needed (A-M21-2) — R/G/B are already 5-bit after the clamp and pack directly.

### 2.4 D6 — scroll/blink/interlace (grounded per-register, hedged where the reference itself is uncertain)

| Feature | Register(s) | Grounding |
|---|---|---|
| Vertical scroll | R#23 | `VDP.hh:328-333` (`getVerticalScroll`) |
| Horizontal scroll, character modes | R#26 (high/coarse) | `VDP.hh:344-351`; applied INSIDE `CharacterConverter`'s own per-mode functions (A-M21-8) |
| Horizontal scroll, bitmap modes | R#26/R#27 (high/low) | `VDP.hh:335-351`; applied by the RASTERIZER layer in openMSX — developer must independently ground the exact windowing formula at implementation time (A-M21-8), citing the specific line range used |
| Border mask (MSK) | R#25 bit1 | `VDP.hh:353-360` |
| Multi-page scroll | R#25 bit0 + R#2 bit5 | `VDP.hh:362-370` |
| Blink (TEXT2 + bitmap page-flip) | R#12 (colors), R#13 (on/off period ×10 frames) | `VDP.cc:600-608`,`1040-1057`; already has a frame-boundary hook point (`on_vsync()`) in the existing M14 `V9958Vdp` |
| Interlace/EO bitmap-mode page addressing | R#9 IL/EO | `VDP.hh:395-475`; **hedged, A-M21-7** — parity-with-openMSX bar, not independent ground truth |
| Superimpose/digitize | R#0 TP | Explicitly scoped N/A for this machine (§1.2) |

Blink timing is modeled as a deterministic, frame-hook-driven countdown (mirroring the EXISTING
M14 `on_vsync()` hook pattern exactly — no new clock-consumer axis, no wall-clock): a
`blink_countdown_`/`blink_state_` pair on `V9958Vdp` (or the renderer, developer's call, though
`V9958Vdp` is the natural owner since `getBlinkState()`-equivalent is queried by the renderer per
scanline in real hardware and the state must persist across `render_frame()` calls) decremented once
per `on_vsync()`, re-armed from R#13 exactly per `VDP.cc:601-608`'s re-derived formula:
`blink_countdown_ = (blink_state_ ? (R13 >> 4) : (R13 & 0x0F)) * 10`.

### 2.5 D7 — G6/G7 planar interleave (both closable pieces; full formula in §1.4/A-M21-10/A-M21-11)

- **CPU-port piece** (edit to `V9958Vdp::effective_address()`): `if
  (vdp_base_is_planar(mode_.base)) { addr = (addr >> 1) | ((addr & 1) << 16); }` applied to the
  already-computed `(R14<<14)|pointer` value, BEFORE indexing `vram_`. `advance_vram_pointer()`
  unchanged (A-M21-12).
- **Display-path piece** (in `VdpFrameRenderer`): a `planar_row_spans(logical_row_base, length) ->
  (span_bank0, span_bank1)` helper implementing the bank-split direct-indexing form
  (A-M21-11), reused by the GRAPHIC6/GRAPHIC7/YJK/YJK+YAE render functions.
- **Command-engine piece**: explicitly NOT built this milestone (§1.4) — carried to M22/D3.

### 2.6 Determinism (hard requirement)

- `VdpFrameRenderer` is a pure function of `V9958Vdp`'s stored state at call time — no
  `elapsed_cycles()` dependency, no new clock consumer (mirrors M17/M19/M20's own "no new clock
  consumer" precedent for pure/combinational devices). The ONE stateful addition (blink countdown,
  §2.4) is driven by the EXISTING frame-boundary hook (`on_vsync()`), not by any new time source.
- Two-run determinism: identical VRAM/register write sequences on two independent `Hbf1xvMachine`
  (or bare `V9958Vdp`) instances produce byte-identical `FrameBuffer` content.
- *Verify:* the M9/M12 CPU-timing oracle suites remain green unmodified (no CPU-visible timing
  change from this milestone at all).

### 2.7 openMSX A/B acceptance plan (derived-value comparison, not screenshot-pixel diff — reasoned explicitly)

**Why not a literal screenshot-pixel diff.** openMSX is fundamentally a visual emulator; a genuine
frame-buffer-vs-frame-buffer comparison would require: (a) a correctly-configured, verified-working
headless/off-screen capture path from the installed WSL `/usr/bin/openmsx` (unconfirmed in this
environment — not to be assumed), and (b) neutralizing openMSX's OWN presentation-layer
transformations (`RenderSettings::transformRGB`/color-matrix/gamma, scaler choice, aspect-ratio
correction, exact border-pixel geometry) to make a fair "raw hardware color" comparison — none of
which are part of the SPECIFICATION being implemented here (they are host-display presentation
choices, not V9958 hardware behavior). Bit-exact agreement on such a comparison would be fragile and
would not actually validate the color-decode ARITHMETIC any better than a more direct technique
would; disagreement could easily be an artifact of the comparison method (incidental scaling/gamma
differences) rather than a real defect. Per the task's own instruction, this specific technique is
therefore **not attempted**, and is reported as an explicit, honest scope boundary (not fabricated),
mirroring the M19 slot-lettering / M20 SHA1-enforcement "honest BLOCKED" precedent — but framed as a
**deliberate technique choice**, not a capability gap, since a stronger, MORE decision-relevant
technique (below) is available and is the one this project has used successfully in every prior
A/B probe.

**The technique used instead: derived-value (color-arithmetic) comparison over the Tcl debugger.**
Mirrors the established M11-M20 methodology (read/write VRAM and registers via the live WSL Tcl
debugger) exactly, extended to VALUES rather than raw bytes for the color-decode claims:

1. Drive VRAM content + mode/palette registers identically on both sides (our engine via the M14
   port API; openMSX via `debug write "VRAM" <addr> <byte>` / equivalent register-write Tcl
   commands — the SAME established technique from `docs/m14-parity-trace-diff.md`).
2. Read back the SAME raw inputs from openMSX's live debugger: VRAM bytes (`debug read "VRAM"
   <addr>`, already-verified technique) and palette register values (verify at implementation time
   whether openMSX exposes a live "VDP regs" debuggable comparable to what M14's own A/B probe
   already used — if not directly readable, reconstruct via the SAME port-write sequence issued
   to both sides, which IS already an established, verified technique).
3. On OUR side, feed the identical raw inputs through our own `VdpFrameRenderer` and record the
   computed RGB555 (or intermediate Y/J/K/R/G/B) values.
4. **The comparison claim is: "given the SAME raw VRAM/register inputs, our documented-formula
   arithmetic computes the value openMSX's own reference implementation computes for those same
   inputs."** This is a STRONGER specification-conformance signal than a final-pixel screenshot diff,
   because it validates the actual arithmetic (the thing D1/D5/D6/D7 actually specify), independent
   of either side's host-presentation choices.
5. Where openMSX does not expose a live introspection point for a computed color value (to be
   verified, not assumed, at implementation time) — e.g., if there is no Tcl-reachable "give me the
   RGB result for this VRAM content" command — the sub-claim is reported **BLOCKED**, honestly, per
   the task's explicit instruction, rather than fabricating a workaround.
6. **Subjects to cover:** (a) 16-color palette expansion (a representative sweep of 3-bit component
   values incl. all 8 boundary values); (b) GRAPHIC7 fixed 256-color decode (incl. the GGGRRRBB
   byte-order boundary case, A-M21-4); (c) YJK/YJK+YAE decode at several representative Y/J/K
   combinations INCLUDING at least one that exercises the negative-numerator rounding boundary
   (A-M21-5); (d) the G6/G7 planar interleave (CPU-port piece): write at chosen even/odd logical
   addresses, confirm the SAME physical bank/offset resolution on both sides via each engine's own
   raw-VRAM debug read.
7. **Adversarial comparator self-check** (as in every prior milestone): confirm an empty-side input
   → BLOCKED and a corrupted field → DIVERGENCE.
8. **Mechanics**: `tools/gen-m21-vdp-render-probe.py` (new, synthetic VRAM/register fixtures per
   mode) and `tools/openmsx-m21-vdp-render-parity.ps1` (new); output `docs/m21-parity-trace-diff.md`.
9. **Hard rule (unchanged)**: no parity claim without a genuine captured diff; the screenshot-diff
   sub-claim is explicitly marked NOT ATTEMPTED / out of technique scope (§ above), not silently
   omitted.

---

## 3. Milestones (Slice Plan M21-S1 … S7)

Every slice runs the full evidence-gate set (§5, item 9) and leaves `ctest` green, including the
**entire M1-M20 suite (95 tests)**, per the human's explicit "deliberate cross-check" directive —
not just VDP-adjacent tests.

### M21-S1 — Rendering infrastructure + two simplest modes (pipeline proof)
- **Goal:** `FrameBuffer`/`Field` types (`frame_buffer.h`); `vdp_palette.h` (3-bit→5-bit expansion,
  RGB555 pack/unpack); `VdpFrameRenderer` skeleton with mode dispatch on `vdp.mode()`; TEXT1 and
  GRAPHIC1 fully implemented (the simplest text mode and simplest tile mode) as the pipeline proof;
  backdrop/border color computed via `getBackgroundColor()`-equivalent logic for these two modes.
- **Files:** `src/devices/video/{frame_buffer.h, vdp_palette.h, vdp_frame_renderer.h, vdp_frame_renderer.cpp}` (new); `CMakeLists.txt`.
- **Unit tests** (`tests/unit/devices/video_vdp_palette_unit_test.cpp`,
  `video_vdp_frame_renderer_text1_unit_test.cpp`, `..._graphic1_unit_test.cpp`): 3-bit→5-bit
  expansion table exact for all 8 inputs (A-M21-3); RGB555 pack/unpack round-trip for boundary
  values; TEXT1 renders correct 240-wide row from name/pattern-table content driven through the
  M14 port API; GRAPHIC1 renders correct 256-wide row from name/pattern/color-table content;
  border color reflects R#7; two-run determinism.
- **Gates:** build + ctest green (full suite).

### M21-S2 — Remaining character-converter modes
- **Goal:** TEXT2 (+ blink flip-color decode, blink countdown state added to `V9958Vdp` per §2.4),
  GRAPHIC2, GRAPHIC3, MULTICOLOR; TEXT1Q/MULTIQ/Unknown → flat-fill blank fallback (A-M21-6).
- **Files:** `vdp_frame_renderer.{h,cpp}` (extend); `v9958_vdp.{h,cpp}` (blink countdown, driven by
  the existing `on_vsync()` hook — additive only).
- **Unit tests**: TEXT2 name/color/pattern triple-lookup + blink-color-flip on/off states (R#12/13
  countdown re-derived per §2.4's formula, exercised across several `on_vsync()` calls); GRAPHIC2/3
  quarter-addressed pattern/color table; MULTICOLOR 4-line block granularity, 256-wide canvas
  (A-M21-9, NOT 64-wide); TEXT1Q selection renders flat blank, not TMS content; two-run
  determinism.
- **Gates:** build + ctest green (full suite).

### M21-S3 — Non-planar bitmap modes (GRAPHIC4/GRAPHIC5)
- **Goal:** GRAPHIC4 (SCREEN5, 4bpp packed, 256-wide) and GRAPHIC5 (SCREEN6, 2bpp packed,
  even/odd-pixel palette-half split, 512-wide) — no planar interleave involved (base `0x0C`/`0x10`,
  both fail `vdp_base_is_planar`).
- **Files:** `vdp_frame_renderer.{h,cpp}` (extend).
- **Unit tests**: GRAPHIC4 nibble unpack (high nibble = even pixel); GRAPHIC5 2-bit unpack with the
  even/odd palette-half addressing (`palette16[0+...]` vs `[16+...]`-style split, independently
  re-derived from `BitmapConverter.cc:145-157`); two-run determinism.
- **Gates:** build + ctest green (full suite).

### M21-S4 — Planar bitmap modes + D7 (CPU-port AND display-path pieces)
- **Goal:** the `vdp_base_is_planar` helper (`vdp_mode.h`); the CPU-port planar-transform edit to
  `V9958Vdp::effective_address()` (D7 piece 1, §2.5); the renderer's `planar_row_spans` helper (D7
  piece 2, §2.5); GRAPHIC6 (SCREEN7) and GRAPHIC7 (SCREEN8, incl. the GGGRRRBB fixed-color decode,
  A-M21-4) built on top of both.
- **Files:** `vdp_mode.h` (extend); `v9958_vdp.{h,cpp}` (the one D7 CPU-port edit, A-M21-13);
  `vdp_frame_renderer.{h,cpp}` (extend).
- **Unit tests**: CPU-port planar transform — write at logical odd/even addresses, assert raw
  physical bank placement via `vram().read(...)` directly (A-M21-10's verification action);
  `advance_vram_pointer()`/R#14-carry unit tests re-run UNMODIFIED and green (A-M21-12);
  `planar_row_spans` cross-checked against a byte-by-byte rotate reference for a full synthetic
  256-byte row (A-M21-11); GRAPHIC6 4bpp planar decode; GRAPHIC7 fixed-256-color decode with the
  EXACT byte-order assertion (green=bits7-5, red=bits4-2, blue=bits1-0, A-M21-4); an END-TO-END
  cross-path test: write distinguishing bytes via the CPU port at chosen logical addresses while in
  G6 or G7 mode, then render via `VdpFrameRenderer` and confirm the EXPECTED (not mirrored/garbled)
  pixel content — the strong round-trip signal identified in §1.4; two-run determinism.
- **Gates:** build + ctest green (full suite).

### M21-S5 — YJK / YJK+YAE color decode (D5)
- **Goal:** SCREEN12 (YJK) and SCREEN10/11 (YJK+YAE) decode, layered on S4's planar plumbing (YJK
  overlays GRAPHIC7's base, `vdp_mode.h`'s existing `ScreenYjk`/`ScreenYjkYae` identities, already
  decoded by M14).
- **Files:** `vdp_frame_renderer.{h,cpp}` (extend).
- **Unit tests**: J/K unpack from the 4-pixel byte group (A-M21-5's exact bit formula); Y unpack
  (5-bit YJK / 4-bit-even-only YAE); the R/G/B clamp formula INCLUDING a case with a negative,
  non-multiple-of-4 pre-clamp numerator (the rounding risk, A-M21-5, asserting plain-truncation
  behavior); YAE's attribute-bit (0x08) palette-color branch; two-run determinism.
- **Gates:** build + ctest green (full suite).

### M21-S6 — Effects layer (D6): scroll, remaining blink coverage, interlace/EO
- **Goal:** vertical scroll (R#23) applied to character-mode line addressing; horizontal scroll for
  character modes (R#26, applied per-mode as in the reference, A-M21-8) and for bitmap modes (R#26/
  R#27, independently grounded at implementation time per A-M21-8's verification action); border
  mask (R#25 bit1); multi-page scroll (R#25 bit0 + R#2 bit5); interlace/EO bitmap-mode page
  addressing (hedged per A-M21-7, parity-with-openMSX bar); superimpose/digitize explicitly
  documented as N/A (§1.2, no production code needed beyond a comment pointing at the fact-sheet
  citation).
- **Files:** `vdp_frame_renderer.{h,cpp}` (extend).
- **Unit tests**: vertical/horizontal scroll shifts content by the expected offset for at least one
  character mode and one bitmap mode; border-mask left-edge behavior; multi-page-scroll wraps to
  the lower even page as documented; `Field::Even`/`Field::Odd` select the openMSX-parity-hedged
  page-alternation address for a representative bitmap-mode case; two-run determinism.
- **Gates:** build + ctest green (full suite).

### M21-S7 — Machine wiring + system integration + A/B evidence + backlog finalization
- **Goal:** `Hbf1xvMachine::render_frame(Field)` accessor (additive only, §2.1); a dedicated SYSTEM
  INTEGRATION test exercising a real CPU program (via the M11 bus) that writes VRAM/registers
  through `OUT (#98)/(#99)/(#9A)` and then calls the machine's render accessor, asserting against a
  hand-computed golden for at least one representative case per mode FAMILY (character, non-planar
  bitmap, planar bitmap, YJK); the A/B evidence capture (§2.7); full re-verification of the ENTIRE
  M1-M20 suite (95 tests, not just VDP-adjacent) by BOTH the developer AND (independently) QA, per
  the human's explicit "deliberate cross-check" directive; backlog finalization (§4).
- **Files:** `src/machine/hbf1xv_machine.{h,cpp}` (edit, additive only);
  `tests/integration/machine/hbf1xv_m21_vdp_render_integration_test.cpp` (new);
  `tests/system/hbf1xv_m21_vdp_render_system_test.cpp` (new, per `tests/CLAUDE.md`'s system-test
  tier for "machine-level boot/workflow" scenarios); `tools/gen-m21-vdp-render-probe.py` (new);
  `tools/openmsx-m21-vdp-render-parity.ps1` (new); `docs/m21-parity-trace-diff.md`; refreshed
  `docs/asset-checksums.txt`.
- **Gates:** all evidence gates (validate-assets, checksum, Debug build, ctest — FULL suite) **plus**
  the A/B gate. No parity claim without a genuine capture; the screenshot-diff sub-claim explicitly
  marked as a deliberate non-goal (§2.7), not fabricated or silently dropped.
- **Backlog effect:** this is the slice that finalizes D1/D5/D6 → DONE and D7 → IN-PROGRESS (M21
  partial, command-engine piece carried to M22) in the ledger.

---

## 4. Full Deferred-Backlog Review (mandatory — every row across all six registry sections, per DEC-0005)

Source of truth: `agent-protocol/state/deferred-backlog.md`, read in FULL this cycle (all 34 rows
across Sections A-F). Every row re-affirmed below with a one-line justification.

### Section A (human-directive-tracked rows)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| B1 | PSG/YM2149 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, no M21 impact (audio device, unrelated to VDP rendering). |
| B2 | RTC/RP5C01 internals | DONE (M15) — unchanged | Closed at M15; re-affirmed, unrelated to M21. |
| B3 | FM-PAC (OPLL YM2413) device internals | DONE (M17) — unchanged | Closed at M17; re-affirmed, unrelated to M21. |
| B4 | MSX-JE 16 KB SRAM | DONE (M20) — unchanged | Closed at M20; re-affirmed, unrelated to M21. |
| B5 | Kanji-font I/O `#D8-DB` | DONE (M18) — unchanged | Closed at M18; re-affirmed, unrelated to M21. |
| B6 | Halnote/MSX-JE firmware mapping | DONE (M20) — unchanged | Closed at M20; re-affirmed, unrelated to M21. |
| B7 | Cartridge loading | DONE (M19) — unchanged | Closed at M19; re-affirmed, unrelated to M21. |
| B8 | FDC drive mechanics | DONE (M16) — unchanged | Closed at M16; re-affirmed, unrelated to M21. |
| B9 | VRAM/V9958 VDP (register/status/interrupt contract) | DONE (M14) — unchanged | Closed at M14; M21 EXTENDS the same device family with rendering depth but does not reopen B9's own contract scope; the one M14-file edit (D7 CPU-port piece) is additive, not a re-opening of B9's closed acceptance criteria. |

### Section B (other tracked deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| C1 | Exact cycle/T-state timing parity | OPEN — unchanged | M21 introduces no new CPU-visible timing behavior (§2.6); the renderer is a pure pull-model snapshot function, unrelated to C1. Candidate owner remains M23. |
| C2 | Z80 HALT-R increment | OPEN — unchanged | Per DEC-0004; unrelated to M21. |
| C3 | ZEXDOC/ZEXALL full sweep | OPEN — unchanged | `references/zexall/` remains present, unactioned; unrelated to M21's own scope. |
| C4 | S1985 backup-RAM `.sram` persistence | DONE (M15) — unchanged | Closed at M15; re-affirmed, unrelated to M21. |
| C5 | Full boot past first device read | IN-PROGRESS (carried from M16) — unchanged | M21 adds no new boot-path interaction (the renderer is a pull-model, on-demand consumer, not a wired bus device that could gate boot progress); the M16 residual (auto-disk-boot trigger investigation) remains untouched. |
| C6 | Keyboard matrix + joystick | DONE (M15) — unchanged | Closed at M15; re-affirmed, unrelated to M21. |
| C7 | Printer + cassette | DONE (M18) — unchanged | Closed at M18; re-affirmed, unrelated to M21. |
| C8 | Sony speed-controller + hardware PAUSE + Ren-Sha Turbo | OPEN — unchanged | HB-F1XV-specific companion-chip behavior; unrelated to M21. |
| C9 | SDL3 frontend | OPEN — unchanged | Presentation layer; M21 deliberately builds a rendering contract that does NOT depend on C9 existing (§1.1/§2.2), by design — still unbuilt itself, unchanged disposition. |
| C10 | FDC flux-level/DMK fidelity | OPEN — unchanged | Unrelated to M21 (FDC, not VDP). |
| D1 | Pixel-accurate raster rendering pipeline | **CLOSES this cycle (M21)** | Full per-mode VRAM→framebuffer decode delivered (§2.2-§2.4, S1-S3/S6), grounded byte-exact per `CharacterConverter.{hh,cc}`/`BitmapConverter.{hh,cc}`, including border color and blink; deterministic, SDL3-independent, unit/integration-tested. |
| D2 | Sprite rendering + collision | OPEN — unchanged | Explicitly named out of M21 (§1.2); candidate owner M22, paired with D3. |
| D3 | VDP command engine | OPEN — unchanged | Explicitly named out of M21 (§1.2); candidate owner M22. |
| D4 | Cycle-accurate VDP access-slot/command timing | OPEN — unchanged | Explicitly named out of M21 (§1.2, the "hard architectural problem" resolution is a frozen-snapshot pull model, deliberately not this); candidate owner M23. |
| D5 | YJK/YJK+YAE color decode + 15-bit DAC | **CLOSES this cycle (M21)** | Full decode delivered byte-exact per `BitmapConverter.cc:217-285` (§2.3, S5), including the independently-verified rounding risk (A-M21-5) and the 3-bit→5-bit palette expansion table (A-M21-3) shared with every other mode's color output. |
| D6 | Horizontal scroll/interlace/blink/superimpose | **CLOSES this cycle (M21), with one explicit, grounded scope note** | Scroll (R#25/26/27), vertical scroll (R#23), blink (R#12/13) fully delivered (§2.4, S6); interlace/EO bitmap-page addressing delivered to a documented, openMSX-parity confidence bar (A-M21-7, since even the reference project flags its own uncertainty here); superimpose/digitize/external-video correctly modeled as N/A for this specific machine (no digitizer hardware, fact-sheet §9) — a grounded closure, not a gap (§1.2). |
| D7 | G6/G7 VRAM address interleave in the display/command path | **ADVANCES to IN-PROGRESS (M21 partial)** — was OPEN | The CPU-port piece (`executeCpuVramAccess`-equivalent, §2.5/S4) and the display-path piece (renderer row-pointer resolution, §2.5/S4) both close this cycle, independently re-derived and cross-verified (A-M21-10/A-M21-11); the command-engine-path piece (VDPCmdEngine's own coordinate-to-address resolution) is explicitly NOT built this cycle (§1.4) and is carried forward, to close together with M22's D3 work — mirrors the C5 precedent of a single row tracked IN-PROGRESS across milestones rather than force-closed or split into a new letter-row. |

### Section C (M14 VDP-depth deferrals) — covered as D1-D7 above; no separate additional rows.

### Section D (M17 YM2413 DSP/timing deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| E1 | YM2413 FM-synthesis DSP/audio depth | OPEN — unchanged | Unrelated to M21 (audio device). |
| E2 | YM2413 register-write timing constraint | OPEN — unchanged | Unrelated to M21. |

### Section E (M18 printer/cassette depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| F1 | Cassette tape image-format/signal fidelity | OPEN — unchanged | Unrelated to M21. |
| F2 | Printer image/ESC-sequence rendering depth | OPEN — unchanged | Unrelated to M21 (a different device's own rendering depth, not the VDP). |

### Section F (M19 cartridge-mapper-depth deferrals)

| # | Item | Disposition this cycle | Justification |
|---|---|---|---|
| G1 | KonamiSCC mapper + embedded SCC/SCC+ sound chip | OPEN — unchanged | Unrelated to M21 (cartridge/audio). |
| G2 | ROM-database/heuristic mapper-type auto-detection | OPEN — unchanged | Unrelated to M21. |
| G3 | Full `CartridgeSlotManager`-style runtime hot-plug | OPEN — unchanged | Unrelated to M21. |
| G4 | The long tail of ~80 other specialized/vendor mapper types | OPEN — unchanged | Unrelated to M21. |

**Backlog bookkeeping note (executed at ledger-update time by the coordinator, per the exact
M14/M17/M18/M19/M20 precedent):** on M21 closure, mark D1/D5/D6 `DONE (M21)` and D7
`IN-PROGRESS (M21 partial)` in the same cycle. No brand-new backlog row is proposed for D7's
command-engine-path remainder — it is the already-named other half of D7 itself (§1.4), carried
forward to close together with M22's D3 work, not newly-discovered depth requiring a fresh letter.

---

## 5. Acceptance Criteria (M21 exit)

1. `FrameBuffer`/`Field`/`VdpFrameRenderer` (§2.1-§2.2) implemented: deterministic, in-memory,
   RGB555, exercisable purely through the existing M14 port contract, assertable byte-for-byte with
   no PNG/SDL3 dependency. *(S1)*
2. Every Target-Spec text/graphics mode (TEXT1, TEXT2, GRAPHIC1-7, MULTICOLOR; TEXT1Q/MULTIQ/Unknown
   correctly rendered as flat blank per A-M21-6) renders byte-exact per the cited grounding, with
   independently-verified dimensions (§2.2's table, not the fact-sheet's cell-grid figures naively
   transcribed, A-M21-9). *(S1-S3, S6)*
3. YJK (SCREEN12) and YJK+YAE (SCREEN10/11) decode implemented byte-exact per §2.3, including the
   independently-verified rounding behavior (A-M21-5) and the shared 3-bit→5-bit palette expansion
   table (A-M21-3), with the GRAPHIC7 GGGRRRBB byte-order boundary explicitly tested (A-M21-4).
   *(S4-S5)*
4. Horizontal/vertical scroll, blink timing, border-mask, multi-page-scroll, and interlace/EO
   bitmap-page addressing (hedged to an openMSX-parity bar per A-M21-7) implemented per §2.4;
   superimpose/digitize explicitly, correctly scoped N/A for this machine (§1.2), not silently
   dropped. *(S2, S6)*
5. D7's CPU-port piece (edit to `V9958Vdp::effective_address()`) and display-path piece (renderer's
   `planar_row_spans`) both implemented byte-exact per §2.5/A-M21-10/A-M21-11, with
   `advance_vram_pointer()` confirmed UNCHANGED (A-M21-12) and a genuine end-to-end cross-path
   (CPU-port write → renderer read) test proving the two sides compose correctly (§1.4, S4). The
   command-engine-path piece of D7 is explicitly NOT claimed closed (§1.4/§4).
6. **D1, D5, D6 close in full this cycle; D7 advances to IN-PROGRESS (M21 partial)** with the
   command-engine-path remainder explicitly carried to M22 — no force-closure (§4).
7. Deterministic unit + integration + a dedicated SYSTEM integration test (per `tests/CLAUDE.md`'s
   three-tier convention) cover every new behavior in §2.2-§2.5; two identical runs produce
   byte-identical `FrameBuffer` content for identical inputs. *(S1-S7)*
8. **Zero regression across the FULL M1-M20 suite (95 tests)** — not merely VDP-adjacent tests —
   independently re-run and confirmed by BOTH the developer AND (separately) QA, per the human's
   explicit "deliberate cross-check across the entire system" directive. In particular: the M9/M12
   CPU-timing oracles remain untouched (no new clock consumer, §2.6); the existing M14 VRAM-pointer/
   R#14-carry unit tests remain green UNMODIFIED (A-M21-12); every M15-M20 device/slot-map/
   accessor golden remains green. *(S7)*
9. Real openMSX A/B evidence captured via the derived-value comparison technique (§2.7) for: 16-color
   palette expansion, GRAPHIC7 fixed-color decode, YJK/YJK+YAE decode (incl. the rounding boundary),
   and the D7 CPU-port planar transform. The literal screenshot-pixel-diff technique is explicitly,
   honestly marked as a deliberate non-goal (§2.7), not fabricated or silently omitted; any
   individual sub-claim that turns out to have no feasible headless equivalent (e.g., no Tcl-
   reachable computed-color introspection point) is reported BLOCKED, not invented. *(S7)*
10. Every deferred-backlog row (all 34, across all six registry sections) re-affirmed with
    justification (§4); D1/D5/D6 → DONE, D7 → IN-PROGRESS (M21 partial), carried to M22.
11. Evidence gates executed and captured each cycle (validate-assets, checksum, Debug build, ctest —
    full suite).
12. QA sign-off recorded before closure (`docs/m21-qa-signoff.md`), including QA's OWN independent
    re-run of the full M1-M20 regression suite (not a rubber-stamp of the developer's own claim),
    per the human's explicit cross-check directive. Per `agent-protocol/state/current-phase.md`, the
    coordinator is pre-authorized to proceed through the release-decision/tag step for this specific
    M21-M23 run on a clean QA PASS, without an additional pause request — but a real blocker (QA
    anything less than PASS) stops the run for human consultation.

---

## 6. Risks (each with a verification action)

| ID | Risk | Verification action |
|----|------|---------------------|
| R-M21-1 | GRAPHIC7's fixed 256-color byte layout is implemented as the naively-expected `RRRGGGBB` instead of the actual `GGGRRRBB` (A-M21-4). | Dedicated unit test asserts `byte=0b111_000_00` (green=7 in the TOP 3 bits) yields max-green output, not max-red. |
| R-M21-2 | The YJK B-channel formula is "cleaned up" with `std::floor()`/floating point instead of plain C++ truncating `int` division, silently diverging for negative numerators (A-M21-5). | Unit test picks Y/J/K values producing a negative, non-multiple-of-4 pre-clamp numerator and asserts the truncating-division result. |
| R-M21-3 | The G6/G7 planar transform is implemented as a naive nibble-swap/byte-interleave guess instead of the actual 17-bit rotate-right-by-1 (A-M21-10). | Unit test writes at logical odd/even addresses and asserts the exact physical bank/offset via the raw `vram().read(...)` accessor, independent of any round-trip read-back (which would pass either way per §1.4's own analysis). |
| R-M21-4 | The D7 CPU-port edit accidentally also changes `advance_vram_pointer()`'s R#14-carry logic, regressing the already-QA-verified M14 pointer-carry behavior (A-M21-12). | The EXISTING M14 pointer-carry unit tests (`video_v9958_ports_unit_test.cpp`) are re-run UNMODIFIED and confirmed green after the D7 edit; the implementation report confirms the edit is confined to `effective_address()` only. |
| R-M21-5 | MULTICOLOR is rendered as a literal 64×48-pixel image (naive fact-sheet-table transcription) instead of the correct 256×192 host-pixel canvas with 4×4 color-cell granularity (A-M21-9). | Unit test asserts a 256-wide row buffer with the documented 2-colors-per-4-pixel-group block structure. |
| R-M21-6 | TEXT1Q/MULTIQ/Unknown modes render fabricated TMS9918-compatible content instead of the correct V9958 flat-blank fill (A-M21-6), since HB-F1XV is never `isMSX1VDP()`. | Unit test sets the TEXT1Q/MULTIQ mode-bit pattern and asserts a flat single-color fill (palette entry 15), not striped or character-cell content. |
| R-M21-7 | The interlace/EO bitmap-page-addressing feature is claimed with unwarranted confidence beyond what openMSX's own authors document as verified (their own "TODO: verify" comments, A-M21-7), risking a fabricated ground-truth claim. | Implementation report and A/B evidence explicitly frame this sub-feature's acceptance bar as "parity with openMSX's own modeled behavior," not independently-proven-correct; risk disclosed verbatim in the QA sign-off. |
| R-M21-8 | Bitmap-mode horizontal scroll is implemented by copying the character-mode mechanism (which lives inside the per-mode converter itself) instead of independently grounding the actual rasterizer-level windowing formula bitmap modes use (A-M21-8). | S6 implementation report cites the specific `SDLRasterizer.cc` (or equivalent) line range independently read and re-derived for the bitmap-mode case, distinct from the character-mode citation. |
| R-M21-9 | Superimpose/digitize is silently dropped/forgotten rather than explicitly, honestly scoped as N/A for this specific machine (§1.2), leaving an undocumented gap against D6's original wording. | §1.2/§4 explicitly name and cite the fact-sheet §9 grounding for this scope reduction; QA sign-off confirms this citation is present and accurate, not a silent omission. |
| R-M21-10 | The FULL M1-M20 regression suite (95 tests) is not actually independently re-run by both the developer AND QA — only the "obviously VDP-related" subset is checked, missing the human's explicit "deliberate cross-check across the entire system" requirement. | Both the developer's implementation report and the QA sign-off MUST record a fresh, full `ctest` run (all 95+ prior tests plus new M21 tests) with an explicit pass count, not a claim of "no VDP-adjacent test failed." |
| R-M21-11 | The A/B evidence plan degrades into (or is misrepresented as) a screenshot-pixel-diff claim without confirming openMSX-under-WSL can actually produce a fair, neutral-color-matrix headless capture — fabricating parity evidence for a technique never actually verified feasible. | §2.7 explicitly marks the screenshot-diff technique as a deliberate NON-GOAL, not attempted; the actual A/B evidence document (`docs/m21-parity-trace-diff.md`) must show the derived-value comparison technique only, with any genuinely-blocked introspection sub-claim marked BLOCKED, never silently upgraded to PASS. |
| R-M21-12 | The D7 CPU-port edit to the already-shipped, QA-signed M14 `v9958_vdp.cpp` is treated as routine/uncommented, understating that it slightly broadens D7's originally-backlog-worded "display path" framing to explicitly include the CPU port too (A-M21-13). | Implementation report names this file/edit explicitly, cross-references A-M21-13/§1.4's reasoning, and the diff is confirmed minimal (single function, additive) by both the developer and QA. |
| R-M21-13 | `CartridgeRomWindow`-class reuse patterns from prior milestones create pressure to over-engineer `VdpFrameRenderer` into a shared/generic "renderer" base class prematurely, before a second consumer (sprites, M22) exists to justify the abstraction. | §2.1 explicitly leaves internal class-organization latitude to the developer but fixes ONLY the public `FrameBuffer`/`Field`/`VdpFrameRenderer` facade — any speculative generalization beyond mode-dispatch is flagged as unnecessary scope for this milestone (guardrails "Scope Control: smallest meaningful step"). |

---

## 7. Developer Handoff

- **Start at M21-S1** (rendering infrastructure + TEXT1 + GRAPHIC1) — grounded per §2.1-§2.2;
  cite `references/openmsx-21.0/src/video/{CharacterConverter,BitmapConverter,DisplayMode}.{hh,cc}`
  line ranges in code comments (never copy the code itself — GPL isolation).
- **Sequence S1→S7 in order**; each runs the full evidence-gate set INCLUDING the full M1-M20
  regression (95 tests), not a subset; keep `ctest` green every cycle.
- **`src/` placement fixed by §2.1**: new files stay in the EXISTING `src/devices/video/` family
  (no new top-level device family, unlike M18/M19/M20 — this milestone extends M14's own device
  family with rendering depth). Machine wiring only in `src/machine/hbf1xv_machine.{h,cpp}`
  (additive: one member, one accessor).
- **Critical architectural findings to honor:**
  - The G7 byte layout is `GGGRRRBB`, not `RRRGGGBB` (A-M21-4/R-M21-1) — green is bits 7-5.
  - The YJK B-formula MUST use plain C++ `int` division (truncation toward zero), never
    `std::floor()` (A-M21-5/R-M21-2) — this is what makes it byte-exact to openMSX automatically,
    since both are C++ with the same integer semantics.
  - The G6/G7 planar transform is a 17-bit rotate-right-by-1: `physical = (logical >> 1) |
    ((logical & 1) << 16)` (A-M21-10/R-M21-3) — do not guess a nibble-swap or naive interleave.
  - `advance_vram_pointer()`/R#14-carry logic is UNCHANGED by the D7 edit — only
    `effective_address()`'s STORAGE-address computation changes (A-M21-12/R-M21-4). Re-run the
    existing M14 pointer-carry tests unmodified to confirm.
  - MULTICOLOR's real canvas is 256×192 (not literally 64×48) — the fact-sheet's "64×48" is a
    color-cell grid, independently re-derived from the actual `CharacterConverter.cc` draw-call
    structure (A-M21-9/R-M21-5).
  - TEXT1Q/MULTIQ/Unknown render FLAT BLANK (palette entry 15), never TMS9918-compatible content —
    HB-F1XV's V9958 is never `isMSX1VDP()` (A-M21-6/R-M21-6).
  - Superimpose/digitize is explicitly, correctly OUT (no digitizer hardware on this machine,
    fact-sheet §9) — do not silently skip it without the citation; do not attempt to fabricate a
    compositing path either.
  - Interlace/EO bitmap-page addressing inherits openMSX's OWN disclosed uncertainty (its authors'
    "TODO: verify" comments) — build it to parity with openMSX's modeled formula, but do not
    over-claim independent ground-truth confidence beyond that (A-M21-7/R-M21-7).
- **D7's scope boundary (§1.4) is final for this milestone**: CPU-port piece + display-path piece
  close in M21; the command-engine-path piece is explicitly NOT built here — do not attempt to
  pre-build VDPCmdEngine-equivalent address resolution ahead of M22; that would be scope creep
  without D3's actual command registers/handshake existing yet.
- **No new clock consumer (§2.6):** the renderer is a pure pull-model function; the only new stateful
  piece (blink countdown) hooks the EXISTING `on_vsync()` call, not a new time source.
- **Full regression discipline (the human's explicit standing instruction for this M21-M23 run):**
  every slice's evidence gate must include a FRESH, FULL `ctest` run (all M1-M20 tests plus new
  M21 tests) — not an assumption that "untouched files can't have regressed." QA must independently
  reproduce this, not rubber-stamp the developer's count.
- **A/B (§2.7):** build `tools/gen-m21-vdp-render-probe.py` + `tools/openmsx-m21-vdp-render-parity.ps1`
  using the derived-value comparison technique (raw VRAM/register inputs → computed color values on
  both sides), NOT a screenshot-pixel diff (explicitly out of technique scope, §2.7); verify before
  claiming whether openMSX's Tcl debugger exposes any computed-color introspection point beyond raw
  VRAM/register reads — report BLOCKED honestly for any sub-claim that turns out infeasible.
  Include the negative-numerator YJK rounding boundary as one of the covered subjects (A-M21-5).
  Include the D7 CPU-port planar transform as one of the covered subjects (A-M21-10).
  Include the GRAPHIC7 GGGRRRBB byte-order boundary as one of the covered subjects (A-M21-4).
- **Ledger discipline (DEC-0005):** on closure, mark D1/D5/D6 `DONE (M21)` and D7
  `IN-PROGRESS (M21 partial)` in `agent-protocol/state/deferred-backlog.md`; do NOT invent a new
  letter-row for D7's remainder (§1.4/§4); update `state/current-phase.md` and `state/milestones.md`.
- **Gate:** per `agent-protocol/state/current-phase.md`, the coordinator is pre-authorized to
  proceed through the release-decision/tag step for this specific M21-M23 run on a clean QA PASS —
  but STOP and consult the human if QA does not reach a clean PASS.
- Deliverables: source per §2.1; tests per §3 (unit + integration + a dedicated system test);
  `docs/m21-parity-trace-diff.md`; refreshed `docs/asset-checksums.txt`; an implementation report
  `docs/m21-implementation-report.md`; then hand to QA for `docs/m21-qa-signoff.md`.
