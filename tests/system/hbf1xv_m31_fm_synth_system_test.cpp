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

// Suite: Machine_Hbf1xvM31FmSynth_System (M31-S6, backlog E1,
// docs/m31-planner-package.md §3-S6)
//
// End-to-end register-driven FM synthesis over the REAL M11 bus: an in-RAM
// Z80 program (fetched and executed by the real CPU) programs the YM2413
// through genuine OUT (#7C)/OUT (#7D) instructions; the M31-S5
// MachineAudioMixer then produces the mixed PCM stream (silent PSG + no SCC
// + the machine's own OPLL as the third source).
//
// Stages: (A) key-on -> machine-level pitch measured against the S2 formula
// oracle f = F-Num * 2^BLOCK * MUL * 49716 / 2^19, expressed in the output
// domain (native period x 72/81); (B) key-off via a second CPU program ->
// release-bounded decay to EXACT digital silence; (C) a §6 rhythm burst
// (Yamaha-recommended setup) -> non-silent output. Deterministic oracle:
// the ENTIRE session (all three stages' PCM) is byte-identical across two
// fully independent runs.
//
// No BIOS asset is needed (map_flat_ram + the debug #A8/poke seams; the
// synthetic-fixture discipline, M29 system-test precedent).

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "devices/audio/ym2413_synth.h"
#include "frontend/machine_audio_mixer.h"
#include "machine/hbf1xv_machine.h"

namespace {

using sony_msx::devices::audio::Ym2413Synth;
using sony_msx::frontend::MachineAudioMixer;
using sony_msx::machine::Hbf1xvMachine;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

// Assembles "LD A,reg / OUT (7C),A / LD A,val / OUT (7D),A" pair sequences.
void emit_opll_write(std::vector<std::uint8_t>& code, const std::uint8_t reg,
                     const std::uint8_t value) {
    code.push_back(0x3E);
    code.push_back(reg);
    code.push_back(0xD3);
    code.push_back(0x7C);
    code.push_back(0x3E);
    code.push_back(value);
    code.push_back(0xD3);
    code.push_back(0x7D);
}

void poke_program(Hbf1xvMachine& machine, const std::uint16_t base,
                  const std::vector<std::uint8_t>& code) {
    for (std::size_t i = 0; i < code.size(); ++i) {
        machine.debug_bus_write(static_cast<std::uint16_t>(base + i), code[i]);
    }
}

void run_to_halt(Hbf1xvMachine& machine, const std::uint16_t pc) {
    machine.cpu().state().set_halted(false);
    machine.cpu().state().regs().pc = pc;
    int steps = 0;
    while (!machine.cpu().state().halted() && steps < 100000) {
        machine.step_cpu_instruction();
        ++steps;
    }
}

// Rising-crossing span on the LEFT channel, tracking the last NON-ZERO sign
// (near-crossing samples quantize to exactly 0 in the log-domain floor).
long measure_span_left(const std::vector<std::int16_t>& pcm, const int cycles) {
    int seen = 0;
    long first = -1;
    int last_sign = 0;
    for (std::size_t i = 0; i < pcm.size(); i += 2) {
        if (pcm[i] == 0) {
            continue;
        }
        const int sign = pcm[i] > 0 ? 1 : -1;
        if (last_sign < 0 && sign > 0) {
            if (seen == 0) {
                first = static_cast<long>(i / 2);
            }
            if (seen == cycles) {
                return static_cast<long>(i / 2) - first;
            }
            ++seen;
        }
        last_sign = sign;
    }
    return -1;
}

struct SessionResult {
    std::vector<std::int16_t> stage_a;
    std::vector<std::int16_t> stage_b;
    std::vector<std::int16_t> stage_c;
    bool key_on_seen = false;
    bool halted_each_stage = true;
};

SessionResult run_session() {
    SessionResult result;

    Hbf1xvMachine machine;
    machine.cold_boot();
    machine.map_flat_ram();

    // Stage A program: carrier-only user-patch tone on channel 0 --
    // fnum 256, block 4, MUL 1 (dP = 4096 -> native period exactly 128),
    // carrier AR=15/sustained/SL=0/RR=8, modulator silent (AR=0, TL max).
    std::vector<std::uint8_t> program_a;
    emit_opll_write(program_a, 0x00, 0x00);
    emit_opll_write(program_a, 0x01, 0x21);
    emit_opll_write(program_a, 0x02, 0x3F);
    emit_opll_write(program_a, 0x03, 0x00);
    emit_opll_write(program_a, 0x04, 0x00);
    emit_opll_write(program_a, 0x05, 0xF0);
    emit_opll_write(program_a, 0x06, 0x00);
    emit_opll_write(program_a, 0x07, 0x08);  // SL=0, RR=8
    emit_opll_write(program_a, 0x10, 0x00);
    emit_opll_write(program_a, 0x30, 0x00);
    emit_opll_write(program_a, 0x20, 0x19);  // KEY ON, block 4, fnum8=1
    program_a.push_back(0x76);               // HALT

    // Stage B program: key-off (same block/fnum bits, bit4 clear).
    std::vector<std::uint8_t> program_b;
    emit_opll_write(program_b, 0x20, 0x09);
    program_b.push_back(0x76);

    // Stage C program: the §6 Yamaha-recommended rhythm setup + all five
    // drums keyed.
    std::vector<std::uint8_t> program_c;
    emit_opll_write(program_c, 0x0E, 0x20);
    emit_opll_write(program_c, 0x16, 0x20);
    emit_opll_write(program_c, 0x17, 0x50);
    emit_opll_write(program_c, 0x18, 0xC0);
    emit_opll_write(program_c, 0x26, 0x05);
    emit_opll_write(program_c, 0x27, 0x05);
    emit_opll_write(program_c, 0x28, 0x01);
    emit_opll_write(program_c, 0x36, 0x00);
    emit_opll_write(program_c, 0x37, 0x00);
    emit_opll_write(program_c, 0x38, 0x00);
    emit_opll_write(program_c, 0x0E, 0x3F);  // rhythm on + BD/SD/TOM/T-CY/HH
    program_c.push_back(0x76);

    poke_program(machine, 0x8000, program_a);
    poke_program(machine, 0x8100, program_b);
    poke_program(machine, 0x8200, program_c);
    machine.cpu().state().regs().sp = 0xF000;

    const MachineAudioMixer mixer(81);

    // --- Stage A: key-on via real CPU OUTs, then pump. ---
    run_to_halt(machine, 0x8000);
    result.halted_each_stage &= machine.cpu().state().halted();
    result.key_on_seen = machine.ym2413().key_on(0);
    result.stage_a = mixer.mix_interleaved_stereo(
        machine.psg(), MachineAudioMixer::SccSources{nullptr, nullptr}, &machine.ym2413(), 4096);

    // --- Stage B: key-off, decay to exact silence. ---
    run_to_halt(machine, 0x8100);
    result.halted_each_stage &= machine.cpu().state().halted();
    result.stage_b = mixer.mix_interleaved_stereo(
        machine.psg(), MachineAudioMixer::SccSources{nullptr, nullptr}, &machine.ym2413(), 16384);

    // --- Stage C: rhythm burst. ---
    run_to_halt(machine, 0x8200);
    result.halted_each_stage &= machine.cpu().state().halted();
    result.stage_c = mixer.mix_interleaved_stereo(
        machine.psg(), MachineAudioMixer::SccSources{nullptr, nullptr}, &machine.ym2413(), 8192);

    return result;
}

}  // namespace

