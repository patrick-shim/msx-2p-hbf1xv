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

#include <iostream>  // log_io_geometry diagnostic

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
    s.slot1_loaded = machine.cartridge_slot1().loaded();  // Eject Cartridge enablement
    s.slot2_loaded = machine.cartridge_slot2().loaded();
    s.paused = machine.pause_controller().button_engaged();
    s.speed_level = machine.pause_controller().speed_level();
    s.rensha_speed = machine.rensha_turbo().speed();
    s.dram_kb = machine.dram_size() / 1024u;
    s.bios_dir = app.bios_dir();  // label shows the current dir basename (DEC-0089)
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
    s.recent = app.recent_entries();  // File > Recent MRU, empty unless enabled (DEC-0095)
    // Bottom status-bar fields (DEC-0096).
    s.fdd_motor = machine.disk_drive().motor_on(machine.elapsed_cycles());
    s.disk_name = app.current_disk_name();
    s.fdd_track = machine.disk_drive().physical_track();
    s.disk_write_protected = machine.disk_drive().write_protected();
    s.slot1_name = app.cart_name(1);
    s.slot2_name = app.cart_name(2);
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
    // Keyboard navigation OFF so ImGui never binds Alt (never opens the menu
    // bar via Alt) and never raises WantCaptureKeyboard outside a focused text
    // widget (v1 has none) -- the host Alt+letter hotkeys stay unshadowed.
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    // Prime one empty frame so ImGui builds its font atlas
    // and sets io.FontGlobalScale/FontSize -> ImGui::GetFrameHeight() (bar_height())
    // returns the true bar height immediately, letting Sdl3App::init() size the
    // window (+strip height) and set the presenter inset BEFORE the first render.
    // A NewFrame/EndFrame pair is a valid empty frame (no draw data submitted).
    // (DEC-0085)
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGui::EndFrame();
}

Sdl3Menu::~Sdl3Menu() {
    // Backends shut down while the SDL_Renderer is still alive (Sdl3App resets
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

int Sdl3Menu::bar_height() const {
    // ImGui::GetFrameHeight() = FontSize + 2*FramePadding.y (imgui.cpp
    // BeginMainMenuBar). Rounded UP so the reserved strip never leaves a 1px
    // sliver of the picture under the bar. Valid from construction (primed frame).
    const float h = ImGui::GetFrameHeight();
    return h > 0.0f ? static_cast<int>(h + 0.999f) : 0;
}

int Sdl3Menu::status_bar_height() const {
    // Same GetFrameHeight() basis as the top bar (rounded up), so the
    // bottom strip reserves exactly the space the status window fills. (DEC-0096)
    const float h = ImGui::GetFrameHeight();
    return h > 0.0f ? static_cast<int>(h + 0.999f) : 0;
}

void Sdl3Menu::begin_frame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Sdl3Menu::build(Sdl3App& app) {
    render_menu_bar(app);
    render_status_bar(app);
    render_help_windows();
}

void Sdl3Menu::render_status_bar(Sdl3App& app) {
    const int status_h = status_bar_height();
    if (status_h <= 0) {
        return;
    }
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    if (disp.x <= 0.0f || disp.y <= 0.0f) {
        return;
    }
    const MenuState state = snapshot(app);

    // A fixed, non-interactive strip pinned to the reserved bottom band. The band
    // is reserved by Sdl3App (window grows by status_h; the picture insets ABOVE
    // it), so this window never overlaps the MSX picture.
    ImGui::SetNextWindowPos(ImVec2(0.0f, disp.y - static_cast<float>(status_h)));
    ImGui::SetNextWindowSize(ImVec2(disp.x, static_cast<float>(status_h)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 2.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 22, 26, 235));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("##fdd_status_bar", nullptr, flags)) {
        // FDD activity LED at the far left: pulsing green while the motor is on,
        // a dim dot when idle (so the indicator is always locatable).
        const float lit = fdd_led_alpha(state.fdd_motor, ImGui::GetTime());
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float line = ImGui::GetTextLineHeight();
        const float radius = line * 0.34f;
        const ImVec2 center(origin.x + radius + 1.0f, origin.y + line * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (lit > 0.0f) {
            const int a = static_cast<int>(lit * 255.0f);
            dl->AddCircleFilled(center, radius * 1.6f, IM_COL32(40, 255, 90, a / 4));
            dl->AddCircleFilled(center, radius, IM_COL32(48, 240, 96, a));
        } else {
            dl->AddCircleFilled(center, radius, IM_COL32(70, 90, 70, 180));  // idle
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x + radius * 2.0f + 8.0f, origin.y));
        ImGui::TextUnformatted(format_status_bar(state).c_str());
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void Sdl3Menu::render(SDL_Renderer* renderer) {
    ImGui::Render();
    // Logical-presentation bracket: ImGui computes vertices in WINDOW-PIXEL coordinates
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

void Sdl3Menu::log_io_geometry(const char* tag) const {
    // Diagnostic: dump what ImGui itself sees -- io.DisplaySize (the space
    // the menu bar lays out + hit-tests in) and io.DisplayFramebufferScale
    // (points->pixels). Paired with the SDL window/pixel/render-output sizes
    // Sdl3App prints, this pins the Pi fullscreen mis-scaling to real numbers
    // instead of a guess. Presentation-only; only called from interactive paths.
    const ImGuiIO& io = ImGui::GetIO();
    std::cerr << "sdl3: imgui[" << (tag != nullptr ? tag : "") << "] DisplaySize "
              << io.DisplaySize.x << "x" << io.DisplaySize.y << ", FramebufferScale "
              << io.DisplayFramebufferScale.x << "x" << io.DisplayFramebufferScale.y << "\n";
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
            // SCC is a CARTRIDGE-side chip (present only with an SCC
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
        case MenuAction::OpenRecent: app.open_recent(param); break;  // Recent-file open (DEC-0095)
        case MenuAction::Exit: app.request_quit(); break;
        // Machine
        case MenuAction::Reset: app.request_reset(); break;
        case MenuAction::Pause: machine.pause_controller().press_pause_button(); break;
        case MenuAction::SetSpeed: machine.pause_controller().set_speed_level(param); break;
        case MenuAction::SetRensha: machine.rensha_turbo().set_speed(param); break;
        // Machine>RAM live power-cycle. param is the KB
        // size; rebuild the machine at param*1024 ONLY when it differs from the live
        // size (re-selecting the current size is inert -- the TogglePersistenceMode
        // guard precedent). The orchestrator emits the stderr note.
        // (DEC-0085-AMENDMENT-A)
        case MenuAction::SetRam: {
            const std::size_t want_bytes = static_cast<std::size_t>(param) * 1024u;
            if (want_bytes != machine.dram_size()) {
                app.request_power_cycle(want_bytes);
            }
            break;
        }
        // Machine > BIOS Folder... -- launch the async folder
        // picker; the drained selection is transactionally validated + applied
        // in Sdl3App::apply_bios_folder (power-cycle into the chosen dir).
        // (DEC-0089)
        case MenuAction::OpenBiosFolder: app.open_bios_folder_dialog(); break;
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
