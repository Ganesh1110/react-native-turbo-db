#pragma once
#include <jsi/jsi.h>
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace turbo_db {

class ValueCache {
public:
    static constexpr size_t DEFAULT_CAPACITY = 256;

    explicit ValueCache(size_t capacity = DEFAULT_CAPACITY);

    bool get(const std::string& key, facebook::jsi::Value& out, facebook::jsi::Runtime& runtime);
    void put(const std::string& key, const facebook::jsi::Value& value, facebook::jsi::Runtime& runtime);
    void remove(const std::string& key);
    void clear();
    size_t size() const;
    bool contains(const std::string& key) const;

    struct [[nodiscard]] CacheStats {
        size_t hits;
        size_t misses;
        size_t size;
        double hitRate() const { return hits + misses > 0 ? static_cast<double>(hits) / (hits + misses) : 0.0; }
    };
    CacheStats getStats() const;

private:
    struct CacheEntry {
        std::string key;
        facebook::jsi::Value value;
        std::vector<uint8_t> serialized;
    };

    size_t capacity_;
    mutable std::mutex mutex_;
    std::list<CacheEntry> lru_list_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> map_;
    mutable CacheStats stats_{0, 0, 0};
};

class ZeroCopyDecoder {
public:
    static bool canZeroCopy(const uint8_t* data, size_t len);

    static facebook::jsi::Value decode(
        facebook::jsi::Runtime& runtime,
        const uint8_t* data,
        size_t len);
};

}