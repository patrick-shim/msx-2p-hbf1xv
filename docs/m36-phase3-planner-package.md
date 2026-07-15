# M36 Phase 3 Planner Package — Comprehensive Debug Snapshot (DEC-0051)

- Spec Owner: MSX Planner Agent
- Milestone: M36 Phase 3 (debug snapshot tooling; sibling of Phase 1 playtest harness)
- Authorizing decision: DEC-0051 (`agent-protocol/channels/decisions.md:612-619`, human-authorized
  2026-07-10), building on the DEC-0050 FM-PAC/MSX-MUSIC architecture
  (`agent-protocol/channels/decisions.md:567-608`).
- Tag target: v1.0.37 (alongside the rest of M36) — see §7 Risks, "Tag target recommendation".
- Status: PLANNING ONLY. No production code in this cycle.

This package is grounded by direct reads of the cited `src/...`, `agent-protocol/...`, and
`.gitignore` files. Every "state lives here" / "getter needed here" claim cites the concrete file
(and line where it pins down a private member). No reference (`references/openmsx-21.0/`,
`references/fmsx-60/`, `references/sdl3/`) code is copied into `src/`; behavior grounding for the
device state itself was already established in the milestone that built each device (M9-M35) and is
not re-litigated here — Phase 3 only *reads* already-modeled state.

---

## 1. Scope and Assumptions

### 1.1 Scope (from DEC-0051)

A NEW, comprehensive, live-triggerable **debug snapshot** capability that captures the EXACT current
state of EVERY machine component into `debug/snapshot/<id>/` (a typed dump per component).

- **F12** in the SDL3 frontend triggers a capture of the running machine.
- **`--snapshot <dir>`** CLI option (both executables) enables the capture and controls the output
  directory (headless trigger + output-dir control), mirroring the M27 `--dump-state` /
  `--debug-session` precedent (`src/main.cpp:732-861`, `src/frontend/sdl3_cli.h:56-67`).
- **CAPTURE-ONLY** (inspection) this cycle. NO reload/resume. The serialization format is designed
  **RESTORE-READY** so a future save-state can layer on without a rewrite (§3.3).
- **COMPREHENSIVE** — well beyond the existing CPU/DRAM/SRAM/VRAM `serialize_state_dump()`
  (`src/machine/hbf1xv_machine.cpp:1153-1177`). Every component in §2.3.
- **ADDITIVE + read-only w.r.t. emulation** — no behavior change, no `src/devices/cpu/` or
  `src/core/` timing edits; ZEXALL stays withheld (`feedback_slow_test_cadence`).
- **DETERMINISTIC content** — an identical run to the same frame/cycle produces byte-identical
  snapshot CONTENT (auditable). The snapshot writer consumes NO RNG and NO wall-clock for content.

### 1.2 Assumptions (each with a verification action)

- **Assumption A1**: F12 is unbound in both the SDL3 frontend and the MSX keyboard matrix, so it can
  be claimed as a host hotkey without colliding with F11 (disk-swap). VERIFIED: the only VDP/host
  hotkey wired is F11 (`src/frontend/sdl3_app.cpp:253-259`, `SDL_SCANCODE_F11`); a repo-wide grep for
  `F12`/`SDL_SCANCODE_F12` returns zero hits. Developer re-confirms with a grep before wiring.
- **Assumption A2**: the existing `serialize_state_dump()` text format is golden-locked by the
  M10/M13/M14 dump-golden tests (referenced as "debug_dump A-5 golden" in DEC-0016 and the M13/M14
  closure records). Phase 3 therefore introduces a SEPARATE snapshot serializer and MUST NOT modify
  `debug_dump::serialize_cpu()` / `serialize_region()` in place (that would re-baseline a signed
  golden — out of scope). VERIFY: `ctest -R dump` (or the debug-dump golden test) stays green
  byte-for-byte after Phase 3.
- **Assumption A3**: `.gitignore` swallows generated `debug/**` content by extension
  (`.txt/.log/.bin/.dump/...`, `.gitignore:33-43`) but NOT arbitrary-extension files (e.g. a
  `manifest.json` with no matched suffix). VERIFY + ACTION: S5 adds a `/debug/snapshot/` blanket
  ignore line (mirroring `/debug/playtest_*/` at `.gitignore:46`) so no snapshot artifact is ever
  accidentally tracked.
- **Assumption A4**: the "inserted-disk INDEX" in DEC-0051 is a FRONTEND concept — it lives in
  `Sdl3App::current_disk_index_` (`src/frontend/sdl3_app.h:222`, exposed via `current_disk_index()`
  at `:171`) and in the headless `run_debug_session` local, NOT in the machine's `DiskImage`. The
  machine-level snapshot captures drive/image state; the multi-disk list index is added to the
  manifest by the CALLER. VERIFY: the manifest carries a `disk_index`/`disk_count` line from the
  frontend, not from `Hbf1xvMachine`.
- **Assumption A5**: `debug_io_read(port)` and `debug_bus_read(address)` are genuinely non-perturbing
  (`src/machine/hbf1xv_machine.h:122-130`, "do not advance the CPU or the clock"). The snapshot may
  use them to read #F4/#F5/#A8 without a new machine accessor. VERIFY: developer confirms `#F4`/`#F5`
  reads are side-effect-free in `reset_status_register.cpp` / `system_control.cpp` (both return a
  stored latch; §2.3 rows).

### 1.3 Non-goals (explicit)

