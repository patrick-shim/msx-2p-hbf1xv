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

#include "frontend/recent_files.h"

#include <algorithm>

namespace sony_msx::frontend {

namespace {

// Trim leading/trailing ASCII whitespace (incl. a trailing '\r' from CRLF text).
std::string trim(const std::string& s) {
    const auto not_space = [](unsigned char c) {
        return c != ' ' && c != '\t' && c != '\r' && c != '\n';
    };
    const auto begin = std::find_if(s.begin(), s.end(), not_space);
    const auto end = std::find_if(s.rbegin(), s.rend(), not_space).base();
    return (begin < end) ? std::string(begin, end) : std::string();
}

}  // namespace

void RecentFiles::push(const std::string& path) {
    const std::string p = trim(path);
    if (p.empty()) {
        return;  // ignore blank paths
    }
    // Move-to-front semantics: drop any existing exact duplicate first.
    entries_.erase(std::remove(entries_.begin(), entries_.end(), p), entries_.end());
    entries_.insert(entries_.begin(), p);
    if (entries_.size() > kMaxRecentFiles) {
        entries_.resize(kMaxRecentFiles);
    }
}

std::string RecentFiles::at(const std::size_t index) const {
    return index < entries_.size() ? entries_[index] : std::string();
}

std::string RecentFiles::serialize() const {
    std::string out;
    for (const std::string& e : entries_) {
        out += e;
        out += '\n';
    }
    return out;
}

RecentFiles RecentFiles::parse(const std::string& text) {
    // Read newest-first lines. Build back-to-front so push()'s move-to-front
    // leaves the first line newest, and reuse push() for identical dedup + cap.
    std::vector<std::string> lines;
    std::string line;
    for (const char c : text) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += c;
        }
    }
    if (!line.empty()) {
        lines.push_back(line);
    }

    RecentFiles r;
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        r.push(*it);  // push() trims + skips blanks + dedups + caps
    }
    return r;
}

}  // namespace sony_msx::frontend
