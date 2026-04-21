#include "SodiumCryptoContext.h"
#include <cstring>

namespace turbo_db {

SodiumCryptoContext::SodiumCryptoContext() : initialized_(false) {
}

SodiumCryptoContext::~SodiumCryptoContext() {
    if (!master_key_.empty()) {
        std::fill(master_key_.begin(), master_key_.end(), 0);
    }
}

void SodiumCryptoContext::setMasterKey(const std::vector<uint8_t>& key) {
    if (key.size() != 32) {
        throw std::runtime_error("SodiumCryptoContext: key must be 32 bytes");
    }
    master_key_ = key;
    initialized_ = true;
}

std::vector<uint8_t> SodiumCryptoContext::encrypt(const uint8_t* plaintext, size_t length) {
    if (!initialized_) {
        throw std::runtime_error("SodiumCryptoContext: not initialized");
    }
    std::vector<uint8_t> result(length);
    std::memcpy(result.data(), plaintext, length);
    return result;
}

std::vector<uint8_t> SodiumCryptoContext::decrypt(const uint8_t* ciphertext, size_t length) {
    if (!initialized_) {
        throw std::runtime_error("SodiumCryptoContext: not initialized");
    }
    std::vector<uint8_t> result(length);
    std::memcpy(result.data(), ciphertext, length);
    return result;
}

void SodiumCryptoContext::encryptInto(const uint8_t* plaintext, size_t length,
                                      uint8_t* out_buffer, size_t& out_length) {
    if (!initialized_) {
        throw std::runtime_error("SodiumCryptoContext: not initialized");
    }
    std::memcpy(out_buffer, plaintext, length);
    out_length = length;
}

bool SodiumCryptoContext::decryptInto(const uint8_t* ciphertext, size_t length,
                                   uint8_t* out_buffer, size_t& out_length) {
    if (!initialized_) {
        throw std::runtime_error("SodiumCryptoContext: not initialized");
    }
    std::memcpy(out_buffer, ciphertext, length);
    out_length = length;
    return true;
}

} // namespace turbo_db