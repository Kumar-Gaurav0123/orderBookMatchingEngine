#include "OrderIdPool.hpp"

namespace obk {

OrderIdPool::OrderIdPool(uint64_t maxIds)
    : maxIds_(maxIds),
      nextId_(1)          // IDs start at 1 for clarity
{
    // Reserve capacity once; no further allocations in hot path
    freeIds_.reserve(maxIds_);
}

uint64_t OrderIdPool::allocate() noexcept {
    // Reuse an ID if available
    if (!freeIds_.empty()) {
        uint64_t id = freeIds_.back();
        freeIds_.pop_back();
        return id;
    }

    // Otherwise generate a new sequential ID
    // Debug-only: ensure we don't exceed maxIds
    assert(nextId_ <= maxIds_ && "OrderIdPool: out of available IDs!");

    return nextId_++;
}

void OrderIdPool::release(uint64_t id) noexcept {
    // Basic sanity check
    assert(id > 0 && "Cannot release ID zero");
    assert(id < nextId_ && "Cannot release ID that was never allocated");

    freeIds_.push_back(id);
}

} // namespace obk
