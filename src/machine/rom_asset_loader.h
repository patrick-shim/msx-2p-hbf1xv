#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sony_msx::machine {

// Machine-side ROM asset loader (M13-S2).
//
// Resolves local `bios/` image paths, reads them into byte images, validates the
// image size against the expected role size (from the machine XML `<mem>` /
// `<window>` sizes), and applies a DETERMINISTIC missing-asset policy:
//
//   A required-but-absent (or unreadable, or wrong-size) file yields a ROM image
//   filled with open-bus 0xFF of the EXACT expected size, AND a human-readable
//   diagnostic note is appended to `diagnostics()`. Never a silent zero-fill,
//   never fabricated SHA/provenance (guardrails "Asset and Script Safety";
//   planner A-7). The overall `tools/validate-assets.ps1` gate still fails if a
//   required BIOS is missing, so a green run always has real assets; the fill
//   path exists for determinism/robustness and is unit-tested with a bad path.
//
// This type keeps `devices::memory::RomDevice` ignorant of the filesystem
// (boundary rule, src/CLAUDE.md).
class RomAssetLoader {
public:
    struct Spec {
        std::string filename;        // e.g. "f1xvbios.rom"
        std::uint32_t expected_size;  // exact ROM window size in bytes
        std::string label;           // slot/role label for the diagnostic note
    };

    explicit RomAssetLoader(std::filesystem::path root);

    // Load `spec` and return an image of EXACTLY `spec.expected_size` bytes.
    // Present + correct size -> the file bytes. Absent / unreadable / wrong size
    // -> 0xFF-fill (resized to the expected size) plus a recorded diagnostic.
    [[nodiscard]] std::vector<std::uint8_t> load(const Spec& spec);

    [[nodiscard]] const std::vector<std::string>& diagnostics() const { return diagnostics_; }
    [[nodiscard]] bool ok() const { return diagnostics_.empty(); }
    void clear_diagnostics() { diagnostics_.clear(); }

    [[nodiscard]] const std::filesystem::path& root() const { return root_; }
    void set_root(std::filesystem::path root) { root_ = std::move(root); }

private:
    std::filesystem::path root_;
    std::vector<std::string> diagnostics_;
};

}  // namespace sony_msx::machine
