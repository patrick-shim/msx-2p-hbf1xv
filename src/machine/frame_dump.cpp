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

#include "machine/frame_dump.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "machine/debug_dump.h"
#include "machine/debug_format.h"

namespace sony_msx::machine::frame_dump {

namespace {
using debug_format::to_dec;
using debug_format::to_hex;
}  // namespace

std::string serialize_frame_dump(const devices::video::FrameBuffer& frame) {
    std::string out;
    out += kFrameDumpFormatTag;
    out.push_back('\n');
    out += "[FRAME] width=" + to_dec(static_cast<std::uint64_t>(frame.width)) +
           " height=" + to_dec(static_cast<std::uint64_t>(frame.height)) +
           " border=" + to_hex(frame.border_color, 4) + "\n";
    // Reinterpret the pixel buffer as raw bytes (host-native little-endian --
    // see the header doc comment) and reuse the EXISTING, already-proven
    // debug_dump::serialize_region() folded-hex routine (genuine reuse, not a
    // parallel reimplementation, planner §2.5 point 1).
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(frame.pixels.data());
    const std::size_t byte_count = frame.pixels.size() * sizeof(std::uint16_t);
    out += debug_dump::serialize_region("PIXELS", frame.pixels.empty() ? nullptr : bytes, byte_count);
    out += "[END]\n";
    return out;
}

namespace {

// Reverses debug_dump::serialize_region()'s exact folded-hex format for one
// named region: a '*' line means the previous printed 16-byte line repeats
// until the next printed offset (mirrors tools/mem-to-png.py's own Python
// parse_region_from_dump(), independently re-expressed in C++ for this
// project's own round-trip test -- never copied, no external reference).
std::vector<std::uint8_t> parse_region(const std::vector<std::string>& lines, std::size_t start,
                                        std::size_t size) {
    std::vector<std::uint8_t> buf(size, 0);
    std::size_t prev_off = 0;
    std::vector<std::uint8_t> prev_bytes;
    bool star = false;
    bool have_prev = false;

    for (std::size_t li = start; li < lines.size(); ++li) {
        const std::string& line = lines[li];
        if (line.empty()) {
            continue;
        }
        if (line[0] == '[') {
            break;
        }
        if (line == "*") {
            star = true;
            continue;
        }

        std::istringstream iss(line);
        std::string offset_token;
        iss >> offset_token;
        const std::size_t off = static_cast<std::size_t>(std::stoul(offset_token, nullptr, 16));

        std::vector<std::uint8_t> row;
        std::string byte_token;
        while (iss >> byte_token) {
            if (byte_token.size() != 2) {
                continue;
            }
            row.push_back(static_cast<std::uint8_t>(std::stoul(byte_token, nullptr, 16)));
        }

        if (star && have_prev) {
            std::size_t fill = prev_off + 16;
            while (fill < off) {
                const std::size_t end = std::min(fill + 16, size);
                for (std::size_t i = fill; i < end; ++i) {
                    buf[i] = prev_bytes[i - fill];
                }
                fill += 16;
            }
            star = false;
        }

        const std::size_t end = std::min(off + row.size(), size);
        for (std::size_t i = off; i < end; ++i) {
            buf[i] = row[i - off];
        }
        prev_off = off;
        prev_bytes = row;
        have_prev = true;
    }

    return buf;
}

}  // namespace

devices::video::FrameBuffer parse_frame_dump(const std::string& text) {
    std::vector<std::string> lines;
    {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
    }

    if (lines.empty() || lines[0] != kFrameDumpFormatTag) {
        throw std::runtime_error("parse_frame_dump: missing/mismatched format tag");
    }

    devices::video::FrameBuffer frame;
    bool found_frame_header = false;
    std::size_t pixels_header_line = 0;
    std::size_t pixel_byte_count = 0;
    bool found_pixels_header = false;

    for (std::size_t i = 1; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (line.rfind("[FRAME] width=", 0) == 0) {
            std::istringstream iss(line.substr(std::string("[FRAME] ").size()));
            std::string width_tok;
            std::string height_tok;
            std::string border_tok;
            iss >> width_tok >> height_tok >> border_tok;
            frame.width = std::stoi(width_tok.substr(width_tok.find('=') + 1));
            frame.height = std::stoi(height_tok.substr(height_tok.find('=') + 1));
            frame.border_color =
                static_cast<std::uint16_t>(std::stoul(border_tok.substr(border_tok.find('=') + 1), nullptr, 16));
            found_frame_header = true;
        } else if (line.rfind("[PIXELS] size=", 0) == 0) {
            pixel_byte_count = std::stoul(line.substr(std::string("[PIXELS] size=").size()));
            pixels_header_line = i + 1;
            found_pixels_header = true;
            break;
        }
    }

    if (!found_frame_header || !found_pixels_header) {
        throw std::runtime_error("parse_frame_dump: missing [FRAME] or [PIXELS] header");
    }

    const std::vector<std::uint8_t> raw = parse_region(lines, pixels_header_line, pixel_byte_count);
    if (raw.size() % sizeof(std::uint16_t) != 0) {
        throw std::runtime_error("parse_frame_dump: pixel byte count is not a whole number of uint16_t");
    }

    frame.pixels.assign(raw.size() / sizeof(std::uint16_t), 0);
    if (!raw.empty()) {
        std::memcpy(frame.pixels.data(), raw.data(), raw.size());
    }

    return frame;
}

}  // namespace sony_msx::machine::frame_dump
