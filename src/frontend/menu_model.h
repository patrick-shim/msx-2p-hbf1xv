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

// The SDL-free / ImGui-free menu MODEL. (DEC-0083)
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
// The complete tree matches the owner-approved layout (DEC-0083):
//   File / Machine / Video / Audio / Disk / Help.
// The runtime-media set -- Open Cartridge (slot 1 / slot
// 2), Open Disk(s), Eject (Disk / Cartridge Slot 1 / Slot 2), Reset, New Blank
// Disk -- carries per-state enablement (Eject Disk iff a disk is mounted, Eject
// Cartridge iff that slot is occupied, etc.; DEC-0084). RAM is a LIVE
// power-cycle radio (DEC-0085-AMENDMENT-A) and Machine > BIOS Folder... is a
// runtime selector (DEC-0089), so only Border (no runtime setter) stays grayed.

namespace sony_msx::frontend {

// Every actionable / stateful menu LEAF. `None` marks a structural node (a
// submenu parent, a separator, or a disabled info label with no dispatch).
enum class MenuAction {
    None = 0,

    // --- File ---
    OpenCartridgeSlot1,   // runtime .rom insert into slot 1 (implies reset)
    OpenCartridgeSlot2,   // runtime .rom insert into slot 2 (implies reset)
    OpenDisk,             // multi-select .dsk open (REPLACE the F11 cycle)
    SwapDisk,             // F11 (enabled iff disk_count > 1)
    EjectDisk,            // flush-if-writable + empty the drive (enabled iff disk_count > 0)
    EjectCartridgeSlot1,  // unload slot 1 + reset (enabled iff slot1_loaded)
    EjectCartridgeSlot2,  // unload slot 2 + reset (enabled iff slot2_loaded)
    OpenRecent,           // File > Recent > <path>; param = index into MenuState::recent (DEC-0095)
    Exit,

    // --- Machine ---
    Pause,                // PAUSE (checkmark = paused)
    Reset,                // reset_machine() (disks + carts persist)
    SetSpeed,             // radio, param = 0..7 (F6/F7)
    SetRensha,            // radio, param = 0..100 step 10 (F8/F9)
    SetRam,               // radio, param = KB; LIVE (power-cycles to the new size)
    OpenBiosFolder,       // folder dialog; validated selection power-cycles (DEC-0089)

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
    NewBlankDisk,         // write a fresh blank 720 KB MSX-DOS .dsk

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
    bool slot1_loaded = false;       // Eject Cartridge Slot 1 enabled iff true
    bool slot2_loaded = false;       // Eject Cartridge Slot 2 enabled iff true
    // The File > Recent MRU paths (most-recent first). Empty => the
    // Recent submenu renders a single disabled "(none)" item. Each entry becomes
    // an OpenRecent child whose param is its index here. (DEC-0095)
    std::vector<std::string> recent;
    // Machine
    bool paused = false;
    int speed_level = 0;             // 0..7
    int rensha_speed = 0;            // 0..100 (grid of 10)
    std::size_t dram_kb = 64;        // 64/128/256/512
    // The CURRENT BIOS directory (Sdl3AppConfig::bios_dir); the
    // "BIOS Folder..." item surfaces its basename so the active BIOS set is
    // always visible in the menu. Default mirrors Sdl3AppConfig's "bios". (DEC-0089)
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
    // Extra read-only status the bottom status bar surfaces (all
    // presentation-only; populated by snapshot() from public accessors). (DEC-0096)
    bool fdd_motor = false;             // drive motor on (the activity LED)
    std::string disk_name;              // mounted disk basename ("" = no disk)
    int fdd_track = 0;                  // physical head cylinder
    bool disk_write_protected = false;  // mounted disk is write-protected
    std::string slot1_name;             // slot-1 cartridge basename ("" = empty)
    std::string slot2_name;             // slot-2 cartridge basename ("" = empty)
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

// The canonical host-hotkey table (single source).
const std::vector<HotkeyEntry>& hotkey_table();

// The canonical hotkey label for a menu action, or "" if the action has no
// hotkey. Sourced from hotkey_table() so the menu and the Help window agree.
std::string hotkey_label_for(MenuAction action);

// Build the complete v1 menu tree from a state snapshot (pure function).
MenuModel build_menu_model(const MenuState& state);

// The [0,1] opacity of the FDD-activity LED overlay (a translucent green circle
// the SDL3 frontend draws in the upper-right corner). Returns 0 when the drive is
// idle (LED fully hidden) and a gentle ~2.5 Hz pulse in [0.40, 0.90] while the
// floppy motor is on -- driven by the REAL hardware motor line
// (DiskDrive::motor_on, which keeps spinning ~4 s after each access, so even a
// brief fast-disk read shows). Pure + SDL/ImGui-free so the pulse curve is
// unit-testable; sdl3_menu applies it to the ImGui draw. `t` = seconds (monotonic).
[[nodiscard]] float fdd_led_alpha(bool motor_on, double t);

// The bottom status-bar TEXT (the FDD activity LED is drawn separately
// as a colored dot before it). Pure function of the snapshot so the exact layout
// is unit-testable; sdl3_menu renders the returned string. Format (segments
// joined by "  |  "): FDD <disk|no disk> [trk<n>] [WP]  |  S1 <name|->  |
// S2 <name|->  |  RAM <kb>K  [ |  SPD <n>]  [ |  FAST]  |  <WR|RO>. (DEC-0096)
[[nodiscard]] std::string format_status_bar(const MenuState& state);

// The pure event-capture predicate. Returns true when a
// polled event must be SWALLOWED by the menu -- i.e. NOT forwarded to the host
// hotkeys, the input recorder (DEC-0072), or the MSX keyboard matrix: a key event while
// the menu owns the keyboard, or a mouse event while the menu owns the mouse.
// SDL/ImGui-free (takes plain bools) so it is unit-testable without a context.
constexpr bool menu_captures_event(bool is_key_event, bool is_mouse_event, bool wants_keyboard,
                                   bool wants_mouse) {
    return (is_key_event && wants_keyboard) || (is_mouse_event && wants_mouse);
}

// The exact set of actions that are ALWAYS grayed regardless of state -- after
// the runtime-media enablement pass, only RAM (info-only, no runtime resize)
// and Border (no runtime setter) (DEC-0084). Exposed so the model unit test can
// assert the invariant without hand-listing it in two places. (Name kept for
// call-site stability.)
const std::vector<MenuAction>& v1_grayed_star_actions();

}  // namespace sony_msx::frontend
