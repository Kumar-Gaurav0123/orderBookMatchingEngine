#pragma once

#include <cstdint>
#include <chrono>

namespace obk {

// Unified timestamp used throughout the engine.
// Internally represented as a 64-bit unsigned integer.
// You can interpret it as:
//   - nanoseconds since epoch
//   - monotonic counter (TSC normalized)
//   - user-provided logical timestamp
//
// This makes the design flexible for real exchange integration.

struct TimeStamp {
    uint64_t value = 0;

    TimeStamp() = default;
    explicit TimeStamp(uint64_t v) : value(v) {}

    // Comparison operators (for price-time priority)
    constexpr bool operator<(const TimeStamp& other) const noexcept { return value < other.value; }
    constexpr bool operator>(const TimeStamp& other) const noexcept { return value > other.value; }
    constexpr bool operator<=(const TimeStamp& other) const noexcept { return value <= other.value; }
    constexpr bool operator>=(const TimeStamp& other) const noexcept { return value >= other.value; }
    constexpr bool operator==(const TimeStamp& other) const noexcept { return value == other.value; }
    constexpr bool operator!=(const TimeStamp& other) const noexcept { return value != other.value; }

    // Allow implicit conversion to uint64_t for sorting/printing
    constexpr operator uint64_t() const noexcept { return value; }

    // Called by Order constructor. Production engines replace this with a
    // fast RDTSC reader; steady_clock is correct and sufficient for testing.
    static TimeStamp now() noexcept {
        using namespace std::chrono;
        return TimeStamp(duration_cast<nanoseconds>(
            steady_clock::now().time_since_epoch()).count());
    }
};

// -----------------------------------------------------------------------------
// Optional: REAL nanosecond timestamp using steady_clock
// -----------------------------------------------------------------------------
// This is used for debugging & testing.
// Production HFT engines typically replace this with a fast TSC reader.
// -----------------------------------------------------------------------------

inline TimeStamp now_ns() noexcept {
    using namespace std::chrono;
    return TimeStamp(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace obk
