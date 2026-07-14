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

#include "devices/video/vdp_command_engine.h"

#include <algorithm>

#include "devices/video/vdp_access_timing.h"
#include "devices/video/vdp_command_address.h"

namespace sony_msx::devices::video {

namespace {

// Per-command VDP-cycle cost, re-derived as sums/differences of the named
// VdpAccessDelta values ALREADY present + license-blessed in
// vdp_access_timing.h:72-88 (the header re-derived those as "hardware fact, not
// copied code", :65-71). A COMPOSITION of existing deltas -- NOT a transcription
// of openMSX's ~340-entry VDPAccessSlots.cc slot-position table (that is the
// deferred Phase-2b cycle-exact model + the license-sensitive-scope ban).
//
// TWO cost components per command (VDP command-timing hardware-parity
// correction):
//   (a) PER-UNIT (per pixel or byte) -- charged for every drawn unit; and
//   (b) PER-LINE end-of-line overhead -- charged once per completed row.
// Both are grounded in TIER-1 references/fact-sheets/Yamaha V9958 VDP.md §8 (the
// openMSX 2013 logic-analyzer table, "per pixel" and "per line" columns) and
// corroborated by openMSX VDPCmdEngine.cc's own Delta usage (behavior reference
// only, never copied), where the per-line overhead is exactly the end-of-line
// Delta minus the per-unit Delta:
//   HMMV  48/byte  (D48 write)               + 56/line  (D104-D48)   :1389/1391
//   YMMM  64/byte  (D24 read + D40 write)    +  0/line  ("does not
//                                               take extra time")    :1636/1654
//   HMMM  88/byte  (D24 read + D64 write)    + 64/line  (D128-D64)   :1499/1510
//   LMMV  96/pixel (D24 read + D72 write)    + 64/line  (D136-D72)   :1005/1016
//   LMMM 120/pixel (D32+D24 read + D64 wr)   + 64/line  (D128-D64)   :1133/1151
//   LINE 112/pixel (D24 read + D88 write)    + 32/minor-Bresenham
//                                               step     (D120-D88)  :900/909/924
//   SRCH  88/searched-px (D88 read)          (no per-line term)      :862
// NOTE (the correction): the pre-correction constants used LINE=88/px (dropping
// the D24 destination read) and omitted EVERY per-line term -- i.e. they matched
// only openMSX's deliberate calcFinishTime UNDER-estimate (:741-743 "assumes ...
// there's no overhead per line"), NOT the real per-Delta execution, so tall/thin
// blits reported ~2x too fast. Both are fixed here.
using vdp_access_timing::VdpAccessDelta;
constexpr int kD(const VdpAccessDelta delta) { return static_cast<int>(delta); }

// (a) per-unit costs
constexpr int kTicksHmmv = kD(VdpAccessDelta::D48);                           // 48  / byte
constexpr int kTicksYmmm = kD(VdpAccessDelta::D24) + kD(VdpAccessDelta::D40); // 64  / byte
constexpr int kTicksHmmm = kD(VdpAccessDelta::D24) + kD(VdpAccessDelta::D64); // 88  / byte
constexpr int kTicksLmmv = kD(VdpAccessDelta::D72) + kD(VdpAccessDelta::D24); // 96  / pixel
constexpr int kTicksLmmm =
    kD(VdpAccessDelta::D64) + kD(VdpAccessDelta::D32) + kD(VdpAccessDelta::D24); // 120 / pixel
constexpr int kTicksLine = kD(VdpAccessDelta::D88) + kD(VdpAccessDelta::D24); // 112 / pixel
constexpr int kTicksSrch = kD(VdpAccessDelta::D88);                           // 88  / searched px

// (b) per-line end-of-line overhead. Charged (tmp_ny - 1) times: openMSX applies
// the end-of-line Delta at every row boundary EXCEPT the last (commandDone breaks
// before that delta; VDPCmdEngine.cc:1394/1513/1019/1154).
constexpr int kPerLineHmmv = kD(VdpAccessDelta::D104) - kD(VdpAccessDelta::D48);  // 56
constexpr int kPerLineYmmm = 0;                                                  //  0
constexpr int kPerLineHmmm = kD(VdpAccessDelta::D128) - kD(VdpAccessDelta::D64);  // 64
constexpr int kPerLineLmmv = kD(VdpAccessDelta::D136) - kD(VdpAccessDelta::D72);  // 64
constexpr int kPerLineLmmm = kD(VdpAccessDelta::D128) - kD(VdpAccessDelta::D64);  // 64

// LINE charges +32 per MINOR Bresenham step (the minor-axis advance), NOT per
// line (fact sheet §8 LINE "per line" column note; openMSX D120-D88, :924/947).
constexpr int kTicksLineMinorStep = kD(VdpAccessDelta::D120) - kD(VdpAccessDelta::D88);  // 32

// ceil(vdp_cycles / 6): VDP cycles -> CPU T-states, the /6 being the project's
// fact-sheet-grounded kCpuTstatesPerVdpCycleRatio (vdp_access_timing.h:93-95).
constexpr std::uint64_t tstates_from_vdp_cycles(const std::uint64_t vdp_cycles) {
    const auto ratio = static_cast<std::uint64_t>(vdp_access_timing::kCpuTstatesPerVdpCycleRatio);
    return (vdp_cycles + ratio - 1) / ratio;
}

// Per-pixel-only work count (SRCH's searched pixels; also LINE's base term).
std::uint64_t duration_tstates(const int ticks_per_unit, const std::uint64_t work_units) {
    return tstates_from_vdp_cycles(static_cast<std::uint64_t>(ticks_per_unit) * work_units);
}

// Rectangle/fill commands. The per-unit term reduces (at ANX==tmp_nx) to
// openMSX's calcFinishTime work count tmp_nx*tmp_ny - 1 (VDPCmdEngine.cc:745);
// the PER-LINE term (missing pre-correction) adds the end-of-line overhead
// (tmp_ny - 1) times. Both accumulate as VDP cycles and convert ONCE (single
// ceil). Degenerate (either count 0) => 0 (no busy window).
std::uint64_t rect_duration_tstates(const int ticks_per_unit, const int per_line_ticks,
                                    const unsigned tmp_nx, const unsigned tmp_ny) {
    const std::uint64_t units =
        static_cast<std::uint64_t>(tmp_nx) * static_cast<std::uint64_t>(tmp_ny);
    if (units == 0) {
        return 0;
    }
    const std::uint64_t vdp_cycles =
        static_cast<std::uint64_t>(ticks_per_unit) * (units - 1) +
        static_cast<std::uint64_t>(per_line_ticks) * static_cast<std::uint64_t>(tmp_ny - 1);
    return tstates_from_vdp_cycles(vdp_cycles);
}

// Per-scrMode addressing/packing traits (VDPCmdEngine.cc:155-410's
// Graphic4Mode/Graphic5Mode/Graphic6Mode/Graphic7Mode/NonBitmapMode structs),
// re-expressed as a runtime table keyed by scrMode instead of the
// reference's compile-time template parameter -- behaviorally equivalent,
// simpler to dispatch on a value computed from R#0/R#1/R#25 at runtime.
// `address_of` is one of the five vdp_command_address.h functions (the
// D7-closing piece, §1.5), never V9958Vdp::effective_address().
struct ModeTraits {
    std::uint8_t color_mask;
    std::uint8_t pixels_per_byte;
    std::uint8_t pixels_per_byte_shift;
    unsigned pixels_per_line;
    std::uint32_t (*address_of)(unsigned, unsigned);
};

constexpr ModeTraits kModeTraits[5] = {
    {0x0F, 2, 1, 256, &graphic4_command_address},    // scrMode 0: GRAPHIC4
    {0x03, 4, 2, 512, &graphic5_command_address},    // scrMode 1: GRAPHIC5
    {0x0F, 2, 1, 512, &graphic6_command_address},    // scrMode 2: GRAPHIC6
    {0xFF, 1, 0, 256, &graphic7_command_address},    // scrMode 3: GRAPHIC7 (+YJK/YJK+YAE)
    {0xFF, 1, 0, 256, &non_bitmap_command_address},  // scrMode 4: NonBitmap (R#25 CMD bit)
};

const ModeTraits& traits_for(const int scr_mode) {
    return kModeTraits[static_cast<std::size_t>(scr_mode)];
}

unsigned wrap_step(const unsigned v, const int delta) {
    return static_cast<unsigned>(static_cast<int>(v) + delta);
}

// clipNX_1_pixel/clipNX_1_byte/clipNX_2_pixel/clipNX_2_byte/clipNY_1/clipNY_2
// (VDPCmdEngine.cc:56-126), independently re-expressed with a plain `bool`
// direction flag instead of the raw ARG byte + bit constant.
unsigned clip_nx_1_pixel(unsigned dx, unsigned nx, const bool dix, const unsigned pixels_per_line) {
    if (dx >= pixels_per_line) return 1;
    nx = nx ? nx : pixels_per_line;
    return dix ? std::min(nx, dx + 1) : std::min(nx, pixels_per_line - dx);
}

unsigned clip_nx_1_byte(unsigned dx, unsigned nx, const bool dix, const unsigned pixels_per_line,
                         const unsigned shift) {
    const unsigned bytes_per_line = pixels_per_line >> shift;
    dx >>= shift;
    if (bytes_per_line <= dx) return 1;
    nx >>= shift;
    nx = nx ? nx : bytes_per_line;
    return dix ? std::min(nx, dx + 1) : std::min(nx, bytes_per_line - dx);
}

unsigned clip_nx_2_pixel(unsigned sx, unsigned dx, unsigned nx, const bool dix, const unsigned pixels_per_line) {
    if (sx >= pixels_per_line || dx >= pixels_per_line) return 1;
    nx = nx ? nx : pixels_per_line;
    return dix ? std::min(nx, std::min(sx, dx) + 1) : std::min(nx, pixels_per_line - std::max(sx, dx));
}

unsigned clip_nx_2_byte(unsigned sx, unsigned dx, unsigned nx, const bool dix, const unsigned pixels_per_line,
                         const unsigned shift) {
    const unsigned bytes_per_line = pixels_per_line >> shift;
    sx >>= shift;
    dx >>= shift;
    if (bytes_per_line <= sx || bytes_per_line <= dx) return 1;
    nx >>= shift;
    nx = nx ? nx : bytes_per_line;
    return dix ? std::min(nx, std::min(sx, dx) + 1) : std::min(nx, bytes_per_line - std::max(sx, dx));
}

unsigned clip_ny_1(const unsigned dy, unsigned ny, const bool diy) {
    ny = ny ? ny : 1024;
    return diy ? std::min(ny, dy + 1) : ny;
}

unsigned clip_ny_2(const unsigned sy, const unsigned dy, unsigned ny, const bool diy) {
    ny = ny ? ny : 1024;
    return diy ? std::min(ny, std::min(sy, dy) + 1) : ny;
}

}  // namespace

// --- register file -----------------------------------------------------

void VdpCommandEngine::write_register(const int index, const std::uint8_t value) {
    switch (index) {
    case 0: sx_ = (sx_ & 0x100u) | value; break;
    case 1: sx_ = (sx_ & 0x0FFu) | (static_cast<unsigned>(value & 0x01u) << 8); break;
    case 2: sy_ = (sy_ & 0x300u) | value; break;
    case 3: sy_ = (sy_ & 0x0FFu) | (static_cast<unsigned>(value & 0x03u) << 8); break;
    case 4: dx_ = (dx_ & 0x100u) | value; break;
    case 5: dx_ = (dx_ & 0x0FFu) | (static_cast<unsigned>(value & 0x01u) << 8); break;
    case 6: dy_ = (dy_ & 0x300u) | value; break;
    case 7: dy_ = (dy_ & 0x0FFu) | (static_cast<unsigned>(value & 0x03u) << 8); break;
    case 8: nx_ = (nx_ & 0x300u) | value; break;
    case 9: nx_ = (nx_ & 0x0FFu) | (static_cast<unsigned>(value & 0x03u) << 8); break;
    case 10: ny_ = (ny_ & 0x300u) | value; break;
    case 11: ny_ = (ny_ & 0x0FFu) | (static_cast<unsigned>(value & 0x03u) << 8); break;
    case 12:  // R#44 COL
        col_ = value;
        // Any COL write arms one pending transfer unit, even before a
        // command is issued (VDPCmdEngine.cc:1856-1863 `transfer = true` --
        // the MSX2+ boot-logo LMMC protocol pre-loads its first pixel this
        // way; see transfer_pending_'s doc comment in the header).
        transfer_pending_ = true;
        if ((transfer_kind_ == TransferKind::Lmmc || transfer_kind_ == TransferKind::Hmmc) && ce()) {
            perform_transfer_step();
        } else if (!ce()) {
            status_ = static_cast<std::uint8_t>(status_ & ~kTr);
        }
        break;
    case 13:  // R#45 ARG
        arg_ = value;
        break;
    case 14:  // R#46 CMD
        cmd_ = value;
        execute_command();
        break;
    default:
        break;
    }
}

std::uint8_t VdpCommandEngine::read_register(const int index) const {
    switch (index) {
    case 0: return static_cast<std::uint8_t>(sx_ & 0xFF);
    case 1: return static_cast<std::uint8_t>(sx_ >> 8);
    case 2: return static_cast<std::uint8_t>(sy_ & 0xFF);
    case 3: return static_cast<std::uint8_t>(sy_ >> 8);
    case 4: return static_cast<std::uint8_t>(dx_ & 0xFF);
    case 5: return static_cast<std::uint8_t>(dx_ >> 8);
    case 6: return static_cast<std::uint8_t>(dy_ & 0xFF);
    case 7: return static_cast<std::uint8_t>(dy_ >> 8);
    case 8: return static_cast<std::uint8_t>(nx_ & 0xFF);
    case 9: return static_cast<std::uint8_t>(nx_ >> 8);
    case 10: return static_cast<std::uint8_t>(ny_ & 0xFF);
    case 11: return static_cast<std::uint8_t>(ny_ >> 8);
    case 12: return col_;
    case 13: return arg_;
    case 14: return cmd_;
    default: return 0xFF;
    }
}

void VdpCommandEngine::notify_mode_change(const VdpModeState& mode, const bool cmd_bit) {
    int new_scr_mode;
    switch (mode.base) {
    case 0x0C: new_scr_mode = 0; break;  // GRAPHIC4
    case 0x10: new_scr_mode = 1; break;  // GRAPHIC5
    case 0x14: new_scr_mode = 2; break;  // GRAPHIC6
    case 0x1C: new_scr_mode = 3; break;  // GRAPHIC7 (+YJK/YJK+YAE, same base)
    default: new_scr_mode = cmd_bit ? 4 : -1; break;
    }
    if (new_scr_mode != scr_mode_) {
        if (ce() && new_scr_mode < 0) {
            // Losing command legality entirely aborts any in-progress
            // command (VDPCmdEngine.cc:1924-1935).
            command_done();
            transfer_kind_ = TransferKind::None;
        }
        scr_mode_ = new_scr_mode;
    }
}

void VdpCommandEngine::reset() {
    sx_ = sy_ = dx_ = dy_ = nx_ = ny_ = 0;
    asx_ = 0;
    col_ = arg_ = cmd_ = 0;
    status_ = 0;
    scr_mode_ = -1;
    last_cmd_duration_tstates_ = 0;
    transfer_pending_ = false;
    transfer_kind_ = TransferKind::None;
    transfer_adx_ = transfer_dy_ = transfer_dx_start_ = 0;
    transfer_anx_ = transfer_tmp_nx_ = transfer_tmp_ny_ = 0;
    transfer_tx_ = transfer_ty_ = 1;
    transfer_op_ = LogicalOp::Imp;
    transfer_transparent_ = false;
}

// --- status side effects -------------------------------------------------

void VdpCommandEngine::on_color_register_read() {
    ++color_read_count_;  // DEC-0072 diagnostic: count every S#7 color-register read
    if (transfer_kind_ != TransferKind::Lmcm || !ce()) {
        return;
    }
    ++transfer_units_consumed_;  // one LMCM read unit serviced (VDP TR-pacing hook)
    col_ = point_pixel(scr_mode_, asx_, transfer_dy_, vram_);
    asx_ = wrap_step(asx_, transfer_tx_);
    if (--transfer_anx_ == 0) {
        transfer_dy_ = wrap_step(transfer_dy_, transfer_ty_);
        --transfer_tmp_ny_;
        asx_ = transfer_dx_start_;
        transfer_anx_ = transfer_tmp_nx_;
        if (transfer_tmp_ny_ == 0) {
            command_done();
            transfer_kind_ = TransferKind::None;
            status_ = static_cast<std::uint8_t>(status_ & ~kTr);
        }
    }
}

void VdpCommandEngine::on_border_x_register_read() {
    status_ = static_cast<std::uint8_t>(status_ & ~kBd);
}

// --- pixel-level helpers (shared by POINT/PSET/SRCH/LINE/LMMV/LMMM/LMCM/LMMC) ---

VdpCommandEngine::DecodedOp VdpCommandEngine::decode_op(const std::uint8_t nibble) {
    const std::uint8_t base = static_cast<std::uint8_t>(nibble & 0x07);
    const bool transparent = (nibble & 0x08) != 0;
    if (base > 4) {
        return DecodedOp{LogicalOp::Dummy, transparent};
    }
    return DecodedOp{static_cast<LogicalOp>(base), transparent};
}

std::uint8_t VdpCommandEngine::point_pixel(const int scr_mode, const unsigned x, const unsigned y,
                                           const VdpVram& vram) {
    const ModeTraits& t = traits_for(scr_mode);
    const std::uint32_t addr = t.address_of(x, y) & 0x1FFFFu;
    const std::uint8_t byte = vram.read(addr);
    if (t.pixels_per_byte == 1) {
        return byte;
    }
    const int bits = 8 / t.pixels_per_byte;
    const int shift = static_cast<int>((~x) & static_cast<unsigned>(t.pixels_per_byte - 1)) * bits;
    return static_cast<std::uint8_t>((byte >> shift) & ((1u << bits) - 1u));
}

void VdpCommandEngine::write_pixel(const int scr_mode, const unsigned x, const unsigned y,
                                   const std::uint8_t color, const LogicalOp op, const bool transparent,
                                   VdpVram& vram) {
    if (op == LogicalOp::Dummy) {
        return;  // undefined low-nibble: no VRAM access at all (DummyOp).
    }
    const ModeTraits& t = traits_for(scr_mode);
    const int bits = 8 / t.pixels_per_byte;
    const std::uint8_t pixel_mask = static_cast<std::uint8_t>((1u << bits) - 1u);
    const std::uint8_t src_pixel = static_cast<std::uint8_t>(color & pixel_mask);
    if (transparent && src_pixel == 0) {
        return;  // TransparentOp<Op>: "if (color) op(...)" -- skip entirely.
    }
    const std::uint32_t addr = t.address_of(x, y) & 0x1FFFFu;
    const int shift = static_cast<int>((~x) & static_cast<unsigned>(t.pixels_per_byte - 1)) * bits;
    const std::uint8_t old_byte = vram.read(addr);
    const std::uint8_t old_pixel = static_cast<std::uint8_t>((old_byte >> shift) & pixel_mask);
    std::uint8_t new_pixel;
    switch (op) {
    case LogicalOp::Imp: new_pixel = src_pixel; break;
    case LogicalOp::And: new_pixel = static_cast<std::uint8_t>(old_pixel & src_pixel); break;
    case LogicalOp::Or: new_pixel = static_cast<std::uint8_t>(old_pixel | src_pixel); break;
    case LogicalOp::Xor: new_pixel = static_cast<std::uint8_t>(old_pixel ^ src_pixel); break;
    case LogicalOp::Not: new_pixel = static_cast<std::uint8_t>((~src_pixel) & pixel_mask); break;
    default: return;
    }
    const std::uint8_t new_byte = static_cast<std::uint8_t>(
        (old_byte & static_cast<std::uint8_t>(~(pixel_mask << shift))) |
        static_cast<std::uint8_t>(new_pixel << shift));
    vram.write(addr, new_byte);
}

// --- command dispatch ------------------------------------------------------

void VdpCommandEngine::execute_command() {
    // M44 Phase 2a: default this command's reported duration to 0 (instant).
    // Every atomic rectangle/LINE/SRCH command below overwrites it from its own
    // work count; ABRT, degenerate no-ops, POINT/PSET, illegal-mode, and the
    // event-driven transfers all leave it 0 (paced by their own mechanism).
    last_cmd_duration_tstates_ = 0;
    if (scr_mode_ < 0) {
        // No commands possible at all (A-M22-6): writing CMD immediately
        // behaves as ABRT (VDPCmdEngine.cc:1944-1947).
        command_done();
        transfer_kind_ = TransferKind::None;
        return;
    }
    status_ = static_cast<std::uint8_t>(status_ | kCe);
    const std::uint8_t code = static_cast<std::uint8_t>(cmd_ >> 4);
    switch (code) {
    case 0x0: case 0x1: case 0x2: case 0x3:  // ABRT/STOP (4 aliased reserved code-points)
        command_done();
        transfer_kind_ = TransferKind::None;
        break;
    case 0x4: run_point(); break;
    case 0x5: run_pset(); break;
    case 0x6: run_srch(); break;
    case 0x7: run_line(); break;
    case 0x8: run_lmmv(); break;
    case 0x9: run_lmmm(); break;
    case 0xA: start_lmcm(); break;
    case 0xB: start_lmmc(); break;
    case 0xC: run_hmmv(); break;
    case 0xD: run_hmmm(); break;
    case 0xE: run_ymmm(); break;
    case 0xF: start_hmmc(); break;
    default: command_done(); break;
    }
}

void VdpCommandEngine::command_done() {
    status_ = static_cast<std::uint8_t>(status_ & ~kCe);
    cmd_ = 0;
}

// --- atomic commands: no logical op (POINT/SRCH/HMMV/HMMM/YMMM) -----------

void VdpCommandEngine::run_point() {
    col_ = point_pixel(scr_mode_, sx_, sy_, vram_);
    command_done();
}

void VdpCommandEngine::run_srch() {
    const ModeTraits& t = traits_for(scr_mode_);
    const std::uint8_t cl = static_cast<std::uint8_t>(col_ & t.color_mask);
    const int tx = (arg_ & kDix) ? -1 : 1;
    const bool aeq = (arg_ & kEq) != 0;
    asx_ = sx_;
    std::uint64_t searched = 0;  // M44 Phase 2a: examined-pixel count
    for (;;) {
        ++searched;
        const std::uint8_t p = point_pixel(scr_mode_, asx_, sy_, vram_);
        if ((p == cl) != aeq) {
            status_ = static_cast<std::uint8_t>(status_ | kBd);  // border detected
            break;
        }
        asx_ = wrap_step(asx_, tx);
        if ((asx_ & t.pixels_per_line) != 0) {
            // Left/right border hit: command terminates WITHOUT touching BD
            // (VDPCmdEngine.cc:857-861, "this does NOT reset the BD flag").
            break;
        }
    }
    last_cmd_duration_tstates_ = duration_tstates(kTicksSrch, searched);  // M44 Phase 2a
    command_done();
}

void VdpCommandEngine::run_hmmv() {
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    unsigned tmp_nx = clip_nx_1_byte(dx_, nx_, (arg_ & kDix) != 0, t.pixels_per_line, t.pixels_per_byte_shift);
    unsigned tmp_ny = clip_ny_1(dy_, ny_, (arg_ & kDiy) != 0);
    const int tx = (arg_ & kDix) ? -static_cast<int>(t.pixels_per_byte) : static_cast<int>(t.pixels_per_byte);
    const int ty = (arg_ & kDiy) ? -1 : 1;
    unsigned adx = dx_;
    unsigned dy = dy_;
    unsigned anx = tmp_nx;
    if (tmp_nx == 0 || tmp_ny == 0) {
        command_done();
        return;
    }
    last_cmd_duration_tstates_ = rect_duration_tstates(kTicksHmmv, kPerLineHmmv, tmp_nx, tmp_ny);
    notify_dest_row(dy);  // M44: render-sync BEFORE this row's writes
    for (;;) {
        vram_.write(t.address_of(adx, dy) & 0x1FFFFu, col_);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            dy = wrap_step(dy, ty);
            --tmp_ny;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
            notify_dest_row(dy);  // M44: render-sync BEFORE the next row's writes
        }
    }
    command_done();
}

