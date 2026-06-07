#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <type_traits>
#include "../utils/NonCopyable.hpp"

// Single-Producer / Single-Consumer bounded ring buffer.
// Capacity must be > 0, preferably a power of two for best performance.
template<typename T>
class AtomicRingBuffer : private NonCopyable {
public:
    explicit AtomicRingBuffer(std::size_t capacity)
        : capacity_(capacity)
        , mask_(capacity - 1)
        , buffer_(capacity)
    {}

    // Not thread-safe: just for debug/introspection.
    std::size_t capacity() const noexcept { return capacity_; }

    // Approximate count (may be slightly stale across threads).
    std::size_t size() const noexcept {
        auto tail = tail_.load(std::memory_order_acquire);
        auto head = head_.load(std::memory_order_acquire);
        return tail - head;
    }

    bool empty() const noexcept {
        auto tail = tail_.load(std::memory_order_acquire);
        auto head = head_.load(std::memory_order_acquire);
        return tail == head;
    }

    bool full() const noexcept {
        auto tail = tail_.load(std::memory_order_acquire);
        auto head = head_.load(std::memory_order_acquire);
        return (tail - head) >= capacity_;
    }

    // Producer: enqueue item. Returns false if buffer is full.
    bool push(const T& value) noexcept {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto head = head_.load(std::memory_order_acquire);

        if ((tail - head) >= capacity_) {
            // full
            return false;
        }

        buffer_[index(tail)] = value;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    bool push(T&& value) noexcept {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto head = head_.load(std::memory_order_acquire);

        if ((tail - head) >= capacity_) {
            return false;
        }

        buffer_[index(tail)] = std::move(value);
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Consumer: pop item. Returns false if buffer is empty.
    bool pop(T& out) noexcept {
        auto head = head_.load(std::memory_order_relaxed);
        auto tail = tail_.load(std::memory_order_acquire);

        if (tail == head) {
            // empty
            return false;
        }

        out = std::move(buffer_[index(head)]);
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

private:
    std::size_t index(std::size_t i) const noexcept {
        // if capacity is power of two we can mask, else use modulo
        if ((capacity_ & mask_) == 0) {
            return i & mask_;
        } else {
            return i % capacity_;
        }
    }

    const std::size_t capacity_;
    const std::size_t mask_;
    std::vector<T>    buffer_;

    // Each atomic is on its own cache line to prevent false sharing.
    // The producer owns tail_ (writes it, reads head_).
    // The consumer owns head_ (writes it, reads tail_).
    // Without this padding every write by one thread invalidates the
    // other thread's cache line — a classic SPSC performance pitfall.
    alignas(64) std::atomic<std::size_t> head_{0}; // consumer writes
    alignas(64) std::atomic<std::size_t> tail_{0}; // producer writes
};
