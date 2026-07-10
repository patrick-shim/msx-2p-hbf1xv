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

#include "devices/video/v9958_vdp.h"

#include <limits>
#include <span>

#include "devices/video/vdp_access_timing.h"

namespace sony_msx::devices::video {

namespace {

// V9938 boot palette (16 x 9-bit GRB), from appendix 8 of the V9938 data book
// (page 148); reproduced by openMSX at VDP.cc:299-302. The V9958 boots to the
// same V9938-compatible palette (fact-sheet §5). Behavior reference only.
constexpr std::array<std::uint16_t, 16> kBootPalette = {
    0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
    0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777,
};

}  // namespace

V9958Vdp::V9958Vdp() : cmd_engine_(vram_) {
    reset();
}

void V9958Vdp::reset() {
    vram_.clear();
    control_regs_.fill(0);

    // M22: reset the sprite/command engines before recompute_mode() below
    // (re-)notifies the freshly-reset command engine of the reset display
    // mode (GRAPHIC1, no commands legal -> scrMode = -1).
    cmd_engine_.reset();
    sprite_engine_.reset();

    data_latch_ = 0;
    register_data_stored_ = false;
    palette_data_stored_ = false;

    vram_pointer_ = 0;
    write_access_ = false;
    cpu_vram_data_ = 0;

    // Status registers (VDP.cc:290-296): S#0=0x00, S#1=0x04 (V9958 ID#=2),
    // S#2=0x0C (undocumented bits 3,2 = 1). S#1/S#2 are computed on read from
    // their reset bases + the live flags, so only S#0's F flag is stored.
    status_reg0_ = 0x00;
    eo_field_ = false;
    irq_vertical_ = false;
    irq_horizontal_ = false;

    // Color-burst registers default 00/3B/05 (fact-sheet §4 line 77; VDP.cc
    // reset). Stored only (no visual consequence in M14).
    control_regs_[20] = 0x00;
    control_regs_[21] = 0x3B;
    control_regs_[22] = 0x05;

    palette_ = kBootPalette;
    recompute_mode();

    // Blink state (M21-S2): stable/off at reset (VDP.cc:278-279).
    blink_countdown_ = 0;
    blink_state_ = false;

    // Release the /INT line at reset (both IRQ helpers reset, VDP.cc:295-296).
    irq_level_ = false;
    if (irq_sink_ != nullptr) {
        irq_sink_->set_irq(false);
    }
}

void V9958Vdp::set_irq_line(IrqLine* const sink) {
    irq_sink_ = sink;
}

void V9958Vdp::attach_clock_source(VdpClockSource* const source) {
    clock_source_ = source;
}

void V9958Vdp::attach_render_sync(VdpRenderSyncListener* const listener) {
    render_sync_ = listener;
}

void V9958Vdp::attach_register_write_observer(VdpRegisterWriteObserver* const observer) {
    register_write_observer_ = observer;
}

// --- core::IoDevice ---------------------------------------------------------

core::BusData V9958Vdp::io_read(const core::BusAddress port) {
    // Any read from #98/#99 aborts a half-completed port-1 write (VDP.cc:998).
    register_data_stored_ = false;

    switch (port & 0x03) {
    case 0:  // #98 VRAM data read
        return vram_data_read();
    case 1:  // #99 status register read via the R#15 pointer
        return read_status(control_regs_[15] & 0x0F);
    default:  // #9A / #9B are write-only (VDP.cc:1006-1008)
        return 0xFF;
    }
}

void V9958Vdp::io_write(const core::BusAddress port, const core::BusData value) {
    // M32-S1 render-sync seam: notify BEFORE any state mutates, for all four
    // ports uniformly -- openMSX's sync-before-change protocol at line
    // granularity (PixelRenderer.cc:253-394 register/palette updates;
    // :510-517 VRAM writes: every update handler calls sync(time)/
    // renderUntil(time) FIRST, then applies the change). Zero interrupt/
    // status/VRAM side effects here -- the listener only reads state and
    // writes into its own pixel store (docs/m32-planner-package.md §2.3).
    if (render_sync_ != nullptr) {
        render_sync_->on_before_state_change();
    }
    const auto byte = static_cast<std::uint8_t>(value & 0xFF);
    switch (port & 0x03) {
    case 0:  // #98 VRAM data write
        vram_data_write(byte);
        register_data_stored_ = false;  // VDP.cc:661
        break;
    case 1:  // #99 register / address write
        port1_write(byte);
        break;
    case 2:  // #9A palette write
        palette_write(byte);
        break;
    default:  // #9B indirect register write
        indirect_write(byte);
        break;
    }
}

// --- #98 VRAM data path -----------------------------------------------------

std::uint32_t V9958Vdp::effective_address() const {
    // 17-bit address = (R#14 A16..A14 << 14) | 14-bit pointer (VDP.cc:851).
    std::uint32_t addr =
        ((static_cast<std::uint32_t>(control_regs_[14]) << 14) | vram_pointer_) & 0x1FFFF;

    // D7 CPU-port piece (M21-S4, backlog D7): in G6/G7 (and the YJK overlay
    // modes sharing GRAPHIC7's base) planar VRAM is spread over two banks;
    // the STORAGE address is a 17-bit rotate-right-by-1 (A-M21-10, derived
    // from VDP.cc:849-857: `addr = ((addr << 16) | (addr >> 1)) & 0x1FFFF`,
    // which for a 17-bit addr reduces to `(addr >> 1) | ((addr & 1) << 16)`
    // -- even logical addresses land in bank 0 (0x00000-0x0FFFF), odd in
    // bank 1 (0x10000-0x1FFFF), same `addr >> 1` value in both banks).
    // `advance_vram_pointer()` below still operates on the untransformed
    // `vram_pointer_`/`control_regs_[14]` (A-M21-12).
    if (vdp_base_is_planar(mode_.base)) {
        addr = (addr >> 1) | ((addr & 1) << 16);
    }
    return addr;
}

void V9958Vdp::advance_vram_pointer() {
    // Post-access auto-increment (VDP.cc:883-887): the 14-bit pointer wraps, and
    // on carry R#14 increments ONLY in V9938 modes (128 KB counting). Legacy
    // TMS9918 modes (G1/G2/MC/T1) wrap at the 16 KB boundary (fact-sheet §2:40).
    vram_pointer_ = static_cast<std::uint16_t>((vram_pointer_ + 1) & 0x3FFF);
    if (vram_pointer_ == 0 && is_v9938_mode()) {
        control_regs_[14] = static_cast<std::uint8_t>((control_regs_[14] + 1) & 0x07);
    }
}

void V9958Vdp::vram_data_write(const std::uint8_t value) {
    // The shared read/write latch takes the written byte (VDP.cc:789-791), so a
    // following #98 read returns the just-written value.
    cpu_vram_data_ = value;
    vram_.write(effective_address(), value);
    advance_vram_pointer();
}

std::uint8_t V9958Vdp::vram_data_read() {
    // Return the read-ahead buffer, then refill it from the current address and
    // advance (VDP.cc:776-785 + executeCpuVramAccess). The immediate (non-slot-
    // scheduled) model is used; cycle-accurate access-slot timing is DEFERRED D4.
    const std::uint8_t result = cpu_vram_data_;
    cpu_vram_data_ = vram_.read(effective_address());
    advance_vram_pointer();
    return result;
}

// --- #99 register / address protocol + status read --------------------------

void V9958Vdp::port1_write(const std::uint8_t value) {
    if (register_data_stored_) {
        if (value & 0x80) {
            // Register write: R# = value & 0x3F (controlRegMask), data = latch
            // (VDP.cc:665-671; V9958 uses the full 6-bit register number).
            change_register(value & 0x3F, data_latch_);
        } else {
            // Address setup: A13..A0 = ((value << 8) | latch) & 0x3FFF;
            // bit6 = write access (0 = read -> issue a read-ahead) (VDP.cc:686-694).
            write_access_ = (value & 0x40) != 0;
            vram_pointer_ = static_cast<std::uint16_t>(((value << 8) | data_latch_) & 0x3FFF);
            if (!(value & 0x40)) {
                (void)vram_data_read();  // read-ahead prefetch
            }
        }
        register_data_stored_ = false;
    } else {
        data_latch_ = value;
        register_data_stored_ = true;
    }
}

std::uint8_t V9958Vdp::read_status(const int reg) {
    // S#7 (COL) must compute/advance the pending LMCM pixel BEFORE the value
    // is fetched (mirrors readColor()'s internal sync(), which runs before
    // peekStatusReg returns; VDP.cc:950-951/978-979). All other registers'
    // peek value is unaffected by their own read-side effect, so the
    // existing peek-then-mutate order is kept for them.
    if (reg == 7) {
        cmd_engine_.on_color_register_read();
    }
    if (reg == 0) {
        // Line-granular collision re-latch (boot-logo fix): before the value
        // is fetched, latch any per-line collision event the raster has
        // scanned since the last S#0 read-clear. Real hardware checks
        // sprites progressively as the raster advances (SpriteChecker.hh:
        // 70-100 sync()/checkSprites), so C re-latches per colliding LINE,
        // not per frame -- the HB-F1XV BIOS boot-logo wobble paces one
        // scroll-register write per S#0-C poll exit and needs this.
        sprite_engine_.sync_collision_to_raster(raster_display_line());
    }
    const std::uint8_t ret = peek_status_register(reg);
    switch (reg) {
    case 0:
        // Reading S#0 clears the VBlank flag and releases the vertical IRQ line
        // (VDP.cc:967-968), and clears ONLY the sprite engine's 5S/C bits
        // (A-M22-14; SpriteChecker.hh:104-110's `& 0x1F`), leaving the
        // sprite-number field stale until the next frame's recompute.
        status_reg0_ = static_cast<std::uint8_t>(status_reg0_ & ~0x80);
        irq_vertical_ = false;
        sprite_engine_.reset_status();
        // Events at lines the raster has already scanned are in the past --
        // they can never re-latch after this read's clear (progressive
        // hardware checking; see sprite_engine.h).
        sprite_engine_.consume_collision_events_up_to(raster_display_line());
        update_irq();
        break;
    case 1:
        // Reading S#1 releases the line IRQ only when line interrupts are enabled
        // (R#0 IE1) (VDP.cc:971-973).
        if (control_regs_[0] & 0x10) {
            irq_horizontal_ = false;
            update_irq();
        }
        break;
    case 5:
        // Reading S#5 (collision-Y low) zeroes BOTH collision X and Y
        // (VDP.cc:975-977; SpriteChecker::resetCollision()).
        sprite_engine_.reset_collision();
        break;
    case 9:
        // Reading S#9 (border-X high) clears BD (VDP.cc:981-982; resetBD()).
        cmd_engine_.on_border_x_register_read();
        break;
    default:
        break;
    }
    return ret;
}

// --- #9A palette ------------------------------------------------------------

void V9958Vdp::palette_write(const std::uint8_t value) {
    if (palette_data_stored_) {
        // byte1 (latch) = 0 R2R1R0 0 B2B1B0, byte2 (value) = 0 0 0 0 0 G2G1G0;
        // stored GRB masked to 9 bits (& 0x777), R#16 pointer auto-increments
        // (VDP.cc:709-714; fact-sheet §4 line 78).
        const int index = control_regs_[16] & 0x0F;
        palette_[index] =
            static_cast<std::uint16_t>(((value << 8) | data_latch_) & 0x777);
        control_regs_[16] = static_cast<std::uint8_t>((control_regs_[16] + 1) & 0x0F);
        palette_data_stored_ = false;
    } else {
        data_latch_ = value;
        palette_data_stored_ = true;
    }
}

// --- #9B indirect register write (R#17 pointer + AII) -----------------------

void V9958Vdp::indirect_write(const std::uint8_t value) {
    // changeRegister(R#17 & 0x3F, value); auto-increment R#17 unless the AII bit
    // (R#17 bit7) is set (VDP.cc:720-729). The old R#17 governs the increment
    // even if R#17 is written indirectly (the documented openMSX edge case, R-6).
    data_latch_ = value;
    const std::uint8_t reg_nr = control_regs_[17];
    change_register(reg_nr & 0x3F, value);
    if ((reg_nr & 0x80) == 0) {
        control_regs_[17] = static_cast<std::uint8_t>((reg_nr + 1) & 0x3F);
    }
}

// --- register-write side effects --------------------------------------------

void V9958Vdp::change_register(const std::uint8_t reg, const std::uint8_t value) {
    if (reg >= kNumControlRegs) {
        // R#32..R#46 = the command engine's own register file (A-M22-1,
        // grounded 1:1 against VDP.cc:1020-1033's changeRegister()). Reached
        // identically via the #99 two-write latch or the #9B indirect path
        // -- both already route through this function.
        if (reg < 47) {
            cmd_engine_.write_register(static_cast<int>(reg) - 32, value);
        }
        return;
    }

    // M36 Bug B: capture the OLD value before committing so the IE0/IE1
    // /INT re-evaluation below (cases 0/1) can compute `change = old ^ new`
    // -- openMSX's `changeRegister` does exactly this: commits the new
    // value, then re-evaluates the IE0/IE1 interrupt lines against `change`
    // (references/openmsx-21.0/src/video/VDP.cc:1170-1198).
    const std::uint8_t old_value = control_regs_[reg];
    control_regs_[reg] = value;
    const std::uint8_t change = static_cast<std::uint8_t>(old_value ^ value);

    // DEC-0052 stream-light watchlog hook (default-off): notify the diagnostic
    // observer of this R#0..R#31 control-register write. No-op unless installed
    // (only while a stream capture is armed); the machine filters for R#1.
    if (register_write_observer_ != nullptr) {
        register_write_observer_->on_register_write(reg, value);
    }

    switch (reg) {
    case 0:  // M3..M5 + IE1 (R#0 bit4, line-int enable, gates S#1 FH)
        // M36 Bug B: an R#0 write that TOGGLES IE1 re-evaluates the horizontal
        // /INT on the write itself (VDP.cc:1177-1185). Only the CLEAR edge acts
        // here -- IE1 off must immediately de-assert a held line /INT, as
        // openMSX's `irqHorizontal.reset()` does (VDP.cc:1182). The SET edge
        // is left to the existing per-step line-match poll
        // (Hbf1xvMachine::poll_line_interrupt -> on_line_match()), our
        // analogue of openMSX's `scheduleHScan(time)` (VDP.cc:1180): it
        // asserts irq_horizontal_ at the next R#19-R#23 match-line crossing
        // while IE1 is on, so enabling IE1 mid-frame arms rather than
        // instantly fires.
        if (change & 0x10) {
            if (!(value & 0x10)) {
                irq_horizontal_ = false;
            }
            update_irq();
        }
        recompute_mode();
        break;
    case 1:  // M1..M2 + IE0 (R#1 bit5, VBlank-int enable)
        // M36 Bug B (the YS II building-interior crash): an R#1 write that
        // TOGGLES IE0 re-evaluates the vertical /INT on the write itself -- the
        // path the pre-fix code missed (it only called recompute_mode(), so a
        // held /INT survived an IE0-clear until the next S#0 read, letting the
        // ISR's later EI re-fire forever -> nested-VBLANK stack overflow).
        // openMSX VDP.cc:1186-1198: IE0 set -> re-assert ONLY if F is already
        // pending (the documented Andonis/Zanac case, VDP.cc:1189-1194); IE0
        // clear -> de-assert immediately (VDP.cc:1196, irqVertical.reset()).
        if (change & 0x20) {
            if (value & 0x20) {
                if (status_reg0_ & 0x80) {  // F pending
                    irq_vertical_ = true;
                }
            } else {
                irq_vertical_ = false;  // IE0 cleared -> /INT de-asserts now
            }
            update_irq();
        }
        recompute_mode();
        break;
    case 25:  // YJK/YAE mode bits
        recompute_mode();
        break;
    case 16:
        // Writing R#16 aborts a half-finished palette load (VDP.cc:1135).
        palette_data_stored_ = false;
        break;
    case 13: {
        // Blink on/off timing (M21-S2, backlog D6). Writing R#13 resets blink
        // state even if the value is unchanged (VDP.cc:1040-1057): force
        // blink_state_ to "ON" unless the ON period (bits 7-4) is zero, then
        // re-arm the countdown from the current phase's period (*10 frames)
        // when both nibbles are non-zero (alternating); otherwise freeze
        // (blink_countdown_ = 0, a single stable color).
        blink_state_ = (value & 0xF0) != 0;
        blink_countdown_ = ((value & 0xF0) != 0 && (value & 0x0F) != 0)
                                ? static_cast<int>(value >> 4) * 10
                                : 0;
        break;
    }
    default:
        break;
    }
}

void V9958Vdp::recompute_mode() {
    mode_ = decode_vdp_mode(control_regs_[0], control_regs_[1], control_regs_[25]);
    // scrMode recompute (A-M22-6): governs BOTH command legality and which
    // command-engine address formula applies. R#25 bit6 = CMD (V9958-only,
    // "commands possible in non-bitmap modes", VDP.hh:544-549).
    cmd_engine_.notify_mode_change(mode_, (control_regs_[25] & 0x40) != 0);
}

bool V9958Vdp::is_v9938_mode() const {
    return vdp_base_is_v9938_mode(mode_.base);
}

// --- interrupts -------------------------------------------------------------

void V9958Vdp::on_vsync() {
    status_reg0_ = static_cast<std::uint8_t>(status_reg0_ | 0x80);  // F (VDP.cc:403)
    eo_field_ = !eo_field_;                                          // S#2 EO toggle
    if (control_regs_[1] & 0x20) {                                   // IE0 (VDP.cc:404)
        irq_vertical_ = true;
    }

    // Blink countdown (M21-S2, backlog D6; VDP.cc:600-608). Decrements once
    // per frame boundary; on reaching 0, flips the phase and re-arms from
    // the newly-current phase's R#13 nibble (*10 frames).
    if (blink_countdown_ != 0) {
        --blink_countdown_;
        if (blink_countdown_ == 0) {
            blink_state_ = !blink_state_;
            const std::uint8_t r13 = control_regs_[13];
            blink_countdown_ =
                static_cast<int>(blink_state_ ? (r13 >> 4) : (r13 & 0x0F)) * 10;
        }
    }

    // Sprite check/collision/5th-sprite recompute (M22-S1/S2, backlog D2).
    // Once per frame boundary, mirroring the blink-countdown precedent -- no
    // new clock consumer. `height` deliberately duplicates
    // VdpFrameRenderer::height()'s own formula: V9958Vdp (core) has no
    // dependency on VdpFrameRenderer (presentation), keeping the existing
    // one-directional layering intact.
    int height = 192;
    switch (mode_.mode) {
    case VdpMode::Text1:
    case VdpMode::Text2:
    case VdpMode::Text1Q:
    case VdpMode::MulticolorQ:
    case VdpMode::Unknown:
        height = 192;
        break;
    default:
        height = (control_regs_[9] & 0x80) ? 212 : 192;
        break;
    }
    sprite_engine_.recompute_frame(vram_, std::span<const std::uint8_t, kNumControlRegs>(control_regs_), mode_,
                                    height);

    update_irq();
}

void V9958Vdp::on_line_match() {
    if (control_regs_[0] & 0x10) {  // IE1 (VDP.cc:412)
        irq_horizontal_ = true;
    }
    update_irq();
}

bool V9958Vdp::irq_active() const {
    return irq_vertical_ || irq_horizontal_;
}

void V9958Vdp::update_irq() {
    const bool level = irq_vertical_ || irq_horizontal_;
    if (level != irq_level_) {
        irq_level_ = level;
        if (irq_sink_ != nullptr) {
            irq_sink_->set_irq(level);
        }
    }
}

// --- accessors --------------------------------------------------------------

const VdpVram& V9958Vdp::vram() const {
    return vram_;
}

VdpVram& V9958Vdp::vram() {
    return vram_;
}

std::uint8_t V9958Vdp::control_register(const int index) const {
    if (index < 0 || index >= kNumControlRegs) {
        return 0;
    }
    return control_regs_[static_cast<std::size_t>(index)];
}

int V9958Vdp::raster_display_line() const {
    if (clock_source_ == nullptr) {
        return std::numeric_limits<int>::min();  // clockless: hooks no-op
    }
    // Same frame-position arithmetic as the S#2 VR bit (fact-sheet §7 NTSC
    // breakdown; on_vsync() fires at the start of the lower border, so
    // tstates==0 is the first bottom-border line): the active display of the
    // frame being scanned occupies [non_active_lines, 262).
    const std::uint64_t tstates = clock_source_->cpu_tstates_since_vsync();
    const int line_since_vsync =
        static_cast<int>((tstates / static_cast<std::uint64_t>(vdp_access_timing::kCpuTstatesPerLine)) % 262);
    const bool ln212 = (control_regs_[9] & 0x80) != 0;
    const int non_active_lines = ln212 ? 50 : 70;  // 262-212 / 262-192
    return line_since_vsync - non_active_lines;
}

std::uint8_t V9958Vdp::peek_status_register(const int reg) const {
    switch (reg) {
    case 0:
        // bit7 F (owned by V9958Vdp); bits6-0 = 5S/C/sprite-number, live
        // from the sprite engine (M22-S1, backlog D2).
        return static_cast<std::uint8_t>(status_reg0_ | sprite_engine_.status_bits());
    case 1: {
        // Reset base 0x04 (ID#=2); FH (bit0) reflects the held line only when
        // line interrupts are enabled; LPS/FL (bits 6/7) dead -> 0 on V9958.
        std::uint8_t base = kStatusReg1Reset;
        if ((control_regs_[0] & 0x10) && irq_horizontal_) {
            base = static_cast<std::uint8_t>(base | 0x01);
        }
        return base;
    }
    case 2: {
        // Undocumented bits 3,2 = 1 (0x0C); EO field toggle in bit1; bit7 TR,
        // bit4 BD, bit0 CE live from the command engine (M22-S3..S6, backlog
        // D3). Bits5/6 (HR/VR) are LIVE, raster-position-derived (bug fix,
        // post-M28 -- discovered via a real, CPU-driven BIOS boot: the BIOS
        // polls S#2 bit6 (VR) waiting for it to toggle before writing any VDP
        // register, and hangs forever if VR is a hardcoded constant of
        // either polarity; confirmed by forcing VR=1 and observing the same
        // hang on the opposite edge).
        std::uint8_t s2 = static_cast<std::uint8_t>(kStatusReg2Base | (eo_field_ ? 0x02 : 0x00));
        if (clock_source_ != nullptr) {
            const std::uint64_t tstates = clock_source_->cpu_tstates_since_vsync();
            // fact-sheet §7: on_vsync() fires "at the start of the lower
            // border" -- i.e. tstates==0 is the FIRST line of the bottom
            // border, not the top of the frame. Relative to that origin,
            // the 262-line frame runs: [0, non_active_lines) = bottom-
            // border+bottom-erase+vsync+top-erase+top-border, then
            // [non_active_lines, 262) = active display (fact-sheet §7 NTSC
            // breakdown: LN=0 13+26+192+25+3+3=262; LN=1 13+16+212+15+3+3=
            // 262; the erase/vsync line counts, 13/3/3, are IDENTICAL
            // between LN modes -- only the border sizes differ).
            const int line_since_vsync = static_cast<int>(
                (tstates / static_cast<std::uint64_t>(vdp_access_timing::kCpuTstatesPerLine)) % 262);
            const bool ln212 = (control_regs_[9] & 0x80) != 0;
            const int non_active_lines = ln212 ? 50 : 70;  // 262-212 / 262-192
            if (line_since_vsync < non_active_lines) {
                s2 = static_cast<std::uint8_t>(s2 | 0x40);  // VR
            }
            // fact-sheet §7 per-line breakdown: sync[0,100) + left-erase
            // [100,202) + left-border[202,258) + DISPLAY[258,1282)=1024 +
            // right-border[1282,1341) + right-erase[1341,1368). HR is
            // outside the display window, independent of LN.
            const int vdp_cycle = vdp_access_timing::vdp_cycle_within_line(tstates);
            if (vdp_cycle < 258 || vdp_cycle >= 1282) {
                s2 = static_cast<std::uint8_t>(s2 | 0x20);  // HR
            }
        }
        if (cmd_engine_.tr()) s2 = static_cast<std::uint8_t>(s2 | 0x80);
        if (cmd_engine_.bd()) s2 = static_cast<std::uint8_t>(s2 | 0x10);
        if (cmd_engine_.ce()) s2 = static_cast<std::uint8_t>(s2 | 0x01);
        return s2;
    }
    case 3:
        return static_cast<std::uint8_t>(sprite_engine_.collision_x() & 0xFF);  // collision-X low
    case 4:
        return static_cast<std::uint8_t>((sprite_engine_.collision_x() >> 8) | 0xFE);  // collision-X high
    case 5:
        return static_cast<std::uint8_t>(sprite_engine_.collision_y() & 0xFF);  // collision-Y low
    case 6:
        return static_cast<std::uint8_t>((sprite_engine_.collision_y() >> 8) | 0xFC);  // collision-Y high
    case 7:
        return cmd_engine_.color();  // POINT/LMCM color result
    case 8:
        return static_cast<std::uint8_t>(cmd_engine_.border_x() & 0xFF);  // border-X (ASX) low
    case 9:
        return static_cast<std::uint8_t>((cmd_engine_.border_x() >> 8) | 0xFE);  // border-X (ASX) high
    default:
        return 0xFF;  // non-existent status register
    }
}

std::uint16_t V9958Vdp::vram_pointer() const {
    return vram_pointer_;
}

std::uint32_t V9958Vdp::vram_address() const {
    return effective_address();
}

std::uint16_t V9958Vdp::palette_entry(const int index) const {
    return palette_[static_cast<std::size_t>(index & 0x0F)];
}

const VdpModeState& V9958Vdp::mode() const {
    return mode_;
}

bool V9958Vdp::blink_state() const {
    return blink_state_;
}

const SpriteEngine& V9958Vdp::sprite_engine() const {
    return sprite_engine_;
}

const VdpCommandEngine& V9958Vdp::cmd_engine() const {
    return cmd_engine_;
}

}  // namespace sony_msx::devices::video
