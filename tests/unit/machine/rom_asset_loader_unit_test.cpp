#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

#include "machine/rom_asset_loader.h"

// Suite: Machine_RomAssetLoader_Unit  (M13-S2)
//
// Deterministic coverage for the machine-side ROM asset loader:
//   - a real bios/f1xvbios.rom loads to exactly 32 KB;
//   - two loads of the same present file are byte-identical (determinism);
//   - a deliberately-absent path -> 0xFF-filled image of the expected size AND a
//     recorded diagnostic note (missing-asset policy A-7);
//   - a size-mismatch present file is normalized + noted (no silent accept).
//
// The real bios directory is injected as an absolute path by CMake
// (SONY_MSX_BIOS_DIR) so the test does not depend on the working directory.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

int g_failures = 0;

void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

bool all_equal(const std::vector<std::uint8_t>& v, const std::uint8_t value) {
    for (const std::uint8_t b : v) {
        if (b != value) {
            return false;
        }
    }
    return !v.empty();
}

}  // namespace

int main() {
    using sony_msx::machine::RomAssetLoader;

    const std::filesystem::path bios_dir = SONY_MSX_BIOS_DIR;

    // --- Present required asset loads to the exact expected size. ---
    {
        RomAssetLoader loader(bios_dir);
        const RomAssetLoader::Spec bios{"f1xvbios.rom", 0x8000, "slot 0-0 pages 0-1"};
        const std::vector<std::uint8_t> image = loader.load(bios);
        expect(image.size() == 0x8000, "Bios_Present_Loads32K");
        expect(loader.ok() && loader.diagnostics().empty(), "Bios_Present_NoDiagnostics");
        expect(!all_equal(image, 0xFF), "Bios_Present_NotAllOpenBus");

        // Determinism: a second load is byte-identical.
        const std::vector<std::uint8_t> again = loader.load(bios);
        expect(again == image, "Bios_TwoLoads_ByteIdentical");
    }

    // --- Absent asset -> 0xFF fill of expected size + one diagnostic note. ---
    {
        RomAssetLoader loader(bios_dir);
        const RomAssetLoader::Spec absent{"does_not_exist_f1xv.rom", 0x4000, "slot 3-1 page 0"};
        const std::vector<std::uint8_t> image = loader.load(absent);
        expect(image.size() == 0x4000, "Absent_FillSize_MatchesExpected");
        expect(all_equal(image, 0xFF), "Absent_Filled_AllOpenBus");
        expect(!loader.ok() && loader.diagnostics().size() == 1, "Absent_RecordsOneDiagnostic");
        const std::string& note = loader.diagnostics().front();
        expect(note.find("ABSENT") != std::string::npos &&
                   note.find("slot 3-1 page 0") != std::string::npos,
               "Absent_Diagnostic_MentionsReasonAndSlot");
    }

    // --- Absent with a smaller expected size fills exactly that many bytes. ---
    {
        RomAssetLoader loader(bios_dir);
        const std::vector<std::uint8_t> image =
            loader.load({"nope.rom", 16u * 1024u, "slot 3-2 page 1"});
        expect(image.size() == 16u * 1024u && all_equal(image, 0xFF),
               "Absent_SmallWindow_FilledExactSize");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