void VdpCommandEngine::run_hmmm() {
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    unsigned tmp_nx = clip_nx_2_byte(sx_, dx_, nx_, dix, t.pixels_per_line, t.pixels_per_byte_shift);
    unsigned tmp_ny = clip_ny_2(sy_, dy_, ny_, diy);
    const int step = static_cast<int>(t.pixels_per_byte);
    const int tx = dix ? -step : step;
    const int ty = diy ? -1 : 1;
    unsigned asx = sx_, adx = dx_, sy = sy_, dy = dy_, anx = tmp_nx;
    if (tmp_nx == 0 || tmp_ny == 0) {
        command_done();
        return;
    }
    last_cmd_duration_tstates_ = rect_duration_tstates(kTicksHmmm, kPerLineHmmm, tmp_nx, tmp_ny);
    notify_dest_row(dy);  // M44: render-sync BEFORE this row's writes
    for (;;) {
        const std::uint8_t byte = vram_.read(t.address_of(asx, sy) & 0x1FFFFu);
        vram_.write(t.address_of(adx, dy) & 0x1FFFFu, byte);
        asx = wrap_step(asx, tx);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            sy = wrap_step(sy, ty);
            dy = wrap_step(dy, ty);
            --tmp_ny;
            asx = sx_;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
            notify_dest_row(dy);  // M44: render-sync BEFORE the next row's writes
        }
    }
    command_done();
}

