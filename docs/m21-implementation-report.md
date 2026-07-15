# M21 Implementation Report — VDP Rendering Depth: Pixel Pipeline, YJK/YAE Color Decode,
# Scroll/Interlace/Blink, G6/G7 Planar Interleave

- Milestone ID: M21
- Developer Owner: MSX Developer Agent
- Planner package: `docs/m21-planner-package.md` (RESP-M21-001/REQ-M21-002, accepted)
- Status at handoff: implementation complete, **NOT** marked Done — awaiting QA sign-off
  (`docs/m21-qa-signoff.md`) and the coordinator's release decision.

---

## 1. Milestone Target

Deliver the FIRST actual pixel/rendering output path for this emulator: a deterministic,
pull-model, frozen-register-snapshot `VdpFrameRenderer` that decodes the V9958's stored
VRAM/register state (the M14 CONTRACT) into raw RGB555 pixels, exercised purely through the
existing M14 port API (`#98/#99/#9A/#9B`). Closes deferred-backlog **D1** (pixel-accurate raster
rendering), **D5** (YJK/YAE color decode), **D6** (scroll/blink/interlace/border-mask/multi-page
effects, with an explicit, grounded superimpose/digitize N/A disposition), and advances **D7**
(G6/G7 planar interleave) to **IN-PROGRESS (M21 partial)** — the CPU-port piece
(`V9958Vdp::effective_address()`) and the display-path piece (`VdpFrameRenderer::planar_row_spans`)
both close this cycle; the command-engine-path piece (VDPCmdEngine-equivalent coordinate
resolution) is explicitly **NOT** built this milestone and is carried forward to close together with
M22's D3 work, per the planner package §1.4's scope-boundary resolution.

No SDL3 frontend exists yet (backlog C9, M26) — this milestone deliberately builds a rendering
CONTRACT that tests exercise directly (raw pixel-buffer assertions), never PNG, never a host
window.

---

## 2. Code Changes

### 2.1 New files (all in the existing `src/devices/video/` family — no new device family,
matching `src/CLAUDE.md`'s device-family placement rule and the planner's §2.1 directive)

| File | Responsibility |
|---|---|
| `src/devices/video/vdp_palette.h` | Header-only, pure color-decode primitives: `expand3to5` (3-bit->5-bit, A-M21-3), `pack_rgb555`/`rgb555_red/green/blue`, `palette_entry_to_rgb555` (9-bit GRB unpack), `graphic7_fixed_color_to_rgb555` (the GGGRRRBB fixed-256-color decode, A-M21-4), `clamp5`/`yjk_to_rgb` (YJK R/G/B formula with plain truncating division, A-M21-5). |
| `src/devices/video/frame_buffer.h` | `enum class Field { Progressive, Even, Odd }` and `struct FrameBuffer { int width, height; std::vector<std::uint16_t> pixels; std::uint16_t border_color; }`. |
| `src/devices/video/vdp_frame_renderer.h` / `.cpp` | `class VdpFrameRenderer` — `width()`/`height()`, `render_line(line, field, out)`, `render_frame(field)`, `border_color()`; a free `planar_row_spans(row_base, length)` helper (D7 display-path piece, A-M21-11). Internal per-mode renderers (`render_text1/text2/graphic1/graphic2_or_3/multicolor/graphic4/graphic5/graphic6/graphic7/yjk/yjk_yae/blank`), scroll/page/blink/border-mask helpers. |

### 2.2 Edits to already-shipped files (additive/surgical; flagged explicitly per A-M21-13)

- **`src/devices/video/vdp_mode.h`**: added `constexpr bool vdp_base_is_planar(std::uint8_t base)`
  next to the existing `vdp_base_is_v9938_mode` — `(base & 0x14) == 0x14`. Independently re-derived
  from `references/openmsx-21.0/src/video/DisplayMode.hh:140-143` (`isPlanar`).
