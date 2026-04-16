#include "MMapRegion.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>
#include <iostream>

namespace secure_db {

MMapRegion::MMapRegion() : base_addr_(MAP_FAILED), size_(0), fd_(-1), mapped_(false) {}

MMapRegion::~MMapRegion() {
    close();
}

void MMapRegion::init(const std::string& path, size_t size) {
    if (mapped_) close();
    
    path_ = path;
    size_ = size;
    
    // Open or create file
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd_ < 0) {
        throw std::runtime_error("MMapRegion: Failed to open file: " + path);
    }
    
    // Ensure file is large enough
    if (::ftruncate(fd_, size_) != 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("MMapRegion: Failed to truncate file");
    }
    
    // Map into memory
    base_addr_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (base_addr_ == MAP_FAILED) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("MMapRegion: Failed to mmap file");
    }
    
    // Lock critical pages into memory if possible. Limits depend on OS policies.
    // For iOS/Android, lock up to 1MB or so by default just to prevent swapping out 
    // basic B-Tree index pages.
    ::mlock(base_addr_, std::min((size_t)1024 * 1024, size_));
    
    mapped_ = true;
}

void MMapRegion::close() {
    if (mapped_) {
        ::munmap(base_addr_, size_);
        ::close(fd_);
        base_addr_ = MAP_FAILED;
        fd_ = -1;
        mapped_ = false;
    }
}

void MMapRegion::sync(size_t offset, size_t length, bool async) {
    if (!mapped_) return;
    
    void* target_addr = base_addr_;
    size_t sync_len = size_;
    
    if (length > 0) {
        target_addr = static_cast<char*>(base_addr_) + offset;
        sync_len = length;
    }
    
    if (async) {
        ::msync(target_addr, sync_len, MS_ASYNC);
    } else {
        ::msync(target_addr, sync_len, MS_SYNC);
    }
}

void MMapRegion::write(size_t offset, const std::string& data) {
    write(offset, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void MMapRegion::write(size_t offset, const uint8_t* data, size_t length) {
    if (!mapped_) throw std::runtime_error("MMapRegion: Not mapped");
    if (offset + length > size_) throw std::runtime_error("MMapRegion: Write out of bounds");
    
    std::memcpy(static_cast<char*>(base_addr_) + offset, data, length);
}

std::string MMapRegion::read(size_t offset, size_t length) {
    if (!mapped_) throw std::runtime_error("MMapRegion: Not mapped");
    if (offset + length > size_) throw std::runtime_error("MMapRegion: Read out of bounds");
    
    const char* src = static_cast<const char*>(base_addr_) + offset;
    return std::string(src, length);
}

const uint8_t* MMapRegion::get_address(size_t offset) const {
    if (!mapped_) return nullptr;
    if (offset >= size_) return nullptr;
    return static_cast<const uint8_t*>(base_addr_) + offset;
}

} // namespace secure_db
