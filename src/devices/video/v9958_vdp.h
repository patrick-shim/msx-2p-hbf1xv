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

#include "core/device_contracts.h"
#include "devices/video/irq_line.h"
#include "devices/video/sprite_engine.h"
#include "devices/video/vdp_command_engine.h"
#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace sony_msx::devices::video {

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
    // display line standing in for openMSX's EmuTime). DEFAULT EMPTY => every
    // existing listener (and the M32 render-sync seam unit oracle) is inert and
    // byte-identical; only the machine's VdpRenderSyncAdapter overrides it.
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

    // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): arm the S#2-bit0 CE busy window
    // from the command engine's just-computed pure duration. Called after an
    // R#46 command dispatch when a clock is attached and command timing is not
    // suspended. Applies a per-mode active-display slot-availability correction
    // to the base (openMSX-underestimate) duration -- the single calibrated
    // knob (§3.3). Sets cmd_busy_until_cycles_ to the absolute cycle at which CE
    // clears; a 0 base duration yields cmd_busy_until == now (inert).
    void arm_command_busy_window();

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

    // M44: the command-engine per-destination-row render-sync adapter, installed
    // into cmd_engine_ in the constructor. Holds only a reference to *this; the
    // command engine drives it during atomic command execution.
    CommandRowSyncAdapter command_row_sink_{*this};
};

}  // namespace sony_msx::devices::video
