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

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <system_error>

#include "devices/memory/battery_backed_sram.h"

// Suite: Devices_MemoryBatteryBackedSram_Unit  (M17-S4, backlog B4)
//
// Reusable, parametric-size, deterministic battery-backed byte store,
// generalizing S1985Engine::load_backup_ram/save_backup_ram. Unit-tested
// standalone at 16 KB (0x4000), matching RomHalnote.cc's SRAM size exactly.
// NOT wired to any slot in M17 (§3.3, DEC-0012) -- no machine-level test here
// by design; see the implementation report.

namespace {

using sony_msx::devices::memory::BatteryBackedSram;

constexpr std::size_t kHalnoteSramBytes = 0x4000;  // 16 KB, RomHalnote.cc:44

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

}  // namespace

int main() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "sony_msx_m17_battery_backed_sram_test.bin";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    // --- Construct at 16 KB, zero-initialized. ---
    {
        BatteryBackedSram sram(kHalnoteSramBytes);
        if (!expect_true(sram.size() == kHalnoteSramBytes, "Construct_16KB_SizeExact")) {
            return 1;
        }
        bool all_zero = true;
        for (std::size_t i = 0; i < sram.size(); ++i) {
            if (sram.read(i) != 0) {
                all_zero = false;
            }
        }
        if (!expect_true(all_zero, "Construct_ZeroInitialized")) {
            return 1;
        }
    }

    // --- Absent file -> load() returns false, store stays zero. ---
    {
        BatteryBackedSram sram(kHalnoteSramBytes);
        if (!expect_true(!sram.load(path), "AbsentFile_LoadReturnsFalse")) {
            return 1;
        }
        bool all_zero = true;
        for (std::size_t i = 0; i < sram.size(); ++i) {
            if (sram.read(i) != 0) {
                all_zero = false;
            }
        }
        if (!expect_true(all_zero, "AbsentFile_StateStaysZero")) {
            return 1;
        }
    }

    // --- Write bytes, save(), construct a FRESH instance, load() -> identical bytes. ---
    {
        BatteryBackedSram writer(kHalnoteSramBytes);
        for (std::size_t i = 0; i < writer.size(); ++i) {
            writer.write(i, static_cast<std::uint8_t>((i * 37 + 11) & 0xFF));
        }
        if (!expect_true(writer.save(path), "Save_Succeeds")) {
            return 1;
        }

        BatteryBackedSram reader(kHalnoteSramBytes);
        if (!expect_true(reader.load(path), "Reload_Succeeds")) {
            return 1;
        }
        bool identical = true;
        for (std::size_t i = 0; i < reader.size(); ++i) {
            if (reader.read(i) != static_cast<std::uint8_t>((i * 37 + 11) & 0xFF)) {
                identical = false;
            }
        }
        if (!expect_true(identical, "Reload_MatchesSavedBytes")) {
            return 1;
        }
    }

    // --- Determinism across two independent round-trips. ---
    {
        const std::filesystem::path path_b =
            std::filesystem::temp_directory_path() / "sony_msx_m17_battery_backed_sram_test_b.bin";
        std::error_code ec_b;
        std::filesystem::remove(path_b, ec_b);

        auto round_trip = [](const std::filesystem::path& p) {
            BatteryBackedSram writer(kHalnoteSramBytes);
            for (std::size_t i = 0; i < writer.size(); ++i) {
                writer.write(i, static_cast<std::uint8_t>((i ^ 0x5A) & 0xFF));
            }
            writer.save(p);
            BatteryBackedSram reader(kHalnoteSramBytes);
            reader.load(p);
            return reader;
        };

        const BatteryBackedSram a = round_trip(path);
        const BatteryBackedSram b = round_trip(path_b);
        bool identical = true;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a.read(i) != b.read(i)) {
                identical = false;
            }
        }
        if (!expect_true(identical, "TwoRoundTrips_Deterministic_IdenticalBytes")) {
            return 1;
        }
        std::filesystem::remove(path_b, ec_b);
    }

    // --- Too-short/corrupt file behaves like "absent" (untouched, load() false). ---
    {
        const std::filesystem::path short_path =
            std::filesystem::temp_directory_path() / "sony_msx_m17_battery_backed_sram_short.bin";
        std::error_code ec_short;
        std::filesystem::remove(short_path, ec_short);
        {
            std::ofstream short_file(short_path, std::ios::binary | std::ios::trunc);
            const std::array<char, 4> junk{'A', 'B', 'C', 'D'};  // far shorter than 16 KB
            short_file.write(junk.data(), static_cast<std::streamsize>(junk.size()));
        }

        BatteryBackedSram sram(kHalnoteSramBytes);
        sram.write(0, 0x77);  // pre-existing state to prove it is untouched on failure
        if (!expect_true(!sram.load(short_path), "ShortFile_LoadReturnsFalse")) {
            return 1;
        }
        if (!expect_true(sram.read(0) == 0x77, "ShortFile_StoreUntouched")) {
            return 1;
        }
        std::filesystem::remove(short_path, ec_short);
    }

    // --- clear() resets to deterministic zero regardless of prior content. ---
    {
        BatteryBackedSram sram(kHalnoteSramBytes);
        sram.write(0, 0xFF);
        sram.write(kHalnoteSramBytes - 1, 0xFF);
        sram.clear();
        bool all_zero = true;
        for (std::size_t i = 0; i < sram.size(); ++i) {
            if (sram.read(i) != 0) {
                all_zero = false;
            }
        }
        if (!expect_true(all_zero, "Clear_ResetsToZero")) {
            return 1;
        }
    }

    std::filesystem::remove(path, ec);
    return 0;
}
