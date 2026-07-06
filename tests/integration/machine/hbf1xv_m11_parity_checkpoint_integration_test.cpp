#include <array>
#include <cstdint>
#include <iostream>
#include <string>

#include "machine/cpu_trace_sink.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_Hbf1xvM11ParityCheckpoint_Integration
//
// M11-S6: regression-locks THIS emulator's half of the S1985 I/O A/B parity
// harness. The committed probe (tests/parity/m11_bus_probe.bin) is run to its
// HALT checkpoint and the deterministic M10-S1 trace is asserted byte-for-byte
// against a committed golden, so the emulator side of the parity diff cannot
// silently drift, independent of the WSL/openMSX environment. The openMSX side
// (genuine Sony HB-F1XV machine with a real S1985) and the captured EMPTY diff
// are in docs/m11-parity-trace-diff.md, produced by tools/openmsx-io-parity.ps1.

namespace {

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }
    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Byte-identical copy of tests/parity/m11_bus_probe.bin (26 bytes).
// SHA-256 f8be4803533d77147e5ddf398415046f093168dd7bb8796342a51aec8b799455.
const std::array<std::uint8_t, 26> kProbe{
    0x3E, 0x25, 0xD3, 0xFC, 0xDB, 0xFC, 0x47, 0x3E, 0xFE, 0xD3, 0x40, 0xDB, 0x40,
    0x4F, 0x3E, 0x05, 0xD3, 0x41, 0x3E, 0x3C, 0xD3, 0x42, 0xDB, 0x42, 0x57, 0x76,
};

constexpr std::uint16_t kBase = 0xC000;
constexpr std::uint16_t kCheckpointPc = 0xC019;

// Committed golden: exact CpuTraceSink serialization for the probe. This is the
// captured emulator trace A that produced an EMPTY architectural diff against the
// genuine Sony HB-F1XV S1985 in openMSX 19.1 (docs/m11-parity-trace-diff.md).
const char* kGoldenTrace =
    "SEQ=0000 PC=C000 OP=3E A=00 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=0000 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=00 IFF1=0 IFF2=0 IM=1 T=7 TC=7\n"
    "SEQ=0001 PC=C002 OP=D3 A=25 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=2500 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=01 IFF1=0 IFF2=0 IM=1 T=11 TC=18\n"
    "SEQ=0002 PC=C004 OP=DB A=25 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=2500 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=02 IFF1=0 IFF2=0 IM=1 T=11 TC=29\n"
    "SEQ=0003 PC=C006 OP=47 A=85 F=00[........] B=00 C=00 D=00 E=00 H=00 L=00 AF=8500 BC=0000 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=03 IFF1=0 IFF2=0 IM=1 T=4 TC=33\n"
    "SEQ=0004 PC=C007 OP=3E A=85 F=00[........] B=85 C=00 D=00 E=00 H=00 L=00 AF=8500 BC=8500 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=04 IFF1=0 IFF2=0 IM=1 T=7 TC=40\n"
    "SEQ=0005 PC=C009 OP=D3 A=FE F=00[........] B=85 C=00 D=00 E=00 H=00 L=00 AF=FE00 BC=8500 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=05 IFF1=0 IFF2=0 IM=1 T=11 TC=51\n"
    "SEQ=0006 PC=C00B OP=DB A=FE F=00[........] B=85 C=00 D=00 E=00 H=00 L=00 AF=FE00 BC=8500 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=06 IFF1=0 IFF2=0 IM=1 T=11 TC=62\n"
    "SEQ=0007 PC=C00D OP=4F A=01 F=00[........] B=85 C=00 D=00 E=00 H=00 L=00 AF=0100 BC=8500 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=07 IFF1=0 IFF2=0 IM=1 T=4 TC=66\n"
    "SEQ=0008 PC=C00E OP=3E A=01 F=00[........] B=85 C=01 D=00 E=00 H=00 L=00 AF=0100 BC=8501 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=08 IFF1=0 IFF2=0 IM=1 T=7 TC=73\n"
    "SEQ=0009 PC=C010 OP=D3 A=05 F=00[........] B=85 C=01 D=00 E=00 H=00 L=00 AF=0500 BC=8501 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=09 IFF1=0 IFF2=0 IM=1 T=11 TC=84\n"
    "SEQ=000A PC=C012 OP=3E A=05 F=00[........] B=85 C=01 D=00 E=00 H=00 L=00 AF=0500 BC=8501 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=0A IFF1=0 IFF2=0 IM=1 T=7 TC=91\n"
    "SEQ=000B PC=C014 OP=D3 A=3C F=00[........] B=85 C=01 D=00 E=00 H=00 L=00 AF=3C00 BC=8501 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=0B IFF1=0 IFF2=0 IM=1 T=11 TC=102\n"
    "SEQ=000C PC=C016 OP=DB A=3C F=00[........] B=85 C=01 D=00 E=00 H=00 L=00 AF=3C00 BC=8501 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=0C IFF1=0 IFF2=0 IM=1 T=11 TC=113\n"
    "SEQ=000D PC=C018 OP=57 A=3C F=00[........] B=85 C=01 D=00 E=00 H=00 L=00 AF=3C00 BC=8501 DE=0000 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=0D IFF1=0 IFF2=0 IM=1 T=4 TC=117\n"
    "SEQ=000E PC=C019 OP=76 A=3C F=00[........] B=85 C=01 D=3C E=00 H=00 L=00 AF=3C00 BC=8501 DE=3C00 HL=0000 AF'=0000 BC'=0000 DE'=0000 HL'=0000 IX=0000 IY=0000 SP=FFFF I=00 R=0E IFF1=0 IFF2=0 IM=1 T=4 TC=121\n";

