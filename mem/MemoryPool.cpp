#include "MemoryPool.hpp"
#include <cstdlib>      // std::malloc, std::free
#include <cstring>      // std::memset
#include <cassert>

namespace mem {

MemoryPool::MemoryPool(std::size_t blockSize, std::size_t blockCount)
    : blockSize_(blockSize),
      blockCount_(blockCount),
      buffer_(nullptr),
      freeList_(nullptr),
      freeBlocks_(blockCount)
{
    // Ensure block size can contain an IntrusiveNode
    assert(blockSize_ >= sizeof(IntrusiveNode) && "blockSize too small!");

    // Allocate raw memory
    buffer_ = static_cast<std::uint8_t*>(std::malloc(blockSize_ * blockCount_));
    assert(buffer_ && "MemoryPool malloc failed");

    // Optional: zero out (not required in production)
    // std::memset(buffer_, 0, blockSize_ * blockCount_);

    initializeFreeList();
}

MemoryPool::~MemoryPool() {
    std::free(buffer_);
    buffer_ = nullptr;
    freeList_ = nullptr;
}

// Build the intrusive free list by linking all blocks.
void MemoryPool::initializeFreeList() {
    freeList_ = nullptr;

    for (std::size_t i = 0; i < blockCount_; ++i) {
        // Pointer to this block
        std::uint8_t* blockPtr = buffer_ + i * blockSize_;

        // Use block memory as an IntrusiveNode
        auto* node = reinterpret_cast<IntrusiveNode*>(blockPtr);

        // Push-front into free list
        node->next = freeList_;
        freeList_ = node;
    }
}

void* MemoryPool::allocate() {
    if (!freeList_) {
        // Out of memory
        return nullptr;
    }

    // Pop from free list
    IntrusiveNode* node = freeList_;
    freeList_ = freeList_->next;

    freeBlocks_--;
    return reinterpret_cast<void*>(node);
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) return;

    // Push ptr back to free list
    auto* node = reinterpret_cast<IntrusiveNode*>(ptr);

    node->next = freeList_;
    freeList_ = node;

    freeBlocks_++;
}

} // namespace mem
