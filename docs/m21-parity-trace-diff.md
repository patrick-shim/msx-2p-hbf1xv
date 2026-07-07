# M21 openMSX VDP-Render Parity Trace Diff (Sony HB-F1XV)

- Captured (UTC): 2026-07-07T05:57:56Z
- Reference: openMSX on WSL, machine `Sony_HB-F1XV` (genuine V9958).
- Technique: derived-value / raw-input comparison (docs/m21-planner-package.md section 2.7), NOT a screenshot-pixel diff (deliberate non-goal, see the script header).
- A = this emulator (`--vdp-render-parity`); B = openMSX (`physical VRAM` / `VDP palette` / `VDP regs` SimpleDebuggables).
- Cross-engine-comparable gate fields: raw VRAM bytes (incl. the physical bank1 window at 0x10000, evidencing the D7 CPU-port planar transform), the raw 16-entry palette register file, and the explicitly-written R#0/R#25.
- NOT live-cross-engine-comparable (honest BLOCKED disposition, not fabricated): a COMPUTED RGB555 pixel/color value. openMSX's Tcl debugger exposes no computed-pixel-color SimpleDebuggable (confirmed via a live `debug list` query this cycle -- no framebuffer/pixel/screen entry exists for this machine). This engine's OWN computed pixel values are recorded below for the record (already independently unit-tested against the cited formulas), NOT claimed as a live B-side match.

## Probe: palette -- Result: PARITY (empty diff)

- Adversarial comparator check: PASS (corrupted VRAM[0]: 00 -> 01 diverges from B=00)
- Computed pixel values (A-side, this engine, NOT live-B-comparable -- see disposition above):
  - PX00=0000
  - PX01=0000
  - PX02=0000
  - PX03=0000
  - PX04=0000
  - PX05=0000
  - PX06=0000
  - PX07=0000

### Diff (empty = pass, raw VRAM/palette/register fields only)
```
(no differences on the gate fields)
```

## Probe: planar -- Result: PARITY (empty diff)

- Adversarial comparator check: PASS (corrupted VRAM[0]: F0 -> F1 diverges from B=F0)
- Computed pixel values (A-side, this engine, NOT live-B-comparable -- see disposition above):
  - PX00=7FFF
  - PX01=0000
  - PX02=0000
  - PX03=6F64
  - PX04=0000
  - PX05=0000
  - PX06=0000
  - PX07=0000

### Diff (empty = pass, raw VRAM/palette/register fields only)
```
(no differences on the gate fields)
```

## Probe: graphic7 -- Result: PARITY (empty diff)

- Adversarial comparator check: PASS (corrupted VRAM[0]: E0 -> E1 diverges from B=E0)
- Computed pixel values (A-side, this engine, NOT live-B-comparable -- see disposition above):
  - PX00=03E0
  - PX01=7C00
  - PX02=0000
  - PX03=0000
  - PX04=0000
  - PX05=0000
  - PX06=0000
  - PX07=0000

### Diff (empty = pass, raw VRAM/palette/register fields only)
```
(no differences on the gate fields)
```

## Probe: yjk -- Result: PARITY (empty diff)

- Adversarial comparator check: PASS (corrupted VRAM[0]: 00 -> 01 diverges from B=00)
- Computed pixel values (A-side, this engine, NOT live-B-comparable -- see disposition above):
  - PX00=0800
  - PX01=0800
  - PX02=0800
  - PX03=0800
  - PX04=0000
  - PX05=0000
  - PX06=0000
  - PX07=0000

### Diff (empty = pass, raw VRAM/palette/register fields only)
```
(no differences on the gate fields)
```

## Overall

- 16-color palette expansion (A-M21-3): raw palette byte parity per the `palette` probe above.
- GRAPHIC7 fixed-color decode incl. the GGGRRRBB boundary (A-M21-4): raw VRAM-byte parity per the `graphic7` probe above; the GGGRRRBB arithmetic itself is independently unit-tested (tests/unit/devices/video_vdp_palette_unit_test.cpp), not live-B-comparable (see disposition above).
- YJK/YAE decode incl. the rounding boundary (A-M21-5): raw VRAM-byte parity per the `yjk` probe above; the rounding arithmetic is independently unit-tested, not live-B-comparable.
- D7 CPU-port planar transform (A-M21-10): raw VRAM-byte parity (INCLUDING the bank1 window at 0x10000) per the `planar` probe above -- this IS a genuine, direct, live cross-engine comparison of the transform's OUTPUT PLACEMENT, mirroring the M14 precedent exactly.

All four probes' raw VRAM/palette/register fields achieved ARCHITECTURAL PARITY (empty diff).

