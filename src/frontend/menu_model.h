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

#include <cstddef>
#include <string>
#include <vector>

// M55 (DEC-0083): the SDL-free / ImGui-free menu MODEL.
//
// This translation unit is compiled into `sony_msx_core` (NOT the SDL3 frontend
// library) precisely because it depends on NOTHING platform-specific: it turns a
// plain snapshot of runtime app state (`MenuState`) into a per-item descriptor
// tree (`MenuModel`) with each item's label, single-sourced hotkey label,
// enabled/grayed flag, and checkmark/radio state. The ImGui view
// (`src/frontend/sdl3_menu.*`) walks this tree to draw `BeginMainMenuBar` /
// `MenuItem` calls and to dispatch clicks back into `Sdl3App`. Because the model
// is pure data it is unit-testable OUTSIDE the SONY_MSX_ENABLE_SDL3 guard,
// mirroring `session_summary` / `config_runtime` / `master_volume`.
//
// The complete v1 tree matches the owner-approved DEC-0083 layout:
//   File / Machine / Video / Audio / Disk / Help.
// The five genuine new-capability items -- Open Cartridge (slot 1 / slot 2),
// Open Disk, Eject, Reset, New Blank Disk (the "star" set) -- render GRAYED
// (visible, honestly disabled) in v1.

namespace sony_msx::frontend {

// Every actionable / stateful menu LEAF. `None` marks a structural node (a
// submenu parent, a separator, or a disabled info label with no dispatch).
enum class MenuAction {
    None = 0,

    // --- File ---
    OpenCartridgeSlot1,   // star: grayed in v1 (follow-up F2)
    OpenCartridgeSlot2,   // star: grayed in v1 (follow-up F2)
    OpenDisk,             // star: grayed in v1 (follow-up F1)
    SwapDisk,             // F11 (enabled iff disk_count > 1)
    Eject,                // star: grayed in v1 (follow-up F3)
    Exit,

    // --- Machine ---
    Pause,                // PAUSE (checkmark = paused)
    Reset,                // star: grayed in v1 (follow-up F4)
    SetSpeed,             // radio, param = 0..7 (F6/F7)
    SetRensha,            // radio, param = 0..100 step 10 (F8/F9)
    SetRam,               // info radio, param = KB; always grayed ("restart to change")

    // --- Video ---
    ToggleFullscreen,     // Alt+Enter
    SetScale,             // radio, param = 1..8
    SetFilterLinear,      // radio member
    SetFilterNearest,     // radio member
    ToggleBorder,         // v1: grayed ("(startup)") -- no runtime setter exists
    PersistenceUp,        // Alt+B
    PersistenceDown,      // Shift+Alt+B
    TogglePersistenceMode,// Alt+M

    // --- Audio ---
    VolumeUp,             // Alt+U
    VolumeDown,           // Alt+D
    ToggleMute,           // checkmark = volume == 0

    // --- Disk ---
    ToggleFastDisk,       // Alt+F
    ToggleDiskWritable,   // Alt+S
    NewBlankDisk,         // star: grayed in v1 (follow-up F5)

    // --- Help ---
    HelpHotkeys,
    HelpAbout,
};

// A plain snapshot of the runtime state the menu reflects. The ImGui view
// populates it each frame by reading public Sdl3App/machine/presenter accessors;
// the model NEVER touches SDL/ImGui or emulation objects directly.
struct MenuState {
    // File
    std::size_t disk_count = 0;      // Swap Disk enabled iff > 1
    // Machine
    bool paused = false;
    int speed_level = 0;             // 0..7
    int rensha_speed = 0;            // 0..100 (grid of 10)
    std::size_t dram_kb = 64;        // 64/128/256/512
    // Video
    bool fullscreen = false;
    int scale = 3;                   // current window scale N (1..8)
    bool filter_nearest = false;     // false => Linear
    bool border_enabled = false;
    bool border_runtime_settable = false;  // v1 = false => Border grayed "(startup)"
    int persistence = 0;             // 0..100
    bool persistence_peak = false;   // false => Average
    // Audio
    int master_volume = 100;         // 0..100 (0 => Mute checked)
    // Disk
    bool fast_disk = false;
    bool disk_writable = false;
};

// One rendered menu leaf/branch. A non-empty `children` makes this a submenu; a
// `separator` renders a divider (label/action ignored); otherwise it is a
// clickable/toggle/radio item. `enabled == false` => grayed. `checked` drives
// the checkmark (toggles) or radio bullet (radio groups).
struct MenuItem {
    MenuAction action = MenuAction::None;
    int param = 0;                   // radio parameter (speed level, scale N, ...)
    std::string label;
    std::string hotkey_label;        // single-sourced (see hotkey_label_for); "" if none
    bool enabled = true;
    bool checked = false;
    bool separator = false;
    std::vector<MenuItem> children;
};

// One top-level menu ("File", "Machine", ...).
struct Menu {
    std::string label;
    std::vector<MenuItem> items;
};

// The complete menu bar.
struct MenuModel {
    std::vector<Menu> menus;
};

// One canonical in-window hotkey. This is the SINGLE SOURCE of every hotkey
// string: the menu builder reads it via hotkey_label_for(), and the Help >
// Hotkeys window + (optionally) the usage/banner render from the same table --
// so a hotkey label can never drift between the menu and the docs.
struct HotkeyEntry {
    MenuAction action;               // None for host hotkeys with no menu item (F10/F12)
    std::string description;         // human label for the Help window
    std::string hotkey;              // e.g. "F11", "PAUSE", "Alt+Enter", "F6 / F7"
};

// The canonical host-hotkey table (single source, §3.4).
const std::vector<HotkeyEntry>& hotkey_table();

// The canonical hotkey label for a menu action, or "" if the action has no
// hotkey. Sourced from hotkey_table() so the menu and the Help window agree.
std::string hotkey_label_for(MenuAction action);

// Build the complete v1 menu tree from a state snapshot (pure function).
MenuModel build_menu_model(const MenuState& state);

// The pure event-capture predicate (§4.1 / §5.2 test 3). Returns true when a
// polled event must be SWALLOWED by the menu -- i.e. NOT forwarded to the host
// hotkeys, the DEC-0072 recorder, or the MSX keyboard matrix: a key event while
// the menu owns the keyboard, or a mouse event while the menu owns the mouse.
// SDL/ImGui-free (takes plain bools) so it is unit-testable without a context.
constexpr bool menu_captures_event(bool is_key_event, bool is_mouse_event, bool wants_keyboard,
                                   bool wants_mouse) {
    return (is_key_event && wants_keyboard) || (is_mouse_event && wants_mouse);
}

// The exact set of star (new-capability) actions that are ALWAYS grayed in v1,
// regardless of state -- exposed so the model unit test can assert the invariant
// without hand-listing it in two places.
const std::vector<MenuAction>& v1_grayed_star_actions();

}  // namespace sony_msx::frontend
