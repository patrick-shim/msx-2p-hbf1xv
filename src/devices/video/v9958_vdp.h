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
// audio::Ym2413ClockSource: returns CPU T-states elapsed since the most
// recent on_vsync() call, READ-ONLY, consulted PULL-STYLE only from
// peek_status_register(2) -- never wired into step_cpu_instruction()/
// run_cycles()/run_frame(), so it cannot perturb the M9/M12/M23
// zero-tolerance CPU-timing oracles.
class VdpClockSource {
public:
    virtual ~VdpClockSource() = default;
    [[nodiscard]] virtual std::uint64_t cpu_tstates_since_vsync() const = 0;
};

// Render-sync seam (M32-S1, Defect A of DEC-0039; docs/m32-planner-package.md
// §2.3). Nullable listener notified at the TOP of V9958Vdp::io_write() for
// all four ports (#98 VRAM data, #99 register/address, #9A palette, #9B
// indirect) BEFORE the write mutates any state -- the direct analogue of
// openMSX's sync-before-change protocol, where every VDP state-change
// notification renders up to the current beam position before the change
// applies (references/openmsx-21.0/src/video/PixelRenderer.cc:253-394
// register/palette updates; :510-517 VRAM writes). One call site, uniform:
// covers control registers, palette, VRAM, and every command-engine mutation
// (all 13 commands execute inside register/data-port writes per the M22
// hybrid model, so their VRAM effects are bracketed by hooked writes).
// X-pattern of VdpClockSource/IrqLine: default nullptr = byte-identical
// no-op (proven by unit test). Rendering through this seam has ZERO
// interrupt/status side effects -- the listener reads registers/VRAM and
// writes only into its own pixel store (§2.3).
class VdpRenderSyncListener {
public:
    virtual ~VdpRenderSyncListener() = default;
    virtual void on_before_state_change() = 0;
};

// Yamaha V9958 VDP — register / VRAM / status / interrupt CONTRACT (M14).
//
// This device delivers the externally observable behavior a program drives
// through the four I/O ports #98-#9B (+ the S1985 #9C-#9F mirror) and observes
// through status registers, VRAM bytes, and the /INT line. It owns the 128 KB
// VRAM store. It computes NO output pixels or colors: pixel raster rendering,
// sprites+collision, the command engine, cycle-accurate slot/command timing, the
// exact sub-frame IRQ position, YJK/YAE decode, and the visual scroll/interlace/
// blink/superimpose effects are DEFERRED (backlog D1-D7). Every register bit
// those features need is stored here so software can drive the chip and the
// contract is A/B-parity-verifiable against openMSX.
//
// Grounding: references/fact-sheets/Yamaha V9958 VDP.md (registers, ports,
// status, interrupts); behavior reference references/openmsx-21.0/src/video/
// VDP.cc (writeIO 645-733, readIO 988-1012, status 903-986, IRQ 402-415,
// reset 289-305, VRAM access 849-888) — read for understanding, NEVER copied
// (GPL isolation).
class V9958Vdp final : public core::IoDevice {
public:
    // Number of control registers modeled (R#0..R#31). Command registers
    // R#32..R#46 are DEFERRED (backlog D3) and ignored on write.
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
    // Nullptr (the default) means VR/HR read as their prior hardcoded-0
    // fallback (no clock attached, e.g. unit tests constructing a bare
    // V9958Vdp with no machine).
    void attach_clock_source(VdpClockSource* source);

    // Wire the render-sync listener (M32-S1, see VdpRenderSyncListener
    // above). Nullptr (the default) detaches -- byte-identical no-op.
    void attach_render_sync(VdpRenderSyncListener* listener);

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
    // attached (turning the sprite engine's raster-collision hooks into
    // no-ops, preserving clockless-test behavior). Used by the S#0 read path
    // for line-granular collision re-latching (boot-logo fix) and -- public
    // since M32-S2 -- by the machine's render-sync adapter (write at line L
    // takes effect from line L+1) and line-interrupt delivery
    // (docs/m32-planner-package.md §2.3/§2.5).
    [[nodiscard]] int raster_display_line() const;

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

    // Render-sync listener (M32-S1). Nullptr by default (byte-identical
    // no-op); attached by the machine in wire_bus().
    VdpRenderSyncListener* render_sync_ = nullptr;

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
};

}  // namespace sony_msx::devices::video
