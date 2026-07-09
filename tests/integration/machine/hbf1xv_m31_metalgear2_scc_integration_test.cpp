// Suite: Machine_Hbf1xvM31MetalGear2Scc_Integration (M31-S6, RC criterion
// 7(d), docs/m31-planner-package.md §4; closes M29's disclosed A-M29-4
// residual -- real-SCC-title audio evidence)
//
// roms/metalgear2_scc.rom (softwaredb SHA1
// 78560a5c2e6de02896a56c5662b1aaccaf9d386f = "Metal Gear 2 - Solid Snake",
// KonamiSCC, coordinator-registered 2026-07-09) with NO explicit type ->
// DB SHA1 match -> KonamiSCC -> boots under the real frame loop
// (step_cpu_instruction() + on_vsync_boundary(), the DEC-0034 shape) -> and
// during the title sequence the SCC genuinely produces samples THROUGH
// MachineAudioMixer (deterministic capture; SCC sample()-activity assertion,
// the criterion's own named alternative).
//
// A-M31-6 free negative control: MG2 is SCC-only -- the FM-activity scan
// must observe ZERO YM2413 key-on edges across the whole run (frame-boundary
// register polling; the title never writes the OPLL ports at all).
//
// SKIP DISCIPLINE (DEC-0016/A-M30-1 precedent): SKIPs -- never fails -- when
// the ROM/BIOS/softwaredb assets are absent or the ROM is a different dump.
//
// Deterministic oracle: identical ROM + BIOS bytes -> an identical
// mixed-PCM stream, byte-identical across two fully independent sessions of
// the same fixed frame count (no wall clock, no input).

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "frontend/machine_audio_mixer.h"
#include "machine/cartridge_identifier.h"
#include "machine/hbf1xv_machine.h"
#include "machine/sha1.h"
#include "machine/software_db.h"

#ifndef SONY_MSX_ROMS_DIR
#error "SONY_MSX_ROMS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif
#ifndef SONY_MSX_SOFTWAREDB_PATH
#error "SONY_MSX_SOFTWAREDB_PATH must be defined by the build (see tests/CMakeLists.txt)"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* name) {
    if (!condition) {
        std::cerr << "Case failed: " << name << "\n";
        ++g_failures;
    }
}

constexpr const char* kMg2Sha1 = "78560a5c2e6de02896a56c5662b1aaccaf9d386f";
constexpr int kMaxFrames = 1500;
constexpr int kTailFrames = 100;         // keep running a bit past first SCC activity
constexpr std::size_t kSamplesPerFrame = 735;  // ~59736 cycles / 81

struct SessionResult {
    std::vector<std::int16_t> pcm;
    int frames_run = 0;
    int first_scc_activity_frame = -1;
    long fm_key_on_edges = 0;
    bool scc_attached = false;
    std::uint8_t scc_enable_bits = 0;
};

SessionResult run_session(const std::vector<std::uint8_t>& image,
                          const sony_msx::devices::cartridge::CartridgeMapperType type,
                          const int fixed_frames) {
    using sony_msx::frontend::MachineAudioMixer;
    using sony_msx::machine::Hbf1xvMachine;

    SessionResult result;
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();
    if (machine.load_cartridge(1, type, image) !=
        sony_msx::devices::cartridge::CartridgeLoadResult::Ok) {
        return result;
    }

    const MachineAudioMixer mixer(81);
    const std::uint64_t target = machine.frame_cycles_per_frame();
    bool prev_keys[10] = {};  // 9 melody key bits + rhythm-mode drum keying

    for (int frame = 0; frame < kMaxFrames; ++frame) {
        const std::uint64_t frame_start = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - frame_start < target) {
            machine.step_cpu_instruction();
        }
        machine.on_vsync_boundary();
        ++result.frames_run;

        // Mixed-PCM production through the REAL M31 three-source mixer
        // (PSG + SCC + FM; the FM stays silent -- MG2 is SCC-only).
        auto* scc = machine.scc_chip(1);
        result.scc_attached = scc != nullptr;
        const std::vector<std::int16_t> pcm = mixer.mix_interleaved_stereo(
            machine.psg(), MachineAudioMixer::SccSources{scc, nullptr}, &machine.ym2413(),
            kSamplesPerFrame);
        result.pcm.insert(result.pcm.end(), pcm.begin(), pcm.end());

        if (scc != nullptr) {
            result.scc_enable_bits |= scc->enable_bits();
            if (scc->sample() != 0 && result.first_scc_activity_frame < 0) {
                result.first_scc_activity_frame = frame;
            }
        }

        // A-M31-6 FM-activity scan (frame-boundary key-bit polling).
        const auto& fm = machine.ym2413();
        bool keys[10] = {};
        for (int channel = 0; channel < 9; ++channel) {
            keys[channel] = (fm.register_value(static_cast<std::uint8_t>(0x20 + channel)) & 0x10) != 0;
        }
        keys[9] = fm.rhythm_enabled() &&
                  (fm.register_value(0x0E) & 0x1F) != 0;  // any drum keyed
        for (int k = 0; k < 10; ++k) {
            if (keys[k] && !prev_keys[k]) {
                ++result.fm_key_on_edges;
            }
            prev_keys[k] = keys[k];
        }

        if (fixed_frames > 0) {
            if (result.frames_run >= fixed_frames) {
                break;
            }
        } else if (result.first_scc_activity_frame >= 0 &&
                   frame >= result.first_scc_activity_frame + kTailFrames) {
            break;
        }
    }
    return result;
}

}  // namespace

