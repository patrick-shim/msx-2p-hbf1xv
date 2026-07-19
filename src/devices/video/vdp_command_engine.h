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

// V9958/V9938 VDP command engine. Owned INSIDE
// V9958Vdp as a private member (mirrors the SpriteEngine/blink_countdown_
// ownership style), constructed with a reference to the SAME VdpVram the
// V9958Vdp owns -- command-engine writes land directly in the shared VRAM
// store, so VdpFrameRenderer's EXISTING background-rendering code paths see
// command-engine-written content with ZERO changes.
//
// Register file (SX/SY/DX/DY/NX/NY/COL/ARG/CMD + internal ASX/ADX/ANX
// trackers) is owned HERE, a SEPARATE storage from V9958Vdp::control_regs_
// (which stays at its existing 32 entries, unchanged).
//
// Execution model is HYBRID: 10
// commands (ABRT/STOP, POINT, SRCH, LINE, LMMV, LMMM, HMMV, HMMM, YMMM,
// PSET) execute ATOMICALLY -- the whole NX*NY operation completes
// synchronously within the call that writes CMD (R#46); CE reads 0
// immediately after that call returns. The other 3 (LMCM, LMMC, HMMC) need
// an EVENT-DRIVEN (never wall-clock/cycle-scheduled) multi-step state
// machine, mirroring this project's own FDC DRQ/INTRQ precedent
// (src/devices/fdc/wd2793.*): CE stays 1 across the whole transfer, which
// completes only after NX*NY separate CPU-port interactions (writes to
// R#44/COL, or reads of S#7) have each been serviced.
//
// Disclosed simplification (low risk, not exercised by any required test):
// unlike the reference, this engine does NOT mutate the SX/SY/DX/DY register
// members during command execution (the reference mutates DY/SY/ADX/ASX in
// place as a block command progresses -- a real but obscure hardware quirk
// visible only via non-standard mid-command register peeking). Coordinate
// advancement uses local working copies for the 10 atomic commands, and
// dedicated transfer_* members for the 3 event-driven commands; only ASX
// (exposed via S#8/S#9, used by SRCH and LMCM) is a genuine persistent
// member, matching the one case where real software can observe it
// (VDPCmdEngine.hh:104-114 getBorderX() -- "real VDP simply returns the
// current value of the ASX...counter, regardless of the command being
// executed").
//
// Grounding (behavior reference only, GPL isolation -- never copied):
// openMSX 21.0: src/video/VDPCmdEngine.{hh,cc}.
class VdpCommandEngine {
public:
    // A default-null per-destination-row observer: lets the owner render/commit
    // a row's display state BEFORE the command's writes land in it. The ATOMIC
    // command writers (run_lmmv/lmmm/hmmv/hmmm/ymmm/pset/
    // line) invoke it ONCE per destination row, BEFORE that row's VRAM writes,
    // carrying the destination page-row coordinate `dy` (full 10-bit; the
    // implementation derives the display line + page from live registers). This
    // is the structural analog of openMSX routing each command write through
    // VDPVRAM::writeCommon -> VRAMWindow::notify BEFORE committing the byte
    // ("Subsystem synchronisation should happen before the commit, to be able to
    // draw backlog using old state", openMSX 21.0: src/video/
    // VDPVRAM.hh:575-593, 309-322) -- read for behavior only, never copied.
    // (DEC-0065)
    //
    // A NULL sink (the default, and the only state for a bare command engine or
    // any unit test constructing one directly) means ZERO behavior change: the
    // writers execute byte-for-byte as before. The event-driven transfers
    // (LMCM/LMMC/HMMC) and the non-writing commands (POINT/SRCH) do NOT call it
    // -- transfers are already per-unit synced through the io_write seam
    // (v9958_vdp.cpp io_write), and POINT/SRCH write no bitmap VRAM.
    class CommandRowSink {
    public:
        virtual ~CommandRowSink() = default;
        // Called BEFORE the writes of destination page-row `dy` (10-bit VDP Y).
        virtual void on_dest_row(unsigned dy) = 0;
    };

