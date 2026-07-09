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

#include <cstdint>

#include "devices/video/vdp_mode.h"
#include "devices/video/vdp_vram.h"

namespace sony_msx::devices::video {

// V9958/V9938 VDP command engine (M22-S3..S7, backlog D3; closes D7's
// remaining command-engine-path piece, planner package §1.5). Owned INSIDE
// V9958Vdp as a private member (mirrors the SpriteEngine/blink_countdown_
// ownership style), constructed with a reference to the SAME VdpVram the
// V9958Vdp owns -- command-engine writes land directly in the shared VRAM
// store, so VdpFrameRenderer's EXISTING background-rendering code paths see
// command-engine-written content with ZERO changes.
//
// Register file (SX/SY/DX/DY/NX/NY/COL/ARG/CMD + internal ASX/ADX/ANX
// trackers) is owned HERE, a SEPARATE storage from V9958Vdp::control_regs_
// (which stays at its existing 32 entries, unchanged; A-M22-2).
//
// Execution model is HYBRID (planner package §1.4 Resolution 2): 10
// commands (ABRT/STOP, POINT, SRCH, LINE, LMMV, LMMM, HMMV, HMMM, YMMM,
// PSET) execute ATOMICALLY -- the entire NX*NY operation completes
// synchronously within the call that writes CMD (R#46); CE is observably 0
// immediately after that call returns. The other 3 (LMCM, LMMC, HMMC)
// require a genuine, EVENT-DRIVEN (never wall-clock/cycle-scheduled),
// multi-step state machine, directly mirroring this project's own FDC
// DRQ/INTRQ precedent (src/devices/fdc/wd2793.*): CE stays 1 across the
// whole transfer; the command completes only after NX*NY individual,
// separate CPU-port interactions (writes to R#44/COL, or reads of S#7) have
// each been serviced.
//
// Disclosed simplification (low risk, not exercised by any required test):
// unlike the reference, this engine does NOT mutate the SX/SY/DX/DY register
// members during command execution (the reference mutates DY/SY/ADX/ASX in
// place as a block command progresses, a real but obscure hardware quirk
// visible only via non-standard mid-command register peeking). All
// coordinate advancement uses local working copies for the 10 atomic
// commands, and dedicated transfer_* members for the 3 event-driven
// commands; only ASX (exposed via S#8/S#9, used by SRCH and LMCM) is a
// genuine persistent member, matching the one case where real software can
// observe it (VDPCmdEngine.hh:104-114 getBorderX() -- "real VDP simply
// returns the current value of the ASX...counter, regardless of the command
// being executed").
//
// Grounding (behavior reference only, GPL isolation -- never copied):
// references/openmsx-21.0/src/video/VDPCmdEngine.{hh,cc}.
class VdpCommandEngine {
public:
    explicit VdpCommandEngine(VdpVram& vram) : vram_(vram) {}

    // R#32..R#46 register-write dispatch (index 0..14). Works identically
    // whether the caller reached it via the #99 two-write latch protocol or
    // the #9B indirect-register path (A-M22-1) -- both already route through
    // V9958Vdp::change_register(), which is the only caller.
    void write_register(int index, std::uint8_t value);
    // Debug/test peek of a command register (mirrors peekCmdReg).
    [[nodiscard]] std::uint8_t read_register(int index) const;

    // scrMode recompute (A-M22-6), called from V9958Vdp::recompute_mode()
    // whenever the mode or R#25's CMD bit changes.
    void notify_mode_change(const VdpModeState& mode, bool cmd_bit);

    // S#2 status bits, live.
    [[nodiscard]] bool tr() const { return (status_ & kTr) != 0; }
    [[nodiscard]] bool bd() const { return (status_ & kBd) != 0; }
    [[nodiscard]] bool ce() const { return (status_ & kCe) != 0; }

