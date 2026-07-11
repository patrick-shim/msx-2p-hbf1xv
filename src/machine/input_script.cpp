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

#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "machine/debug_format.h"
#include "peripherals/msx_key_names.h"

namespace sony_msx::machine {

namespace {

// JOY= control token <-> JoyControl. Kept adjacent to the parser/serializer so
// the two directions stay in lockstep (round-trip). Unknown token -> nullopt.
[[nodiscard]] std::optional<JoyControl> joy_control_from_token(const std::string& token) {
    if (token == "UP") return JoyControl::Up;
    if (token == "DOWN") return JoyControl::Down;
    if (token == "LEFT") return JoyControl::Left;
    if (token == "RIGHT") return JoyControl::Right;
    if (token == "A") return JoyControl::TriggerA;
    if (token == "B") return JoyControl::TriggerB;
    return std::nullopt;
}

[[nodiscard]] const char* joy_control_to_token(const JoyControl control) {
    switch (control) {
        case JoyControl::Up: return "UP";
        case JoyControl::Down: return "DOWN";
        case JoyControl::Left: return "LEFT";
        case JoyControl::Right: return "RIGHT";
        case JoyControl::TriggerA: return "A";
        case JoyControl::TriggerB: return "B";
    }
    return "UP";  // unreachable (all enumerators handled)
}

}  // namespace

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
        std::string kind_token;
        iss >> t_token >> kind_token;
        if (t_token.rfind("T=", 0) != 0) {
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

        InputScriptEvent event;
        event.at_tstate = t_value;

        if (kind_token.rfind("KEY=", 0) == 0) {
            // KEY= line (pre-M41, unchanged): T=<t> KEY=<name> DOWN|UP.
            std::string state_token;
            iss >> state_token;
            if (state_token != "DOWN" && state_token != "UP") {
                throw std::runtime_error("parse_input_script: malformed event line: " + line);
            }
            const std::string key_name = kind_token.substr(4);
            if (!peripherals::key_name_to_row_col(key_name).has_value()) {
                throw std::runtime_error("parse_input_script: unrecognized KEY name: " + key_name);
            }
            event.kind = InputEventKind::Key;
            event.key_name = key_name;
            event.pressed = (state_token == "DOWN");
        } else if (kind_token.rfind("JOY=", 0) == 0) {
            // M41-S1 JOY= line: T=<t> JOY=<port> <control> DOWN|UP.
            std::string control_token;
            std::string state_token;
            iss >> control_token >> state_token;
            if (state_token != "DOWN" && state_token != "UP") {
                throw std::runtime_error("parse_input_script: malformed event line: " + line);
            }
            int port = 0;
            try {
                std::size_t consumed = 0;
                port = std::stoi(kind_token.substr(4), &consumed);
                if (consumed != kind_token.size() - 4) {
                    throw std::runtime_error("trailing characters");
                }
            } catch (const std::exception&) {
                throw std::runtime_error("parse_input_script: malformed JOY port: " + line);
            }
            if (port != 1 && port != 2) {
                throw std::runtime_error("parse_input_script: JOY port must be 1 or 2: " + line);
            }
            const std::optional<JoyControl> control = joy_control_from_token(control_token);
            if (!control.has_value()) {
                throw std::runtime_error("parse_input_script: unrecognized JOY control: " + control_token);
            }
            event.kind = InputEventKind::Joy;
            event.joy_port = port;
            event.joy_control = *control;
            event.pressed = (state_token == "DOWN");
        } else {
            throw std::runtime_error("parse_input_script: malformed event line: " + line);
        }

        events.push_back(std::move(event));
    }

    return events;
}

std::string serialize_input_script(const std::vector<InputScriptEvent>& events) {
    std::string out;
    out += kInputScriptFormatTag;
    out.push_back('\n');
    for (const InputScriptEvent& event : events) {
        const std::string state = event.pressed ? "DOWN" : "UP";
        if (event.kind == InputEventKind::Joy) {
            out += "T=" + debug_format::to_dec(event.at_tstate) + " JOY=" +
                   debug_format::to_dec(static_cast<std::uint64_t>(event.joy_port)) + " " +
                   joy_control_to_token(event.joy_control) + " " + state + "\n";
        } else {
            // KEY= line -- byte-identical to the pre-M41 serializer output.
            out += "T=" + debug_format::to_dec(event.at_tstate) + " KEY=" + event.key_name + " " +
                   state + "\n";
        }
    }
    out += "[END]\n";
    return out;
}

InputScriptPlayer::InputScriptPlayer(std::vector<InputScriptEvent> events) : events_(std::move(events)) {}

void InputScriptPlayer::attach_joystick(peripherals::JoystickPorts* joystick) { joystick_ = joystick; }

void InputScriptPlayer::apply_due(const std::uint64_t current_tstate, peripherals::KeyboardMatrix& keyboard) {
    while (cursor_ < events_.size() && events_[cursor_].at_tstate <= current_tstate) {
        const InputScriptEvent& event = events_[cursor_];
        if (event.kind == InputEventKind::Joy) {
            // M41-S1: accumulate into the shadow PortState so distinct controls
            // coexist (LEFT does not clear a held trigger). A null joystick_
            // (keyboard-only path) makes this a safe no-op; the cursor still
            // advances so no later KEY= event is ever skipped.
            if (joystick_ != nullptr && (event.joy_port == 1 || event.joy_port == 2)) {
                peripherals::JoystickPorts::PortState& state =
                    joy_state_[static_cast<std::size_t>(event.joy_port - 1)];
                switch (event.joy_control) {
                    case JoyControl::Up: state.up = event.pressed; break;
                    case JoyControl::Down: state.down = event.pressed; break;
                    case JoyControl::Left: state.left = event.pressed; break;
                    case JoyControl::Right: state.right = event.pressed; break;
                    case JoyControl::TriggerA: state.trigger_a = event.pressed; break;
                    case JoyControl::TriggerB: state.trigger_b = event.pressed; break;
                }
                joystick_->set_port(event.joy_port - 1, state);
            }
        } else {
            const auto coord = peripherals::key_name_to_row_col(event.key_name);
            if (coord.has_value()) {
                keyboard.set_key(coord->first, coord->second, event.pressed);
            }
        }
        ++cursor_;
    }
}

}  // namespace sony_msx::machine
