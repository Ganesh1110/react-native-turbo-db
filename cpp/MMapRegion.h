#pragma once

#include <string>
#include <stdexcept>

namespace secure_db {

class MMapRegion {
public:
    MMapRegion();
    ~MMapRegion();

    // Map a file to memory, up to a certain size
    void init(const std::string& path, size_t size);
    
    // Unmap memory and close
    void close();

    // Sync a region of memory back to disk
    void sync(size_t offset = 0, size_t length = 0);

    // Read/Write accessors
    void write(size_t offset, const std::string& data);
    std::string read(size_t offset, size_t length);

    // Property accessors
    std::string getPath() const { return path_; }
    size_t getSize() const { return size_; }

private:
    void* base_addr_;
    size_t size_;
    int fd_;
    std::string path_;
    bool mapped_;
};

}
