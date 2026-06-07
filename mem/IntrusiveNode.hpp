#pragma once

namespace mem {

// Base intrusive node for linking objects without separate allocation.
// Used by OrderQueue, MemoryPool free list, etc.

struct IntrusiveNode {
    IntrusiveNode* next = nullptr;
    IntrusiveNode* prev = nullptr;   // optional: needed for doubly-linked queues

    IntrusiveNode() = default;

    // Non-copyable (you don't want accidental copies of nodes)
    IntrusiveNode(const IntrusiveNode&) = delete;
    IntrusiveNode& operator=(const IntrusiveNode&) = delete;

    // Movable is okay but rarely used
    IntrusiveNode(IntrusiveNode&&) = default;
    IntrusiveNode& operator=(IntrusiveNode&&) = default;
};

} // namespace mem
