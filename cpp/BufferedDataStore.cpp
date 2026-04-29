#include "BufferedDataStore.h"
#include <algorithm>

namespace turbo_db {

BufferedDataStore::BufferedDataStore()
    : next_offset_(1024 * 1024) {
    worker_ = std::thread(&BufferedDataStore::workerThread, this);
}

BufferedDataStore::~BufferedDataStore() {
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        stop_worker_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void BufferedDataStore::setWriter(
    std::function<size_t(const std::string&, const std::vector<uint8_t>&, bool)> writer) {
    writer_ = std::move(writer);
}

bool BufferedDataStore::write(const std::string& key, const std::vector<uint8_t>& data, bool is_tombstone) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto it = key_to_buffer_idx_.find(key);
    size_t new_bytes = data.size();

    if (it != key_to_buffer_idx_.end()) {
        size_t idx = it->second;
        size_t old_bytes = write_buffer_[idx].data.size();
        write_buffer_[idx] = {key, data, is_tombstone, 0};
        buffer_bytes_ += new_bytes - old_bytes;
    } else {
        write_buffer_.push_back({key, data, is_tombstone, 0});
        key_to_buffer_idx_[key] = write_buffer_.size() - 1;
        buffer_bytes_ += new_bytes;
    }

    if (buffer_bytes_ >= FLUSH_THRESHOLD) {
        flush_requested_ = true;
        cv_.notify_one();
    }

    return true;
}

std::vector<uint8_t> BufferedDataStore::read(const std::string& key) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto it = key_to_buffer_idx_.find(key);
    if (it != key_to_buffer_idx_.end()) {
        const auto& record = write_buffer_[it->second];
        if (!record.is_tombstone) {
            return record.data;
        }
    }
    return {};
}

bool BufferedDataStore::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    auto it = key_to_buffer_idx_.find(key);
    if (it != key_to_buffer_idx_.end()) {
        return !write_buffer_[it->second].is_tombstone;
    }
    return false;
}

void BufferedDataStore::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto it = key_to_buffer_idx_.find(key);
    if (it != key_to_buffer_idx_.end()) {
        size_t idx = it->second;
        buffer_bytes_ -= write_buffer_[idx].data.size();
        write_buffer_[idx].is_tombstone = true;
        write_buffer_[idx].data.clear();
    }
}

void BufferedDataStore::flush() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    flush_requested_ = true;
    cv_.notify_one();
}

void BufferedDataStore::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    write_buffer_.clear();
    key_to_buffer_idx_.clear();
    buffer_bytes_ = 0;
}

size_t BufferedDataStore::getBufferSizeBytes() const {
    return buffer_bytes_.load();
}

size_t BufferedDataStore::getBufferCount() const {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return write_buffer_.size();
}

void BufferedDataStore::workerThread() {
    while (!stop_worker_) {
        std::deque<BufferedRecord> to_flush;
        {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            cv_.wait(lock, [this] {
                return stop_worker_ || flush_requested_ || buffer_bytes_ >= FLUSH_THRESHOLD;
            });

            if (stop_worker_ && write_buffer_.empty()) break;

            if (!write_buffer_.empty()) {
                flushing_buffer_ = std::move(write_buffer_);
                write_buffer_ = std::deque<BufferedRecord>();
                key_to_buffer_idx_.clear();
                to_flush = std::move(flushing_buffer_);
                buffer_bytes_ = 0;
            }
            flush_requested_ = false;
        }

        if (!to_flush.empty() && writer_) {
            for (const auto& record : to_flush) {
                writer_(record.key, record.data, record.is_tombstone);
            }
        }
    }
}

void BufferedDataStore::flushBatch(std::deque<BufferedRecord>&& batch) {
    if (writer_) {
        for (const auto& record : batch) {
            writer_(record.key, record.data, record.is_tombstone);
        }
    }
}

}