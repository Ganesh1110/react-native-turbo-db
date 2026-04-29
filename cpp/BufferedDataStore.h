#pragma once
#include <deque>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <cstdint>

namespace turbo_db {

struct BufferedRecord {
    std::string key;
    std::vector<uint8_t> data;
    bool is_tombstone;
    size_t offset;
};

class BufferedDataStore {
public:
    static constexpr size_t MAX_BUFFER_BYTES = 2 * 1024 * 1024; // 2MB buffer
    static constexpr size_t FLUSH_THRESHOLD = 512 * 1024; // 512KB

    BufferedDataStore();
    ~BufferedDataStore();

    void setWriter(std::function<size_t(const std::string&, const std::vector<uint8_t>&, bool)> writer);

    bool write(const std::string& key, const std::vector<uint8_t>& data, bool is_tombstone);
    std::vector<uint8_t> read(const std::string& key);
    bool contains(const std::string& key) const;
    void remove(const std::string& key);

    void flush();
    void clear();

    size_t getBufferSizeBytes() const;
    size_t getBufferCount() const;

private:
    void workerThread();
    void flushBatch(std::deque<BufferedRecord>&& batch);

    std::function<size_t(const std::string&, const std::vector<uint8_t>&, bool)> writer_;

    std::deque<BufferedRecord> write_buffer_;
    std::deque<BufferedRecord> flushing_buffer_;
    std::unordered_map<std::string, size_t> key_to_buffer_idx_;
    mutable std::mutex buffer_mutex_;
    std::thread worker_;
    std::condition_variable cv_;
    std::atomic<bool> stop_worker_{false};
    std::atomic<bool> flush_requested_{false};
    std::atomic<size_t> buffer_bytes_{0};
    size_t next_offset_;
};

}