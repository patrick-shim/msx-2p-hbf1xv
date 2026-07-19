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

// Suite: Devices_CartridgeFmPacSramPersistence_Unit (M43 Slice 3, DEC-0062/0063)
//
// Deterministic coverage of the FM-PAC battery-SRAM PERSISTENCE format + lossless
// migration, grounded byte-for-byte in openMSX:
//   references/openmsx-21.0/src/sound/MSXFmPac.cc:7,11  (PAC_Header + 0x1FFE SRAM)
//   references/openmsx-21.0/src/memory/SRAM.cc:83-131   (header write/validate)
// The adopted `.sram` file layout is a 16-byte "PAC2 BACKUP DATA" WRAPPER header
// followed by 8190 (0x1FFE) addressable data bytes = an 8206-byte file, byte-
// identical to openMSX's SRAM::save() output for the same content.
// Covers: exact save format; new-format load (openMSX interop); save->load
// round-trip via the game register path; LOSSLESS migration of a legacy raw-8192
// save (content preserved, original file untouched by load, reflushed as the new
// format, phantom bytes dropped); and fail-SAFE rejection of wrong-header/short/
// absent files (store left untouched, never partial-loaded garbage).
//
// NON-TAUTOLOGY: the migration + save-format cases fill every one of the 8190
// addressable bytes with a distinct per-index pattern and compare all of them, so
// any dropped/shifted/truncated byte fails (verified by an adversarial mutation in
// the M43 Slice-3 implementation report).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_fmpac_rom.h"

using sony_msx::devices::cartridge::CartridgeFmPacRom;

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr std::size_t kWindow = CartridgeFmPacRom::kSramWindow;       // 8190
constexpr std::size_t kHeaderSize = CartridgeFmPacRom::kSramHeaderSize;  // 16
constexpr std::size_t kFileBytes = CartridgeFmPacRom::kSramFileBytes;    // 8206
constexpr std::size_t kLegacyBytes = CartridgeFmPacRom::kLegacyRawBytes; // 8192

const std::string kHeader = "PAC2 BACKUP DATA";  // the openMSX wrapper header

// Distinct per-index pattern so any off-by-one / drop / shift is observable.
std::uint8_t pat(const std::size_t i) {
    return static_cast<std::uint8_t>((i * 31 + 7) & 0xFF);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& p, const std::vector<std::uint8_t>& b) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

// A minimal FM-PAC-shaped 16 KB ROM (AB header). CartridgeSlot::load() semantics:
// construct then reset().
std::vector<std::uint8_t> make_rom_16k() {
    std::vector<std::uint8_t> rom(CartridgeFmPacRom::kBankSize, 0x00);
    rom[0] = 'A';
    rom[1] = 'B';
    return rom;
}

CartridgeFmPacRom fresh_cart() {
    CartridgeFmPacRom cart(make_rom_16k());
    cart.reset();
    return cart;
}

bool header_ok(const std::vector<std::uint8_t>& bytes) {
    return bytes.size() >= kHeaderSize &&
           std::equal(kHeader.begin(), kHeader.end(), bytes.begin());
}

}  // namespace

