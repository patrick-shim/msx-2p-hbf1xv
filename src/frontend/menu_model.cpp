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

#include "frontend/menu_model.h"

namespace sony_msx::frontend {

namespace {

// A tiny builder helper: a normal clickable/toggle item.
MenuItem item(const MenuAction action, std::string label, const bool enabled = true,
              const bool checked = false) {
    MenuItem it;
    it.action = action;
    it.label = std::move(label);
    it.hotkey_label = hotkey_label_for(action);
    it.enabled = enabled;
    it.checked = checked;
    return it;
}

MenuItem separator() {
    MenuItem it;
    it.separator = true;
    return it;
}

// A radio member: same action, distinct param, checked when param == current.
MenuItem radio(const MenuAction action, const int param, std::string label, const int current,
               const bool enabled = true) {
    MenuItem it;
    it.action = action;
    it.param = param;
    it.label = std::move(label);
    it.enabled = enabled;
    it.checked = (param == current);
    return it;
}

}  // namespace

const std::vector<HotkeyEntry>& hotkey_table() {
    // The single source of truth for every in-window host hotkey. Order is the
    // display order for the Help > Hotkeys window. F10 (live capture) and F12
    // (debug snapshot) are host hotkeys with NO menu item -- action == None.
    static const std::vector<HotkeyEntry> kTable = {
        {MenuAction::Pause, "Hardware PAUSE button", "PAUSE"},
        {MenuAction::SwapDisk, "Swap to the next disk", "F11"},
        {MenuAction::SetSpeed, "Speed Controller slow -/+", "F6 / F7"},
        {MenuAction::SetRensha, "Ren-Sha auto-fire -/+", "F8 / F9"},
        {MenuAction::ToggleFullscreen, "Toggle fullscreen", "Alt+Enter"},
        {MenuAction::ToggleFastDisk, "Toggle fast-disk (FDC turbo)", "Alt+F"},
        {MenuAction::VolumeDown, "Master volume down 10%", "Alt+D"},
        {MenuAction::VolumeUp, "Master volume up 10%", "Alt+U"},
        {MenuAction::ToggleDiskWritable, "Toggle disk-writable", "Alt+S"},
        {MenuAction::PersistenceUp, "Phosphor persistence +10%", "Alt+B"},
        {MenuAction::PersistenceDown, "Phosphor persistence -10%", "Shift+Alt+B"},
        {MenuAction::TogglePersistenceMode, "Toggle phosphor mode (avg/peak)", "Alt+M"},
        {MenuAction::None, "Write a debug snapshot", "F12"},
        {MenuAction::None, "Live capture (needs --capture on)", "F10"},
    };
    return kTable;
}

std::string hotkey_label_for(const MenuAction action) {
    if (action == MenuAction::None) {
        return std::string();
    }
    for (const HotkeyEntry& e : hotkey_table()) {
        if (e.action == action) {
            return e.hotkey;
        }
    }
    return std::string();
}

const std::vector<MenuAction>& v1_grayed_star_actions() {
    // M56 (DEC-0084) + M57 (DEC-0085-AMENDMENT-A): SetRam is now LIVE (Machine>RAM
    // triggers a power-cycle rebuild at the chosen size), so only ToggleBorder
    // remains unconditionally grayed (no runtime border setter exists).
    static const std::vector<MenuAction> kAlwaysGrayed = {
        MenuAction::ToggleBorder,
    };
    return kAlwaysGrayed;
}

MenuModel build_menu_model(const MenuState& state) {
    MenuModel model;

    // ----- File -----
    {
        Menu file;
        file.label = "File";
        // M56 (F2): runtime cartridge insert (implies reset) -- always available.
        MenuItem open_cart;
        open_cart.label = "Open Cartridge";
        open_cart.children.push_back(item(MenuAction::OpenCartridgeSlot1, "Slot 1..."));
        open_cart.children.push_back(item(MenuAction::OpenCartridgeSlot2, "Slot 2..."));
        file.items.push_back(std::move(open_cart));
        // M56 (F1): multi-select open (REPLACE the F11 cycle) -- always available.
        file.items.push_back(item(MenuAction::OpenDisk, "Open Disk(s)..."));
        file.items.push_back(
            item(MenuAction::SwapDisk, "Swap Disk", /*enabled=*/state.disk_count > 1));
        // M56 (F3): Eject submenu with per-state enablement.
        MenuItem eject;
        eject.label = "Eject";
        eject.children.push_back(
            item(MenuAction::EjectDisk, "Disk", /*enabled=*/state.disk_count > 0));
        eject.children.push_back(
            item(MenuAction::EjectCartridgeSlot1, "Cartridge Slot 1", /*enabled=*/state.slot1_loaded));
        eject.children.push_back(
            item(MenuAction::EjectCartridgeSlot2, "Cartridge Slot 2", /*enabled=*/state.slot2_loaded));
        file.items.push_back(std::move(eject));
        file.items.push_back(separator());
        file.items.push_back(item(MenuAction::Exit, "Exit"));
        model.menus.push_back(std::move(file));
    }

    // ----- Machine -----
    {
        Menu machine;
        machine.label = "Machine";
        machine.items.push_back(item(MenuAction::Pause, "Pause", true, state.paused));
        // M56 (F4): reset (mounted disks + inserted carts persist) -- always available.
        machine.items.push_back(item(MenuAction::Reset, "Reset"));
        machine.items.push_back(separator());

        MenuItem speed;
        // Submenu parents carry no ImGui shortcut column, so surface the hotkey
        // in the label -- single-sourced from the same table (anti-drift).
        speed.label = "Speed  (" + hotkey_label_for(MenuAction::SetSpeed) + ")";
        for (int n = 0; n <= 7; ++n) {
            const std::string label =
                (n == 0) ? "0 (full speed)" : std::to_string(n);
            speed.children.push_back(radio(MenuAction::SetSpeed, n, label, state.speed_level));
        }
        machine.items.push_back(std::move(speed));

        MenuItem rensha;
        rensha.label = "Ren-Sha Turbo  (" + hotkey_label_for(MenuAction::SetRensha) + ")";
        for (int n = 0; n <= 100; n += 10) {
            const std::string label = std::to_string(n) + "%";
            rensha.children.push_back(radio(MenuAction::SetRensha, n, label, state.rensha_speed));
        }
        machine.items.push_back(std::move(rensha));

        MenuItem ram;
        ram.label = "RAM (power cycle)";
        const int cur_kb = static_cast<int>(state.dram_kb);
        for (const int kb : {64, 128, 256, 512}) {
            // M57 (DEC-0085-AMENDMENT-A): LIVE radio -- the bullet shows the CURRENT
            // size (state.dram_kb) and selecting a different size power-cycles the
            // machine to it (rebuilds at the new RAM size, media surviving). The
            // dispatch guards on size-change so re-selecting the current size is
            // inert.
            ram.children.push_back(radio(MenuAction::SetRam, kb, std::to_string(kb) + " KB", cur_kb));
        }
        machine.items.push_back(std::move(ram));
        model.menus.push_back(std::move(machine));
    }

    // ----- Video -----
    {
        Menu video;
        video.label = "Video";
        video.items.push_back(
            item(MenuAction::ToggleFullscreen, "Fullscreen", true, state.fullscreen));

        MenuItem scale;
        scale.label = "Scale";
        for (int n = 1; n <= 8; ++n) {
            scale.children.push_back(radio(MenuAction::SetScale, n,
                                           std::to_string(n) + "x  (" + std::to_string(320 * n) +
                                               "x" + std::to_string(240 * n) + ")",
                                           state.scale));
        }
        video.items.push_back(std::move(scale));

        MenuItem filter;
        filter.label = "Filter";
        {
            MenuItem linear = item(MenuAction::SetFilterLinear, "Linear (smooth)", true,
                                   !state.filter_nearest);
            MenuItem nearest = item(MenuAction::SetFilterNearest, "Nearest (sharp)", true,
                                    state.filter_nearest);
            filter.children.push_back(std::move(linear));
            filter.children.push_back(std::move(nearest));
        }
        video.items.push_back(std::move(filter));

        // Border: no runtime setter exists in v1 (config-time-only), so it is
        // rendered GRAYED with a "(startup)" note -- honest, no new machinery.
        {
            MenuItem border = item(MenuAction::ToggleBorder,
                                   state.border_runtime_settable ? "Border" : "Border (startup)",
                                   /*enabled=*/state.border_runtime_settable, state.border_enabled);
            video.items.push_back(std::move(border));
        }
        video.items.push_back(separator());

        MenuItem persistence;
        persistence.label = "Persistence  (" + std::to_string(state.persistence) + "%)";
        persistence.children.push_back(item(MenuAction::PersistenceUp, "Increase +10%"));
        persistence.children.push_back(item(MenuAction::PersistenceDown, "Decrease -10%"));
        video.items.push_back(std::move(persistence));

        MenuItem pmode;
        pmode.label = "Persistence Mode";
        // Two-item radio over the toggle handler: param 0 = Average, 1 = Peak.
        // The view only fires the toggle when the clicked mode differs from the
        // current one (clicking the already-selected mode is a no-op), so the
        // Avg/Peak radio stays correct even though the underlying seam toggles.
        MenuItem avg = item(MenuAction::TogglePersistenceMode, "Average", true,
                            !state.persistence_peak);
        avg.param = 0;
        MenuItem peak = item(MenuAction::TogglePersistenceMode, "Peak", true,
                             state.persistence_peak);
        peak.param = 1;
        pmode.children.push_back(std::move(avg));
        pmode.children.push_back(std::move(peak));
        video.items.push_back(std::move(pmode));
        model.menus.push_back(std::move(video));
    }

    // ----- Audio -----
    {
        Menu audio;
        audio.label = "Audio";
        MenuItem volume;
        volume.label = "Volume  (" + std::to_string(state.master_volume) + "%)";
        volume.children.push_back(item(MenuAction::VolumeUp, "Up +10%"));
        volume.children.push_back(item(MenuAction::VolumeDown, "Down -10%"));
        audio.items.push_back(std::move(volume));
        audio.items.push_back(
            item(MenuAction::ToggleMute, "Mute", true, state.master_volume == 0));
        model.menus.push_back(std::move(audio));
    }

    // ----- Disk -----
    {
        Menu disk;
        disk.label = "Disk";
        disk.items.push_back(item(MenuAction::ToggleFastDisk, "Fast Disk", true, state.fast_disk));
        disk.items.push_back(
            item(MenuAction::ToggleDiskWritable, "Disk Writable", true, state.disk_writable));
        disk.items.push_back(separator());
        // M56 (F5): write a fresh blank 720 KB MSX-DOS disk -- always available.
        disk.items.push_back(item(MenuAction::NewBlankDisk, "New Blank Disk..."));
        model.menus.push_back(std::move(disk));
    }

    // ----- Help -----
    {
        Menu help;
        help.label = "Help";
        help.items.push_back(item(MenuAction::HelpHotkeys, "Hotkeys..."));
        help.items.push_back(item(MenuAction::HelpAbout, "About..."));
        model.menus.push_back(std::move(help));
    }

    return model;
}

}  // namespace sony_msx::frontend
