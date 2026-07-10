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
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "machine/hbf1xv_machine.h"

// Suite: Hbf1xv_M36StreamCapture_System (DEC-0052 live stream-capture).
//
// Drives a deterministic crafted program (a countdown loop that HALTs) from RAM,
// arms the machine-level stream capture, and proves END TO END, against REAL
// on-disk output, that:
//   (1) while armed, a full per-component snapshot bundle is written at each
//       frame boundary into <root>/snapshot/stream_<id>/<frame-id>/;
//   (2) the instant the CPU enters HALT the capture AUTO-STOPS -- dumping the
//       per-instruction trace ring (oldest->newest) to
//       <root>/traces/stream_<id>_trace.txt and writing one final snapshot
//       (stream_<id>/HALT_<frame-id>/), then disarming;
//   (3) two identical armed runs produce a BYTE-IDENTICAL stream tree + trace
//       (the determinism oracle);
//   (4) an UNARMED run that executes the SAME program writes NOTHING and keeps
//       an empty ring (default-off no-op).
//
// DEC-0052 enhancement (M36 Bug B) adds two more cases:
//   (5) STACK-RUNAWAY finalize: a program that drives SP below kStreamStackFloor
//       (0x4000) with no HALT auto-finalizes into a CRASH_ bundle whose manifest
//       carries finalize_reason=sp_underflow (+ the ring dumped), and
//   (6) a per-sector FDC READ LOG: a real Read Sector while armed appends one
//       line to <root>/traces/stream_<id>_fdc.log with a CRC-32 that matches an
//       independent CRC of the mounted sector's 512 bytes.

namespace {

int g_failures = 0;
void expect(const bool ok, const char* name) {
    if (!ok) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

namespace fs = std::filesystem;
using sony_msx::machine::Hbf1xvMachine;

// LD HL,0x0C00 / (DEC HL / LD A,H / OR L / JR NZ -5) / HALT. ~3072 loop
// iterations -> spans at least one 59736-cycle frame boundary before the HALT,
// so at least one per-frame bundle is emitted, then the HALT finalizes.
const std::array<std::uint8_t, 9> kProgram{
    0x21, 0x00, 0x0C,  // LD HL,0x0C00
    0x2B,              // DEC HL
    0x7C,              // LD A,H
    0xB5,              // OR L
    0x20, 0xFB,        // JR NZ,-5  (-> DEC HL)
    0x76,              // HALT
};

// Read a whole file into a string (binary; verbatim bytes).
std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return out;
}

// Collect every regular file under `root` into a map keyed by its path RELATIVE
// to `root` (so two runs under different roots compare structurally). Sorted +
// deterministic.
std::map<std::string, std::string> collect_tree(const fs::path& root) {
    std::map<std::string, std::string> files;
    if (!fs::exists(root)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file()) {
            const std::string rel = fs::relative(entry.path(), root).generic_string();
            files[rel] = read_file(entry.path());
        }
    }
    return files;
}

// Drive the crafted program to completion (HALT) or a frame cap, optionally
// arming the stream first. Uses the production loop shape (step to each frame
// boundary via step_cpu_instruction(), then on_vsync_boundary()).
void drive(Hbf1xvMachine& m, const bool arm, const fs::path& debug_root) {
    m.cold_boot();
    m.set_debug_root(debug_root);
    m.map_flat_ram();  // program runs from RAM page 0 (M13-S4)
    m.load_memory(0x0000, kProgram.data(), static_cast<std::uint32_t>(kProgram.size()));

    if (arm) {
        m.set_stream_capture_enabled(true);  // stream id defaults to snapshot_id() (f0_c0)
    }

    constexpr int kMaxFrames = 20;
    const std::uint64_t target = m.frame_cycles_per_frame();
    for (int frame = 0; frame < kMaxFrames; ++frame) {
        const std::uint64_t start = m.elapsed_cycles();
        while (m.elapsed_cycles() - start < target) {
            m.step_cpu_instruction();
            if (m.cpu().state().halted()) {
                break;
            }
        }
        if (m.cpu().state().halted()) {
            break;  // HALT finalized mid-frame; do not seal a partial boundary
        }
        m.on_vsync_boundary();  // full frame completed -> per-frame bundle (when armed)
    }
}

