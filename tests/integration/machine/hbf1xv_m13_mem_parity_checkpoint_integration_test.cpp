#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "machine/cpu_trace_sink.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvM13MemParityCheckpoint_Integration  (M13-S5)
//
// Regression-locks THIS emulator's half of the M13 CPU-visible RAM/ROM A/B probe
// (docs/m13-parity-trace-diff.md, driven vs openMSX Sony_HB-F1XV by
// tools/openmsx-mem-parity.ps1). The committed BIOS-independent probe runs from a
// RAM-mapped state at 0xC000 and:
//   - reads a known BIOS byte (via a slot switch of page 0 to slot 0-0) into a
//     register;
//   - writes + reads back a mapper-RAM cell;
//   - switches a mapper segment via OUT (#FC),A then reads back the S1985
//     100xxxxx value.
// All results land in CPU registers so the architectural-register comparator
// captures them. The deterministic M10-S1 trace is asserted byte-for-byte against
// a self-derived golden so the emulator side of the diff cannot silently drift,
// independent of the WSL/openMSX environment.
//
// Code runs from page 3 (slot 3-0 RAM, seg 3) which is never remapped by the
// probe, so switching page 0's slot / mapper segment does not move the running
// code.
//
// The real bios directory is injected as an absolute path by CMake.

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif

namespace {

bool expect_true(const bool ok, const char* name) {
    if (ok) {
        return true;
    }
    std::cerr << "Case failed: " << name << "\n";
    return false;
}

std::vector<std::uint8_t> read_bios() {
    const std::filesystem::path path = std::filesystem::path(SONY_MSX_BIOS_DIR) / "f1xvbios.rom";
    std::ifstream file(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
}

constexpr std::uint16_t kBase = 0xC000;

// BIOS-independent RAM/ROM probe (runs from page-3 RAM):
//   LD A,0xEE ; LD (0xC800),A          ; mapper-RAM write (page 3, seg 3)
//   LD A,(0xC800) ; LD B,A             ; B = RAM readback (0xEE)
//   LD A,0x25 ; OUT (0xFC),A           ; page-0 mapper segment = 0x25
//   IN A,(0xFC) ; LD C,A               ; C = S1985 readback 0x80|(0x25&0x1F)=0x85
//   LD A,0xFC ; OUT (0xA8),A           ; page 0 -> slot 0-0 = BIOS; page 3 STAYS slot 3
//   LD A,(0x0000) ; LD D,A             ; D = BIOS[0]
//   HALT
const std::array<std::uint8_t, 25> kProbe{
    0x3E, 0xEE,        // LD A,0xEE
    0x32, 0x00, 0xC8,  // LD (0xC800),A
    0x3A, 0x00, 0xC8,  // LD A,(0xC800)
    0x47,              // LD B,A
    0x3E, 0x25,        // LD A,0x25
    0xD3, 0xFC,        // OUT (0xFC),A
    0xDB, 0xFC,        // IN A,(0xFC)
    0x4F,              // LD C,A
    0x3E, 0xFC,        // LD A,0xFC        (page0=slot0, pages1-3=slot3)
    0xD3, 0xA8,        // OUT (0xA8),A
    0x3A, 0x00, 0x00,  // LD A,(0x0000)
    0x57,              // LD D,A
    0x76,              // HALT
};

std::string run_probe_trace(std::uint8_t& b, std::uint8_t& c, std::uint8_t& d) {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();
    machine.map_flat_ram();  // page 3 -> slot 3-0 RAM seg 3; code base 0xC000
    machine.load_memory(kBase, kProbe.data(), static_cast<std::uint32_t>(kProbe.size()));
    machine.cpu().state().regs().pc = kBase;
    machine.set_cpu_trace_enabled(true);
    for (int i = 0; i < 64 && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
    const auto& r = machine.cpu().state().regs();
    b = r.b();
    c = r.c();
    d = r.d();
    return machine.cpu_trace().serialize();
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> bios = read_bios();
    if (!expect_true(bios.size() == 0x8000, "Bios_Present_Is32K")) {
        return 1;
    }

    std::uint8_t b = 0, c = 0, d = 0;
    const std::string trace_a = run_probe_trace(b, c, d);

    if (!expect_true(b == 0xEE, "MapperRam_WriteReadBack_ReturnsWrittenByte")) {
        return 1;
    }
    if (!expect_true(c == 0x85, "MapperSegmentSwitch_Fc_Returns100xxxxx")) {
        return 1;
    }
    if (!expect_true(d == bios[0], "BiosByte_ViaSlotSwitch_MatchesImage")) {
        std::cerr << "  D=0x" << std::hex << static_cast<int>(d) << " bios[0]=0x"
                  << static_cast<int>(bios[0]) << std::dec << "\n";
        return 1;
    }

    // Determinism: a second run is byte-for-byte identical (trace + registers).
    std::uint8_t b2 = 0, c2 = 0, d2 = 0;
    const std::string trace_b = run_probe_trace(b2, c2, d2);
    if (!expect_true(trace_a == trace_b && b == b2 && c == c2 && d == d2,
                     "Probe_TwoRuns_ByteForByteIdentical")) {
        return 1;
    }

    return 0;
}
