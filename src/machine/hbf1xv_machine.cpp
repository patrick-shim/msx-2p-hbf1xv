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
    // HB-F1XV slot/sub-slot/page population, derived from the authoritative
    // machine XML references/openmsx-21.0/share/machines/Sony_HB-F1XV.xml (§2 of
    // docs/m13-planner-package.md). BOTH primary slots 0 and 3 are expanded (each
    // has four <secondary> children: XML lines 85-116 and 123-199; A-6 corrects
    // the M11 wiring that expanded only slot 3).
    slot_bus_.set_expanded(0, true);
    slot_bus_.set_expanded(3, true);

    // Slot 0-0: MSX BIOS + BASIC ROM, <mem base=0x0000 size=0x8000> -> pages 0-1
    // (XML:87-98). Reset fetch origin once #A8=0.
    slot_bus_.attach(0, 0, 0, &bios_rom_);
    slot_bus_.attach(0, 0, 1, &bios_rom_);
    // Slot 0-1 / 0-2 empty (XML:101,103); slot 0-3 MSX-JE Halnote (XML:106-114)
    // is DEFERRED (D-3) -> reserved open-bus (unattached).

    // Slot 3-0: 64 KB main-RAM MemoryMapper, pages 0-3 (XML:125-130). The mapper
    // RAM consumes mapper_io_'s live #FC-#FF segments (fact-sheet §4/§9).
    for (int page = 0; page < devices::chipset::SlotBus::kPages; ++page) {
        slot_bus_.attach(3, 0, page, &ram_mapper_);
    }
    // Slot 3-1: MSX Sub ROM at page 0 (<mem 0x0000/0x4000>, XML:134-145) and MSX
    // Kanji Driver/BASIC at pages 1-2 (<mem 0x4000/0x8000>, XML:146-157) coexist
    // at disjoint pages; page 3 of 3-1 stays open-bus.
    slot_bus_.attach(3, 1, 0, &sub_rom_);
    slot_bus_.attach(3, 1, 1, &kanji_rom_);
    slot_bus_.attach(3, 1, 2, &kanji_rom_);
    // Slot 3-2: WD2793 DISK ROM PRESENCE only, page 1 (<mem 0x4000/0x4000>,
    // rom_visibility page 1, XML:161-176). FDC mechanics are OUT of M13.
    slot_bus_.attach(3, 2, 1, &disk_rom_);
    // Slot 3-3: MSX-MUSIC (FM-PAC) ROM PRESENCE only, page 1 (<mem 0x4000/0x4000>,
    // XML:180-196). OPLL synthesis + #7C/#7D I/O are OUT of M13.
    slot_bus_.attach(3, 3, 1, &fmmusic_rom_);

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
    // Main RAM power-on content (A-5): the XML alternating 00/FF initialContent,
    // NOT all-zero (Sony_HB-F1XV.xml:129; openMSX Ram::clear, Ram.cc:37-78). VRAM
    // and FM-PAC SRAM remain zero-initialized (no initialContent for them).
    initialize_dram_pattern();
    vram_.clear();
    sram_.clear();

    // Reset the S1985 chipset volatile state (primary select + sub-slot regs -> 0).
    slot_bus_.reset();
    switched_io_.reset();
    s1985_engine_.reset();

    // Populate the ROM devices from the local bios/ assets (missing-asset policy
    // A-7: absent/unreadable/wrong-size -> 0xFF fill + recorded diagnostic).
    load_rom_assets();

    // --- Authentic reset slot default (A-2 / discharges M11 R-1) ---
    // Real hardware resets #A8 to 0 and every sub-slot register to 0, so page 0
    // resolves to slot 0-0 = BIOS ROM and the Z80 fetches the reset vector at
    // 0x0000 from BIOS (Zilog Z80A §7; S1985 §3). This supersedes the M11
    // bring-up default (#A8 = 0xFF), now that slot 0 is populated with the BIOS
    // ROM device. Tests that run a program from RAM call map_flat_ram() to page
    // the 64 KB RAM in explicitly (M13-S4 reconciliation).
    ppi_slot_select_.io_write(0xA8, 0x00);  // drives slot_bus_ primary select -> slot 0
    slot_bus_.write_ffff(0x00);             // page-3 sub-slot register -> sub 0

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