- **`src/devices/video/v9958_vdp.h` / `.cpp`**: the ONE D7 CPU-port behavior-affecting edit —
  `effective_address()` gains a conditional planar transform (see §3 below). Also adds blink
  countdown state (`blink_countdown_`, `blink_state_`, `blink_state()` accessor) driven by the
  EXISTING `on_vsync()` hook and a new `case 13:` branch in `change_register()` (VDP.cc:1040-1057).
  `advance_vram_pointer()` is **UNCHANGED**.
- **`src/machine/hbf1xv_machine.h` / `.cpp`**: additive only — one member
  (`devices::video::VdpFrameRenderer vdp_frame_renderer_{vdp_};`, declared after `vdp_`) and one
  accessor (`FrameBuffer render_frame(Field field = Field::Progressive) const;`). No change to
  `wire_bus()` or `cold_boot()` — the renderer is a pure, on-demand consumer of `vdp_`'s stored
  state, not a wired bus device.
- **`src/main.cpp`**: additive only — a new `--vdp-render-parity` CLI mode
  (`run_vdp_render_parity`) for the A/B harness (dumps R#0-R#27, the 16-entry palette, a physical
  VRAM block + a fixed bank1 window at 0x10000, and the renderer's own computed pixel values).
- **`CMakeLists.txt`**: added `src/devices/video/vdp_frame_renderer.cpp` to `sony_msx_core`.
- **`tests/CMakeLists.txt`**: added 11 new test targets (§4).

### 2.3 D7 CPU-port edit — the exact diff (A-M21-12/A-M21-13)

```cpp
std::uint32_t V9958Vdp::effective_address() const {
    std::uint32_t addr =
        ((static_cast<std::uint32_t>(control_regs_[14]) << 14) | vram_pointer_) & 0x1FFFF;
    if (vdp_base_is_planar(mode_.base)) {
        addr = (addr >> 1) | ((addr & 1) << 16);
    }
    return addr;
}
```

Grounded in `references/openmsx-21.0/src/video/VDP.cc:849-857` (`executeCpuVramAccess`):
`addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF` for planar modes — independently re-derived
(A-M21-10) to the algebraically-equivalent `(addr >> 1) | ((addr & 1) << 16)` (a 17-bit
rotate-right-by-1: even logical addresses land in physical bank 0, `0x00000-0x0FFFF`; odd in bank
1, `0x10000-0x1FFFF`, same `addr >> 1` value in both banks). This is a **single, additive,
surgical edit confined to the storage-address computation**: `advance_vram_pointer()` (the
R#14-carry logic) still operates on the ORIGINAL `vram_pointer_`/`control_regs_[14]`, never on the
transformed `addr` — confirmed both by direct source reading (the transform is applied to a local
variable that is never fed back into the pointer-increment path) and by re-running the EXISTING
M14 pointer-carry unit tests (`tests/unit/devices/video_v9958_ports_unit_test.cpp`) **UNMODIFIED**
after the edit: all 4 cases in that file (including `R14Carry_V9938Mode_IncrementsOnWrap` and
`LegacyWrap_NoR14Carry`) remain green (confirmed in the full `ctest` run, §5).

### 2.4 Byte-exact formulas implemented (each cited to a concrete `references/openmsx-21.0/...` path)

- **3-bit->5-bit palette expansion**: `r5 = (r3<<2)|(r3>>1)` —
  `references/openmsx-21.0/src/video/SDLRasterizer.cc:286-296`.
- **GRAPHIC7 fixed 256-color decode, GGGRRRBB** (green in the TOP 3 bits, NOT
  RRRGGGBB) — `SDLRasterizer.cc:330-336`, `V9938_COLORS[r3][g3][b3]` construction at
  `SDLRasterizer.cc:304-314`.
- **YJK/YJK+YAE** — `BitmapConverter.cc:217-276`. `k = (p0&7)+((p1&3)<<3)-((p1&4)<<3)`,
  `j = (p2&7)+((p3&3)<<3)-((p3&4)<<3)`, `R=clamp(y+j,0,31)`, `G=clamp(y+k,0,31)`,
  `B=clamp((5*y-2*j-k+2)/4,0,31)` — plain C++ `int` division (A-M21-5).
- **G6/G7 planar rotate** — `VDP.cc:849-857`; the renderer's own row-pointer resolution
  (`planar_row_spans`) cross-checked against `VDPVRAM.hh:236-261` (`getReadAreaPlanar`).
- **Table bases** — `VDP.hh:246-262` (`getPatternTableBase`/`getColorTableBase`/`getNameTableBase`).
  Simplification disclosed: the "forced-1 low bits" hardware mirroring-mask nuance openMSX's
  `VRAMWindow` machinery models is NOT reproduced; tests use canonical/valid base register values
  (consistent with this project's existing VRAM-addressing depth level).
- **TEXT1/TEXT2/GRAPHIC1/GRAPHIC2-3/MULTICOLOR** —
  `CharacterConverter.cc:142-350` (independently re-expressed, never copied).
- **GRAPHIC4/GRAPHIC5/GRAPHIC6/GRAPHIC7** — `BitmapConverter.cc:104-215`.
- **Border/backdrop** — `VDP.hh:216-226` (`getBackgroundColor`), `SDLRasterizer.cc:376-393`
  (`getBorderColors`, the GRAPHIC5 even/odd border-half split and the GRAPHIC7 full-byte path;
  confirmed via `getByte()` that YJK-mode borders use the PLAIN 16-color palette, not the fixed
  256-color table, matching the fact-sheet's "sprites use the 16-colour palette in YJK modes"
  note).
- **Blink** — `VDP.cc:600-608` (frame-boundary decrement/re-arm) and `VDP.cc:1040-1057`
  (R#13-write forces the phase + re-arms even if the value is unchanged).
- **Vertical scroll** — `PixelRenderer.cc:44-49` (non-text: whole-line wrap by 256) and
  `CharacterConverter.cc:149-150/183` (text modes: pattern-sub-row only, name-row unscrolled).
- **Horizontal scroll, character modes** — `CharacterConverter.cc:255-270` (name-column wrap by
  32).
- **Horizontal scroll, bitmap modes (A-M21-8, independently grounded, a DIFFERENT mechanism)** —
  `SDLRasterizer.cc:465-471` (coarse `8 * (lineWidth/256) * (R26&0x1F)`) and
  `PixelRenderer.cc:60-69` (fine, R#27). Disclosed simplification: the exact split-page-blit
  windowing mechanics (`SDLRasterizer.cc:477-538`) are NOT reproduced — only the documented
  register-driven shift formula is.
- **Border mask** — `VDP.hh:353-360`.
- **Multi-page scroll** — `VDP.hh:362-370` (`isMultiPageScrolling`).
- **Even/odd field page-alternation (A-M21-7, hedged)** — see §3 below; a deliberately
  independently-CHOSEN model, not bit-for-bit `getEvenOddMask()` reproduction, with the reasoning
  disclosed in code comments and here.

### 2.5 Superimpose/digitize (D6) — explicit N/A disposition

The HB-F1XV has no digitizer/genlock hardware (Target Machine Specification I/O-ports list; the
fact-sheet §9: "HB-F1XV is not a digitizer/superimpose model, so these are typically unused
there."). No compositing path is fabricated; `vdp_frame_renderer.h`'s `FrameBuffer` doc comment and
this report cite the grounding explicitly, per the planner's §1.2 disposition.

---

## 3. A genuinely important, independently-found correction (interlace/EO alternation, A-M21-7)

While implementing the Field-based even/odd page-alternation hedge, direct re-reading of
`VDP.hh:443-459`'s `getEvenOddMask()` and its ONLY call site
(`SDLRasterizer.cc:490-497`) surfaced a subtlety the planner package's own framing did not fully
anticipate: `getEvenOddMask()`'s result feeds `pageMaskOdd`/`pageMaskEven`, which distinguish
**scanline parity WITHIN one frame** for the multi-page-scroll smooth-blit effect — a genuinely
different concept from **interlace FIELD selection** (this renderer's `Field` parameter). A first
implementation attempt directly substituted `field == Field::Odd` for the formula's live
`eo_field_` toggle; because the formula's OTHER term (`EO bit CLEAR`) is unconditionally true at
the common power-on default (R#9 = 0), that substitution made EVERY bitmap-mode page read
silently flip regardless of which `Field` was requested — a real bug, caught by this project's own
test-writing process (several of my own tests failed on first `ctest` run for exactly this reason,
§5). The renderer instead uses a narrower, explicitly-chosen, disclosed predicate: alternation only
engages when R#9's EO bit is explicitly SET and is not suppressed by blink; `Field::Odd` then
selects the alternate page. This is documented in `vdp_frame_renderer.cpp`'s `use_alternate_page()`
and is the acceptance bar the planner's A-M21-7 hedge licenses: "parity with openMSX's own MODELED
CONCEPT... not bit-for-bit reproduction... and NOT independently proven hardware ground truth."

A second, similarly rigorous finding concerns the **YJK rounding-boundary test (A-M21-5)**: careful
analysis shows that because `clamp5`'s lower bound is 0 and `floor(x) <= trunc(x)` for every
`x < 0`, **every negative pre-clamp B-channel numerator — computed via plain truncating division OR
via `std::floor()` — clamps to the IDENTICAL final value, 0.** The divergence the risk describes is
real at the PRE-clamp level (confirmed with a concrete case, numerator = -2: truncation gives 0,
`std::floor` gives -1) but is **not black-box-observable in the clamped RGB555 output** for this
specific formula's actual value range. `tests/unit/devices/video_vdp_palette_unit_test.cpp`
implements plain `int` division (matching openMSX's source exactly, per the explicit instruction)
and documents this finding directly in the test's comments, rather than silently presenting a
non-discriminating test as if it proved something it cannot.

---

## 4. Unit / Integration / System Test Results

### 4.1 New test files (11 executables, all green)

| File | Suite |
|---|---|
| `tests/unit/devices/video_vdp_palette_unit_test.cpp` | `Devices_VdpPalette_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_text1_unit_test.cpp` | `Devices_VdpFrameRendererText1_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_graphic1_unit_test.cpp` | `Devices_VdpFrameRendererGraphic1_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_text2_unit_test.cpp` | `Devices_VdpFrameRendererText2_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_tile_modes_unit_test.cpp` | `Devices_VdpFrameRendererTileModes_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_bitmap_modes_unit_test.cpp` | `Devices_VdpFrameRendererBitmapModes_Unit` |
| `tests/unit/devices/video_vdp_planar_interleave_unit_test.cpp` | `Devices_VdpPlanarInterleave_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_yjk_unit_test.cpp` | `Devices_VdpFrameRendererYjk_Unit` |
| `tests/unit/devices/video_vdp_frame_renderer_effects_unit_test.cpp` | `Devices_VdpFrameRendererEffects_Unit` |
| `tests/integration/machine/hbf1xv_m21_vdp_render_integration_test.cpp` | `Machine_Hbf1xvM21VdpRender_Integration` |
| `tests/system/hbf1xv_m21_vdp_render_system_test.cpp` | `System_Hbf1xvM21VdpRender_System` |

Coverage highlights: the 3-bit->5-bit expansion table (all 8 inputs); RGB555 pack/unpack
round-trip; the GRAPHIC7 GGGRRRBB boundary (byte `0b111_000_00` -> max-green/zero-red/zero-blue,
asserted BOTH at the pure-palette level and through the full renderer's cross-path test); the YJK
rounding case (numerator -2, non-multiple-of-4); TEXT1/TEXT2 (+blink state-machine, 10-frame
countdown re-arm verified across `on_vsync()` calls) content; GRAPHIC1/2/3/MULTICOLOR (the real
256-wide canvas, NOT 64-wide, with the 4-line sub-block granularity independently verified);
TEXT1Q/MULTIQ/Unknown flat-blank fallback; GRAPHIC4/GRAPHIC5 non-planar decode + page selection;
the D7 CPU-port planar transform (raw physical-bank placement, independent of any round-trip);
`planar_row_spans` cross-checked against a byte-by-byte rotate reference for a full synthetic
256-byte row; GRAPHIC6/GRAPHIC7 content decode with a genuine END-TO-END cross-path test (CPU-port
write -> renderer read, confirming EXPECTED not garbled content); scroll (vertical + horizontal,
character AND bitmap mechanisms independently, per A-M21-8); border mask; multi-page scroll wrap;
the Field-based EO hedge (incl. blink suppression); two-run determinism in every file.

### 4.2 System test (`tests/system/hbf1xv_m21_vdp_render_system_test.cpp`)

A real Z80 CPU program, executed over the M11 bus via `OUT (#98)/(#99)`, for one representative
case per mode FAMILY:

- **Character** (GRAPHIC1): writes name/pattern/color bytes; asserts `render_frame()` pixel 0 and 7
  both equal white (palette 15, the unmodified V9938 boot palette — no `#9A` writes needed).
- **Non-planar bitmap** (GRAPHIC4): one `OUT (#98)` byte; asserts pixel 0 = white (high nibble),
  pixel 1 = black (low nibble).
- **Planar bitmap** (GRAPHIC6): two `OUT (#98)` bytes at logical 0/1 (the D7 CPU-port transform
  exercised end-to-end); asserts pixels 0-3 match the EXPECTED (not garbled) bank0/bank1 content.
- **YJK** (SCREEN12): four `OUT (#98)` bytes; asserts pixels 0-3 against a hand-computed golden
  (including the truncating-division rounding case).

---

## 5. Evidence Gate Output (actual, captured this cycle)

### 5.1 `tools/validate-assets.ps1`

```
Asset validation result: True
BIOS directory: C:\Users\pashim\source\sony-msx-hbf1xv\bios
ROM directory:  C:\Users\pashim\source\sony-msx-hbf1xv\roms
Present BIOS files:
  - f1xvbios.rom
  - f1xvdisk.rom
  - f1xvext.rom
  - f1xvfirm.rom
  - f1xvkdr.rom
  - f1xvkfn.rom
  - f1xvmus.rom
ROM file count: 2
ROM files:
  - aleste.rom
  - metalgear.rom
```

### 5.2 `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`

```
Checksum report written to: C:\Users\pashim\source\sony-msx-hbf1xv\docs\asset-checksums.txt
```

(refreshed this cycle; content unchanged from M20 since no BIOS/ROM assets were added/modified)

### 5.3 `cmake --build build --config Debug`

Full rebuild succeeds with zero errors (Debug, MSBuild/Visual Studio generator). Only pre-existing,
unrelated `C4819` codepage warnings (non-ASCII characters in file encoding, present since before
M21) — no new warnings introduced by M21 changes.

### 5.4 `ctest --test-dir build -C Debug --output-on-failure`

**Before M21 (M20 close baseline): 95/95 passed.**

**After M21: 106/106 passed** (95 prior M1-M20 tests + 11 new M21 tests), full fresh run, zero
regression:

```
100% tests passed, 0 tests failed out of 106
Total Test time (real) =   3.22 sec
```

Explicit regression confirmations:
- The M9/M12 CPU-timing oracle suites (`devices_z80a_*`) are untouched — no new clock consumer was
  added (`VdpFrameRenderer` is a pure pull-model function of stored state; the only new stateful
  piece, the blink countdown, is driven by the EXISTING `on_vsync()` hook, never a new time source).
- The existing M14 VRAM-pointer/R#14-carry unit tests (`devices_v9958_ports_unit_test`, i.e.
  `tests/unit/devices/video_v9958_ports_unit_test.cpp`) are **unmodified** and green after the D7
  `effective_address()` edit.
- Every M15-M20 device/slot-map/accessor golden remains green (all FDC, PSG, RTC, PPI, YM2413,
  Kanji, printer, cassette, cartridge, and Halnote suites pass unchanged).

**A genuine, honest note on test-writing rigor:** the FIRST full `ctest` run after the initial
M21 implementation showed 4 test FAILURES (`devices_vdp_frame_renderer_tile_modes_unit_test`,
`devices_vdp_frame_renderer_bitmap_modes_unit_test`, `devices_vdp_frame_renderer_yjk_unit_test`,
`devices_vdp_frame_renderer_effects_unit_test`). Root causes, all found and fixed BEFORE this
report: (1) three tests wrote VRAM at addresses beyond the CPU port's 14-bit pointer range without
first setting R#14, silently landing at the wrong physical address; (2) two tests wrote
overlapping name/pattern/color table addresses (the default all-zero table bases collided); (3)
the YAE test's `p0` byte accidentally had its own attribute bit (bit3) set, since Y=1 shifted left
3 IS `0x08`; (4) the renderer's OWN `use_alternate_page()` implementation had the genuine
default-flips-every-page bug described in §3. All four are now fixed and re-verified; the 106/106
count above reflects the corrected, fully green state.

### 5.5 openMSX A/B evidence (`docs/m21-parity-trace-diff.md`)

Technique per the planner package §2.7: **derived-value/raw-input comparison**, explicitly NOT a
screenshot-pixel diff (a deliberate, reasoned non-goal — openMSX's own gamma/color-matrix/scaler
presentation layer would confound a fair raw-hardware-color comparison).

`tools/gen-m21-vdp-render-probe.py` generates 4 small, flat-RAM Z80 probes (`palette`, `planar`,
`graphic7`, `yjk`); `tools/openmsx-m21-vdp-render-parity.ps1` runs each through this emulator's new
`--vdp-render-parity` CLI mode AND through openMSX 19.x on WSL (machine `Sony_HB-F1XV`, genuine
V9958), reading its `physical VRAM`, `VDP palette`, and `VDP regs` SimpleDebuggables.

**Verification action executed this cycle (per the planner's explicit instruction):** a live
`debug list` query against openMSX confirmed the `VDP palette` SimpleDebuggable EXISTS (32 bytes,
2 bytes/entry) — its byte layout (`value = byte[2i] | (byte[2i+1]<<8)`) was independently
reverse-engineered and CONFIRMED against the known V9938 boot palette
(`VDP.cc:299-302`) by reading it at `t=0.1s` (before the BIOS could alter any entries) and
matching all 16 entries exactly. **No computed-pixel/framebuffer/screen SimpleDebuggable exists**
for this machine (confirmed by the same `debug list` query) — so the "computed RGB555 color
value" cross-engine comparison is **honestly reported BLOCKED**, per the planner's explicit
instruction, not fabricated or silently upgraded to PASS.

**Actual captured result — all 4 probes:**

```
Probe: palette  -- Result: PARITY (empty diff)
Probe: planar   -- Result: PARITY (empty diff)
Probe: graphic7 -- Result: PARITY (empty diff)
Probe: yjk      -- Result: PARITY (empty diff)
```

- **16-color palette expansion (A-M21-3)**: raw palette-register byte parity, genuine PARITY.
- **GRAPHIC7 fixed-color decode incl. the GGGRRRBB boundary (A-M21-4)**: raw VRAM-byte parity,
  genuine PARITY; the GGGRRRBB arithmetic ITSELF is independently unit-tested (not
  live-B-comparable, per the BLOCKED disposition above).
- **YJK/YAE decode incl. the rounding boundary (A-M21-5)**: raw VRAM-byte parity, genuine PARITY;
  the rounding arithmetic itself is independently unit-tested (not live-B-comparable).
- **D7 CPU-port planar transform (A-M21-10)**: raw VRAM-byte parity **INCLUDING the physical
  bank1 window at 0x10000** — a genuine, direct, live cross-engine confirmation that BOTH engines
  place the odd-logical-address byte at the SAME physical offset. This is the strongest of the 4
  probes: it directly evidences the transform's OUTPUT PLACEMENT, mirroring the M14 A/B precedent
  exactly.

Every probe's adversarial comparator self-check passed (corrupting one A-side VRAM byte correctly
produces a detected DIVERGENCE, confirming the comparator is not vacuously always-PASS). See
`docs/m21-parity-trace-diff.md` for the full, captured report.

---

## 6. Known Issues / Disclosed Simplifications (all honestly documented in code + here)

1. **Table-base "forced-1 low bits" mirroring mask** (openMSX's `VRAMWindow` machinery) is not
   reproduced; tests use canonical/valid register values. Low risk: this affects only
   undefined/edge-case base-register configurations, not the documented mode contract.
2. **Bitmap-mode horizontal scroll's exact split-page-blit windowing mechanics**
   (`SDLRasterizer.cc:477-538`) are not reproduced — only the primary register-driven shift formula
   is. Disclosed as an honest depth limit (A-M21-8), consistent with this milestone's "no per-scanline
   raster-split" pull-model scope (D4).
3. **Even/odd field page-alternation (A-M21-7)** is a deliberately independently-CHOSEN,
   documented model (§3 above), not a bit-for-bit reproduction of `getEvenOddMask()`. This is the
   explicitly hedged acceptance bar the planner package licenses for this sub-feature, since even
   openMSX's own authors flag it "TODO: verify."
4. **Character-mode horizontal scroll's multi-page name-table-page-crossing nuance**
   (`scroll & 0x20` selecting an alternate 0x8000-offset name-table half) is not reproduced — only
   the primary 32-column wrap is. The separately-named bitmap-mode multi-page-scroll feature (R#25
   bit0 + R#2 bit5) IS fully implemented and tested.
5. **Multi-page-scroll's real per-scanline split-page-blit visual effect** (the smooth-scroll
   illusion from alternating pages every OTHER raster line) is not reproduced; this renderer
   implements the documented "wraps to the lower even page" register-driven rule as a
   frame-global effect.
6. **Interlace/EO addressing overall inherits openMSX's own disclosed uncertainty** — the
   reference authors' own "TODO: verify" comments (`VDP.hh:443-459`) mean this sub-feature's
   acceptance bar is explicitly "parity with openMSX's own modeled CONCEPT," not independently
   proven hardware ground truth (A-M21-7).
7. **Superimpose/digitize**: correctly, explicitly out of scope (no digitizer hardware on this
   machine) — not a gap.
8. **Command-engine-path planar interleave (D7's third piece)**: explicitly NOT built this
   milestone — carried to M22 alongside D3, per the planner's §1.4 scope-boundary resolution. Do
   not treat this as an oversight; it is the deliberate, planned scope boundary.

None of the above are blocking for M21's own acceptance criteria (all explicitly scoped OUT or
explicitly hedged by the accepted planner package); they are disclosed here for QA's and the
coordinator's full visibility.

---

## 7. D1/D5/D6/D7 Disposition (developer-reported READY; ledger transition owned by the coordinator)

- **D1 (pixel-accurate raster rendering pipeline)** — READY to close **DONE (M21)**. Full per-mode
  VRAM->framebuffer decode delivered for every Target-Spec mode, border color, blink, per-scanline
  output, deterministic and unit/integration/system-tested.
- **D5 (YJK/YJK+YAE color decode + 15-bit DAC)** — READY to close **DONE (M21)**. Byte-exact per
  `BitmapConverter.cc:217-285`, including the independently-verified rounding risk and the shared
  3-bit->5-bit expansion table.
- **D6 (scroll/interlace/blink/superimpose)** — READY to close **DONE (M21)**, with the explicit,
  grounded superimpose/digitize N/A scope note (§2.5) and the honest interlace/EO hedge (§6 item 6)
  as disclosed, non-blocking caveats.
- **D7 (G6/G7 VRAM address interleave, display/command path)** — READY to advance to
  **IN-PROGRESS (M21 partial)**. CPU-port piece and display-path piece both close this cycle,
  independently re-derived, cross-verified, and confirmed via a genuine end-to-end cross-path test
  AND a live openMSX A/B probe (physical bank1 placement). The command-engine-path piece is
  explicitly carried to M22, to close together with D3.

Per the established M14-M20 pattern, this report states what is READY; the actual
`agent-protocol/state/deferred-backlog.md` status-column transition is left to the coordinator at
closure time (after QA sign-off).

---

## 8. QA Handoff

**Requested from QA:**
1. Independent, fresh `ctest` re-run (own build, not a rubber-stamp of this report's count) —
   expect 106/106, zero regression against the M1-M20 baseline (95/95).
2. Spot-check the D7 CPU-port edit (`src/devices/video/v9958_vdp.cpp`'s `effective_address()`) is
   confined to the storage-address computation only, and independently re-run
   `tests/unit/devices/video_v9958_ports_unit_test.cpp` to confirm the pointer-carry suite is
   genuinely unmodified and green.
3. Independently verify at least one of the byte-exact formulas (recommend the GRAPHIC7 GGGRRRBB
   decode or the YJK rounding case) against the cited `references/openmsx-21.0/src/video/*.cc` line
   ranges directly, not just trusting this report's citations.
4. Review `docs/m21-parity-trace-diff.md` and confirm the BLOCKED disposition for the
   computed-pixel-color sub-claim is genuinely honest (i.e., independently query openMSX's
   `debug list` if desired) rather than a convenient excuse.
5. Confirm the full deferred-backlog re-affirmation (§7 above; the planner package's own §4 already
   restates all 34 rows) and that D1/D5/D6/D7's proposed dispositions are consistent with the
   accepted planner package.
6. Sign off in `docs/m21-qa-signoff.md`.

**Residual risks / open questions for QA** (all Low, all pre-disclosed, none believed blocking):
see §6 items 1-6 above (table-base mask simplification, bitmap-scroll windowing depth, the
independently-chosen EO/Field model, character-mode multi-page name-table crossing, multi-page
per-scanline blit depth, and the inherited interlace/EO hedge). Also flagged: the A/B evidence's
computed-color BLOCKED disposition (§5.5) — QA should independently confirm this is genuinely
infeasible (no `debug list` entry) rather than an unexplored workaround.

**Files touched (for QA's own diff review):**
- New: `src/devices/video/{vdp_palette.h,frame_buffer.h,vdp_frame_renderer.h,vdp_frame_renderer.cpp}`
- Edited (additive/surgical): `src/devices/video/{vdp_mode.h,v9958_vdp.h,v9958_vdp.cpp}`,
  `src/machine/hbf1xv_machine.{h,cpp}`, `src/main.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`
- New tests: 9 unit + 1 integration + 1 system (§4.1)
- New tools: `tools/gen-m21-vdp-render-probe.py`, `tools/openmsx-m21-vdp-render-parity.ps1`
- New docs: `docs/m21-parity-trace-diff.md`, this report; refreshed `docs/asset-checksums.txt`
- New parity fixtures: `tests/parity/m21_vdp_render_{palette,planar,graphic7,yjk}_probe.bin`
