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

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "machine/debug_format.h"
#include "machine/debug_snapshot.h"
#include "machine/hbf1xv_machine.h"

// Suite: Machine_DebugSnapshotAudio_Unit (M36 Phase 3 slice S3). Writes PSG +
// YM2413 registers, advances the PSG generator a known number of cycles, and
// asserts the dumped register file + generator counters match live state. Also
// proves an EMPTY (no FM-PAC) run omits [FMPAC.OPLL] and a run with an inserted
// FM-PAC includes it.

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}
bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}
const std::string* find_file(const sony_msx::machine::debug_snapshot::Snapshot& snap,
                             const std::string& name) {
    for (const auto& f : snap.files) {
        if (f.name == name) {
            return &f.content;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::debug_format::to_dec;
    using sony_msx::machine::debug_format::to_hex;
    namespace ds = sony_msx::machine::debug_snapshot;

    Hbf1xvMachine machine;
    machine.cold_boot();

    // PSG: program a channel-A tone + volume, then advance the generator.
    machine.psg().write_address(0);
    machine.psg().write_data(0x10);
    machine.psg().write_address(1);
    machine.psg().write_data(0x00);
    machine.psg().write_address(8);
    machine.psg().write_data(0x0F);
    machine.psg().advance_cycles(64);

    // YM2413: latch address 0x30, write data 0x21.
    machine.ym2413().write_address(0x30);
    machine.ym2413().write_data(0x21);

    const ds::Snapshot snap = machine.serialize_snapshot("audio");
    const std::string* audio = find_file(snap, "audio.txt");
    expect(audio != nullptr, "AudioFile_Present");
    if (audio == nullptr) {
        return 1;
    }

    // --- Section completeness. ---
    for (const char* sec : {"[PSG.REGS]", "[PSG.GEN]", "[OPLL.REGS]", "[OPLL.TIMING]",
                            "[OPLL.SYNTH]"}) {
        expect(contains(*audio, sec), (std::string("AudioSection_") + sec).c_str());
    }

    // --- PSG register + generator typed exactness (against live state). ---
    expect(contains(*audio, "R00=" + to_hex(machine.psg().register_value(0), 2)), "Psg_R00_Exact");
    expect(contains(*audio, "R08=0F"), "Psg_R08_Exact");
    const auto gen = machine.psg().generator_snapshot();
    expect(contains(*audio, "NOISE_LFSR=" + to_hex(gen.noise_lfsr, 6)), "Psg_NoiseLfsr_Exact");
    expect(contains(*audio, "CYCLE_RESIDUAL=" + to_dec(gen.cycle_residual)), "Psg_CycleResidual_Exact");

    // --- YM2413 register file + latch typed exactness. ---
    expect(contains(*audio, "R30=" + to_hex(machine.ym2413().register_value(0x30), 2)),
           "Opll_R30_Exact");
    expect(contains(*audio, "LATCH=" + to_hex(machine.ym2413().address_latch(), 2)), "Opll_Latch_Exact");

    // --- No FM-PAC inserted -> [FMPAC.OPLL] is absent. ---
    expect(!contains(*audio, "[FMPAC.OPLL]"), "NoFmPac_OpllSectionAbsent");

    // --- Insert an FM-PAC cartridge -> [FMPAC.OPLL] appears (the inserted
    //     cart's own OPLL register file). ---
    {
        std::vector<std::uint8_t> image(0x4000, 0x00);  // one 16 KB bank (valid FM-PAC image)
        const auto result =
            machine.load_cartridge(1, sony_msx::devices::cartridge::CartridgeMapperType::FmPac,
                                   std::move(image));
        expect(result == sony_msx::devices::cartridge::CartridgeLoadResult::Ok, "FmPac_Loads");
        expect(machine.fmpac(1) != nullptr, "FmPac_AccessorNonNull");

        const ds::Snapshot snap2 = machine.serialize_snapshot("audio2");
        const std::string* audio2 = find_file(snap2, "audio.txt");
        expect(audio2 != nullptr && contains(*audio2, "[FMPAC.OPLL]"), "FmPac_OpllSectionPresent");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_DebugSnapshotAudio_Unit cases passed\n";
    return 0;
}
