#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

#include "devices/video/frame_buffer.h"
#include "machine/debug_dump.h"
#include "machine/frame_dump.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::video::FrameBuffer;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// A small, known synthetic FrameBuffer (4x3, RGB555 values chosen so every
// byte of every pixel is distinguishable -- a strong, hand-computable
// round-trip oracle).
FrameBuffer make_known_frame() {
    FrameBuffer frame;
    frame.width = 4;
    frame.height = 3;
    frame.border_color = 0x1234;
    frame.pixels.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height));
    for (std::size_t i = 0; i < frame.pixels.size(); ++i) {
        frame.pixels[i] = static_cast<std::uint16_t>(0x0100 * (i + 1) + i);
    }
    return frame;
}

}  // namespace

int main() {
    using sony_msx::machine::frame_dump::kFrameDumpFormatTag;
    using sony_msx::machine::frame_dump::parse_frame_dump;
    using sony_msx::machine::frame_dump::serialize_frame_dump;

    // --- Case 1: deterministic header/content assertions against a
    // hand-computed oracle (mirrors debug_dump's own oracle-construction
    // discipline, reusing the SAME production serialize_region() routine). ---
    {
        const FrameBuffer frame = make_known_frame();
        const std::string dump = serialize_frame_dump(frame);

        std::istringstream iss(dump);
        std::string first_line;
        std::getline(iss, first_line);
        expect(first_line == kFrameDumpFormatTag, "Header_FirstLine_MatchesFormatTag");

        std::string frame_line;
        std::getline(iss, frame_line);
        expect(frame_line == "[FRAME] width=4 height=3 border=1234", "Header_FrameLine_MatchesHandComputedOracle");

        // The [PIXELS] section is produced by the EXISTING, already-proven
        // debug_dump::serialize_region() routine (genuine reuse) -- assert the
        // FULL dump equals what calling it directly on the raw pixel bytes
        // produces, byte-for-byte (a strong oracle, not merely a substring
        // check).
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(frame.pixels.data());
        const std::size_t byte_count = frame.pixels.size() * sizeof(std::uint16_t);
        const std::string expected_pixels_section =
            sony_msx::machine::debug_dump::serialize_region("PIXELS", bytes, byte_count);
        const std::string expected_full = std::string(kFrameDumpFormatTag) + "\n" +
                                           "[FRAME] width=4 height=3 border=1234\n" + expected_pixels_section +
                                           "[END]\n";
        expect(dump == expected_full, "FullDump_MatchesHandAssembledOracle_ByteForByte");
    }

    // --- Case 2: real round-trip -- write_frame_dump -> re-parse -> byte-
    // identical pixel recovery (planner §2.5 point 1 Acceptance). ---
    {
        const FrameBuffer original = make_known_frame();
        const std::string dump = serialize_frame_dump(original);
        const FrameBuffer recovered = parse_frame_dump(dump);

        expect(recovered.width == original.width, "RoundTrip_Width_Recovered");
        expect(recovered.height == original.height, "RoundTrip_Height_Recovered");
        expect(recovered.border_color == original.border_color, "RoundTrip_BorderColor_Recovered");
        expect(recovered.pixels == original.pixels, "RoundTrip_Pixels_ByteIdenticalRecovery");
    }

    // --- Case 3: an all-identical-16-byte-line region exercises the '*'
    // fold path (mirrors debug_dump's own folding discipline) and still
    // round-trips exactly. ---
    {
        FrameBuffer frame;
        frame.width = 8;
        frame.height = 8;  // 64 pixels = 128 bytes = 8 lines of 16 bytes, all identical
        frame.border_color = 0x0000;
        frame.pixels.assign(64, 0x7C1F);  // a fixed, non-zero RGB555 value

        const std::string dump = serialize_frame_dump(frame);
        expect(dump.find("*\n") != std::string::npos, "FoldedRegion_ContainsStarMarker");
        const FrameBuffer recovered = parse_frame_dump(dump);
        expect(recovered.pixels == frame.pixels, "FoldedRegion_RoundTrips_ByteIdentical");
    }

    // --- Case 4: Hbf1xvMachine::write_frame_dump writes a real file under
    // <debug_root_>/frames/<filename>, and its content matches
    // serialize_frame_dump() exactly (mirrors write_state_dump's own
    // directory-creation/content-parity contract). ---
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.cold_boot();

        const std::filesystem::path temp_root =
            std::filesystem::temp_directory_path() / "sony-msx-hbf1xv-m26-frame-dump-unit-test";
        std::error_code ec;
        std::filesystem::remove_all(temp_root, ec);
        machine.set_debug_root(temp_root);

        const bool ok = machine.write_frame_dump("boot.frame");
        expect(ok, "WriteFrameDump_ReturnsTrue");

        const std::filesystem::path expected_path = temp_root / "frames" / "boot.frame";
        expect(std::filesystem::exists(expected_path), "WriteFrameDump_CreatesFileUnderFramesSubdir");

        std::string on_disk;
        {
            // Scoped so the file handle is closed BEFORE remove_all() below --
            // Windows refuses to delete a directory tree while a handle into
            // it is still open.
            std::ifstream file(expected_path, std::ios::binary);
            on_disk.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }
        expect(on_disk == machine.serialize_frame_dump(), "WriteFrameDump_ContentMatchesSerializeFrameDump");

        std::filesystem::remove_all(temp_root, ec);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_FrameDump_Unit cases passed\n";
    return 0;
}