    explicit VdpCommandEngine(VdpVram& vram) : vram_(vram) {}

    // Install (or detach with nullptr) the per-destination-row sink. The
    // pointer is externally owned; reset() deliberately does NOT clear it
    // (X-pattern of the V9958Vdp render-sync/observer wiring).
    void set_command_row_sink(CommandRowSink* sink) { row_sink_ = sink; }

    // R#32..R#46 register-write dispatch (index 0..14). Works identically
    // whether the caller reached it via the #99 two-write latch protocol or
    // the #9B indirect-register path -- both already route through
    // V9958Vdp::change_register(), which is the only caller.
    void write_register(int index, std::uint8_t value);
    // Debug/test peek of a command register (mirrors peekCmdReg).
    [[nodiscard]] std::uint8_t read_register(int index) const;

    // scrMode recompute, called from V9958Vdp::recompute_mode()
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

    // Diagnostic counters: how many times software read
    // the S#7 color register (every on_color_register_read call) and how many
    // LMCM commands were issued. Pure introspection; not part of emulated state
    // (reset() leaves them alone). Used to test whether the save-corruption path
    // ever exercises the LMCM/color-register read at all. (DEC-0072)
    [[nodiscard]] std::uint64_t color_read_count() const { return color_read_count_; }
    [[nodiscard]] std::uint64_t lmcm_start_count() const { return lmcm_start_count_; }

    // S#8/S#9: ASX (border-X working register), high byte masked by the
    // caller with | 0xFE. S#9 read
    // additionally clears BD (mirrors resetBD()).
    [[nodiscard]] unsigned border_x() const { return asx_; }
    void on_border_x_register_read();

    // Deterministic power-on reset.
    void reset();

    // The estimated emulated DURATION
    // (in CPU T-states) of the command most recently issued through
    // write_register(R#46). PURE function of the command parameters -- no clock,
    // no "now" -- so the value is fully unit-testable in isolation and the VDP
    // (which owns the clock) turns it into the S#2-bit0 CE busy window. Set at
    // command issue from the already-clipped work count (tmp_nx*tmp_ny for
    // rectangle ops; the drawn/searched pixel count for LINE/SRCH) times the
    // per-command access-slot cost, converted VDP-cycles -> T-states by ceil(/6).
    // 0 for ABRT/degenerate-no-op/POINT/PSET and the event-driven transfers
    // (LMCM/LMMC/HMMC are paced by their own held kCe + TR handshake). Grounding
    // (behavior reference only, never copied): openMSX VDPCmdEngine::calcFinishTime
    // (openMSX 21.0: src/video/VDPCmdEngine.cc:733-747) + the
    // per-command ticksPerPixel (:980,1105,1365,1470,1606). reset() clears it.
    // (DEC-0069)
    [[nodiscard]] std::uint64_t last_cmd_duration_tstates() const {
        return last_cmd_duration_tstates_;
    }

    // VDP command-timing parity (S#2 TR pacing): monotonic count of event-driven
    // transfer UNITS (LMMC/HMMC written pixels+bytes, LMCM read pixels) actually
    // serviced. The owning VDP snapshots this across a COL (R#44) write or an S#7
    // read; if it advanced, a unit was consumed and the VDP drop-then-rearms the
    // S#2 TR (transfer-ready) bit for transfer_unit_cost_tstates() T-states,
    // modelling the VRAM access slot the VDP consumes per unit. Pure pacing
    // introspection -- NOT emulated device state, so reset() deliberately leaves
    // it (only the delta across a single call pair is ever used).
    [[nodiscard]] std::uint64_t transfer_units_consumed() const { return transfer_units_consumed_; }
    // Per-unit T-state cost the TR bit drops for after each serviced transfer
    // unit (0 when no transfer active). Grounded in the byte/dot analog per-unit
    // tick cost; see the .cpp for the citation.
    [[nodiscard]] std::uint64_t transfer_unit_cost_tstates() const;