void VdpCommandEngine::run_ymmm() {
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    // "large enough that it gets clipped" (VDPCmdEngine.cc:1600-1601).
    unsigned tmp_nx = clip_nx_1_byte(dx_, 512u, dix, t.pixels_per_line, t.pixels_per_byte_shift);
    unsigned tmp_ny = clip_ny_2(sy_, dy_, ny_, diy);
    const int step = static_cast<int>(t.pixels_per_byte);
    const int tx = dix ? -step : step;
    const int ty = diy ? -1 : 1;
    unsigned adx = dx_, sy = sy_, dy = dy_, anx = tmp_nx;
    if (tmp_nx == 0 || tmp_ny == 0) {
        command_done();
        return;
    }
    last_cmd_duration_tstates_ = rect_duration_tstates(kTicksYmmm, kPerLineYmmm, tmp_nx, tmp_ny);
    notify_dest_row(dy);  // M44: render-sync BEFORE this row's writes
    for (;;) {
        const std::uint8_t byte = vram_.read(t.address_of(adx, sy) & 0x1FFFFu);
        vram_.write(t.address_of(adx, dy) & 0x1FFFFu, byte);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            sy = wrap_step(sy, ty);
            dy = wrap_step(dy, ty);
            --tmp_ny;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
            notify_dest_row(dy);  // M44: render-sync BEFORE the next row's writes
        }
    }
    command_done();
}

