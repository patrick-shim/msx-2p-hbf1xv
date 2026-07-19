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

// Suite: System_Hbf1xvM62RacingStability (M62, DEC-0091-AMENDMENT-A)
//
// Permanent regression oracle for a racing title's racing-view
// flicker (docs/m62-investigation-report.md): the M44 command-row sink
// (VdpRenderSyncAdapter::on_commit_up_to) used to seal rows far AHEAD of the
// render beam with COMMIT-TIME register state (frame-start R#26/R#27
// horizontal scroll + the instantaneous SAT). The game bends its straight-road
// page-0 canvas by rewriting R#26/#27 per scanline band mid-frame; once per
// ~13-frame beat its dashboard blit landed while the beam was at raster ~2,
// sealing ~145 rows with the frame-start scroll -> a full-scene flash
// (straight road, shifted dashboard, car sprite absent). openMSX 21.0 over
// the identical window: 0/40 anomalous (debug/m62/omx/run2-metrics.csv);
// ours pre-fix: 3/40 anomalous on an exact 13-frame period
// (debug/m62/ours/classification.csv). The M62 beam clamp
// (src/machine/hbf1xv_machine.cpp on_commit_up_to) restores the hardware
// contract -- a command writing rows ahead of the beam has NO early-display
// effect (fact-sheet "Yamaha V9958 VDP.md" per-scanline model, tier 1;
// openMSX renders only the backlog per command write, VDPVRAM.hh:575-593 --
// effect only, never copied).
//
// RECIPE (the pinned M62 repro, in-process equivalent of
//   sony_msx_headless --debug-session bios 0 --stock
//     --disk games/disks/racing/racing-d2.dsk --disk games/disks/racing/racing-d3.dsk
//     --swap-disk-frame 19000 --input-script debug/m62/scripts/step7.script):
// stock machine (64 KB, no FM-PAC, accurate disk timing), boot disk B
// ("NO.1" = racing-d2), scripted course-select/car-setup navigation, hot-swap to
// racing-d3 at frame 19000, SPACE held from ~f26500 (accelerator), racing view
// from ~f21500; measurement window = frames 30000..30039 while driving.
//
// ORACLE (the M51/M62 rate metric, self-referential -- no absolute pixel
// values pinned): across the 40 consecutive racing frames, build the
// per-sample MAJORITY of the dashboard band (rows [150, height), x step 2 --
// the band the pre-fix defect shifted by 428/~7900 sampled pixels while all
// 37 normal frames agreed exactly); a frame is ANOMALOUS when > 150 of its
// sampled band pixels differ from the majority. Assert 0/40 anomalous and 0
// present<->anomalous transitions (the openMSX reference rate). Pre-fix HEAD
// fails this exact test with 3/40 anomalous frames (the fail->pass proof is
// recorded in the M62 fix report).
//
// Non-vacuity guards: correct GRAPHIC4 racing-view geometry (256-wide), a
// colorful scene (>= 5 distinct colors, not near-monochrome), and live
// gameplay motion across the window (>= 10 of 40 frames differ from the
// first frame somewhere in the full picture) -- a hung/blank/prompt screen
// cannot pass vacuously.
//
// Game disks are owner-provisioned, untracked assets under games/disks/, so
// this test SKIPS (returns 0) when they are absent -- the same
// skip-when-absent discipline as the m19/m28/m58 asset-gated tests.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "devices/fdc/disk_image.h"
#include "devices/video/frame_buffer.h"
#include "machine/hbf1xv_machine.h"
#include "machine/input_script.h"

#ifndef SONY_MSX_BIOS_DIR
#error "SONY_MSX_BIOS_DIR must be defined by the build"
#endif
#ifndef SONY_MSX_GAMES_DIR
#error "SONY_MSX_GAMES_DIR must be defined by the build"
#endif

