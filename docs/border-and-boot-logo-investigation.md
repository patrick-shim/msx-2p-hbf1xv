# Border-Box + Boot-Logo Investigation — Two Human-Reported Presentation Defects

- Date: 2026-07-08
- Trigger: human reports from live interactive SDL3 play (post-DEC-0029): (A) the SDL3 window
  shows the active display area stretched edge-to-edge with no border box (a real MSX shows the
  MSX-blue BASIC area surrounded by a sky-blue border); (B) the MSX2+ animated boot logo never
  appears — boot goes straight to BASIC.
- Status: **both issues root-caused and fixed** — issue B turned out to be THREE stacked defects
  (each masking the next), plus a fourth latent renderer crash exposed along the way. All fixes
  are universal hardware/presentation models (no program-specific keying), each with new
  deterministic regression tests. Changes left uncommitted for coordinator/QA review.
- Precedent: coordinator-authorized targeted bug investigation (DEC-0026/DEC-0028/DEC-0029
  discipline). Second cross-reference `references/fmsx-60/` (DEC-0030) consulted where noted.

## 1. Ground truth (openMSX 19.1, WSL, Sony_HB-F1XV, natural boot)

Captured via scripted `screenshot -raw` + `debug read "VDP regs"` at emulated-time intervals
(technique from docs/sprite-invisibility-investigation.md):

| t (s) | State | Key registers |
| --- | --- | --- |
| 0.25–1.0 | black, VDP all zero | `R0=00 R1=00` |
| 1.25–2.25 | logo mode set, display off | `R0=08` (GRAPHIC5/SCREEN 6) `R1=23` |
| 2.5–3.0 | **logo animation** (letters sliding) | `R1=63 R2=3F R7=05 R25=03` |
| 3.25–4.0 | **logo static: "MSX" + "Main RAM:64Kbytes"** | `R25=00` |
| 4.5+ | BASIC screen (SCREEN 1) | `R0=00 R2=06 R5=36 **R7=07**` |

- Reference PNGs: `debug/frames/boot-logo-openmsx-reference-t2500ms.png` (mid-slide),
  `boot-logo-openmsx-reference-t3500ms.png` (full logo + "Main RAM:64Kbytes"),
  `border-openmsx-reference-basic-t15s.png` (BASIC: blue active area inside a sky-blue border).
- Port `#F4` (Tcl `debug read ioports 0xF4`): **0x00 at t=1 s, 0x80 at t=15.5 s** — the BIOS
  consumes a cold-boot flag and then sets bit 7.
