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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "devices/audio/psg_ym2149.h"
#include "devices/audio/scc_wavetable.h"
#include "devices/audio/ym2413_opll.h"
#include "devices/cartridge/cartridge_fmpac_rom.h"
#include "devices/cartridge/cartridge_slot.h"
#include "devices/chipset/mapper_io.h"
#include "devices/chipset/mb670836_pause.h"
#include "devices/chipset/s1985_engine.h"
#include "devices/cpu/z80a_state.h"
#include "devices/fdc/disk_drive.h"
#include "devices/fdc/disk_image.h"
#include "devices/fdc/wd2793.h"
#include "devices/halnote/halnote_rom.h"
#include "devices/kanji/kanji_font_rom.h"
#include "devices/rtc/rp5c01.h"
#include "devices/video/v9958_vdp.h"
#include "devices/video/vdp_vram.h"
#include "machine/memory_region.h"
#include "peripherals/cassette_interface.h"
#include "peripherals/printer_port.h"
#include "peripherals/rensha_turbo.h"

namespace sony_msx::machine::debug_snapshot {

// Comprehensive, RESTORE-READY, deterministic per-component debug-snapshot
// serializers (M36 Phase 3, DEC-0051; docs/m36-phase3-planner-package.md). This
// is a SEPARATE artifact from the golden-locked debug_dump / serialize_state_dump
// (M10/M13/M14): it never edits those, it SUPERSETS them across EVERY machine
// component (§2.3 inventory).
//
// Determinism is guaranteed by construction, exactly like debug_dump: every
// serializer is hand-rolled ASCII (fixed field order, fixed-width uppercase hex,
// '\n' endings, symbolic + numeric enum tokens), it reads NO wall-clock and NO
// RNG for content, and it reuses the proven debug_dump::serialize_region() fold
// for byte regions. Two identical runs to the same frame/cycle produce a
// byte-identical snapshot (S5 system test is the hard gate).
//
// Every accessor consumed here is a const getter or a non-perturbing seam: the
// snapshot ADVANCES neither the CPU nor the scheduler and issues no
// debug_io_write -- it is read-only w.r.t. emulation.

// Snapshot format version tag (mirrors debug_dump::kDumpFormatTag +
// frame_dump::kFrameDumpFormatTag). A future load_snapshot() reader dispatches
// on this; additive v2 fields never break a v1 reader (restore-ready, §3.3).
inline constexpr const char* kSnapshotFormatTag = "HBF1XV-SNAPSHOT v1";

// One named file within a snapshot bundle (written under
// debug/snapshot/<id>/<name>).
struct SnapshotFile {
    std::string name;
    std::string content;
};

// A full snapshot bundle: a deterministic id (frame/cycle stamp) + an ordered
// file list (manifest.txt first, then the per-component files).
struct Snapshot {
    std::string id;
    std::vector<SnapshotFile> files;
};

// Wrap a concatenation of section bodies into a complete, framed snapshot file:
//   HBF1XV-SNAPSHOT v1\n <body> [END]\n
[[nodiscard]] std::string frame_file(const std::string& body);

// Deterministic 32-bit FNV-1a checksum of a byte range / string (the manifest
// integrity anchor). Pure function of the bytes -- no wall-clock/RNG.
[[nodiscard]] std::uint32_t checksum(const std::uint8_t* data, std::size_t size);
[[nodiscard]] std::uint32_t checksum(const std::string& text);

// --- Per-section builders. Each returns a "[SECTION]...\n" body (no version
//     tag / no [END]); the machine concatenates related sections into one file
//     and wraps it with frame_file(). All const / non-perturbing. ---

[[nodiscard]] std::string cpu_section(const devices::cpu::Z80aState& state);
[[nodiscard]] std::string dram_section(const MemoryRegion& dram);
[[nodiscard]] std::string mapper_section(const devices::chipset::MapperIo& mapper,
                                         const std::array<std::uint8_t, 4>& readback);
[[nodiscard]] std::string vram_section(const devices::video::VdpVram& vram);
[[nodiscard]] std::string vdp_section(const devices::video::V9958Vdp& vdp,
                                      std::uint64_t cycles_since_vsync, int vdp_cycle_position);
[[nodiscard]] std::string audio_section(const devices::audio::PsgYm2149& psg,
                                        const devices::audio::Ym2413Opll& opll,
                                        const devices::cartridge::CartridgeFmPacRom* fmpac);
[[nodiscard]] std::string rtc_section(const devices::rtc::Rp5c01& rtc);
[[nodiscard]] std::string fdc_section(const devices::fdc::Wd2793& fdc,
                                      const devices::fdc::DiskDrive& drive,
                                      const devices::fdc::DiskImage& image, std::uint64_t now);
[[nodiscard]] std::string s1985_section(const devices::chipset::S1985Engine& s1985);
[[nodiscard]] std::string slots_section(std::uint8_t a8,
                                        const std::array<std::uint8_t, 4>& subslots,
                                        const std::array<bool, 4>& expanded);
[[nodiscard]] std::string sysctrl_section(std::uint8_t f4, std::uint8_t f5);
[[nodiscard]] std::string clock_section(std::uint64_t elapsed_cycles, std::uint64_t frame_count,
                                        std::uint64_t cycles_per_frame,
                                        std::uint64_t cycles_since_vsync, int vdp_cycle_position);
[[nodiscard]] std::string pause_rensha_section(
    const devices::chipset::Mb670836PauseController& pause, const peripherals::RenshaTurbo& rensha);
[[nodiscard]] std::string cartridge_section(const std::string& label,
                                            const devices::cartridge::CartridgeSlot& slot,
                                            const devices::cartridge::CartridgeFmPacRom* fmpac,
                                            const devices::audio::SccWavetable* scc);
[[nodiscard]] std::string halnote_section(const devices::halnote::HalnoteRom& halnote);
[[nodiscard]] std::string peripherals_section(const devices::kanji::KanjiFontRom& kanji,
                                              const peripherals::PrinterPort& printer,
                                              const peripherals::CassetteInterface& cassette);

}  // namespace sony_msx::machine::debug_snapshot
