#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sony_msx::machine {

// Execution-event categories captured by the debug event log (M10-S3).
enum class DebugEventType : std::uint8_t {
    Reset = 0,             // Machine cold_boot / CPU reset.
    InstructionRetire = 1, // A single CPU instruction retired.
    Halt = 2,              // CPU entered the halted state this step.
    Dump = 3,              // A full-state dump was produced.
};

// One deterministic execution event. `tstate` is the cumulative machine clock
// (scheduler T-states) at the event — a deterministic monotonic value, NOT a
// wall-clock time. `detail` is a fixed ASCII payload.
struct DebugEvent {
    std::uint64_t sequence = 0;
    std::uint64_t tstate = 0;
    DebugEventType type = DebugEventType::Reset;
    std::string detail;
};

// Deterministic collector + serializer for execution events (M10-S3).
//
// The event stream is byte-stable across identical runs: sequence indices are a
// monotonic counter, `tstate` comes from the deterministic scheduler clock, and
// serialization is hand-rolled ASCII (fixed field order, fixed-width uppercase
// hex, '\n' line endings, no locale/stream state, no wall-clock).
class DebugEventLog {
public:
    void clear();
    void record(DebugEventType type, std::uint64_t tstate, std::string detail);

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const std::vector<DebugEvent>& events() const;
    [[nodiscard]] const DebugEvent& at(std::size_t index) const;

    // Fixed short name for an event type (e.g. "RESET", "INSTR", "HALT", "DUMP").
    [[nodiscard]] static const char* type_name(DebugEventType type);

    // Deterministic single-line serialization of one event.
    [[nodiscard]] static std::string format_event(const DebugEvent& event);

    // Deterministic multi-line serialization; one '\n'-terminated line per event.
    [[nodiscard]] std::string serialize() const;

private:
    std::vector<DebugEvent> events_;
    std::uint64_t next_sequence_ = 0;
};

}  // namespace sony_msx::machine