// Count how many per-frame bundle directories exist under stream_<id>/ (each is
// a directory whose name starts with 'f', i.e. a snapshot_id(); excludes the
// HALT_ finalize bundle).
int count_frame_bundles(const fs::path& stream_dir) {
    int n = 0;
    if (!fs::exists(stream_dir)) {
        return 0;
    }
    for (const auto& entry : fs::directory_iterator(stream_dir)) {
        if (entry.is_directory() && !entry.path().filename().string().empty() &&
            entry.path().filename().string().front() == 'f') {
            ++n;
        }
    }
    return n;
}

// Last '\n'-terminated line of a trace text (excluding the trailing newline).
std::string last_line(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    std::size_t end = text.size();
    if (text.back() == '\n') {
        --end;
    }
    const std::size_t nl = text.rfind('\n', end == 0 ? 0 : end - 1);
    const std::size_t begin = (nl == std::string::npos) ? 0 : nl + 1;
    return text.substr(begin, end - begin);
}

// Independent standard IEEE-802.3 CRC-32 (reflected, poly 0xEDB88420) so the FDC
// case cross-checks the LOGGED crc against the sector bytes the test reads
// itself. Matches the machine's crc32 AND python's zlib.crc32 (the human's
// raw-.dsk tool), which is the whole point of that byte-for-byte diff.
std::uint32_t crc32_ref(const std::uint8_t* data, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1u) != 0u ? 0xEDB88320u : 0u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// Uppercase, zero-padded 8-hex -- mirrors debug_format::to_hex(v, 8).
std::string hex8(std::uint32_t v) {
    static const char* kD = "0123456789ABCDEF";
    std::string s(8, '0');
    for (int i = 7; i >= 0; --i) {
        s[static_cast<std::size_t>(i)] = kD[v & 0xFu];
        v >>= 4;
    }
    return s;
}

// Tiny Z80 assembler + Restore/Read-Sector(LBA0) probe, lifted verbatim from the
// proven hbf1xv_m16_fdc_integration_test so the FDC case drives a REAL sector
// read over the full M11 bus (SlotBus/IoBus) exactly as the disk ROM does.
class Prog {
public:
    void emit(std::initializer_list<std::uint8_t> bytes) {
        for (const std::uint8_t b : bytes) {
            bytes_.push_back(b);
        }
    }
    [[nodiscard]] std::size_t here() const { return bytes_.size(); }
    void emit_jr_back(const std::uint8_t opcode, const std::size_t target) {
        bytes_.push_back(opcode);
        const std::size_t next_pc = bytes_.size() + 1;
        const int disp = static_cast<int>(target) - static_cast<int>(next_pc);
        bytes_.push_back(static_cast<std::uint8_t>(disp & 0xFF));
    }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

constexpr std::uint16_t kBase = 0xC000;
constexpr std::uint16_t kBuffer = 0xC200;

std::vector<std::uint8_t> build_restore_read_sector_probe() {
    Prog p;
    p.emit({0x3E, 0x08});        // LD A,0x08
    p.emit({0x32, 0xFF, 0xFF});  // LD (0xFFFF),A  ; page1->slot3-2(FDC)
    p.emit({0x3E, 0x00});        // LD A,0x00
    p.emit({0x32, 0xFC, 0x7F});  // LD (0x7FFC),A  ; side = 0
    p.emit({0x3E, 0x80});        // LD A,0x80
    p.emit({0x32, 0xFD, 0x7F});  // LD (0x7FFD),A  ; drive-select=A, motor on
    p.emit({0x3E, 0x00});        // LD A,0x00
    p.emit({0x32, 0xF8, 0x7F});  // LD (0x7FF8),A  ; Restore (Type I)
    const std::size_t restore_wait = p.here();
    p.emit({0x3A, 0xF8, 0x7F});  // LD A,(0x7FF8)
    p.emit({0xE6, 0x01});        // AND 0x01 (Busy)
    p.emit_jr_back(0x20, restore_wait);
    p.emit({0x3E, 0x00});        // LD A,0x00
    p.emit({0x32, 0xF9, 0x7F});  // LD (0x7FF9),A  ; TR = 0
    p.emit({0x3E, 0x01});        // LD A,0x01
    p.emit({0x32, 0xFA, 0x7F});  // LD (0x7FFA),A  ; SR = 1
    p.emit({0x3E, 0x80});        // LD A,0x80
    p.emit({0x32, 0xF8, 0x7F});  // LD (0x7FF8),A  ; Read Sector (Type II)
    p.emit({0x21, static_cast<std::uint8_t>(kBuffer & 0xFF),
            static_cast<std::uint8_t>(kBuffer >> 8)});  // LD HL,kBuffer
    p.emit({0x01, 0x00, 0x02});  // LD BC,0x0200
    const std::size_t read_wait = p.here();
    p.emit({0x3A, 0xFF, 0x7F});  // LD A,(0x7FFF)
    p.emit({0xE6, 0x80});        // AND 0x80 (!DTRQ)
    p.emit_jr_back(0x20, read_wait);
    p.emit({0x3A, 0xFB, 0x7F});  // LD A,(0x7FFB) ; data
    p.emit({0x77});              // LD (HL),A
    p.emit({0x23});              // INC HL
    p.emit({0x0B});              // DEC BC
    p.emit({0x78});              // LD A,B
    p.emit({0xB1});              // OR C
    p.emit_jr_back(0x20, read_wait);
    p.emit({0x76});              // HALT
    return p.bytes();
}

}  // namespace

