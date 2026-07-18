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

#include <cstdint>
#include <ostream>
#include <string>

namespace sony_msx::utils {

// Exit codes (planner package §2.3). Small deterministic set.
inline constexpr int kExitSuccess = 0;  // success (incl. --help)
inline constexpr int kExitUsage = 1;    // usage / argument error
inline constexpr int kExitIo = 2;       // I/O error (open/read/write; target missing)
inline constexpr int kExitSafety = 3;   // safety refusal (--force required)

enum class Mode { None, Create, Format, Read, Help };

// Parsed command line. `ok == false` means a usage/argument error whose reason
// is in `error`; run() then prints usage and returns kExitUsage.
struct Args {
    Mode mode = Mode::None;
    std::string path;
    bool force = false;

    bool has_sector = false;
    std::uint32_t sector = 0;

    bool has_range = false;
    std::uint64_t range_start = 0;
    std::uint64_t range_end = 0;  // exclusive

    bool ok = true;
    std::string error;
};

// Pure argument parsing (NO file I/O). Recognizes exactly one mode flag, an
// optional path, --force, and (--read only) --sector <lba 0..1439> |
// --range <startHex>-<endHex> (mutually exclusive).
[[nodiscard]] Args parse_args(int argc, const char* const* argv);

// Execute the parsed command. All file I/O lives here (taking streams makes it
// unit-testable without spawning a process). Returns the process exit code.
[[nodiscard]] int run(const Args& args, std::ostream& out, std::ostream& err);

// The usage/help text (also the --help output).
void print_usage(std::ostream& out);

}  // namespace sony_msx::utils