// --- atomic commands: with logical op (PSET/LINE/LMMV/LMMM) ---------------

void VdpCommandEngine::run_pset() {
    const DecodedOp decoded = decode_op(static_cast<std::uint8_t>(cmd_ & 0x0F));
    const ModeTraits& t = traits_for(scr_mode_);
    const std::uint8_t cl = static_cast<std::uint8_t>(col_ & t.color_mask);
    notify_dest_row(dy_);  // M44: render-sync BEFORE the single PSET write
    write_pixel(scr_mode_, dx_, dy_, cl, decoded.op, decoded.transparent, vram_);
    command_done();
}

void VdpCommandEngine::run_line() {
    const DecodedOp decoded = decode_op(static_cast<std::uint8_t>(cmd_ & 0x0F));
    const ModeTraits& t = traits_for(scr_mode_);
    const std::uint8_t cl = static_cast<std::uint8_t>(col_ & t.color_mask);
    const int tx = (arg_ & kDix) ? -1 : 1;
    const int ty = (arg_ & kDiy) ? -1 : 1;
    const bool major_y = (arg_ & kMaj) != 0;
    ny_ &= 1023u;
    const unsigned ny_limit = ny_;
    unsigned asx = (nx_ - 1u) >> 1;  // unsigned; NX==0 is a pathological, unused input
    unsigned adx = dx_;
    unsigned dy = dy_;
    unsigned anx = 0;

    // M44: LINE advances dy per PIXEL; the destination-row sink must fire only
    // when dy actually changes (R-5), BEFORE that row's write. sync_to_line is
    // idempotent so a redundant equal-line call would be harmless anyway.
    bool row_notified = false;
    unsigned last_notified_dy = 0;
    std::uint64_t line_pixels = 0;  // M44 Phase 2a: drawn major-axis pixel count
    std::uint64_t minor_steps = 0;  // minor-axis (Bresenham) advances (+32 VDP cycles each)

    for (int guard = 0; guard < 1'000'000; ++guard) {
        if (!row_notified || dy != last_notified_dy) {
            notify_dest_row(dy);
            last_notified_dy = dy;
            row_notified = true;
        }
        write_pixel(scr_mode_, adx, dy, cl, decoded.op, decoded.transparent, vram_);
        ++line_pixels;

        bool done = false;
        if (!major_y) {
            adx = wrap_step(adx, tx);
            const bool anx_hit = (anx == nx_);
            ++anx;
            if (anx_hit || (adx & t.pixels_per_line) != 0) {
                done = true;
            } else {
                if (asx < ny_limit) {
                    asx += nx_;
                    dy = wrap_step(dy, ty);
                    ++minor_steps;
                    if (ty < 0 && static_cast<std::int32_t>(dy) < 0) done = true;
                }
                asx = (asx - ny_limit) & 1023u;
            }
        } else {
            dy = wrap_step(dy, ty);
            if (ty < 0 && static_cast<std::int32_t>(dy) < 0) {
                done = true;
            } else {
                if (asx < ny_limit) {
                    asx += nx_;
                    adx = wrap_step(adx, tx);
                    ++minor_steps;
                }
                asx = (asx - ny_limit) & 1023u;
                const bool anx_hit = (anx == nx_);
                ++anx;
                if (anx_hit || (adx & t.pixels_per_line) != 0) done = true;
            }
        }
        if (done) break;
    }
    // VDP command-timing parity: 112/px base (D24 dst-read + D88 write) plus +32
    // per minor Bresenham step (D120-D88). Same "final unit's own slot uncounted"
    // convention as rect_duration_tstates (per-unit * (line_pixels - 1)).
    const std::uint64_t line_vdp_cycles =
        static_cast<std::uint64_t>(kTicksLine) * (line_pixels != 0 ? line_pixels - 1 : 0) +
        static_cast<std::uint64_t>(kTicksLineMinorStep) * minor_steps;
    last_cmd_duration_tstates_ = tstates_from_vdp_cycles(line_vdp_cycles);
    command_done();
}

