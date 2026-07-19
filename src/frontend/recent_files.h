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

// A small, SDL-free most-recently-used list for the File > Recent menu. Kept
// entirely OUT of the machine-layer config (owner decision): recents are pure
// frontend UX state, not machine configuration, so they live in their own
// plain-text sidecar file (one path per line, most-recent first) rather than
// the strict `<hbf1xv-config>` XML. Pure string/vector ops -> headlessly
// unit-testable; the SDL3 frontend owns the interactive-only file I/O + the
// determinism gate (a headless / --hidden-window run never reads or writes it).
namespace sony_msx::frontend {

// The maximum number of entries retained (older ones fall off the tail).
inline constexpr std::size_t kMaxRecentFiles = 8;

// A most-recent-first, de-duplicated, capped list of media paths.
class RecentFiles {
public:
    RecentFiles() = default;

    // Insert `path` at the FRONT as the newest entry. A case-sensitive exact
    // duplicate anywhere in the list is first removed (so re-opening a file
    // moves it to the front rather than adding a second copy). Blank/whitespace
    // paths are ignored. The list is then truncated to kMaxRecentFiles.
    void push(const std::string& path);

    // The current entries, newest first.
    [[nodiscard]] const std::vector<std::string>& entries() const { return entries_; }
    [[nodiscard]] std::size_t size() const { return entries_.size(); }
    [[nodiscard]] bool empty() const { return entries_.empty(); }

    // Bounds-checked lookup; returns "" for an out-of-range index.
    [[nodiscard]] std::string at(std::size_t index) const;

    void clear() { entries_.clear(); }

    // --- Serialization (plain text, one path per line, newest first) ---------
    // Round-trips: parse(serialize(r)) == r for any r. `serialize` newline-
    // terminates every entry; `parse` skips blank lines and applies the same
    // dedup + cap as push() (so a hand-edited or oversized file is normalized).
    [[nodiscard]] std::string serialize() const;
    static RecentFiles parse(const std::string& text);

private:
    std::vector<std::string> entries_;
};

}  // namespace sony_msx::frontend
