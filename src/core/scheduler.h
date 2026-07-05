#pragma once

#include <cstdint>

namespace sony_msx::core {

class Scheduler {
public:
    void reset();
    void tick(std::uint64_t cycles);
    void tick_many(std::uint32_t steps, std::uint64_t cycles_per_step);
    bool advance_to(std::uint64_t target_cycles);
    [[nodiscard]] std::uint64_t total_cycles() const;

private:
    std::uint64_t total_cycles_ = 0;
};

}  // namespace sony_msx::core
