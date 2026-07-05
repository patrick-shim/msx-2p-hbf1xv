#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

std::string run_dump() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
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

    // --- DRAM region: program bytes on the first line, folded zeros, framed. ---
    // Line 0 carries the program bytes; line 1 (first all-zero line) differs
    // from line 0 so it is emitted verbatim; identical zero lines after it fold
    // into a single '*'; the final line is always emitted verbatim.
    const std::string expected_dram =
        "[DRAM] size=65536\n"
        "00000000 3E 2A 06 03 80 76 00 00 00 00 00 00 00 00 00 00\n"
        "00000010 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "*\n"
        "0000FFF0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n";
    if (!expect_true(contains(dump, expected_dram), "Dump_DramRegion_FoldedHexExact")) {
        return 1;
    }

    // --- SRAM region: all-zero, folded, 8 KiB framing. ---
    const std::string expected_sram =
        "[SRAM] size=8192\n"
        "00000000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "*\n"
        "00001FF0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n";
    if (!expect_true(contains(dump, expected_sram), "Dump_SramRegion_FoldedHexExact")) {
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
