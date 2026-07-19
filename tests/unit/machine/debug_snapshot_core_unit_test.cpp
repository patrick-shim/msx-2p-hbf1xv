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
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

#include "machine/debug_format.h"
#include "machine/debug_snapshot.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_DebugSnapshotCore_Unit (M36 Phase 3 slice S1, DEC-0051).
//
// The CPU-full serializer (incl WZ/Q + interrupt/ei/pending), the DRAM +
// mapper-segment + clock + slots + sysctrl scaffold, the versioned header, the
// deterministic id, the golden-serialize_cpu-unchanged guard (A2), and the
// default-off no-op regression guard.

namespace {

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// Fixed program: LD A,0x2A / LD B,0x03 / ADD A,B / HALT (the M13 debug-dump
// golden program, so the golden CPU block is the known reference).
const std::array<std::uint8_t, 6> kProgram{0x3E, 0x2A, 0x06, 0x03, 0x80, 0x76};

const std::string* find_file(const sony_msx::machine::debug_snapshot::Snapshot& snap,
                             const std::string& name) {
    for (const auto& f : snap.files) {
        if (f.name == name) {
            return &f.content;
        }
    }
    return nullptr;
}

void run_program(sony_msx::machine::Hbf1xvMachine& machine) {
    machine.cold_boot();
    machine.map_flat_ram();
    machine.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));
    for (int i = 0; i < 4; ++i) {
        machine.step_cpu_instruction();
    }
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    namespace ds = sony_msx::machine::debug_snapshot;

    Hbf1xvMachine machine;
    run_program(machine);

    // The deterministic id derives from the machine's own counters (frame=0,
    // cycle=scheduler total -- NOT the CPU T-state count, which differs by the
    // S1985 M1 wait states).
    const std::string id = machine.snapshot_id();
    const ds::Snapshot snap = machine.serialize_snapshot(id);

    // --- Bundle shape: manifest.txt first, then 9 component files (10 total). ---
    expect(snap.files.size() == 10, "Bundle_HasManifestPlusNineComponents");
    expect(!snap.files.empty() && snap.files.front().name == "manifest.txt",
           "Manifest_IsFirstFile");

    // --- Every file opens with the versioned format tag (restore-ready v1). ---
    bool all_tagged = true;
    for (const auto& f : snap.files) {
        if (f.content.rfind("HBF1XV-SNAPSHOT v1\n", 0) != 0) {
            all_tagged = false;
        }
        if (f.content.find("[END]\n") == std::string::npos) {
            all_tagged = false;
        }
    }
    expect(all_tagged, "EveryFile_VersionTaggedAndEndFramed");

    // --- CPU section: exact typed fields for the known end-state, INCLUDING the
    //     WZ/Q + interrupt/ei-delay/pending delta over the golden serializer. ---
    const std::string* cpu = find_file(snap, "cpu.txt");
    expect(cpu != nullptr, "CpuFile_Present");
    if (cpu != nullptr) {
        expect(contains(*cpu, "[CPU]\n"), "Cpu_HasHeader");
        expect(contains(*cpu, "PC=0006 SP=FFFF\n"), "Cpu_PcSp_Exact");
        expect(contains(*cpu, "AF=2D28 BC=0300 DE=0000 HL=0000\n"), "Cpu_MainRegs_Exact");
        expect(contains(*cpu, "A=2D F=28[..Y.X...] B=03 C=00 D=00 E=00 H=00 L=00\n"),
               "Cpu_ByteRegs_Exact");
        // The comprehensive delta: WZ/Q + interrupt/ei/pending lines.
        expect(contains(*cpu, "WZ="), "Cpu_HasWZ");
        expect(contains(*cpu, " Q="), "Cpu_HasQ");
        expect(contains(*cpu, "EI_DELAY="), "Cpu_HasEiDelay");
        expect(contains(*cpu, "MASKABLE_PENDING="), "Cpu_HasMaskablePending");
        expect(contains(*cpu, "NMI_PENDING="), "Cpu_HasNmiPending");
        expect(contains(*cpu, "INT_VECTOR_SUPPLIED="), "Cpu_HasIntVectorSupplied");
        expect(contains(*cpu, "TSTATES=22\n"), "Cpu_Tstates_Exact");
    }

    // --- A2: the golden serialize_state_dump()'s [CPU] block is UNCHANGED
    //     byte-for-byte (the M13 golden) and DOES NOT gain WZ/Q -- proving
    //     debug_dump::serialize_cpu was not edited in place. ---
    const std::string golden = machine.serialize_state_dump();
    const std::string golden_cpu =
        "[CPU]\n"
        "PC=0006 SP=FFFF\n"
        "AF=2D28 BC=0300 DE=0000 HL=0000\n"
        "AF'=0000 BC'=0000 DE'=0000 HL'=0000\n"
        "IX=0000 IY=0000\n"
        "A=2D F=28[..Y.X...] B=03 C=00 D=00 E=00 H=00 L=00\n"
        "I=00 R=04 IFF1=0 IFF2=0 IM=1 HALT=1\n"
        "TSTATES=22\n";
    expect(contains(golden, golden_cpu), "GoldenStateDump_CpuBlock_Unchanged");
    expect(!contains(golden, "WZ="), "GoldenStateDump_HasNoWZ_SeparateArtifact");

    // --- memory.txt: DRAM region + [MAPPER] segments (flat-RAM {0,1,2,3}). ---
    const std::string* mem = find_file(snap, "memory.txt");
    expect(mem != nullptr, "MemoryFile_Present");
    if (mem != nullptr) {
        expect(contains(*mem, "[DRAM] size=65536\n"), "Memory_DramRegion");
        expect(contains(*mem, "[MAPPER]\n"), "Memory_MapperHeader");
        expect(contains(*mem, "SEG0=00 SEG1=01 SEG2=02 SEG3=03\n"), "Memory_MapperSegments_FlatRam");
        // Readback = 0x80 | (seg & 0x1F): 80/81/82/83.
        expect(contains(*mem, "READBACK0=80 READBACK1=81 READBACK2=82 READBACK3=83\n"),
               "Memory_MapperReadback_Exact");
    }

    // --- vram.txt: 128 KB region present + folded. ---
    const std::string* vram = find_file(snap, "vram.txt");
    expect(vram != nullptr && contains(*vram, "[VRAM] size=131072\n"), "VramFile_Region");

    // --- chipset.txt: [SLOTS] / [SYSCTRL] / [CLOCK] present + typed. ---
    const std::string* chip = find_file(snap, "chipset.txt");
    expect(chip != nullptr, "ChipsetFile_Present");
    if (chip != nullptr) {
        expect(contains(*chip, "[SLOTS]\nA8="), "Chipset_SlotsHeader");
        expect(contains(*chip, "SUB0=") && contains(*chip, "EXP0="), "Chipset_SubExpLines");
        expect(contains(*chip, "[SYSCTRL]\nF4="), "Chipset_SysctrlHeader");
        expect(contains(*chip, "[CLOCK]\nELAPSED_CYCLES="), "Chipset_ClockHeader");
        expect(contains(*chip, "FRAME_COUNT=0 "), "Chipset_FrameCountZero");
    }

    // --- manifest.txt: id + frame/cycle stamps + a FILES index with sizes+crc. ---
    const std::string* man = find_file(snap, "manifest.txt");
    expect(man != nullptr, "ManifestFile_Present");
    if (man != nullptr) {
        expect(contains(*man, "ID=" + id + "\n"), "Manifest_Id");
        expect(contains(*man, "FRAME=0\n") &&
                   contains(*man, "CYCLE=" +
                                     sony_msx::machine::debug_format::to_dec(machine.elapsed_cycles()) +
                                     "\n"),
               "Manifest_Stamps");
        expect(contains(*man, "FILE=cpu.txt size=") && contains(*man, " crc="), "Manifest_FilesIndex");
    }

    // --- Deterministic id helper: "f<frame>_c<cycle>", zero-padded (frame>=6
    //     digits, cycle>=16 digits); stable across calls and reconstructable. ---
    {
        const std::string expected =
            "f000000_c" + [&] {
                std::string s = sony_msx::machine::debug_format::to_dec(machine.elapsed_cycles());
                while (s.size() < 16) {
                    s.insert(s.begin(), '0');
                }
                return s;
            }();
        expect(machine.snapshot_id() == expected && machine.snapshot_id() == machine.snapshot_id(),
               "SnapshotId_Deterministic");
    }

    // --- Two serializations are byte-identical AND non-perturbing (CPU + clock
    //     unchanged) -- the writer consumes no RNG / wall-clock for content. ---
    {
        const std::uint16_t pc_before = machine.cpu().state().regs().pc;
        const std::uint64_t cyc_before = machine.elapsed_cycles();
        const ds::Snapshot a = machine.serialize_snapshot("X");
        const ds::Snapshot b = machine.serialize_snapshot("X");
        bool identical = a.files.size() == b.files.size();
        for (std::size_t i = 0; identical && i < a.files.size(); ++i) {
            identical = a.files[i].name == b.files[i].name && a.files[i].content == b.files[i].content;
        }
        expect(identical, "SerializeTwice_ByteIdentical");
        expect(machine.cpu().state().regs().pc == pc_before &&
                   machine.elapsed_cycles() == cyc_before,
               "SerializeSnapshot_NonPerturbing");
    }

    // --- Default-off no-op: a machine that NEVER calls write_snapshot creates NO
    //     snapshot dir (the regression guard: flag-absent = byte-for-byte
    //     unchanged). Then write_snapshot creates <root>/snapshot/<id>/ with the
    //     full bundle. ---
    {
        const std::filesystem::path root =
            std::filesystem::temp_directory_path() / "hbf1xv_snap_core_unit";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);

        Hbf1xvMachine m2;
        m2.set_debug_root(root);
        run_program(m2);
        const bool no_dir_before = !std::filesystem::exists(root / "snapshot");
        expect(no_dir_before, "NoWrite_NoSnapshotDir");

        const std::string id = m2.snapshot_id();
        const bool wrote = m2.write_snapshot(id);
        const std::filesystem::path dir = root / "snapshot" / id;
        expect(wrote && std::filesystem::exists(dir / "manifest.txt") &&
                   std::filesystem::exists(dir / "cpu.txt") &&
                   std::filesystem::exists(dir / "vram.txt"),
               "WriteSnapshot_CreatesBundle");
        std::filesystem::remove_all(root, ec);
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_DebugSnapshotCore_Unit cases passed\n";
    return 0;
}
