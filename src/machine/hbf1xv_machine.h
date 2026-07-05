#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "core/bus.h"
#include "core/scheduler.h"
#include "devices/chipset/io_bus.h"
#include "devices/chipset/mapper_io.h"
#include "devices/chipset/ppi_slot_select.h"
#include "devices/chipset/s1985_engine.h"
#include "devices/chipset/slot_bus.h"
#include "devices/chipset/switched_io.h"
#include "devices/chipset/system_bus.h"
#include "devices/cpu/cpu_bus_client.h"
#include "devices/cpu/z80a_cpu.h"
#include "machine/cpu_trace_sink.h"
#include "machine/debug_event_log.h"
#include "machine/memory_region.h"
#include "machine/ram_slot_backing.h"

namespace sony_msx::machine {

class Hbf1xvMachine {
public:
    Hbf1xvMachine();

    void cold_boot();
    void run_frame();
    void run_frames(std::uint32_t frames);
    void run_cycles(std::uint64_t cycles);
    bool run_until_cycle(std::uint64_t target_cycle);
    std::uint32_t step_cpu_instruction();
    void load_memory(std::uint16_t address, const std::uint8_t* bytes, std::uint32_t size);
    [[nodiscard]] std::uint8_t read_memory(std::uint16_t address) const;
    [[nodiscard]] const devices::cpu::Z80aCpu& cpu() const;
    devices::cpu::Z80aCpu& cpu();
    [[nodiscard]] std::uint64_t elapsed_cycles() const;
    [[nodiscard]] std::uint64_t frame_count() const;
    [[nodiscard]] std::uint64_t frame_cycles_per_frame() const;

    // Deterministic CPU trace-export facility (M10-S1). Off by default. When
    // enabled, the machine attaches its owned sink as the CPU's per-instruction
    // observer; the sink collects records and serializes a diffable text form.
    // Disabling detaches the observer (zero effect on emulation).
    void set_cpu_trace_enabled(bool enabled);
    [[nodiscard]] bool cpu_trace_enabled() const;
    [[nodiscard]] const CpuTraceSink& cpu_trace() const;
    CpuTraceSink& cpu_trace();

    // Minimum INERT memory regions (M10-S2). These are pure storage byte
    // buffers, sized to the strict Target Machine Specification. They are
    // deterministically zero-initialized at cold_boot and expose read/write/
    // load/dump via their MemoryRegion accessors. They carry NO device
    // behavior (no slot/mapper decoding, no V9958 VDP semantics, no FM-PAC
    // battery/mapper behavior, no I/O bus) — those are separate milestones
    // (planner DP-1/DP-2/DP-3).
    //
    // - DRAM = 64 KB main RAM. This is the same store the CPU sees over the
    //   bus; load_memory/read_memory below are the CPU-visible aliases.
    // - VRAM = 128 KB V9958 video RAM storage only (no VDP behavior).
    // - SRAM = FM-PAC battery SRAM inert region.
    //   Assumption: the strict spec table lists no SRAM capacity; the standard
    //   Panasonic FM-PAC carries 8 KB of battery-backed SRAM, so kSramBytes is
    //   set to 8 KB. Verification action: confirm the FM-PAC SRAM capacity
    //   against the real device datasheet / an FM-PAC ROM+SRAM dump when the
    //   FM-PAC device milestone (planner DP-3) is implemented.
    static constexpr std::size_t kDramBytes = 64 * 1024;
    static constexpr std::size_t kVramBytes = 128 * 1024;
    static constexpr std::size_t kSramBytes = 8 * 1024;

    [[nodiscard]] std::size_t dram_size() const;
    [[nodiscard]] std::size_t vram_size() const;
    [[nodiscard]] std::size_t sram_size() const;

    [[nodiscard]] const MemoryRegion& dram() const;
    MemoryRegion& dram();
    [[nodiscard]] const MemoryRegion& vram() const;
    MemoryRegion& vram();
    [[nodiscard]] const MemoryRegion& sram() const;
    MemoryRegion& sram();

