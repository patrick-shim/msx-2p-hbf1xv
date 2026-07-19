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

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>

#include "devices/video/vdp_access_timing.h"

namespace sony_msx::devices::video {

namespace {

// --- M51 (DEC-0078) Task 2 diagnostic sprite trace --------------------------
// Env-gated singleton (SONY_MSX_M51_SPRITE_TRACE = path, or "1" -> stderr).
// DEFAULT-OFF: with the env var unset enabled() is a cached `false`, every
// call site short-circuits, and the emulated state is untouched (A-M51-4 /
// EG-8 byte-identity; M47 logger precedent). Owns the frame counter so all
// five event classes land in one ordered stream with "F<n> " prefixes.
class M51SpriteTrace {
public:
    static M51SpriteTrace& instance() {
        static M51SpriteTrace trace;
        return trace;
    }
    [[nodiscard]] bool enabled() const { return out_ != nullptr; }
    void write(const char* fmt, std::va_list args) {
        if (out_ == nullptr) {
            return;
        }
        std::fprintf(out_, "F%d ", frame_);
        std::vfprintf(out_, fmt, args);
        std::fputc('\n', out_);
    }
    void next_frame() { ++frame_; }

private:
    M51SpriteTrace() {
        const char* value = std::getenv("SONY_MSX_M51_SPRITE_TRACE");
        if (value != nullptr && value[0] != '\0') {
            out_ = (std::strcmp(value, "1") == 0) ? stderr : std::fopen(value, "w");
        }
    }
    std::FILE* out_ = nullptr;
    int frame_ = 0;
};

}  // namespace

bool m51_sprite_trace_enabled() {
    return M51SpriteTrace::instance().enabled();
}

void m51_sprite_trace(const char* fmt, ...) {
    std::va_list args;
    va_start(args, fmt);
    M51SpriteTrace::instance().write(fmt, args);
    va_end(args);
}

void m51_sprite_trace_next_frame() {
    M51SpriteTrace::instance().next_frame();
}

namespace {

// V9938 boot palette (16 x 9-bit GRB), from appendix 8 of the V9938 data book
// (page 148); reproduced by openMSX at VDP.cc:299-302. The V9958 boots to the
// same V9938-compatible palette (fact-sheet §5). Behavior reference only.
constexpr std::array<std::uint16_t, 16> kBootPalette = {
    0x000, 0x000, 0x611, 0x733, 0x117, 0x327, 0x151, 0x627,
    0x171, 0x373, 0x661, 0x664, 0x411, 0x265, 0x555, 0x777,
};

// M48 Slice 1 (DEC-0075) SUPERSEDED the M44 Phase-2a empirical per-mode
// kActiveDisplaySlotFactorPercent[] placeholder (200% GRAPHIC4/5, 300%
// GRAPHIC6/7). The active-display command-throughput cap is now the principled,
// sprite-aware tier-1 slot-availability factor slot_factor = 154 /
// effective_slots_per_line() applied in arm_command_busy_window() (fact-sheet
// §8 line 163; the three per-line slot COUNTS live in vdp_access_timing.h). The
// old placeholder was a coarse per-mode scalar that was NOT sprite-aware and NOT
// grounded in the 154/88/31 slot counts; it is removed here per M48 AC-1.

}  // namespace

V9958Vdp::V9958Vdp() : cmd_engine_(vram_) {
    // M44 (DEF-M44-CMDSYNC Phase 1): route the command engine's per-destination-
    // row writes through this VDP's render-sync gates. Installed once; reset()
    // does not clear it (externally-owned-lifecycle pattern).
    cmd_engine_.set_command_row_sink(&command_row_sink_);
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
    // M49 Slice 2 (backlog D9): no progressive sprite-split pass survives a reset.
    sprite_split_active_ = false;

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

    // M44 Phase 2a: no command busy window survives a reset (both the engine's
    // pure duration -- cleared in cmd_engine_.reset() above -- and this VDP-side
    // expiry timestamp reset to 0 == already-expired). Same for the S#2 TR
    // per-unit drop-then-rearm window (VDP command-timing parity).
    cmd_busy_until_cycles_ = 0;
    tr_busy_until_cycles_ = 0;

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

void V9958Vdp::set_command_timing_suspended(const bool suspended) {
    command_timing_suspended_ = suspended;
}

int V9958Vdp::effective_slots_per_line() const {
    // M48 Slice 1 (DEC-0075): select the live 3-way CPU/command VRAM access-slot
    // regime (fact-sheet §8 line 163: 154 display-off / 88 sprites-off / 31
    // sprites-on) from live registers per A-M48-2.
    //
    // display ON  = R#1 bit6 (BL, display enable; fact-sheet §4 line 71) AND the
    //               raster is inside the active display region
    //               (raster_display_line() >= 0, fact-sheet §7). A blanked screen
    //               or the border/vblank region frees ALL slots -> 154.
    // sprites ON  = R#8 bit1 (SPD; SPD=0 means sprites are NOT disabled;
    //               fact-sheet §4 line 72) AND the current mode actually has
    //               sprites (vdp_sprite_mode(base) != 0 -> excludes TEXT1/TEXT2,
    //               fact-sheet §6 lines 116-120).
    //
    // Sprites-on slows the command engine IDENTICALLY regardless of how many
    // sprites are active on the line (fact-sheet §6(a) line 120: the sprite
    // subsystem always runs the same VRAM fetch pattern even with fewer/no
    // sprites), so this keys on sprites-ENABLED, never on sprite count.
    const bool display_on = (control_regs_[1] & 0x40) != 0 && raster_display_line() >= 0;
    if (!display_on) {
        return vdp_access_timing::kSlotsPerLineDisplayOff;  // 154
    }
    const bool sprites_on =
        (control_regs_[8] & 0x02) == 0 && vdp_sprite_mode(mode_.base) != 0;
    return sprites_on ? vdp_access_timing::kSlotsPerLineSpritesOn      // 31
                      : vdp_access_timing::kSlotsPerLineSpritesOff;    // 88
}

void V9958Vdp::arm_command_busy_window() {
    // Caller guarantees clock_source_ != nullptr && !command_timing_suspended_.
    // M44 Phase 2a: turn the command engine's pure, clock-free base duration into
    // an absolute expiry timestamp for the S#2-bit0 CE busy window.
    std::uint64_t duration = cmd_engine_.last_cmd_duration_tstates();
    if (duration != 0) {
        // M48 Slice 1 (DEC-0075): per-line slot-availability throughput cap. The
        // engine's base duration corresponds to the MAX-availability 154-slot
        // regime (A-M48-3); inflate it by slot_factor = 154 / S_effective when
        // fewer slots are free. From the three tier-1 counts (fact-sheet §8 line
        // 163) this is a 3-valued, sprite-aware factor:
        //   S=154 -> 1.00x (display off/border/vblank -> byte-identical to v1.1.4,
        //            A-M48-4: (d*154 + 153)/154 == d exactly),
        //   S= 88 -> 1.75x (display on, sprites off),
        //   S= 31 -> 4.97x (display on, sprites on).
        // Integer ceil of duration * 154 / S. This SUPERSEDES the empirical
        // per-mode kActiveDisplaySlotFactorPercent placeholder (DEC-0069). It is
        // a pure CE busy-WINDOW / S#2 STATUS overlay ONLY: VRAM mutation stays
        // atomic (§5) and the engine's pure duration + M44 per-command render-sync
        // oracle are untouched (the engine stays state-agnostic).
        const auto s_eff = static_cast<std::uint64_t>(effective_slots_per_line());
        const auto s_base =
            static_cast<std::uint64_t>(vdp_access_timing::kSlotsPerLineDisplayOff);
        duration = (duration * s_base + s_eff - 1u) / s_eff;
    }
    // DEC-0072 diagnostic bias (default 0), applied AFTER the slot factor. Clamp
    // the biased duration at 0 so a large negative bias cannot underflow the
    // unsigned expiry.
    std::int64_t biased = static_cast<std::int64_t>(duration) + cmd_busy_bias_tstates_;
    if (biased < 0) {
        biased = 0;
    }
    cmd_busy_until_cycles_ = clock_source_->cpu_total_cycles() + static_cast<std::uint64_t>(biased);
}

void V9958Vdp::arm_transfer_ready_window() {
    // Caller guarantees clock_source_ != nullptr && !command_timing_suspended_ and
    // that the command engine JUST serviced a transfer unit. VDP command-timing
    // parity: the S#2 TR (transfer-ready, bit7) bit drops while the VDP consumes
    // the VRAM access slot(s) for that unit, then re-raises -- modelled as an
    // absolute expiry timestamp exactly like the CE window. A 0 per-unit cost
    // (kind None, i.e. the transfer just completed) yields tr_busy_until == now
    // (inert), leaving TR raised as real HW does after the final byte. The
    // --vdp-cmd-bias diagnostic (a CE-window knob) is deliberately NOT applied
    // here; this window is a small, separate per-unit VRAM-slot cost.
    tr_busy_until_cycles_ =
        clock_source_->cpu_total_cycles() + cmd_engine_.transfer_unit_cost_tstates();
}

void V9958Vdp::steal_command_slot_for_cpu_vram_access() {
    // M48 Slice 2 (DEC-0075; backlog C1/D4 CONTENTION remainder): CPU-priority
    // VRAM access-slot STEAL. On real hardware a CPU VRAM access through #98 takes
    // priority over the command engine and STEALS an access slot from it,
    // lengthening the command's completion -- while the CPU access itself is
    // serviced with its normal timing (fact-sheet §8 line 163: "CPU VRAM access
    // takes priority over the command engine and steals slots -- with sprites on,
    // a HMMV can be cut ~2x by simultaneous OUT (#98),A").
    //
    // ONE-DIRECTIONAL, by tier-1 hardware fact: the HB-F1XV does NOT wire the
    // V9958 /WAIT generator (fact-sheet §1 line 34 / §7 line 129), so the VDP can
    // NEVER hold the CPU. This model therefore ONLY extends the command's reported
    // S#2-bit0 CE busy window (a STATUS/timing overlay); the CPU #98 read/write in
    // io_read/io_write completes UNCHANGED -- no stall, no delay, no dropped byte,
    // no CPU T-state change (AC-4). VRAM mutation stays atomic (§5) and
    // vdp_command_engine.* is untouched.
    //
    // Inert without a clock or while command timing is suspended -- exactly like
    // arm_command_busy_window() (the null-clock headless / non-perturbing debug-
    // seam paths stay byte-identical).
    if (clock_source_ == nullptr || command_timing_suspended_) {
        return;
    }
    // Only a command that is genuinely BUSY at THIS CPU access can lose a slot; an
    // access after the CE window has already closed contends with nothing (no
    // phantom extension). Same absolute-u64 clock compare the S#2 CE bit uses.
    if (cmd_busy_until_cycles_ <= clock_source_->cpu_total_cycles()) {
        return;
    }
    // One stolen slot's worth of command progress at the CURRENT live slot regime
    // (Slice 1's S_effective): slot_cost_tstates(S) = ceil(1368 / (6 * S)) ->
    // 154->2, 88->3, 31->8 T-states/slot. The sprites-on 8 T/access steal is what
    // reproduces the §8 "~2x HMMV cut by a concurrent OUT (#98)" on average.
    cmd_busy_until_cycles_ += static_cast<std::uint64_t>(
        vdp_access_timing::slot_cost_tstates(effective_slots_per_line()));
}

// --- core::IoDevice ---------------------------------------------------------

core::BusData V9958Vdp::io_read(const core::BusAddress port) {
    // Any read from #98/#99 aborts a half-completed port-1 write (VDP.cc:998).
    register_data_stored_ = false;

    switch (port & 0x03) {
    case 0:  // #98 VRAM data read
        // M48 Slice 2 (DEC-0075): CPU-priority slot steal -- a concurrent CPU #98
        // access lengthens a BUSY command's CE window ONLY; the read below is
        // serviced with unchanged timing (never stalls the CPU).
        steal_command_slot_for_cpu_vram_access();
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
    // M49-S2 (backlog D9): the adapter now ALSO keeps the incremental sprite plane
    // in pace with the background BEFORE the background commits (sprite THEN
    // background), so a mid-frame sprite-relevant change splits the sprite plane at
    // the SAME line as the background split -- see commit_sprite_split().
    if (render_sync_ != nullptr) {
        render_sync_->on_before_state_change();
    }
    const auto byte = static_cast<std::uint8_t>(value & 0xFF);
    switch (port & 0x03) {
    case 0:  // #98 VRAM data write
        // M48 Slice 2 (DEC-0075): CPU-priority slot steal -- extends a BUSY
        // command's CE window ONLY; the VRAM write below commits atomically with
        // unchanged CPU timing (never stalls the CPU, never drops the byte).
        steal_command_slot_for_cpu_vram_access();
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
        const std::uint64_t transfer_units_before = cmd_engine_.transfer_units_consumed();
        cmd_engine_.on_color_register_read();
        // VDP command-timing parity: an S#7 read that advanced an LMCM transfer
        // drop-then-rearms TR for the per-unit VRAM-read-slot cost (same model as
        // the COL-write path in change_register).
        if (clock_source_ != nullptr && !command_timing_suspended_ &&
            cmd_engine_.transfer_units_consumed() != transfer_units_before) {
            arm_transfer_ready_window();
        }
    }
    if (reg == 0) {
        // Line-granular collision re-latch (boot-logo fix): before the value
        // is fetched, latch any per-line collision event the raster has
        // scanned since the last S#0 read-clear. Real hardware checks
        // sprites progressively as the raster advances (SpriteChecker.hh:
        // 70-100 sync()/checkSprites), so C re-latches per colliding LINE,
        // not per frame -- the HB-F1XV BIOS boot-logo wobble paces one
        // scroll-register write per S#0-C poll exit and needs this.
        //
        // M49-S2 (backlog D9): the collision-event queue is FRAME-ATOMIC (collected
        // by recompute_frame at on_vsync, exactly as pre-M49) -- the seam's render-
        // only split pass never touches it -- so this re-latch is byte-identical for
        // every game (DEC-0031 boot-logo pacing unchanged).
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
            const std::uint64_t transfer_units_before = cmd_engine_.transfer_units_consumed();
            cmd_engine_.write_register(static_cast<int>(reg) - 32, value);
            // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): an R#46 (CMD) write just
            // ran the burst and recomputed the engine's pure duration; arm the
            // S#2-bit0 CE busy window from it. Only for real CPU-driven writes
            // (a clock is attached) that are not the machine's non-perturbing
            // debug seam (command_timing_suspended_). ABRT / degenerate / instant
            // commands report duration 0 -> cmd_busy_until == now -> inert.
            if (reg == 46 && clock_source_ != nullptr && !command_timing_suspended_) {
                arm_command_busy_window();
            }
            // VDP command-timing parity: if that write serviced an event-driven
            // transfer unit -- a COL/R#44 write feeding an active LMMC/HMMC, or the
            // pre-armed first unit consumed at an R#46 LMMC/HMMC issue -- drop-then-
            // rearm the S#2 TR bit for the per-unit VRAM-slot cost.
            if (clock_source_ != nullptr && !command_timing_suspended_ &&
                cmd_engine_.transfer_units_consumed() != transfer_units_before) {
                arm_transfer_ready_window();
            }
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
        // M36 Bug B (a multi-disk RPG title's building-interior crash): an R#1 write that
        // TOGGLES IE0 re-evaluates the vertical /INT on the write itself -- the
        // path the pre-fix code missed (it only called recompute_mode(), so a
        // held /INT survived an IE0-clear until the next S#0 read, letting the
        // ISR's later EI re-fire forever -> nested-VBLANK stack overflow).
        // openMSX VDP.cc:1186-1198: IE0 set -> re-assert ONLY if F is already
        // pending (the case openMSX documents at VDP.cc:1189-1194); IE0
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

void V9958Vdp::command_row_sync(const unsigned dy) {
    // M44 (DEF-M44-CMDSYNC Phase 1, DEC-0065). Fired by the command engine
    // BEFORE each atomic-command destination row is written; commit the display
    // rows up to this row's display line from the CURRENT (pre-this-row) live
    // VRAM, so each display row observes the correct partial-command state.
    // Structural analog of openMSX routing every command write through
    // VDPVRAM::writeCommon -> VRAMWindow::notify(addr,time) -> renderUntil(time)
    // BEFORE committing the byte (references/openmsx-21.0/src/video/VDPVRAM.hh:
    // 575-593, 309-322), with the destination display line standing in for
    // openMSX's EmuTime. Behavior reference only -- never copied.
    if (render_sync_ == nullptr) {
        return;  // no listener installed -> byte-identical no-op (strict superset)
    }

    // (1) Bitmap-mode + page-mask selection. Non-bitmap/text modes have no cheap
    //     address->display-line inverse -> fall back to the io_write-seam
    //     behavior (the openMSX VRAMWindow::isInside miss), no regression.
    int page_mask;
    switch (mode_.mode) {
    case VdpMode::Graphic4:
    case VdpMode::Graphic5:
        page_mask = 0x03;  // 4 pages x 0x8000 (256 lines each)
        break;
    case VdpMode::Graphic6:
    case VdpMode::Graphic7:
    case VdpMode::ScreenYjk:
    case VdpMode::ScreenYjkYae:
        page_mask = 0x01;  // 2 pages x 0x10000 (256 lines each; same page-row inverse)
        break;
    default:
        return;  // non-bitmap / text / sprite-table destinations -> fall back (§2.7)
    }

    // (2) VISIBLE-PAGE gate (openMSX's isInside(bitmapVisibleWindow) analog).
    //     The accumulator always renders Field::Progressive, for which
    //     resolve_bitmap_page() reduces to the base page (EO/blink never alter
    //     it); the displayed set is {base} normally, or {base&~1, (base&~1)|1}
    //     under multi-page scrolling (VdpFrameRenderer::bitmap_scroll_pages()).
    const int base_page = (control_regs_[2] >> 5) & page_mask;
    const int dest_page = static_cast<int>(dy >> 8) & page_mask;
    const bool multi_page =
        (control_regs_[25] & 0x01) != 0 && (control_regs_[2] & 0x20) != 0;
    bool displayed;
    if (multi_page) {
        const int even = base_page & ~0x01;
        displayed = (dest_page == even) || (dest_page == ((even | 0x01) & page_mask));
    } else {
        displayed = (dest_page == base_page);
    }
    if (!displayed) {
        return;  // off-screen source/template page -> commit nothing spuriously
    }

    // (3) ACTIVE-DISPLAY gate: suppress during vblank/border so the common
    //     "rebuild HUD in the vblank ISR" path stays byte-identical (the whole
    //     next frame renders from final VRAM). Also covers the clockless-test
    //     case (raster_display_line() == INT_MIN < 0 -> inert).
    if (raster_display_line() < 0) {
        return;
    }

    // (4) Display-line inverse: the exact inverse of
    //     VdpFrameRenderer::vertical_scroll_wrap(line) = (line + R#23) & 0xFF
    //     (vdp_frame_renderer.cpp:170-171). The horizontal/multi-page geometry
    //     needs no inversion -- render_line reads it live. The WRAP guard (never
    //     tripping the accumulator's mid-command finalize) and the M62 BEAM
    //     CLAMP (DEC-0091-AMENDMENT-A: never sealing rows AHEAD of the beam
    //     with commit-time register/SAT state) are enforced at the commit
    //     primitive (Hbf1xvMachine::VdpRenderSyncAdapter::on_commit_up_to),
    //     which alone knows the authoritative accumulator watermark.
    const int display_line =
        (static_cast<int>(dy & 0xFFu) - control_regs_[23]) & 0xFF;
    // M51 Task 2 trace (event class 3 context): the command-engine row sink's
    // source coordinates, BEFORE the machine adapter decides commit/skip.
    if (m51_sprite_trace_enabled()) {
        m51_sprite_trace("CMDROW dy=%d r23=%d display_line=%d raster=%d",
                         static_cast<int>(dy & 0xFFu), control_regs_[23],
                         display_line, raster_display_line());
    }
    render_sync_->on_commit_up_to(display_line);
}

// --- M49 Slice 2 (backlog D9): progressive per-frame sprite-split lifecycle ------

int V9958Vdp::sprite_frame_height() const {
    // The exact pre-M49 on_vsync() height formula, extracted verbatim.
    switch (mode_.mode) {
    case VdpMode::Text1:
    case VdpMode::Text2:
    case VdpMode::Text1Q:
    case VdpMode::MulticolorQ:
    case VdpMode::Unknown:
        return 192;
    default:
        return (control_regs_[9] & 0x80) ? 212 : 192;
    }
}

void V9958Vdp::begin_sprite_frame() {
    // Bind the LIVE VRAM/registers + capture the frame geometry (height/mode) for
    // the render-only progressive pass, resetting the watermark. Collision/status
    // are DELIBERATELY untouched (begin_frame preserves them) -- they stay the
    // previous frame's frame-atomic result until on_vsync's recompute.
    sprite_engine_.begin_frame(vram_, control_regs_span(), mode_, sprite_frame_height());
}

void V9958Vdp::commit_sprite_split(const int target, const bool wrap) {
    // Lazy-open the per-frame RENDERING-ONLY progressive pass on the FIRST active-
    // display write; a genuine frame wrap / D10 geometry jump (`wrap`) re-opens it,
    // mirroring the background accumulator's wrap-safety restart. Until first opened,
    // the sprite buffers hold the previous on_vsync() recompute (fully populated --
    // no empty-sprite regression on the lines already committed).
    // M51 Task 2 trace (event classes 1+2): lazy-open / wrap-reopen with the
    // prior watermark, then the visible-only segment with its LIVE gates.
    if (m51_sprite_trace_enabled()) {
        if (!sprite_split_active_ || wrap) {
            m51_sprite_trace("SPLIT-OPEN raster=%d target=%d wrap=%d prior_wm=%d was_active=%d",
                             raster_display_line(), target, wrap ? 1 : 0,
                             sprite_engine_.watermark(), sprite_split_active_ ? 1 : 0);
        }
        m51_sprite_trace("SPRITE-SEG lo=%d end=%d r1=%02X r8=%02X r23=%d mode_base=%d",
                         (!sprite_split_active_ || wrap) ? 1 : sprite_engine_.watermark(),
                         target, control_regs_[1], control_regs_[8], control_regs_[23],
                         static_cast<int>(mode_.base));
    }
    if (!sprite_split_active_ || wrap) {
        begin_sprite_frame();
        sprite_split_active_ = true;
    }
    // `target` is the EXCLUSIVE beam+2 commit boundary the background sync_to_line
    // uses; check_until_visible_only() is INCLUSIVE, so target-1 commits sprite lines
    // [wm, target) -- the SAME lines the background commits, split at the SAME line
    // (AC-S2). RENDERING-ONLY: it populates the visible-sprite buffers per-line-live
    // WITHOUT touching the frame-atomic collision/status the CPU reads (no
    // game-behaviour change). Advance-only within a frame (no-op when target <=
    // watermark) satisfies the M44 "never move the watermark backwards" discipline.
    // Reading the LIVE registers/VRAM HERE -- before the pending
    // write applies -- is what makes a mid-frame sprite-relevant change (still the
    // OLD value) split the RENDERED plane at exactly `target`.
    sprite_engine_.check_until_visible_only(target - 1);
}

void V9958Vdp::commit_sprite_rows(const int target) {
    // M51 (DEC-0078) fix, branch (a) shape (i) -- see the header contract. The
    // consumer-side pacing rule (openMSX PixelRenderer.cc:580-584 /
    // SpriteChecker.hh:242-247, effect only): no background row may be committed
    // from a sprite line-buffer that is empty purely because of the lazy-open
    // begin_frame() clear. ADVANCE-ONLY-WHEN-ACTIVE: split not open -> buffers
    // hold the previous on_vsync() recompute (populated; the documented
    // 1-frame-stale fallback), so pacing is neither needed nor wanted (opening
    // here would CLEAR lines the beam already passed). check_until_visible_only
    // clamps to the frame height internally, so a wrap-space destination row
    // (e.g. a scrolling-shooter title's dy inverse mapping to display line 231 > 212) simply
    // paces the remaining visible rows. RENDER-ONLY: frame-atomic
    // collision/5th-sprite/S#0 stay untouched (DEC-0031).
    if (!sprite_split_active_) {
        return;
    }
    if (m51_sprite_trace_enabled()) {
        m51_sprite_trace("CMDSPRITE target=%d sprite_wm=%d", target,
                         sprite_engine_.watermark());
    }
    sprite_engine_.check_until_visible_only(target - 1);
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

    // Sprite check/collision/5th-sprite pass (M22-S1/S2 backlog D2; M49-S2
    // backlog D9). Once per frame boundary, mirroring the blink-countdown
    // precedent -- no new clock consumer. `height` deliberately duplicates
    // VdpFrameRenderer::height()'s own formula (sprite_frame_height()): V9958Vdp
    // (core) has no dependency on VdpFrameRenderer (presentation), keeping the
    // existing one-directional layering intact.
    // M49-S2 (backlog D9): ALWAYS the frame-atomic recompute -- collision /
    // 5th-sprite / S#0 status are byte-identical to pre-M49 for EVERY game (DEC-0031
    // boot-logo path preserved AND no game-behaviour change from the split). The
    // per-line-live sprite RENDERING split was applied DURING the frame by the seam's
    // render-only pass (commit_sprite_split -> check_until_visible_only), whose
    // visible buffers the scanline accumulator already consumed as it committed each
    // line; this recompute overwrites those buffers with the frame-atomic result the
    // finalize's remaining (uncommitted) lines render from, and computes the
    // frame-atomic collision/status the CPU reads. sprite_split_active_ rolls to
    // false so next frame's first ACTIVE-display write re-opens the render-only pass.
    const int height = sprite_frame_height();
    // M51 Task 2 trace (event class 5): the frame-atomic recompute boundary.
    if (m51_sprite_trace_enabled()) {
        m51_sprite_trace("VSYNC recompute height=%d split_was_active=%d sprite_wm=%d",
                         height, sprite_split_active_ ? 1 : 0, sprite_engine_.watermark());
        m51_sprite_trace_next_frame();
    }
    sprite_engine_.recompute_frame(vram_, control_regs_span(), mode_, height);
    sprite_split_active_ = false;

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
        // VDP command-timing parity: TR (bit7) reads 1 only when the engine holds
        // it AND the per-unit drop-then-rearm window has expired -- a poll during
        // the window sees TR=0, exactly as real HW while the VDP consumes that
        // unit's VRAM slot. Same pull-style absolute-u64 clock compare as CE/VR/HR;
        // inert without a clock (pre-parity behavior). This gates only what a
        // STATUS read returns; a COL write is always serviced regardless, so it can
        // never drop transfer data.
        const bool tr = cmd_engine_.tr() &&
                        !(clock_source_ != nullptr &&
                          tr_busy_until_cycles_ > clock_source_->cpu_total_cycles());
        if (tr) s2 = static_cast<std::uint8_t>(s2 | 0x80);
        if (cmd_engine_.bd()) s2 = static_cast<std::uint8_t>(s2 | 0x10);
        // M44 Phase 2a (DEF-M44-CMDSYNC, DEC-0069): CE (bit0) = the UNION of the
        // engine-level status bit (unchanged: event-driven transfers hold it
        // across their handshake; atomic commands clear it in command_done) and
        // the atomic busy window armed at the last R#46 write. The absolute u64
        // compare needs no wrap/epoch bookkeeping and correctly stays busy across
        // a vsync. Same pull-style clock read as the VR/HR bits above.
        const bool ce = cmd_engine_.ce() ||
                        (clock_source_ != nullptr &&
                         cmd_busy_until_cycles_ > clock_source_->cpu_total_cycles());
        if (ce) s2 = static_cast<std::uint8_t>(s2 | 0x01);
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