    // --- Debug snapshot: additive, read-only introspection of
    //     the register file + transfer FSM; zero behavior change. Enum
    //     fields return numeric
    //     codes to avoid exposing the private LogicalOp/TransferKind enums. ---
    [[nodiscard]] std::uint8_t status_byte() const { return status_; }
    [[nodiscard]] int scr_mode() const { return scr_mode_; }
    [[nodiscard]] unsigned sx() const { return sx_; }
    [[nodiscard]] unsigned sy() const { return sy_; }
    [[nodiscard]] unsigned dx() const { return dx_; }
    [[nodiscard]] unsigned dy() const { return dy_; }
    [[nodiscard]] unsigned nx() const { return nx_; }
    [[nodiscard]] unsigned ny() const { return ny_; }
    [[nodiscard]] unsigned asx() const { return asx_; }
    [[nodiscard]] std::uint8_t arg() const { return arg_; }
    [[nodiscard]] std::uint8_t cmd() const { return cmd_; }
    [[nodiscard]] bool transfer_pending() const { return transfer_pending_; }
    [[nodiscard]] std::uint8_t transfer_kind_code() const {
        return static_cast<std::uint8_t>(transfer_kind_);
    }
    [[nodiscard]] unsigned transfer_adx() const { return transfer_adx_; }
    [[nodiscard]] unsigned transfer_dy() const { return transfer_dy_; }
    [[nodiscard]] unsigned transfer_dx_start() const { return transfer_dx_start_; }
    [[nodiscard]] unsigned transfer_anx() const { return transfer_anx_; }
    [[nodiscard]] unsigned transfer_tmp_nx() const { return transfer_tmp_nx_; }
    [[nodiscard]] unsigned transfer_tmp_ny() const { return transfer_tmp_ny_; }
    [[nodiscard]] int transfer_tx() const { return transfer_tx_; }
    [[nodiscard]] int transfer_ty() const { return transfer_ty_; }
    [[nodiscard]] std::uint8_t transfer_op_code() const {
        return static_cast<std::uint8_t>(transfer_op_);
    }
    [[nodiscard]] bool transfer_transparent() const { return transfer_transparent_; }

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

    // Fire the per-destination-row sink (no-op when unset -> strict superset).
    void notify_dest_row(const unsigned dy) {
        if (row_sink_ != nullptr) {
            row_sink_->on_dest_row(dy);
        }
    }

    VdpVram& vram_;

    // Per-destination-row render-sync sink (default-null = inert).
    CommandRowSink* row_sink_ = nullptr;

    unsigned sx_ = 0, sy_ = 0, dx_ = 0, dy_ = 0, nx_ = 0, ny_ = 0;
    unsigned asx_ = 0;
    std::uint8_t col_ = 0, arg_ = 0, cmd_ = 0;

    std::uint8_t status_ = 0;
    int scr_mode_ = -1;

    // Diagnostic counters (see color_read_count()/lmcm_start_count(); DEC-0072).
    std::uint64_t color_read_count_ = 0;
    std::uint64_t lmcm_start_count_ = 0;

    // Estimated T-state duration of the last-issued command
    // (see last_cmd_duration_tstates()); recomputed on every execute_command().
    std::uint64_t last_cmd_duration_tstates_ = 0;

    // VDP command-timing parity: count of event-driven transfer units serviced
    // (see transfer_units_consumed()); a pure pacing hook, not reset() state.
    std::uint64_t transfer_units_consumed_ = 0;

    // Event-driven transfer state (LMCM/LMMC/HMMC only).
    // transfer_pending_ mirrors the reference's `transfer` flag
    // (VDPCmdEngine.cc:1856-1863 setCmdReg case 0x0C): ANY R#44/COL write
    // arms exactly one pending CPU->VRAM transfer unit -- even BEFORE the
    // command is issued -- while startLmmc/startHmmc deliberately do NOT arm
    // it themselves (VDPCmdEngine.cc:1303-1305/1732-1733 "do not set
    // 'transfer = true'", bug#1014). A pre-armed unit is consumed as
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