namespace {

int g_failures = 0;

void expect(const bool condition, const char* case_name) {
    if (!condition) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
}

// The pinned M62 repro input script, embedded VERBATIM from
// debug/m62/scripts/step7.script (parsed by the production parser, exactly as
// the headless --input-script path does): SPACE taps through title/course
// select (BRAZIL), DOWN/DOWN/SPACE car-body OK, 6x DOWN + SPACE through NOW
// SETTING, SPACE at the "PLEASE SET DISK B" prompt after the frame-19000
// swap, SPACE held from ~f26500 as the accelerator through the whole race.
constexpr const char* kRaceScript = R"(HBF1XV-INPUT-SCRIPT v1
T=298680000 KEY=SPACE DOWN
T=299277360 KEY=SPACE UP
T=307640400 KEY=SPACE DOWN
T=308237760 KEY=SPACE UP
T=316600800 KEY=SPACE DOWN
T=317198160 KEY=SPACE UP
T=325561200 KEY=SPACE DOWN
T=326158560 KEY=SPACE UP
T=334521600 KEY=SPACE DOWN
T=335118960 KEY=SPACE UP
T=343482000 KEY=SPACE DOWN
T=344079360 KEY=SPACE UP
T=352442400 KEY=SPACE DOWN
T=353039760 KEY=SPACE UP
T=361402800 KEY=SPACE DOWN
T=362000160 KEY=SPACE UP
T=370363200 KEY=SPACE DOWN
T=370960560 KEY=SPACE UP
T=379323600 KEY=SPACE DOWN
T=379920960 KEY=SPACE UP
T=388284000 KEY=SPACE DOWN
T=388881360 KEY=SPACE UP
T=397244400 KEY=SPACE DOWN
T=397841760 KEY=SPACE UP
T=406204800 KEY=SPACE DOWN
T=406802160 KEY=SPACE UP
T=415165200 KEY=SPACE DOWN
T=415762560 KEY=SPACE UP
T=424125600 KEY=SPACE DOWN
T=424722960 KEY=SPACE UP
T=433086000 KEY=SPACE DOWN
T=433683360 KEY=SPACE UP
T=442046400 KEY=SPACE DOWN
T=442643760 KEY=SPACE UP
T=451006800 KEY=SPACE DOWN
T=451604160 KEY=SPACE UP
T=459967200 KEY=SPACE DOWN
T=460564560 KEY=SPACE UP
T=468927600 KEY=SPACE DOWN
T=469524960 KEY=SPACE UP
T=477888000 KEY=SPACE DOWN
T=478485360 KEY=SPACE UP
T=486848400 KEY=SPACE DOWN
T=487445760 KEY=SPACE UP
T=495808800 KEY=SPACE DOWN
T=496406160 KEY=SPACE UP
T=504769200 KEY=SPACE DOWN
T=505366560 KEY=SPACE UP
T=513729600 KEY=SPACE DOWN
T=514326960 KEY=SPACE UP
T=522690000 KEY=SPACE DOWN
T=523287360 KEY=SPACE UP
T=531650400 KEY=SPACE DOWN
T=532247760 KEY=SPACE UP
T=669043200 KEY=DOWN DOWN
T=669640560 KEY=DOWN UP
T=678003600 KEY=DOWN DOWN
T=678600960 KEY=DOWN UP
T=686964000 KEY=SPACE DOWN
T=687561360 KEY=SPACE UP
T=955776000 KEY=DOWN DOWN
T=956373360 KEY=DOWN UP
T=961749600 KEY=DOWN DOWN
T=962346960 KEY=DOWN UP
T=967723200 KEY=DOWN DOWN
T=968320560 KEY=DOWN UP
T=973696800 KEY=DOWN DOWN
T=974294160 KEY=DOWN UP
T=979670400 KEY=DOWN DOWN
T=980267760 KEY=DOWN UP
T=985644000 KEY=DOWN DOWN
T=986241360 KEY=DOWN UP
T=997591200 KEY=SPACE DOWN
T=998188560 KEY=SPACE UP
T=1164852000 KEY=SPACE DOWN
T=1165449360 KEY=SPACE UP
T=1583004000 KEY=SPACE DOWN
T=2389440000 KEY=SPACE UP
[END]
)";

constexpr std::uint32_t kSwapDiskFrame = 19000;   // racing-d2 -> racing-d3 hot swap
constexpr std::uint32_t kFirstWindowFrame = 30000;  // first measured frame
constexpr std::uint32_t kWindowFrames = 40;         // f30000..f30039
constexpr std::uint32_t kTotalFrames = kFirstWindowFrame + kWindowFrames - 1;  // 30039

