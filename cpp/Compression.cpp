#include "Compression.h"
#include <zlib.h>
#include <cstring>

namespace turbo_db {

bool Compression::canCompress(size_t data_size) {
    return data_size >= MIN_SIZE_FOR_COMPRESSION;
}

std::vector<uint8_t> Compression::compress(
    const uint8_t* data,
    size_t len,
    Algorithm algo)
{
    if (len < MIN_SIZE_FOR_COMPRESSION) {
        return {};
    }

    if (algo == Algorithm::LZ4 || algo == Algorithm::Zstd) {
        return {};
    }

    uLongf dest_len = len + (len >> 12) + 128;
    std::vector<uint8_t> compressed(dest_len);

    int result = compress2(
        compressed.data(),
        &dest_len,
        data,
        static_cast<uLong>(len),
        Z_DEFAULT_COMPRESSION);

    if (result != Z_OK || dest_len >= len) {
        return {};
    }

    compressed.resize(dest_len);
    return compressed;
}

std::vector<uint8_t> Compression::decompress(
    const uint8_t* compressed_data,
    size_t compressed_len,
    size_t original_len,
    Algorithm algo)
{
    if (algo == Algorithm::LZ4 || algo == Algorithm::Zstd) {
        return {};
    }

    std::vector<uint8_t> decompressed(original_len);
    uLongf dest_len = static_cast<uLongf>(original_len);

    int result = uncompress(
        decompressed.data(),
        &dest_len,
        compressed_data,
        static_cast<uLong>(compressed_len));

    if (result != Z_OK) {
        return {};
    }

    decompressed.resize(dest_len);
    return decompressed;
}

}