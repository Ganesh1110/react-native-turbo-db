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
    if (mapped_) {
        close();
    }
    
    path_ = path;
    size_ = size;
    
    // Open or create the file
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd_ < 0) {
        throw std::runtime_error("MMapRegion: Failed to open file at " + path);
    }
    
    // Check file size, and resize if necessary using POSIX fstat
    struct stat st;
    if (::fstat(fd_, &st) < 0) {
        ::close(fd_);
        throw std::runtime_error("MMapRegion: Failed to fstat file");
    }
    
    if (static_cast<size_t>(st.st_size) < size_) {
        if (::ftruncate(fd_, size_) < 0) {
            ::close(fd_);
            throw std::runtime_error("MMapRegion: Failed to resize file");
        }
    }
    
    // Map into memory
    base_addr_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (base_addr_ == MAP_FAILED) {
        ::close(fd_);
        throw std::runtime_error("MMapRegion: Failed to mmap file");
    }
    
    // Lock critical pages into memory if possible. Limits depend on OS policies.
    // For iOS/Android, lock up to 1MB or so by default just to prevent swapping out 
    // the root B+ tree pages or the WAL headers.
    size_t lock_size = std::min(size_, static_cast<size_t>(1024 * 1024));
    ::mlock(base_addr_, lock_size);
    
    mapped_ = true;
}

void MMapRegion::close() {
    if (mapped_ && base_addr_ != MAP_FAILED) {
        ::munlock(base_addr_, std::min(size_, static_cast<size_t>(1024 * 1024)));
        ::msync(base_addr_, size_, MS_SYNC);
        ::munmap(base_addr_, size_);
        base_addr_ = MAP_FAILED;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    mapped_ = false;
}

void MMapRegion::sync(size_t offset, size_t length) {
    if (!mapped_) return;
    
    void* target_addr = static_cast<char*>(base_addr_) + offset;
    size_t sync_len = (length == 0) ? (size_ - offset) : length;
    
    ::msync(target_addr, sync_len, MS_SYNC);
}

void MMapRegion::write(size_t offset, const std::string& data) {
    if (!mapped_) throw std::runtime_error("MMapRegion: not mapped");
    if (offset + data.size() > size_) {
        throw std::runtime_error("MMapRegion: Write out of bounds");
    }
    
    char* dest = static_cast<char*>(base_addr_) + offset;
    std::memcpy(dest, data.data(), data.size());
}

std::string MMapRegion::read(size_t offset, size_t length) {
    if (!mapped_) throw std::runtime_error("MMapRegion: not mapped");
    if (offset + length > size_) {
        throw std::runtime_error("MMapRegion: Read out of bounds");
    }
    
    const char* src = static_cast<const char*>(base_addr_) + offset;
    return std::string(src, length);
}

}
