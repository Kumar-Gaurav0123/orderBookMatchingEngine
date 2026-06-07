#pragma once

#include <cstddef>
#include <cstdint>
#include "IntrusiveNode.hpp"

namespace mem {

// A fixed-size block allocator using an intrusive free list.
// Very fast: O(1) allocate and deallocate, no fragmentation.
//
// NOTE: MemoryPool only manages fixed-size blocks. The caller
//       must ensure all allocated objects fit into blockSize_.

class MemoryPool {
public:
    // Construct a pool of (blockCount) blocks, each blockSize bytes.
    MemoryPool(std::size_t blockSize, std::size_t blockCount);
    ~MemoryPool();

    // Allocate one fixed-size block. Returns nullptr if empty.
    void* allocate();

    // Return a block back to the free list.
    void deallocate(void* ptr);

    // For debugging / telemetry
    std::size_t freeBlocks() const { return freeBlocks_; }

private:
    std::size_t blockSize_;
    std::size_t blockCount_;

    std::uint8_t* buffer_;        // raw memory buffer
    mem::IntrusiveNode* freeList_; // head of free-list

    std::size_t freeBlocks_;      // optional: track available blocks

    // Builds a linked list of free blocks
    void initializeFreeList();
};

} // namespace mem
