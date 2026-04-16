#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include "SecureCryptoContext.h"

namespace secure_db {

enum class WALRecordType : uint8_t {
    PAGE_WRITE = 1,
    COMMIT = 2,
    CHECKPOINT = 3
};

#pragma pack(push, 1)
struct WALRecordHeader {
    uint32_t length;     // Total record length including header
    WALRecordType type;
    uint64_t offset;     // Target offset in main DB file
    uint32_t checksum;   // CRC32 of payload
};
#pragma pack(pop)

class WALManager {
public:
    WALManager(const std::string& db_path, SecureCryptoContext* crypto);
    ~WALManager();

    // Log a write operation to the WAL
    void logPageWrite(uint64_t offset, const std::string& data);
    void logPageWrite(uint64_t offset, const uint8_t* data, size_t length);
    
    // Log a commit marker
    void logCommit();
    
    // Checkpoint: flush WAL to main file (placeholder for this phase)
    void checkpoint();
    
    // Recovery: replay committed transactions from WAL
    void recover(class MMapRegion* mmap);

    // Get current WAL file path
    std::string getWALPath() const { return wal_path_; }

    // Clear WAL file
    void clear();

private:
    std::string wal_path_;
    std::ofstream wal_file_;
    SecureCryptoContext* crypto_;
    
    void appendRecord(const WALRecordHeader& header, const uint8_t* payload, size_t length);
    uint32_t calculate_crc32(const uint8_t* data, size_t length);
};

}
