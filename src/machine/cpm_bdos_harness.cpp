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

#include "machine/cpm_bdos_harness.h"

namespace sony_msx::machine {

CpmBdosHarness::LoadResult CpmBdosHarness::load_com(Hbf1xvMachine& machine, std::vector<std::uint8_t> image,
                                                     std::uint16_t top_of_memory) {
    // Flat, linear 64 KB RAM view -- the CP/M loading convention this harness
    // models assumes a single flat address space, not the HB-F1XV's own
    // slotted memory map. Applied unconditionally: it is a harmless, idempotent
    // remap (no image bytes are written by it), so doing it before the size
    // guard below costs nothing even when the load is about to be refused.
    machine.map_flat_ram();

    const std::uint32_t end_address = static_cast<std::uint32_t>(kLoadBase) + image.size();
    if (end_address >= top_of_memory) {
        // Defensive, GENERIC safety guard (NOT a magic number specific to any
        // one CP/M program): refuse to load rather than risk silently
        // corrupting memory above the image. Neither the image nor the
        // top-of-memory word is written, and PC is left untouched.
        return LoadResult::ImageTooLargeForMemory;
    }

    machine.load_memory(kLoadBase, image.data(), static_cast<std::uint32_t>(image.size()));

    // CP/M "top of TPA" word at 0x0006-0x0007, little-endian. This harness
    // traps CALL 5 rather than hosting real BDOS machine code, so the exact
    // value only needs to be a safe stack-pointer seed clear of the loaded
    // image -- not a real BDOS base address.
    const std::uint8_t low = static_cast<std::uint8_t>(top_of_memory & 0xFF);
    const std::uint8_t high = static_cast<std::uint8_t>((top_of_memory >> 8) & 0xFF);
    machine.load_memory(kTopOfMemoryWordAddress, &low, 1);
    machine.load_memory(static_cast<std::uint16_t>(kTopOfMemoryWordAddress + 1), &high, 1);

    machine.cpu().state().regs().pc = kLoadBase;
    return LoadResult::Ok;
}

CpmBdosHarness::RunResult CpmBdosHarness::run(Hbf1xvMachine& machine, std::uint64_t max_instructions) {
    RunResult result;
    auto& regs = machine.cpu().state().regs();

    while (true) {
        if (regs.pc == kWarmBootAddress) {
            result.finished = true;
            break;
        }

        if (regs.pc == kBdosEntryAddress) {
            const std::uint8_t function = regs.c();
            if (function == kBdosFunctionPrintString) {
                std::uint16_t address =
                    static_cast<std::uint16_t>((static_cast<std::uint16_t>(regs.d()) << 8) | regs.e());
                while (true) {
                    const std::uint8_t byte = machine.read_memory(address);
                    if (byte == kDollarTerminator) {
                        break;
                    }
                    result.captured_output.push_back(byte);
                    ++address;
                }
            } else if (function == kBdosFunctionConsoleOut) {
                result.captured_output.push_back(regs.e());
            } else {
                result.unexpected_bdos_calls.push_back(function);
            }

            // Synthesize the RET the real BDOS entry point would eventually
            // execute: pop the return address the caller's CALL 5 pushed and
            // resume there. No step_cpu_instruction() call for this trap --
            // the CPU never decodes whatever raw byte sits at 0x0005.
            const std::uint16_t sp = regs.sp;
            const std::uint16_t ret_low = machine.read_memory(sp);
            const std::uint16_t ret_high = machine.read_memory(static_cast<std::uint16_t>(sp + 1));
            regs.sp = static_cast<std::uint16_t>(sp + 2);
            regs.pc = static_cast<std::uint16_t>(ret_low | (ret_high << 8));
            continue;
        }

        if (result.instructions_executed >= max_instructions) {
            break;
        }

        machine.step_cpu_instruction();
        ++result.instructions_executed;
    }

    return result;
}

}  // namespace sony_msx::machine
