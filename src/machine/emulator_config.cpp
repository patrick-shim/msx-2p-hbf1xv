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

#include "machine/emulator_config.h"

#include <cctype>
#include <charconv>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>

#include "frontend/sdl3_cli.h"  // reuse parse_ram_kb / parse_speed_level
#include "machine/xml_tokenizer.h"

// NOTE (layering): emulator_config lives in src/machine/ and
// reuses the ALREADY-TESTED value
// validators parse_ram_kb() / parse_speed_level() so the {64,128,256,512}KB and
// [0..7] range policy has ONE source of truth (no duplicate range policy). Those
// two helpers are pure + SDL-free (see their contract in sdl3_cli.h); the
// include is confined to THIS .cpp (the header stays frontend-free) so the model
// remains a clean machine-layer type.

namespace sony_msx::machine {

namespace {

// ---- warning emitters ------------------------------------------------------

void warn_value(std::vector<std::string>& warnings, const std::string& key, const std::string& value,
                const std::string& reason, const std::string& def) {
    warnings.push_back("config: WARNING " + key + " = '" + value + "' invalid (" + reason +
                       "); using default " + def);
}

void warn_unknown(std::vector<std::string>& warnings, const std::string& key) {
    warnings.push_back("config: WARNING unknown key '" + key + "'; ignoring");
}

// Warn on any attribute of `tok` not in the element's known set (naming the
// offending `<elem_key>@<attr>` key). Known attributes are handled by the
// caller; this catches typos / unsupported knobs strictly.
void warn_unknown_attrs(std::vector<std::string>& warnings, const XmlToken& tok,
                        const std::string& elem_key, std::initializer_list<const char*> known) {
    for (const XmlAttribute& a : tok.attributes) {
        bool ok = false;
        for (const char* k : known) {
            if (a.name == k) {
                ok = true;
                break;
            }
        }
        if (!ok) {
            warn_unknown(warnings, elem_key + "@" + a.name);
        }
    }
}

// ---- typed value parsers ----------------------------------------------------

bool parse_bool_ci(const std::string& v, bool& out) {
    std::string low;
    low.reserve(v.size());
    for (const char c : v) {
        low.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (low == "true") {
        out = true;
        return true;
    }
    if (low == "false") {
        out = false;
        return true;
    }
    return false;
}

bool parse_int_strict(const std::string& v, int& out) {
    int value = 0;
    const char* begin = v.data();
    const char* end = v.data() + v.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end) {
        return false;
    }
    out = value;
    return true;
}

// Apply a bool-attribute knob: absent -> keep default (no warning); present +
// valid -> set; present + invalid -> keep default + per-key warning.
void apply_bool(const XmlToken& tok, const char* attr, const std::string& key, bool& field,
                std::vector<std::string>& warnings) {
    const std::string* v = tok.attribute(attr);
    if (v == nullptr) {
        return;
    }
    bool parsed = false;
    if (parse_bool_ci(*v, parsed)) {
        field = parsed;
        return;
    }
    warn_value(warnings, key, *v, "expected true|false", field ? "true" : "false");
}

// Dispatch one element (identified by its parent context + own name) into the
// config, validating each known attribute and warning per-key on any
// unknown element / unknown attribute / bad value. Never throws.
void apply_element(const std::string& parent, const XmlToken& tok, EmulatorConfig& cfg,
                   std::vector<std::string>& warnings) {
    const std::string& name = tok.name;

    // --- children of the root: the two section containers ---
    if (parent == "hbf1xv-config") {
        if (name == "defaults" || name == "machine") {
            return;  // known container -- its attributes (none) / children handled elsewhere
        }
        warn_unknown(warnings, "hbf1xv-config/" + name);
        return;
    }

    // --- <defaults> section ---
    if (parent == "defaults") {
        if (name == "fast-disk") {
            warn_unknown_attrs(warnings, tok, "defaults/fast-disk", {"enabled"});
            apply_bool(tok, "enabled", "defaults/fast-disk@enabled", cfg.fast_disk, warnings);
        } else if (name == "fmpac") {
            warn_unknown_attrs(warnings, tok, "defaults/fmpac", {"autoload", "slot"});
            apply_bool(tok, "autoload", "defaults/fmpac@autoload", cfg.fmpac_autoload, warnings);
            if (const std::string* v = tok.attribute("slot")) {
                int slot = 0;
                if (parse_int_strict(*v, slot) && (slot == 1 || slot == 2)) {
                    cfg.fmpac_slot = slot;
                } else {
                    warn_value(warnings, "defaults/fmpac@slot", *v, "expected 1|2", "2");
                }
            }
        } else if (name == "border") {
            warn_unknown_attrs(warnings, tok, "defaults/border", {"enabled"});
            apply_bool(tok, "enabled", "defaults/border@enabled", cfg.border_enabled, warnings);
        } else if (name == "disk-writable") {
            warn_unknown_attrs(warnings, tok, "defaults/disk-writable", {"enabled"});
            apply_bool(tok, "enabled", "defaults/disk-writable@enabled", cfg.disk_writable, warnings);
        } else if (name == "volume") {
            // Master gain percent [0,100]; out-of-range/bad keeps
            // the default 100 + a per-key warning (mirrors
            // defaults/persistence@percent). (DEC-0079)
            warn_unknown_attrs(warnings, tok, "defaults/volume", {"percent"});
            if (const std::string* v = tok.attribute("percent")) {
                int pct = 0;
                if (parse_int_strict(*v, pct) && pct >= 0 && pct <= 100) {
                    cfg.master_volume = pct;
                } else {
                    warn_value(warnings, "defaults/volume@percent", *v, "expected 0..100", "100");
                }
            }
        } else if (name == "speed") {
            warn_unknown_attrs(warnings, tok, "defaults/speed", {"level"});
            if (const std::string* v = tok.attribute("level")) {
                std::vector<std::string> throwaway;
                int level = 0;
                if (frontend::parse_speed_level(*v, level, throwaway, "config")) {
                    cfg.speed_level = level;
                } else {
                    warn_value(warnings, "defaults/speed@level", *v, "expected 0..7", "0");
                }
            }
        } else if (name == "video") {
            warn_unknown_attrs(warnings, tok, "defaults/video", {"scale", "filter"});
            if (const std::string* v = tok.attribute("scale")) {
                int scale = 0;
                if (parse_int_strict(*v, scale) && scale >= 1 && scale <= 8) {
                    cfg.video_scale = scale;
                } else {
                    warn_value(warnings, "defaults/video@scale", *v, "expected 1..8", "3");
                }
            }
            if (const std::string* v = tok.attribute("filter")) {
                if (*v == "nearest" || *v == "linear") {
                    cfg.video_filter = *v;
                } else {
                    warn_value(warnings, "defaults/video@filter", *v, "expected nearest|linear",
                               "linear");
                }
            }
        } else if (name == "persistence") {
            warn_unknown_attrs(warnings, tok, "defaults/persistence", {"percent", "mode"});
            if (const std::string* v = tok.attribute("percent")) {
                int pct = 0;
                if (parse_int_strict(*v, pct) && pct >= 0 && pct <= 100) {
                    cfg.persistence_percent = pct;
                } else {
                    warn_value(warnings, "defaults/persistence@percent", *v, "expected 0..100", "0");
                }
            }
            if (const std::string* v = tok.attribute("mode")) {
                if (*v == "avg" || *v == "peak") {
                    cfg.persistence_mode = *v;
                } else {
                    warn_value(warnings, "defaults/persistence@mode", *v, "expected avg|peak", "avg");
                }
            }
        } else if (name == "fullscreen") {
            warn_unknown_attrs(warnings, tok, "defaults/fullscreen", {"enabled"});
            apply_bool(tok, "enabled", "defaults/fullscreen@enabled", cfg.fullscreen, warnings);
        } else if (name == "capture") {
            warn_unknown_attrs(warnings, tok, "defaults/capture", {"enabled"});
            apply_bool(tok, "enabled", "defaults/capture@enabled", cfg.capture_enabled, warnings);
        } else {
            warn_unknown(warnings, "defaults/" + name);
        }
        return;
    }

    // --- <machine> section ---
    if (parent == "machine") {
        if (name == "ram") {
            warn_unknown_attrs(warnings, tok, "machine/ram", {"kb"});
            if (const std::string* v = tok.attribute("kb")) {
                std::vector<std::string> throwaway;
                int kb = 0;
                if (frontend::parse_ram_kb(*v, kb, throwaway, "config")) {
                    cfg.ram_kb = kb;
                } else {
                    warn_value(warnings, "machine/ram@kb", *v, "expected 64|128|256|512", "512");
                }
            }
        } else if (name == "vram") {
            warn_unknown_attrs(warnings, tok, "machine/vram", {"kb"});
            if (const std::string* v = tok.attribute("kb")) {
                int kb = 0;
                if (parse_int_strict(*v, kb) && kb == 128) {
                    cfg.vram_kb = kb;
                } else {
                    warn_value(warnings, "machine/vram@kb", *v, "not 128 (strict HB-F1XV spec)",
                               "128");
                }
            }
        } else if (name == "bios") {
            warn_unknown_attrs(warnings, tok, "machine/bios", {"dir"});
            if (const std::string* v = tok.attribute("dir")) {
                cfg.bios_dir = *v;
            }
        } else if (name == "fmpac") {
            warn_unknown_attrs(warnings, tok, "machine/fmpac", {"rom", "sram"});
            if (const std::string* v = tok.attribute("rom")) {
                cfg.fmpac_rom = *v;
            }
            if (const std::string* v = tok.attribute("sram")) {
                cfg.fmpac_sram = *v;
            }
        } else if (name == "cartridge") {
            // Open Cartridge (slot 1/2) dialog default directory.
            warn_unknown_attrs(warnings, tok, "machine/cartridge", {"dir"});
            if (const std::string* v = tok.attribute("dir")) {
                cfg.cartridge_dir = *v;
            }
        } else if (name == "disk") {
            // Open Disk(s) dialog default directory.
            warn_unknown_attrs(warnings, tok, "machine/disk", {"dir"});
            if (const std::string* v = tok.attribute("dir")) {
                cfg.disk_dir = *v;
            }
        } else if (name == "softwaredb") {
            warn_unknown_attrs(warnings, tok, "machine/softwaredb", {"path"});
            if (const std::string* v = tok.attribute("path")) {
                cfg.softwaredb_path = *v;
            }
        } else if (name == "slots") {
            // OPTIONAL/advanced; the whole <slots> subtree is accepted and
            // ignored (it never remaps here) -- no warning.
        } else {
            warn_unknown(warnings, "machine/" + name);
        }
        return;
    }

    // --- <bios> children: the 7 role-keyed <rom> filenames ---
    if (parent == "bios") {
        if (name == "rom") {
            warn_unknown_attrs(warnings, tok, "machine/bios/rom", {"role", "file"});
            const std::string* role = tok.attribute("role");
            const std::string* file = tok.attribute("file");
            if (role == nullptr) {
                warn_value(warnings, "machine/bios/rom@role", "", "role attribute required",
                           "(none)");
                return;
            }
            std::string* target = nullptr;
            if (*role == "bios") {
                target = &cfg.bios_roms.bios;
            } else if (*role == "sub") {
                target = &cfg.bios_roms.sub;
            } else if (*role == "kanji-driver") {
                target = &cfg.bios_roms.kanji_driver;
            } else if (*role == "disk") {
                target = &cfg.bios_roms.disk;
            } else if (*role == "fm-music") {
                target = &cfg.bios_roms.fm_music;
            } else if (*role == "kanji-font") {
                target = &cfg.bios_roms.kanji_font;
            } else if (*role == "firmware") {
                target = &cfg.bios_roms.firmware;
            }
            if (target == nullptr) {
                warn_value(warnings, "machine/bios/rom@role", *role,
                           "expected bios|sub|kanji-driver|disk|fm-music|kanji-font|firmware",
                           "(ignored)");
                return;
            }
            if (file != nullptr) {
                *target = *file;  // omitted file -> keep the role's default filename
            }
        } else {
            warn_unknown(warnings, "machine/bios/" + name);
        }
        return;
    }

    // --- inside <slots>: ignore the whole subtree in S1 (validated-to-builtin
    //     is S3; S1 must not spuriously warn on documented advanced markup) ---
    if (parent == "slots") {
        return;
    }

    // Any other (unexpected) parent context: tolerated + ignored.
}

}  // namespace

EmulatorConfig EmulatorConfig::parse(std::string_view xml_text, std::vector<std::string>& warnings) {
    EmulatorConfig cfg;  // built-in defaults
    XmlTagScanner scanner(xml_text, /*capture_attributes=*/true);

    std::vector<std::string> stack;  // currently-open element names (context)
    bool saw_root = false;
    bool started = false;

    for (;;) {
        const XmlToken tok = scanner.next(nullptr);
        if (tok.kind == XmlTokenKind::Eof) {
            break;
        }

        // The FIRST element token must be the <hbf1xv-config> root; anything
        // else (wrong root, a stray close, binary garbage) is a STRUCTURAL
        // failure -> whole-file fallback.
        if (!started) {
            started = true;
            const bool is_element =
                (tok.kind == XmlTokenKind::OpenTag || tok.kind == XmlTokenKind::SelfCloseTag);
            if (is_element && tok.name == "hbf1xv-config") {
                saw_root = true;
                if (tok.kind == XmlTokenKind::OpenTag) {
                    stack.push_back(tok.name);
                }
            }
            if (!saw_root) {
                break;  // structural failure; fall through to the whole-file warning
            }
            continue;
        }

        if (tok.kind == XmlTokenKind::CloseTag) {
            if (!stack.empty()) {
                stack.pop_back();
            }
            if (stack.empty()) {
                break;  // closed the root element -> done
            }
            continue;
        }

        const bool self_close = (tok.kind == XmlTokenKind::SelfCloseTag);
        const std::string parent = stack.empty() ? std::string() : stack.back();
        apply_element(parent, tok, cfg, warnings);
        if (!self_close) {
            stack.push_back(tok.name);
        }
    }

    if (!saw_root) {
        warnings.push_back(
            "config: WARNING input is not a valid hbf1xv-config document; using built-in defaults");
        return EmulatorConfig{};  // all built-in defaults
    }
    return cfg;
}

EmulatorConfig EmulatorConfig::load_from_file(const std::string& path,
                                              std::vector<std::string>& warnings) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        warnings.push_back("config: WARNING " + path +
                           " could not be read; using built-in defaults");
        return EmulatorConfig{};
    }
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse(content, warnings);
}

}  // namespace sony_msx::machine
