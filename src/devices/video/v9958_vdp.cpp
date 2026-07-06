#include "devices/video/v9958_vdp.h"

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

V9958Vdp::V9958Vdp() {
    reset();
}

void V9958Vdp::reset() {
    vram_.clear();
    control_regs_.fill(0);

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

    // Release the /INT line at reset (both IRQ helpers reset, VDP.cc:295-296).
    irq_level_ = false;
    if (irq_sink_ != nullptr) {
        irq_sink_->set_irq(false);
    }
}

void V9958Vdp::set_irq_line(IrqLine* const sink) {
    irq_sink_ = sink;
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
    return ((static_cast<std::uint32_t>(control_regs_[14]) << 14) | vram_pointer_) & 0x1FFFF;
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
    const std::uint8_t ret = peek_status_register(reg);
    switch (reg) {
    case 0:
        // Reading S#0 clears the VBlank flag and releases the vertical IRQ line
        // (VDP.cc:967-968).
        status_reg0_ = static_cast<std::uint8_t>(status_reg0_ & ~0x80);
        irq_vertical_ = false;
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
    default:
        // S#5/S#7/S#9 read-side resets are inert until the sprite/command engines
        // exist (DEFERRED D2/D3).
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
        // Command-engine registers R#32..R#46 are DEFERRED (backlog D3).
        return;
    }
    control_regs_[reg] = value;

    switch (reg) {
    case 0:   // M3..M5 + IE1 (line-int enable, gates S#1 FH)
    case 1:   // M1..M2 + IE0 (VBlank-int enable)
    case 25:  // YJK/YAE mode bits
        recompute_mode();
        break;
    case 16:
        // Writing R#16 aborts a half-finished palette load (VDP.cc:1135).
        palette_data_stored_ = false;
        break;
    default:
        break;
    }
}

void V9958Vdp::recompute_mode() {
    mode_ = decode_vdp_mode(control_regs_[0], control_regs_[1], control_regs_[25]);
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

std::uint8_t V9958Vdp::peek_status_register(const int reg) const {
    switch (reg) {
    case 0:
        return status_reg0_;  // bit7 F; 5S/C + fifth-sprite number idle 0 (D2)
    case 1: {
        // Reset base 0x04 (ID#=2); FH (bit0) reflects the held line only when
        // line interrupts are enabled; LPS/FL (bits 6/7) dead -> 0 on V9958.
        std::uint8_t base = kStatusReg1Reset;
        if ((control_regs_[0] & 0x10) && irq_horizontal_) {
            base = static_cast<std::uint8_t>(base | 0x01);
        }
        return base;
    }
    case 2:
        // Undocumented bits 3,2 = 1 (0x0C); EO field toggle in bit1. TR/CE/HR/
        // VR/BD are live command/raster bits -> idle 0 (DEFERRED D3/D4).
        return static_cast<std::uint8_t>(kStatusReg2Base | (eo_field_ ? 0x02 : 0x00));
    case 3:
        return 0x00;  // collision-X low (idle; live -> D2)
    case 4:
        return 0xFE;  // collision-X high, bits7..1 = 1 (idle mask)
    case 5:
        return 0x00;  // collision-Y low (idle; live -> D2)
    case 6:
        return 0xFC;  // collision-Y high, bits7..2 = 1 (idle mask)
    case 7:
        return 0x00;  // color register POINT/LMCM (idle; live -> D3)
    case 8:
        return 0x00;  // border-X low (idle; live -> D3)
    case 9:
        return 0xFE;  // border-X high, bits7..1 = 1 (idle mask)
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

}  // namespace sony_msx::devices::video
