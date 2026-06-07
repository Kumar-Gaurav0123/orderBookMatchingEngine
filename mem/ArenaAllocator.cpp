#include "ArenaAllocator.hpp"
#include <cstdlib>   // std::malloc, std::free
#include <cassert>
#include <cstdint>
#include <cstring>   // std::memset
#include <new>       // std::align (not used portably everywhere)
#include <algorithm>

namespace mem {

static constexpr std::size_t DEFAULT_ALIGNMENT = alignof(std::max_align_t);

ArenaAllocator::ArenaAllocator(std::size_t capacity)
    : capacity_(capacity),
      offset_(0),
      buffer_(nullptr)
{
    assert(capacity_ > 0 && "Arena capacity must be > 0");
    buffer_ = static_cast<std::uint8_t*>(std::malloc(capacity_));
    assert(buffer_ && "ArenaAllocator malloc failed");
    // Optional: zero memory for easier debugging
    // std::memset(buffer_, 0, capacity_);
}

ArenaAllocator::~ArenaAllocator() {
    std::free(buffer_);
    buffer_ = nullptr;
    offset_ = 0;
    capacity_ = 0;
}

void* ArenaAllocator::allocate(std::size_t size) {
    if (size == 0) return nullptr;

    // Align the allocation to DEFAULT_ALIGNMENT
    std::size_t align = DEFAULT_ALIGNMENT;
    std::size_t current = reinterpret_cast<std::size_t>(buffer_) + offset_;
    std::size_t mis = current & (align - 1);
    std::size_t adjustment = (mis == 0) ? 0 : (align - mis);

    // Check overflow and capacity
    if (offset_ + adjustment + size > capacity_) {
        return nullptr; // out of memory
    }

    std::size_t aligned_offset = offset_ + adjustment;
    void* ptr = buffer_ + aligned_offset;
    offset_ = aligned_offset + size;
    return ptr;
}

void ArenaAllocator::reset() {
    // NOTE: does not free memory, only resets bump pointer
    offset_ = 0;
}

} // namespace mem
