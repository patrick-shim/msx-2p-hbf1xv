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

#include "machine/debug_snapshot.h"

#include "devices/audio/ym2413_synth.h"
#include "devices/cartridge/cartridge_mapper_device.h"
#include "devices/cartridge/cartridge_mapper_type.h"
#include "devices/cartridge/cartridge_rom_window.h"
#include "devices/memory/battery_backed_sram.h"
#include "devices/video/vdp_mode.h"
#include "machine/debug_dump.h"
#include "machine/debug_format.h"

namespace sony_msx::machine::debug_snapshot {

namespace {

using debug_dump::serialize_region;
using debug_format::flag_string;
using debug_format::to_dec;
using debug_format::to_hex;

// Boolean -> "1"/"0" (matches the CPU dump's IFF1=1 discipline).
[[nodiscard]] std::string b01(const bool value) { return value ? "1" : "0"; }

// Signed decimal (raster line can be negative in the border region; INT_MIN is
// the "no clock attached" sentinel -- a real machine always has one).
[[nodiscard]] std::string sdec(const long long value) {
    if (value < 0) {
        return "-" + to_dec(static_cast<std::uint64_t>(-value));
    }
    return to_dec(static_cast<std::uint64_t>(value));
}

[[nodiscard]] std::string vdp_mode_name(const devices::video::VdpMode mode) {
    using devices::video::VdpMode;
    switch (mode) {
    case VdpMode::Graphic1: return "GRAPHIC1";
    case VdpMode::Text1: return "TEXT1";
    case VdpMode::Multicolor: return "MULTICOLOR";
    case VdpMode::Graphic2: return "GRAPHIC2";
    case VdpMode::Text1Q: return "TEXT1Q";
    case VdpMode::MulticolorQ: return "MULTICOLORQ";
    case VdpMode::Graphic3: return "GRAPHIC3";
    case VdpMode::Text2: return "TEXT2";
    case VdpMode::Graphic4: return "GRAPHIC4";
    case VdpMode::Graphic5: return "GRAPHIC5";
    case VdpMode::Graphic6: return "GRAPHIC6";
    case VdpMode::Graphic7: return "GRAPHIC7";
    case VdpMode::ScreenYjk: return "SCREEN_YJK";
    case VdpMode::ScreenYjkYae: return "SCREEN_YJK_YAE";
    case VdpMode::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

[[nodiscard]] std::string fdc_phase_name(const std::uint8_t code) {
    switch (code) {
    case 0: return "Idle";
    case 1: return "Type1Seek";
    case 2: return "ReadSector";
    case 3: return "WriteSector";
    case 4: return "ReadAddress";
    case 5: return "ReadTrack";
    case 6: return "WriteTrack";
    default: return "Unknown";
    }
}

}  // namespace

std::string frame_file(const std::string& body) {
    std::string out;
    out += kSnapshotFormatTag;
    out.push_back('\n');
    out += body;
    out += "[END]\n";
    return out;
}

std::uint32_t checksum(const std::uint8_t* data, const std::size_t size) {
    // FNV-1a 32-bit -- deterministic, locale-free, no wall-clock/RNG.
    std::uint32_t hash = 0x811C9DC5u;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 0x01000193u;
    }
    return hash;
}

std::uint32_t checksum(const std::string& text) {
    return checksum(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}

std::string cpu_section(const devices::cpu::Z80aState& state) {
    const devices::cpu::Z80aRegisters& r = state.regs();
    std::string out;
    out += "[CPU]\n";
    out += "PC=" + to_hex(r.pc, 4) + " SP=" + to_hex(r.sp, 4) + "\n";
    out += "AF=" + to_hex(r.af, 4) + " BC=" + to_hex(r.bc, 4) + " DE=" + to_hex(r.de, 4) +
           " HL=" + to_hex(r.hl, 4) + "\n";
    out += "AF'=" + to_hex(r.af_shadow, 4) + " BC'=" + to_hex(r.bc_shadow, 4) +
           " DE'=" + to_hex(r.de_shadow, 4) + " HL'=" + to_hex(r.hl_shadow, 4) + "\n";
    out += "IX=" + to_hex(r.ix, 4) + " IY=" + to_hex(r.iy, 4) + "\n";
    // The comprehensive CPU serializer's delta over the golden serialize_cpu:
    // the internal WZ/MEMPTR + Q latch (restore-ready; planner §2.4 item 1).
    out += "WZ=" + to_hex(r.wz, 4) + " Q=" + to_hex(r.q, 2) + "\n";
    out += "A=" + to_hex(r.a(), 2) + " F=" + to_hex(r.f(), 2) + "[" + flag_string(r.f()) + "]" +
           " B=" + to_hex(r.b(), 2) + " C=" + to_hex(r.c(), 2) + " D=" + to_hex(r.d(), 2) +
           " E=" + to_hex(r.e(), 2) + " H=" + to_hex(r.h(), 2) + " L=" + to_hex(r.l(), 2) + "\n";
    out += "I=" + to_hex(r.i, 2) + " R=" + to_hex(r.r, 2) + " IFF1=" + b01(state.iff1()) +
           " IFF2=" + b01(state.iff2()) +
           " IM=" + to_dec(static_cast<std::uint8_t>(state.interrupt_mode())) +
           " HALT=" + b01(state.halted()) + "\n";
    out += "EI_DELAY=" + b01(state.ei_delay_active()) +
           " MASKABLE_PENDING=" + b01(state.maskable_interrupt_pending()) +
           " NMI_PENDING=" + b01(state.nmi_pending()) +
           " INT_BUS_VECTOR=" + to_hex(state.interrupt_bus_vector(), 2) +
           " INT_VECTOR_SUPPLIED=" + b01(state.interrupt_vector_supplied()) + "\n";
    out += "TSTATES=" + to_dec(state.total_tstates()) + "\n";
    return out;
}

std::string dram_section(const MemoryRegion& dram) {
    return serialize_region("DRAM", dram.data(), dram.size());
}

std::string mapper_section(const devices::chipset::MapperIo& mapper,
                           const std::array<std::uint8_t, 4>& readback) {
    std::string out;
    out += "[MAPPER]\n";
    out += "SEG0=" + to_hex(mapper.segment(0), 2) + " SEG1=" + to_hex(mapper.segment(1), 2) +
           " SEG2=" + to_hex(mapper.segment(2), 2) + " SEG3=" + to_hex(mapper.segment(3), 2) + "\n";
    out += "READBACK0=" + to_hex(readback[0], 2) + " READBACK1=" + to_hex(readback[1], 2) +
           " READBACK2=" + to_hex(readback[2], 2) + " READBACK3=" + to_hex(readback[3], 2) + "\n";
    return out;
}

std::string vram_section(const devices::video::VdpVram& vram) {
    return serialize_region("VRAM", vram.data(), vram.size());
}

std::string vdp_section(const devices::video::V9958Vdp& vdp, const std::uint64_t cycles_since_vsync,
                        const int vdp_cycle_position) {
    std::string out;

    out += "[VDP.REGS]\n";
    for (int i = 0; i < devices::video::V9958Vdp::kNumControlRegs; ++i) {
        out += "R" + to_hex(static_cast<std::uint8_t>(i), 2) + "=" +
               to_hex(vdp.control_register(i), 2);
        out += ((i % 8) == 7) ? "\n" : " ";
    }

    out += "[VDP.CMDREGS]\n";
    for (int i = 0; i < 15; ++i) {  // R#32..R#46
        out += "R" + to_dec(static_cast<std::uint64_t>(32 + i)) + "=" +
               to_hex(vdp.cmd_engine().read_register(i), 2);
        out += ((i % 8) == 7 || i == 14) ? "\n" : " ";
    }

    out += "[VDP.STATUS]\n";
    for (int i = 0; i < devices::video::V9958Vdp::kNumStatusRegs; ++i) {
        out += "S" + to_dec(static_cast<std::uint64_t>(i)) + "=" +
               to_hex(vdp.peek_status_register(i), 2);
        out += (i == devices::video::V9958Vdp::kNumStatusRegs - 1) ? "\n" : " ";
    }

    out += "[VDP.LATCH]\n";
    out += "DATA_LATCH=" + to_hex(vdp.data_latch(), 2) +
           " REG_STORED=" + b01(vdp.register_data_stored()) +
           " PAL_STORED=" + b01(vdp.palette_data_stored()) + "\n";

    out += "[VDP.VRAMPTR]\n";
    out += "VRAM_POINTER=" + to_hex(vdp.vram_pointer(), 4) +
           " VRAM_ADDRESS=" + to_hex(vdp.vram_address(), 5) +
           " WRITE_ACCESS=" + b01(vdp.write_access()) +
           " CPU_VRAM_DATA=" + to_hex(vdp.cpu_vram_data(), 2) + "\n";

    out += "[VDP.PALETTE]\n";
    for (int i = 0; i < devices::video::V9958Vdp::kNumPaletteEntries; ++i) {
        out += "P" + to_hex(static_cast<std::uint8_t>(i), 2) + "=" + to_hex(vdp.palette_entry(i), 4);
        out += ((i % 8) == 7) ? "\n" : " ";
    }

    const devices::video::VdpModeState& m = vdp.mode();
    out += "[VDP.MODE]\n";
    out += "MODE=" + vdp_mode_name(m.mode) + " BASE=" + to_hex(m.base, 2) + " YJK=" + b01(m.yjk) +
           " YAE=" + b01(m.yae) + "\n";

    out += "[VDP.IRQ]\n";
    out += "IRQ_ACTIVE=" + b01(vdp.irq_active()) + " IRQ_VERTICAL=" + b01(vdp.irq_vertical()) +
           " IRQ_HORIZONTAL=" + b01(vdp.irq_horizontal()) + " IRQ_LEVEL=" + b01(vdp.irq_level()) +
           " STATUS_REG0=" + to_hex(vdp.status_reg0(), 2) + " EO_FIELD=" + b01(vdp.eo_field()) + "\n";

    out += "[VDP.BLINK]\n";
    out += "BLINK_STATE=" + b01(vdp.blink_state()) +
           " BLINK_COUNTDOWN=" + to_dec(static_cast<std::uint64_t>(vdp.blink_countdown())) + "\n";

    out += "[VDP.RASTER]\n";
    out += "RASTER_LINE=" + sdec(vdp.raster_display_line()) +
           " CYCLES_SINCE_VSYNC=" + to_dec(cycles_since_vsync) +
           " VDP_CYCLE_POS=" + sdec(vdp_cycle_position) + "\n";

    const devices::video::SpriteEngine& sp = vdp.sprite_engine();
    out += "[VDP.SPRITE]\n";
    out += "SPRITE_STATUS=" + to_hex(sp.status_bits(), 2) + " COLLISION_X=" + sdec(sp.collision_x()) +
           " COLLISION_Y=" + sdec(sp.collision_y()) + "\n";

    const devices::video::VdpCommandEngine& ce = vdp.cmd_engine();
    out += "[VDP.CMD]\n";
    out += "CE=" + b01(ce.ce()) + " BD=" + b01(ce.bd()) + " TR=" + b01(ce.tr()) +
           " STATUS=" + to_hex(ce.status_byte(), 2) + " SCR_MODE=" + sdec(ce.scr_mode()) +
           " COLOR=" + to_hex(ce.color(), 2) +
           " BORDER_X=" + to_dec(static_cast<std::uint64_t>(ce.border_x())) + "\n";
    out += "SX=" + to_dec(ce.sx()) + " SY=" + to_dec(ce.sy()) + " DX=" + to_dec(ce.dx()) +
           " DY=" + to_dec(ce.dy()) + " NX=" + to_dec(ce.nx()) + " NY=" + to_dec(ce.ny()) +
           " ASX=" + to_dec(ce.asx()) + " ARG=" + to_hex(ce.arg(), 2) + " CMD=" + to_hex(ce.cmd(), 2) +
           "\n";
    out += "TRANSFER_PENDING=" + b01(ce.transfer_pending()) +
           " TRANSFER_KIND=" + to_dec(ce.transfer_kind_code()) +
           " TRANSFER_OP=" + to_dec(ce.transfer_op_code()) +
           " TRANSFER_TRANSPARENT=" + b01(ce.transfer_transparent()) + "\n";
    out += "TRANSFER_ADX=" + to_dec(ce.transfer_adx()) + " TRANSFER_DY=" + to_dec(ce.transfer_dy()) +
           " TRANSFER_DX_START=" + to_dec(ce.transfer_dx_start()) +
           " TRANSFER_ANX=" + to_dec(ce.transfer_anx()) +
           " TRANSFER_TMP_NX=" + to_dec(ce.transfer_tmp_nx()) +
           " TRANSFER_TMP_NY=" + to_dec(ce.transfer_tmp_ny()) +
           " TRANSFER_TX=" + sdec(ce.transfer_tx()) + " TRANSFER_TY=" + sdec(ce.transfer_ty()) + "\n";
    return out;
}

namespace {

std::string opll_regs_section(const std::string& label, const devices::audio::Ym2413Opll& opll) {
    std::string out;
    out += "[" + label + "]\n";
    for (std::size_t i = 0; i < devices::audio::Ym2413Opll::kRegisterCount; ++i) {
        out += "R" + to_hex(static_cast<std::uint8_t>(i), 2) + "=" +
               to_hex(opll.register_value(static_cast<std::uint8_t>(i)), 2);
        out += ((i % 8) == 7) ? "\n" : " ";
    }
    out += "LATCH=" + to_hex(opll.address_latch(), 2) + "\n";
    return out;
}

}  // namespace

std::string audio_section(const devices::audio::PsgYm2149& psg,
                          const devices::audio::Ym2413Opll& opll,
                          const devices::cartridge::CartridgeFmPacRom* fmpac) {
    std::string out;

    out += "[PSG.REGS]\n";
    for (std::uint8_t i = 0; i < devices::audio::PsgYm2149::kRegisterCount; ++i) {
        out += "R" + to_hex(i, 2) + "=" + to_hex(psg.register_value(i), 2);
        out += ((i % 8) == 7) ? "\n" : " ";
    }
    out += "LATCH=" + to_hex(psg.selected_register(), 2) + "\n";

    const devices::audio::PsgYm2149::GeneratorSnapshot g = psg.generator_snapshot();
    out += "[PSG.GEN]\n";
    for (int c = 0; c < 3; ++c) {
        out += "TONE" + to_dec(static_cast<std::uint64_t>(c)) +
               "_COUNT=" + sdec(g.tone_count[static_cast<std::size_t>(c)]) + " TONE" +
               to_dec(static_cast<std::uint64_t>(c)) +
               "_OUTPUT=" + sdec(g.tone_output[static_cast<std::size_t>(c)]) +
               " TONE_PERIOD=" + to_dec(psg.tone_period(c)) + "\n";
    }
    out += "NOISE_COUNT=" + sdec(g.noise_count) + " NOISE_LFSR=" + to_hex(g.noise_lfsr, 6) +
           " NOISE_OUTPUT=" + sdec(g.noise_output) +
           " NOISE_PERIOD=" + to_dec(psg.noise_period()) + "\n";
    out += "ENV_COUNT=" + sdec(g.envelope_count) + " ENV_STEP=" + sdec(g.envelope_step) +
           " ENV_ATTACK=" + sdec(g.envelope_attack) + " ENV_HOLD=" + b01(g.envelope_hold) +
           " ENV_ALTERNATE=" + b01(g.envelope_alternate) + " ENV_HOLDING=" + b01(g.envelope_holding) +
           " ENV_PERIOD=" + to_dec(psg.envelope_period()) +
           " ENV_VOLUME=" + to_dec(psg.envelope_volume()) + "\n";
    out += "CYCLE_RESIDUAL=" + to_dec(g.cycle_residual) +
           " INTEGRAL0=" + to_dec(g.level_dwell_integral[0]) +
           " INTEGRAL1=" + to_dec(g.level_dwell_integral[1]) +
           " INTEGRAL2=" + to_dec(g.level_dwell_integral[2]) + "\n";

    out += opll_regs_section("OPLL.REGS", opll);
    out += "[OPLL.TIMING]\n";
    out += "WRITE_TIMING_ENFORCED=" + b01(opll.write_timing_enforced()) +
           " HAS_LAST_WRITE=" + b01(opll.has_last_write()) +
           " LAST_WRITE_WAS_ADDRESS=" + b01(opll.last_write_was_address()) +
           " LAST_WRITE_CYCLE=" + to_dec(opll.last_write_cycle()) + "\n";

    const devices::audio::Ym2413Synth& synth = opll.synth();
    out += "[OPLL.SYNTH]\n";
    out += "GLOBAL_COUNTER=" + to_dec(synth.global_counter()) +
           " NOISE_LFSR=" + to_hex(synth.noise_lfsr(), 6) + " SAMPLE=" + sdec(opll.fm_sample()) + "\n";
    for (int s = 0; s < devices::audio::Ym2413Synth::kSlotCount; ++s) {
        out += "SLOT" + to_hex(static_cast<std::uint8_t>(s), 2) +
               " EG_STATE=" + to_dec(static_cast<std::uint8_t>(synth.eg_state(s))) +
               " EG_LEVEL=" + to_dec(synth.eg_level(s)) + " PHASE=" + to_hex(synth.phase(s), 5) + "\n";
    }

    if (fmpac != nullptr) {
        out += opll_regs_section("FMPAC.OPLL", fmpac->opll());
    }
    return out;
}

std::string rtc_section(const devices::rtc::Rp5c01& rtc) {
    std::string out;
    out += "[RTC.REGS]\n";
    out += "LATCH=" + to_hex(rtc.address_latch(), 2) + " MODE=" + to_hex(rtc.mode_register(), 2) + "\n";
    for (std::uint8_t block = 0; block < devices::rtc::Rp5c01::kBlocks; ++block) {
        out += "B" + to_dec(block) + "=";
        for (std::uint8_t reg = 0; reg < devices::rtc::Rp5c01::kRegsPerBlock; ++reg) {
            out += to_hex(rtc.peek_register(block, reg), 1);
        }
        out += "\n";
    }
    out += "[RTC.TIME]\n";
    out += "TEST=" + to_hex(rtc.test_register(), 2) + " RESET=" + to_hex(rtc.reset_register(), 2) +
           " FRACTION=" + to_dec(rtc.fraction()) + " SECONDS=" + to_dec(rtc.seconds()) +
           " MINUTES=" + to_dec(rtc.minutes()) + " HOURS=" + to_dec(rtc.hours()) +
           " DAY_WEEK=" + to_dec(rtc.day_week()) + " DAYS=" + to_dec(rtc.days()) +
           " MONTHS=" + to_dec(rtc.months()) + " YEARS=" + to_dec(rtc.years()) +
           " LEAP=" + to_dec(rtc.leap_year()) + " LAST_TICKS=" + to_dec(rtc.last_rtc_ticks()) + "\n";
    return out;
}

std::string fdc_section(const devices::fdc::Wd2793& fdc, const devices::fdc::DiskDrive& drive,
                        const devices::fdc::DiskImage& image, const std::uint64_t now) {
    std::string out;
    out += "[FDC.REGS]\n";
    out += "STATUS=" + to_hex(fdc.peek_status(), 2) + " COMMAND=" + to_hex(fdc.command_register(), 2) +
           " TRACK=" + to_hex(fdc.track_register(), 2) + " SECTOR=" + to_hex(fdc.sector_register(), 2) +
           " DATA=" + to_hex(fdc.data_register(), 2) + "\n";

    out += "[FDC.DIAG]\n";
    out += "READ_CMDS_ACCEPTED=" + to_dec(fdc.read_sector_commands_accepted()) +
           " BYTES_TRANSFERRED=" + to_dec(fdc.read_sector_bytes_transferred()) +
           " COMPLETIONS_OK=" + to_dec(fdc.read_sector_completions_ok()) + "\n";

    out += "[FDC.FSM]\n";
    out += "PHASE=" + fdc_phase_name(fdc.phase_code()) + "(" + to_dec(fdc.phase_code()) + ")" +
           " DIRECTION_IN=" + b01(fdc.direction_in()) + " INTRQ_PENDING=" + b01(fdc.intrq_pending()) +
           " IMMEDIATE_IRQ=" + b01(fdc.immediate_irq()) +
           " INDEX_IRQ_ARMED=" + b01(fdc.index_irq_armed()) +
           " INDEX_IRQ_DEADLINE=" + to_dec(fdc.index_irq_deadline()) + "\n";
    out += "HLD_ACTIVE=" + b01(fdc.hld_active()) + " HLD_SINCE=" + to_dec(fdc.hld_since()) +
           " BUSY_UNTIL=" + to_dec(fdc.busy_until()) + " DRQ_DEADLINE=" + to_dec(fdc.drq_deadline()) +
           " LAST_SYNC=" + to_dec(fdc.last_sync()) + " DATA_INDEX=" + sdec(fdc.data_index()) +
           " DATA_AVAILABLE=" + sdec(fdc.data_available()) + "\n";
    out += "WRITE_SECTOR_NUM=" + to_hex(fdc.write_sector_num(), 2) +
           " WT_A1_RUN=" + sdec(fdc.wt_a1_run()) + " WT_EXPECT_ID=" + b01(fdc.wt_expect_id()) +
           " WT_ID_COUNT=" + sdec(fdc.wt_id_count()) + " WT_ID=" + to_hex(fdc.wt_id(0), 2) + to_hex(fdc.wt_id(1), 2) +
           to_hex(fdc.wt_id(2), 2) + to_hex(fdc.wt_id(3), 2) + " WT_IN_DATA=" + b01(fdc.wt_in_data()) +
           " WT_DATA_COUNT=" + sdec(fdc.wt_data_count()) +
           " WT_SECTOR_NUM=" + to_hex(fdc.wt_sector_num(), 2) + "\n";

    out += "[DRIVE]\n";
    out += "PHYSICAL_TRACK=" + to_dec(drive.physical_track()) + " SIDE=" + to_dec(drive.side()) +
           " AVAILABLE=" + b01(drive.available()) + " READY=" + b01(drive.ready()) +
           " TRACK00=" + b01(drive.is_track00()) + " WRITE_PROTECT=" + b01(drive.write_protected()) +
           " DISK_CHANGED=" + b01(drive.disk_changed()) + " MOTOR=" + b01(drive.motor_on(now)) +
           " MOTOR_LATCHED=" + b01(drive.motor_latched()) +
           " MOTOR_OFF_PENDING=" + b01(drive.motor_off_pending()) +
           " MOTOR_OFF_DEADLINE=" + to_dec(drive.motor_off_deadline()) + "\n";

    out += "[DISK]\n";
    out += "PRESENT=" + b01(image.present()) + " SIZE=" + to_dec(image.size()) +
           " WRITE_PROTECT=" + b01(image.write_protected()) + " DIRTY=" + b01(image.dirty()) +
           " HOST_PATH=\"" + image.host_path().string() + "\"" + " CONTENT_CHECKSUM=" +
           to_hex(checksum(image.data().data(), image.data().size()), 8) + "\n";
    return out;
}

std::string s1985_section(const devices::chipset::S1985Engine& s1985) {
    std::string out;
    out += "[S1985.REGS]\n";
    out += "ADDRESS=" + to_hex(s1985.address_register(), 2) +
           " PATTERN=" + to_hex(s1985.pattern_register(), 2) +
           " COLOR1=" + to_hex(s1985.color1_register(), 2) +
           " COLOR2=" + to_hex(s1985.color2_register(), 2) + "\n";
    std::array<std::uint8_t, devices::chipset::S1985Engine::kBackupRamBytes> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = s1985.backup_byte(static_cast<std::uint8_t>(i));
    }
    out += serialize_region("S1985.SRAM", bytes.data(), bytes.size());
    return out;
}

std::string slots_section(const std::uint8_t a8, const std::array<std::uint8_t, 4>& subslots,
                          const std::array<bool, 4>& expanded) {
    std::string out;
    out += "[SLOTS]\n";
    out += "A8=" + to_hex(a8, 2) + "\n";
    out += "SUB0=" + to_hex(subslots[0], 2) + " SUB1=" + to_hex(subslots[1], 2) +
           " SUB2=" + to_hex(subslots[2], 2) + " SUB3=" + to_hex(subslots[3], 2) + "\n";
    out += "EXP0=" + b01(expanded[0]) + " EXP1=" + b01(expanded[1]) + " EXP2=" + b01(expanded[2]) +
           " EXP3=" + b01(expanded[3]) + "\n";
    return out;
}

std::string sysctrl_section(const std::uint8_t f4, const std::uint8_t f5) {
    std::string out;
    out += "[SYSCTRL]\n";
    out += "F4=" + to_hex(f4, 2) + " F5=" + to_hex(f5, 2) + "\n";
    return out;
}

std::string clock_section(const std::uint64_t elapsed_cycles, const std::uint64_t frame_count,
                          const std::uint64_t cycles_per_frame, const std::uint64_t cycles_since_vsync,
                          const int vdp_cycle_position) {
    std::string out;
    out += "[CLOCK]\n";
    out += "ELAPSED_CYCLES=" + to_dec(elapsed_cycles) + " FRAME_COUNT=" + to_dec(frame_count) +
           " CYCLES_PER_FRAME=" + to_dec(cycles_per_frame) +
           " CYCLES_SINCE_VSYNC=" + to_dec(cycles_since_vsync) +
           " VDP_CYCLE_POS=" + sdec(vdp_cycle_position) + "\n";
    return out;
}

std::string pause_rensha_section(const devices::chipset::Mb670836PauseController& pause,
                                 const peripherals::RenshaTurbo& rensha) {
    std::string out;
    out += "[PAUSE]\n";
    out += "BUTTON_ENGAGED=" + b01(pause.button_engaged()) +
           " SPEED_LEVEL=" + sdec(pause.speed_level()) +
           " PAUSED_THIS_FRAME=" + b01(pause.speed_controller_paused_this_frame()) +
           " CPU_SHOULD_PAUSE=" + b01(pause.cpu_should_pause()) +
           " FRAME_INDEX=" + to_dec(pause.frame_index()) + "\n";
    out += "[RENSHA]\n";
    out += "SPEED=" + sdec(rensha.speed()) + " SIGNAL=" + b01(rensha.signal()) + "\n";
    return out;
}

std::string cartridge_section(const std::string& label,
                              const devices::cartridge::CartridgeSlot& slot,
                              const devices::cartridge::CartridgeFmPacRom* fmpac,
                              const devices::audio::SccWavetable* scc) {
    std::string out;
    out += "[" + label + "]\n";
    out += "LOADED=" + b01(slot.loaded());
    if (slot.loaded()) {
        out += " MAPPER_TYPE=" + std::string(devices::cartridge::to_string(slot.mapper_type())) + "(" +
               to_dec(static_cast<std::uint8_t>(slot.mapper_type())) + ")";
    }
    out += "\n";

    // Generic bank-state dump via the additive rom_window() seam (planner §2.4
    // item 13): window-based mappers expose their 8-slot window; others (FM-PAC)
    // return nullptr and are dumped through their own sections below.
    if (const devices::cartridge::CartridgeMapperDevice* mapper = slot.mapper()) {
        if (const devices::cartridge::CartridgeRomWindow* window = mapper->rom_window()) {
            out += "[" + label + ".BANKS]\n";
            for (int s = 0; s < devices::cartridge::CartridgeRomWindow::kSlots; ++s) {
                out += "SLOT" + to_dec(static_cast<std::uint64_t>(s)) + "=";
                if (window->slot_mapped(s)) {
                    out += to_dec(window->slot_bank(s));
                } else {
                    out += "unmapped";
                }
                out += ((s % 4) == 3) ? "\n" : " ";
            }
            out += "NUM_BLOCKS=" + to_dec(window->num_blocks()) +
                   " BLOCK_MASK=" + to_hex(window->block_mask(), 4) + "\n";
        }
    }

    if (fmpac != nullptr) {
        out += "[" + label + ".FMPAC.STATE]\n";
        out += "BANK=" + to_hex(fmpac->bank(), 2) + " ENABLE=" + to_hex(fmpac->enable_register(), 2) +
               " SRAM_ENABLED=" + b01(fmpac->sram_enabled()) + " R1FFE=" + to_hex(fmpac->r1ffe(), 2) +
               " R1FFF=" + to_hex(fmpac->r1fff(), 2) + "\n";
        out += serialize_region(label + ".FMPAC.SRAM", fmpac->sram().data(), fmpac->sram().size());
    }

    if (scc != nullptr) {
        out += "[" + label + ".SCC]\n";
        out += "ENABLE_BITS=" + to_hex(scc->enable_bits(), 2) + " DEFORM=" + to_hex(scc->deform(), 2) +
               "\n";
        for (int c = 0; c < 5; ++c) {
            out += "CH" + to_dec(static_cast<std::uint64_t>(c)) +
                   "_PERIOD=" + to_dec(scc->period(c)) + " CH" + to_dec(static_cast<std::uint64_t>(c)) +
                   "_VOLUME=" + to_dec(scc->volume(c)) + "\n";
        }
    }
    return out;
}

std::string halnote_section(const devices::halnote::HalnoteRom& halnote) {
    std::string out;
    out += "[HALNOTE]\n";
    out += "SRAM_ENABLED=" + b01(halnote.sram_enabled()) +
           " SUB_MAPPER_ENABLED=" + b01(halnote.sub_mapper_enabled()) +
           " SUB_BANK0=" + to_hex(halnote.sub_bank(0), 2) +
           " SUB_BANK1=" + to_hex(halnote.sub_bank(1), 2) + "\n";
    const devices::cartridge::CartridgeRomWindow& w = halnote.window();
    for (int s = 0; s < devices::cartridge::CartridgeRomWindow::kSlots; ++s) {
        out += "SLOT" + to_dec(static_cast<std::uint64_t>(s)) + "=";
        if (w.slot_mapped(s)) {
            out += to_dec(w.slot_bank(s));
        } else {
            out += "unmapped";
        }
        out += ((s % 4) == 3) ? "\n" : " ";
    }
    out += "NUM_BLOCKS=" + to_dec(w.num_blocks()) + " BLOCK_MASK=" + to_hex(w.block_mask(), 4) + "\n";
    out += serialize_region("HALNOTE.SRAM", halnote.sram().data(), halnote.sram().size());
    return out;
}

std::string peripherals_section(const devices::kanji::KanjiFontRom& kanji,
                                const peripherals::PrinterPort& printer,
                                const peripherals::CassetteInterface& cassette) {
    std::string out;
    out += "[KANJI]\n";
    out += "ADDRESS1=" + to_hex(kanji.address1(), 5) + " ADDRESS2=" + to_hex(kanji.address2(), 5) + "\n";
    out += "[PRINTER]\n";
    out += "STROBE=" + b01(printer.strobe()) + " DATA=" + to_hex(printer.data(), 2) +
           " CAPTURED_BYTES=" + to_dec(printer.captured_bytes().size()) + "\n";
    out += "[CASSETTE]\n";
    out += "MOTOR_ON=" + b01(cassette.motor_on()) + " OUTPUT_LEVEL=" + b01(cassette.output_level()) +
           " TAPE_LOADED=" + b01(cassette.tape_loaded()) +
           " INPUT_HIGH=" + b01(cassette.cassette_input_high()) + "\n";
    return out;
}

}  // namespace sony_msx::machine::debug_snapshot
