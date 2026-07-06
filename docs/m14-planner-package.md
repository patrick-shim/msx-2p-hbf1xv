# M14 Planner Package — Yamaha V9958 VDP (incl. 128 KB VRAM ownership)

- Milestone: **M14** (title: *Yamaha V9958 VDP (incl. 128 KB VRAM ownership)*)
- Spec Owner: MSX Planner Agent
- Request: **REQ-M14-002** (planner-first; NO production code in this package)
- Closes deferred-backlog item **B9** (VRAM / V9958 VDP). Records new depth deferrals **D1–D7**.
- Grounding read in full for this package: `references/fact-sheets/Yamaha V9958 VDP.md`;
  openMSX behavior reference `references/openmsx-21.0/src/video/VDP.cc` / `VDP.hh` (register/port/
  status/interrupt core), plus `VDPVRAM.*`, `VDPCmdEngine.*`, `SpriteChecker.*`,
  `VDPAccessSlots.*` (consulted for the in-scope/deferred boundary; **behavior reference only —
  GPL, never copied into `src/`**).

---

## 1. Scope and Assumptions

### 1.1 In scope (the V9958 *contract*)

M14 delivers the **register / VRAM / status / interrupt CONTRACT** of the Yamaha V9958 as a
device that owns the 128 KB VRAM and is driven entirely through I/O ports — so software (BIOS,
BASIC, tests) can drive it and its externally observable state is A/B-parity-verified against
openMSX. Concretely:

1. **VRAM ownership migration** — the inert `vram_` (128 KB `MemoryRegion`) currently living in
   `Hbf1xvMachine` (`src/machine/hbf1xv_machine.h:184`, `.cpp:94/318/338/381`) migrates *into*
   the VDP device. After M14 there is **no CPU-addressable VRAM region** in the machine; the CPU
   reaches VRAM **only** through ports `#98/#99` (+ the S1985 `#9C/#9D` mirror).
2. **Port model** — `#98` (VRAM data r/w with auto-increment + read-ahead buffer), `#99`
   (status read via R#15 pointer / register write via the two-write register-set protocol),
   `#9A` (palette write, 9-bit GRB, two-write), `#9B` (indirect-register data with R#17
   pointer + AII auto-increment-inhibit). Reconciled onto the M11 `IoBus` and the S1985
   `#9C-#9F` mirror.
3. **Register file** — R#0–R#23 and R#25/26/27 (write-only), R#24 skipped, the R#15 status
   pointer, R#16 palette pointer, R#17 control-register pointer. Two-write latch protocol.
4. **VRAM addressing** — 17-bit address (`(R#14 << 14) | vramPointer14`), auto-increment on
   each `#98` access, carry from the 14-bit pointer into R#14 (with the legacy G1/G2/MC/T1
   non-increment/wrap exception noted), read-ahead buffer semantics.
5. **Status registers** — S#0..S#9 (V9958 set) at a **feasible, contract-level** fidelity:
   S#0 F (VBlank flag), S#1 FH (line flag) + the fixed V9958 ID (`ID#=2`, S#1=`0x04` at reset)
   with the dead LPS/FL bits reading 0, S#2 timing/status bits that are computable without a
   full raster clock, and the collision/border/color registers S#3–S#9 returning their
   documented **fixed/idle** values (their live values belong to the deferred sprite/command
   engines, D2/D3).
