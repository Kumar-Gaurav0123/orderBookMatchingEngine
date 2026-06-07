#pragma once

#include <cstdint>
#include <string_view>

namespace obk {

// The side of the order: Buy or Sell
enum class Side : uint8_t {
    Buy  = 0,
    Sell = 1
};

// Convert enum to string (constexpr)
constexpr std::string_view to_string(Side s) noexcept {
    switch (s) {
        case Side::Buy:  return "BUY";
        case Side::Sell: return "SELL";
    }
    return "UNKNOWN";
}

// Order type: whether the order has a price limit or takes any available price.
// IOC and FOK are time-in-force policies (see below) — NOT order types.
// Mixing them here was the original design flaw; this enum is now clean.
enum class OrderType : uint8_t {
    LIMIT  = 0,   // rests at specified price if unmatched (subject to TIF)
    MARKET = 1    // takes best available price; never rests in the book
};

constexpr std::string_view to_string(OrderType t) noexcept {
    switch (t) {
        case OrderType::LIMIT:  return "LIMIT";
        case OrderType::MARKET: return "MARKET";
    }
    return "UNKNOWN";
}

// Time-in-force policies
enum class TimeInForce : uint8_t {
    GTC = 0,   // Good till canceled
    IOC = 1,   // Immediate or cancel
    FOK = 2    // Fill or kill
};

constexpr std::string_view to_string(TimeInForce tif) noexcept {
    switch (tif) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
    }
    return "UNKNOWN";
}

// Order status (useful for reporting, not required for matching engine internals)
enum class OrderStatus : uint8_t {
    New,
    PartiallyFilled,
    Filled,
    Cancelled,
    Rejected
};

constexpr std::string_view to_string(OrderStatus s) noexcept {
    switch (s) {
        case OrderStatus::New:             return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled:          return "FILLED";
        case OrderStatus::Cancelled:       return "CANCELLED";
        case OrderStatus::Rejected:        return "REJECTED";
    }
    return "UNKNOWN";
}

} // namespace obk