void VdpCommandEngine::run_lmmv() {
    const DecodedOp decoded = decode_op(static_cast<std::uint8_t>(cmd_ & 0x0F));
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    unsigned tmp_nx = clip_nx_1_pixel(dx_, nx_, dix, t.pixels_per_line);
    unsigned tmp_ny = clip_ny_1(dy_, ny_, diy);
    const int tx = dix ? -1 : 1;
    const int ty = diy ? -1 : 1;
    const std::uint8_t cl = static_cast<std::uint8_t>(col_ & t.color_mask);
    unsigned adx = dx_, dy = dy_, anx = tmp_nx;
    if (tmp_nx == 0 || tmp_ny == 0) {
        command_done();
        return;
    }
    last_cmd_duration_tstates_ = rect_duration_tstates(kTicksLmmv, kPerLineLmmv, tmp_nx, tmp_ny);
    notify_dest_row(dy);  // M44: render-sync BEFORE this row's writes
    for (;;) {
        write_pixel(scr_mode_, adx, dy, cl, decoded.op, decoded.transparent, vram_);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            dy = wrap_step(dy, ty);
            --tmp_ny;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
            notify_dest_row(dy);  // M44: render-sync BEFORE the next row's writes
        }
    }
    command_done();
}

void VdpCommandEngine::run_lmmm() {
    const DecodedOp decoded = decode_op(static_cast<std::uint8_t>(cmd_ & 0x0F));
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    unsigned tmp_nx = clip_nx_2_pixel(sx_, dx_, nx_, dix, t.pixels_per_line);
    unsigned tmp_ny = clip_ny_2(sy_, dy_, ny_, diy);
    const int tx = dix ? -1 : 1;
    const int ty = diy ? -1 : 1;
    unsigned asx = sx_, adx = dx_, sy = sy_, dy = dy_, anx = tmp_nx;
    if (tmp_nx == 0 || tmp_ny == 0) {
        command_done();
        return;
    }
    last_cmd_duration_tstates_ = rect_duration_tstates(kTicksLmmm, kPerLineLmmm, tmp_nx, tmp_ny);
    notify_dest_row(dy);  // M44: render-sync BEFORE this row's writes
    for (;;) {
        const std::uint8_t src = point_pixel(scr_mode_, asx, sy, vram_);
        write_pixel(scr_mode_, adx, dy, src, decoded.op, decoded.transparent, vram_);
        asx = wrap_step(asx, tx);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            sy = wrap_step(sy, ty);
            dy = wrap_step(dy, ty);
            --tmp_ny;
            asx = sx_;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
            notify_dest_row(dy);  // M44: render-sync BEFORE the next row's writes
        }
    }
    command_done();
}

