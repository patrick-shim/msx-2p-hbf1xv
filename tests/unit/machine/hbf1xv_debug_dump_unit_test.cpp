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

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "machine/debug_dump.h"
#include "machine/hbf1xv_machine.h"

namespace {

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Fixed program: LD A,0x2A / LD B,0x03 / ADD A,B / HALT.
const std::array<std::uint8_t, 6> kProgram{
    0x3E, 0x2A,  // LD A,0x2A
    0x06, 0x03,  // LD B,0x03
    0x80,        // ADD A,B
    0x76,        // HALT
};

// Main RAM powers on with the alternating 00/FF initialContent the machine
// XML declares (openMSX 21.0: Sony_HB-F1XV.xml:129), not all-zero. The
// 512-byte pattern (00,FF x128 then FF,00 x128) is repeated over 64 KB.
std::uint8_t a5_byte(const std::size_t index) {
    const std::size_t p = index & 0x1FFu;
    if (p < 256) {
        return (p & 1u) ? 0xFF : 0x00;
    }
    return (p & 1u) ? 0x00 : 0xFF;
}

// The exact expected [DRAM] dump section, built independently of the machine
// from the a5_byte() pattern with the program bytes overwritten at 0x0000, then
// run through the SAME production serializer (debug_dump::serialize_region).
// This asserts the ENTIRE 64 KB region byte-for-byte (stronger than an earlier
// partial-substring check, whose all-zero fold no longer holds with the 00/FF
// power-on pattern).
std::string expected_dram_section() {
    std::vector<std::uint8_t> dram(64u * 1024u);
    for (std::size_t i = 0; i < dram.size(); ++i) {
        dram[i] = a5_byte(i);
    }
    for (std::size_t i = 0; i < kProgram.size(); ++i) {
        dram[i] = kProgram[i];  // program (no memory writes) overwrites 0x0000..0x0005
    }
    return sony_msx::machine::debug_dump::serialize_region("DRAM", dram.data(), dram.size());
}

std::string run_dump() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // run the program from RAM (authentic #A8=0 boots BIOS)
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 4; ++i) {
        machine.step_cpu_instruction();
    }
    return machine.serialize_state_dump();
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

}  // namespace