    // S#7: live COL. Reading the STATUS register (not the raw R#44 peek)
    // must call on_color_register_read() first (computes/advances the next
    // LMCM pixel, mirrors resetColor()'s pairing with readColor()'s sync()),
    // THEN return color().
    [[nodiscard]] std::uint8_t color() const { return col_; }
    void on_color_register_read();

    // S#8/S#9: ASX (border-X working register), high byte masked by the
    // caller with | 0xFE (matches the existing M14 stub mask). S#9 read
    // additionally clears BD (mirrors resetBD()).
    [[nodiscard]] unsigned border_x() const { return asx_; }
    void on_border_x_register_read();

    // Deterministic power-on reset.
    void reset();

private:
    static constexpr std::uint8_t kTr = 0x80;
    static constexpr std::uint8_t kBd = 0x10;
    static constexpr std::uint8_t kCe = 0x01;

    // ARG register bits (VDPCmdEngine.hh:27-33).
    static constexpr std::uint8_t kMxd = 0x20;
    static constexpr std::uint8_t kMxs = 0x10;
    static constexpr std::uint8_t kDiy = 0x08;
    static constexpr std::uint8_t kDix = 0x04;
    static constexpr std::uint8_t kEq = 0x02;
    static constexpr std::uint8_t kMaj = 0x01;

    enum class LogicalOp : std::uint8_t { Imp, And, Or, Xor, Not, Dummy };
    struct DecodedOp {
        LogicalOp op;
        bool transparent;
    };
    enum class TransferKind : std::uint8_t { None, Lmcm, Lmmc, Hmmc };

    static DecodedOp decode_op(std::uint8_t nibble);
    static std::uint8_t point_pixel(int scr_mode, unsigned x, unsigned y, const VdpVram& vram);
    static void write_pixel(int scr_mode, unsigned x, unsigned y, std::uint8_t color, LogicalOp op,
                            bool transparent, VdpVram& vram);

    void execute_command();
    void command_done();

    void run_point();
    void run_pset();
    void run_srch();
    void run_line();
    void run_lmmv();
    void run_lmmm();
    void run_hmmv();
    void run_hmmm();
    void run_ymmm();
    void start_lmcm();
    void start_lmmc();
    void start_hmmc();
    void perform_transfer_step();

    VdpVram& vram_;

    unsigned sx_ = 0, sy_ = 0, dx_ = 0, dy_ = 0, nx_ = 0, ny_ = 0;
    unsigned asx_ = 0;
    std::uint8_t col_ = 0, arg_ = 0, cmd_ = 0;

    std::uint8_t status_ = 0;
    int scr_mode_ = -1;

    // Event-driven transfer state (LMCM/LMMC/HMMC only).
    // transfer_pending_ mirrors the reference's `transfer` flag
    // (VDPCmdEngine.cc:1856-1863 setCmdReg case 0x0C: ANY R#44/COL write
    // arms exactly one pending CPU->VRAM transfer unit -- even BEFORE the
    // command is issued -- while startLmmc/startHmmc deliberately do NOT arm
    // it themselves, VDPCmdEngine.cc:1303-1305/1732-1733 "do not set
    // 'transfer = true'", their bug#1014). A pre-armed unit is consumed as
    // the FIRST transferred unit when LMMC/HMMC starts: the MSX2+ boot logo
    // (HB-F1XV SUB-ROM) pre-loads the first pixel color into R#44, issues
    // LMMC, then sends only NX*NY-1 further writes -- without this flag the
    // command never completes and the BIOS spins forever on S#2 CE.
    bool transfer_pending_ = false;
    TransferKind transfer_kind_ = TransferKind::None;
    unsigned transfer_adx_ = 0, transfer_dy_ = 0;
    unsigned transfer_dx_start_ = 0;
    unsigned transfer_anx_ = 0, transfer_tmp_nx_ = 0, transfer_tmp_ny_ = 0;
    int transfer_tx_ = 1, transfer_ty_ = 1;
    LogicalOp transfer_op_ = LogicalOp::Imp;
    bool transfer_transparent_ = false;
};

}  // namespace sony_msx::devices::video
