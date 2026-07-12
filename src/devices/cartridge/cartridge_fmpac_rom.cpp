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

#include "devices/cartridge/cartridge_fmpac_rom.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace sony_msx::devices::cartridge {

namespace {

// The openMSX FM-PAC `.sram` wrapper header, byte-for-byte
// (references/openmsx-21.0/src/sound/MSXFmPac.cc:7
//   `static constexpr const char* const PAC_Header = "PAC2 BACKUP DATA";`).
// Kept as an explicit char array (NOT a NUL-terminated literal) so exactly the
// 16 payload bytes are written -- matching SRAM.cc:121-124 `strlen(header)`.
constexpr std::array<char, CartridgeFmPacRom::kSramHeaderSize> kSramFileHeader = {
    'P', 'A', 'C', '2', ' ', 'B', 'A', 'C', 'K', 'U', 'P', ' ', 'D', 'A', 'T', 'A'};

}  // namespace

bool CartridgeFmPacRom::is_valid_image_size(const std::size_t size) {
    // 1..4 whole 16 KB banks. The real FM-PAC BIOS is exactly one 16 KB bank;
    // 64 KB 4-bank variants exist (MSXFmPac.cc:53-54 rom[bank*0x4000 + ...]).
    return size >= kBankSize && (size % kBankSize) == 0 && size <= 4 * kBankSize;
}

CartridgeFmPacRom::CartridgeFmPacRom(std::vector<std::uint8_t> image) : rom_(std::move(image)) {
    num_banks_ = rom_.empty() ? 1 : (rom_.size() / kBankSize);
    if (num_banks_ == 0) {
        num_banks_ = 1;
    }
    // Deliberately NO reset() here (M19 convention): CartridgeSlot::load()
    // resets once before installing.
}

void CartridgeFmPacRom::reset() {
    // MSXFmPac.cc:17-25 reset(): enable=0, sramEnabled=false, bank=0,
    // r1ffe=r1fff=0 (any non-magic value works). The battery SRAM is NOT
    // cleared -- it survives a power cycle (that is the whole point of a
    // battery-backed store; a fresh cold boot loads it from the .sram file).
    enable_ = 0;
    sram_enabled_ = false;
    bank_ = 0;
    r1ffe_ = 0;
    r1fff_ = 0;
    opll_.reset();
}

std::uint8_t CartridgeFmPacRom::rom_read(const std::uint16_t rel) const {
    // Bank masked against the ACTUAL bank count so a bank>available never reads
    // past a small image (planner §2.1). rel is already < 0x4000.
    const std::size_t index = (static_cast<std::size_t>(bank_ % num_banks_) * kBankSize) + rel;
    if (index >= rom_.size()) {
        return 0xFF;
    }
    return rom_[index];
}

core::BusData CartridgeFmPacRom::mem_read(const core::BusAddress address) {
    // PAGE-1 device only: open bus on pages 0/2/3 (a bare FM-PAC answers only
    // 0x4000-0x7FFF). This is what keeps the slot-3-0 memory-mapper reachable
    // on the other pages and coexists with #A8 primary-slot decode (AC-d4).
    if (address < 0x4000 || address >= 0x8000) {
        return 0xFF;
    }
    const std::uint16_t rel = static_cast<std::uint16_t>(address & 0x3FFF);
    switch (rel) {
        case 0x3FF6:
            return enable_;
        case 0x3FF7:
            return bank_;
        default:
            if (sram_enabled_) {
                if (rel < kSramWindow) {
                    return sram_.read(rel);
                }
                if (rel == 0x1FFE) {
                    return r1ffe_;  // always 0x4D while unlocked
                }
                if (rel == 0x1FFF) {
                    return r1fff_;  // always 0x69 while unlocked
                }
                return 0xFF;
            }
            return rom_read(rel);
    }
}

void CartridgeFmPacRom::mem_write(const core::BusAddress address, const core::BusData value) {
    if (address < 0x4000 || address >= 0x8000) {
        return;  // page-1 device: writes elsewhere are ignored
    }
    const std::uint16_t rel = static_cast<std::uint16_t>(address & 0x3FFF);
    const std::uint8_t v = static_cast<std::uint8_t>(value);
    switch (rel) {
        case 0x1FFE:
            // 'enable' bit4 gates the magic latches (MSXFmPac.cc:84-89): while
            // the force-reset bit is set the magic registers cannot be armed.
            if ((enable_ & 0x10) == 0) {
                r1ffe_ = v;
                check_sram_enable();
            }
            break;
        case 0x1FFF:
            if ((enable_ & 0x10) == 0) {
                r1fff_ = v;
                check_sram_enable();
            }
            break;
        case 0x3FF4:  // OPLL address port (memory-mapped)
            opll_.write_address(v);
            break;
        case 0x3FF5:  // OPLL data port (memory-mapped)
            opll_.write_data(v);
            break;
        case 0x3FF6:
            // MSXFmPac.cc:100-106. bit0 = I/O-port enable, bit4 = force-reset
            // of the magic latches. Setting bit4 clears r1ffe/r1fff (re-locking
            // the SRAM).
            enable_ = static_cast<std::uint8_t>(v & 0x11);
            if (enable_ & 0x10) {
                r1ffe_ = 0;
                r1fff_ = 0;
                check_sram_enable();
            }
            break;
        case 0x3FF7:
            // 16 KB ROM bank select (MSXFmPac.cc:107-113). 'enable' has no
            // effect on memory-mapped access.
            bank_ = static_cast<std::uint8_t>(v & 0x03);
            break;
        default:
            // SRAM byte write, ONLY while unlocked and within the addressable
            // 8190-byte window (MSXFmPac.cc:114-117). 'enable' does not gate
            // memory-mapped SRAM writes.
            if (sram_enabled_ && rel < kSramWindow) {
                sram_.write(rel, v);
            }
            break;
    }
}

