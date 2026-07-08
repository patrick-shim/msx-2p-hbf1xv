#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "devices/cartridge/cartridge_mapper_type.h"
#include "frontend/sdl3_audio_presenter.h"
#include "frontend/sdl3_input_mapper.h"
#include "frontend/sdl3_video_presenter.h"
#include "machine/hbf1xv_machine.h"

namespace sony_msx::frontend {

// Session configuration (M26-S2/S7, docs/m26-planner-package.md §2.8).
struct Sdl3AppConfig {
    std::string bios_dir = "bios";
    std::optional<std::string> cart1_path;
    devices::cartridge::CartridgeMapperType cart1_type = devices::cartridge::CartridgeMapperType::Mirrored;
    std::optional<std::string> cart2_path;
    devices::cartridge::CartridgeMapperType cart2_type = devices::cartridge::CartridgeMapperType::Mirrored;
    // Real disk-image loading (A-M26-6): reads the file's bytes and mounts
    // them via the existing, unmodified devices::fdc::DiskImage(bytes)
    // constructor -- zero machine-level change required.
    std::optional<std::string> disk_path;
    int window_width = 640;
    int window_height = 480;
    // SDL_WINDOW_HIDDEN -- test/CI convenience; never required for a real
    // interactive session.
    bool hidden_window = false;
    // A bounded, non-interactive run length for headless/manual-verification
    // use (§2.3). std::nullopt (the default) means run_interactive() only
    // stops on SDL_EVENT_QUIT.
    std::optional<std::uint32_t> max_frames;
};

// The SDL3 real-time application (M26, backlog C9). Owns a real
// Hbf1xvMachine session end to end: window/renderer/audio-device creation,
// cold_boot() + asset loading, and the real-time frame loop.
//
// Architecture (docs/m26-planner-package.md §2.3, A-M26-5/A-M26-8): the
// CORE STEP (run_one_frame()) is a pure function of "poll input, step the
// CPU to the next frame boundary via step_cpu_instruction(), call
// on_vsync_boundary() (NEVER run_frame() -- the double-count hazard,
// R-M25-5), blit one video frame, pump one batch of audio samples" -- ZERO
// wall-clock delay/pacing logic, so ctest can call it directly, a bounded
// number of times, with fully deterministic, assertable results. The
// REAL-TIME wrapper (run_interactive()) adds SDL_Delay-based pacing +
// SDL_EVENT_QUIT handling on top and is NEVER exercised by ctest.
class Sdl3App {
public:
    // The real MSX NTSC system clock (X-pattern precedent: wd2793.h/
    // disk_drive.h/rp5c01.h/rensha_turbo.h all independently declare this
    // same constant).
    static constexpr std::uint64_t kSystemClockHz = 3579545;

    explicit Sdl3App(Sdl3AppConfig config);
    ~Sdl3App();

    Sdl3App(const Sdl3App&) = delete;
    Sdl3App& operator=(const Sdl3App&) = delete;

    // SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) + window/renderer/video-
    // presenter/audio-presenter creation, then cold_boot()s the owned
    // machine and loads the configured BIOS/cartridge(s)/disk. Returns false
    // (and records last_error()) on ANY failure -- never partially
    // initializes (a failed init() leaves shutdown() already applied).
    bool init();
    void shutdown();
    [[nodiscard]] bool initialized() const { return initialized_; }

    // The deterministic core step -- see class doc comment. Directly,
    // repeatedly callable from a ctest integration test.
    void run_one_frame();

    // The real-time outer loop: run_one_frame() + SDL_Delay-based pacing
    // toward ~16.69 ms/frame (kFrameCycles/kSystemClockHz, the same
    // pre-existing frame-cadence arithmetic docs/m26-planner-package.md
    // §2.3 computes from hbf1xv_machine.cpp's own kFrameCycles) +
    // SDL_PollEvent-driven SDL_EVENT_QUIT handling. NEVER called from ctest.
    // Honors config.max_frames when set (a bounded, non-interactive run for
    // headless/manual-verification use). Returns a process exit code.
    int run_interactive();

    [[nodiscard]] machine::Hbf1xvMachine& machine() { return machine_; }
    [[nodiscard]] const machine::Hbf1xvMachine& machine() const { return machine_; }
    [[nodiscard]] const std::string& last_error() const { return last_error_; }
    [[nodiscard]] bool quit_requested() const { return quit_requested_; }
    [[nodiscard]] std::uint64_t frames_run() const { return frames_run_; }

    [[nodiscard]] SDL_Window* window() const { return window_; }
    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }
    [[nodiscard]] Sdl3VideoPresenter* video_presenter() const { return video_presenter_.get(); }
    [[nodiscard]] Sdl3AudioPresenter* audio_presenter() const { return audio_presenter_.get(); }
    [[nodiscard]] Sdl3InputMapper& input_mapper() { return input_mapper_; }

private:
    void poll_and_dispatch_events();
    bool load_configured_assets();

    Sdl3AppConfig config_;
    machine::Hbf1xvMachine machine_;
    std::string last_error_;
    bool sdl_initialized_ = false;
    bool initialized_ = false;
    bool quit_requested_ = false;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<Sdl3VideoPresenter> video_presenter_;
    std::unique_ptr<Sdl3AudioPresenter> audio_presenter_;
    Sdl3InputMapper input_mapper_;

    std::uint64_t frames_run_ = 0;
};

}  // namespace sony_msx::frontend
