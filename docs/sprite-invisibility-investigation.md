# Sprite Invisibility Investigation — Metal Gear (Sprite Mode 2 R#5/R#11 Mask Semantics)

- Date: 2026-07-08
- Trigger: human-reported bug from live interactive SDL3 play (`sony_msx_sdl3.exe --bios-dir bios
  --cart1 roms\metalgear.rom --cart1-type Konami`): title screen, background tiles, scrolling, UI,
  PSG sound, keys, and game logic all work — but the player character sprite (and in fact every
  sprite) is completely absent. Not garbled: absent.
- Status: **root cause found and fixed** (one bounded change in
  `src/devices/video/sprite_engine.cpp`), with a new regression unit test, updated existing tests,
  before/after PNG evidence, an openMSX A/B check, and a full green regression sweep (147/147,
  including the ZEXALL/ZEXDOC slow sweep).
- Precedent: coordinator-authorized targeted bug investigation (DEC-0026/DEC-0028 discipline).

## 1. Reproduction (headless, deterministic)

A temporary, uncommitted `--mg-sprite-diag` mode was added to `src/main.cpp` (reverted via
`git checkout -- src/main.cpp` at the end, per the DEC-0026/DEC-0028 precedent). It mirrors
`Sdl3App::run_one_frame()`'s exact loop shape (`step_cpu_instruction()` sub-loop to each
`frame_cycles_per_frame()` boundary, then `on_vsync_boundary()` — the loop shape that reproduces
real-app behavior):

1. `Hbf1xvMachine`, `set_asset_root("bios")`, `cold_boot()`, `load_cartridge(1, Konami,
   roms/metalgear.rom)`.
2. 900 frames → title screen ("METAL GEAR / ©Konami 1987 / PUSH SPACE KEY") — rendered perfectly.
3. SPACE injected (`keyboard().set_key(8, 0, true)`, row 8 col 0) for 5 frames, released.
4. 600 more frames → gameplay (first screen, dock at bottom of Outer Heaven). Background/UI
   perfect; **player sprite entirely absent** — the exact reported failure, reproduced headless.

## 2. Observed VDP state at gameplay (pre-fix build)

Mode base `0x0C` (GRAPHIC4 / SCREEN 5 → sprite mode 2). Registers:
`R#0=06 R#1=62 R#2=1F R#5=E7 R#6=1F R#8=08 R#9=80 R#11=01 R#23=00 R#25=00`.

- `R#8=0x08`: SPD (bit1) clear → sprites enabled. Polarity in `SpriteEngine` verified correct.
- `R#1=0x62`: display enabled, 16x16 sprites.
- Sprite attribute base registers: `R#5=0xE7`, `R#11=0x01`.

VRAM content at the two candidate table locations:

| Location | Address | Content |
| --- | --- | --- |
| Pre-fix engine read (naive `((R11<<15)\|(R5<<7))+512`) | 0xF580 | **all zeros** (32 phantom Y=0/X=0/pattern-0 sprites, zero pattern bytes → nothing drawn) |
| Pre-fix engine colors (naive base) | 0xF380 | all zeros → every pixel color 0 (transparent) |
| openMSX-semantics read (masked, `+512`) | 0xF200 | **real data**: sprite 0 `Y=B0 X=2A pat=00`, sprite 1 `Y=B0 X=2A pat=04` (the 16x16 player, two sprite planes), rest parked at Y=0xE0 |
| openMSX-semantics colors (masked base) | 0xF000 | sprite 0: `0E` x16 lines, sprite 1: `0F` x16 lines |
| Sprite pattern table (`R#6<<11`) | 0xF800 | real pattern bitmaps |

Conclusion from data alone: the game wrote perfectly sane sprite tables; the engine read them from
the wrong address. "Renderer ignores good data" — variant: **data was never where the engine
looked.**

## 3. Root cause

In sprite mode 2, the V9958's sprite attribute table registers form an **AND-mask**, not a plain
base address. openMSX (behavior reference; read, never copied):

- `references/openmsx-21.0/src/video/VDP.cc:1357-1371` (`updateSpriteAttributeBase`):
  `baseMask = (R#11<<15) | (R#5<<7) | 0x7F`, `indexMask = ~0u<<7` (mode 1) / `~0u<<10` (mode 2).