// --- event-driven transfer commands: LMCM/LMMC/HMMC -----------------------

void VdpCommandEngine::start_lmcm() {
    ++lmcm_start_count_;  // DEC-0072 diagnostic: count every LMCM command issue
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    transfer_tmp_nx_ = clip_nx_1_pixel(sx_, nx_, dix, t.pixels_per_line);
    transfer_tmp_ny_ = clip_ny_1(sy_, ny_, diy);
    transfer_tx_ = dix ? -1 : 1;
    transfer_ty_ = diy ? -1 : 1;
    transfer_dx_start_ = sx_;
    asx_ = sx_;
    transfer_dy_ = sy_;
    transfer_anx_ = transfer_tmp_nx_;
    transfer_kind_ = TransferKind::Lmcm;
    status_ = static_cast<std::uint8_t>(status_ | kTr);
    if (transfer_tmp_nx_ == 0 || transfer_tmp_ny_ == 0) {
        command_done();
        transfer_kind_ = TransferKind::None;
    }
}

void VdpCommandEngine::start_lmmc() {
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    transfer_tmp_nx_ = clip_nx_1_pixel(dx_, nx_, dix, t.pixels_per_line);
    transfer_tmp_ny_ = clip_ny_1(dy_, ny_, diy);
    transfer_tx_ = dix ? -1 : 1;
    transfer_ty_ = diy ? -1 : 1;
    transfer_dx_start_ = dx_;
    transfer_adx_ = dx_;
    transfer_dy_ = dy_;
    transfer_anx_ = transfer_tmp_nx_;
    const DecodedOp decoded = decode_op(static_cast<std::uint8_t>(cmd_ & 0x0F));
    transfer_op_ = decoded.op;
    transfer_transparent_ = decoded.transparent;
    transfer_kind_ = TransferKind::Lmmc;
    status_ = static_cast<std::uint8_t>(status_ | kTr);
    if (transfer_tmp_nx_ == 0 || transfer_tmp_ny_ == 0) {
        command_done();
        transfer_kind_ = TransferKind::None;
        return;
    }
    // Consume a pre-armed COL unit as the FIRST pixel (bug#1014 semantics:
    // start itself never arms, but a COL value written BEFORE the command
    // is one pending unit -- the MSX2+ boot-logo protocol).
    if (transfer_pending_) {
        perform_transfer_step();
    }
}

