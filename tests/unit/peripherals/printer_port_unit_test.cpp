#include <cstdint>
#include <iostream>
#include <vector>

#include "peripherals/printer_port.h"

// Suite: Peripherals_PrinterPort_Unit  (M18-S2, part of backlog C7)
//
// Centronics-style strobe/data write protocol + falling-edge byte capture
// over the real 8-port claim #90-#97 (A-M18-5/A-M18-6), grounded in
// references/openmsx-21.0/src/MSXPrinterPort.cc:46-62 and Printer.cc:59-66.

namespace {

using sony_msx::peripherals::PrinterPort;

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    // --- Data-then-strobe-pulse captures exactly one byte. ---
    {
        PrinterPort port;
        port.reset();
        port.io_write(0x91, 0x41);  // data = 'A'
        port.io_write(0x90, 0x00);  // strobe low (falling edge from idle-high)
        port.io_write(0x90, 0x01);  // strobe back high
        expect(port.captured_bytes().size() == 1 && port.captured_bytes()[0] == 0x41,
               "DataThenStrobePulse_CapturesExactlyOneByte");
    }

    // --- Strobe held low (no edge) captures nothing. ---
    {
        PrinterPort port;
        port.reset();
        port.io_write(0x90, 0x00);  // first falling edge (idle high -> low): captures once
        port.io_write(0x91, 0x99);  // change data while strobe stays LOW
        port.io_write(0x90, 0x00);  // still low -> low, NOT an edge -> no capture
        // The first write DID capture (falling edge from the idle-high
        // default); assert no SECOND capture happened for the low->low write.
        expect(port.captured_bytes().size() == 1, "StrobeHeldLow_NoRepeatCapture");
    }

    // --- Writing data with strobe already low (never pulsed back up first)
    //     then pulsing low again produces no additional capture (no edge). ---
    {
        PrinterPort port;
        port.reset();
        port.io_write(0x90, 0x00);  // falling edge -> captures data_ (0x00 default)
        expect(port.captured_bytes().size() == 1, "InitialFallingEdge_CapturesDefaultData");
        port.io_write(0x90, 0x00);  // still low -> no edge
        expect(port.captured_bytes().size() == 1, "RepeatedLowWrite_NoAdditionalCapture");
    }

    // --- Multiple pulses capture multiple bytes IN ORDER. ---
    {
        PrinterPort port;
        port.reset();
        const std::vector<std::uint8_t> bytes{0x10, 0x20, 0x30, 0x7F};
        for (const std::uint8_t b : bytes) {
            port.io_write(0x91, b);
            port.io_write(0x90, 0x00);  // falling edge -> capture
            port.io_write(0x90, 0x01);  // rising edge -> idle
        }
        expect(port.captured_bytes() == bytes, "MultiplePulses_CaptureInOrder");
    }

    // --- Status read (#90/#94) is ALWAYS 0x00 (ready) by default in this
    //     milestone, matching unused_bits=0x00 (A-M18-7). ---
    {
        PrinterPort port;
        port.reset();
        expect(port.io_read(0x90) == 0x00, "StatusRead_Port90_AlwaysReady");
        port.io_write(0x91, 0x55);
        port.io_write(0x90, 0x00);
        port.io_write(0x90, 0x01);
        expect(port.io_read(0x90) == 0x00, "StatusRead_Port90_StillReadyAfterCapture");
    }

    // --- #91 read is open-bus 0xFF (write-only per the XML). ---
    {
        PrinterPort port;
        port.reset();
        port.io_write(0x91, 0xAB);
        expect(port.io_read(0x91) == 0xFF, "DataRead_Port91_OpenBus");
    }

    // --- mod-4 cases 2/3 (#92/#93) are inert/open-bus (A-M18-6: #93 PDIR is
    //     explicitly not implemented, matching openMSX's own scope limit). ---
    {
        PrinterPort port;
        port.reset();
        expect(port.io_read(0x92) == 0xFF, "Port92Read_OpenBus");
        expect(port.io_read(0x93) == 0xFF, "Port93Read_OpenBus");
        port.io_write(0x92, 0x7E);  // no-op
        port.io_write(0x93, 0x7E);  // PDIR, unimplemented -- no-op
        expect(port.captured_bytes().empty(), "Port92And93Writes_NoCapture");
        expect(port.data() == 0, "Port92And93Writes_DataUnaffected");
    }

    // --- #94-#97 alias #90-#93 exactly (mod-4 dispatch). ---
    {
        PrinterPort port;
        port.reset();
        port.io_write(0x95, 0x77);  // == #91 (data write)
        port.io_write(0x94, 0x00);  // == #90 (strobe low, falling edge)
        expect(port.captured_bytes().size() == 1 && port.captured_bytes()[0] == 0x77,
               "Port94And95_AliasPort90And91");
        expect(port.io_read(0x94) == 0x00, "Port94Read_AliasesPort90Status");
        expect(port.io_read(0x95) == 0xFF, "Port95Read_AliasesPort91OpenBus");
        expect(port.io_read(0x96) == 0xFF, "Port96Read_AliasesPort92OpenBus");
        expect(port.io_read(0x97) == 0xFF, "Port97Read_AliasesPort93OpenBus");
    }

    // --- reset() does not spuriously capture (fields set directly, not via
    //     the edge-detecting setter). ---
    {
        PrinterPort port;
        port.io_write(0x91, 0x64);
        port.io_write(0x90, 0x00);
        port.io_write(0x90, 0x01);
        expect(port.captured_bytes().size() == 1, "PreReset_OneCapture");
        port.reset();
        expect(port.captured_bytes().empty(), "Reset_ClearsCaptures");
        expect(port.strobe(), "Reset_StrobeIdleHigh");
        expect(port.data() == 0, "Reset_DataZero");
        // No spurious capture landed DURING reset itself.
        port.io_write(0x90, 0x00);  // first genuine edge post-reset
        expect(port.captured_bytes().size() == 1, "Reset_NoSpuriousCaptureDuringReset");
    }

    // --- Two-run determinism: identical write sequence -> identical
    //     captured bytes and final state. ---
    {
        auto run = []() {
            PrinterPort port;
            port.reset();
            const std::vector<std::uint8_t> bytes{0x01, 0x02, 0x03};
            for (const std::uint8_t b : bytes) {
                port.io_write(0x95, b);
                port.io_write(0x94, 0x00);
                port.io_write(0x94, 0x01);
            }
            return port.captured_bytes();
        };
        const std::vector<std::uint8_t> a = run();
        const std::vector<std::uint8_t> b = run();
        expect(a == b, "TwoRunDeterminism_IdenticalSequence_IdenticalCaptures");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Peripherals_PrinterPort_Unit cases passed\n";
    return 0;
}
