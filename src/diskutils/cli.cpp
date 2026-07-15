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

#include "diskutils/cli.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include "diskutils/hex_dump.h"
#include "diskutils/msx_disk_format.h"

namespace sony_msx::diskutils {

namespace {

// Parse a plain decimal unsigned into `out`. Returns false on any non-digit,
// empty, or overflow.
bool parse_u32_dec(const std::string& s, std::uint32_t& out) {
    if (s.empty()) {
        return false;
    }
    std::uint32_t value = 0;
    const char* first = s.data();
    const char* last = s.data() + s.size();
    const std::from_chars_result r = std::from_chars(first, last, value, 10);
    if (r.ec != std::errc{} || r.ptr != last) {
        return false;
    }
    out = value;
    return true;
}

// Parse a hex unsigned (no 0x prefix) into `out`. Returns false on failure.
bool parse_u64_hex(const std::string& s, std::uint64_t& out) {
    if (s.empty()) {
        return false;
    }
    std::uint64_t value = 0;
    const char* first = s.data();
    const char* last = s.data() + s.size();
    const std::from_chars_result r = std::from_chars(first, last, value, 16);
    if (r.ec != std::errc{} || r.ptr != last) {
        return false;
    }
    out = value;
    return true;
}

// Parse "<startHex>-<endHex>" (inclusive start, exclusive end). Requires
// start < end. Returns false on any malformation or inverted/empty range.
bool parse_range(const std::string& s, std::uint64_t& start, std::uint64_t& end) {
    const std::size_t dash = s.find('-');
    if (dash == std::string::npos || dash == 0 || dash + 1 >= s.size()) {
        return false;
    }
    if (!parse_u64_hex(s.substr(0, dash), start)) {
        return false;
    }
    if (!parse_u64_hex(s.substr(dash + 1), end)) {
        return false;
    }
    return start < end;
}

bool read_file(const std::filesystem::path& path, std::vector<std::uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return static_cast<bool>(in) || in.eof();
}

bool write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& data) {
    // Ensure the parent directory exists so a "diskutils/out.dsk"-style path in
    // a fresh checkout does not fail. A bare filename has no parent -> skip.
    std::error_code ec;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

// Clamp [start, end) to the file bytes; empty span if start is past EOF.
std::span<const std::uint8_t> clamp_slice(const std::vector<std::uint8_t>& bytes,
                                          std::uint64_t start, std::uint64_t end) {
    const std::uint64_t sz = bytes.size();
    if (start >= sz) {
        return {};
    }
    const std::uint64_t e = (end > sz) ? sz : end;
    return std::span<const std::uint8_t>(bytes.data() + static_cast<std::size_t>(start),
                                         static_cast<std::size_t>(e - start));
}

int run_create(const Args& a, std::ostream& out, std::ostream& err) {
    std::error_code ec;
    if (std::filesystem::exists(a.path, ec) && !a.force) {
        err << "error: refusing to overwrite existing file (use --force): " << a.path << "\n";
        return kExitSafety;
    }
    const std::vector<std::uint8_t> image = build_blank_image();
    if (!write_file(a.path, image)) {
        err << "error: cannot write file: " << a.path << "\n";
        return kExitIo;
    }
    out << "created blank 720 KB MSX-DOS FAT12 image: " << a.path << " (" << image.size()
        << " bytes)\n";
    return kExitSuccess;
}

int run_format(const Args& a, std::ostream& out, std::ostream& err) {
    std::error_code ec;
    if (!std::filesystem::exists(a.path, ec)) {
        err << "error: target does not exist: " << a.path << "\n";
        return kExitIo;
    }
    const std::uintmax_t size = std::filesystem::file_size(a.path, ec);
    if (ec) {
        err << "error: cannot determine size of: " << a.path << "\n";
        return kExitIo;
    }
    if (size != DiskFormat::kImageBytes && !a.force) {
        err << "error: refusing to format: target is " << size << " bytes, expected "
            << DiskFormat::kImageBytes << " (use --force to normalize): " << a.path << "\n";
        return kExitSafety;
    }
    const std::vector<std::uint8_t> image = build_blank_image();
    if (!write_file(a.path, image)) {
        err << "error: cannot write file: " << a.path << "\n";
        return kExitIo;
    }
    out << "formatted to blank 720 KB MSX-DOS FAT12 image: " << a.path << " (" << image.size()
        << " bytes)\n";
    return kExitSuccess;
}

int run_read(const Args& a, std::ostream& out, std::ostream& err) {
    std::vector<std::uint8_t> bytes;
    if (!read_file(a.path, bytes)) {
        err << "error: cannot open/read file: " << a.path << "\n";
        return kExitIo;
    }
    std::uint64_t base = 0;
    std::span<const std::uint8_t> view(bytes);
    if (a.has_sector) {
        base = static_cast<std::uint64_t>(a.sector) * DiskFormat::kSectorSize;
        view = clamp_slice(bytes, base, base + DiskFormat::kSectorSize);
    } else if (a.has_range) {
        base = a.range_start;
        view = clamp_slice(bytes, a.range_start, a.range_end);
    }
    write_hex_dump(out, view, base);
    return kExitSuccess;
}

}  // namespace

void print_usage(std::ostream& out) {
    out << "msx-disk -- standalone Sony HB-F1XV (MSX2+) 720 KB disk utility\n"
           "\n"
           "USAGE:\n"
           "  msx-disk --create <out.dsk> [--force]\n"
           "  msx-disk --format <disk.dsk> [--force]\n"
           "  msx-disk --read   <disk.dsk> [--sector <lba> | --range <startHex>-<endHex>]\n"
           "  msx-disk --help | -h\n"
           "\n"
           "MODES (exactly one):\n"
           "  --create   Write a new, fully-formatted blank 720 KB MSX-DOS FAT12 image\n"
           "             (737280 bytes). Refuses to overwrite an existing file without\n"
           "             --force.\n"
           "  --format   Re-format an existing image in place. Refuses a target that is\n"
           "             not exactly 737280 bytes without --force.\n"
           "  --read     Deterministic xxd-style hex dump to stdout. Default: whole image.\n"
           "             --sector <lba>  dumps one 512-byte sector (lba 0..1439).\n"
           "             --range <A-B>   dumps byte offsets [A, B) (hex, exclusive end).\n"
           "\n"
           "EXIT CODES:\n"
           "  0 success   1 usage error   2 I/O error   3 safety refusal\n"
           "\n"
           "The created/formatted image is a genuinely EMPTY, non-bootable data/files\n"
           "medium (empty root directory => zero files); it is NOT a bootable OS disk.\n";
}

Args parse_args(int argc, const char* const* argv) {
    Args a;
    bool path_set = false;

    auto set_mode = [&](Mode m) {
        if (a.mode != Mode::None) {
            a.ok = false;
            a.error = "multiple mode flags specified (choose exactly one of "
                      "--create/--format/--read/--help)";
            return;
        }
        a.mode = m;
    };

    for (int i = 1; i < argc && a.ok; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            set_mode(Mode::Help);
        } else if (arg == "--create") {
            set_mode(Mode::Create);
        } else if (arg == "--format") {
            set_mode(Mode::Format);
        } else if (arg == "--read") {
            set_mode(Mode::Read);
        } else if (arg == "--force") {
            a.force = true;
        } else if (arg == "--sector") {
            if (i + 1 >= argc) {
                a.ok = false;
                a.error = "--sector requires a value";
                break;
            }
            const std::string value = argv[++i];
            if (!parse_u32_dec(value, a.sector) || a.sector > DiskFormat::kMaxSectorIndex) {
                a.ok = false;
                a.error = "invalid --sector (expected 0.." +
                          std::to_string(DiskFormat::kMaxSectorIndex) + "): " + value;
                break;
            }
            a.has_sector = true;
        } else if (arg == "--range") {
            if (i + 1 >= argc) {
                a.ok = false;
                a.error = "--range requires a value";
                break;
            }
            const std::string value = argv[++i];
            if (!parse_range(value, a.range_start, a.range_end)) {
                a.ok = false;
                a.error = "invalid --range (expected <startHex>-<endHex>, start<end): " + value;
                break;
            }
            a.has_range = true;
        } else if (!arg.empty() && arg[0] == '-') {
            a.ok = false;
            a.error = "unknown option: " + arg;
            break;
        } else {
            if (path_set) {
                a.ok = false;
                a.error = "unexpected extra argument: " + arg;
                break;
            }
            a.path = arg;
            path_set = true;
        }
    }