void VdpCommandEngine::start_hmmc() {
    const ModeTraits& t = traits_for(scr_mode_);
    ny_ &= 1023u;
    const bool dix = (arg_ & kDix) != 0;
    const bool diy = (arg_ & kDiy) != 0;
    transfer_tmp_nx_ = clip_nx_1_byte(dx_, nx_, dix, t.pixels_per_line, t.pixels_per_byte_shift);
    transfer_tmp_ny_ = clip_ny_1(dy_, ny_, diy);
    const int step = static_cast<int>(t.pixels_per_byte);
    transfer_tx_ = dix ? -step : step;
    transfer_ty_ = diy ? -1 : 1;
    transfer_dx_start_ = dx_;
    transfer_adx_ = dx_;
    transfer_dy_ = dy_;
    transfer_anx_ = transfer_tmp_nx_;
    transfer_kind_ = TransferKind::Hmmc;
    status_ = static_cast<std::uint8_t>(status_ | kTr);
    if (transfer_tmp_nx_ == 0 || transfer_tmp_ny_ == 0) {
        command_done();
        transfer_kind_ = TransferKind::None;
        return;
    }
    // Same pre-armed-COL first-byte consumption as start_lmmc() (bug#1014
    // semantics; VDPCmdEngine.cc:1732-1733 "see startLmmc()").
    if (transfer_pending_) {
        perform_transfer_step();
    }
}