int main() {
    using sony_msx::devices::cartridge::CartridgeMapperType;
    using sony_msx::machine::CartridgeIdentificationMethod;
    using sony_msx::machine::ParsedCartridgeSlotCli;
    using sony_msx::machine::SoftwareDb;
    using sony_msx::machine::resolve_cartridge_type;
    using sony_msx::machine::sha1_hex;

    // --- Skip gates (DEC-0016 discipline). ---
    const std::filesystem::path rom_path =
        std::filesystem::path(SONY_MSX_ROMS_DIR) / "metalgear2_scc.rom";
    std::ifstream rom_in(rom_path, std::ios::binary);
    if (!rom_in) {
        std::cout << "SKIP: " << rom_path << " not present (local dev asset)\n";
        return 0;
    }
    const std::vector<std::uint8_t> image((std::istreambuf_iterator<char>(rom_in)),
                                          std::istreambuf_iterator<char>());
    const std::string actual_sha1 = sha1_hex(image);
    if (actual_sha1 != kMg2Sha1) {
        std::cout << "SKIP: metalgear2_scc.rom sha1=" << actual_sha1
                  << " differs from the specified dump " << kMg2Sha1 << "\n";
        return 0;
    }
    if (!std::filesystem::exists(std::filesystem::path(SONY_MSX_BIOS_DIR) / "f1xvbios.rom")) {
        std::cout << "SKIP: BIOS assets not present\n";
        return 0;
    }
    const std::string db_path = SONY_MSX_SOFTWAREDB_PATH;
    if (!std::filesystem::exists(db_path)) {
        std::cout << "SKIP: softwaredb not present at " << db_path << "\n";
        return 0;
    }

    // --- Auto-identification via DB SHA1 (RC 7(d): no type flag). ---
    std::vector<std::string> diagnostics;
    const auto db = SoftwareDb::load_from_file(db_path, diagnostics);
    expect(db.has_value(), "SoftwareDb_Loads");
    if (!db.has_value()) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    ParsedCartridgeSlotCli spec;
    spec.path = rom_path.string();
    spec.type = CartridgeMapperType::Mirrored;
    spec.type_was_explicit = false;
    const auto ident = resolve_cartridge_type(spec, image, &*db);
    expect(ident.type == CartridgeMapperType::KonamiSCC, "Mg2_IdentifiedAsKonamiSCC");
    expect(ident.method == CartridgeIdentificationMethod::SoftwareDbSha1,
           "Mg2_MethodIsSoftwareDbSha1");
    expect(ident.title == "Metal Gear 2 - Solid Snake", "Mg2_TitleFromDb");

    // --- Session 1: boot to the title sequence; find real SCC activity. ---
    const SessionResult first = run_session(image, ident.type, 0);
    expect(first.scc_attached, "Mg2_SccChipAttachedInBay1");
    expect(first.frames_run > 0 && first.frames_run <= kMaxFrames, "Mg2_SessionRan");
    expect(first.first_scc_activity_frame >= 0,
           "Mg2_RealTitleSccSamplesNonZero_ThroughMixer_ClosesAM29_4");
    expect(first.scc_enable_bits != 0, "Mg2_TitleEnabledSccChannels");
    // A-M31-6 negative control: an SCC-only title -- zero FM key-ons.
    expect(first.fm_key_on_edges == 0, "Mg2_FmActivityProbe_ZeroKeyOns_AM31_6");
    std::cout << "MG2 evidence: first SCC-activity frame = " << first.first_scc_activity_frame
              << ", frames run = " << first.frames_run
              << ", scc_enable_bits = " << static_cast<int>(first.scc_enable_bits)
              << ", fm_key_on_edges = " << first.fm_key_on_edges << "\n";

    // --- Session 2: the deterministic oracle -- an independent session of
    //     the identical fixed frame count is byte-identical. ---
    const SessionResult second = run_session(image, ident.type, first.frames_run);
    expect(second.frames_run == first.frames_run, "Mg2_SecondSession_SameFrameCount");
    expect(second.pcm == first.pcm, "Mg2_TwoSessions_ByteIdenticalMixedPcm");
    expect(second.first_scc_activity_frame == first.first_scc_activity_frame,
           "Mg2_TwoSessions_SameFirstActivityFrame");

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Machine_Hbf1xvM31MetalGear2Scc_Integration cases passed\n";
    return 0;
}
