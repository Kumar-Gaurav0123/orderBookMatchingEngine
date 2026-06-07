#pragma once

#include <atomic>
#include <cstddef>
#include "../utils/NonCopyable.hpp"

// Multi-Producer / Single-Consumer queue.
// Unbounded, lock-free. Suitable for passing pointers or small PODs.
template<typename T>
class MPSCQueue : private NonCopyable {
public:
    explicit MPSCQueue(std::size_t /*capacity*/)
        : MPSCQueue() {}   // delegate to default
    
    MPSCQueue()
        : head_(new Node()), tail_(head_.load(std::memory_order_relaxed))
    {}

    ~MPSCQueue() {
        // Drain and delete all remaining nodes.
        T tmp;
        while (pop(tmp)) { /* discard */ }

        Node* front = head_.load(std::memory_order_relaxed);
        delete front;
    }

    // Multiple producers: enqueue value.
    void push(const T& value) {
        Node* node = new Node(value);
        node->next.store(nullptr, std::memory_order_relaxed);

        Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    void push(T&& value) {
        Node* node = new Node(std::move(value));
        node->next.store(nullptr, std::memory_order_relaxed);

        Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Single consumer: dequeue. Returns false if empty.
    bool pop(T& out) {
        Node* head = head_.load(std::memory_order_relaxed);
        Node* next = head->next.load(std::memory_order_acquire);

        if (!next) {
            // queue empty
            return false;
        }

        out = std::move(next->value);

        head_.store(next, std::memory_order_relaxed);
        delete head; // delete old dummy

        return true;
    }

    // Approximate emptiness check (only strictly correct on consumer thread).
    bool empty() const {
        Node* head = head_.load(std::memory_order_relaxed);
        return head->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        std::atomic<Node*> next;
        T                  value;

        Node() : next(nullptr), value() {}
        explicit Node(const T& v) : next(nullptr), value(v) {}
        explicit Node(T&& v) : next(nullptr), value(std::move(v)) {}
    };

    std::atomic<Node*> head_; // consumer side, points to dummy
    std::atomic<Node*> tail_; // producers push at tail
};
