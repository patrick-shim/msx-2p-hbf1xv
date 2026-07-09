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

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <system_error>

#include "devices/chipset/s1985_engine.h"

// Suite: Devices_ChipsetBackupRamSram_Unit  (M15-S5, backlog C4)
//
// S1985 16-byte backup-RAM .sram persistence: write via switched-I/O ID 0xFE,
// save, reload into a fresh engine -> identical bytes. Absent file -> zero state
// (M11 golden preserved). Grounding: fact-sheet §6; MSXS1985.cc.

namespace {

using sony_msx::devices::chipset::S1985Engine;

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

// Write one backup-RAM byte through the switched-I/O register protocol:
// port 1 = address (4-bit), port 2 = data.
void write_byte(S1985Engine& eng, std::uint8_t index, std::uint8_t value) {
    eng.switched_write(0x41, index);  // (port & 0x0F)==1 -> address
    eng.switched_write(0x42, value);  // (port & 0x0F)==2 -> data
}

}  // namespace

int main() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "sony_msx_m15_backup_ram_test.sram";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    // --- Absent file -> load fails, state stays zero (M11 golden). ---
    {
        S1985Engine eng;
        eng.reset();
        if (!expect_true(!eng.load_backup_ram(path), "AbsentFile_LoadReturnsFalse")) {
            return 1;
        }
        bool all_zero = true;
        for (std::uint8_t i = 0; i < S1985Engine::kBackupRamBytes; ++i) {
            if (eng.backup_byte(i) != 0) {
                all_zero = false;
            }
        }
        if (!expect_true(all_zero, "AbsentFile_StateZero")) {
            return 1;
        }
    }

    // --- Write 16 bytes, save, reload into a fresh engine -> identical. ---
    {
        S1985Engine writer;
        writer.reset();
        for (std::uint8_t i = 0; i < S1985Engine::kBackupRamBytes; ++i) {
            write_byte(writer, i, static_cast<std::uint8_t>(0xA0 + i));
        }
        if (!expect_true(writer.save_backup_ram(path), "Save_Succeeds")) {
            return 1;
        }

        S1985Engine reader;
        reader.reset();  // zero, then load
        if (!expect_true(reader.load_backup_ram(path), "Reload_Succeeds")) {
            return 1;
        }
        bool identical = true;
        for (std::uint8_t i = 0; i < S1985Engine::kBackupRamBytes; ++i) {
            if (reader.backup_byte(i) != static_cast<std::uint8_t>(0xA0 + i)) {
                identical = false;
            }
        }
        if (!expect_true(identical, "Reload_MatchesSavedBytes")) {
            return 1;
        }
    }

    // --- reset() after load returns to zero (battery-clear determinism). ---
    {
        S1985Engine eng;
        eng.reset();
        eng.load_backup_ram(path);
        eng.reset();
        if (!expect_true(eng.backup_byte(0) == 0, "ResetAfterLoad_ClearsToZero")) {
            return 1;
        }
    }

    std::filesystem::remove(path, ec);
    return 0;
}
