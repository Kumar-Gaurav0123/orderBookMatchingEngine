#pragma once

#include <cstdint>
#include <cassert>
#include "../order/Order.hpp"
#include "../order/OrderTypes.hpp"

// Represents a single price level in the order book.
// Holds all resting orders at this price in FIFO (price-time priority) order.
// Uses the intrusive next/prev pointers from Order for O(1) insert/remove.

namespace obk {

class PriceLevel {
public:
    using OrderType = Order;

private:
    uint64_t    price_;           // price of this level (integer ticks)
    uint64_t    total_quantity_;  // sum of remaining quantities of all resting orders
    OrderType*  head_;            // first order in FIFO (matched first)
    OrderType*  tail_;            // last order in FIFO (matched last)

public:
    explicit PriceLevel(uint64_t price);

    // --- Accessors ---------------------------------------------------------

    uint64_t price()         const noexcept { return price_; }
    uint64_t totalQuantity() const noexcept { return total_quantity_; }

    bool empty() const noexcept { return head_ == nullptr; }

    OrderType* front()             noexcept { return head_; }
    const OrderType* front() const noexcept { return head_; }

    // --- Core FIFO operations ----------------------------------------------

    // Insert a new resting order at the back of the queue (O(1)).
    void addOrder(OrderType* order) noexcept;

    // Pop front order (used during matching). Does NOT update total_quantity_;
    // caller must call onFill() with the traded amount before popping.
    OrderType* popFront() noexcept;

    // Remove an arbitrary resting order (cancel/modify). O(1) with intrusive ptrs.
    void removeOrder(OrderType* order) noexcept;

    // Called by the matching loop to keep total_quantity_ in sync as fills
    // reduce a resting order's remaining quantity before popFront().
    void onFill(uint64_t traded_qty) noexcept { total_quantity_ -= traded_qty; }

    // O(n) walk — for debugging/display only.
    size_t size() const noexcept;
};

} // namespace obk