void CartridgeFmPacRom::check_sram_enable() {
    // MSXFmPac.cc:137-144: both magic bytes required (AND, not either-or).
    sram_enabled_ = (r1ffe_ == 0x4D) && (r1fff_ == 0x69);
}

bool CartridgeFmPacRom::load_sram(const std::filesystem::path& path) {
    // Data-safety contract (the human has real FM-PAC saves): a load NEVER
    // writes the host file; it only reads. A failed/ambiguous read leaves the
    // (deterministic zero) store UNTOUCHED. The one destructive step -- replacing
    // an old file -- happens ONLY on a deliberate save_sram()/flush of a
    // fully-formed new-format image.
    sram_migrated_from_legacy_ = false;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;  // absent/unreadable -> deterministic blank (never fabricated)
    }
    const std::vector<char> raw((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());

    // NEW openMSX format: 16-byte "PAC2 BACKUP DATA" wrapper header + 8190 data
    // bytes (SRAM.cc:92-100 validates the header, then reads the data). Size and
    // header must BOTH match -- otherwise it is corrupt and we fail SAFE (blank),
    // exactly like openMSX's "Warning no correct SRAM file" path (SRAM.cc:101-104).
    if (raw.size() == kSramFileBytes &&
        std::equal(kSramFileHeader.begin(), kSramFileHeader.end(), raw.begin())) {
        for (std::size_t i = 0; i < kSramWindow; ++i) {
            sram_.write(i, static_cast<std::uint8_t>(raw[kSramHeaderSize + i]));
        }
        return true;
    }

    // LEGACY raw-8192 format (our pre-M43 headerless save): migrate LOSSLESSLY.
    // Carry the 8190 addressable bytes (rel 0x0000..0x1FFD) forward; the 2
    // trailing bytes (0x1FFE/0x1FFF) are the magic-register shadows, NOT real
    // SRAM, so dropping them loses nothing (MSXFmPac.cc:11,46-49). The new-format
    // file is written only on the next save_sram()/flush -- the original raw file
    // stays intact until then, so an interrupted session never loses the save.
    if (raw.size() == kLegacyRawBytes) {
        for (std::size_t i = 0; i < kSramWindow; ++i) {
            sram_.write(i, static_cast<std::uint8_t>(raw[i]));
        }
        sram_migrated_from_legacy_ = true;
        std::cerr << "note: migrated legacy raw-8192 FM-PAC SRAM save '" << path.string()
                  << "' to the openMSX-compatible format (written on next flush; "
                     "original preserved until then)\n";
        return true;
    }

    // Anything else (short/truncated/wrong-size/wrong-header) -> fail SAFE: leave
    // the store at its deterministic default, do not partial-load garbage.
    return false;
}

bool CartridgeFmPacRom::save_sram(const std::filesystem::path& path) const {
    // Build the openMSX-exact file image in memory first: the 16-byte
    // "PAC2 BACKUP DATA" header (SRAM.cc:121-124) then the 8190 addressable data
    // bytes (SRAM.cc:125). Byte-identical to openMSX's SRAM::save() output for the
    // same content, so <cart>.rom.sram is cross-emulator interchangeable.
    std::vector<char> image;
    image.reserve(kSramFileBytes);
    image.insert(image.end(), kSramFileHeader.begin(), kSramFileHeader.end());
    for (std::size_t i = 0; i < kSramWindow; ++i) {
        image.push_back(static_cast<char>(sram_.read(i)));
    }

    // ATOMIC write for data safety: write the full image to a sibling temp file,
    // then rename it over the target. If the write fails mid-way, the temp file
    // (not the real save) is the casualty; the existing save is only replaced once
    // the new image is completely on disk. (openMSX writes in place; we harden it
    // because the human has irreplaceable saves.)
    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out.write(image.data(), static_cast<std::streamsize>(image.size()));
        out.flush();
        if (!out) {
            std::error_code rm;
            std::filesystem::remove(tmp, rm);
            return false;
        }
    }  // stream closed before rename

    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        // Some platforms refuse rename onto an existing file: remove + retry.
        std::error_code rm;
        std::filesystem::remove(path, rm);
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(tmp, rm);  // clean up the temp on total failure
            return false;
        }
    }
    return true;
}

}  // namespace sony_msx::devices::cartridge