- `references/openmsx-21.0/src/video/VDPVRAM.hh:263-279` (`readNP`/`getReadArea`): effective
  address = `baseMask & (indexMask | index)`.
- `references/openmsx-21.0/src/video/SpriteChecker.cc:283-330` (`checkSprites2`): Y/X/pattern
  sub-table read at index `512 + sprite*4 + k`; per-line color byte at index
  `(~0u<<10) | (sprite*16 + spriteLine)`.

So R#5's low 3 bits (address bits A9-A7) are **mask bits** in mode 2. Real software sets them to 1
(BIOS SCREEN5 `R#5=0xEF`; Metal Gear `R#5=0xE7`), making the table a 1KB-aligned region: per-line
colors at offsets 0-511, Y/X/pattern sub-table at offsets 512-1023. For Metal Gear
(`R#5=0xE7`,`R#11=0x01`): colors at 0xF000, Y/X/pattern at 0xF200.

The pre-fix `SpriteEngine::recompute_frame()` treated R#5's full value as a plain base
(`attrib_base = (R11<<15)|(R5<<7)` = 0xF380) and added +512 → read Y/X/pattern from 0xF580 (a
zero-filled gap between the game's real tables and its pattern table) and colors from 0xF380.
Every sprite therefore decoded as Y=0/X=0/pattern-0 with all-zero pattern bytes and color 0 —
nothing was ever composited. This affects **every sprite-mode-2 title** (all MSX2/2+ SCREEN 4-8
games), not just Metal Gear; mode 1 (`GRAPHIC1/2/Multicolor`) was and remains correct (all 8 R#5
bits are genuine base bits there, and the ≤128-byte index never collides with the `|0x7F` term).

## 4. Fix

`src/devices/video/sprite_engine.cpp` (`recompute_frame`, mode-2 path only):

- `attrib_base_mask = (R#11<<15) | (R#5<<7) | 0x7F`
- every mode-2 table read goes through `mode2_attr_addr(index) = attrib_base_mask & (~0x3FF | index)`
  with index `512 + sprite*4 + {0,1,2}` (Y/X/pattern) and `sprite*16 + line` (per-line color).

The planar (G6/G7) rotate-right-by-1 transform is applied after the mask combine, exactly as
before — equivalent to openMSX's transformed-mask formulation because the rotation is a pure bit
permutation that distributes over AND/OR. Mode 1 and the sprite pattern-table decode
(`R#6<<11`, index < 2048) are unchanged (already equivalent to the reference formulas).

Note: with mask bits **cleared**, the fix also reproduces real hardware's aliasing (address bits
forced to 0), which openMSX exhibits — see §6.3.

## 5. Tests

- **New:** `tests/unit/devices/video_sprite_engine_mode2_attribute_masking_unit_test.cpp`
  (registered in `tests/CMakeLists.txt` as
  `devices_sprite_engine_mode2_attribute_masking_unit_test`):
  - Case 1 — the exact Metal Gear register shape (`R#5=0xE7 R#11=0x01 R#6=0x1F`, 16x16, sprite 0
    at Y=0xB0/X=0x2A): asserts engine per-line selection AND `FrameBuffer` pixels at (42,177) and
    (57,177). Fails on the pre-fix code (sprite invisible).
  - Case 2 — BIOS SCREEN5 shape (`R#5=0xEF`) with a decoy sprite planted at the pre-fix naive
    read address (0x7980): asserts the real sprite (masked address) is read and the decoy is not.
  - Two-run determinism oracle (byte-identical frames).
- **Updated:** `tests/unit/devices/video_sprite_engine_mode2_unit_test.cpp` and
  `tests/unit/devices/video_vdp_frame_renderer_sprites_unit_test.cpp` — their mode-2 helpers now
  program `R#5=0x07` (mask bits set, base 0), matching real software convention; their VRAM
  layouts (colors at 0, Y/X/pattern at 512) are unchanged. The pre-fix versions left `R#5=0x00`,
  which only worked because the engine's addressing was wrong in the same direction.

## 6. Evidence

### 6.1 Before/after frame dumps (this emulator, headless MG reproduction)

- `debug/frames/mg-sprite-invisibility-title.png` — title screen (identical before/after,
  SHA256-identical dumps; title has no sprites).
