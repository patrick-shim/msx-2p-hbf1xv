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

// Suite: Frontend_MenuModel_Unit (M55, DEC-0083).
//
// The SDL-free / ImGui-free menu-model logic proven headlessly (links
// sony_msx_core, registers OUTSIDE the SONY_MSX_ENABLE_SDL3 guard -- the
// established SDL-free-frontend-logic pattern). Covers: (1) the complete v1 tree
// vs the DEC-0083 layout, (2) the exact grayed "star" set is ALWAYS disabled,
// (3) per-item checkmark / radio bindings track the state snapshot, (4) Swap
// Disk enablement vs disk_count, (5) the single-sourced hotkey-label table is
// internally consistent (no drift, no duplicate hotkey), (6) the pure
// event-capture predicate.

#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "frontend/menu_model.h"

namespace {

using namespace sony_msx::frontend;

int g_failures = 0;

void expect(const bool ok, const char* case_name) {
    if (!ok) {
        std::cerr << "Case failed: " << case_name << "\n";
        ++g_failures;
    }
}

// Flatten every LEAF item (recursively into submenus), skipping separators and
// submenu parents, into a flat pointer list for lookups.
void collect_leaves(const std::vector<MenuItem>& items, std::vector<const MenuItem*>& out) {
    for (const MenuItem& it : items) {
        if (it.separator) {
            continue;
        }
        if (!it.children.empty()) {
            collect_leaves(it.children, out);
            continue;
        }
        out.push_back(&it);
    }
}

std::vector<const MenuItem*> leaves(const MenuModel& model) {
    std::vector<const MenuItem*> out;
    for (const Menu& m : model.menus) {
        collect_leaves(m.items, out);
    }
    return out;
}

// First leaf matching action (and param when param >= 0).
const MenuItem* find(const MenuModel& model, const MenuAction action, const int param = -1) {
    for (const MenuItem* it : leaves(model)) {
        if (it->action == action && (param < 0 || it->param == param)) {
            return it;
        }
    }
    return nullptr;
}

// First leaf whose label matches exactly.
const MenuItem* find_label(const MenuModel& model, const std::string& label) {
    for (const MenuItem* it : leaves(model)) {
        if (it->label == label) {
            return it;
        }
    }
    return nullptr;
}

MenuState make_state() {
    // A deliberately non-default snapshot so a binding that ignores state fails.
    MenuState s;
    s.disk_count = 3;
    s.paused = true;
    s.speed_level = 5;
    s.rensha_speed = 40;
    s.dram_kb = 256;
    s.fullscreen = true;
    s.scale = 4;
    s.filter_nearest = true;
    s.border_enabled = true;
    s.border_runtime_settable = false;
    s.persistence = 30;
    s.persistence_peak = true;
    s.master_volume = 0;   // muted
    s.fast_disk = true;
    s.disk_writable = true;
    s.slot1_loaded = true;   // M56: for the Eject Cartridge enablement matrix
    s.slot2_loaded = false;
    s.bios_dir = "C:\\assets\\mybios";  // M60: non-default dir for the label binding
    return s;
}

void test_tree_completeness() {
    const MenuModel model = build_menu_model(MenuState{});
    expect(model.menus.size() == 6, "Tree_HasSixTopMenus");
    const std::vector<std::string> expected_labels = {"File", "Machine", "Video",
                                                      "Audio", "Disk", "Help"};
    bool labels_ok = model.menus.size() == expected_labels.size();
    for (std::size_t i = 0; labels_ok && i < expected_labels.size(); ++i) {
        labels_ok = model.menus[i].label == expected_labels[i];
    }
    expect(labels_ok, "Tree_TopMenuLabels_MatchDEC0083Order");

    // Every leaf action is present (M56 expanded Eject into Disk/Slot1/Slot2).
    const std::vector<MenuAction> required = {
        MenuAction::OpenCartridgeSlot1, MenuAction::OpenCartridgeSlot2, MenuAction::OpenDisk,
        MenuAction::SwapDisk,           MenuAction::EjectDisk,          MenuAction::EjectCartridgeSlot1,
        MenuAction::EjectCartridgeSlot2, MenuAction::Exit,
        MenuAction::Pause,              MenuAction::Reset,              MenuAction::SetSpeed,
        MenuAction::SetRensha,          MenuAction::SetRam,             MenuAction::OpenBiosFolder,
        MenuAction::ToggleFullscreen,
        MenuAction::SetScale,           MenuAction::SetFilterLinear,    MenuAction::SetFilterNearest,
        MenuAction::ToggleBorder,       MenuAction::PersistenceUp,      MenuAction::PersistenceDown,
        MenuAction::TogglePersistenceMode, MenuAction::VolumeUp,        MenuAction::VolumeDown,
        MenuAction::ToggleMute,         MenuAction::ToggleFastDisk,     MenuAction::ToggleDiskWritable,
        MenuAction::NewBlankDisk,       MenuAction::HelpHotkeys,        MenuAction::HelpAbout,
    };
    bool all_present = true;
    for (const MenuAction a : required) {
        if (find(model, a) == nullptr) {
            all_present = false;
            std::cerr << "  missing action index " << static_cast<int>(a) << "\n";
        }
    }
    expect(all_present, "Tree_EveryDEC0083Action_Present");

    // Radio group cardinalities.
    int speed_n = 0;
    int scale_n = 0;
    int rensha_n = 0;
    int ram_n = 0;
    for (const MenuItem* it : leaves(model)) {
        switch (it->action) {
            case MenuAction::SetSpeed: ++speed_n; break;
            case MenuAction::SetScale: ++scale_n; break;
            case MenuAction::SetRensha: ++rensha_n; break;
            case MenuAction::SetRam: ++ram_n; break;
            default: break;
        }
    }
    expect(speed_n == 8, "Tree_SpeedRadio_HasEightMembers");
    expect(scale_n == 8, "Tree_ScaleRadio_HasEightMembers");
    expect(rensha_n == 11, "Tree_RenshaRadio_HasElevenMembers");
    expect(ram_n == 4, "Tree_RamRadio_HasFourMembers");
}

void test_grayed_star_set() {
    // M56 (DEC-0084) + M57 (DEC-0085-AMENDMENT-A): SetRam is now LIVE (Machine>RAM
    // triggers a power-cycle), so ONLY ToggleBorder stays unconditionally grayed;
    // every F1-F5 item is enabled per state (see test_new_capability_enablement).
    for (const MenuState& s : {MenuState{}, make_state()}) {
        const MenuModel model = build_menu_model(s);
        for (const MenuAction a : v1_grayed_star_actions()) {
            const MenuItem* it = find(model, a);
            expect(it != nullptr && !it->enabled, "Grayed_AlwaysGrayedAction_Disabled");
        }
        // M57: RAM radio items are now ENABLED (live power-cycle).
        bool ram_all_enabled = true;
        int ram_leaves = 0;
        for (const MenuItem* it : leaves(model)) {
            if (it->action == MenuAction::SetRam) {
                ++ram_leaves;
                if (!it->enabled) {
                    ram_all_enabled = false;
                }
            }
        }
        expect(ram_leaves == 4 && ram_all_enabled, "Live_RamRadioItems_Enabled");
        // Border stays grayed (no runtime setter).
        const MenuItem* border = find(model, MenuAction::ToggleBorder);
        expect(border != nullptr && !border->enabled, "Grayed_Border_Disabled");
    }
    // The always-grayed set is now exactly {ToggleBorder}.
    expect(v1_grayed_star_actions().size() == 1, "Grayed_AlwaysGrayedSet_ExactlyOne");
    const std::set<MenuAction> grayed(v1_grayed_star_actions().begin(),
                                      v1_grayed_star_actions().end());
    expect(grayed.count(MenuAction::SetRam) == 0 && grayed.count(MenuAction::ToggleBorder) == 1,
           "Grayed_AlwaysGrayedSet_IsBorderOnly");
}

void test_new_capability_enablement() {
    // M56 (DEC-0084, planner §9 test 4): Open Disk / Open Cartridge Slot 1/2 /
    // Reset / New Blank Disk are UNCONDITIONALLY enabled; Eject Disk enabled iff
    // disk_count > 0; Eject Cartridge Slot N enabled iff slotN_loaded.
    for (const MenuState& s : {MenuState{}, make_state()}) {
        const MenuModel model = build_menu_model(s);
        for (const MenuAction a :
             {MenuAction::OpenDisk, MenuAction::OpenCartridgeSlot1, MenuAction::OpenCartridgeSlot2,
              MenuAction::Reset, MenuAction::NewBlankDisk}) {
            const MenuItem* it = find(model, a);
            expect(it != nullptr && it->enabled, "Enabled_F1toF5_AlwaysEnabled");
        }
    }

    // Eject Disk tracks disk_count > 0.
    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{3}}) {
        MenuState s;
        s.disk_count = count;
        const MenuModel model = build_menu_model(s);
        const MenuItem* ed = find(model, MenuAction::EjectDisk);
        expect(ed != nullptr && ed->enabled == (count > 0), "Enabled_EjectDisk_IffDiskMounted");
    }

    // Eject Cartridge Slot N tracks slotN_loaded independently.
    for (const bool s1 : {false, true}) {
        for (const bool s2 : {false, true}) {
            MenuState s;
            s.slot1_loaded = s1;
            s.slot2_loaded = s2;
            const MenuModel model = build_menu_model(s);
            const MenuItem* e1 = find(model, MenuAction::EjectCartridgeSlot1);
            const MenuItem* e2 = find(model, MenuAction::EjectCartridgeSlot2);
            expect(e1 != nullptr && e1->enabled == s1, "Enabled_EjectCartSlot1_IffSlot1Loaded");
            expect(e2 != nullptr && e2->enabled == s2, "Enabled_EjectCartSlot2_IffSlot2Loaded");
        }
    }
}

