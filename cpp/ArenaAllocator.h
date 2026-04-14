#pragma once

#include <cstddef>
#include <vector>
#include <stdexcept>

namespace secure_db {

// A simple bump allocator to manage memory during extremely fast serialization,
// preventing standard heap allocation overhead (new/delete) in tight indexing loops.
class ArenaAllocator {
public:
    ArenaAllocator(size_t capacity) : capacity_(capacity), offset_(0) {
        buffer_ = new uint8_t[capacity_];
    }
    
    ~ArenaAllocator() {
        delete[] buffer_;
    }

    void* allocate(size_t bytes) {
        if (offset_ + bytes > capacity_) {
            // In a real system, you might chain arenas together.
            // For now, fail fast if we exceed buffer.
            throw std::bad_alloc(); 
        }
        void* ptr = buffer_ + offset_;
        offset_ += bytes;
        return ptr;
    }

    void reset() {
        offset_ = 0;
    }
    
    uint8_t* data() const { return buffer_; }
    size_t size() const { return offset_; }

private:
    size_t capacity_;
    size_t offset_;
    uint8_t* buffer_;
};

}
