#pragma once

#include <cstddef>
#include "../order/Order.hpp"

using obk::Order;

// Intrusive FIFO queue for Order pointers. No dynamic allocation.
// Reuses the next/prev pointers embedded in Order (intrusive design).
// PriceLevel uses this for each price level's order queue.

class OrderQueue {
public:
    using OrderType = Order;

private:
    OrderType* head_;
    OrderType* tail_;
    size_t     size_;

public:
    OrderQueue() : head_(nullptr), tail_(nullptr), size_(0) {}

    bool   empty() const noexcept { return size_ == 0; }
    size_t size()  const noexcept { return size_; }

    OrderType* head() const noexcept { return head_; }
    OrderType* tail() const noexcept { return tail_; }

    // FIFO insert at back
    void pushBack(OrderType* order) noexcept;

    // Remove and return front (oldest) order
    OrderType* popFront() noexcept;

    // O(1) removal of an arbitrary order via intrusive pointers
    void remove(OrderType* order) noexcept;

    // Unlinks all orders without freeing (engine/pool owns memory)
    void clear() noexcept;
};
