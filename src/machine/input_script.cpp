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

#include "machine/input_script.h"

#include <sstream>
#include <stdexcept>
#include <utility>

#include "machine/debug_format.h"
#include "peripherals/msx_key_names.h"

namespace sony_msx::machine {

std::vector<InputScriptEvent> parse_input_script(const std::string& text) {
    std::vector<std::string> lines;
    {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
    }

    if (lines.empty() || lines[0] != kInputScriptFormatTag) {
        throw std::runtime_error("parse_input_script: missing/mismatched format tag");
    }

    std::vector<InputScriptEvent> events;
    std::uint64_t last_tstate = 0;
    bool have_last = false;

    for (std::size_t i = 1; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (line.empty()) {
            continue;
        }
        if (line == "[END]") {
            break;
        }

        std::istringstream iss(line);
        std::string t_token;
        std::string key_token;
        std::string state_token;
        iss >> t_token >> key_token >> state_token;
        if (t_token.rfind("T=", 0) != 0 || key_token.rfind("KEY=", 0) != 0 ||
            (state_token != "DOWN" && state_token != "UP")) {
            throw std::runtime_error("parse_input_script: malformed event line: " + line);
        }

        std::uint64_t t_value = 0;
        try {
            std::size_t consumed = 0;
            t_value = std::stoull(t_token.substr(2), &consumed);
            if (consumed != t_token.size() - 2) {
                throw std::runtime_error("trailing characters");
            }
        } catch (const std::exception&) {
            throw std::runtime_error("parse_input_script: malformed T value: " + line);
        }
        if (have_last && t_value < last_tstate) {
            throw std::runtime_error("parse_input_script: T values are not non-decreasing: " + line);
        }
        last_tstate = t_value;
        have_last = true;

        const std::string key_name = key_token.substr(4);
        if (!peripherals::key_name_to_row_col(key_name).has_value()) {
            throw std::runtime_error("parse_input_script: unrecognized KEY name: " + key_name);
        }

        InputScriptEvent event;
        event.at_tstate = t_value;
        event.key_name = key_name;
        event.pressed = (state_token == "DOWN");
        events.push_back(std::move(event));
    }

    return events;
}

std::string serialize_input_script(const std::vector<InputScriptEvent>& events) {
    std::string out;
    out += kInputScriptFormatTag;
    out.push_back('\n');
    for (const InputScriptEvent& event : events) {
        out += "T=" + debug_format::to_dec(event.at_tstate) + " KEY=" + event.key_name + " " +
               (event.pressed ? "DOWN" : "UP") + "\n";
    }
    out += "[END]\n";
    return out;
}

InputScriptPlayer::InputScriptPlayer(std::vector<InputScriptEvent> events) : events_(std::move(events)) {}

void InputScriptPlayer::apply_due(const std::uint64_t current_tstate, peripherals::KeyboardMatrix& keyboard) {
    while (cursor_ < events_.size() && events_[cursor_].at_tstate <= current_tstate) {
        const InputScriptEvent& event = events_[cursor_];
        const auto coord = peripherals::key_name_to_row_col(event.key_name);
        if (coord.has_value()) {
            keyboard.set_key(coord->first, coord->second, event.pressed);
        }
        ++cursor_;
    }
}

}  // namespace sony_msx::machine