6. **Interrupts** — VBlank (S#0 bit7 F; enabled by R#1 bit5 IE0) and line/HBlank (S#1 bit0 FH;
   line = R#19; enabled by R#0 bit4 IE1), modeled at **frame / line-count granularity**, driving
   a **level-held `/INT`** to the M12 Z80A (cleared on the corresponding status-register read).
7. **Mode-selection decode** — the M1–M5 (R#1 b4,b3; R#0 b3,b2,b1) + LN (R#9 b7) + YJK/YAE
   (R#25 b3/b4) bits decoded into a **display-mode identity** covering the Target-Spec modes,
   plus storage of the V9958 additions (R#25 CMD/YJK/YAE/WTE/MSK/SP2, R#26/27 horizontal
   scroll). **Bit-level only — no pixels are produced.**
8. **System integration** — VDP attached to the M11 `IoBus` on `#98-#9B`; mirror-port
   equivalence via the already-wired `register_mirror` calls; a deterministic per-frame VSYNC
   event so a running CPU program can observe/accept the VBlank interrupt.

### 1.2 Non-goals (explicitly deferred; see §6 and backlog D1–D7)

Pixel-accurate raster rendering (D1), sprite rendering + collision/5th-sprite (D2), the VDP
command engine R#32–R#46 (D3), cycle-accurate access-slot / command timing and exact sub-frame
raster-IRQ position (D4), YJK/YAE color decode + 15-bit DAC output (D5), horizontal-scroll /
interlace / blink / superimpose *visual* effects (D6), and the G6/G7 planar display/command
interleave (D7). M14 stores every register bit these features need, but computes **no output
pixels or colors**.

### 1.3 Assumptions (each with a verification action)

- **A-1** The M11 `IoBus` mirror (`register_mirror(0x98,0x9C)…0x9F`) is already wired in
  `wire_bus()` (`src/machine/hbf1xv_machine.cpp:69-72`) with **no device** on `#98-#9B`.
  *Verify:* re-read those lines before wiring; attach the VDP on the four base ports only and
  confirm `#9C-#9F` resolve to it via `IoBus::resolve` one-hop (`io_bus.cpp:28-32`).
- **A-2** `IoBus::resolve` passes the **original** port to `device->io_read(port)` (mirror
  included), so the VDP must decode `port & 0x03` (both base `0x98-0x9B` and mirror
  `0x9C-0x9F` collapse to the same 0..3). *Verify:* `io_bus.cpp:34-48` (io_read/io_write pass
  `port` through unchanged).
- **A-3** The M12 Z80A maskable interrupt is **level-sampled**: `step()` accepts when
  `state_.maskable_interrupt_pending() && state_.iff1()` (`z80a_cpu.cpp:56`), with
  `request_maskable_interrupt()` / `clear_maskable_interrupt()` on the state
  (`z80a_state.h:91-99`). The correct model is a **held line owned by the VDP**: assert on
  raise, clear on status-read release; the CPU must **not** silently drop the line on accept.
  *Verify:* read `Z80aCpu::service_maskable_interrupt()` (`z80a_cpu.cpp:1758+`) to confirm
  whether servicing clears `maskable_interrupt_pending_`; if it does, the VDP must re-assert
  while the flag is still high (level semantics). This is **R-1** below.
- **A-4** VRAM is 128 KB fixed on HB-F1XV, no expansion socket (fact-sheet §2 line 38). *Verify:*
  size the VDP store at `128*1024`, matching the retired `kVramBytes` constant
  (`hbf1xv_machine.h:107`).
- **A-5** The debug full-state dump serializes VRAM via `serialize_region("VRAM", vram_.data(),
  vram_.size())` (`hbf1xv_machine.cpp:381`). After migration this must read the VDP's VRAM (so
  the M10 debug-dump path stays intact). *Verify:* keep a `const`-accessor on the VDP returning
  the VRAM span and repoint the dump; confirm the M10/M13 dump golden updates deterministically
  (the VRAM block previously all-zero remains all-zero at boot unless a test writes it).
- **A-6** The frame length used by the machine is `kFrameCycles = 228*262`
  (`hbf1xv_machine.cpp:14`), i.e. 262 NTSC lines × 228 Z80 T-states/line. The VDP VBlank fires
  at the start of the bottom border (line 192 or 212 per R#9 LN). *Verify:* fact-sheet §7
  lines 122-126 (262-line NTSC frame, VBlank at line 192/212); M14 models VBlank at the
  **frame-boundary** deterministically and defers the exact sub-frame VDP-cycle position to D4.

---

## 2. Spec Summary (behavior the device must implement, grounded)

### 2.1 Ports (`port & 0x03` after the isMSX1 test; V9958 uses the low 2 bits)

Grounding: `references/openmsx-21.0/src/video/VDP.cc:645-733` (writeIO) and `:988-1012`
(readIO); fact-sheet §4 lines 66-98.

| Port | `&3` | Write | Read |
|------|------|-------|------|
| `#98` | 0 | VRAM data write → store at `(R#14<<14)|ptr`, then auto-increment; clears the register-latch (`registerDataStored=false`) | VRAM data read → returns the **read-ahead buffer**, then schedules/executes the next read + increment |
| `#99` | 1 | Register/address write (two-write latch, see §2.2) | Status register read via R#15 pointer (see §2.4); also aborts an in-progress port-1 write |
| `#9A` | 2 | Palette write (two-write; byte1=`0 R2R1R0 0 B2B1B0`, byte2=`0 0 0 0 0 G2G1G0`), R#16 pointer auto-increments | (returns 0xFF) |
| `#9B` | 3 | Indirect-register data write → `changeRegister(R#17 & 0x3F, value)`, auto-increment R#17 unless AII (R#17 bit7) set | (returns 0xFF) |

`#9C-#9F` are the S1985 straight-alias mirror of `#98-#9B` (already wired, A-1/A-2); no separate
device registration.

### 2.2 `#99` two-write register/address protocol
Grounding: `VDP.cc:663-707`. First write to `#99` latches the low byte (`dataLatch`,
`registerDataStored=true`). The second write's top bits decide:
- `value & 0x80` set → **register write**: `changeRegister(value & 0x3F, dataLatch)` (mask via
  `controlRegMask`; V9958 uses the full 6-bit register number, `0x3F`).
- `value & 0x80` clear → **address setup**: `vramPointer = ((value<<8)|dataLatch) & 0x3FFF`,
  `writeAccess = value & 0x40` (bit6: 0=read → issue a **read-ahead**; 1=write). The high 3
  address bits A16–A14 come from **R#14**, not this port.

Any read from `#98/#99` sets `registerDataStored=false`, aborting a half-completed port-1 write
(`VDP.cc:998`).

### 2.3 R#17 indirect path (`#9B`)
Grounding: `VDP.cc:720-731`. `changeRegister(R#17 & 0x3F, value)`; if `(R#17 & 0x80)==0`
auto-increment `R#17 = (R#17+1) & 0x3F`. Writing R#17 *itself* indirectly is an edge case
(openMSX leaves a TODO); M14 models the documented AII path and unit-tests the auto-increment
sweep.

### 2.4 Status registers (V9958 set; feasible subset)
Grounding: `VDP.cc:903-986` (peek/read + flag reset), fact-sheet §4 lines 89-99, §7 lines 126.

| Reg | M14 model | Deferred component |
|-----|-----------|--------------------|
| S#0 | bit7 **F** (VBlank), reset on read (`VDP.cc:967`); bits6/5 (5S/C) + fifth-sprite number = idle 0 | live 5S/C → D2 |
| S#1 | reset value `0x04` (V9958 `ID#=2`, `VDP.cc:291`); bit0 **FH** from the line-IRQ line, reset on read when line-int enabled; LPS/FL read 0 (dead on 9958) | exact FH sub-frame window → D4 |
| S#2 | bits3/2 = `1` (fact-sheet notes the disagreement; follow `statusReg2` init `0x0C`, `VDP.cc:292`); EO field toggle per frame (`VDP.cc:591`); TR/CE/HR/VR/BD → feasible/idle | live TR/CE/HR/VR/BD → D3/D4 |
| S#3–S#6 | collision-X/Y registers → documented idle (`0` / `0xFE` / `0xFC` masks) | live values → D2 |
| S#7 | color register (POINT/LMCM result) → idle | → D3 |
| S#8/S#9 | border-X (SRCH) → idle (`0xFE` high mask) | → D3 |

`readStatusReg` performs the flag-reset side effects (`VDP.cc:961-986`) — M14 replicates S#0 and
S#1 resets exactly; the S#5/S#7/S#9 resets are inert until D2/D3.

### 2.5 Interrupts
Grounding: `VDP.cc:402-415` (raise), `:965-973` (clear), `:290-296` (reset), fact-sheet §7
lines 126.
- **VBlank**: at bottom-border start set `S#0 |= 0x80`; if `R#1 & 0x20` (IE0) assert the vertical
  IRQ line. Cleared by reading S#0 (`S#0 &= ~0x80` + IRQ reset).
- **Line/HBlank**: at the R#19 match line, if `R#0 & 0x10` (IE1) assert the horizontal IRQ line;
  S#1 bit0 FH reflects it; reading S#1 clears it (when enabled).
- **`/INT` = vertical OR horizontal** (open-drain wired-OR; openMSX uses two `IRQHelper`s that
  both pull the shared line). M14 drives the M12 CPU line as the OR of the two.
- Reset: `S#0=0x00`, `S#1=0x04` (V9958), `S#2=0x0C`, both IRQ lines low, palette = the V9938
  boot palette (`VDP.cc:298-304`), color-burst R#21/22 = `0x3B/0x05` (`VDP.cc:263-264`).

### 2.6 VRAM addressing
Grounding: `VDP.cc:849-857` (`executeCpuVramAccess`: `addr=(R#14<<14)|vramPointer`), fact-sheet
§2 lines 40-43.
- Effective address = `(controlRegs[14] << 14) | (vramPointer & 0x3FFF)` → 17 bits → 128 KB.
- Auto-increment: `vramPointer` increments each `#98` access; on carry out of the 14-bit field
  R#14 increments (**except** G1/G2/MC/T1 legacy modes — fact-sheet §2 line 40). M14 implements
  the full-128 KB counting path and the legacy 16 KB-wrap exception, unit-tested both ways.
- Read-ahead: an address-setup-for-read (`#99` bit6=0) pre-loads the buffer (`VDP.cc:690-693`);
  `#98` read returns the buffer then refills. `cpuVramData` is **shared** between read and write
  (`VDP.cc:789-791`: `OUT(#98)` then `IN(#98)` returns the just-written byte) — M14 replicates.
- **Planar G6/G7 interleave** (`VDP.cc:851-857`) is **deferred (D7)**: M14 keeps a flat 128 KB
  store and the linear CPU-port addressing; the interleave is only observable through the
  deferred display/command paths.

### 2.7 Mode selection + V9958 additions (bit-level identity only)
Grounding: fact-sheet §3 lines 45-64, §4 lines 68-85. Decode M1–M5 (R#1 b4,b3; R#0 b3,b2,b1) +
LN (R#9 b7) + IL (R#9 b3) + R#25 YJK (b3)/YAE (b4) into a mode enum spanning the Target-Spec set:
TEXT1 (40×24), TEXT2 (80×24), G1 (SCREEN1), G2 (SCREEN2 256×192), MULTICOLOR (64×48), G3, G4
(256×212), G5 (512×212), G6 (512×212), G7 (256×212), and the YJK/YJK+YAE identities (SCREEN
10/11/12). Store R#25 (CMD/VDS/YAE/YJK/WTE/MSK/SP2), R#26/R#27 (horizontal scroll H08–H00).
**No raster/color consequence in M14** — the enum + stored bits are the deliverable; rendering
is D1/D5/D6. Palette = 16 entries × 9-bit GRB (512-color space); the 19,268 figure is the
YJK/YAE **on-screen max**, NOT a palette dimension (Target Machine Specification;
`CLAUDE.md` COLORS row) — M14 never allocates a 19,268-entry structure.

---

## 3. src/ Placement and `vram_` Migration

Per `src/CLAUDE.md` (`src/devices/` = chip implementations; `src/machine/` = composition), the
VDP device + its VRAM live under a new **`src/devices/video/`** folder; machine wiring stays in
`src/machine/`.

### 3.1 New files (device logic — `src/devices/video/`)
- `src/devices/video/vdp_vram.h` / `.cpp` — **128 KB VRAM store** owned by the VDP. Flat
  `std::array`/vector byte buffer with `read(addr)/write(addr,v)` (17-bit address), `clear()`,
  and a `data()/size()` span for the debug dump (A-5). Behavior ref (understanding only, not
  copied): `references/openmsx-21.0/src/video/VDPVRAM.hh`.
- `src/devices/video/v9958_registers.h` / `.cpp` — the R#0–R#23/R#25–R#27 control-register file
  + pointers R#15/16/17, the two-write latch state (`dataLatch`, `registerDataStored`,
  `paletteDataStored`), `controlRegMask`, and the mode-decode helper producing the display-mode
  enum. (Optional split — the developer may fold this into `v9958_vdp.*` if it stays cohesive.)
- `src/devices/video/v9958_status.h` / `.cpp` — S#0..S#9 model + the read-side flag-reset side
  effects. (Optional split as above.)
- `src/devices/video/v9958_vdp.h` / `.cpp` — the **device**: implements `core::IoDevice`
  (`io_read`/`io_write` keyed on `port & 0x03`), owns `VdpVram` + the register file + status +
  the two IRQ line states; exposes `reset()`, a per-frame `on_vsync()` / `on_line_match()` hook,
  a `const VdpVram& vram()` accessor for the debug dump, and an IRQ-line sink (see §3.3).
- `src/devices/video/vdp_mode.h` — the display-mode enum + the M1–M5/LN/YJK decode table.

### 3.2 Machine changes (`src/machine/`)
- **Remove** `MemoryRegion vram_{kVramBytes};` from `Hbf1xvMachine` (`hbf1xv_machine.h:184`) and
  the `vram()/vram_size()` machine accessors (`.h:111/116-117`, `.cpp:318/334-340`) — or
  repoint them to `vdp_.vram()`. Remove `vram_.clear()` from `cold_boot` (`.cpp:94`); the VDP's
  `reset()` clears its own VRAM. Repoint `serialize_state_dump`'s VRAM block
  (`.cpp:381`) to `vdp_.vram().data()/size()` (A-5). `kVramBytes` moves to the VDP as its
  authoritative size (keep a re-export or update the M10/M13 dump golden deterministically).
- **Add** `devices::video::V9958Vdp vdp_;` as a machine member; wire it in `wire_bus()`:
  `io_bus_.attach(0x98,&vdp_); … attach(0x9B,&vdp_);` (the `#9C-#9F` mirrors already route, A-1).
- **Wire the IRQ seam** (§3.3) and a deterministic **VSYNC event** (§3.4).

### 3.3 CPU IRQ seam (coordinate with M12 — do not rebuild)
The VDP must drive the existing level-held CPU line without the VDP depending on the CPU class.
Introduce a tiny sink interface (placement: `src/devices/video/` next to the VDP, or reuse an
existing contract if one fits):

```
struct IrqLine { virtual void set_irq(bool asserted) = 0; };
```

The machine supplies an adapter whose `set_irq(true)` calls
`cpu_.request_maskable_interrupt()` and `set_irq(false)` calls the state's
`clear_maskable_interrupt()` (via a CPU method the developer adds if absent — a thin pass-through,
not new interrupt logic). The VDP calls `set_irq(vertical || horizontal)` whenever either line
changes. **This reuses the M12 IM1 acceptance path unchanged** (`z80a_cpu.cpp:56`); M14 adds only
the *source* of the level. See R-1 for the accept-vs-hold verification.

### 3.4 Deterministic VBlank delivery
`Hbf1xvMachine::run_frame()` (`.cpp:191-194`) ticks a whole frame at once. For a testable,
deterministic VBlank, add a per-frame VDP hook: at each frame boundary call `vdp_.on_vsync()`
(sets S#0 F + asserts vertical IRQ if IE0). Because `run_frame` advances the clock in one tick,
the VBlank becomes acceptable by the CPU on the **next** `step_cpu_instruction`. The A/B and
integration tests drive `step_cpu_instruction` in a loop so the interrupt is observed
deterministically. Exact sub-frame raster position is **deferred (D4)**; the frame-boundary
model is sufficient for the register/flag/accept contract.

---

## 4. Milestones — Slice Plan (M14-S1 … M14-S6)

The V9958 is decomposed into **six independently-verifiable slices**. Each slice: green build +
ctest + evidence gates before handoff; no slice leaves the register/VRAM/status/interrupt
contract half-wired.

### M14-S1 — VDP device skeleton + VRAM ownership migration + `#98` data path
- **Goal:** Stand up `V9958Vdp` as an `IoDevice`, migrate the 128 KB VRAM out of the machine into
  the VDP, implement `#98` VRAM data read/write with 17-bit addressing, auto-increment, and the
  read-ahead buffer (shared read/write latch).
- **Files:** `src/devices/video/vdp_vram.*`, `src/devices/video/v9958_vdp.*`;
  `src/machine/hbf1xv_machine.*` (remove `vram_`, attach VDP on `#98-#9B`, repoint debug dump).
- **Unit tests** (`tests/`): VRAM store read/write/clear over 128 KB bounds; `#98` write then
  `#98` read returns the just-written byte (shared latch, `VDP.cc:789-791`); auto-increment
  wraps the 14-bit pointer and carries into R#14; read-ahead buffer returns the pre-address byte.
- **Integration test:** CPU program `OUT (#98)` fill via the M11 `SystemBus`, then read-back
  through `#98`; assert VRAM contents deterministically.
- **Evidence gate:** validate-assets, checksum, Debug build, ctest — all green.

### M14-S2 — `#99` register/address protocol + register file + `#9B` indirect + AII
- **Goal:** Two-write `#99` latch → register-write vs address-setup; full R#0–R#23/R#25–R#27
  file with `controlRegMask=0x3F`; R#17 indirect path on `#9B` with AII auto-increment.
- **Files:** `src/devices/video/v9958_registers.*` (or folded into `v9958_vdp.*`),
  `v9958_vdp.*`.
- **Unit tests:** two-write register set (`#99` low byte then `0x80|reg`); address setup
  (`#99` low then `<0x80` with bit6 read/write) sets `vramPointer` + `writeAccess`; a `#98/#99`
  read aborts a half-written port-1 latch (`VDP.cc:998`); R#17 indirect sweep auto-increments,
  and stops when AII (bit7) set; R#14 supplies A16–A14.
- **Integration test:** CPU sets R#14 + address via `#99`, fills via `#98`, reads back — address
  spans all 128 KB.
- **Evidence gate:** all four gates green.

### M14-S3 — Palette (`#9A`) + mode-selection decode + V9958 additions
- **Goal:** `#9A` two-write palette (9-bit GRB, R#16 pointer auto-increment, boot palette from
  `VDP.cc:298-304`); decode M1–M5/LN/IL/YJK/YAE into the display-mode enum; store R#25/26/27.
- **Files:** `src/devices/video/vdp_mode.h`, `v9958_registers.*`, `v9958_vdp.*`.
- **Unit tests:** palette write pairs land at R#16 index with GRB masking (`& 0x777`,
  `VDP.cc:711`) + pointer auto-increment; boot palette equals the V9938 table; mode decode maps
  each Target-Spec bit-combo (TEXT1/2, G1–G7, MULTICOLOR, SCREEN 10/11/12) to the correct enum;
  R#25 YJK/YAE/CMD and R#26/27 h-scroll store and read back; **no** 19,268-sized structure exists.
- **Integration test:** CPU sets SCREEN-4/SCREEN-8/SCREEN-12 register sequences; assert the
  decoded mode identity + stored R#25 bits.
- **Evidence gate:** all four gates green.

### M14-S4 — Status registers S#0..S#9 + VBlank & line interrupt model + `/INT` seam
- **Goal:** S#0..S#9 feasible model (§2.4); VBlank (S#0 F / IE0) and line (S#1 FH / IE1 / R#19)
  raise+clear; the `IrqLine` seam driving the M12 CPU level (§3.3); reset values
  (S#0=`0x00`, S#1=`0x04`, S#2=`0x0C`).
- **Files:** `src/devices/video/v9958_status.*` (or folded), `v9958_vdp.*`;
  `src/machine/hbf1xv_machine.*` (IRQ adapter + `on_vsync` hook).
- **Unit tests:** reading S#0 returns then clears F + drops the vertical line; S#1 reset value +
  FH set/clear when IE1; LPS/FL read 0; S#1 ID bits = 2 (`0x04`); S#2 bits3/2 = 1 + EO toggle;
  `/INT` = vertical OR horizontal; R#1 IE0 gating and R#0 IE1 gating.
- **Integration test:** CPU enables IE0 (R#1 bit5) + IM1 (already set at boot,
  `hbf1xv_machine.cpp:117`); machine drives `on_vsync`; CPU `step` **accepts** the maskable
  interrupt (PC → `0x0038`), then S#0 read clears F and the line — verifying the M12 seam
  end-to-end (R-1).
- **Evidence gate:** all four gates green.

### M14-S5 — System integration + per-frame VSYNC + mirror-port equivalence
- **Goal:** Full wiring: VDP on the M11 `IoBus`; per-frame VSYNC delivery in `run_frame`;
  mirror-port equivalence (`#9C-#9F` ≡ `#98-#9B`); debug-dump VRAM repointed (A-5). Prior suites
  green.
- **Files:** `src/machine/hbf1xv_machine.*`; test files under `tests/`.
- **Unit/integration tests:** `debug_io_write/read` on `#9C-#9F` behaves identically to
  `#98-#9B` (mirror A-1/A-2); a multi-frame run raises N VBlank flags deterministically; the
  M10/M13 debug-state dump includes the VDP's VRAM with a deterministic (updated) golden; **no
  CPU-addressable VRAM region remains** (attempting `read_memory` over a VRAM address hits DRAM,
  not VRAM — the machine no longer exposes a VRAM memory window).
- **Evidence gate:** all four gates green; **zero regression** across M0–M13 suites.

### M14-S6 — openMSX A/B trace-diff parity (VDP port/VRAM/status/interrupt)
- **Goal:** Produce a **genuine captured** A/B trace-diff vs openMSX `Sony_HB-F1XV` over a
  CPU→VDP sequence (see §7), including a **VRAM read-back** comparison (VRAM is now comparable —
  it was excluded in M13). No parity claim without a real capture.
- **Files:** a parity harness under `tools/` (extend `tools/openmsx-io-parity.ps1` /
  `openmsx-ab-smoke.ps1` or add `tools/openmsx-vdp-parity.ps1`); report `docs/m14-parity-trace-diff.md`.
- **Tests/evidence:** the harness runs the same program on our machine (headless, trace + VRAM
  dump) and on `/usr/bin/openmsx` (WSL) with the real V9958, diffs the register/status/VRAM
  trace; the report embeds the actual diff (empty = pass) + an adversarial comparator check
  (corrupt one field → DIVERGENCE, as the M11–M13 harnesses did).
- **Evidence gate:** all four gates green + the A/B report.

---

## 5. Acceptance Criteria (M14 exit)

Traces the milestone-record exit criteria (`state/milestones.md` M14) and the evidence gates.

1. **VRAM ownership** — the V9958 device owns the 128 KB VRAM; `vram_` is gone from
   `Hbf1xvMachine`; CPU VRAM access flows **only** through `#98/#99` (+ `#9C/#9D` mirror); no
   CPU-addressable VRAM region remains in the machine. *(S1, S5)*
2. **Port/register contract** — `#98` (auto-increment r/w + read-ahead), `#99` (two-write
   register set + address setup + status read), `#9A` (palette), `#9B` (R#17 indirect + AII)
   implemented and unit-verified against the fact-sheet + cited `VDP.cc` lines. *(S1–S4)*
3. **VRAM addressing** — 17-bit `(R#14<<14)|ptr`, auto-increment with R#14 carry, legacy-mode
   wrap exception, read-ahead + shared read/write latch — unit-verified. *(S1, S2)*
4. **Status + interrupts** — S#0..S#9 at the feasible contract level; VBlank (S#0 F / IE0) and
   line (S#1 FH / IE1 / R#19) raise+clear; level-held `/INT` drives the M12 CPU IM1 path and is
   released on status read; reset values correct (S#1 ID=2). CPU **accepts** a VBlank interrupt
   in an integration test. *(S4, S5)*
5. **Modes** — Target-Spec modes decoded to a mode identity; palette = 16-of-512 (9-bit GRB);
   R#25/26/27 stored; **19,268 is treated as a YJK/YAE on-screen max, never a palette
   dimension**. *(S3)*
6. **System integration** — VDP on the M11 `IoBus`; mirror-port equivalence; per-frame VSYNC;
   all prior M0–M13 suites remain green (zero regression). *(S5)*
7. **Evidence gates** executed and captured every cycle: `tools/validate-assets.ps1`,
   `tools/checksum-assets.ps1 -OutFile docs/asset-checksums.txt`,
   `cmake --build build --config Debug`,
   `ctest --test-dir build -C Debug --output-on-failure`.
8. **A/B parity** — a **genuine** openMSX `Sony_HB-F1XV` trace-diff over the §7 sequence,
   including VRAM read-back, captured in `docs/m14-parity-trace-diff.md` with an adversarial
   comparator check. No parity claim without a real capture. *(S6)*
9. **Backlog** — B9 marked DONE (M14) at closure; D1–D7 recorded as OPEN (done in this package).
10. **QA sign-off** + the normal **human release decision** (no auto-close grant for M14).

---

## 6. In-Scope vs Deferred Boundary (CRITICAL) + Justification

**Principle.** M14 makes the V9958 *register / VRAM / status / interrupt CONTRACT* correct and
parity-verified — the externally observable device behavior a program drives through ports and
observes through status/VRAM/interrupts — **without committing to a pixel pipeline**. This is the
boundary that is verifiable by deterministic unit tests + an A/B trace-diff of comparable
architectural state (ports, registers, status, VRAM bytes, IRQ acceptance), exactly as M10–M13
established for the CPU/bus/memory. Pixel/sprite/command/slot-timing depth is (a) *very large*,
(b) only observable through a rendering surface or cycle-accurate clock the project has not yet
built, and (c) independently verifiable later against openMSX frame captures / timing suites — so
folding it into M14 would make the milestone unslicing-ly large and its acceptance unverifiable by
the current headless harness.

**In scope (M14):** everything in §1.1 — the register file, the four ports + auto-increment +
read-ahead, 128 KB VRAM ownership + addressing, S#0..S#9 (feasible), VBlank + line interrupts at
frame/line-count granularity, mode-bit decode + stored V9958 feature bits, and system
integration.

**Deferred (recorded as backlog rows D1–D7 in `agent-protocol/state/deferred-backlog.md`, added
this cycle):**

| ID | Deferred depth | Why out of M14 | Verifiable later by |
|----|----------------|----------------|---------------------|
| D1 | Pixel-accurate raster rendering (all modes) | No framebuffer/renderer surface exists; enormous per-mode scope | openMSX frame captures (video milestone) |
| D2 | Sprite rendering + collision / 5th-sprite (S#0 5S/C, S#3–S#6) | Needs the raster path (D1) + `SpriteChecker` model | frame + collision-flag A/B |
| D3 | VDP command engine R#32–R#46 (+ S#2 TR/CE, S#7, S#8/9) | Large state machine; needs slot timing (D4) for fidelity | command trace / timing suites |
| D4 | Cycle-accurate access-slot + command timing; exact sub-frame IRQ position | Needs a 1368-cycle/line VDP clock the project lacks; measured logic-analyzer model | timing-fidelity milestone (with C1) |
| D5 | YJK/YAE color decode + 15-bit DAC + 3→5-bit palette expansion | Produces *color*, only meaningful with D1 | SCREEN 12 image A/B |
| D6 | H-scroll / interlace / blink / superimpose *visual* effects | Register bits stored in M14; effects are rendering-time | video milestone |
| D7 | G6/G7 planar VRAM interleave in display/command path | Only observable through D1/D3; M14 keeps a flat store + linear CPU addressing | video/command milestone |

Each row cites `references/fact-sheets/Yamaha V9958 VDP.md` and the relevant
`references/openmsx-21.0/src/video/` path (see the backlog file). None is a waiver — each is
committed scope sequenced to a later milestone per DEC-0005.

---

## 7. A/B Acceptance Test (openMSX trace-diff, real capture required)

- **Reference machine:** openMSX `Sony_HB-F1XV` on WSL `/usr/bin/openmsx` with the real V9958
  (`references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml`). Behavior reference only.
- **Program (CPU → VDP over the M11 bus):**
  1. Set several control registers via `#99` two-write (e.g. R#0, R#1 with IE0, R#7, R#19, R#25).
  2. Set VRAM write address via `#99` (R#14 + address, bit6=1) and **fill a VRAM block** via
     `#98` auto-increment (e.g. 256 bytes of a known ramp).
  3. **Palette writes** via `#9A` (a few GRB pairs) and an R#17 **indirect** register write via
     `#9B`.
  4. Set a read address via `#99` (bit6=0) and **read the block back** via `#98` (exercises the
     read-ahead + increment).
  5. Enable IE0, run to a **VBlank**, let the CPU **accept the maskable interrupt** (IM1 → 0x0038),
     then **read S#0** to clear F and observe the line release.
- **Compared state (architectural, deterministic):** the control-register file, the R#15/16/17
  pointers, S#0/S#1/S#2 status reads at defined checkpoints, the **VRAM bytes** written/read
  (VRAM is now comparable — it was excluded in M13's diff), the palette entries, and the fact of
  interrupt acceptance (PC path). **No pixel/color/sprite/command state** is compared (deferred).
- **Method:** our headless run emits a trace + VRAM dump; the openMSX run is driven to the same
  checkpoints; the harness diffs. **Empty diff = pass.** Include an adversarial check (corrupt one
  field → DIVERGENCE) to prove the comparator, as M11–M13 did. Report: `docs/m14-parity-trace-diff.md`.
- **No parity claim without a genuine captured diff** (guardrails; `state/current-phase.md`
  watch-item).

---

## 8. Risks and Assumptions (each with a verification action)

- **R-1 — Interrupt level vs edge / accept-clear.** If `Z80aCpu::service_maskable_interrupt()`
  clears `maskable_interrupt_pending_` on accept, a naive edge model would lose the held `/INT`.
  Real hardware holds `/INT` until the S#0 read. *Mitigation/verify:* read `z80a_cpu.cpp:1758+`
  and `z80a_state.h`; model the VDP as the **owner** of the line (`set_irq(vertical||horizontal)`
  on every change; release only on status read). Add the S4 integration test that accepts the
  interrupt **and** confirms a second acceptance does **not** occur until re-raised. Coordinate
  with M12 — do not modify the IM1 acceptance timing (14T, proven in M12).
- **R-2 — VBlank sub-frame timing.** `run_frame` ticks a whole frame atomically, so the exact
  VDP-cycle of VBlank is not modeled. *Verify:* fact-sheet §7 (VBlank at line 192/212). M14 uses
  frame-boundary delivery and **explicitly defers** exact position to D4; document that
  raster-split effects are not yet supported. Ensure the A/B checkpoints are frame-aligned so the
  diff is deterministic despite the coarse timing.
- **R-3 — Debug-dump golden churn (A-5).** Repointing the VRAM block from `vram_` to
  `vdp_.vram()` must keep the M10/M13 debug-dump byte-deterministic. *Verify:* the VDP clears VRAM
  to 0 at reset (matching the old `vram_.clear()`), so the boot dump's VRAM block is unchanged;
  re-run the dump golden test and update only if a *new* test writes VRAM.
- **R-4 — Mirror routing regression.** The VDP must be reachable on `#9C-#9F`. *Verify:* the
  mirror is pre-wired (A-1); S5 adds an explicit `#9C-#9F` ≡ `#98-#9B` equivalence test. Confirm
  the VDP decodes `port & 0x03` (A-2) so the mirror ports map to the same 4 functions.
- **R-5 — S#2 undocumented bits 2/3.** Community docs disagree (1 per MSX wiki, 0 per others;
  fact-sheet §4 line 99, Caveats line 194). *Verify:* follow openMSX `statusReg2` init `0x0C`
  (`VDP.cc:292`) — bits 3,2 = 1 — for A/B parity with the reference; note the disagreement.
- **R-6 — R#17 self-write edge case.** Writing R#17 indirectly via `#9B` is a documented TODO in
  openMSX (`VDP.cc:722`). *Verify:* M14 implements the documented AII auto-increment path and
  unit-tests it; the self-write edge is left as reference-matching (whatever openMSX does) and
  noted, not independently invented.
- **R-7 — 128 KB size authority.** `kVramBytes` moves from the machine to the VDP. *Verify:*
  fact-sheet §2 line 38 (128 KB fixed, HB-F1XV); keep the constant + a size assertion; no
  expansion RAM (R#45 bit6 / 192 KB path) — that is not populated on HB-F1XV.
- **R-8 — Legacy-mode auto-increment exception.** R#14 does not carry-increment in G1/G2/MC/T1
  (fact-sheet §2 line 40). *Verify:* unit-test both the new-mode full-128 KB counting and the
  legacy 16 KB wrap; ground against the fact-sheet (openMSX handles this in the display/command
  addressing, deferred here — M14 tests the CPU-port auto-increment behavior only).
- **A-1…A-6** — listed in §1.3, each with its own verification action.

---

## 9. Developer Handoff

- **Build M14 as six slices S1→S6** (§4); run all four evidence gates green before each handoff;
  **zero regression** across M0–M13 is a standing condition.
- **Placement:** new device folder `src/devices/video/` (`vdp_vram.*`, `v9958_vdp.*`, and the
  optional `v9958_registers.*` / `v9958_status.*` / `vdp_mode.h`); machine wiring only in
  `src/machine/hbf1xv_machine.*`. Do **not** put device logic in `core/` or machine wiring in
  `devices/` (`src/CLAUDE.md`).
- **Migrate `vram_` first (S1):** remove it from the machine, give the VDP the 128 KB store,
  repoint the debug dump (A-5), and confirm no CPU-addressable VRAM region survives.
- **Reuse, don't rebuild, the M11 seams:** attach on `#98-#9B` (mirrors pre-wired, A-1/A-2);
  decode `port & 0x03`. **Reuse the M12 IM1 path** via the `IrqLine` adapter (§3.3, R-1) — add
  only a thin `clear_maskable_interrupt` pass-through on the CPU if one is not already exposed;
  do not touch IM1 acceptance timing.
- **Ground every behavior** in `references/fact-sheets/Yamaha V9958 VDP.md` and the cited
  `references/openmsx-21.0/src/video/VDP.cc` lines; **never copy** openMSX code (GPL isolation).
- **A/B (S6):** produce a **genuine** capture (§7) with a VRAM read-back and an adversarial
  comparator check → `docs/m14-parity-trace-diff.md`. No parity claim without a real diff.
- **Backlog:** D1–D7 are already recorded OPEN. At M14 closure the coordinator marks B9
  `DONE (M14)`.
- **Reference:** this package `docs/m14-planner-package.md`; milestone record
  `agent-protocol/state/milestones.md` (M14); phase `agent-protocol/state/current-phase.md`.