int main() {
    const SessionResult first = run_session();

    expect(first.halted_each_stage, "AllThreeCpuPrograms_ReachedHalt");
    expect(first.key_on_seen, "StageA_RealCpuOuts_KeyedChannel0");

    // --- Stage A: audible, and pitched per the S2 formula oracle. Native
    //     period = 2^19 / 4096 = 128 ticks; output-domain period =
    //     128 * 72 / 81 = 113.777... 12 measured periods = 1365.3 +- 3. ---
    {
        bool non_silent = false;
        for (const std::int16_t v : first.stage_a) {
            if (v != 0) {
                non_silent = true;
            }
        }
        expect(non_silent, "StageA_KeyOn_ProducesFmAudio");
        const long span = measure_span_left(first.stage_a, 12);
        const double expected = 12.0 * 128.0 * 72.0 / 81.0;
        expect(span > 0 && std::abs(static_cast<double>(span) - expected) <= 3.0,
               "StageA_MachineLevelPitch_MatchesFormulaOracle");
    }

    // --- Stage B: release-bounded decay to EXACT digital silence. RR=8 ->
    //     effective rate 34 with this note's Rks (QA F2 correction: the
    //     original comment said "rate 32 / 8128 ticks"; the actual effective
    //     rate is 34 -> §5 closed form (1<<(14-8))*85 = 5440 native ticks
    //     (~4836 output samples) -- the assertion below is a valid BOUND
    //     either way); the final quarter of 16384 samples must be exactly
    //     zero. ---
    {
        bool tail_silent = true;
        for (std::size_t i = first.stage_b.size() - 8192; i < first.stage_b.size(); ++i) {
            if (first.stage_b[i] != 0) {
                tail_silent = false;
            }
        }
        expect(tail_silent, "StageB_KeyOff_DecaysToExactSilence_WithinReleaseBound");
        bool head_had_audio = false;
        for (std::size_t i = 0; i < 2048; ++i) {
            if (first.stage_b[i] != 0) {
                head_had_audio = true;
            }
        }
        expect(head_had_audio, "StageB_ReleaseHead_StillAudibleBeforeDecay");
    }

    // --- Stage C: the rhythm burst is audible. ---
    {
        bool non_silent = false;
        for (const std::int16_t v : first.stage_c) {
            if (v != 0) {
                non_silent = true;
            }
        }
        expect(non_silent, "StageC_RhythmBurst_ProducesAudio");
    }

    // --- Two-run byte identity across the WHOLE session. ---
    {
        const SessionResult second = run_session();
        expect(second.stage_a == first.stage_a && second.stage_b == first.stage_b &&
                   second.stage_c == first.stage_c,
               "TwoIndependentSessions_ByteIdenticalPcm_AllStages");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM31FmSynth_System cases passed\n";
    return 0;
}
