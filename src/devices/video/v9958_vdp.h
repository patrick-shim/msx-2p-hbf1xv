// ============================================================================
//  Sony HB-F1XV MSX2+ Emulator
//  Copyright (c) 2026 Patrick Shim <patrick.shim@live.co.kr>
//
//  LEGAL NOTICE - Personal reference only.
//  This source code is made available solely for personal, non-commercial
//  reference and educational study. Commercial use, sale, or redistribution
//  for profit is not permitted without the author's written consent.
//  Provided "AS IS", without warranty of any kind.
//  Proprietary BIOS/ROM/disk assets remain the property of their respective
//  rights holders and are NOT licensed by this notice.
// ============================================================================

#pragma once

#include <array>
#include <cstdint>
#include <span>

#include "core/device_contracts.h"
#include "devices/video/irq_line.h"
#include "devices/video/sprite_engine.h"
#include "devices/video/vdp_command_engine.h"
#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace sony_msx::devices::video {

// --- M51 (DEC-0078) Task 2 diagnostic sprite trace --------------------------
// Env-gated (SONY_MSX_M51_SPRITE_TRACE = output file path, or "1" -> stderr);
// DEFAULT-OFF and emulation-invisible: when the env var is unset every call
// site is a cached branch-not-taken and NO emulated state is touched
// (A-M51-4 / EG-8 dual-run byte-identity; the M47 env-gated FDC write-logger
// precedent). Declared here (no new header) so V9958Vdp AND the machine
// render-sync seam (hbf1xv_machine.cpp) write the five M51 event classes
// (docs/m51-planner-package.md §3 Task 2) into ONE raster-ordered stream.
// Lines carry an "F<n> " prefix from the trace's own vsync counter
// (advanced by V9958Vdp::on_vsync()).
[[nodiscard]] bool m51_sprite_trace_enabled();
void m51_sprite_trace(const char* fmt, ...);
void m51_sprite_trace_next_frame();

// Deterministic emulated-cycle clock source for the S#2 VR/HR raster-position
// status bits (bug fix, post-M28). X-pattern of rtc::RtcClockSource/
// fdc::FdcClockSource/peripherals::CassetteClockSource/RenshaTurboClockSource/
// audio::Ym2413ClockSource: read-only, returns CPU T-states elapsed since the
// last on_vsync(), consulted PULL-STYLE only from peek_status_register(2) --
// never wired into step_cpu_instruction()/run_cycles()/run_frame(), so it
// cannot perturb the M9/M12/M23 zero-tolerance CPU-timing oracles.
class VdpClockSource {
public:
    virtual ~VdpClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_tstates_since_vsync() const = 0;

    // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): the ABSOLUTE monotonic CPU-cycle
    // count (never reset at vsync), used ONLY by the command-engine CE busy
    // window so a command's busy duration can span a frame boundary as a trivial
    // u64 comparison (the direct analog of openMSX's monotonic EmuTime). NON-PURE
    // with a default so the two existing test mock clocks -- which implement only
    // cpu_tstates_since_vsync() and never exercise CE-duration -- keep compiling
    // UNEDITED and inert. The machine's VdpRasterClock overrides it with the
    // scheduler's absolute total_cycles() (reads an existing core::Scheduler
    // accessor; does NOT edit src/core).
    [[nodiscard]] virtual std::uint64_t cpu_total_cycles() const {
        return cpu_tstates_since_vsync();
    }
};

// Render-sync seam (M32-S1, Defect A of DEC-0039; docs/m32-planner-package.md
// §2.3). Nullable listener notified at the TOP of V9958Vdp::io_write(), for
// all four ports (#98 VRAM data, #99 register/address, #9A palette, #9B
// indirect), BEFORE the write mutates any state -- the direct analogue of
// openMSX's sync-before-change protocol, where every VDP state-change
// notification renders up to the current beam position before the change
// applies (references/openmsx-21.0/src/video/PixelRenderer.cc:253-394
// register/palette updates; :510-517 VRAM writes). One call site covers
// control registers, palette, VRAM, and every command-engine mutation (all
// 13 commands execute inside register/data-port writes per the M22 hybrid
// model, so their VRAM effects are bracketed by hooked writes). X-pattern of
// VdpClockSource/IrqLine: default nullptr = byte-identical no-op (proven by
// unit test). Zero interrupt/status side effects -- the listener reads
// registers/VRAM and writes only into its own pixel store (§2.3).
class VdpRenderSyncListener {
public:
    virtual ~VdpRenderSyncListener() = default;
    virtual void on_before_state_change() = 0;

