#include "OrderQueue.hpp"

OrderQueue::OrderQueue()
    : head_(nullptr), tail_(nullptr), size_(0) {}

// ---------------- Query Methods ----------------

bool OrderQueue::empty() const noexcept {
    return size_ == 0;
}

size_t OrderQueue::size() const noexcept {
    return size_;
}

OrderQueue::OrderType* OrderQueue::head() const noexcept {
    return head_;
}

OrderQueue::OrderType* OrderQueue::tail() const noexcept {
    return tail_;
}

// ---------------- Modifiers ----------------

void OrderQueue::pushBack(OrderType* order) noexcept {
    if (!order) return;

    order->next = nullptr;
    order->prev = tail_;

    if (tail_) {
        tail_->next = order;
    } else {
        // Queue was empty
        head_ = order;
    }

    tail_ = order;
    ++size_;
}

OrderQueue::OrderType* OrderQueue::popFront() noexcept {
    if (!head_) return nullptr;

    OrderType* removed = head_;
    head_ = static_cast<OrderType*>(head_->next);

    if (head_) {
        head_->prev = nullptr;
    } else {
        // Queue is empty now
        tail_ = nullptr;
    }

    removed->next = nullptr;
    removed->prev = nullptr;
    --size_;

    return removed;
}

void OrderQueue::remove(OrderType* order) noexcept {
    if (!order) return;

    if (order->prev) {
        order->prev->next = order->next;
    } else {
        // removing head
        head_ = static_cast<OrderType*>(order->next);
    }

    if (order->next) {
        order->next->prev = order->prev;
    } else {
        // removing tail
        tail_ = static_cast<OrderType*>(order->prev);
    }

    order->next = nullptr;
    order->prev = nullptr;
    --size_;
}

// Clears queue without deleting memory (orders live in memory pool)
void OrderQueue::clear() noexcept {
    OrderType* cur = head_;
    while (cur) {
        OrderType* next = static_cast<OrderType*>(cur->next);
        cur->next = nullptr;
        cur->prev = nullptr;
        cur = next;
    }

    head_ = nullptr;
    tail_ = nullptr;
    size_ = 0;
}
