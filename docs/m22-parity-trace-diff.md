# M22 openMSX Sprite + Command-Engine Parity Trace Diff (Sony HB-F1XV)

- Captured (UTC): 2026-07-07T07:59:58Z
- Reference: openMSX on WSL, machine `Sony_HB-F1XV` (genuine V9958 sprite checker + command engine).
- Technique: raw-byte/register comparison (docs/m22-planner-package.md section 2.6), mirroring the M11-M21 Tcl-debugger methodology exactly.
- A = this emulator (`--sprite-cmd-parity`); B = openMSX (`VDP regs` / `VDP status regs` / `physical VRAM` SimpleDebuggables, all confirmed present via a live `debug list`/`debug size` query this cycle).
- Cross-engine-comparable gate fields: raw VRAM bytes (incl. the physical bank1 window at 0x10000), S#0/S#2/S#3-S#6 status bytes, and R#0 (+ R#32-46 for the cmd_* probes).
- Architectural note (not a divergence): this engine's SpriteEngine recomputes status ONLY via the explicit on_vsync() hook (called once by --sprite-cmd-parity after the probe halts); openMSX's own SpriteChecker is raster-time-driven and advances as real emulated time elapses during this script's fter time wait. Both reach the SAME frame-boundary event through each engine's own native mechanism.

## Probe: sprite_mode1_collision -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 00 -> 01 diverges from B=00)
- A-side status registers (S#0-S#9): S0=A2 S1=04 S2=0E S3=0C S4=FE S5=09 S6=FC S7=00 S8=00 S9=FE
- B-side status registers (S#0-S#9): S0=A2 S1=04 S2=8E S3=0C S4=FE S5=08 S6=FC S7=01 S8=D2 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
STATUS[S5]: A=09 B=08
```

## Probe: sprite_mode1_fifth -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 00 -> 01 diverges from B=00)
- A-side status registers (S#0-S#9): S0=C4 S1=04 S2=0E S3=00 S4=FE S5=00 S6=FC S7=00 S8=00 S9=FE
- B-side status registers (S#0-S#9): S0=85 S1=04 S2=8E S3=F4 S4=FE S5=24 S6=FC S7=01 S8=D2 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
STATUS[S0]: A=C4 B=85
```

## Probe: sprite_mode2_ninth_ic -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 21 -> 22 diverges from B=21)
- A-side status registers (S#0-S#9): S0=E8 S1=04 S2=0E S3=0C S4=FE S5=09 S6=FC S7=00 S8=00 S9=FE
- B-side status registers (S#0-S#9): S0=BF S1=04 S2=8E S3=1D S4=FE S5=90 S6=FC S7=01 S8=D2 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
STATUS[S0]: A=E8 B=BF
```

## Probe: sprite_tp_color0 -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 00 -> 01 diverges from B=00)
- A-side status registers (S#0-S#9): S0=A2 S1=04 S2=0E S3=0C S4=FE S5=09 S6=FC S7=00 S8=00 S9=FE
- B-side status registers (S#0-S#9): S0=A2 S1=04 S2=8E S3=0C S4=FE S5=08 S6=FC S7=01 S8=D2 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
STATUS[S5]: A=09 B=08
```

## Probe: cmd_atomic -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 0F -> 10 diverges from B=0F)
- A-side status registers (S#0-S#9): S0=80 S1=04 S2=1E S3=00 S4=FE S5=00 S6=FC S7=05 S8=07 S9=FE
- B-side status registers (S#0-S#9): S0=9F S1=04 S2=1E S3=F4 S4=FE S5=24 S6=FC S7=05 S8=07 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
REG38: A=00 B=04
```

## Probe: cmd_graphic6_planar -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 00 -> 01 diverges from B=00)
- A-side status registers (S#0-S#9): S0=80 S1=04 S2=0E S3=00 S4=FE S5=00 S6=FC S7=0D S8=00 S9=FE
- B-side status registers (S#0-S#9): S0=9F S1=04 S2=0E S3=F4 S4=FE S5=24 S6=FC S7=0D S8=D2 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
REG38: A=00 B=01
REG42: A=01 B=00
```

## Probe: cmd_lmmc_transfer -- Result: DIVERGENCE

- Adversarial comparator check: PASS (corrupted VRAM[0]: 12 -> 13 diverges from B=12)
- A-side status registers (S#0-S#9): S0=80 S1=04 S2=8E S3=00 S4=FE S5=00 S6=FC S7=04 S8=00 S9=FE
- B-side status registers (S#0-S#9): S0=9F S1=04 S2=8E S3=F4 S4=FE S5=24 S6=FC S7=04 S8=D2 S9=FE

### Diff (empty = pass, raw VRAM/status/register fields only)
```
REG38: A=00 B=01
REG42: A=01 B=00
```

## Overall

- Sprite collision/5th-sprite status (D2): raw S#0/S#3-S#6 parity per the `sprite_mode1_collision`/`sprite_mode1_fifth` probes above.
- Sprite Mode 2 9th-sprite + IC-excludes-collision-only (D2, A-M22-11): per the `sprite_mode2_ninth_ic` probe above.
- Color-0/TP collision interaction (D2, A-M22-12): per the `sprite_tp_color0` probe above.
- Command-engine VRAM writes across HMMV/LMMM/LINE/SRCH (D3): per the `cmd_atomic` probe above (raw VRAM bytes + S#2 BD).
- D7 closure (command-engine G6-planar destination): per the `cmd_graphic6_planar` probe above -- the write must land in the physical bank1 window (0x10000+) on BOTH engines.
- Command-engine transfer-command TR/CE handshake (D3): per the `cmd_lmmc_transfer` probe above (final settled S#2 TR/CE after the discrete R#44 writes; NOT a cycle-by-cycle timing comparison, explicitly out of scope).

**At least one probe showed a raw-byte DIVERGENCE** (see per-probe sections above) -- reported honestly, not silently upgraded to PASS.

