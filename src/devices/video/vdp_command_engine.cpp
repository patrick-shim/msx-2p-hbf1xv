#include "devices/video/vdp_command_engine.h"

#include <algorithm>

#include "devices/video/vdp_command_address.h"

namespace sony_msx::devices::video {

namespace {

// Per-scrMode addressing/packing traits (VDPCmdEngine.cc:155-410's
// Graphic4Mode/Graphic5Mode/Graphic6Mode/Graphic7Mode/NonBitmapMode structs,
// independently re-expressed as a runtime table keyed by scrMode instead of
// the reference's compile-time template parameter -- behaviorally
// equivalent, much simpler to dispatch on a value computed from R#0/R#1/R#25
// at runtime). `address_of` is one of the five vdp_command_address.h
// functions (the D7-closing piece, §1.5) -- never V9958Vdp::effective_address().
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
    if (transfer_kind_ != TransferKind::Lmcm || !ce()) {
        return;
    }
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
    for (;;) {
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
    for (;;) {
        vram_.write(t.address_of(adx, dy) & 0x1FFFFu, col_);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            dy = wrap_step(dy, ty);
            --tmp_ny;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
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
        }
    }
    command_done();
}

// --- atomic commands: with logical op (PSET/LINE/LMMV/LMMM) ---------------

void VdpCommandEngine::run_pset() {
    const DecodedOp decoded = decode_op(static_cast<std::uint8_t>(cmd_ & 0x0F));
    const ModeTraits& t = traits_for(scr_mode_);
    const std::uint8_t cl = static_cast<std::uint8_t>(col_ & t.color_mask);
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
    unsigned asx = (nx_ - 1u) >> 1;  // ASX = (NX-1)>>1 (unsigned; NX==0 is a pathological, unused input)
    unsigned adx = dx_;
    unsigned dy = dy_;
    unsigned anx = 0;

    for (int guard = 0; guard < 1'000'000; ++guard) {
        write_pixel(scr_mode_, adx, dy, cl, decoded.op, decoded.transparent, vram_);

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
                }
                asx = (asx - ny_limit) & 1023u;
                const bool anx_hit = (anx == nx_);
                ++anx;
                if (anx_hit || (adx & t.pixels_per_line) != 0) done = true;
            }
        }
        if (done) break;
    }
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
    for (;;) {
        write_pixel(scr_mode_, adx, dy, cl, decoded.op, decoded.transparent, vram_);
        adx = wrap_step(adx, tx);
        if (--anx == 0) {
            dy = wrap_step(dy, ty);
            --tmp_ny;
            adx = dx_;
            anx = tmp_nx;
            if (tmp_ny == 0) break;
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
        }
    }
    command_done();
}

// --- event-driven transfer commands: LMCM/LMMC/HMMC -----------------------

void VdpCommandEngine::start_lmcm() {
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

}  // namespace sony_msx::devices::video
