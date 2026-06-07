#pragma once

#include <cstddef>
#include <cstdint>

namespace mem {

// A simple bump-pointer allocator.
// Extremely fast: allocate = pointer bump, deallocate = no-op.
// Best used for temporary buffers, snapshots, feed parsing, etc.
//
// NOT for long-lived hot-path objects like Orders.

class ArenaAllocator {
public:
    // Creates an arena of given size (in bytes)
    explicit ArenaAllocator(std::size_t capacity);
    ~ArenaAllocator();

    // Bump allocate: returns nullptr if out of memory
    void* allocate(std::size_t size);

    // Reset entire arena (all memory reclaimed)
    void reset();

    // Read-only helpers
    std::size_t used() const { return offset_; }
    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;    // total bytes
    std::size_t offset_;      // current bump position
    std::uint8_t* buffer_;    // raw memory
};

} // namespace mem
