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

#pragma once

#include <SDL3/SDL.h>

#include "frontend/menu_model.h"

// M55 (DEC-0083): the ImGui VIEW/CONTROLLER for the in-window menu bar.
//
// Owns the Dear ImGui context lifecycle (create in ctor, shut down in dtor),
// translates the SDL-free MenuModel (menu_model.h) into
// ImGui::BeginMainMenuBar / MenuItem draw calls, and routes each clicked
// MenuAction back into Sdl3App through its narrow public seams. Lives ONLY in
// the SDL3 frontend library and is only ever constructed for a genuinely
// interactive launch (Sdl3App::init() gates it on !hidden_window), so no ImGui
// context is ever created under ctest / --hidden-window -- the determinism
// guarantee (DEC-0083). This header keeps ImGui OUT of its public surface (only
// the .cpp includes imgui.h) so sdl3_app.h need not see ImGui.

namespace sony_msx::frontend {

class Sdl3App;  // forward-decl: the .cpp includes the full sdl3_app.h

class Sdl3Menu {
public:
    // Runs IMGUI_CHECKVERSION -> CreateContext -> ImGui_ImplSDL3_InitForSDLRenderer
    // -> ImGui_ImplSDLRenderer3_Init. Keyboard navigation is left OFF so ImGui
    // never binds Alt / never sets WantCaptureKeyboard except inside a focused
    // text widget (v1 has none) -- the host Alt+letter hotkeys stay unshadowed.
    Sdl3Menu(SDL_Window* window, SDL_Renderer* renderer);
    // ImGui_ImplSDLRenderer3_Shutdown -> ImGui_ImplSDL3_Shutdown -> DestroyContext.
    // Sdl3App::shutdown() resets the owning unique_ptr BEFORE SDL_DestroyRenderer
    // (R7 ordering).
    ~Sdl3Menu();

    Sdl3Menu(const Sdl3Menu&) = delete;
    Sdl3Menu& operator=(const Sdl3Menu&) = delete;

    // Feed one polled SDL_Event to ImGui (ImGui_ImplSDL3_ProcessEvent). Must run
    // for EVERY event so hover/click/scroll accumulate. Called before the host
    // hotkey block in poll_and_dispatch_events().
    void process_event(const SDL_Event& event);

    // ImGui::GetIO().WantCaptureKeyboard / WantCaptureMouse. In v1 (no text
    // widgets, keyboard-nav off) wants_keyboard() is effectively always false, so
    // Alt+letter host hotkeys keep reaching their handlers; the gate is wired
    // anyway so a future filename field is already correct.
    [[nodiscard]] bool wants_keyboard() const;
    [[nodiscard]] bool wants_mouse() const;

    // Per-frame, called between Sdl3VideoPresenter::blit_frame() and present():
    //   begin_frame() -> the three NewFrame calls,
    //   build()       -> read Sdl3App state, build the model, draw the menu bar +
    //                    Help windows, and dispatch any click this frame,
    //   render()      -> ImGui::Render + RenderDrawData, bracketed by a
    //                    save/disable/restore of the renderer's logical
    //                    presentation so the menu draws in 1:1 window pixels
    //                    (never inside the 320x240 letterbox space).
    void begin_frame();
    void build(Sdl3App& app);
    void render(SDL_Renderer* renderer);

private:
    void render_menu_bar(Sdl3App& app);
    void render_help_windows();
    void dispatch(MenuAction action, int param, Sdl3App& app);

    bool show_hotkeys_window_ = false;
    bool show_about_window_ = false;
};

}  // namespace sony_msx::frontend