void test_bios_folder_item() {
    // M60 (DEC-0089): the Machine > "BIOS Folder..." item -- always ENABLED (NOT
    // part of the grayed set), lives under the Machine top menu, carries no
    // hotkey, and its label surfaces the CURRENT bios_dir basename.

    // Default state: bios_dir == "bios" (the Sdl3AppConfig default).
    {
        const MenuModel model = build_menu_model(MenuState{});
        const MenuItem* it = find(model, MenuAction::OpenBiosFolder);
        expect(it != nullptr && it->enabled, "BiosFolder_DefaultState_PresentAndEnabled");
        expect(it != nullptr && it->label == "BIOS Folder...  (bios)",
               "BiosFolder_DefaultLabel_ShowsBiosBasename");
        expect(it != nullptr && it->hotkey_label.empty(), "BiosFolder_NoHotkey");

        // It must live under the MACHINE top menu specifically (menus[1]).
        std::vector<const MenuItem*> machine_leaves;
        collect_leaves(model.menus[1].items, machine_leaves);
        const bool in_machine =
            std::any_of(machine_leaves.begin(), machine_leaves.end(),
                        [](const MenuItem* m) { return m->action == MenuAction::OpenBiosFolder; });
        expect(in_machine, "BiosFolder_Item_UnderMachineMenu");
    }

    // Non-default dir (Windows separators): label shows the basename only.
    {
        const MenuModel model = build_menu_model(make_state());
        const MenuItem* it = find(model, MenuAction::OpenBiosFolder);
        expect(it != nullptr && it->enabled, "BiosFolder_CustomState_Enabled");
        expect(it != nullptr && it->label == "BIOS Folder...  (mybios)",
               "BiosFolder_CustomLabel_WindowsPathBasename");
    }

    // Trailing separator tolerated; POSIX separators too.
    {
        MenuState s;
        s.bios_dir = "/opt/msx/bios2/";
        const MenuModel model = build_menu_model(s);
        const MenuItem* it = find(model, MenuAction::OpenBiosFolder);
        expect(it != nullptr && it->label == "BIOS Folder...  (bios2)",
               "BiosFolder_TrailingSlash_BasenameStillShown");
    }

    // Empty dir string degrades to the bare label (no empty "()" suffix).
    {
        MenuState s;
        s.bios_dir.clear();
        const MenuModel model = build_menu_model(s);
        const MenuItem* it = find(model, MenuAction::OpenBiosFolder);
        expect(it != nullptr && it->label == "BIOS Folder...",
               "BiosFolder_EmptyDir_BareLabel");
    }

    // The always-grayed set is UNCHANGED by M60: still exactly {ToggleBorder}.
    const std::set<MenuAction> grayed(v1_grayed_star_actions().begin(),
                                      v1_grayed_star_actions().end());
    expect(grayed.count(MenuAction::OpenBiosFolder) == 0,
           "BiosFolder_NotInAlwaysGrayedSet");
}