    // M44 (DEF-M44-CMDSYNC Phase 1, DEC-0065): commit the accumulated frame up
    // to display line `display_line` (exclusive) from the CURRENT live VRAM.
    // Driven by the command engine's per-destination-row sink so each display
    // row observes the correct partial-command state (the openMSX
    // writeCommon->window.notify->renderUntil analog, with the destination
    // display line standing in for openMSX's EmuTime). M62 BEAM CLAMP
    // (DEC-0091-AMENDMENT-A): the implementation MUST NOT commit rows AHEAD of
    // the render beam -- a command writing rows the beam has not reached has no
    // early-display effect on real hardware (fact-sheet §6 per-scanline model;
    // openMSX renders only the BACKLOG per command write, VDPVRAM.hh:575-593),
    // so the machine adapter clamps the sweep to its beam+2 seam boundary and
    // leaves rows ahead of the beam to render per-line-live on the beam path.
    // DEFAULT EMPTY => every existing listener (and the M32 render-sync seam
    // unit oracle) is inert and byte-identical; only the machine's
    // VdpRenderSyncAdapter overrides it.
    virtual void on_commit_up_to(int /*display_line*/) {}
};

// DEC-0052 (M36 stream-light): non-perturbing control-register-write observer.
// Notified from change_register() for every R#0..R#31 write (the single
// funnel for both the #99 two-write and the #9B indirect protocols) with
// (reg, value), so a diagnostic can watch a specific register (e.g. R#1,
// IE0 = bit5). Command-engine registers R#32..R#46 do NOT notify (they
// return before the funnel). Default-null => byte-identical no-op (X-pattern
// of VdpClockSource/VdpRenderSyncListener); reset() does NOT clear it --
// externally-owned lifecycle pointer. An implementation MUST NOT mutate VDP
// state or advance any clock.
class VdpRegisterWriteObserver {
public:
    virtual ~VdpRegisterWriteObserver() = default;
    virtual void on_register_write(std::uint8_t reg, std::uint8_t value) = 0;
};

// Yamaha V9958 VDP — register / VRAM / status / interrupt CONTRACT (M14).
//
// This device delivers the externally observable behavior a program drives
// through the four I/O ports #98-#9B (+ the S1985 #9C-#9F mirror) and observes
// through status registers, VRAM bytes, and the /INT line. It owns the 128 KB
// VRAM store plus the sprite engine and VDP command engine (M22) that mutate
// it. It computes NO output pixels or colors -- pixel raster rendering,
// sprite/command visual compositing, and YJK/YAE decode live in
// VdpFrameRenderer (backlog D1/D2/D3/D5/D6/D7, all DONE). Still open here is
// D4: cycle-accurate slot/command timing and the exact sub-frame IRQ raster
// position (frame/line-count granularity stands in; see on_vsync()/
// on_line_match() below). Every register bit those features need is stored
// here so software can drive the chip and the contract is A/B-parity-
// verifiable against openMSX.
//
// Grounding: references/fact-sheets/Yamaha V9958 VDP.md (registers, ports,
// status, interrupts); behavior reference references/openmsx-21.0/src/video/
// VDP.cc (writeIO 645-733, readIO 988-1012, status 903-986, IRQ 402-415,
// reset 289-305, VRAM access 849-888) — read for understanding, NEVER copied
// (GPL isolation).
class V9958Vdp final : public core::IoDevice {
public:
    // Number of control registers modeled here (R#0..R#31). R#32..R#46 are
    // the command engine's own register file (see cmd_engine(), M22) and are
    // dispatched to it separately in change_register().
    static constexpr int kNumControlRegs = 32;
    static constexpr int kNumStatusRegs = 10;   // S#0..S#9 (V9958 set)
    static constexpr int kNumPaletteEntries = 16;

    // V9958 VDP identity: S#1 bits 1..5 read 2 -> S#1 reset value 0x04
    // (fact-sheet §Identity; VDP.cc:291).
    static constexpr std::uint8_t kStatusReg1Reset = 0x04;
    // S#2 reset value: undocumented bits 3,2 = 1 (VDP.cc:292; fact-sheet §4/R-5).
    static constexpr std::uint8_t kStatusReg2Base = 0x0C;

