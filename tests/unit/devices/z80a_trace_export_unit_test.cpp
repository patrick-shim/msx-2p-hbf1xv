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
#include <iostream>
#include <vector>

#include "core/bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"
#include "devices/cpu/z80a_trace.h"

namespace {

class ArrayBus final : public sony_msx::core::Bus {
public:
    sony_msx::core::BusData read(const sony_msx::core::BusAddress address) override {
        return memory[address];
    }

    void write(const sony_msx::core::BusAddress address, const sony_msx::core::BusData value) override {
        memory[address] = value;
    }

    std::array<std::uint8_t, 65536> memory{};
};

// Minimal devices-layer observer: retains the emitted records for assertion.
class RecordingObserver final : public sony_msx::devices::cpu::Z80aTraceObserver {
public:
    void on_instruction_retired(const sony_msx::devices::cpu::Z80aTraceRecord& record) override {
        records.push_back(record);
    }

    std::vector<sony_msx::devices::cpu::Z80aTraceRecord> records;
};

bool expect_true(const bool condition, const char* case_name) {
    if (condition) {
        return true;
    }

    std::cerr << "Case failed: " << case_name << "\n";
    return false;
}

// Fixed program: LD A,0x2A / LD B,0x03 / ADD A,B / HALT.
void load_program(ArrayBus& bus) {
    bus.memory[0x0000] = 0x3E;  // LD A,0x2A
    bus.memory[0x0001] = 0x2A;
    bus.memory[0x0002] = 0x06;  // LD B,0x03
    bus.memory[0x0003] = 0x03;
    bus.memory[0x0004] = 0x80;  // ADD A,B
    bus.memory[0x0005] = 0x76;  // HALT
}

bool records_equal(const sony_msx::devices::cpu::Z80aTraceRecord& a,
                   const sony_msx::devices::cpu::Z80aTraceRecord& b) {
    return a.sequence == b.sequence && a.pc == b.pc && a.opcode_length == b.opcode_length &&
           a.opcode_bytes == b.opcode_bytes && a.af == b.af && a.bc == b.bc && a.de == b.de &&
           a.hl == b.hl && a.af_shadow == b.af_shadow && a.bc_shadow == b.bc_shadow &&
           a.de_shadow == b.de_shadow && a.hl_shadow == b.hl_shadow && a.ix == b.ix &&
           a.iy == b.iy && a.sp == b.sp && a.i == b.i && a.r == b.r && a.iff1 == b.iff1 &&
           a.iff2 == b.iff2 && a.im == b.im && a.instr_tstates == b.instr_tstates &&
           a.cumulative_tstates == b.cumulative_tstates;
}

}  // namespace