int main() {
    const fs::path base = fs::temp_directory_path() / "sony_msx_stream_test";
    std::error_code ec;
    fs::remove_all(base, ec);  // fresh

    const fs::path root_a = base / "A";
    const fs::path root_b = base / "B";
    const fs::path root_c = base / "C";
    const fs::path root_d = base / "D";
    const fs::path root_e = base / "E";
    const fs::path root_f = base / "F";              // DEC-0052 stream-light watchlog
    const fs::path root_g_light = base / "G_light";  // light coarse-anchor suppression
    const fs::path root_g_heavy = base / "G_heavy";  // heavy per-frame contrast

    // --- Run A: armed. ---
    Hbf1xvMachine a;
    drive(a, /*arm=*/true, root_a);

    const std::string stream_id = a.stream_capture_id();
    const fs::path stream_dir_a = root_a / "snapshot" / ("stream_" + stream_id);
    const fs::path trace_a = root_a / "traces" / ("stream_" + stream_id + "_trace.txt");

    // (2) AUTO-STOP on HALT: capture disarmed itself.
    expect(!a.stream_capture_active(), "AutoStop_OnHalt_Disarmed");

    // (1) at least one per-frame bundle directory exists.
    const int frame_bundles = count_frame_bundles(stream_dir_a);
    expect(frame_bundles >= 1, "PerFrame_Bundles_AtLeastOne");

    // The HALT finalize bundle exists (stream_<id>/HALT_<frame-id>/manifest.txt).
    bool halt_bundle = false;
    if (fs::exists(stream_dir_a)) {
        for (const auto& entry : fs::directory_iterator(stream_dir_a)) {
            if (entry.is_directory() &&
                entry.path().filename().string().rfind("HALT_", 0) == 0 &&
                fs::exists(entry.path() / "manifest.txt")) {
                halt_bundle = true;
            }
        }
    }
    expect(halt_bundle, "Finalize_HaltSnapshotBundle_Written");

    // (2) the trace ring dumped to disk, and it matches the in-memory ring text.
    const std::string trace_text_a = read_file(trace_a);
    expect(!trace_text_a.empty(), "Finalize_TraceFile_NonEmpty");
    expect(trace_text_a == a.serialize_stream_trace(), "Finalize_TraceFile_MatchesRing");

    // The ring holds every executed instruction (well under the 1M cap -> no
    // wrap), and the NEWEST record is the HALT at PC=0x0008 / opcode 0x76.
    expect(a.stream_trace_ring_size() > 100, "Ring_HoldsAllInstructions");
    const std::string tail = last_line(trace_text_a);
    expect(tail.find("PC=0008") != std::string::npos && tail.find("OP=76") != std::string::npos,
           "Ring_NewestRecord_IsHalt");
    // The captured #A8 primary-slot select is present on every line (flat RAM =
    // all pages -> primary slot 3 -> 0xFF).
    expect(tail.find("A8=FF") != std::string::npos, "Ring_Record_CarriesA8Slot");
    std::cout << "stream trace tail: " << tail << "\n";
    std::cout << "stream id=" << stream_id << " ring=" << a.stream_trace_ring_size()
              << " frame_bundles=" << frame_bundles << "\n";

    // --- Run B: identical armed run -> byte-identical tree + trace. ---
    Hbf1xvMachine b;
    drive(b, /*arm=*/true, root_b);

    expect(b.stream_capture_id() == stream_id, "Determinism_SameStreamId");
    expect(b.serialize_stream_trace() == a.serialize_stream_trace(), "Determinism_SameTraceRing");

    const auto tree_a = collect_tree(root_a);
    const auto tree_b = collect_tree(root_b);
    bool same_shape = tree_a.size() == tree_b.size();
    expect(same_shape && !tree_a.empty(), "Determinism_SameFileSet");
    bool all_identical = same_shape;
    for (const auto& [rel, content] : tree_a) {
        const auto it = tree_b.find(rel);
        if (it == tree_b.end() || it->second != content) {
            all_identical = false;
            std::cerr << "  divergent stream file: " << rel << "\n";
        }
    }
    expect(all_identical, "Determinism_ByteIdenticalStreamTree");

    // --- Run C: UNARMED run of the SAME program writes NOTHING. ---
    Hbf1xvMachine c;
    drive(c, /*arm=*/false, root_c);

    expect(c.stream_trace_ring_size() == 0, "DefaultOff_EmptyRing");
    expect(!c.stream_capture_active(), "DefaultOff_NeverArmed");
    expect(!fs::exists(root_c / "snapshot"), "DefaultOff_NoSnapshotDir");
    expect(!fs::exists(root_c / "traces"), "DefaultOff_NoTracesDir");
    // The CPU genuinely ran (and halted) in the unarmed run -- proving the
    // no-write is default-off behavior, not a dead run.
    expect(c.cpu().state().halted(), "DefaultOff_CpuStillExecuted");

    // The FDC-log CRC must be zlib-compatible so it diffs against the human's
    // python raw-.dsk tooling: pin it to the standard CRC-32 check value.
    {
        const std::uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
        expect(crc32_ref(check, sizeof(check)) == 0xCBF43926u, "Crc32_StandardCheckValue");
    }

    // --- Run D (DEC-0052 enhancement 2): per-sector FDC read log while
    //     streaming. Arm, drive a REAL Restore + Read Sector(LBA0) over the full
    //     bus, and prove one deterministic log line with a CRC that matches the
    //     mounted sector (the human diffs our returned bytes against the raw .dsk).
    {
        Hbf1xvMachine d;
        d.cold_boot();
        d.set_debug_root(root_d);
        d.map_flat_ram();
        const std::vector<std::uint8_t> probe = build_restore_read_sector_probe();
        d.load_memory(kBase, probe.data(), static_cast<std::uint32_t>(probe.size()));
        d.cpu().state().regs().pc = kBase;
        d.set_stream_capture_enabled(true);  // id = f0_c0 (cold_boot -> cycle 0)
        const std::string sid = d.stream_capture_id();

        int guard = 0;
        while (!d.cpu().state().halted() && guard < 2'000'000) {
            d.step_cpu_instruction();
            ++guard;
        }
        expect(d.cpu().state().halted(), "Fdc_Probe_ReachesHalt");
        expect(!d.stream_capture_active(), "Fdc_Stream_FinalizedOnHalt");

        const fs::path fdc_log = root_d / "traces" / ("stream_" + sid + "_fdc.log");
        const std::string log = read_file(fdc_log);
        expect(!log.empty(), "Fdc_Log_NonEmpty");
        // The single (non-multi) Read Sector => exactly one begin_read_sector =>
        // one log line for track0/side0/sector1 (== LBA 0), command 0x80.
        expect(log.find("track=0 side=0 sector=1 lba=0") != std::string::npos,
               "Fdc_Log_HasChsLba");
        expect(log.find("cmd=80") != std::string::npos, "Fdc_Log_HasReadCommand");
        expect(log.find("frame=0") != std::string::npos, "Fdc_Log_HasFrame");
        // The logged crc must equal an independent CRC-32 of the SAME sector bytes.
        std::array<std::uint8_t, 512> sector{};
        expect(d.disk_image().read_chs(0, 0, 1, sector.data()), "Fdc_Image_ReadChs_Ok");
        const std::string want_crc = "crc=" + hex8(crc32_ref(sector.data(), sector.size()));
        expect(log.find(want_crc) != std::string::npos, "Fdc_Log_CrcMatchesSector");
        std::cout << "fdc log line: " << last_line(log) << "  (want " << want_crc << ")\n";
    }

    // --- Run E (DEC-0052 enhancement 1): auto-finalize on a STACK RUNAWAY. Drive
    //     SP below kStreamStackFloor (0x4000) via three PUSHes and prove the
    //     capture finalizes itself into a CRASH_ bundle carrying
    //     finalize_reason=sp_underflow -- WITHOUT any HALT. ---
    {
        Hbf1xvMachine e;
        e.cold_boot();
        e.set_debug_root(root_e);
        e.map_flat_ram();
        // LD HL,0x1234 ; LD SP,0x4004 ; PUSH HL x3 (SP: 0x4002,0x4000,0x3FFE).
        // The 3rd PUSH underflows 0x4000 -> finalize fires; there is NO HALT.
        const std::array<std::uint8_t, 9> crash_prog{
            0x21, 0x34, 0x12,  // LD HL,0x1234
            0x31, 0x04, 0x40,  // LD SP,0x4004
            0xE5,              // PUSH HL -> SP=0x4002
            0xE5,              // PUSH HL -> SP=0x4000
            0xE5,              // PUSH HL -> SP=0x3FFE (< 0x4000)
        };
        e.load_memory(0x0000, crash_prog.data(), static_cast<std::uint32_t>(crash_prog.size()));
        e.set_stream_capture_enabled(true);
        const std::string sid = e.stream_capture_id();

        int guard = 0;
        while (e.stream_capture_active() && guard < 1000) {
            e.step_cpu_instruction();
            ++guard;
        }
        expect(!e.stream_capture_active(), "SpUnderflow_FinalizedItself");
        expect(!e.cpu().state().halted(), "SpUnderflow_NotAHalt");
        expect(e.cpu().state().regs().sp < 0x4000, "SpUnderflow_SpBelowFloor");

        // A CRASH_ bundle (not HALT_) with a manifest carrying the reason note.
        const fs::path stream_dir = root_e / "snapshot" / ("stream_" + sid);
        bool crash_bundle = false;
        std::string manifest_text;
        if (fs::exists(stream_dir)) {
            for (const auto& entry : fs::directory_iterator(stream_dir)) {
                if (entry.is_directory() &&
                    entry.path().filename().string().rfind("CRASH_", 0) == 0 &&
                    fs::exists(entry.path() / "manifest.txt")) {
                    crash_bundle = true;
                    manifest_text = read_file(entry.path() / "manifest.txt");
                }
            }
        }
        expect(crash_bundle, "SpUnderflow_CrashBundle_Written");
        expect(manifest_text.find("finalize_reason=sp_underflow") != std::string::npos,
               "SpUnderflow_Manifest_ReasonNote");
        // The trace ring dumped (holds the 5 executed instructions, oldest->newest).
        const fs::path trace_e = root_e / "traces" / ("stream_" + sid + "_trace.txt");
        const std::string trace_text_e = read_file(trace_e);
        expect(!trace_text_e.empty(), "SpUnderflow_TraceFile_NonEmpty");
        std::cout << "sp-underflow bundle written; sp=" << std::hex
                  << e.cpu().state().regs().sp << std::dec
                  << " ring=" << e.stream_trace_ring_size() << "\n";
    }

    // --- Run F (DEC-0052 stream-light): a LIGHT-mode armed run produces a
    //     per-event WATCHLOG for the decisive upstream events (memory writes to
    //     0x0039/0x003A/0xA5E1 + a write to VDP R#1) and does NOT emit a per-frame
    //     bundle (the HALT is immediate, in frame 0); the HALT finalize bundle is
    //     still written. Proves the watchlog + no-heavy-per-frame contract. ---
    {
        // 4 decisive writes then HALT. Layout (flat RAM, PC=0):
        //   0000 LD A,E3 / 0002 LD (0039),A  -> mem watch (pc=0002)
        //   0005 LD A,A5 / 0007 LD (003A),A  -> mem watch (pc=0007)
        //   000A LD A,01 / 000C LD (A5E1),A  -> mem watch (pc=000C)
        //   000F LD A,40 / 0011 OUT (99),A   -> #99 data latch 0x40
        //   0013 LD A,81 / 0015 OUT (99),A   -> R#1 = 0x40 (vdp watch, pc=0015)
        //   0017 HALT
        const std::array<std::uint8_t, 24> watch_prog{
            0x3E, 0xE3, 0x32, 0x39, 0x00, 0x3E, 0xA5, 0x32, 0x3A, 0x00, 0x3E, 0x01,
            0x32, 0xE1, 0xA5, 0x3E, 0x40, 0xD3, 0x99, 0x3E, 0x81, 0xD3, 0x99, 0x76,
        };
        Hbf1xvMachine f;
        f.cold_boot();
        f.set_debug_root(root_f);
        f.map_flat_ram();
        f.load_memory(0x0000, watch_prog.data(), static_cast<std::uint32_t>(watch_prog.size()));
        f.set_stream_capture_enabled(true, std::string{}, /*light=*/true);
        expect(f.stream_capture_light(), "Light_Armed_IsLight");
        const std::string sid = f.stream_capture_id();

        int guard = 0;
        while (!f.cpu().state().halted() && guard < 100000) {
            f.step_cpu_instruction();
            ++guard;
        }
        expect(f.cpu().state().halted(), "Light_Watch_ReachesHalt");
        expect(!f.stream_capture_active(), "Light_Watch_FinalizedOnHalt");

        const fs::path watch_log = root_f / "traces" / ("stream_" + sid + "_watch.log");
        const std::string wl = read_file(watch_log);
        expect(!wl.empty(), "Light_WatchLog_NonEmpty");
        expect(wl.find("pc=0002 WRITE addr=0039 val=E3") != std::string::npos, "Light_WatchLog_Vec0039");
        expect(wl.find("pc=0007 WRITE addr=003A val=A5") != std::string::npos, "Light_WatchLog_Vec003A");
        expect(wl.find("pc=000C WRITE addr=A5E1 val=01") != std::string::npos, "Light_WatchLog_SoundArm");
        expect(wl.find("pc=0015 VDP R1=40") != std::string::npos, "Light_WatchLog_VdpR1");
        expect(wl.find("frame=0") != std::string::npos, "Light_WatchLog_HasFrame");

        const fs::path stream_dir_f = root_f / "snapshot" / ("stream_" + sid);
        expect(count_frame_bundles(stream_dir_f) == 0, "Light_NoPerFrameBundle");
        bool halt_bundle_f = false;
        if (fs::exists(stream_dir_f)) {
            for (const auto& entry : fs::directory_iterator(stream_dir_f)) {
                if (entry.is_directory() &&
                    entry.path().filename().string().rfind("HALT_", 0) == 0 &&
                    fs::exists(entry.path() / "manifest.txt")) {
                    halt_bundle_f = true;
                }
            }
        }
        expect(halt_bundle_f, "Light_HaltBundle_Written");
        std::cout << "light watch.log:\n" << wl;
    }

    // --- Run G (DEC-0052 stream-light): the per-frame-snapshot SUPPRESSION +
    //     coarse-anchor interval, directly contrasted against heavy mode over the
    //     SAME frame boundaries. Driven purely by on_vsync_boundary() (frame_count_
    //     is the anchor driver) so the ring/trace stay tiny. LIGHT: 121 boundaries
    //     -> exactly ONE coarse anchor (the frame-120 bundle). HEAVY: 5 boundaries
    //     -> a per-frame bundle EVERY frame. ---
    {
        Hbf1xvMachine gl;
        gl.cold_boot();
        gl.set_debug_root(root_g_light);
        gl.set_stream_capture_enabled(true, std::string{}, /*light=*/true);
        const std::string sid_l = gl.stream_capture_id();
        for (int i = 0; i < 121; ++i) {
            gl.on_vsync_boundary();  // frame_count_ 1..121; anchor fires only at 120
        }
        const fs::path stream_dir_gl = root_g_light / "snapshot" / ("stream_" + sid_l);
        expect(count_frame_bundles(stream_dir_gl) == 1, "Light_CoarseAnchor_ExactlyOne");
        gl.set_stream_capture_enabled(false);  // manual finalize (MANUAL_ bundle)
        expect(!gl.stream_capture_active(), "Light_Coarse_Finalized");

        Hbf1xvMachine gh;
        gh.cold_boot();
        gh.set_debug_root(root_g_heavy);
        gh.set_stream_capture_enabled(true, std::string{}, /*light=*/false);  // heavy
        const std::string sid_h = gh.stream_capture_id();
        for (int i = 0; i < 5; ++i) {
            gh.on_vsync_boundary();
        }
        const fs::path stream_dir_gh = root_g_heavy / "snapshot" / ("stream_" + sid_h);
        expect(count_frame_bundles(stream_dir_gh) == 5, "Heavy_PerFrameBundle_EveryFrame");
        gh.set_stream_capture_enabled(false);
        std::cout << "light coarse anchors=" << count_frame_bundles(stream_dir_gl)
                  << " (of 121 boundaries); heavy per-frame bundles="
                  << count_frame_bundles(stream_dir_gh) << " (of 5 boundaries)\n";
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Hbf1xv_M36StreamCapture_System cases passed\n";
    return 0;
}