void Hbf1xvMachine::initialize_dram_pattern() {
    // Decoded XML initialContent (Sony_HB-F1XV.xml:129 comment):
    //   (chr(0)+chr(255))*128 + (chr(255)+chr(0))*128  -> a 512-byte pattern:
    //   bytes 0..255   alternate 00,FF (even index 00, odd FF)
    //   bytes 256..511 alternate FF,00 (even index FF, odd 00)
    // openMSX repeats this pattern over the whole 64 KB (Ram::clear, Ram.cc:66-73).
    for (std::size_t i = 0; i < kDramBytes; ++i) {
        const std::size_t p = i & 0x1FFu;  // position within the 512-byte pattern
        std::uint8_t value;
        if (p < 256) {
            value = (p & 1u) ? 0xFF : 0x00;
        } else {
            value = (p & 1u) ? 0x00 : 0xFF;
        }
        dram_.write(i, value);
    }
}

void Hbf1xvMachine::load_rom_assets() {
    rom_diagnostics_.clear();
    RomAssetLoader loader(asset_root_);

    // filename / expected window size / slot-role label. Sizes are the XML <mem>
    // window sizes (§2). Local SHA1s were confirmed to match the XML "confirmed
    // by Meits" revisions via tools/checksum-assets.ps1 (A-1) — never asserted here.
    bios_rom_.set_image(loader.load({"f1xvbios.rom", 0x8000, "slot 0-0 pages 0-1 (BIOS+BASIC)"}));
    sub_rom_.set_image(loader.load({"f1xvext.rom", 0x4000, "slot 3-1 page 0 (SUB)"}));
    kanji_rom_.set_image(loader.load({"f1xvkdr.rom", 0x8000, "slot 3-1 pages 1-2 (Kanji driver)"}));
    disk_rom_.set_image(loader.load({"f1xvdisk.rom", 0x4000, "slot 3-2 page 1 (DISK presence)"}));
    fmmusic_rom_.set_image(loader.load({"f1xvmus.rom", 0x4000, "slot 3-3 page 1 (FM-MUSIC presence)"}));

    // Diagnostics go to the machine diagnostics list only (rom_diagnostics()),
    // NOT the execution-event log: the event stream must stay byte-deterministic
    // and independent of whether the working directory can resolve bios/ (the M10
    // event-log golden depends on it). rom_diagnostics() is the authoritative,
    // never-fabricated missing-asset record; a green run has it empty.
    rom_diagnostics_ = loader.diagnostics();
}

void Hbf1xvMachine::map_flat_ram() {
    // Page the 64 KB mapper RAM (slot 3-0) into all four CPU pages as a flat,
    // linear 64 KB view (see the header doc). #A8 = 0xFF -> every page primary
    // slot 3; slot-3 sub-slot register 0 -> sub 0 = RAM mapper; mapper segments
    // {0,1,2,3} -> page p maps to physical p*0x4000.
    ppi_slot_select_.io_write(0xA8, 0xFF);
    slot_bus_.write_ffff(0x00);
    for (std::uint16_t page = 0; page < 4; ++page) {
        mapper_io_.io_write(static_cast<std::uint16_t>(0xFC + page), static_cast<std::uint8_t>(page));
    }
}

void Hbf1xvMachine::set_asset_root(std::filesystem::path root) {
    asset_root_ = std::move(root);
}

const std::filesystem::path& Hbf1xvMachine::asset_root() const {
    return asset_root_;
}

const std::vector<std::string>& Hbf1xvMachine::rom_diagnostics() const {
    return rom_diagnostics_;
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

std::uint8_t Hbf1xvMachine::debug_bus_read(const std::uint16_t address) {
    return bus_.read(address);
}

void Hbf1xvMachine::debug_bus_write(const std::uint16_t address, const std::uint8_t value) {
    bus_.write(address, value);
}

std::uint8_t Hbf1xvMachine::debug_io_read(const std::uint16_t port) {
    return bus_.io_read(port);
}

void Hbf1xvMachine::debug_io_write(const std::uint16_t port, const std::uint8_t value) {
    bus_.io_write(port, value);
}

bool Hbf1xvMachine::slot_expanded(const int primary) const {
    return slot_bus_.is_expanded(primary);
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