    V9958Vdp();

    // Deterministic power-on / reset: clears VRAM, control registers, status,
    // latches, the VRAM pointer, the IRQ lines, and loads the V9938 boot palette
    // (VDP.cc:289-305). Releases the /INT line.
    void reset();

    // Wire the /INT sink (the machine's CPU adapter). Nullptr detaches.
    void set_irq_line(IrqLine* sink);

    // Wire the S#2 VR/HR raster-position clock source (bug fix, post-M28).
    // Nullptr (the default): VR/HR read as their prior hardcoded-0 fallback
    // (e.g. a unit test constructing a bare V9958Vdp with no machine).
    void attach_clock_source(VdpClockSource* source);

    // Wire the render-sync listener (M32-S1, see VdpRenderSyncListener
    // above). Nullptr (the default) detaches -- byte-identical no-op.
    void attach_render_sync(VdpRenderSyncListener* listener);

    // DEC-0052 stream-light: wire the control-register-write observer (see
    // VdpRegisterWriteObserver above). Nullptr (the default) detaches --
    // byte-identical no-op. Not cleared by reset().
    void attach_register_write_observer(VdpRegisterWriteObserver* observer);

    // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): while set, an R#46 command write
    // does NOT arm the S#2-bit0 CE busy window (§2.4.3). The machine brackets its
    // non-perturbing debug_io_write with this exactly as it brackets the
    // render-sync adapter's set_suspended, so debug/inspection-issued commands
    // stay byte-identical (only real CPU-driven OUT writes pace CE). Default false.
    void set_command_timing_suspended(bool suspended);

    // DEC-0072 diagnostic knob (M47-followup, NOT committed): add a signed
    // T-state bias to the CE busy-window duration computed in
    // arm_command_busy_window(). Lets a diagnostic sweep probe whether a multi-disk
    // RPG title's save-build corruption is causally gated by the command-engine CE/finish
    // timing (the named #1 suspect vs openMSX calcFinishTime). Default 0 =
    // byte-for-byte unchanged.
    void set_cmd_busy_bias(std::int64_t bias_tstates) { cmd_busy_bias_tstates_ = bias_tstates; }

    // --- core::IoDevice (dispatch on port & 0x03; the S1985 mirror #9C-#9F
    //     collapses to the same 0..3, A-2). ---
    core::BusData io_read(core::BusAddress port) override;
    void io_write(core::BusAddress port, core::BusData value) override;

    // --- Per-frame interrupt hooks (frame/line-count granularity; exact sub-
    //     frame raster position is DEFERRED D4). ---
    // Frame boundary (bottom-border start): set S#0 F, toggle the S#2 field bit,
    // and assert the vertical IRQ line when R#1 IE0 is enabled (VDP.cc:402-406).
    void on_vsync();
    // R#19 line match: assert the horizontal IRQ line when R#0 IE1 is enabled
    // (VDP.cc:409-415). Provided as a deterministic hook for the line-int model.
    void on_line_match();

    // M49 Slice 2 (backlog D9): the machine's render-sync adapter calls this from
    // on_before_state_change() -- BEFORE it commits the background -- to keep the
    // incremental sprite plane in pace with the background at `target`, the
    // EXCLUSIVE beam+2 commit boundary the background VdpScanlineAccumulator::
    // sync_to_line uses (hbf1xv_machine.cpp:101). Committing the sprite lines FIRST
    // (from the CURRENT pre-write state) makes the background read a per-line-live
    // sprite table AND makes a mid-frame sprite-relevant register change (still the
    // OLD value here) split the sprite plane at the SAME display line as the
    // background split (AC-S2). Lazy-opens the per-frame progressive pass on the
    // first ACTIVE-display write (binding live VRAM/registers + capturing the frame
    // geometry, watermark reset); `wrap` (a genuine frame wrap / D10 geometry jump,
    // the raster line DECREASED) re-opens it, mirroring the accumulator's wrap-safety
    // restart. on_vsync() completes + finalizes it. Advance-only within a frame
    // (SpriteEngine::check_until never moves the watermark backwards -- the M44
    // direction discipline).
    void commit_sprite_split(int target, bool wrap);