- RTC CMOS (`debug read "Real time clock SRAM"`): block 2 = `A 0 0 1 D 1 F 4 7 3 0 0 0`
  (valid marker 10; SCREEN 1; WIDTH 29; colors fg=15/bg=4/**border=7 — the sky blue**),
  block 3 all zero. This ruled the RTC-CMOS hypothesis OUT for issue B (see §3.1).
- Border geometry, measured from the 320x240 raw screenshots (pixel-scanned): 256x192 active
  area at x∈[32,287], y∈[24,215] → **32 px left/right, 24 px top/bottom**; SCREEN 0 text
  (240 px wide) leftmost text pixel at **x=41**. Border RGB (82,224,255) = palette 7 sky blue;
  active bg (39,39,255) = palette 4 MSX blue — exactly the human's description.
- Human-calibration note (coordinator relay): the human's expected-screen screenshot (large MSX
  logo + "VIDEO RAM:128Kbytes"/"USER RAM:…" two-line text) is the **generic MSX2P.ROM variant**
  (their fMSX executables set). The Sony HB-F1XV's own BIOS renders the Sony variant: sliding
  letters, then "MSX" + a single line "Main RAM:64Kbytes" (64 KB main RAM, correct per the
  target machine specification). Both openMSX and our fixed boot show the Sony variant.

## 2. Issue A — border/backdrop region never presented

### Reproduction / root cause

`Sdl3VideoPresenter::blit_frame()` uploaded the bare active-area `FrameBuffer` and drew it to
the full window (`SDL_RenderTexture(..., nullptr, nullptr)`) — by construction no border region
existed. The M21 `FrameBuffer` contract deliberately carries only active pixels plus a single
per-frame `border_color` (frame_buffer.h documents border *geometry* as out of scope), and the
renderer's `border_color()` is already mode-aware R#7 (vdp_frame_renderer.cpp:193-213) — the
color was computed every frame and then never shown.

### Fix (universal, presentation-level composition)

New pure, SDL-free module `src/frontend/border_composer.{h,cpp}` (compiled into
`sony_msx_core`, the sdl3_cli.cpp precedent):

- `border_geometry(active_w, active_h)` → canvas 320x240 (640x240 for 480/512-wide modes),
  active area anchored at its raster-true position: x0 = 32 (256-wide), 41 (TEXT1 240), 64
  (512-wide), 82 (TEXT2 480); y0 = 24 (192-line) / 14 (212-line).
- `compose_border_canvas(frame)` → border_color-filled canvas with the active pixels copied at
  the anchor; border color live per frame.
- `Sdl3VideoPresenter::blit_frame()` composes, then uploads the canvas (zero per-pixel
  conversion preserved — composition copies raw RGB555 values).
- `tools/frame-to-png.py --with-border` mirrors the same geometry for headless PNG evidence
  (cross-referenced comments both ways to prevent drift).

Grounding: V9958 fact-sheet §7 (left border [202,258) = 56 cycles, display [258,1282) = 1024,
right [1282,1341) = 59; NTSC LN=0 borders 26/25, LN=1 16/15 — a 4-cycles-per-wide-pixel line);
openMSX `SDLRasterizer.cc:28-76` translateX() ("borders are extended" to the 320/640 canvas,
centered on the visible midpoint) and `:174-179` lineRenderTop; `VDP.hh:598-604` (text mode
starts +9 px, 960-cycle text display region). Every anchor was verified against the measured
openMSX screenshots above (32/24 graphics; 41 text). Placement rationale: composing in the
presenter (not in `FrameBuffer` itself) preserves the documented deterministic core-output
contract, every existing test coordinate, and the frame-dump format; openMSX composes border in
its rasterizer, which *is* its presentation layer — `Sdl3VideoPresenter` is ours. R#18
set-adjust repositioning remains unmodeled (matches existing renderer scope).

fMSX cross-check (references/fmsx-60/source/fMSX/Common.h:59-97 RefreshBorder + Unix.c WIDTH
272/HEIGHT 228): same border-box concept (active area centered in a live per-line R#7-colored
surround), different canvas-extension convention (272x228, ~8/18 px borders). The two
references agree on behavior and differ only in presentation proportions; we adopt openMSX's
because it was measured on this exact machine and scales 4:3 cleanly.

### Evidence

- Before: `debug/frames/border-before-f0300-active-area-only.png` (256x192 active area only —
  what the window used to show stretched edge-to-edge).
- After: `debug/frames/border-after-f0300-basic-skyblue-border.png` (320x240: MSX-blue active
  area inside the sky-blue border box) — visually matches
  `debug/frames/border-openmsx-reference-basic-t15s.png`.

## 3. Issue B — boot logo never appears

Our pre-fix boot (temporary `--boot-logo-diag` in src/main.cpp, since reverted; frame dumps +
VDP register timeline every 15 frames): **no GRAPHIC5 phase at all** — screen-off → SCREEN 1 at
frame ~90 → display on at frame ~135. `debug/frames/boot-logo-before-f0135-no-logo.png`.
Three stacked root causes were found; each fix exposed the next.

### 3.1 Root cause 1 — port #F4 reset-status register missing entirely

- Our machine had no device at I/O port #F4 → reads returned open-bus **0xFF** → bit 7 set →
  the BIOS took the warm-restart path and skipped the whole logo routine. (The RTC-CMOS
  hypothesis was investigated first and ruled out: our BIOS re-initializes CMOS to defaults
  byte-identical to openMSX's persisted `hb-f1xv.cmos` — block 2 `A 0 0 1 D 1 F 4 7 3 0 0 0` on
  both engines — which is also why the BASIC colors were already correct.)
- Grounding: `references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml` declares
  `<ResetStatusRegister>` `<inverted>false</inverted>` `<io base="0xF4" num="1"/>`;
  `references/openmsx-21.0/src/MSXResetStatusRegister.cc:13-35` (power-up 0x00; write keeps
  `(status & 0x20) | (value & 0xA0)`). Empirical: openMSX #F4 reads 0x00 at boot, 0x80 after;
  the local `bios/f1xvbios.rom` contains the `IN A,(0F4h)`/`OUT (0F4h),A` primitives at offsets
  0x146A/0x146F. fMSX (MSX.c): does not model #F4 at all — silent, no disagreement to resolve.
- Fix: new `src/devices/chipset/reset_status_register.{h,cpp}` attached at #F4 in
  `Hbf1xvMachine::wire_bus()`, cleared in `cold_boot()` (`power_on_reset()`; deliberately not
  named `reset()` — real hardware preserves the latch across warm resets, and this machine
  model only exposes cold power-up).

### 3.2 Root cause 2 — S#0 sprite-collision flag was frame-granular, logo wobble paces per line

With #F4 fixed, the logo started but never finished: the sliding/wobble phase looped forever
(R#26/R#27 oscillating at frame 900+). CPU trace (temporary diagnostic): the SUB-ROM effect
loop (`bios/f1xvext.rom`, executing at 0x7A5D-0x7A74 with page 0 switched to the SUB-ROM) sets
R#17=26, **polls S#0 bit 5 (collision flag C, R#15=0 verified) until it sets, writes one
R#26/R#27 scroll pair, DJNZ ×84**. The logo's overlapping letter sprites collide on many
scanlines; real hardware checks sprites progressively as the raster scans, so C re-latches with
LINE granularity after each S#0 read-clear. Our engine computed collisions once per frame
(`SpriteEngine::recompute_frame` at vsync) — after the first read cleared C it stayed clear all
frame, so the 84-step loop advanced ~one step per frame (~2 orders of magnitude too slow).

- Fix: `recompute_frame()` now collects the frame's per-line collision EVENTS (raster order);
  the V9958 S#0 read path re-latches C from the next unconsumed event the raster has already
  scanned (`sync_collision_to_raster()`), and consumes past events at read-clear
  (`consume_collision_events_up_to()`). Raster position comes from the same pull-style
  `VdpClockSource` the S#2 VR/HR fix uses (new `V9958Vdp::raster_display_line()`); with no
  clock attached both hooks are INT_MIN no-ops (clockless tests keep pre-fix behavior). The
  vsync-time first-event latch is preserved (existing test contract).
- Grounding: `references/openmsx-21.0/src/video/SpriteChecker.hh:70-100` (progressive
  sync-to-EmuTime checking) + SpriteChecker.cc collision latching (+12/+8 offsets unchanged).
- **Reference disagreement (recorded per DEC-0030 discipline):** fMSX checks collisions ONCE
  PER FRAME at ScanLine==192 (`references/fmsx-60/source/fMSX/MSX.c:2178-2184`) — the exact
  pre-fix model. Resolution: the real Sony BIOS's own polling protocol (84 poll-exits expected
  within a fraction of a second) is only satisfiable with sub-frame re-latching, and openMSX
  demonstrably runs the real logo on schedule while the frame-granular model demonstrably does
  not. Line-granular (openMSX) is the hardware-corroborated interpretation; fMSX simply skips
  this fidelity level (it does not model #F4 either, so its generic-ROM logo path differs).

### 3.3 Root cause 3 — command engine missing the pre-armed COL transfer unit

With the wobble fixed, the static-logo phase hung: SUB-ROM loop at 0x2BE4/0x2BEF polls S#2 bit 0
(CE) forever, with `CMD=0xB0` (LMMC 16x8) stuck pending. Trace of the feed: the BIOS pre-loads
the FIRST pixel color into R#44 (via #99 two-write), issues LMMC, then feeds only **127** more
data writes through port #9B (R#17=0xAC, auto-increment disabled) — 128 pixels total counting
the pre-load. Our engine only stepped the transfer on post-CMD R#44 writes, so it waited for a
128th write that never comes → CE never cleared → boot never proceeded.

- Grounding: `references/openmsx-21.0/src/video/VDPCmdEngine.cc:1856-1863` (`setCmdReg` case
  0x0C: ANY COL write sets `transfer = true`, even before a command) and `:1294-1307` +
  `:1726-1735` (startLmmc/startHmmc do NOT arm it themselves — their bug#1014 "Baltak Rampage"
  comment — but a pre-armed unit is consumed as the first transferred unit).
  **fMSX independently corroborates** (`references/fmsx-60/source/fMSX/V9938.c:852-857`
  VDPWrite clears TR = "byte pending"; `:624-651` LmmcEngine consumes the current COL when TR
  is clear; `:1024` command start immediately runs the engine → pre-loaded COL becomes pixel 0).
  Minor reference disagreement, recorded: fMSX would consume a "first unit" at start whenever
  TR happens to be clear even with no COL ever written; openMSX arms strictly on COL writes
  (bug#1014 was exactly a fix for the always-consume behavior, validated against a real game).
  We follow openMSX's arm-on-COL-write-only model.
- Fix: `VdpCommandEngine::transfer_pending_` — armed by every R#44 write, consumed by
  `perform_transfer_step()`, and start_lmmc()/start_hmmc() consume a pre-armed unit as the
  first pixel/byte. `reset()` clears it (deterministic power-on).
- Test-contract correction: `tests/system/hbf1xv_m22_sprites_command_engine_system_test.cpp`'s
  LMMC program previously left the HMMV fill color (0x5A) armed and relied on "start never
  consumes" — per BOTH references real hardware would make that stale byte LMMC pixel 0. The
  program now uses the authentic pre-load protocol (COL before CMD), keeping its original
  expected VRAM bytes while encoding the reference-correct semantics.

### 3.4 Latent defect exposed — GRAPHIC5/6 sprite compositor out-of-bounds write

First post-#F4 boot aborted (debug CRT "span subscript out of range") at the logo phase:
`VdpFrameRenderer::composite_sprites()` bounded its mode-2 pixel loop by the canvas width
(512 in GRAPHIC5/6) while sprite X lives in a 256-wide space and each sprite pixel writes
`out[x*2+0/1]` — a right-edge sprite (the logo's sliding letters) wrote up to `out[573]` on a
512-entry row. Fix: clip at the sprite-space limit (`x_limit = w/2` for G5/G6). Grounding:
`references/openmsx-21.0/src/video/SpriteConverter.hh:134-143` (mode-2 buffer contract "256
pixels" with maxX clipping) and `:186-198` (`pixelPtr[x*2+0/1]` writes).

### Post-fix boot (this emulator, deterministic)

Frames 75–255 = logo (mode/register timeline matches openMSX phase-for-phase, including
`R25=03` wobble ending ~frame 180); SCREEN 1 BASIC from frame ~259 (t≈4.3 s — openMSX: ~4.5 s);
final `#F4 = 0x80` and BASIC registers byte-identical to openMSX (`R2=06 R5=36 R7=07`).
Evidence: `debug/frames/boot-logo-after-f0150-letters-sliding.png`,
`boot-logo-after-f0210-mainram64k.png` (compare `boot-logo-openmsx-reference-t3500ms.png`),
`border-after-f0300-basic-skyblue-border.png`.

## 4. Code changes (all uncommitted, for review)

| File | Change |
| --- | --- |
| `src/devices/chipset/reset_status_register.{h,cpp}` | NEW: #F4 reset-status latch (non-inverted) |
| `src/machine/hbf1xv_machine.{h,cpp}` | wire #F4 device; power_on_reset() in cold_boot() |
| `src/devices/video/sprite_engine.{h,cpp}` | per-line collision events + raster re-latch hooks |
| `src/devices/video/v9958_vdp.{h,cpp}` | `raster_display_line()`; S#0 read path sync/consume |
| `src/devices/video/vdp_command_engine.{h,cpp}` | `transfer_pending_` pre-armed COL unit |
| `src/devices/video/vdp_frame_renderer.cpp` | G5/G6 sprite compositor sprite-space clip |
| `src/frontend/border_composer.{h,cpp}` | NEW: presentation border-canvas composition |
| `src/frontend/sdl3_video_presenter.{h,cpp}` | compose border canvas in blit_frame() |
| `tools/frame-to-png.py` | `--with-border` (same geometry, cross-referenced) |
| `CMakeLists.txt`, `tests/CMakeLists.txt` | new sources + 6 new test registrations |

Temporary `--boot-logo-diag` scaffolding in `src/main.cpp` reverted (`git checkout --`).

## 5. Tests

New (all registered in tests/CMakeLists.txt, fast subset):

1. `devices_chipset_reset_status_register_unit_test` — #F4 semantics (power-on 0x00, 0xA0 write
   mask, bit-5 preservation, power-cycle protocol).
2. `devices_sprite_engine_collision_relatch_unit_test` — engine-level event/latch/consume
   sequences, clockless INT_MIN no-op, and a V9958-level S#0 read sequence under a fake raster
   clock (the exact boot-logo pacer shape; fails pre-fix).
3. `devices_vdp_command_engine_pending_col_unit_test` — pre-armed COL as first unit for
   LMMC/HMMC (the 1+N-1 boot-logo protocol; fails pre-fix), no-preload still requires N writes
   (bug#1014), single consumption, determinism.
4. `devices_vdp_frame_renderer_sprite_edge_clip_unit_test` — G5/G6 right-edge sprite draws
   through canvas x=511 with guard-padding overrun detection (aborts pre-fix in debug).
5. `frontend_border_composer_unit_test` — every geometry anchor exact-value (32/41/64/82 ×
   24/14), pixel-exact composition, live border color, determinism.
6. `machine_hbf1xv_boot_logo_integration_test` — real-BIOS end-to-end: #F4 cold flag, GRAPHIC5
   logo entry (≤ frame 150), exit to GRAPHIC1 within the openMSX window (frames 200–330),
   final #F4 == 0x80, two-run byte-identical determinism. Observed checkpoint: entry=75,
   exit=259, f4_end=0x80. (CORRECTION per `docs/border-boot-logo-qa-review.md` finding 1: this
   report originally recorded exit=270 — QA's four independent binary runs all reproducibly print
   exit=259, deterministic and in-window; 270 was a transcription error, not a real observation.)

Updated: `sdl3_video_presenter_pixel_integration_test` (active area bit-exact at the composed
anchor + border-surround assertions, canvas-sized 1:1 target), `hbf1xv_m22_sprites_command_
engine_system_test` (authentic COL pre-load protocol, §3.3).

## 6. Verification (all captured from real runs)

- `tools/validate-assets.ps1` — pass (required BIOS present, 2 ROMs).
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — regenerated.
- `cmake --build build --config Debug` — exit 0 (headless); `cmake --build build/sdl3-on
  --config Debug` — exit 0.
- Headless fast subset: `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` —
  **152/152 passed** (14.99 s).
- SDL3-gated fast subset (`SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy`, build/sdl3-on) —
  **161/161 passed** (22.55 s).
- ZEXALL/ZEXDOC slow sweep: NOT run, per the coordinator-relayed third-revision cadence rule
  approved by the human mid-cycle (sweep only at production-candidate checkpoints or for direct
  `src/devices/cpu/`/`src/core/` edits — neither was touched here; a sweep started under the
  earlier rule was cancelled when the rule was rescinded). The two fast subsets above are the
  complete regression gate for this change.
- Scope: `git diff v1.0.28 --name-only -- src/devices/ src/peripherals/ src/core/` →
  `src/devices/video/{sprite_engine,v9958_vdp,vdp_command_engine}.{h,cpp}`,
  `src/devices/video/vdp_frame_renderer.cpp` (+ NEW untracked
  `src/devices/chipset/reset_status_register.{h,cpp}`). Nothing under `src/devices/cpu/` or
  `src/core/`.

## 7. Honest residuals

- Without any disk mounted, the BASIC banner text appears only after a long drive-detect phase
  (blank blue SCREEN 1 for > 40 s of emulated time with the synthesized default medium).
  **Pre-existing**, not a regression: the pre-fix tree shows the identical blank screen at the
  same point (`debug/frames/boot-logo-before-f0135-no-logo.png` vs pre-fix frame 900, both
  blank; the M16/M28 investigations document the auto-disk-boot trigger behavior). Real usage
  (real disk via `--disk`, or cartridges) is covered by existing M16/M28 boot-parity evidence.
- The wobble effect's visual scroll positions were not pixel-compared frame-by-frame against
  openMSX (the pacing now matches phase-for-phase and the end state is register-identical);
  only the phase timeline and final states were oracled.
- GRAPHIC5's dual border colors (R#7 bits 3-2/1-0 alternating) remain collapsed to the even
  half — the pre-existing documented FrameBuffer simplification (vdp_frame_renderer.h:56).