    // Full-state debug dump + execution-event logging (M10-S3).
    //
    // Determinism is guaranteed by construction: every serializer is hand-rolled
    // ASCII (fixed field order, fixed-width uppercase hex, '\n' endings), event
    // T-state stamps come from the deterministic scheduler clock, and NO
    // wall-clock or environment-dependent value is ever embedded. Two identical
    // runs produce byte-for-byte identical dump/log content. Dumping is
    // non-perturbing: it reads state only and never advances the CPU or clock.
    //
    // Output-file layout (created on demand under the configurable debug root,
    // default "debug/"):
    //   <root>/traces/<name>  — full-state dump (write_state_dump) and flushed
    //                           per-instruction CPU trace (write_cpu_trace).
    //   <root>/logs/<name>    — execution-event log (write_event_log).

    // Execution-event logging is off by default. When enabled, cold_boot records
    // a Reset event, each step_cpu_instruction records an InstructionRetire (and
    // a Halt event on the step that enters the halted state), and write_state_dump
    // records a Dump event. Enable BEFORE cold_boot to capture the Reset event.
    void set_event_logging_enabled(bool enabled);
    [[nodiscard]] bool event_logging_enabled() const;
    [[nodiscard]] const DebugEventLog& debug_event_log() const;
    DebugEventLog& debug_event_log();

    // Configurable base directory for debug output (default "debug").
    void set_debug_root(std::filesystem::path root);
    [[nodiscard]] const std::filesystem::path& debug_root() const;

    // Deterministic full-state dump text (CPU + DRAM + SRAM + VRAM). Pure/const:
    // it does not mutate machine state, the CPU, or the clock.
    [[nodiscard]] std::string serialize_state_dump() const;

    // Write the full-state dump to <root>/traces/<filename>, creating the
    // directory if absent. Records a Dump event when event logging is enabled.
    // Returns true on success. Non-perturbing to CPU/memory/clock state.
    bool write_state_dump(const std::string& filename);

    // Flush the collected S1 per-instruction CPU trace to <root>/traces/<filename>.
    bool write_cpu_trace(const std::string& filename) const;

    // Write the execution-event log to <root>/logs/<filename>.
    bool write_event_log(const std::string& filename) const;

private:
    static bool write_text_file(const std::filesystem::path& directory, const std::string& filename,
                                const std::string& text);

    // Wire the slot/I/O decode fabric and S1985 layer onto the buses. Called
    // once from the constructor (static composition; per-boot volatile state is
    // (re)initialized in cold_boot).
    void wire_bus();

    core::Scheduler scheduler_;
    MemoryRegion dram_{kDramBytes};
    MemoryRegion vram_{kVramBytes};
    MemoryRegion sram_{kSramBytes};

    // S1985 "MSX-ENGINE" chipset + full system bus (M11). The machine owns the
    // decode fabrics and the residual engine layer; the CPU talks to SystemBus.
    devices::chipset::SlotBus slot_bus_;
    devices::chipset::IoBus io_bus_;
    RamSlotBacking dram_backing_{dram_};        // slot 3-0 main RAM (fact-sheet §9)
    devices::chipset::PpiSlotSelect ppi_slot_select_{slot_bus_};  // #A8 (+#AC mirror)
    devices::chipset::MapperIo mapper_io_;      // #FC-#FF mapper readback
    devices::chipset::SwitchedIoController switched_io_;  // #40-#4F switched I/O
    devices::chipset::S1985Engine s1985_engine_;  // backup RAM ID 0xFE + M1 wait
    devices::chipset::SystemBus bus_{slot_bus_, io_bus_};

    devices::cpu::CpuBusClient cpu_bus_client_;
    devices::cpu::Z80aCpu cpu_;
    CpuTraceSink cpu_trace_sink_;
    DebugEventLog debug_event_log_;
    bool event_logging_enabled_ = false;
    std::filesystem::path debug_root_{"debug"};
    std::uint64_t frame_count_ = 0;
};

}  // namespace sony_msx::machine