void test_swap_disk_enablement() {
    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{5}}) {
        MenuState s;
        s.disk_count = count;
        const MenuModel model = build_menu_model(s);
        const MenuItem* swap = find(model, MenuAction::SwapDisk);
        const bool expect_enabled = count > 1;
        expect(swap != nullptr && swap->enabled == expect_enabled,
               "SwapDisk_EnabledIffMoreThanOneDisk");
    }
}

void test_checkmark_bindings() {
    const MenuState s = make_state();
    const MenuModel model = build_menu_model(s);

    const MenuItem* pause = find(model, MenuAction::Pause);
    expect(pause != nullptr && pause->checked == s.paused, "Check_Pause_TracksPausedState");

    const MenuItem* fs = find(model, MenuAction::ToggleFullscreen);
    expect(fs != nullptr && fs->checked == s.fullscreen, "Check_Fullscreen_TracksState");

    const MenuItem* mute = find(model, MenuAction::ToggleMute);
    expect(mute != nullptr && mute->checked == (s.master_volume == 0),
           "Check_Mute_TracksZeroVolume");

    const MenuItem* fast = find(model, MenuAction::ToggleFastDisk);
    expect(fast != nullptr && fast->checked == s.fast_disk, "Check_FastDisk_TracksState");

    const MenuItem* writable = find(model, MenuAction::ToggleDiskWritable);
    expect(writable != nullptr && writable->checked == s.disk_writable,
           "Check_DiskWritable_TracksState");

    // Speed radio: exactly the item with param == speed_level is checked.
    for (int n = 0; n <= 7; ++n) {
        const MenuItem* it = find(model, MenuAction::SetSpeed, n);
        expect(it != nullptr && it->checked == (n == s.speed_level),
               "Check_SpeedRadio_OnlyCurrentChecked");
    }
    // Scale radio.
    for (int n = 1; n <= 8; ++n) {
        const MenuItem* it = find(model, MenuAction::SetScale, n);
        expect(it != nullptr && it->checked == (n == s.scale),
               "Check_ScaleRadio_OnlyCurrentChecked");
    }
    // Ren-Sha radio.
    const MenuItem* rensha_cur = find(model, MenuAction::SetRensha, s.rensha_speed);
    expect(rensha_cur != nullptr && rensha_cur->checked, "Check_RenshaRadio_CurrentChecked");

    // Filter: nearest is the active one in make_state().
    const MenuItem* linear = find(model, MenuAction::SetFilterLinear);
    const MenuItem* nearest = find(model, MenuAction::SetFilterNearest);
    expect(linear != nullptr && !linear->checked, "Check_FilterLinear_NotCheckedWhenNearest");
    expect(nearest != nullptr && nearest->checked, "Check_FilterNearest_CheckedWhenNearest");

    // Persistence mode: peak is active in make_state() (two items share the
    // action; distinguish by label).
    const MenuItem* avg = find_label(model, "Average");
    const MenuItem* peak = find_label(model, "Peak");
    expect(avg != nullptr && !avg->checked, "Check_PersistenceAvg_NotCheckedWhenPeak");
    expect(peak != nullptr && peak->checked, "Check_PersistencePeak_CheckedWhenPeak");

    // RAM info radio reflects the current size (256 KB in make_state()).
    const MenuItem* ram_cur = find(model, MenuAction::SetRam, 256);
    expect(ram_cur != nullptr && ram_cur->checked, "Check_RamInfo_CurrentSizeMarked");

    // Flip the state and confirm the opposite bindings.
    MenuState s2;  // all defaults: not paused, full speed, linear, average, unmuted
    const MenuModel m2 = build_menu_model(s2);
    expect(find(m2, MenuAction::Pause)->checked == false, "Check_Pause_UncheckedByDefault");
    expect(find_label(m2, "Average")->checked == true, "Check_PersistenceAvg_DefaultChecked");
    expect(find(m2, MenuAction::SetFilterLinear)->checked == true, "Check_FilterLinear_DefaultChecked");
    expect(find(m2, MenuAction::ToggleMute)->checked == false, "Check_Mute_UncheckedAt100");
}

