#pragma once

#include <cstdint>
#include "../mem/IntrusiveNode.hpp"
#include "OrderTypes.hpp"
#include "TimeStamp.hpp"

namespace obk {

// Forward declare PriceLevel to avoid circular dependency
struct PriceLevel;

// The core order structure used throughout the matching engine.
//
// IMPORTANT DESIGN REQUIREMENTS:
// 1. Must be trivially movable but NEVER copied.
// 2. Memory is supplied by a MemoryPool, not by new/delete.
// 3. Contains an intrusive node for ultra-fast queue linking.
// 4. Fields are laid out to avoid padding and keep hot data together.

// Aligned to a cache line (64 bytes) to prevent false sharing between
// adjacent orders in a MemoryPool slab. Hot matching fields (remaining,
// price) are clustered at the top of the struct so they land in the same
// cache line as the intrusive next/prev pointers used during traversal.
struct alignas(64) Order : public mem::IntrusiveNode {
    using Node = mem::IntrusiveNode;

    // ---- Hot matching fields (first cache line) ----
    uint64_t    price;            // Price in integer ticks
    uint64_t    remaining;        // Remaining quantity after partial fills
    uint64_t    qty;              // Original submitted quantity (immutable after creation)
    uint64_t    id;               // Unique order ID (assigned by OrderIdPool)

    // ---- Order Metadata ----
    Side        side;             // BUY / SELL
    OrderType   type;             // LIMIT / MARKET / IOC
    TimeInForce tif;              // GTC / IOC / FOK
    obk::TimeStamp ts;            // Nanosecond timestamp for time-priority

    // ---- Book Linkage ----
    PriceLevel* parentLevel = nullptr; // Back-pointer to owning price level

    // ---- Constructors ----
    Order(uint64_t _id,
          Side _side,
          OrderType _type,
          TimeInForce _tif,
          uint64_t _price,
          uint64_t _qty)
        : price(_price),
          remaining(_qty),
          qty(_qty),
          id(_id),
          side(_side),
          type(_type),
          tif(_tif),
          ts(obk::TimeStamp::now()),
          parentLevel(nullptr)
    {}

    // Disable copying (orders cannot be copied)
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;

    // Disable moving (linked intrusive nodes must never be relocated)
    Order(Order&&) = delete;
    Order& operator=(Order&&) = delete;

    [[nodiscard]] bool is_filled()        const noexcept { return remaining == 0; }
    [[nodiscard]] bool is_partial_fill()  const noexcept { return remaining > 0 && remaining < qty; }
};

} // namespace obk
