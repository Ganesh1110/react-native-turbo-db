#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace turbo_db {

class Compression {
public:
    enum class Algorithm {
        None,
        LZ4,
        Zstd
    };

    static std::vector<uint8_t> compress(
        const uint8_t* data,
        size_t len,
        Algorithm algo = Algorithm::LZ4);

    static std::vector<uint8_t> decompress(
        const uint8_t* compressed_data,
        size_t compressed_len,
        size_t original_len,
        Algorithm algo = Algorithm::LZ4);

    static bool canCompress(size_t data_size);

    static constexpr size_t MIN_SIZE_FOR_COMPRESSION = 256;
    static constexpr double COMPRESSION_RATIO_THRESHOLD = 0.7;
};

class CompressedBuffer {
public:
    std::vector<uint8_t> data;
    size_t original_size;
    Compression::Algorithm algorithm;

    bool isCompressed() const { return algorithm != Compression::Algorithm::None; }
};

}