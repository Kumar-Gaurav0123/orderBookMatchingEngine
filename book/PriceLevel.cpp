#include "PriceLevel.hpp"

namespace obk {

PriceLevel::PriceLevel(uint64_t price)
    : price_(price)
    , total_quantity_(0)
    , head_(nullptr)
    , tail_(nullptr)
{}

void PriceLevel::addOrder(OrderType* order) noexcept {
    assert(order);

    order->prev = tail_;
    order->next = nullptr;

    if (tail_)
        tail_->next = order;
    else
        head_ = order;

    tail_ = order;
    // Track remaining (not original qty) — equal on first insertion,
    // diverges if an order was partially filled before being re-queued.
    total_quantity_ += order->remaining;
}

PriceLevel::OrderType* PriceLevel::popFront() noexcept {
    if (!head_)
        return nullptr;

    OrderType* order = head_;
    head_ = static_cast<OrderType*>(order->next);

    if (head_)
        head_->prev = nullptr;
    else
        tail_ = nullptr;

    order->next = nullptr;
    order->prev = nullptr;
    // NOTE: total_quantity_ is updated via onFill() during matching.
    // By the time popFront() is called the order's remaining == 0,
    // so we intentionally do NOT subtract here to avoid double-counting.
    return order;
}

void PriceLevel::removeOrder(OrderType* order) noexcept {
    assert(order);

    OrderType* prev = static_cast<OrderType*>(order->prev);
    OrderType* next = static_cast<OrderType*>(order->next);

    if (prev)
        prev->next = next;
    else
        head_ = next;

    if (next)
        next->prev = prev;
    else
        tail_ = prev;

    order->next = nullptr;
    order->prev = nullptr;

    // Subtract whatever the order still had remaining (i.e. not yet traded).
    total_quantity_ -= order->remaining;
}

size_t PriceLevel::size() const noexcept {
    size_t count = 0;
    for (auto* o = head_; o; o = static_cast<OrderType*>(o->next))
        ++count;
    return count;
}

} // namespace obk