    // M51 (DEC-0078) fix, branch (a) shape (i): CONSUMER-side sprite pacing for
    // every non-seam background commit -- the M44 command-row sink
    // (VdpRenderSyncAdapter::on_commit_up_to) and the render_frame() mid-display
    // memoization syncs. Before M51 the command sink advanced/committed
    // background rows PAST the render-only sprite watermark while the per-line
    // buffers above it were still empty from commit_sprite_split's lazy-open
    // begin_frame() clear, sealing those rows sprite-less for the whole frame
    // (the scrolling-shooter / sprite-scroll / split-screen regression titles'
    // sprite-disappearance defect; trace
    // evidence docs/m51-implementation-report.md). openMSX makes the CONSUMER
    // responsible for pacing the checker before reading it: every renderUntil()
    // calls spriteChecker.checkUntil(time) BEFORE rasterizing, regardless of
    // what triggered the render (references/openmsx-21.0/src/video/
    // PixelRenderer.cc:580-584, SpriteChecker.hh:242-247 -- EFFECT re-expressed
    // at our line-granular seam, never transcribed); fMSX's fused per-scanline
    // Sprites()/ColorSprites() selection makes the same invariant structural
    // (references/fmsx-60/source/fMSX/Common.h:99-155). ADVANCE-ONLY-WHEN-
    // ACTIVE: while the split is not open the buffers still hold the previous
    // on_vsync() recompute (fully populated -- the documented pre-M49
    // 1-frame-stale fallback, never absence), and the real CPU OUT that started
    // the command already lazy-opened the split via on_before_state_change, so
    // a command commit never needs to open it (Task 2 trace confirms: every
    // failing CMDCOMMIT ran with the split active). `target` is the EXCLUSIVE
    // boundary of the rows about to be committed (same convention as
    // commit_sprite_split); RENDER-ONLY (frame-atomic collision/status
    // untouched, DEC-0031).
    void commit_sprite_rows(int target);

    // Current combined /INT level (vertical OR horizontal). The machine level-
    // samples this to re-assert the held line after the CPU accept clears its
    // internal pending flag (R-1: the VDP owns the level; release only on the
    // status-register read).
    [[nodiscard]] bool irq_active() const;

    // --- Debug / test accessors (const; no side effects). ---
    [[nodiscard]] const VdpVram& vram() const;
    VdpVram& vram();
    [[nodiscard]] std::uint8_t control_register(int index) const;
    // Non-destructive status peek (no flag reset). io_read(#99) uses the
    // destructive path instead.
    [[nodiscard]] std::uint8_t peek_status_register(int index) const;
    [[nodiscard]] std::uint16_t vram_pointer() const;   // 14-bit A13..A0
    [[nodiscard]] std::uint32_t vram_address() const;    // full 17-bit effective
    [[nodiscard]] std::uint16_t palette_entry(int index) const;  // 9-bit GRB
    [[nodiscard]] const VdpModeState& mode() const;

    // --- Blink state (M21-S2, backlog D6). ---
    // Frame-hook-driven countdown, decremented once per on_vsync() (mirrors
    // openMSX's own frameStart() blink logic, VDP.cc:600-608) and re-armed
    // from R#13 on both natural expiry and on any write to R#13 itself
    // (VDP.cc:1040-1057 -- a write to R#13 resets blink state even when the
    // written value is unchanged). No new clock consumer: driven only by the
    // existing on_vsync() hook.
    [[nodiscard]] bool blink_state() const;

    // --- Sprite engine + VDP command engine (M22, backlog D2/D3, closes
    //     D7's remaining command-engine-path piece). Owned INSIDE V9958Vdp
    //     (mirrors the blink_countdown_/blink_state_ ownership style),
    //     exposed read-only for VdpFrameRenderer's sprite compositing pass
    //     and for tests/debug. ---
    [[nodiscard]] const SpriteEngine& sprite_engine() const;
    [[nodiscard]] const VdpCommandEngine& cmd_engine() const;

    // Current raster position as an ACTIVE-DISPLAY line index (0-based;
    // negative while the raster is in the border/erase/sync region between
    // on_vsync() -- the start of the lower border, fact-sheet §7 -- and the
    // top of the next active area). Derived pull-style from the same
    // VdpClockSource the S#2 VR/HR bits use; INT_MIN when no clock is
    // attached (raster-collision hooks become no-ops, preserving clockless-
    // test behavior). Used by the S#0 read path for line-granular collision
    // re-latching (boot-logo fix) and -- public since M32-S2 -- by the
    // machine's render-sync adapter (write at line L takes effect from line
    // L+1) and line-interrupt delivery (docs/m32-planner-package.md §2.3/§2.5).
    [[nodiscard]] int raster_display_line() const;