    if (!a.ok) {
        return a;
    }

    // Cross-field validation.
    if (a.mode == Mode::None) {
        a.ok = false;
        a.error = "no mode specified";
        return a;
    }
    if (a.mode == Mode::Help) {
        return a;  // --help ignores any other argument
    }
    if (!path_set) {
        a.ok = false;
        a.error = "missing <disk.dsk> path argument";
        return a;
    }
    if (a.has_sector && a.has_range) {
        a.ok = false;
        a.error = "--sector and --range are mutually exclusive";
        return a;
    }
    if ((a.has_sector || a.has_range) && a.mode != Mode::Read) {
        a.ok = false;
        a.error = "--sector/--range are only valid with --read";
        return a;
    }
    return a;
}

int run(const Args& args, std::ostream& out, std::ostream& err) {
    if (!args.ok) {
        err << "error: " << args.error << "\n\n";
        print_usage(err);
        return kExitUsage;
    }
    switch (args.mode) {
        case Mode::Help:
            print_usage(out);
            return kExitSuccess;
        case Mode::Create:
            return run_create(args, out, err);
        case Mode::Format:
            return run_format(args, out, err);
        case Mode::Read:
            return run_read(args, out, err);
        case Mode::None:
        default:
            err << "error: no mode specified\n\n";
            print_usage(err);
            return kExitUsage;
    }
}

}  // namespace sony_msx::diskutils
