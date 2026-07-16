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

#include "frontend/sdl3_menu.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "frontend/sdl3_app.h"
#include "frontend/sdl3_video_presenter.h"

namespace sony_msx::frontend {

namespace {

// Snapshot the runtime state the menu reflects, from Sdl3App's public,
// read-only accessors. SDL/ImGui-free values only -- the pure MenuModel builder
// consumes this.
MenuState snapshot(const Sdl3App& app) {
    MenuState s;
    const machine::Hbf1xvMachine& machine = app.machine();
    s.disk_count = app.disk_count();
    s.slot1_loaded = machine.cartridge_slot1().loaded();  // M56: Eject Cartridge enablement
    s.slot2_loaded = machine.cartridge_slot2().loaded();
    s.paused = machine.pause_controller().button_engaged();
    s.speed_level = machine.pause_controller().speed_level();
    s.rensha_speed = machine.rensha_turbo().speed();
    s.dram_kb = machine.dram_size() / 1024u;
    s.fullscreen = app.fullscreen();
    s.scale = app.window_scale();
    if (app.video_presenter() != nullptr) {
        s.filter_nearest = app.video_presenter()->scale_mode() == SDL_SCALEMODE_NEAREST;
        s.border_enabled = app.video_presenter()->border_enabled();
    }
    s.border_runtime_settable = false;  // v1: border is config-time-only (no runtime setter)
    s.persistence = app.persistence();
    s.persistence_peak = app.persistence_mode() == PhosphorMode::Peak;
    s.master_volume = app.master_volume();
    s.fast_disk = machine.fast_disk();
    s.disk_writable = app.disk_writable();
    return s;
}

// Recursively draw a MenuItem list. On a click, records the action + param (only
// one dispatch per frame is possible -- a single click can't hit two items).
void draw_items(const std::vector<MenuItem>& items, MenuAction& clicked, int& clicked_param) {
    for (const MenuItem& it : items) {
        if (it.separator) {
            ImGui::Separator();
            continue;
        }
        if (!it.children.empty()) {
            if (ImGui::BeginMenu(it.label.c_str(), it.enabled)) {
                draw_items(it.children, clicked, clicked_param);
                ImGui::EndMenu();
            }
            continue;
        }
        const char* shortcut = it.hotkey_label.empty() ? nullptr : it.hotkey_label.c_str();
        if (ImGui::MenuItem(it.label.c_str(), shortcut, it.checked, it.enabled)) {
            clicked = it.action;
            clicked_param = it.param;
        }
    }
}

}  // namespace

Sdl3Menu::Sdl3Menu(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // never write imgui.ini beside the exe (presentation-only chrome)
    io.LogFilename = nullptr;
    // R3: keyboard navigation OFF so ImGui never binds Alt (never opens the menu
    // bar via Alt) and never raises WantCaptureKeyboard outside a focused text
    // widget (v1 has none) -- the host Alt+letter hotkeys stay unshadowed.
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
}

Sdl3Menu::~Sdl3Menu() {
    // R7: backends shut down while the SDL_Renderer is still alive (Sdl3App resets
    // this before SDL_DestroyRenderer). Renderer backend first, then platform.
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void Sdl3Menu::process_event(const SDL_Event& event) {
    ImGui_ImplSDL3_ProcessEvent(&event);
}

bool Sdl3Menu::wants_keyboard() const {
    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Sdl3Menu::wants_mouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}

void Sdl3Menu::begin_frame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Sdl3Menu::build(Sdl3App& app) {
    render_menu_bar(app);
    render_help_windows();
}

void Sdl3Menu::render(SDL_Renderer* renderer) {
    ImGui::Render();
    // §4.2 bracket: ImGui computes vertices in WINDOW-PIXEL coordinates
    // (io.DisplaySize from SDL_GetWindowSize), but the renderer is currently in
    // 320x240 LETTERBOX logical presentation -- submitting there would reinterpret
    // the geometry in the tiny logical space. Save, disable (1:1 pixels), draw,
    // restore -- so blit_frame()/present() and the MSX letterbox stay untouched.
    int w = 0;
    int h = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(renderer, &w, &h, &mode);
    SDL_SetRenderLogicalPresentation(renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_SetRenderLogicalPresentation(renderer, w, h, mode);
}

void Sdl3Menu::render_menu_bar(Sdl3App& app) {
    const MenuModel model = build_menu_model(snapshot(app));
    MenuAction clicked = MenuAction::None;
    int clicked_param = 0;
    if (ImGui::BeginMainMenuBar()) {
        for (const Menu& menu : model.menus) {
            if (ImGui::BeginMenu(menu.label.c_str())) {
                draw_items(menu.items, clicked, clicked_param);
                ImGui::EndMenu();
            }
        }
        ImGui::EndMainMenuBar();
    }
    if (clicked != MenuAction::None) {
        dispatch(clicked, clicked_param, app);
    }
}

void Sdl3Menu::render_help_windows() {
    if (show_hotkeys_window_) {
        if (ImGui::Begin("Hotkeys", &show_hotkeys_window_)) {
            ImGui::TextUnformatted("In-window host hotkeys (menu is mouse-operated):");
            ImGui::Separator();
            if (ImGui::BeginTable("hotkeys", 2, ImGuiTableFlags_SizingFixedFit)) {
                for (const HotkeyEntry& e : hotkey_table()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(e.hotkey.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(e.description.c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
    if (show_about_window_) {
        if (ImGui::Begin("About", &show_about_window_)) {
            ImGui::TextUnformatted("Sony HB-F1XV  -  MSX2+ emulator (1988)");
            ImGui::TextUnformatted("Z80A @ 3.58 MHz | Yamaha V9958 VDP (128 KB VRAM)");
            // LOW-2 (M56): SCC is a CARTRIDGE-side chip (present only with an SCC
            // game cart), NOT built-in. Keep the built-in/cartridge boundary explicit.
            ImGui::TextUnformatted("Built-in audio: PSG (YM2149) + MSX-MUSIC FM (YM2413)");
            ImGui::TextUnformatted("Cartridge audio (when inserted): Konami SCC, external FM-PAC");
            ImGui::TextUnformatted("Storage: WD2793 FDC, 720 KB 3.5\" floppy");
            ImGui::Separator();
            ImGui::Text("Menu UI: Dear ImGui %s (MIT) over SDL3.", IMGUI_VERSION);
        }
        ImGui::End();
    }
}

void Sdl3Menu::dispatch(const MenuAction action, const int param, Sdl3App& app) {
    machine::Hbf1xvMachine& machine = app.machine();
    switch (action) {
        // File
        case MenuAction::OpenDisk: app.open_disk_dialog(); break;
        case MenuAction::OpenCartridgeSlot1: app.open_cartridge_dialog(1); break;
        case MenuAction::OpenCartridgeSlot2: app.open_cartridge_dialog(2); break;
        case MenuAction::SwapDisk: app.swap_to_next_disk(); break;
        case MenuAction::EjectDisk: app.eject_disk(); break;
        case MenuAction::EjectCartridgeSlot1: app.eject_cartridge(1); break;
        case MenuAction::EjectCartridgeSlot2: app.eject_cartridge(2); break;
        case MenuAction::Exit: app.request_quit(); break;
        // Machine
        case MenuAction::Reset: app.request_reset(); break;
        case MenuAction::Pause: machine.pause_controller().press_pause_button(); break;
        case MenuAction::SetSpeed: machine.pause_controller().set_speed_level(param); break;
        case MenuAction::SetRensha: machine.rensha_turbo().set_speed(param); break;
        // Video
        case MenuAction::ToggleFullscreen: app.toggle_fullscreen(); break;
        case MenuAction::SetScale: app.set_scale(param); break;
        case MenuAction::SetFilterLinear: app.set_filter(SDL_SCALEMODE_LINEAR); break;
        case MenuAction::SetFilterNearest: app.set_filter(SDL_SCALEMODE_NEAREST); break;
        case MenuAction::PersistenceUp: app.step_persistence_up(); break;
        case MenuAction::PersistenceDown: app.step_persistence_down(); break;
        case MenuAction::TogglePersistenceMode: {
            // param 0 = Average, 1 = Peak. Only flip when the requested mode
            // differs from the current one (clicking the selected mode is inert).
            const bool want_peak = (param == 1);
            const bool is_peak = app.persistence_mode() == PhosphorMode::Peak;
            if (want_peak != is_peak) {
                app.toggle_persistence_mode();
            }
            break;
        }
        // Audio
        case MenuAction::VolumeUp: app.step_volume(+1); break;
        case MenuAction::VolumeDown: app.step_volume(-1); break;
        case MenuAction::ToggleMute: app.toggle_mute(); break;
        // Disk
        case MenuAction::ToggleFastDisk: machine.set_fast_disk(!machine.fast_disk()); break;
        case MenuAction::ToggleDiskWritable: app.toggle_disk_writable(); break;
        case MenuAction::NewBlankDisk: app.new_blank_disk_dialog(); break;
        // Help
        case MenuAction::HelpHotkeys: show_hotkeys_window_ = true; break;
        case MenuAction::HelpAbout: show_about_window_ = true; break;
        // Info/grayed items (RAM, Border, filter/radio submenu parents) are drawn
        // disabled and never dispatch.
        default: break;
    }
}

}  // namespace sony_msx::frontend
