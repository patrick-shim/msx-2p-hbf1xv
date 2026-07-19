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

#pragma once

#include <cstdint>
#include <vector>

#include "machine/hbf1xv_machine.h"

namespace sony_msx::machine {

// Generic, reusable CP/M-style ".com" loader + BDOS-trap harness, used to
// run the ZEXDOC/ZEXALL CPU-exerciser parity sweeps.
//
// Implements ONLY the standard, third-party Digital Research CP/M "TPA"
// loading convention -- predating and external to any one CP/M-hosted
// program:
//   - a `.com` image is a flat memory image loaded at the fixed base 0x0100
//     (the assembler-level `org 100h` convention every CP/M `.com` program
//     is built against);
//   - the "top of memory" word at address 0x0006-0x0007, which every well-
//     behaved CP/M program reads as its initial stack-pointer seed
//     (`LD HL,(6) / LD SP,HL`);
//   - BDOS dispatch is reached by `CALL 5` (address 0x0005); the two BDOS
//     functions this harness understands are C=9 (print a `$`-terminated
//     string pointed to by DE) and C=2 (console character-out, byte in E);
//   - `JP 0x0000` is the CP/M warm-boot convention, i.e. "the program is
//     finished".
//
// It has zero knowledge of any specific CP/M-hosted program's content: no
// message strings, no CRC tables, no test/group names or counts, nothing
// transcribed from any one exerciser's assembly source -- a small,
// independently-designed primitive that could load and run any small CP/M
// `.com` binary needing only BDOS functions 2 and 9. This is a deliberate
// license-isolation boundary: the exercisers it runs are third-party
// (GPL v2) fixtures, so this class must never be extended with anything
// specific to zexall.z80/zexdoc.z80 or any other single CP/M program.
class CpmBdosHarness {
public:
    enum class LoadResult {
        Ok,
        // Defensive, generic safety guard (not a magic number specific to
        // any one CP/M program): refuses to load rather than risk silently
        // corrupting memory above the loaded image when
        // 0x0100 + image.size() would reach or exceed top_of_memory.
        ImageTooLargeForMemory,
    };

    struct RunResult {
        // True only when the CP/M warm-boot trap (PC == 0x0000) was reached --
        // a genuine "program finished" signal, never a budget-exhaustion
        // timeout treated as success.
        bool finished = false;
        // Count of real step_cpu_instruction() calls executed. BDOS-trap and
        // warm-boot-check iterations are not counted (they execute zero real
        // CPU steps -- see run()'s doc comment).
        std::uint64_t instructions_executed = 0;
        // Bytes captured from BDOS functions C=9 (each byte up to, but not
        // including, the terminating '$') and C=2 (the single byte in E),
        // appended in the exact order the two functions were invoked.
        std::vector<std::uint8_t> captured_output;
        // Any BDOS function code (register C) other than 9 or 2 observed at
        // the CALL-5 trap, in the order encountered. Non-fatal: the harness
        // still synthesizes the RET and keeps running. Empty for a program
        // that only ever uses C=9/C=2.
        std::vector<std::uint8_t> unexpected_bdos_calls;
    };

    // Loads `image` at the fixed CP/M TPA base 0x0100 into `machine`'s flat
    // RAM (calls machine.map_flat_ram() first, unconditionally, so the full
    // 64 KB address space is linear, writable RAM -- harmless/idempotent
    // even when the load is about to be refused, since it writes no image
    // bytes itself), writes `top_of_memory` little-endian at address 0x0006
    // (the CP/M "top of TPA" word -- see the class doc comment), and sets
    // PC = 0x0100. Refuses to load (returns ImageTooLargeForMemory without
    // writing the image, the top-of-memory word, or moving PC) when
    // 0x0100 + image.size() would reach or exceed top_of_memory.
    static LoadResult load_com(Hbf1xvMachine& machine, std::vector<std::uint8_t> image,
                                std::uint16_t top_of_memory = 0xFF00);

    // Runs `machine`'s CPU, trapping the two generic CP/M mechanics:
    //   - PC == 0x0000 (CP/M warm boot): sets finished = true and stops.
    //     step_cpu_instruction() is not called for this check.
    //   - PC == 0x0005 (BDOS dispatch): reads register C to select the
    //     function (9 = print $-terminated string at DE; 2 = console char-out
    //     from E; anything else is recorded as an unexpected BDOS call), then
    //     synthesizes the RET the real BDOS entry point would eventually
    //     execute by popping the return address the caller's CALL 5 pushed.
    //     step_cpu_instruction() is not called for this trap either -- the
    //     CPU never decodes whatever raw byte actually sits at 0x0005/0x0000,
    //     since no real BDOS/CCP is hosted in memory.
    //   - Any other PC: calls step_cpu_instruction() normally and increments
    //     the instruction counter.
    // Runs until `finished` becomes true or `max_instructions` real CPU steps
    // have executed, whichever comes first -- an honest "ran out of budget"
    // result (finished stays false) when the budget is exhausted first.
    static RunResult run(Hbf1xvMachine& machine, std::uint64_t max_instructions);

private:
    static constexpr std::uint16_t kLoadBase = 0x0100;
    static constexpr std::uint16_t kTopOfMemoryWordAddress = 0x0006;
    static constexpr std::uint16_t kBdosEntryAddress = 0x0005;
    static constexpr std::uint16_t kWarmBootAddress = 0x0000;
    static constexpr std::uint8_t kDollarTerminator = 0x24;  // '$'
    static constexpr std::uint8_t kBdosFunctionPrintString = 9;
    static constexpr std::uint8_t kBdosFunctionConsoleOut = 2;
};

}  // namespace sony_msx::machine