int main() {
    // Suite: Devices_Z80ATraceExport_Unit
    using sony_msx::devices::cpu::CpuBusClient;
    using sony_msx::devices::cpu::InterruptMode;
    using sony_msx::devices::cpu::Z80aCpu;

    // --- Field-exactness: observer attached over the known sequence. ---
    ArrayBus bus_on;
    load_program(bus_on);
    CpuBusClient client_on(bus_on);
    Z80aCpu cpu_on(client_on);
    cpu_on.reset();

    RecordingObserver observer;
    cpu_on.set_trace_observer(&observer);
    if (!expect_true(cpu_on.trace_observer() == &observer, "SetObserver_Attached_ObserverReadable")) {
        return 1;
    }

    for (int i = 0; i < 4; ++i) {
        cpu_on.step();
    }

    if (!expect_true(observer.records.size() == 4, "Observer_FourInstructions_FourRecords")) {
        return 1;
    }

    const auto& r0 = observer.records[0];
    if (!expect_true(r0.sequence == 0 && r0.pc == 0x0000 && r0.opcode_length == 1 &&
                         r0.opcode_bytes[0] == 0x3E && r0.a == 0x00 && r0.f == 0x00 &&
                         r0.b == 0x00 && r0.sp == 0xFFFF && r0.r == 0x00 && !r0.iff1 &&
                         !r0.iff2 && r0.im == InterruptMode::Im1 && r0.instr_tstates == 7 &&
                         r0.cumulative_tstates == 7,
                     "Record0_LdAImm_PreStateAndTimingExact")) {
        return 1;
    }

    const auto& r1 = observer.records[1];
    if (!expect_true(r1.sequence == 1 && r1.pc == 0x0002 && r1.opcode_bytes[0] == 0x06 &&
                         r1.a == 0x2A && r1.b == 0x00 && r1.r == 0x01 && r1.instr_tstates == 7 &&
                         r1.cumulative_tstates == 14,
                     "Record1_LdBImm_PreStateReflectsPriorInstruction")) {
        return 1;
    }

    const auto& r2 = observer.records[2];
    if (!expect_true(r2.sequence == 2 && r2.pc == 0x0004 && r2.opcode_bytes[0] == 0x80 &&
                         r2.a == 0x2A && r2.b == 0x03 && r2.r == 0x02 && r2.instr_tstates == 4 &&
                         r2.cumulative_tstates == 18,
                     "Record2_AddAB_PreStateExact")) {
        return 1;
    }

    // ADD A,B: 0x2A + 0x03 = 0x2D. Flags: S0 Z0 Y1(bit5) H0 X1(bit3) PV0 N0 C0 => F=0x28.
    const auto& r3 = observer.records[3];
    const bool flags_ok = r3.f == 0x28 && !r3.flag_s() && !r3.flag_z() && r3.flag_y() &&
                          !r3.flag_h() && r3.flag_x() && !r3.flag_pv() && !r3.flag_n() &&
                          !r3.flag_c();
    if (!expect_true(r3.sequence == 3 && r3.pc == 0x0005 && r3.opcode_bytes[0] == 0x76 &&
                         r3.a == 0x2D && flags_ok && r3.r == 0x03 && r3.instr_tstates == 4 &&
                         r3.cumulative_tstates == 22,
                     "Record3_Halt_PreStateHasAddResultAndDecomposedFlags")) {
        return 1;
    }

    // --- Non-perturbation: observer OFF must yield identical end-state/cycles. ---
    ArrayBus bus_off;
    load_program(bus_off);
    CpuBusClient client_off(bus_off);
    Z80aCpu cpu_off(client_off);
    cpu_off.reset();
    // No observer attached (nullptr / off by default).
    for (int i = 0; i < 4; ++i) {
        cpu_off.step();
    }

    const auto& son = cpu_on.state();
    const auto& soff = cpu_off.state();
    const bool same_end_state = son.regs().af == soff.regs().af && son.regs().bc == soff.regs().bc &&
                                son.regs().de == soff.regs().de && son.regs().hl == soff.regs().hl &&
                                son.regs().ix == soff.regs().ix && son.regs().iy == soff.regs().iy &&
                                son.regs().sp == soff.regs().sp && son.regs().pc == soff.regs().pc &&
                                son.regs().i == soff.regs().i && son.regs().r == soff.regs().r &&
                                son.iff1() == soff.iff1() && son.iff2() == soff.iff2() &&
                                son.halted() == soff.halted() &&
                                son.total_tstates() == soff.total_tstates();
    if (!expect_true(same_end_state && cpu_off.trace_observer() == nullptr,
                     "ObserverOff_SameProgram_IdenticalEndStateAndCycles")) {
        return 1;
    }

    // --- Reproducibility: a second ON run yields byte-identical record fields. ---
    ArrayBus bus_on2;
    load_program(bus_on2);
    CpuBusClient client_on2(bus_on2);
    Z80aCpu cpu_on2(client_on2);
    cpu_on2.reset();
    RecordingObserver observer2;
    cpu_on2.set_trace_observer(&observer2);
    for (int i = 0; i < 4; ++i) {
        cpu_on2.step();
    }

    bool reproducible = observer2.records.size() == observer.records.size();
    for (std::size_t i = 0; reproducible && i < observer.records.size(); ++i) {
        reproducible = records_equal(observer.records[i], observer2.records[i]);
    }
    if (!expect_true(reproducible, "TwoOnRuns_SameProgram_RecordsBitIdentical")) {
        return 1;
    }

    // --- Detach: setting nullptr stops record emission. ---
    cpu_on.set_trace_observer(nullptr);
    cpu_on.reset();
    bus_on.memory[0x0000] = 0x00;  // NOP
    const std::size_t before_detach = observer.records.size();
    cpu_on.step();
    if (!expect_true(cpu_on.trace_observer() == nullptr &&
                         observer.records.size() == before_detach,
                     "DetachObserver_Step_NoNewRecords")) {
        return 1;
    }

    return 0;
}
