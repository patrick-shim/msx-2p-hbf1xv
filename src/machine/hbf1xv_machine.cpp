#include "machine/hbf1xv_machine.h"

#include <fstream>
#include <ios>
#include <system_error>
#include <utility>

#include "machine/debug_dump.h"
#include "machine/debug_format.h"

namespace sony_msx::machine {

namespace {
constexpr std::uint64_t kFrameCycles = 228 * 262;
}  // namespace

Hbf1xvMachine::Hbf1xvMachine() : cpu_bus_client_(bus_), cpu_(cpu_bus_client_) {
    wire_bus();
}

void Hbf1xvMachine::wire_bus() {
    // --- Memory fabric (SlotBus) ---
    // HB-F1XV: 64 KB main RAM is the MemoryDevice at slot 3-0, spanning pages
    // 0-3; primary slot 3 is the only expanded slot (fact-sheet §9). ROM/mapper
    // RAM population of the other (primary, sub) cells arrives in M12.
    for (int page = 0; page < devices::chipset::SlotBus::kPages; ++page) {
        slot_bus_.attach(3, 0, page, &dram_backing_);
    }
    slot_bus_.set_expanded(3, true);

    // --- I/O fabric (IoBus) ---
    // PPI port A slot-select on #A8, plus the S1985 straight-alias mirror of all
    // four PPI ports #A8-#AB -> #AC-#AF (fact-sheet §3, §10). Only #A8 has a
    // device in M11; #A9-#AB and their mirrors are inert seams.
    io_bus_.attach(0xA8, &ppi_slot_select_);
    io_bus_.register_mirror(0xA8, 0xAC);
    io_bus_.register_mirror(0xA9, 0xAD);
    io_bus_.register_mirror(0xAA, 0xAE);
    io_bus_.register_mirror(0xAB, 0xAF);

    // VDP port mirror #98-#9B -> #9C-#9F (fact-sheet §7). No device is attached
    // in M11 (the V9958 is M13); the alias is wired so the M13 VDP on #98-#9B is
    // automatically reachable on #9C-#9F.
    io_bus_.register_mirror(0x98, 0x9C);
    io_bus_.register_mirror(0x99, 0x9D);
    io_bus_.register_mirror(0x9A, 0x9E);
    io_bus_.register_mirror(0x9B, 0x9F);

    // Mapper I/O readback on #FC-#FF (fact-sheet §4), pattern configured by S1985.
    s1985_engine_.configure_mapper(mapper_io_);
    for (std::uint16_t port = 0xFC; port <= 0xFF; ++port) {
        io_bus_.attach(static_cast<std::uint8_t>(port), &mapper_io_);
    }

    // Switched I/O on #40-#4F with the S1985 backup RAM (ID 0xFE) attached
    // (fact-sheet §6, §10).
    switched_io_.attach(&s1985_engine_);
    for (std::uint16_t port = 0x40; port <= 0x4F; ++port) {
        io_bus_.attach(static_cast<std::uint8_t>(port), &switched_io_);
    }
}

void Hbf1xvMachine::cold_boot() {
    scheduler_.reset();
    dram_.clear();
    vram_.clear();
    sram_.clear();

    // Reset the S1985 chipset volatile state.
    slot_bus_.reset();
    switched_io_.reset();
    s1985_engine_.reset();

    // --- M11 bring-up slot default (A-2 / risk R-1) ---
    // On real hardware #A8 resets to 0, routing every page to primary slot 0 =
    // BIOS ROM. BIOS/ROM do not exist until M12, so to keep the machine bootable
    // and all prior suites green, M11 initializes the slot registers so all four
    // pages resolve to slot 3-0 (DRAM): #A8 = 0xFF (every page -> primary slot 3)
    // and slot-3 sub-slot register = 0x00 (every page -> sub-slot 0 = DRAM).
    // This is an explicit, tracked deviation documented in
    // docs/m11-planner-package.md §1.3 A-2 / §9 R-1; M12 MUST flip the reset
    // default to the authentic #A8 = 0 / slot-0-BIOS once ROMs populate slot 0.
    ppi_slot_select_.io_write(0xA8, 0xFF);  // drives slot_bus_ primary select
    slot_bus_.write_ffff(0x00);             // slot-3 sub-slot register -> all sub 0

    cpu_.reset();
    cpu_.set_interrupt_mode(devices::cpu::InterruptMode::Im1);
    frame_count_ = 0;

    // The event log is a fresh, deterministic stream per boot. Enable logging
    // BEFORE cold_boot to capture the Reset event that opens the stream.
    debug_event_log_.clear();
    if (event_logging_enabled_) {
        debug_event_log_.record(DebugEventType::Reset, elapsed_cycles(), "cold_boot");
    }
}

void Hbf1xvMachine::run_frame() {
    scheduler_.tick(kFrameCycles);
    ++frame_count_;
}

void Hbf1xvMachine::run_frames(const std::uint32_t frames) {
    for (std::uint32_t i = 0; i < frames; ++i) {
        run_frame();
    }
}

void Hbf1xvMachine::run_cycles(const std::uint64_t cycles) {
    scheduler_.tick(cycles);
}

bool Hbf1xvMachine::run_until_cycle(const std::uint64_t target_cycle) {
    const std::uint64_t now = elapsed_cycles();
    if (target_cycle <= now) {
        return false;
    }

    run_cycles(target_cycle - now);
    return true;
}

std::uint32_t Hbf1xvMachine::step_cpu_instruction() {
    const bool halted_before = cpu_.state().halted();
    const std::uint16_t pre_pc = cpu_.state().regs().pc;
    const std::uint8_t opcode0 = bus_.read(pre_pc);

    // The Z80 core publishes datasheet T-states unchanged (M9 timing oracles stay
    // valid); the S1985 adds +1 T-state per M1 opcode-fetch cycle (fact-sheet §8;
    // A-4 / risk R-3). The MSX-accurate machine time is datasheet + M1 wait, and
    // that is what advances the scheduler and is reported/returned here.
    const std::uint32_t datasheet_tstates = cpu_.step();
    const std::uint32_t m1_wait = s1985_engine_.m1_wait_tstates(cpu_.m1_cycles_last_step());
    const std::uint32_t tstates = datasheet_tstates + m1_wait;
    scheduler_.tick(tstates);

    if (event_logging_enabled_) {
        const std::uint64_t stamp = elapsed_cycles();
        debug_event_log_.record(
            DebugEventType::InstructionRetire, stamp,
            "PC=" + debug_format::to_hex(pre_pc, 4) + " OP=" + debug_format::to_hex(opcode0, 2) +
                " T=" + debug_format::to_dec(tstates));
        if (!halted_before && cpu_.state().halted()) {
            debug_event_log_.record(DebugEventType::Halt, stamp,
                                    "PC=" + debug_format::to_hex(pre_pc, 4));
        }
    }

    return tstates;
}

void Hbf1xvMachine::load_memory(const std::uint16_t address, const std::uint8_t* bytes, const std::uint32_t size) {
    std::uint32_t write_address = address;
    for (std::uint32_t index = 0; index < size; ++index) {
        const std::uint8_t value = bytes[index];
        dram_.write(core::normalize_bus_address(write_address), value);
        ++write_address;
    }
}

std::uint8_t Hbf1xvMachine::read_memory(const std::uint16_t address) const {
    return dram_.read(address);
}

const devices::cpu::Z80aCpu& Hbf1xvMachine::cpu() const {
    return cpu_;
}

devices::cpu::Z80aCpu& Hbf1xvMachine::cpu() {
    return cpu_;
}

std::uint64_t Hbf1xvMachine::elapsed_cycles() const {
    return scheduler_.total_cycles();
}

std::uint64_t Hbf1xvMachine::frame_count() const {
    return frame_count_;
}

std::uint64_t Hbf1xvMachine::frame_cycles_per_frame() const {
    return kFrameCycles;
}

void Hbf1xvMachine::set_cpu_trace_enabled(const bool enabled) {
    cpu_.set_trace_observer(enabled ? &cpu_trace_sink_ : nullptr);
}

bool Hbf1xvMachine::cpu_trace_enabled() const {
    return cpu_.trace_observer() != nullptr;
}

const CpuTraceSink& Hbf1xvMachine::cpu_trace() const {
    return cpu_trace_sink_;
}

CpuTraceSink& Hbf1xvMachine::cpu_trace() {
    return cpu_trace_sink_;
}

std::size_t Hbf1xvMachine::dram_size() const {
    return dram_.size();
}

std::size_t Hbf1xvMachine::vram_size() const {
    return vram_.size();
}

std::size_t Hbf1xvMachine::sram_size() const {
    return sram_.size();
}

const MemoryRegion& Hbf1xvMachine::dram() const {
    return dram_;
}

MemoryRegion& Hbf1xvMachine::dram() {
    return dram_;
}

const MemoryRegion& Hbf1xvMachine::vram() const {
    return vram_;
}

MemoryRegion& Hbf1xvMachine::vram() {
    return vram_;
}

const MemoryRegion& Hbf1xvMachine::sram() const {
    return sram_;
}

MemoryRegion& Hbf1xvMachine::sram() {
    return sram_;
}

void Hbf1xvMachine::set_event_logging_enabled(const bool enabled) {
    event_logging_enabled_ = enabled;
}

bool Hbf1xvMachine::event_logging_enabled() const {
    return event_logging_enabled_;
}

const DebugEventLog& Hbf1xvMachine::debug_event_log() const {
    return debug_event_log_;
}

DebugEventLog& Hbf1xvMachine::debug_event_log() {
    return debug_event_log_;
}

void Hbf1xvMachine::set_debug_root(std::filesystem::path root) {
    debug_root_ = std::move(root);
}

const std::filesystem::path& Hbf1xvMachine::debug_root() const {
    return debug_root_;
}

std::string Hbf1xvMachine::serialize_state_dump() const {
    std::string out;
    out += debug_dump::kDumpFormatTag;
    out.push_back('\n');
    out += debug_dump::serialize_cpu(cpu_.state());
    out += debug_dump::serialize_region("DRAM", dram_.data(), dram_.size());
    out += debug_dump::serialize_region("SRAM", sram_.data(), sram_.size());
    out += debug_dump::serialize_region("VRAM", vram_.data(), vram_.size());
    out += "[END]\n";
    return out;
}

bool Hbf1xvMachine::write_text_file(const std::filesystem::path& directory, const std::string& filename,
                                    const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return false;
    }

    const std::filesystem::path path = directory / filename;
    // Binary mode so '\n' is written verbatim (no CRLF translation) — keeps the
    // on-disk bytes identical across platforms and byte-stable across runs.
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

bool Hbf1xvMachine::write_state_dump(const std::string& filename) {
    const std::string text = serialize_state_dump();
    const bool ok = write_text_file(debug_root_ / "traces", filename, text);
    if (event_logging_enabled_) {
        debug_event_log_.record(DebugEventType::Dump, elapsed_cycles(), "FILE=" + filename);
    }
    return ok;
}

bool Hbf1xvMachine::write_cpu_trace(const std::string& filename) const {
    return write_text_file(debug_root_ / "traces", filename, cpu_trace_sink_.serialize());
}

bool Hbf1xvMachine::write_event_log(const std::string& filename) const {
    return write_text_file(debug_root_ / "logs", filename, debug_event_log_.serialize());
}

}  // namespace sony_msx::machine