    // --- M36 Phase 3 debug snapshot: additive read-only introspection of the
    //     internal two-write latch protocol, VRAM read-ahead/access state, the
    //     status/IRQ/field flags, and the blink countdown. Every accessor is a
    //     const return of an already-existing private member -- ZERO behavior
    //     change (docs/m36-phase3-planner-package.md §2.4 item 3). These make
    //     the snapshot restore-ready without touching any emulation path. ---
    [[nodiscard]] std::uint8_t data_latch() const { return data_latch_; }
    [[nodiscard]] bool register_data_stored() const { return register_data_stored_; }
    [[nodiscard]] bool palette_data_stored() const { return palette_data_stored_; }
    [[nodiscard]] bool write_access() const { return write_access_; }
    [[nodiscard]] std::uint8_t cpu_vram_data() const { return cpu_vram_data_; }
    [[nodiscard]] std::uint8_t status_reg0() const { return status_reg0_; }
    [[nodiscard]] bool eo_field() const { return eo_field_; }
    [[nodiscard]] bool irq_vertical() const { return irq_vertical_; }
    [[nodiscard]] bool irq_horizontal() const { return irq_horizontal_; }
    [[nodiscard]] bool irq_level() const { return irq_level_; }
    [[nodiscard]] int blink_countdown() const { return blink_countdown_; }

    // M48 Slice 2 (DEC-0075) test/debug accessor: the ABSOLUTE CPU cycle at which
    // the current command's S#2-bit0 CE busy window expires (0 == none armed /
    // already expired). A const return of an existing private member -- ZERO
    // behavior change (same read-only-introspection idiom as the M36 accessors
    // above). Lets the CPU-priority slot-steal oracle assert the window was
    // extended by exactly N * slot_cost_tstates() without polling the CE bit.
    [[nodiscard]] std::uint64_t cmd_busy_until_cycles() const { return cmd_busy_until_cycles_; }

private:
    // #98 VRAM data path.
    void vram_data_write(std::uint8_t value);
    [[nodiscard]] std::uint8_t vram_data_read();
    void advance_vram_pointer();
    [[nodiscard]] std::uint32_t effective_address() const;

    // #99 register/address two-write protocol + status read.
    void port1_write(std::uint8_t value);
    [[nodiscard]] std::uint8_t read_status(int reg);  // destructive (flag reset)

    // #9A palette two-write protocol.
    void palette_write(std::uint8_t value);

    // #9B indirect-register data path (R#17 pointer + AII).
    void indirect_write(std::uint8_t value);

    // Central register-write side-effect dispatch (mirrors changeRegister).
    void change_register(std::uint8_t reg, std::uint8_t value);

    void recompute_mode();
    void update_irq();
    [[nodiscard]] bool is_v9938_mode() const;

    // --- M49 Slice 2 (backlog D9): the progressive per-frame sprite-split lifecycle. ---
    // The active-display pixel height for the CURRENT mode -- the on_vsync() sprite
    // buffer height (A-M49-2: a per-frame decision). Extracted so begin_sprite_frame()
    // and on_vsync() compute it identically.
    [[nodiscard]] int sprite_frame_height() const;
    // (Re)bind the progressive sprite pass to the live VRAM/registers + capture the
    // current geometry (mode base + height), resetting the watermark. Called at the
    // lazy-open of the split pass.
    void begin_sprite_frame();
    [[nodiscard]] std::span<const std::uint8_t, kNumControlRegs> control_regs_span() const {
        return std::span<const std::uint8_t, kNumControlRegs>(control_regs_);
    }

    // M48 Slice 1 (DEC-0075): the live 3-way CPU/command VRAM access-slot regime
    // for the command-throughput cap -- one of the three tier-1 per-line slot
    // COUNTS (vdp_access_timing::kSlotsPerLine{DisplayOff=154,SpritesOff=88,
    // SpritesOn=31}, fact-sheet §8 line 163) selected from live R#1 BL +
    // raster_display_line() + R#8 SPD + mode (A-M48-2). Pure/const; consulted at
    // command-issue time by arm_command_busy_window() (Slice 1) and per CPU #98
    // access by the io paths (Slice 2). NEVER gates CPU T-states.
    [[nodiscard]] int effective_slots_per_line() const;

    // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): arm the S#2-bit0 CE busy window
    // from the command engine's just-computed pure duration. Called after an
    // R#46 command dispatch when a clock is attached and command timing is not
    // suspended. M48 Slice 1 (DEC-0075): inflates the base (max-availability,
    // 154-slot) duration by slot_factor = 154 / effective_slots_per_line()
    // (~1.00 / 1.75 / 4.97 for the three tier-1 slot counts, fact-sheet §8 line
    // 163) -- SUPERSEDING the empirical per-mode kActiveDisplaySlotFactorPercent
    // placeholder. Sets cmd_busy_until_cycles_ to the absolute cycle at which CE
    // clears; a 0 base duration yields cmd_busy_until == now (inert). Pure
    // timing/STATUS overlay -- VRAM mutation stays atomic.
    void arm_command_busy_window();

    // M48 Slice 2 (DEC-0075): the CPU-priority VRAM access-slot steal. Called on
    // every CPU #98 VRAM data-port access (io_read/io_write, port & 3 == 0). If a
    // command is BUSY at this access, extends cmd_busy_until_cycles_ by ONE stolen
    // slot = slot_cost_tstates(effective_slots_per_line()) T-states (fact-sheet §8
    // line 163 "CPU VRAM access takes priority over the command engine and steals
    // slots"). ONE-DIRECTIONAL: the CPU access is NEVER stalled/delayed/dropped
    // (the HB-F1XV does not wire /WAIT, fact-sheet §1/§7) -- this ONLY lengthens
    // the reported S#2-bit0 CE busy window (STATUS/timing overlay; VRAM mutation
    // stays atomic). Inert without a clock or while command timing is suspended,
    // exactly like arm_command_busy_window().
    void steal_command_slot_for_cpu_vram_access();

    // VDP command-timing parity: arm the S#2 TR (transfer-ready, bit7) drop-then-
    // rearm window after the command engine services one event-driven transfer
    // unit. Called (clock attached, timing not suspended) from the COL-write /
    // R#46-issue path and the S#7-read path when transfer_units_consumed()
    // advanced. Sets tr_busy_until_cycles_ to the absolute cycle at which TR
    // re-raises; a 0 per-unit cost yields tr_busy_until == now (inert).
    void arm_transfer_ready_window();

    // M44 (DEF-M44-CMDSYNC Phase 1, DEC-0065): the command-engine per-
    // destination-row render-sync. Applies the strict-superset gates and, when
    // they pass, forwards a commit-to-display-line request to render_sync_:
    //   1. bitmap-mode + VISIBLE-PAGE gate -- only when `dy`'s page is one the
    //      renderer is currently displaying (non-bitmap/text and off-screen
    //      pages fall back to today's single-beam-line io_write-seam behavior,
    //      no regression; the openMSX isInside(bitmapVisibleWindow) analog).
    //   2. ACTIVE-DISPLAY gate -- suppressed while the raster is in vblank/
    //      border (raster_display_line() < 0), keeping the common vblank-HUD-
    //      rebuild path byte-identical (the key anti-regression guard).
    //   3. display-line inverse L = ((dy & 0xFF) - R#23) & 0xFF, the exact
    //      inverse of VdpFrameRenderer::vertical_scroll_wrap (the horizontal /
    //      multi-page geometry needs no inversion -- the accumulator's
    //      render_line reads it live). The WRAP guard (never sealing a partial
    //      frame mid-command) lives at the commit primitive in the machine
    //      adapter, where the authoritative accumulator watermark is known.
    void command_row_sync(unsigned dy);

    // Private adapter binding the command engine's CommandRowSink to this VDP.
    class CommandRowSyncAdapter final : public VdpCommandEngine::CommandRowSink {
    public:
        explicit CommandRowSyncAdapter(V9958Vdp& vdp) : vdp_(vdp) {}
        void on_dest_row(unsigned dy) override { vdp_.command_row_sync(dy); }

    private:
        V9958Vdp& vdp_;
    };

    VdpVram vram_;
    std::array<std::uint8_t, kNumControlRegs> control_regs_{};
    std::array<std::uint16_t, kNumPaletteEntries> palette_{};

    // Two-write latch protocol state (VDP.cc:664-706/709-718).
    std::uint8_t data_latch_ = 0;
    bool register_data_stored_ = false;   // #99 first byte latched
    bool palette_data_stored_ = false;    // #9A first byte latched

