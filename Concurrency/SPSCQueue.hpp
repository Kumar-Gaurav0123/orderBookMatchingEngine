#pragma once

#include "AtomicRingBuffer.hpp"

// Single-Producer / Single-Consumer queue.
// Just a nicer name + API over AtomicRingBuffer.
template<typename T>
class SPSCQueue : private NonCopyable {
public:
    explicit SPSCQueue(std::size_t capacity)
        : ring_(capacity)
    {}

    bool push(const T& v) noexcept  { return ring_.push(v); }
    bool push(T&& v) noexcept       { return ring_.push(std::move(v)); }

    bool pop(T& out) noexcept       { return ring_.pop(out); }

    bool empty() const noexcept     { return ring_.empty(); }
    bool full()  const noexcept     { return ring_.full();  }

    std::size_t size() const noexcept { return ring_.size(); }
    std::size_t capacity() const noexcept { return ring_.capacity(); }

private:
    AtomicRingBuffer<T> ring_;
};