- NO restore/resume/load-state (deferred; the FORMAT is restore-ready but no reader is built).
- NO change to emulation behavior, timing, or any `src/devices/cpu/*` or `src/core/*` file.
- NO new clock consumer wired into `step_cpu_instruction()`/`run_cycles()`/`run_frame()`.
- NO PNG/audio decode inside the snapshot (VRAM is dumped as raw bytes; the existing
  `write_frame_dump()` and `tools/frame-to-png.py` remain the decoded-frame path — a snapshot MAY
  additionally emit one decoded frame dump via the existing `serialize_frame_dump()`, but computing
  pixels is not the snapshot's job).
- NO backlog row closed (Phase 3 is tooling/infrastructure, exactly like M27 which "closes zero
  backlog rows"). Deferred-backlog rows remain as summarized in `state/current-phase.md`
  (C1/D4, E3, E4, D8/D9/D10, C10/F1/F2, G3/G4/G5/G6) — none relate to snapshotting.

---

## 2. Spec Summary

### 2.1 Existing foundation to REUSE (do not reinvent)

| Foundation | Where | What it already does | Phase 3 reuse |
| --- | --- | --- | --- |
| `serialize_state_dump()` | `src/machine/hbf1xv_machine.cpp:1153-1177` | CPU + DRAM + SRAM(FM-PAC) + VRAM ASCII dump, `[END]`-framed | Model + partial content source; snapshot SUPERSETS it (do NOT edit it) |
| `write_state_dump()` | `hbf1xv_machine.cpp:1198-1205` + `write_text_file()` `:1179-1196` | dir-create + binary-mode `\n`-verbatim write under `debug_root_/traces/` | Reuse `write_text_file()` verbatim; snapshot writes under `debug_root_/snapshot/<id>/` |
| `debug_dump::serialize_cpu/serialize_region` | `src/machine/debug_dump.{h,cpp}` | fixed-width hex, folded 16-byte region dump, deterministic | Reuse `serialize_region()` for byte buffers; ADD a fuller CPU serializer (§2.4) — do NOT edit `serialize_cpu` |
| `debug_format` hex/dec helpers | `src/machine/debug_format.h` (used by debug_dump.cpp:22-24) | `to_hex`/`to_dec`/`flag_string`, locale-free | Reuse verbatim for all new typed fields |
| `frame_dump::serialize_frame_dump` | `src/machine/frame_dump.{h,cpp}` | deterministic decoded-FrameBuffer dump | Optional extra file in the snapshot dir |
| `--debug-session` CLI mode | `src/main.cpp:732-1030` | additive headless mode: `--dump-state`/`--dump-frame`/`--frames`/`--swap-disk-frame`/`--fmpac-sram` parse + end-of-run write-out | Exact pattern to mirror for `--snapshot` |
| SDL3 `--dump-state`/F11 wiring | `src/frontend/sdl3_app.cpp:207-263`, `sdl3_cli.h:56-67`, `sdl3_app.h:33-103` | end-of-run `flush_debug_session_outputs()`; F11 keydown handler `on_disk_swap_hotkey()` | Exact pattern to mirror for `--snapshot` + F12 |
| `debug_root_` config | `hbf1xv_machine.h:379-381`, default `"debug"` | configurable base dir | Snapshot root = `debug_root_ / "snapshot"` |

**What the existing state-dump already captures** (baseline): only 4 sections — `[CPU]` (PC/SP/AF/BC/
DE/HL + shadows/IX/IY/I/R/IFF1/IFF2/IM/HALT/TSTATES, but NOT WZ or Q), `[DRAM]` 64 KB, `[SRAM]`
(inserted FM-PAC's 8 KB or empty), `[VRAM]` 128 KB. It captures NONE of: VDP registers/status/latch/
palette/command/sprite, PSG, YM2413, RTC, FDC, drive/disk, S1985 backup RAM, slot/subslot selection,
cartridge mapper banks, Halnote, pause/speed/Ren-Sha, or the scheduler/clock counters. Phase 3 adds
all of those.

### 2.2 What Phase 3 adds (delta)

1. A new machine-level `serialize_snapshot()` / `write_snapshot(id)` that produces a MANIFEST plus a
   comprehensive per-component set of typed sections (§3), reading only already-modeled state.
2. A small set of **additive const getters** on device headers where a required internal field is not
   yet exposed (§2.4). Read-only, zero logic change.
3. F12 (SDL3) + `--snapshot` (both executables) triggers (§4).

### 2.3 System-wide component/state inventory (THE CORE)

Legend for "Access": **HAVE** = a const getter already exists; **NEW** = an additive const getter is
required (listed in §2.4); **SEAM** = reachable through an existing non-perturbing machine seam
(`debug_io_read`/`debug_bus_read`/`debug_sub_slot_register`/`slot_expanded`) with no new getter.

Machine-level accessors used below are declared in `src/machine/hbf1xv_machine.h`.

| Component | State fields | Access (getter / seam / NEW at file:line) | Dump representation |
| --- | --- | --- | --- |
| **CPU Z80A** (`devices/cpu`) | PC,SP,AF,BC,DE,HL,AF',BC',DE',HL',IX,IY,I,R | HAVE `cpu().state().regs()` (`z80a_state.h:26-40` public members) | `[CPU]` typed fields (hex) |
| | **WZ/MEMPTR**, **Q latch** | HAVE (public) `regs().wz` (`z80a_state.h:46`), `regs().q` (`:53`) — but NOT emitted by existing `serialize_cpu` | NEW snapshot CPU serializer emits `WZ=`,`Q=` (§2.4 item 1) |
| | IFF1,IFF2,IM,HALT | HAVE `state().iff1()/iff2()/interrupt_mode()/halted()` (`z80a_state.h:90-99`) | typed fields |
| | ei_delay, maskable/nmi pending, interrupt bus vector + supplied flag | HAVE `state().ei_delay_active()/maskable_interrupt_pending()/nmi_pending()/interrupt_bus_vector()/interrupt_vector_supplied()` (`z80a_state.h:101-119`) | typed fields (restore-ready) |
| | total T-states | HAVE `state().total_tstates()` (`z80a_state.h:121`) | typed field |
| **Memory-mapper RAM** (`devices/memory`) | 64 KB main RAM | HAVE `dram()` (`hbf1xv_machine.h:195-196`) | `[DRAM]` folded hex region |
| | 4 segment regs #FC-#FF | NEW machine accessor `mapper_io()` -> `MapperIo::segment(page)` (`mapper_io.h:46`); or SEAM `debug_io_read(0xFC..0xFF)` gives the `0x80\|seg&0x1F` READBACK form only | `[MAPPER]` seg0..seg3 (raw + readback) |
| **VRAM** (V9958) | 128 KB | HAVE `vdp().vram().data()/size()` (`v9958_vdp.h:135`, `vdp_vram.h:50-67`) | `[VRAM]` folded hex region |
| **V9958 VDP** (`devices/video`) | control regs R#0-31 | HAVE `vdp().control_register(i)` (`v9958_vdp.h:137`) | `[VDP.REGS]` R00..R1F |
| | command regs R#32-46 | HAVE `vdp().cmd_engine().read_register(i)` (`vdp_command_engine.h:72`) | `[VDP.CMDREGS]` R32..R46 |
| | status regs S#0-9 | HAVE `vdp().peek_status_register(i)` (`v9958_vdp.h:140`) | `[VDP.STATUS]` S0..S9 |
| | address latch / two-write protocol (`data_latch_`,`register_data_stored_`,`palette_data_stored_`) | NEW getters (`v9958_vdp.h:205-207`) | `[VDP.LATCH]` typed |
| | VRAM pointer/address; read-ahead (`write_access_`,`cpu_vram_data_`) | HAVE `vram_pointer()`,`vram_address()` (`:141-142`); NEW getters for `write_access_`,`cpu_vram_data_` (`:213-214`) | `[VDP.VRAMPTR]` typed |
| | palette (16 x 9-bit GRB) | HAVE `vdp().palette_entry(i)` (`:143`) | `[VDP.PALETTE]` 16 entries |
| | mode identity | HAVE `vdp().mode()` (`:144`, `VdpModeState` `vdp_mode.h:56-63`) | `[VDP.MODE]` mode/base/yjk/yae |
| | internal IRQ + field (`status_reg0_`,`eo_field_`,`irq_vertical_`,`irq_horizontal_`,`irq_level_`) | HAVE `irq_active()` (combined, `:132`); NEW getters for the individual booleans (`:217-221`) | `[VDP.IRQ]` typed (restore-ready) |
| | blink (`blink_state_`,`blink_countdown_`) | HAVE `blink_state()` (`:153`); NEW getter for `blink_countdown_` (`:237`) | `[VDP.BLINK]` typed |
| | raster clock position | HAVE `vdp().raster_display_line()` (`:174`) + machine `cycles_since_last_vsync()`/`vdp_cycle_position()` (`hbf1xv_machine.h:155-156`) | `[VDP.RASTER]` typed |
| | sprite status (S#0 5S/C, collision X/Y) | HAVE `vdp().sprite_engine().status_bits()/collision_x()/collision_y()` (`sprite_engine.h:78-80`); also via S#0/S#3-6 | `[VDP.SPRITE]` typed |
| | command-engine live status + transfer FSM (`status_`,`scr_mode_`,`transfer_*`) | HAVE `cmd_engine().ce()/bd()/tr()/color()/border_x()` (`vdp_command_engine.h:79-94`); NEW getters for `status_`,`scr_mode_`,`transfer_pending_`,`transfer_kind_` etc. (`:147-168`) | `[VDP.CMD]` typed (restore-ready) |
| **PSG / YM2149** (`devices/audio`) | 16 registers, selected register | HAVE `psg().register_value(r)` (`psg_ym2149.h:85`), `selected_register()` (`:82`) | `[PSG.REGS]` R0..R15 + latch |
| | generator internals (tone count/output, noise count/lfsr/output, envelope count/step/attack/hold flags, `cycle_residual_`, integrals) | HAVE derived views `tone_period/noise_period/envelope_period/envelope_volume/channel_amplitude/channel_audible` (`:178-183`); NEW getters for raw counters/LFSR/residual for restore (`:208-241`) | `[PSG.GEN]` typed |
| **YM2413 OPLL (built-in)** | 64 registers, latch | HAVE `ym2413().register_value(a)` (`ym2413_opll.h:147`); NEW getter for `latch_` (`:244`) | `[OPLL.REGS]` + latch |
| | write-timing gate state | HAVE `write_timing_enforced()` (`:139`); NEW getters for `has_last_write_`,`last_write_was_address_`,`last_write_cycle_` (`:252-254`) | `[OPLL.TIMING]` typed |
| | synth engine (EG/phase) | HAVE `synth()` (`:163`); synth internals may need getters (`ym2413_synth.h`, verify) for full restore | `[OPLL.SYNTH]` typed (best-effort) |
| **FM-PAC OPLL (inserted cart)** | its own YM2413 | HAVE `fmpac(slot)->opll()` (`cartridge_fmpac_rom.h:100-101`) — same rows as built-in OPLL | `[FMPAC.OPLL]` (only if inserted) |
| **FM-PAC mapper** (`devices/cartridge`) | 8 KB battery SRAM | HAVE `fmpac(slot)->sram()` (`cartridge_fmpac_rom.h:94-95`) | `[FMPAC.SRAM]` folded hex |
| | bank reg, enable reg, sram_enabled | HAVE `fmpac(slot)->bank()/enable_register()/sram_enabled()` (`:104-106`) | `[FMPAC.STATE]` typed |
| | **unlock latch** (`r1ffe_`,`r1fff_`) | NEW getters (`cartridge_fmpac_rom.h:119-120`) | `[FMPAC.STATE]` r1ffe/r1fff |
| **RTC RP5C01** (`devices/rtc`) | 4 blocks x 13 regs; address latch; mode reg | HAVE `rtc().peek_register(b,r)` (`rp5c01.h:80`), `address_latch()` (`:79`), `mode_register()` (`:81`) | `[RTC.REGS]` 52 nibbles + latch |
| | test/reset regs; decoded time (frac/sec/min/hr/dow/day/mon/yr/leap); `last_rtc_ticks_` | NEW getters (`rp5c01.h:94-108`) | `[RTC.TIME]` typed (restore-ready) |
| **FDC WD2793** (`devices/fdc`) | status, track, sector, command regs | HAVE `fdc().peek_status()/track_register()/sector_register()/command_register()` (`wd2793.h:94-97`) | `[FDC.REGS]` typed |
| | diagnostic counters | HAVE `read_sector_commands_accepted()/..._bytes_transferred()/..._completions_ok()` (`wd2793.h:104-112`) | `[FDC.DIAG]` typed |
| | `data_reg_`, `phase_`, direction, INTRQ/DRQ pending, index-IRQ arm, HLD, timing deadlines, transfer buffer + write-track FSM | NEW getters (`wd2793.h:149-197`) | `[FDC.FSM]` typed (restore-ready) |
| **Disk drive** | physical track, side, available, disk-changed, motor, ready, track00, write-protect | HAVE `disk_drive().physical_track()/side()/available()/disk_changed()` (`disk_drive.h:64/54/50/82`); `ready()/is_track00()/write_protected()` (`:68-70`); `motor_on(now)` (`:58`) | `[DRIVE]` typed |
| | motor-off timer (`motor_off_pending_`,`motor_off_deadline_`) | NEW getters (`disk_drive.h:95-96`) for restore | `[DRIVE]` typed |
| **Disk image** | size, present, write-protect, dirty, host path, content | HAVE `disk_image().size()/present()/write_protected()/dirty()/host_path()/data()` (`disk_image.h:74-115`) | `[DISK]` typed + optional content hash |
| | **inserted-disk INDEX** | FRONTEND: `Sdl3App::current_disk_index()` (`sdl3_app.h:171`); headless local (A4) | manifest field (added by caller) |
| **S1985 backup RAM** (`devices/chipset`) | 16-byte backup RAM | HAVE `s1985().backup_byte(i)` (`s1985_engine.h:66`) | `[S1985.SRAM]` 16 bytes |
| | address/pattern/color1/color2 regs | NEW getters (`s1985_engine.h:83-86`) for restore | `[S1985.REGS]` typed |
| **Slot / subslot** (`devices/chipset`) | #A8 primary select | SEAM `debug_io_read(0xA8)` (PPI); machine `ppi()` (`hbf1xv_machine.h:241-242`) | `[SLOTS]` A8=hex |
| | secondary #FFFF sub-slot regs (all 4 primaries) | HAVE `debug_sub_slot_register(primary)` (`hbf1xv_machine.h:137`) | `[SLOTS]` sub0..sub3 |
| | expanded flags | HAVE `slot_expanded(primary)` (`hbf1xv_machine.h:131`) | `[SLOTS]` exp0..exp3 |
| **Cartridge mapper (slots 1&2)** | loaded?, mapper type | HAVE `cartridge_slot1()/2().loaded()/mapper_type()` (`cartridge_slot.h:64-66`) | `[CART1]`/`[CART2]` typed |
| | bank state (Generic/ASCII/Konami/KonamiSCC) | via `CartridgeRomWindow::slot_bank/slot_mapped/num_blocks/block_mask` (`cartridge_rom_window.h:85-91`) — NOT exposed on the base; NEW virtual `rom_window()` on `CartridgeMapperDevice` (`cartridge_mapper_device.h:28`) OR mapper-type dispatch | `[CARTn.BANKS]` 8 window-slots |
| | SCC chip (if KonamiSCC) | HAVE `scc_chip(slot)` (`hbf1xv_machine.h:314-315`) | `[CARTn.SCC]` (verify getters in `scc_wavetable.h`) |
| **Halnote / MSX-JE** (`devices/halnote`) | 1 MB window banks, 16 KB SRAM, sram_enabled, sub_mapper_enabled, sub-banks | HAVE `halnote().window()/sram()/sram_enabled()/sub_mapper_enabled()/sub_bank(i)` (`halnote_rom.h:105-117`) | `[HALNOTE]` typed + `[HALNOTE.SRAM]` 16 KB |
| **Pause / Speed** (`devices/chipset`) | button engaged, speed level, paused-this-frame, combined gate | HAVE `pause_controller().button_engaged()/speed_level()/speed_controller_paused_this_frame()/cpu_should_pause()` (`mb670836_pause.h:72-96`) | `[PAUSE]` typed |
| | `frame_index_` | NEW getter (`mb670836_pause.h:102`) for restore | `[PAUSE]` typed |
| **Ren-Sha Turbo** (`peripherals`) | speed, live signal | HAVE `rensha_turbo().speed()/signal()` (`rensha_turbo.h:98/103`) | `[RENSHA]` typed |
| **System control #F5 / reset status #F4** | #F5 CLOCK-IC latch; #F4 boot-logo latch | SEAM `debug_io_read(0xF5)`/`debug_io_read(0xF4)` (pure reads — `system_control.h:44-50`, `reset_status_register.h:58-61`) | `[SYSCTRL]` F4/F5 hex |
| **Scheduler / clock** (`core`) | total cycles; frame count; cycles/frame; cycles-since-vsync; vdp cycle pos | HAVE `elapsed_cycles()/frame_count()/frame_cycles_per_frame()/cycles_since_last_vsync()/vdp_cycle_position()` (`hbf1xv_machine.h:140-156`) | `[CLOCK]` typed |
| **Kanji / printer / cassette** (low priority; "every component") | Kanji address latch; printer strobe/data; cassette motor/output | Verify getters in `kanji_font_rom.h` / `peripherals/printer_port.h` / `peripherals/cassette_interface.h`; machine accessors HAVE (`kanji()`/`printer()`/`cassette()`, `hbf1xv_machine.h:255-260`); NEW getters only where a field is unexposed | `[KANJI]`/`[PRINTER]`/`[CASSETTE]` typed (best-effort) |

### 2.4 Additive const getters required (consolidated, per file)

All are `[[nodiscard]]` const returns of an already-existing private member — zero logic change,
zero behavior change. This is the complete "getter needed here" list. If the developer's own read
finds a field already reachable, that row is dropped (never fabricate a getter for a field that is
already exposed).

1. **`src/machine/debug_dump.h/.cpp`** — do NOT edit `serialize_cpu`. Instead ADD a new
   `debug_snapshot` serializer (new file `src/machine/debug_snapshot.{h,cpp}`) whose CPU section
   emits the full set INCLUDING `WZ=` and `Q=` and the interrupt/ei-delay/pending fields (all already
   readable via `z80a_state.h`). Rationale: preserves the golden-locked state-dump format (A2).
2. **`src/machine/hbf1xv_machine.h`** — ADD `const chipset::MapperIo& mapper_io() const;` (the four
   #FC-#FF segment registers; `mapper_io_` is private at `:608`). ADD `serialize_snapshot()` /
   `write_snapshot(const std::string& id)` (mirroring `serialize_state_dump()`/`write_state_dump()`
   at `:385-390`).
3. **`src/devices/video/v9958_vdp.h`** — ADD getters for `data_latch_`, `register_data_stored_`,
   `palette_data_stored_` (`:205-207`); `write_access_`, `cpu_vram_data_` (`:213-214`);
   `eo_field_`, `irq_vertical_`, `irq_horizontal_`, `irq_level_`, `status_reg0_` (`:217-221`);
   `blink_countdown_` (`:237`).
4. **`src/devices/video/vdp_command_engine.h`** — ADD getters for `status_`, `scr_mode_`,
   `transfer_pending_`, `transfer_kind_`, and the `sx_/sy_/dx_/dy_/nx_/ny_/arg_/cmd_` working
   registers + `transfer_*` members (`:143-168`).
5. **`src/devices/audio/psg_ym2149.h`** — ADD getters for the raw generator state:
   tone count/output (x3), noise count/lfsr/output, envelope count/step/attack/hold/alternate/
   holding, `cycle_residual_`, `level_dwell_integral_` (`:208-240`). (Consider one struct-returning
   accessor to keep the header additive-but-compact.)
6. **`src/devices/audio/ym2413_opll.h`** — ADD getter for `latch_` (`:244`); `has_last_write_`,
   `last_write_was_address_`, `last_write_cycle_` (`:252-254`). Verify `ym2413_synth.h` exposes its
   EG/phase state; ADD getters there only if unexposed.
7. **`src/devices/cartridge/cartridge_fmpac_rom.h`** — ADD getters for `r1ffe_`, `r1fff_` unlock
   latch (`:119-120`).
8. **`src/devices/rtc/rp5c01.h`** — ADD getters for `test_reg_`, `reset_reg_` (`:94-95`) and the
   decoded time fields `fraction_..leap_year_` + `last_rtc_ticks_` (`:98-108`).
9. **`src/devices/fdc/wd2793.h`** — ADD getters for `data_reg_`, `phase_`, `direction_in_`,
   `intrq_pending_`, `immediate_irq_`, `index_irq_armed_`, `index_irq_deadline_`, `hld_active_`,
   `hld_since_`, `busy_until_`, `drq_deadline_`, `last_sync_`, `data_index_`, `data_available_`,
   and the write-track parser members (`:149-197`).
10. **`src/devices/fdc/disk_drive.h`** — ADD getters for `motor_off_pending_`, `motor_off_deadline_`
    (`:95-96`).
11. **`src/devices/chipset/s1985_engine.h`** — ADD getters for `address_`, `pattern_`, `color1_`,
    `color2_` (`:83-86`).
12. **`src/devices/chipset/mb670836_pause.h`** — ADD getter for `frame_index_` (`:102`).
13. **`src/devices/cartridge/cartridge_mapper_device.h`** — ADD a virtual
    `const cartridge::CartridgeRomWindow* rom_window() const { return nullptr; }` (default nullptr),
    overridden by the window-based mappers (Generic/ASCII/Konami/KonamiSCC/Mirrored) to return their
    owned window, so the snapshot dumps bank state generically without RTTI dispatch. (FM-PAC keeps
    the default nullptr — it is dumped via its own `[FMPAC.*]` sections.)

Getters 3-13 are for RESTORE-READINESS. For the near-term Bug B root-cause (§6) the snapshot is
already discriminating with the HAVE-only rows (VDP regs/status/mode/palette, FDC regs+diag, drive/
disk, slots, mapper segments). The developer MAY split getters 3-13 across slices (§5) but ALL are
in-scope for a complete, restore-ready snapshot this cycle.

---

## 3. Snapshot format

### 3.1 Directory layout

```
debug/snapshot/<id>/
  manifest.txt          # header: version, id, deterministic stamps, component index, caller notes
  cpu.txt               # [CPU] full (incl WZ/Q/interrupt/ei-delay/pending)
  memory.txt            # [DRAM] 64 KB + [MAPPER] 4 segment regs
  vram.txt              # [VRAM] 128 KB
  vdp.txt               # [VDP.REGS] [VDP.CMDREGS] [VDP.STATUS] [VDP.LATCH] [VDP.VRAMPTR]
                        #   [VDP.PALETTE] [VDP.MODE] [VDP.IRQ] [VDP.BLINK] [VDP.RASTER]
                        #   [VDP.SPRITE] [VDP.CMD]
  audio.txt             # [PSG.REGS] [PSG.GEN] [OPLL.REGS] [OPLL.TIMING] [OPLL.SYNTH]
  rtc.txt               # [RTC.REGS] [RTC.TIME]
  fdc.txt               # [FDC.REGS] [FDC.DIAG] [FDC.FSM] [DRIVE] [DISK]
  chipset.txt           # [S1985.SRAM] [S1985.REGS] [SLOTS] [SYSCTRL] [CLOCK] [PAUSE] [RENSHA]
  cartridges.txt        # [CART1] [CART2] (+ [CARTn.BANKS]/[CARTn.SCC]) [FMPAC.*] [HALNOTE] [HALNOTE.SRAM]
  frame.txt             # OPTIONAL: existing serialize_frame_dump() decoded frame (convenience)
```

Rationale for the manifest + per-component split (vs one file):
- A per-component file keeps each dump independently diff-able and lets a future restore loader
  parse selectively; the manifest is the index + integrity anchor.
- Each file/section reuses the existing ASCII discipline (`debug_dump`/`debug_format`): fixed field
  order, fixed-width uppercase hex, `\n` endings, folded 16-byte regions for big buffers, no
  locale/stream state, no wall-clock — so two identical runs to the same id are byte-identical.
- One structured file is an acceptable alternative; the split is RECOMMENDED for readability and to
  mirror the existing `traces/`,`frames/`,`logs/` per-artifact convention.

Each per-component file begins with the same version tag line (see 3.3) and ends with `[END]`
(matching `serialize_state_dump()`'s framing at `hbf1xv_machine.cpp:1175`).

### 3.2 Deterministic id scheme

Prefer a **deterministic id** over wall-clock (DEC-0051 point 5). The id is:

`f<frame_count>_c<elapsed_cycles>` — e.g. `f000698_c0000000041637890`.

- Both come from the machine deterministically (`frame_count()`, `elapsed_cycles()`,
  `hbf1xv_machine.h:140-141`). For an identical run to the same frame boundary, the id is identical
  and the CONTENT is byte-identical (auditable).
- A monotonic per-session counter (`snap0000`, `snap0001`, ...) MAY prefix the id for ordering when
  multiple F12 captures happen in one session — still deterministic (it counts captures, not time).
- A wall-clock folder name is permitted ONLY as an optional human-friendly suffix and ONLY if the
  CONTENT stays reproducible; the manifest still records the deterministic `frame`/`cycle` stamps so
  determinism is auditable regardless of folder name. RECOMMENDATION: default to the deterministic
  id; do NOT put wall-clock in any file's CONTENT.

The snapshot writer MUST NOT read the host clock or any RNG for CONTENT (guardrail; §7 risk 2).

### 3.3 Restore-ready properties (format is restore-ready; restore itself is out of scope)

The format is designed so a future save-state loader can be layered on WITHOUT reformatting:

1. **Versioned header** — every file and the manifest carry a format tag, e.g.
   `HBF1XV-SNAPSHOT v1` (mirroring `debug_dump::kDumpFormatTag` `debug_dump.h:40` and
   `frame_dump::kFrameDumpFormatTag` `frame_dump.h:25`). A loader dispatches on the version;
   additive v2 fields never break a v1 reader.
2. **Completeness** — every field a future `restore()` would need to reconstruct exact state is
   captured, including the currently-private internals enumerated in §2.4 (VDP latches/IRQ/command
   FSM, PSG generator counters, YM2413 latch/timing, RTC decoded time, WD2793 FSM/deadlines, S1985
   regs, pause frame index). Buffers (DRAM/VRAM/SRAM/disk) are dumped whole. This is why §2.4 lists
   getters beyond the near-term-needed set: restore-readiness = completeness now.
3. **Typed fields** — each value is a named, fixed-width, unambiguously-parseable token
   (`PC=1234`, `WZ=0000`, `NX=00A0`, `PHASE=ReadSector`), not free prose — reversible by a future
   parser exactly as `frame_dump::parse_frame_dump()` (`frame_dump.h:59`) already reverses its own
   dump. Enum-valued fields (VDP mode, FDC phase, transfer kind) serialize as a stable symbolic
   token AND its numeric code, so a loader never guesses.
4. **Self-describing sizes** — every region carries `size=<n>` (as `serialize_region` already does,
   `debug_dump.cpp:53`), so a loader validates length before reading.

Restore itself (a `load_snapshot()` reader + per-device `apply_state()` setters) is explicitly a
FUTURE milestone — NOT built here. The getters added here are const/read-only; the future restore
milestone adds the write-side setters.

---

## 4. Wiring

### 4.1 F12 in the SDL3 frontend

- **No collision**: F11 (`SDL_SCANCODE_F11`) is disk-swap (`sdl3_app.cpp:256`); F12 is unbound
  (A1). F12 must be consumed as a HOST hotkey and NOT dispatched to `input_mapper_` (exactly like
  F11 at `sdl3_app.cpp:253-259`), so it never leaks into the MSX keyboard matrix.
- **Handler shape**: add `on_snapshot_hotkey()` mirroring `on_disk_swap_hotkey()`
  (`sdl3_app.cpp:324-351`). Because the capture is const/read-only it could run inline, but to
  guarantee a clean, deterministic capture POINT the handler sets a `snapshot_requested_` flag; the
  actual `machine_.write_snapshot(id)` runs at the END of `run_one_frame()` — after
  `on_vsync_boundary()` (`sdl3_app.cpp:281`) — so the machine is always at a frame boundary and the
  id (`frame_count()`) is stable. The manifest gets `disk_index=current_disk_index_` /
  `disk_count=disk_images_.size()` (A4) added by the app.
- **Enable gate**: F12 does nothing unless `config_.snapshot_enabled` (set by `--snapshot`); or,
  simpler and friendlier, F12 always captures to the default `debug/snapshot/` and `--snapshot`
  only overrides the directory. RECOMMENDATION: F12 always active in an interactive session (it is
  read-only and harmless); `--snapshot <dir>` overrides the output dir. Confirm with the human via
  the coordinator if a strict enable-gate is preferred.

### 4.2 `--snapshot` CLI on both executables (additive, default-off)

- **Headless** (`src/main.cpp`, `--debug-session` mode): add `--snapshot <dir>` to
  `DebugSessionOptions` (`main.cpp:742-782`) + its parse arm (mirroring `--dump-state`
  `main.cpp:813-817` and `--debug-root` `:808-812`) + an end-of-run `machine.write_snapshot(id)`
  (mirroring the `--dump-state` write at `main.cpp:1011-1013`). Optionally add `--snapshot-frame <N>`
  (mirroring `--swap-disk-frame` `:845-849` / `--dump-frame` `:838-842`) to capture at a specific
  frame in frame-loop mode for deterministic mid-run captures. `<dir>` sets the snapshot root (via
  `machine.set_debug_root()` or a dedicated snapshot-root; RECOMMEND reusing `debug_root_/snapshot`).
- **SDL3** (`src/frontend/sdl3_cli.h` + `sdl3_app.h`): add `snapshot_dir`/`snapshot_enabled` to
  `ParsedSdl3Cli` (`sdl3_cli.h:41-71`) and `Sdl3AppConfig` (`sdl3_app.h:33-103`), mirroring the
  `dump_state_filename` additive fields (`sdl3_cli.h:63`, `sdl3_app.h:94`). Wire the parse in
  `parse_sdl3_cli()` and pass into the app.
- **Regression guard (hard)**: with NO `--snapshot` (and no F12 press), BOTH executables are
  byte-for-byte identical to pre-Phase-3 — no snapshot dir created, no file written, no changed
  output. This mirrors the M27/M30 "explicit-flag-only, single/no-flag invocations byte-for-byte
  unchanged" discipline (DEC-0015/DEC-0016 precedent). A unit test asserts the default-off no-op.

### 4.3 Where the capture is invoked (safe, non-perturbing point)

- The capture path is `serialize_snapshot()` -> `write_text_file()` per component. It reads state
  via const getters/seams only; it advances neither the CPU nor the scheduler (same contract as
  `serialize_state_dump()`/`write_state_dump()`, `hbf1xv_machine.cpp:1153-1205`, and the non-
  perturbing debug seams `hbf1xv_machine.h:122-130`).
- SDL3: invoked at end of `run_one_frame()` after `on_vsync_boundary()` (frame boundary; §4.1).
- Headless: invoked at end-of-run (or at `--snapshot-frame N` inside the frame loop, at the same
  post-`on_vsync_boundary()` boundary the `--dump-frame`/`--swap-disk-frame` paths already use).
- The VdpRenderSyncAdapter `suspended_` gate (`hbf1xv_machine.h:442-451`) is NOT involved — the
  snapshot never issues `debug_io_write`, only reads.

---

## 5. Milestones — Slice decomposition (S1-S5) with per-slice test obligations

Phase 3 is one milestone-scoped body of work, decomposed into 5 deterministic slices. Each slice is
additive and independently testable. Deterministic-first: every slice adds a unit test that a
component's dump section matches a KNOWN driven state; the final slice adds the byte-identical
cross-run determinism system test.

### S1 — Core + memory snapshot scaffold (CPU full, DRAM, mapper segments, clock, slots)

- New `src/machine/debug_snapshot.{h,cpp}` (CPU-full serializer incl. WZ/Q + interrupt/ei/pending)
  reusing `debug_format`/`serialize_region`; new machine `serialize_snapshot()`/`write_snapshot(id)`
  + `mapper_io()` accessor (§2.4 items 1,2); `[CPU]`,`[DRAM]`,`[MAPPER]`,`[CLOCK]`,`[SLOTS]`,
  `[SYSCTRL]` sections; manifest + versioned header + deterministic id (§3.2/§3.3).
- **Unit**: drive a known CPU/register/WZ/Q/mapper-segment/#A8-#FFFF state; assert each dumped field
  equals the driven value (typed-field exactness). Assert `serialize_cpu` golden UNCHANGED (A2).
- **Unit**: `--snapshot`-absent no-op regression guard (no dir, no file) — both code paths.

### S2 — VDP full section

- `[VDP.REGS/CMDREGS/STATUS/LATCH/VRAMPTR/PALETTE/MODE/IRQ/BLINK/RASTER/SPRITE/CMD]` + `[VRAM]`;
  additive getters §2.4 items 3,4.
- **Unit**: drive register writes / a command / a palette set / a mode change via `debug_io_write`;
  assert the dumped R#0-46, S#0-9, palette, mode, latch, VRAM-pointer, command-FSM fields match.
- **Unit**: 128 KB `[VRAM]` region folds correctly (reuse `serialize_region` fold test pattern).

### S3 — Audio section (PSG / built-in OPLL / inserted FM-PAC OPLL)

- `[PSG.REGS/GEN]`,`[OPLL.REGS/TIMING/SYNTH]`,`[FMPAC.OPLL]`; additive getters §2.4 items 5,6.
- **Unit**: write PSG + YM2413 registers, advance the generator a known number of cycles; assert
  dumped register file + generator counters match. Assert an EMPTY (no FM-PAC) run omits
  `[FMPAC.OPLL]` and a run with an inserted FM-PAC includes it (via `fmpac(slot)`).

### S4 — FDC/RTC/drive/disk/S1985/cartridge/Halnote/pause/Ren-Sha

- `[FDC.REGS/DIAG/FSM]`,`[DRIVE]`,`[DISK]`,`[RTC.REGS/TIME]`,`[S1985.SRAM/REGS]`,`[CART1/2]`
  (+`[CARTn.BANKS/SCC]`),`[FMPAC.SRAM/STATE]`,`[HALNOTE]`+`[HALNOTE.SRAM]`,`[PAUSE]`,`[RENSHA]`;
  additive getters §2.4 items 7-13.
- **Unit**: mount the deterministic disk, run a Read Sector to a known track/sector; assert dumped
  FDC regs+diag, drive track/side/motor, disk size/dirty match. Drive RTC to a known epoch tick;
  assert `[RTC.TIME]`. Load a Konami cartridge, set a bank; assert `[CART1.BANKS]`. Toggle pause /
  set speed / set Ren-Sha; assert `[PAUSE]`/`[RENSHA]`.

### S5 — F12 + `--snapshot` wiring + determinism + .gitignore

- SDL3 F12 handler (`on_snapshot_hotkey` + `snapshot_requested_` + end-of-frame capture, §4.1);
  `--snapshot`(+ optional `--snapshot-frame`) on both executables (§4.2); manifest `disk_index`
  from the frontend (A4); `.gitignore` gains `/debug/snapshot/` (A3).
- **Unit**: `parse_sdl3_cli` accepts `--snapshot <dir>` and leaves every pre-existing case green
  (mirror the M27 AC-10 parse-regression guard); F12 keydown handler cross-consistency (does not
  route to `input_mapper_`; mirror the F11 test).
- **System (the headline determinism test)**: run the SAME deterministic scripted session TWICE
  (two independent `Hbf1xvMachine`/headless runs) to the SAME frame, capture a snapshot each time,
  and assert the two snapshot directories are BYTE-IDENTICAL file-for-file (SHA-256 or exact string
  compare) — the auditable determinism guarantee (DEC-0051 point 5). Mirror the M27 replay-
  determinism system test's adversarial shape: a THIRD, deliberately-diverged run (e.g. one extra
  scripted keypress) MUST produce a different snapshot, proving the test is non-vacuous.

Slice ordering rationale (feedback_system_wide_review_first): the whole component inventory (§2.3)
is enumerated BEFORE any slicing; S1 lays the scaffold + determinism discipline that S2-S4 populate;
S5 wires the triggers only after the content is proven. No slice edits `src/devices/cpu/*` or
`src/core/*`.

---

## 6. Acceptance Criteria (mapped to DEC-0051)

- **AC-1 (comprehensive, DEC-0051 §3)**: a snapshot captures EVERY component in the §2.3 inventory —
  CPU (incl. WZ/Q), 64 KB RAM + segment regs, 128 KB VRAM, V9958 R#0-46 + status + latch + palette +
  command-engine + sprite + raster, PSG, built-in YM2413 + inserted FM-PAC OPLL, RTC, WD2793 + drive
  + disk, S1985 backup RAM, slot/subslot (#A8/#FFFF + secondaries), cartridge mapper banks + FM-PAC
  SRAM/unlock latch, Halnote + its SRAM, pause/speed/Ren-Sha, scheduler/clock counters. A unit test
  asserts the presence of every section for a fully-populated machine.
- **AC-2 (F12, DEC-0051 §1)**: F12 in the SDL3 frontend writes a complete snapshot to
  `debug/snapshot/<id>/`; it does NOT collide with F11 and never reaches the MSX keyboard matrix.
- **AC-3 (`--snapshot`, DEC-0051 §1)**: `--snapshot <dir>` on BOTH executables enables + points the
  capture; with the flag ABSENT (and no F12), both executables are byte-for-byte unchanged
  (regression guard, unit-tested).
- **AC-4 (capture-only + restore-ready, DEC-0051 §2)**: no restore/resume is built; the format is
  versioned, complete, and typed per §3.3 (a documented restore-ready checklist in the report).
- **AC-5 (additive + read-only, DEC-0051 §4)**: only additive const getters + new snapshot files are
  added; `git diff` shows ZERO changes to `src/devices/cpu/*` and `src/core/*`; the existing
  state-dump golden is byte-for-byte unchanged (A2). ZEXALL correctly NOT run (fast-subset cadence,
  `feedback_slow_test_cadence`).
- **AC-6 (deterministic content, DEC-0051 §5)**: two identical runs to the same frame produce
  byte-identical snapshot CONTENT (S5 system test); the writer consumes no RNG/wall-clock for
  content; the id is deterministic (frame/cycle).
- **AC-7 (near-term payoff, DEC-0051 §6)**: a snapshot taken at the YS II building-entry BLACK SCREEN
  (once Bug B is reachable/reproducible) exposes the discriminating state — VDP display-enable
  (R#1 BL) / mode / R#7 / palette / blank-vs-content VRAM pointer, FDC status/phase/diag (read-fail
  vs idle), and slot/subslot/mapper-segment routing — enabling VDP-blank vs FDC-read-fail vs
  slot/mapper root-cause WITHOUT depending on the game's own save. Demonstrated in the report by a
  captured snapshot at that scene (or, if Bug B is not yet reachable this cycle, a captured snapshot
  at a known-good comparison scene proving the discriminating fields are present and correct).
- **AC-8 (evidence gates)**: `tools/validate-assets.ps1`; `tools/checksum-assets.ps1 -OutFile
  docs/asset-checksums.txt`; `cmake --build build --config Debug`; `ctest --test-dir build -C Debug
  --output-on-failure` (fast subset) — all green, both headless and SDL3-ON configs. openMSX A/B is
  N/A BY DESIGN (a snapshot is this emulator's own introspection artifact with no openMSX analogue;
  state it explicitly, do not fabricate a parity claim — the M27/M31 "A/B N/A by design" precedent).
- **AC-9 (QA sign-off)**: deferred to the single M36-closure QA sign-off (per `state/current-phase.md`
  — Phase 1 tooling QA is likewise deferred to M36 closure); QA independently reproduces the
  determinism system test and the default-off no-op guard.

---

## 7. Risks

1. **Broad component surface**. "Every component" spans ~20 devices; a missed field silently leaves
   the snapshot incomplete. MITIGATION: the §2.3 inventory is the exhaustive checklist; S1-S4 map
   1:1 to it; AC-1 unit-asserts every section's presence.
2. **Determinism of content**. If any serializer reads the host clock, an RNG, an unordered
   container iteration, or a pointer value, content drifts between runs. MITIGATION: reuse the proven
   `debug_dump`/`debug_format` locale-free ASCII discipline; the S5 byte-identical cross-run system
   test (with an adversarial third run) is the hard gate; NEVER embed a pointer address or wall-clock
   in CONTENT (folder-name wall-clock is optional and content-irrelevant, §3.2).
3. **Additive-getter-only discipline**. The temptation is to expose mutable references or "convenience"
   setters. MITIGATION: every §2.4 getter is `const` and returns a copy/const-ref of an existing
   member; a `git diff` review confirms no method changes logic and no `src/devices/cpu/*` /
   `src/core/*` edit (AC-5). The golden-state-dump non-edit (A2) is an explicit review item.
4. **Restore-ready without building restore**. Over-engineering a restore reader now is scope creep;
   under-specifying the format forces a later rewrite. MITIGATION: build ONLY the write side
   (serialize + getters); enforce the §3.3 checklist (versioned/complete/typed) so a future
   `load_snapshot()` layers on cleanly. Restore is a named future milestone.
5. **F12 collision / event leakage**. A mis-wired F12 could reach the MSX keyboard matrix or clash
   with a future binding. MITIGATION: consume F12 exactly like F11 (early `continue`, never
   dispatched to `input_mapper_`); unit-test that F12 does not mutate keyboard/joystick state.
6. **`.gitignore` gap**. A `manifest.txt`/`*.json` could be accidentally trackable. MITIGATION: S5
   adds `/debug/snapshot/` (A3); the snapshot writes only under that path.
7. **Cartridge-mapper generic bank dump**. Adding `rom_window()` to the base touches every concrete
   mapper header. MITIGATION: it is a defaulted virtual (`return nullptr`) overridden trivially; the
   FM-PAC keeps the default and is dumped via its own sections. If the developer judges the base-
   class virtual too invasive, the fallback is mapper-type dispatch in the snapshot serializer using
   the already-exposed `mapper_type()` — either is acceptable; recommend the virtual for cleanliness.
8. **Synth internal state (`ym2413_synth.h`)**. If the FM synth engine's EG/phase state is not
   exposed, full OPLL restore-readiness needs getters there too. MITIGATION: S3 verifies and adds
   const getters if needed; if the synth is a pure function of the register file + a cycle counter,
   `[OPLL.SYNTH]` can be a small derived summary (documented as such).

**Tag target recommendation**: ship Phase 3 under the SAME M36 closure tag **v1.0.37** (alongside
the Phase-2 slices and R-M35-1, which `state/current-phase.md` records as "carried to M36 closure").
Phase 3 is additive tooling with no behavior change — a distinct tag buys nothing and fragments the
M36 release. RECOMMEND v1.0.37; defer to the human/coordinator at the release decision.

---

## 8. Scope guardrails (restated, binding)

- ADDITIVE ONLY: new `debug_snapshot.{h,cpp}`, new machine `serialize_snapshot()`/`write_snapshot()`,
  new const getters (§2.4), new CLI/hotkey wiring. No existing behavior altered.
- READ-ONLY w.r.t. emulation: the snapshot never advances the CPU/scheduler and never issues a
  `debug_io_write`; it reads state via const getters and the existing non-perturbing seams.
- ZERO `src/devices/cpu/*` and `src/core/*` edits — ZEXALL stays withheld (fast-subset cadence,
  `feedback_slow_test_cadence`). Verify via `git diff` at every gate.
- DETERMINISTIC: byte-identical content for an identical run to the same frame; no RNG/wall-clock in
  content; deterministic id.
- GOLDEN-SAFE: do not edit `debug_dump::serialize_cpu`/`serialize_region` (preserve M10/M13/M14 dump
  goldens); the snapshot is a SEPARATE artifact.
- No backlog row closed; keep `agent-protocol/` state current at closure (append-only channels,
  ISO-8601 stamps).

---

## 9. Developer Handoff

- **Read first**: this package; DEC-0051 (`decisions.md:612-619`) + DEC-0050 (`:567-608`);
  `src/machine/hbf1xv_machine.{h,cpp}` (`serialize_state_dump`/`write_state_dump` at cpp:1153-1205);
  `src/machine/debug_dump.{h,cpp}` + `debug_format.h`; `src/main.cpp:732-1030` (`--debug-session`);
  `src/frontend/sdl3_app.cpp:207-351` + `sdl3_app.h` + `sdl3_cli.h`; `.gitignore:30-46`.
- **Build S1 first** (scaffold + determinism discipline + CPU/memory/clock/slots), then S2 (VDP),
  S3 (audio), S4 (FDC/RTC/chipset/cartridge/Halnote/pause), S5 (F12 + `--snapshot` + determinism
  system test + `.gitignore`). Run the fast-subset evidence gates each slice.
- **Getters**: implement §2.4 as trivial const returns; do NOT expose mutable refs/setters. Confirm
  each field is not already reachable before adding a getter (drop redundant rows).
- **Serializer**: reuse `serialize_region()` for byte buffers; use `debug_format` for typed fields;
  enum fields emit symbol + numeric code (restore-ready). Version tag `HBF1XV-SNAPSHOT v1`; `[END]`
  framing per file.
- **Do NOT** edit `serialize_cpu`/`serialize_region`, any `src/devices/cpu/*`, or any `src/core/*`.
- **Evidence to capture in the implementation report**: default-off no-op `git diff` proof; the
  determinism-system-test SHA/compare output; a captured example snapshot (committed as authorized
  M36 evidence with a `.gitignore` `!` exception if kept, mirroring the M26/M27 committed-example
  precedent); the restore-ready checklist (§3.3); and (AC-7) a snapshot at the Bug B scene or a
  known-good comparison scene demonstrating the discriminating VDP/FDC/slot fields.
- **QA**: deferred to the single M36-closure sign-off; QA independently reproduces the determinism
  test and the no-op guard, and reviews the additive-getter/`git diff` discipline.
