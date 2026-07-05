#include "machine/debug_event_log.h"

#include <utility>

#include "machine/debug_format.h"

namespace sony_msx::machine {

namespace {

using debug_format::to_dec;
using debug_format::to_hex;

}  // namespace

void DebugEventLog::clear() {
    events_.clear();
    next_sequence_ = 0;
}

void DebugEventLog::record(const DebugEventType type, const std::uint64_t tstate, std::string detail) {
    DebugEvent event;
    event.sequence = next_sequence_++;
    event.tstate = tstate;
    event.type = type;
    event.detail = std::move(detail);
    events_.push_back(std::move(event));
}

std::size_t DebugEventLog::size() const {
    return events_.size();
}

bool DebugEventLog::empty() const {
    return events_.empty();
}

const std::vector<DebugEvent>& DebugEventLog::events() const {
    return events_;
}

const DebugEvent& DebugEventLog::at(const std::size_t index) const {
    return events_.at(index);
}

const char* DebugEventLog::type_name(const DebugEventType type) {
    switch (type) {
        case DebugEventType::Reset:
            return "RESET";
        case DebugEventType::InstructionRetire:
            return "INSTR";
        case DebugEventType::Halt:
            return "HALT";
        case DebugEventType::Dump:
            return "DUMP";
    }
    return "UNKNOWN";
}

std::string DebugEventLog::format_event(const DebugEvent& event) {
    std::string line;
    line += "EVT=" + to_hex(event.sequence, 4);
    line += " T=" + to_dec(event.tstate);
    line += " TYPE=" + std::string(type_name(event.type));
    if (!event.detail.empty()) {
        line += " " + event.detail;
    }
    return line;
}

std::string DebugEventLog::serialize() const {
    std::string out;
    for (const DebugEvent& event : events_) {
        out += format_event(event);
        out.push_back('\n');
    }
    return out;
}

}  // namespace sony_msx::machine