void test_hotkey_table_consistency() {
    const std::vector<HotkeyEntry>& table = hotkey_table();
    expect(!table.empty(), "Hotkey_Table_NotEmpty");

    // No duplicate hotkey strings (anti-drift: every hotkey exactly once).
    std::set<std::string> seen;
    bool unique = true;
    for (const HotkeyEntry& e : table) {
        expect(!e.hotkey.empty(), "Hotkey_Entry_HasNonEmptyHotkey");
        if (!seen.insert(e.hotkey).second) {
            unique = false;
            std::cerr << "  duplicate hotkey: " << e.hotkey << "\n";
        }
    }
    expect(unique, "Hotkey_Table_NoDuplicateHotkeys");

    // hotkey_label_for(None) is empty.
    expect(hotkey_label_for(MenuAction::None).empty(), "Hotkey_LabelFor_None_Empty");

    // Every menu item that shows a hotkey label matches the single source
    // hotkey_label_for(action), AND that action appears in the table.
    const MenuModel model = build_menu_model(MenuState{});
    bool all_match = true;
    for (const MenuItem* it : leaves(model)) {
        if (it->hotkey_label.empty()) {
            continue;
        }
        const std::string canonical = hotkey_label_for(it->action);
        if (it->hotkey_label != canonical || canonical.empty()) {
            all_match = false;
            std::cerr << "  hotkey drift for action " << static_cast<int>(it->action) << ": '"
                      << it->hotkey_label << "' vs '" << canonical << "'\n";
        }
    }
    expect(all_match, "Hotkey_MenuLabels_SingleSourced");

    // The specific host hotkeys required by DEC-0083 are present in the table.
    const std::vector<std::string> required_keys = {"PAUSE",    "F11",   "F6 / F7", "F8 / F9",
                                                    "Alt+Enter", "Alt+F", "Alt+D",   "Alt+U",
                                                    "Alt+S",    "Alt+B", "Shift+Alt+B", "Alt+M",
                                                    "F12",      "F10"};
    for (const std::string& k : required_keys) {
        const bool present = std::any_of(table.begin(), table.end(),
                                         [&](const HotkeyEntry& e) { return e.hotkey == k; });
        expect(present, "Hotkey_Table_CoversRequiredKey");
        if (!present) {
            std::cerr << "  missing required hotkey: " << k << "\n";
        }
    }
}