int main() {
    // Suite: Machine_Hbf1xvDebugDump_Unit

    const std::string dump = run_dump();

    // --- Dump opens with the versioned format tag. ---
    if (!expect_true(dump.rfind("HBF1XV-STATE-DUMP v1\n", 0) == 0,
                     "Dump_FirstLine_IsVersionTag")) {
        return 1;
    }

    // --- CPU section is exact for the known program end-state. ---
    // After 4 steps: A=0x2D (0x2A+0x03), F=0x28 (Y,X set), B=0x03, PC=0x0006
    // (past HALT), halted, R=0x04 (4 M1 fetches), 22 cumulative T-states.
    const std::string expected_cpu =
        "[CPU]\n"
        "PC=0006 SP=FFFF\n"
        "AF=2D28 BC=0300 DE=0000 HL=0000\n"
        "AF'=0000 BC'=0000 DE'=0000 HL'=0000\n"
        "IX=0000 IY=0000\n"
        "A=2D F=28[..Y.X...] B=03 C=00 D=00 E=00 H=00 L=00\n"
        "I=00 R=04 IFF1=0 IFF2=0 IM=1 HALT=1\n"
        "TSTATES=22\n";
    if (!expect_true(contains(dump, expected_cpu), "Dump_CpuSection_ExactGolden")) {
        std::cerr << "  --- actual dump head ---\n" << dump.substr(0, 400) << "\n";
        return 1;
    }

    // --- DRAM region: program bytes on the first line, then the alternating
    //     00/FF power-on pattern, hex-folded and framed. The whole 64 KB section
    //     is checked byte-for-byte via the production serializer over an
    //     independently-built expected buffer. ---
    const std::string expected_dram = expected_dram_section();
    if (!expect_true(contains(dump, expected_dram), "Dump_DramRegion_A5PatternFoldedHexExact")) {
        std::cerr << "  --- expected DRAM head ---\n" << expected_dram.substr(0, 200) << "\n";
        return 1;
    }
    // Sanity anchors: the first data line carries the program bytes then the
    // pattern tail, and the last line is the FF,00 half of the pattern.
    if (!expect_true(contains(dump, "[DRAM] size=65536\n"
                                    "00000000 3E 2A 06 03 80 76 00 FF 00 FF 00 FF 00 FF 00 FF\n"),
                     "Dump_DramRegion_FirstLine_ProgramThenPattern")) {
        return 1;
    }

    // --- SRAM region: the bare HB-F1XV has NO internal
    //     SRAM, so the section is EMPTY (size=0, no hex lines) and sits
    //     immediately before [VRAM]. It reflects an inserted FM-PAC peripheral
    //     cartridge's 8 KB SRAM only when present (covered by the FM-PAC
    //     integration test; DEC-0050). ---
    if (!expect_true(contains(dump, "[SRAM] size=0\n[VRAM]"), "Dump_SramRegion_EmptyOnBareMachine")) {
        std::cerr << "  --- expected empty [SRAM] size=0 immediately before [VRAM] ---\n";
        return 1;
    }

    // --- VRAM region: all-zero, folded, 128 KiB framing, then [END]. ---
    const std::string expected_vram =
        "[VRAM] size=131072\n"
        "00000000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "*\n"
        "0001FFF0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "[END]\n";
    if (!expect_true(contains(dump, expected_vram), "Dump_VramRegion_FoldedHexExactThenEnd")) {
        return 1;
    }

    // --- Reproducibility: two identical runs produce byte-identical dumps. ---
    const std::string dump_a = run_dump();
    const std::string dump_b = run_dump();
    if (!expect_true(dump_a == dump_b && dump_a == dump,
                     "TwoRuns_SameProgram_ByteIdenticalDump")) {
        return 1;
    }

    // --- Non-perturbation: dumping does not change CPU state or the clock. ---
    {
        sony_msx::machine::Hbf1xvMachine machine;
        machine.cold_boot();
        machine.map_flat_ram();  // run program from RAM
        machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
        for (int i = 0; i < 4; ++i) {
            machine.step_cpu_instruction();
        }
        const std::uint16_t pc_before = machine.cpu().state().regs().pc;
        const std::uint8_t a_before = machine.cpu().state().regs().a();
        const std::uint8_t r_before = machine.cpu().state().regs().r;
        const bool halt_before = machine.cpu().state().halted();
        const std::uint64_t cycles_before = machine.elapsed_cycles();

        const std::string first = machine.serialize_state_dump();
        const std::string second = machine.serialize_state_dump();

        const bool unchanged = machine.cpu().state().regs().pc == pc_before &&
                               machine.cpu().state().regs().a() == a_before &&
                               machine.cpu().state().regs().r == r_before &&
                               machine.cpu().state().halted() == halt_before &&
                               machine.elapsed_cycles() == cycles_before && first == second;
        if (!expect_true(unchanged, "SerializeDump_Twice_NonPerturbingAndStable")) {
            return 1;
        }
    }

    // --- File round-trip: write_state_dump writes exactly serialize_state_dump,
    //     creating debug/traces under a hermetic root, and is re-runnable. ---
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "hbf1xv_s3_dump_unit";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);

        sony_msx::machine::Hbf1xvMachine machine;
        machine.set_debug_root(root);
        machine.cold_boot();
        machine.map_flat_ram();  // run program from RAM
        machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
        for (int i = 0; i < 4; ++i) {
            machine.step_cpu_instruction();
        }
        const std::string expected = machine.serialize_state_dump();
        const bool wrote = machine.write_state_dump("state.dump");
        const std::filesystem::path dump_path = root / "traces" / "state.dump";
        const bool exists = std::filesystem::exists(dump_path);
        const std::string on_disk = read_file(dump_path);

        // Re-run (overwrite): tolerate re-execution, byte-identical result.
        const bool wrote_again = machine.write_state_dump("state.dump");
        const std::string on_disk_again = read_file(dump_path);

        std::filesystem::remove_all(root, ec);

        if (!expect_true(wrote && wrote_again && exists && on_disk == expected &&
                             on_disk_again == expected,
                         "WriteStateDump_FileMatchesSerializationAndRerunnable")) {
            return 1;
        }
    }

    return 0;
}
