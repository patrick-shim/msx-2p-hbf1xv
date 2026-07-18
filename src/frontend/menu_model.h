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
// The complete tree matches the owner-approved DEC-0083 layout:
//   File / Machine / Video / Audio / Disk / Help.
// M56 (DEC-0084) enabled the former "star" set -- Open Cartridge (slot 1 / slot
// 2), Open Disk(s), Eject (Disk / Cartridge Slot 1 / Slot 2), Reset, New Blank
// Disk -- with per-state enablement (Eject Disk iff a disk is mounted, Eject
// Cartridge iff that slot is occupied, etc.). M57 (DEC-0085-AMENDMENT-A) made
// RAM a LIVE power-cycle radio and M60 (DEC-0089) added the Machine > BIOS
// Folder... selector, so only Border (no runtime setter) stays grayed.

namespace sony_msx::frontend {

// Every actionable / stateful menu LEAF. `None` marks a structural node (a
// submenu parent, a separator, or a disabled info label with no dispatch).
enum class MenuAction {
    None = 0,

    // --- File ---
    OpenCartridgeSlot1,   // M56 (F2): runtime .rom insert into slot 1 (implies reset)
    OpenCartridgeSlot2,   // M56 (F2): runtime .rom insert into slot 2 (implies reset)
    OpenDisk,             // M56 (F1): multi-select .dsk open (REPLACE the F11 cycle)
    SwapDisk,             // F11 (enabled iff disk_count > 1)
    EjectDisk,            // M56 (F3): flush-if-writable + empty the drive (enabled iff disk_count > 0)
    EjectCartridgeSlot1,  // M56 (F3): unload slot 1 + reset (enabled iff slot1_loaded)
    EjectCartridgeSlot2,  // M56 (F3): unload slot 2 + reset (enabled iff slot2_loaded)
    Exit,

    // --- Machine ---
    Pause,                // PAUSE (checkmark = paused)
    Reset,                // M56 (F4): reset_machine() (disks + carts persist)
    SetSpeed,             // radio, param = 0..7 (F6/F7)
    SetRensha,            // radio, param = 0..100 step 10 (F8/F9)
    SetRam,               // radio, param = KB; LIVE since M57 (power-cycles to the new size)
    OpenBiosFolder,       // M60 (DEC-0089): folder dialog; validated selection power-cycles

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
    NewBlankDisk,         // M56 (F5): write a fresh blank 720 KB MSX-DOS .dsk

    // --- Help ---
    HelpHotkeys,
    HelpAbout,
};

// A plain snapshot of the runtime state the menu reflects. The ImGui view
// populates it each frame by reading public Sdl3App/machine/presenter accessors;
// the model NEVER touches SDL/ImGui or emulation objects directly.
struct MenuState {
    // File
    std::size_t disk_count = 0;      // Swap Disk enabled iff > 1; Eject Disk iff > 0
    bool slot1_loaded = false;       // M56: Eject Cartridge Slot 1 enabled iff true
    bool slot2_loaded = false;       // M56: Eject Cartridge Slot 2 enabled iff true
    // Machine
    bool paused = false;
    int speed_level = 0;             // 0..7
    int rensha_speed = 0;            // 0..100 (grid of 10)
    std::size_t dram_kb = 64;        // 64/128/256/512
    // M60 (DEC-0089): the CURRENT BIOS directory (Sdl3AppConfig::bios_dir); the
    // "BIOS Folder..." item surfaces its basename so the active BIOS set is
    // always visible in the menu. Default mirrors Sdl3AppConfig's "bios".
    std::string bios_dir = "bios";
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

// The exact set of actions that are ALWAYS grayed regardless of state -- after
// M56 (DEC-0084) only RAM (info-only, no runtime resize) and Border (no runtime
// setter). Exposed so the model unit test can assert the invariant without
// hand-listing it in two places. (Name kept for call-site stability.)
const std::vector<MenuAction>& v1_grayed_star_actions();

}  // namespace sony_msx::frontend