    // VRAM addressing (VDP.cc:849-888). vram_pointer_ is the 14-bit A13..A0
    // counter; A16..A14 come from R#14. cpu_vram_data_ is the read-ahead buffer,
    // SHARED between read and write (VDP.cc:789-791).
    std::uint16_t vram_pointer_ = 0;
    bool write_access_ = false;
    std::uint8_t cpu_vram_data_ = 0;

    // Status + interrupt state (VDP.cc:289-296/402-415/961-986).
    std::uint8_t status_reg0_ = 0x00;   // bit7 F (VBlank)
    bool eo_field_ = false;             // S#2 EO field toggle
    bool irq_vertical_ = false;         // vertical (VBlank) line held
    bool irq_horizontal_ = false;       // horizontal (line) held
    bool irq_level_ = false;            // last pushed combined level

    IrqLine* irq_sink_ = nullptr;
    VdpModeState mode_{};

    // S#2 VR/HR raster-position clock source (bug fix, post-M28). Nullptr by
    // default; attached by the machine in wire_bus().
    VdpClockSource* clock_source_ = nullptr;

    // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): the ABSOLUTE CPU cycle at which
    // the last atomic command's reported S#2-bit0 CE busy window expires. CE is
    // reported busy while cmd_busy_until_cycles_ > clock_source_->cpu_total_cycles().
    // 0 (== an already-expired window) at reset and whenever no window is armed.
    // Only ever read when clock_source_ != nullptr.
    std::uint64_t cmd_busy_until_cycles_ = 0;
    // VDP command-timing parity: the ABSOLUTE CPU cycle at which the S#2 TR
    // (transfer-ready, bit7) bit re-raises after the last serviced event-driven
    // transfer unit. TR reads 0 while tr_busy_until_cycles_ > cpu_total_cycles().
    // 0 (already-expired) at reset and whenever no unit is in-flight. Only ever
    // read when clock_source_ != nullptr.
    std::uint64_t tr_busy_until_cycles_ = 0;
    // DEC-0072 diagnostic bias (see set_cmd_busy_bias); default 0 = no change.
    std::int64_t cmd_busy_bias_tstates_ = 0;
    // When true, an R#46 command write does NOT arm the busy window above
    // (debug_io_write exclusion; see set_command_timing_suspended). Default false;
    // toggled by the machine around its non-perturbing debug seam. Not a piece of
    // emulated device state -> deliberately NOT touched by reset().
    bool command_timing_suspended_ = false;

    // Render-sync listener (M32-S1). Nullptr by default (byte-identical
    // no-op); attached by the machine in wire_bus().
    VdpRenderSyncListener* render_sync_ = nullptr;

    // DEC-0052 stream-light control-register-write observer. Nullptr by default
    // (byte-identical no-op); installed only while a stream capture is armed.
    VdpRegisterWriteObserver* register_write_observer_ = nullptr;

    // Blink countdown state (M21-S2, backlog D6; VDP.cc:600-608/1040-1057).
    // Frames remaining at the current blink phase; 0 = stable (no further
    // toggling) until R#13 is next written with both nibbles non-zero.
    int blink_countdown_ = 0;
    bool blink_state_ = false;

    // Sprite engine + command engine (M22). cmd_engine_ is constructed with
    // a reference to the SAME vram_ this device owns (declared above), so
    // command-engine writes land directly in the shared VRAM store.
    VdpCommandEngine cmd_engine_;
    SpriteEngine sprite_engine_;

    // M49 Slice 2 (backlog D9): the RENDERING-ONLY progressive sprite pass is
    // "split-active" for the current frame once an ACTIVE-display write lazy-opened
    // it (commit_sprite_split), so subsequent same-frame writes advance the SAME pass
    // instead of re-beginning. It gates ONLY the visible-sprite buffer split; the
    // CPU-visible collision/status is ALWAYS the frame-atomic recompute_frame() at
    // on_vsync (byte-identical to pre-M49 for every game). Reset each frame at
    // on_vsync() and at reset().
    bool sprite_split_active_ = false;

    // M44: the command-engine per-destination-row render-sync adapter, installed
    // into cmd_engine_ in the constructor. Holds only a reference to *this; the
    // command engine drives it during atomic command execution.
    CommandRowSyncAdapter command_row_sink_{*this};
};

}  // namespace sony_msx::devices::video
