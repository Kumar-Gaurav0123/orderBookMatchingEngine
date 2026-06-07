#pragma once

#include <cstdint>
#include <vector>
#include <cassert>

namespace obk {

// A fast O(1) ID allocator.
// IDs are handed out sequentially and recycled when orders are cancelled.
//
// Notes:
//  - NOT thread-safe (your engine loop is single-threaded).
//  - No heap allocations during runtime after constructor.
//  - Recycling uses a simple LIFO free stack for performance.

class OrderIdPool {
public:
    explicit OrderIdPool(uint64_t maxIds)
        : maxIds_(maxIds),
          nextId_(1)      // start IDs from 1 (0 sometimes reserved)
    {
        freeIds_.reserve(maxIds);
    }

    // Allocate a new ID (O(1))
    uint64_t allocate() noexcept {
        if (!freeIds_.empty()) {
            uint64_t id = freeIds_.back();
            freeIds_.pop_back();
            return id;
        }

        // Otherwise issue a new ID
        assert(nextId_ <= maxIds_ && "OrderIdPool: out of IDs!");
        return nextId_++;
    }

    // Recycle ID back into pool
    void release(uint64_t id) noexcept {
        assert(id > 0 && id < nextId_ && "Invalid order ID release");
        freeIds_.push_back(id);
    }

    // Debug info (optional)
    [[nodiscard]] uint64_t nextId() const noexcept { return nextId_; }
    [[nodiscard]] uint64_t freeCount() const noexcept { return freeIds_.size(); }

private:
    uint64_t maxIds_;           // maximum ID range
    uint64_t nextId_;           // next fresh ID to issue
    std::vector<uint64_t> freeIds_; // recycled ID list
};

} // namespace obk