// Dashboard band sampling (the M62 discriminator): rows [150, height),
// every 2nd pixel. Pre-fix anomalous frames differ by 428 sampled pixels;
// all normal frames agree exactly (speed-digit jitter is a few pixels at
// most), so 150 is a wide, stable margin.
constexpr int kBandTopRow = 150;
constexpr int kBandXStep = 2;
constexpr int kAnomalyThreshold = 150;

// FNV-1a 64-bit over a frame's pixels (determinism evidence in the log).
std::uint64_t fnv1a(std::uint64_t h, const std::vector<std::uint16_t>& pixels) {
    for (const std::uint16_t p : pixels) {
        h ^= static_cast<std::uint64_t>(p & 0xFF);
        h *= 1099511628211ull;
        h ^= static_cast<std::uint64_t>(p >> 8);
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace

int main() {
    const std::string disk_dir = std::string(SONY_MSX_GAMES_DIR) + "/disks/racing";
    const std::string disk_b_path = disk_dir + "/racing-d2.dsk";  // game disk "NO.1" (boot)
    const std::string disk_c_path = disk_dir + "/racing-d3.dsk";  // game disk "NO.2" (swap)
    const std::vector<std::uint8_t> disk_b = read_file(disk_b_path);
    const std::vector<std::uint8_t> disk_c = read_file(disk_c_path);
    if (disk_b.size() != 737280 || disk_c.size() != 737280) {
        std::cout << "SKIP: racing-title disks not present under " << disk_dir
                  << " -- skip-when-absent asset guard (games/ is owner-provisioned, untracked)\n";
        return 0;
    }

    using sony_msx::devices::video::FrameBuffer;
    using sony_msx::machine::Hbf1xvMachine;
    using sony_msx::machine::InputScriptPlayer;

    // Stock machine (the pinned repro config): the default Hbf1xvMachine is
    // the bare 64 KB / no-cartridge / accurate-disk-timing machine (the
    // --stock CLI preset).
    Hbf1xvMachine machine;
    machine.set_asset_root(SONY_MSX_BIOS_DIR);
    machine.cold_boot();

    machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk_b);
    machine.disk_drive().attach_image(&machine.disk_image());

    InputScriptPlayer script_player(sony_msx::machine::parse_input_script(kRaceScript));

    // The --debug-session --frames loop shape (src/main.cpp frame-loop mode):
    // step to each frame boundary applying due input edges after each
    // instruction, deliver VBlank at every boundary, hot-swap the medium at
    // the scripted frame (attach + DSKCHG latch, the --swap-disk-frame
    // semantics). Capture the 40 window frames in-process.
    const std::uint64_t target = machine.frame_cycles_per_frame();
    std::vector<FrameBuffer> window;
    window.reserve(kWindowFrames);
    for (std::uint32_t frame = 0; frame < kTotalFrames; ++frame) {
        if (frame == kSwapDiskFrame) {
            machine.disk_image() = sony_msx::devices::fdc::DiskImage(disk_c);
            machine.disk_drive().attach_image(&machine.disk_image());
            machine.disk_drive().set_disk_changed(true);
        }
        const std::uint64_t start = machine.elapsed_cycles();
        while (machine.elapsed_cycles() - start < target) {
            machine.step_cpu_instruction();
            script_player.apply_due(machine.elapsed_cycles(), machine.keyboard());
        }
        machine.on_vsync_boundary();
        if (frame + 1 >= kFirstWindowFrame) {
            window.push_back(machine.render_frame());
        }
    }
    expect(window.size() == kWindowFrames, "Window_40FramesCaptured");
    if (window.size() != kWindowFrames) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }

    // Geometry guard: SCREEN5 racing view (256-wide GRAPHIC4 class).
    const int width = window[0].width;
    const int height = window[0].height;
    expect(width == 256 && (height == 192 || height == 212),
           "RacingFrame_Graphic4Geometry_256Wide");
    bool geometry_stable = true;
    for (const FrameBuffer& f : window) {
        geometry_stable = geometry_stable && f.width == width && f.height == height;
    }
    expect(geometry_stable, "RacingWindow_GeometryStable_AllFrames");
    if (!(width == 256 && height > kBandTopRow && geometry_stable)) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }

    // Determinism evidence: one hash over the whole window (dual runs of this
    // test must print the identical value -- gate (h)).
    std::uint64_t window_hash = 1469598103934665603ull;
    for (const FrameBuffer& f : window) {
        window_hash = fnv1a(window_hash, f.pixels);
    }
    std::cerr << "[M62 racing oracle] window=f" << kFirstWindowFrame << "..f"
              << (kFirstWindowFrame + kWindowFrames - 1) << " hash=" << std::hex << window_hash
              << std::dec << "\n";

    // --- Dashboard-band majority reference (per-sample majority vote). ---
    const int band_rows = height - kBandTopRow;
    const int band_cols = width / kBandXStep;
    const std::size_t samples = static_cast<std::size_t>(band_rows) * band_cols;
    std::vector<std::uint16_t> majority(samples, 0);
    {
        std::map<std::uint16_t, int> votes;
        std::size_t idx = 0;
        for (int y = kBandTopRow; y < height; ++y) {
            for (int x = 0; x < width; x += kBandXStep) {
                votes.clear();
                for (const FrameBuffer& f : window) {
                    ++votes[f.at(x, y)];
                }
                int best = -1;
                std::uint16_t best_val = 0;
                for (const auto& [val, n] : votes) {
                    if (n > best) {
                        best = n;
                        best_val = val;
                    }
                }
                majority[idx++] = best_val;
            }
        }
    }

    // --- Per-frame classification (the M51/M62 rate metric). ---
    int anomalous = 0;
    int transitions = 0;
    bool prev_anom = false;
    for (std::uint32_t i = 0; i < kWindowFrames; ++i) {
        const FrameBuffer& f = window[i];
        std::size_t idx = 0;
        int diff = 0;
        for (int y = kBandTopRow; y < height; ++y) {
            for (int x = 0; x < width; x += kBandXStep) {
                diff += (f.at(x, y) != majority[idx++]) ? 1 : 0;
            }
        }
        const bool anom = diff > kAnomalyThreshold;
        std::cerr << "[M62 racing oracle] f" << (kFirstWindowFrame + i) << " band_diff=" << diff
                  << (anom ? " ANOMALOUS" : "") << "\n";
        if (anom) {
            ++anomalous;
        }
        if (i > 0 && anom != prev_anom) {
            ++transitions;
        }
        prev_anom = anom;
    }
    std::cerr << "[M62 racing oracle] anomalous=" << anomalous << "/" << kWindowFrames
              << " transitions=" << transitions << " (openMSX reference rate: 0/40, 0)\n";

    expect(anomalous == 0, "RacingWindow_40Frames_ZeroAnomalousDashboardFrames");
    expect(transitions == 0, "RacingWindow_40Frames_ZeroPresentAnomalousTransitions");

    // --- Non-vacuity guards. ---
    {
        // Colorful racing scene, not blank/monochrome.
        std::vector<std::uint32_t> hist(1u << 15, 0);
        for (const std::uint16_t p : window[0].pixels) {
            ++hist[p & 0x7FFF];
        }
        int distinct = 0;
        std::uint32_t top = 0;
        for (const std::uint32_t n : hist) {
            if (n != 0) {
                ++distinct;
                if (n > top) {
                    top = n;
                }
            }
        }
        const double top_frac =
            static_cast<double>(top) / static_cast<double>(window[0].pixels.size());
        std::cerr << "[M62 racing oracle] distinct_colors=" << distinct << " top_color_frac="
                  << top_frac << "\n";
        expect(distinct >= 5, "RacingFrame_NonTrivial_AtLeastFiveColors");
        expect(top_frac <= 0.80, "RacingFrame_NonTrivial_NotMonochrome");
    }
    {
        // Live gameplay motion: the window is the moving race, not a hung
        // static screen (which would satisfy the 0-anomaly bar vacuously).
        int moving = 0;
        for (std::uint32_t i = 1; i < kWindowFrames; ++i) {
            if (window[i].pixels != window[0].pixels) {
                ++moving;
            }
        }
        std::cerr << "[M62 racing oracle] frames_differing_from_f30000=" << moving << "/39\n";
        expect(moving >= 10, "RacingWindow_LiveMotion_NotAStaticScreen");
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All System_Hbf1xvM62RacingStability cases passed\n";
    return 0;
}
