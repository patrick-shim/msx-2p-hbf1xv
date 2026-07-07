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