- `debug/frames/mg-sprite-invisibility-before.png` — gameplay, pre-fix: **no player sprite**.
- `debug/frames/mg-sprite-invisibility-after.png` — gameplay, post-fix, same deterministic frame
  count: **Snake visible at (42,176)**, exactly where the SAT places him. Visually confirmed by
  reading the PNGs, not inferred.

### 6.2 openMSX A/B (WSL `/usr/bin/openmsx`, machine `Sony_HB-F1XV`, natural boot, no reset)

Same `roms/metalgear.rom` (md5-verified identical), booted naturally with `-carta`, SPACE via
`keymatrixdown 8 1` (t=25s..27s emulated), sampled at t=45s in live gameplay:

- **Register file byte-identical to ours at gameplay:**
  `R#0=06 R#1=62 R#2=1F R#5=E7 R#6=1F R#8=08 R#9=80 R#11=01 R#23=00 R#25=00`.
- **Visual reference:** `debug/frames/mg-sprite-invisibility-openmsx-reference.png` (openMSX
  `screenshot -raw` at the sampling instant) shows Snake at the same bottom-left position on the
  same first screen as our fixed frame.
- **Table placement:** the game's sprite writes (both the Y=0xE0 park fills and live data) appear
  ONLY at the masked 1KB-aligned tables (0xF200 and its double-buffer sibling 0xF600; MG flips
  `R#5` between 0xE7/0xEF per frame). The naive/pre-fix addresses (0xF580/0xF980) read **all
  zeros** on openMSX — no sprite table has ever lived there.
- Honest caveat: the CPU-time `debug read` samples frequently catch the SAT in its parked phase
  (MG parks then re-places sprites every frame inside the interrupt cycle), while the frame
  renderer latches the live phase — our own reproduction's post-`on_vsync` dump caught the live
  bytes (`B0 2A 00 / B0 2A 04`). The register-file identity plus the screenshot plus the
  zeros-at-naive-address findings are the cross-engine gates.
- Operational note: under WSL, openMSX needed `set throttle off` to advance emulated time
  reliably for scripted runs; `after time`-scheduled events otherwise starved when the host was
  loaded.

### 6.3 The recorded M22 divergence is now explained

`docs/m22-parity-trace-diff.md` recorded `sprite_mode2_ninth_ic -- Result: DIVERGENCE`
(S#0: A=E8 B=BF). The probe (`tools/gen-m22-sprite-cmd-probe.py`) pins `R#5=0x00` while placing
Y/X/pattern at logical 512 — with mask bits cleared, real openMSX forces address bits A9-A7 to 0
and reads aliased locations (which on the B side also contain BIOS leftovers), while the pre-fix
engine read the naive +512 location. The two engines were reading *different bytes by design of
the bug*. Re-baselining that probe with mask-bit-correct programming is left for a future
milestone (M22 artifacts are historical records; not rewritten here). The new regression unit
test covers the semantics directly.

## 7. Regression results (final tree, all captured from real runs)

- `tools/validate-assets.ps1` — pass (required BIOS present, 2 ROMs incl. `metalgear.rom`).
- `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt` — regenerated.
- `cmake --build build --config Debug` — exit 0.
- `ctest --test-dir build -C Debug -LE m24_slow_full_sweep` — **146/146 passed** (14.6 s).
- Full unfiltered `ctest --test-dir build -C Debug` — **147/147 passed** including the
  `m24_slow_full_sweep` ZEXALL/ZEXDOC sweep (1434 s), per the standing slow-cadence rule.
- Scope check: `git diff v1.0.28 --name-only -- src/devices/ src/peripherals/ src/core/` →
  `src/devices/video/sprite_engine.cpp` only. Nothing under `src/devices/cpu/` or `src/core/`.
- Temporary `src/main.cpp` diagnostic scaffolding reverted (`git checkout -- src/main.cpp`).

## 8. Secondary observation (recorded only, out of scope this cycle)

The human also reports slight audio latency in the SDL3 app (punch sound arrives a bit late),
suspected `Sdl3AudioPresenter` buffering. Nothing was learned incidentally during this
investigation that confirms or localizes it (this work never entered the audio path). Left for a
future milestone.