void VdpCommandEngine::perform_transfer_step() {
    transfer_pending_ = false;  // consume the armed unit (reference `transfer = false`)
    const ModeTraits& t = traits_for(scr_mode_);
    if (transfer_kind_ == TransferKind::Lmmc) {
        const std::uint8_t cl = static_cast<std::uint8_t>(col_ & t.color_mask);
        write_pixel(scr_mode_, transfer_adx_, transfer_dy_, cl, transfer_op_, transfer_transparent_, vram_);
    } else if (transfer_kind_ == TransferKind::Hmmc) {
        vram_.write(t.address_of(transfer_adx_, transfer_dy_) & 0x1FFFFu, col_);
    } else {
        return;
    }
    ++transfer_units_consumed_;  // one LMMC/HMMC write unit serviced (VDP TR-pacing hook)
    transfer_adx_ = wrap_step(transfer_adx_, transfer_tx_);
    if (--transfer_anx_ == 0) {
        transfer_dy_ = wrap_step(transfer_dy_, transfer_ty_);
        --transfer_tmp_ny_;
        transfer_adx_ = transfer_dx_start_;
        transfer_anx_ = transfer_tmp_nx_;
        if (transfer_tmp_ny_ == 0) {
            command_done();
            transfer_kind_ = TransferKind::None;
        }
    }
}

std::uint64_t VdpCommandEngine::transfer_unit_cost_tstates() const {
    // VDP command-timing parity: the per-unit VRAM-slot "consume" cost that the
    // S#2 TR (transfer-ready, bit7) bit drops for after each serviced transfer
    // unit, then re-raises. openMSX PUNTS on transfer per-unit timing
    // (nextAccessSlot(limit), "timing is inaccurate", VDPCmdEngine.cc:1288/1349/
    // 1770) and TIER-1 gives no transfer figure, so this reuses the closest
    // grounded per-unit constant of the byte/dot analog (the audit's "consistent
    // with the command's per-unit tick cost"):
    //   HMMC byte write ~ HMMV 48/byte;  LMMC RMW pixel ~ LMMV 96/px;
    //   LMCM pixel read ~ SRCH 88/read-px.
    // 0 when no transfer is active (kind None), e.g. immediately after the final
    // unit completes -- leaving TR raised as real HW does after the last byte
    // (fact-sheet §8: "the transfer can end ... even though TR was 1 after the
    // last byte").
    switch (transfer_kind_) {
    case TransferKind::Hmmc: return tstates_from_vdp_cycles(kTicksHmmv);  // 8
    case TransferKind::Lmmc: return tstates_from_vdp_cycles(kTicksLmmv);  // 16
    case TransferKind::Lmcm: return tstates_from_vdp_cycles(kTicksSrch);  // 15
    default: return 0;
    }
}

}  // namespace sony_msx::devices::video