int main() {
    const std::filesystem::path temp = std::filesystem::temp_directory_path();

    // Constant sanity: the adopted layout matches openMSX exactly.
    expect(kHeaderSize == 16, "Const_HeaderIs16");
    expect(kWindow == 0x1FFE, "Const_WindowIs8190");
    expect(kFileBytes == 8206, "Const_FileIs8206");
    expect(kLegacyBytes == 8192, "Const_LegacyIs8192");

    // --- A. Save format is BYTE-EXACT: 16-byte header + 8190 data (8206 total). ---
    {
        CartridgeFmPacRom cart = fresh_cart();
        for (std::size_t i = 0; i < kWindow; ++i) {
            cart.sram().write(i, pat(i));
        }
        const std::filesystem::path path = temp / "fmpac_saveformat.sram";
        std::filesystem::remove(path);
        expect(cart.save_sram(path), "SaveFormat_SaveOk");

        const std::vector<std::uint8_t> bytes = read_file(path);
        expect(bytes.size() == kFileBytes, "SaveFormat_FileSizeIs8206");
        expect(header_ok(bytes), "SaveFormat_HeaderIsPac2BackupData");
        bool data_ok = true;
        for (std::size_t i = 0; i < kWindow; ++i) {
            if (bytes[kHeaderSize + i] != pat(i)) {
                data_ok = false;
                break;
            }
        }
        expect(data_ok, "SaveFormat_DataMatchesSram");
        std::filesystem::remove(path);
    }

    // --- B. New-format load (openMSX interop): a file in openMSX's exact layout
    //        loads correctly and is NOT treated as a migration. ---
    {
        std::vector<std::uint8_t> f;
        f.insert(f.end(), kHeader.begin(), kHeader.end());
        for (std::size_t i = 0; i < kWindow; ++i) {
            f.push_back(pat(i));
        }
        expect(f.size() == kFileBytes, "NewFormatLoad_FixtureSize");
        const std::filesystem::path path = temp / "fmpac_newfmt.sram";
        write_file(path, f);

        CartridgeFmPacRom cart = fresh_cart();
        expect(cart.load_sram(path), "NewFormatLoad_Ok");
        expect(!cart.sram_migrated_from_legacy(), "NewFormatLoad_NotMigrated");
        bool ok = true;
        for (std::size_t i = 0; i < kWindow; ++i) {
            if (cart.sram().read(i) != pat(i)) {
                ok = false;
                break;
            }
        }
        expect(ok, "NewFormatLoad_ContentMatches");
        std::filesystem::remove(path);
    }

    // --- C. Round-trip via the GAME REGISTER PATH: magic-unlock, write through
    //        the SRAM window, save, reload into a fresh cart, read back. ---
    {
        CartridgeFmPacRom cart = fresh_cart();
        cart.mem_write(0x5FFE, 0x4D);  // magic unlock
        cart.mem_write(0x5FFF, 0x69);
        cart.mem_write(0x4000, 0x11);  // rel 0x0000 (first addressable)
        cart.mem_write(0x4123, 0x77);  // rel 0x0123 (mid)
        cart.mem_write(0x5FFD, 0x22);  // rel 0x1FFD (last addressable)

        const std::filesystem::path path = temp / "fmpac_roundtrip.sram";
        std::filesystem::remove(path);
        expect(cart.save_sram(path), "RoundTrip_SaveOk");

        CartridgeFmPacRom cart2 = fresh_cart();
        expect(cart2.load_sram(path), "RoundTrip_LoadOk");
        expect(!cart2.sram_migrated_from_legacy(), "RoundTrip_NotMigrated");
        cart2.mem_write(0x5FFE, 0x4D);  // unlock to read the window
        cart2.mem_write(0x5FFF, 0x69);
        expect(cart2.mem_read(0x4000) == 0x11, "RoundTrip_BusByte0");
        expect(cart2.mem_read(0x4123) == 0x77, "RoundTrip_BusByteMid");
        expect(cart2.mem_read(0x5FFD) == 0x22, "RoundTrip_BusByteLast");
        expect(cart2.sram().read(0x0000) == 0x11, "RoundTrip_StoreByte0");
        expect(cart2.sram().read(0x1FFD) == 0x22, "RoundTrip_StoreByteLast");
        std::filesystem::remove(path);
    }

    // --- D. LOSSLESS migration of a legacy raw-8192 save. ---
    {
        // Legacy fixture: every addressable byte a distinct pattern; the two
        // phantom bytes (0x1FFE/0x1FFF) set to sentinels that MUST be dropped
        // (they are the magic-register shadows, not real SRAM).
        std::vector<std::uint8_t> legacy(kLegacyBytes, 0x00);
        for (std::size_t i = 0; i < kWindow; ++i) {
            legacy[i] = pat(i);
        }
        legacy[0x1FFE] = 0x5A;
        legacy[0x1FFF] = 0xA5;

        const std::filesystem::path path = temp / "fmpac_legacy.sram";
        write_file(path, legacy);

        CartridgeFmPacRom cart = fresh_cart();
        expect(cart.load_sram(path), "Migration_LoadOk");
        expect(cart.sram_migrated_from_legacy(), "Migration_FlagSet");

        // DATA-SAFETY: load must NOT have modified the original file (the new
        // format is only written on the next deliberate flush).
        expect(read_file(path) == legacy, "Migration_LoadDoesNotTouchFile");

        // Every addressable byte preserved (NON-TAUTOLOGY: a dropped/shifted byte
        // fails this full-content comparison).
        bool all_ok = true;
        for (std::size_t i = 0; i < kWindow; ++i) {
            if (cart.sram().read(i) != pat(i)) {
                all_ok = false;
                break;
            }
        }
        expect(all_ok, "Migration_AllAddressableBytesPreserved");

        // Reflush -> the file becomes the NEW format; the phantom sentinels are
        // gone (file is exactly header + 8190, not the 8192 raw tail).
        expect(cart.save_sram(path), "Migration_ReflushOk");
        const std::vector<std::uint8_t> reflushed = read_file(path);
        expect(reflushed.size() == kFileBytes, "Migration_ReflushIsNewFormat");
        expect(header_ok(reflushed), "Migration_ReflushHeader");
        bool rf_ok = true;
        for (std::size_t i = 0; i < kWindow; ++i) {
            if (reflushed[kHeaderSize + i] != pat(i)) {
                rf_ok = false;
                break;
            }
        }
        expect(rf_ok, "Migration_ReflushDataMatches");

        // A second load of the reflushed file is a normal new-format load (no
        // re-migration), proving migration is one-shot + idempotent thereafter.
        CartridgeFmPacRom cart3 = fresh_cart();
        expect(cart3.load_sram(path), "Migration_ReloadNewFormatOk");
        expect(!cart3.sram_migrated_from_legacy(), "Migration_ReloadNotMigratedAgain");
        expect(cart3.sram().read(0x1FFD) == pat(0x1FFD), "Migration_ReloadLastByte");
        std::filesystem::remove(path);
    }

    // --- E. Fail-SAFE: wrong header (correct size) -> false, store untouched. ---
    {
        std::vector<std::uint8_t> bad(kFileBytes, 0xCC);  // right size, bad header
        const std::filesystem::path path = temp / "fmpac_badheader.sram";
        write_file(path, bad);

        CartridgeFmPacRom cart = fresh_cart();
        cart.sram().write(0x0010, 0x99);  // seed a marker to detect any partial load
        expect(!cart.load_sram(path), "RejectHeader_LoadFalse");
        expect(!cart.sram_migrated_from_legacy(), "RejectHeader_NotMigrated");
        expect(cart.sram().read(0x0010) == 0x99, "RejectHeader_StoreUntouched");
        std::filesystem::remove(path);
    }

    // --- F. Fail-SAFE: short/truncated file -> false, store untouched. ---
    {
        const std::vector<std::uint8_t> shortf(100, 0x7E);
        const std::filesystem::path path = temp / "fmpac_short.sram";
        write_file(path, shortf);

        CartridgeFmPacRom cart = fresh_cart();
        cart.sram().write(0x0005, 0x42);
        expect(!cart.load_sram(path), "RejectShort_LoadFalse");
        expect(cart.sram().read(0x0005) == 0x42, "RejectShort_StoreUntouched");
        std::filesystem::remove(path);
    }

    // --- G. Fail-SAFE: absent file -> false, store untouched (never fabricated). ---
    {
        const std::filesystem::path path = temp / "fmpac_absent_never_exists.sram";
        std::filesystem::remove(path);

        CartridgeFmPacRom cart = fresh_cart();
        cart.sram().write(0x0009, 0x33);
        expect(!cart.load_sram(path), "Absent_LoadFalse");
        expect(cart.sram().read(0x0009) == 0x33, "Absent_StoreUntouched");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Devices_CartridgeFmPacSramPersistence_Unit cases passed\n";
    return 0;
}