void test_capture_gate_predicate() {
    // Key event + menu wants keyboard -> captured (swallowed).
    expect(menu_captures_event(true, false, true, false), "Gate_KeyEvent_WantsKeyboard_Captured");
    // Key event but menu does NOT want keyboard -> forwarded.
    expect(!menu_captures_event(true, false, false, false),
           "Gate_KeyEvent_NoWantKeyboard_Forwarded");
    // Mouse event + menu wants mouse -> captured.
    expect(menu_captures_event(false, true, false, true), "Gate_MouseEvent_WantsMouse_Captured");
    // Mouse event but menu does NOT want mouse -> forwarded.
    expect(!menu_captures_event(false, true, false, false),
           "Gate_MouseEvent_NoWantMouse_Forwarded");
    // A non-key/non-mouse event is never captured.
    expect(!menu_captures_event(false, false, true, true), "Gate_OtherEvent_NeverCaptured");
    // A key event never captured by the MOUSE-want alone.
    expect(!menu_captures_event(true, false, false, true), "Gate_KeyEvent_MouseWantOnly_Forwarded");
}

}  // namespace

int main() {
    test_tree_completeness();
    test_grayed_star_set();
    test_new_capability_enablement();
    test_bios_folder_item();
    test_swap_disk_enablement();
    test_checkmark_bindings();
    test_hotkey_table_consistency();
    test_capture_gate_predicate();

    if (g_failures != 0) {
        std::cerr << g_failures << " case(s) failed\n";
        return 1;
    }
    std::cout << "All Frontend_MenuModel_Unit cases passed\n";
    return 0;
}