std::string run_probe_trace() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM at 0xC000 (page 3); page flat 64K RAM (M13-S4)
    machine.load_memory(kBase, kProbe.data(), static_cast<std::uint32_t>(kProbe.size()));
    machine.cpu().state().regs().pc = kBase;
    machine.set_cpu_trace_enabled(true);
    for (int i = 0; i < 64 && !machine.cpu().state().halted(); ++i) {
        machine.step_cpu_instruction();
    }
    return machine.cpu_trace().serialize();
}

}  // namespace

int main() {
    sony_msx::machine::Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();  // program runs from RAM at 0xC000 (page 3); page flat 64K RAM (M13-S4)
    machine.load_memory(kBase, kProbe.data(), static_cast<std::uint32_t>(kProbe.size()));
    machine.cpu().state().regs().pc = kBase;
    machine.set_cpu_trace_enabled(true);

    int steps = 0;
    for (; steps < 64 && !machine.cpu().state().halted(); ++steps) {
        machine.step_cpu_instruction();
    }

    if (!expect_true(machine.cpu().state().halted(), "Probe_RunToCheckpoint_Halts")) {
        return 1;
    }
    if (!expect_true(machine.cpu_trace().size() == 15, "Probe_InstructionCount_Is15")) {
        std::cerr << "  size=" << machine.cpu_trace().size() << "\n";
        return 1;
    }

    // The S1985 I/O results that the A/B diff confirmed against real HB-F1XV.
    const auto& r = machine.cpu().state().regs();
    if (!expect_true(r.b() == 0x85, "MapperReadback_Fc_100xxxxxPattern")) {
        return 1;
    }
    if (!expect_true(r.c() == 0x01, "SwitchedIo_Id0xFE_ComplementOfId")) {
        return 1;
    }
    if (!expect_true(r.d() == 0x3C, "BackupRam_RoundTrip_ReadsWrittenByte")) {
        return 1;
    }

    const auto& last = machine.cpu_trace().at(machine.cpu_trace().size() - 1);
    if (!expect_true(last.pc == kCheckpointPc && last.opcode_bytes[0] == 0x76,
                     "Probe_FinalInstruction_IsHaltAtCheckpoint")) {
        return 1;
    }

    const std::string serialized = machine.cpu_trace().serialize();
    if (!expect_true(serialized == std::string(kGoldenTrace),
                     "Probe_Serialization_MatchesCommittedGolden")) {
        std::cerr << "  --- actual ---\n" << serialized;
        return 1;
    }

    // Determinism: a second independent run is byte-for-byte identical.
    if (!expect_true(run_probe_trace() == serialized, "Probe_TwoRuns_ByteForByteIdentical")) {
        return 1;
    }

    return 0;
}
