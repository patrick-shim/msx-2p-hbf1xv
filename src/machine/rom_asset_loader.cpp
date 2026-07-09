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

#include "machine/rom_asset_loader.h"

#include <fstream>
#include <ios>
#include <iterator>
#include <system_error>
#include <utility>

namespace sony_msx::machine {

namespace {
constexpr std::uint8_t kOpenBus = 0xFF;

std::string to_dec(const std::uint64_t value) {
    if (value == 0) {
        return "0";
    }
    std::string digits;
    std::uint64_t v = value;
    while (v != 0) {
        digits.push_back(static_cast<char>('0' + (v % 10)));
        v /= 10;
    }
    return std::string(digits.rbegin(), digits.rend());
}
}  // namespace

RomAssetLoader::RomAssetLoader(std::filesystem::path root) : root_(std::move(root)) {
}

std::vector<std::uint8_t> RomAssetLoader::load(const Spec& spec) {
    const std::filesystem::path path = root_ / spec.filename;
    const std::string path_str = path.generic_string();

    auto filled = [&](const std::string& reason) {
        diagnostics_.push_back("ROM asset '" + path_str + "' " + reason + "; filled 0xFF (" +
                               spec.label + ", expected " + to_dec(spec.expected_size) + " bytes)");
        return std::vector<std::uint8_t>(spec.expected_size, kOpenBus);
    };

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return filled("ABSENT");
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return filled("UNREADABLE");
    }

    std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    if (!file && !file.eof()) {
        return filled("UNREADABLE");
    }

    if (image.size() != spec.expected_size) {
        // Deterministic: keep as much real content as possible but normalize to
        // the expected window size; record the divergence (no silent accept).
        diagnostics_.push_back("ROM asset '" + path_str + "' SIZE_MISMATCH actual " +
                               to_dec(image.size()) + "; normalized to " + spec.label +
                               " expected " + to_dec(spec.expected_size) + " bytes (0xFF-pad/truncate)");
        image.resize(spec.expected_size, kOpenBus);
    }

    return image;
}

}  // namespace sony_msx::machine
