#include "ValueCache.h"
#include "BinarySerializer.h"
#include <cstring>

namespace turbo_db {

ValueCache::ValueCache(size_t capacity)
    : capacity_(capacity > 0 ? capacity : 1) {}

bool ValueCache::get(const std::string& key, facebook::jsi::Value& out, facebook::jsi::Runtime& runtime) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) {
        stats_.misses++;
        return false;
    }
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    auto& entry = *it->second;
    if (!entry.serialized.empty()) {
        auto result = BinarySerializer::deserialize(runtime, entry.serialized.data(), entry.serialized.size());
        entry.value = std::move(result.first);
    }
    out = std::move(entry.value);
    stats_.hits++;
    return true;
}

void ValueCache::put(const std::string& key, const facebook::jsi::Value& value, facebook::jsi::Runtime& runtime) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it != map_.end()) {
        it->second->value = facebook::jsi::Value(runtime, value);
        it->second->serialized.clear();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        return;
    }
    if (lru_list_.size() >= capacity_) {
        auto lru = std::prev(lru_list_.end());
        map_.erase(lru->key);
        lru_list_.erase(lru);
    }
    CacheEntry entry;
    entry.key = key;
    entry.value = facebook::jsi::Value(runtime, value);
    lru_list_.push_front(std::move(entry));
    map_[key] = lru_list_.begin();
    stats_.size = lru_list_.size();
}

void ValueCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it != map_.end()) {
        lru_list_.erase(it->second);
        map_.erase(it);
        stats_.size = lru_list_.size();
    }
}

void ValueCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lru_list_.clear();
    map_.clear();
    stats_ = {0, 0, 0};
}

size_t ValueCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lru_list_.size();
}

bool ValueCache::contains(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_.find(key) != map_.end();
}

ValueCache::CacheStats ValueCache::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

bool ZeroCopyDecoder::canZeroCopy(const uint8_t* data, size_t len) {
    if (len < 1) return false;
    BinaryType type = static_cast<BinaryType>(data[0]);
    switch (type) {
        case BinaryType::Null:
        case BinaryType::Boolean:
        case BinaryType::Number:
            return true;
        case BinaryType::String:
            return len >= 5;
        default:
            return false;
    }
}

facebook::jsi::Value ZeroCopyDecoder::decode(facebook::jsi::Runtime& runtime, const uint8_t* data, size_t len) {
    if (len < 1) return facebook::jsi::Value::undefined();
    BinaryType type = static_cast<BinaryType>(data[0]);
    switch (type) {
        case BinaryType::Null:
            return facebook::jsi::Value::null();
        case BinaryType::Boolean:
            return len >= 2 ? facebook::jsi::Value(data[1] != 0) : facebook::jsi::Value::undefined();
        case BinaryType::Number:
            if (len >= 9) {
                double val;
                std::memcpy(&val, data + 1, sizeof(double));
                return facebook::jsi::Value(val);
            }
            return facebook::jsi::Value::undefined();
        case BinaryType::String:
            if (len >= 5) {
                uint32_t str_len;
                std::memcpy(&str_len, data + 1, sizeof(uint32_t));
                if (len >= 5 + str_len) {
                    return facebook::jsi::String::createFromUtf8(runtime, data + 5, str_len);
                }
            }
            return facebook::jsi::Value::undefined();
        default:
            return facebook::jsi::Value::undefined();
    }
}

}