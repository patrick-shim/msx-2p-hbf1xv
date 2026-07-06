// Suite: Devices_VdpVram_Unit
//
// Deterministic unit coverage for the V9958-owned 128 KB VRAM store (M14-S1).
// Asserts the strict 128 KB size, zero-init at construction/clear, boundary +
// interior read/write over the full 17-bit address space, bounds-safe out-of-
// range access, and a load -> dump -> reload round-trip.

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/video/vdp_vram.h"

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

bool all_zero(const std::vector<std::uint8_t>& v) {
    for (const std::uint8_t b : v) {
        if (b != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    using sony_msx::devices::video::VdpVram;

    VdpVram vram;

    // Strict spec size (fact-sheet §2: 128 KB fixed).
    expect(vram.size() == 128u * 1024u, "Size_Strict_Is128KiB");
    expect(VdpVram::kVramBytes == 128u * 1024u, "Constant_kVramBytes_Is128KiB");
    expect(all_zero(vram.dump()), "Construct_ZeroInitialized");

    // Boundary + interior read/write over the full 17-bit address space.
    vram.write(0, 0xA1);
    vram.write(0x10000, 0x5C);            // above 64 KB (needs 17-bit addressing)
    vram.write(vram.size() - 1, 0xFE);    // last byte (0x1FFFF)
    expect(vram.read(0) == 0xA1, "WriteFirst_ReadsBack");
    expect(vram.read(0x10000) == 0x5C, "WriteAbove64K_ReadsBack");
    expect(vram.read(vram.size() - 1) == 0xFE, "WriteLast_ReadsBack");

    // Out-of-range access is deterministic and inert.
    vram.write(vram.size(), 0x99);
    expect(vram.read(vram.size()) == 0x00, "OutOfRange_ReadsZeroWriteIgnored");
    expect(vram.read(0x20000) == 0x00, "OutOfRange_Above128K_ReadsZero");

    // clear() zeroes everything again.
    vram.clear();
    expect(all_zero(vram.dump()), "Clear_ReZeroes");

    // Load -> dump -> reload round-trip across the full 128 KB.
    std::vector<std::uint8_t> pattern(vram.size());
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        pattern[i] = static_cast<std::uint8_t>((i * 7u + 3u) & 0xFFu);
    }
    vram.load(0, pattern.data(), pattern.size());
    const std::vector<std::uint8_t> dumped = vram.dump();
    expect(dumped == pattern, "LoadThenDump_MatchesPattern");

    VdpVram reloaded;
    reloaded.load(0, dumped.data(), dumped.size());
    expect(reloaded.dump() == pattern, "DumpReload_ByteIdentical");

    // Bulk load past end is clamped, never overflows.
    VdpVram v2;
    const std::uint8_t five[5] = {1, 2, 3, 4, 5};
    v2.load(v2.size() - 2, five, 5);
    expect(v2.read(v2.size() - 2) == 1 && v2.read(v2.size() - 1) == 2,
           "LoadPastEnd_ClampsDeterministically");
    expect(v2.size() == 128u * 1024u, "LoadPastEnd_SizeUnchanged");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    return 0;
}
