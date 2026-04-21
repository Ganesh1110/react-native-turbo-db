#pragma once

#include "SecureCryptoContext.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <mutex>

namespace turbo_db {

class SodiumCryptoContext : public SecureCryptoContext {
public:
    SodiumCryptoContext();
    ~SodiumCryptoContext() override;

    SodiumCryptoContext(const SodiumCryptoContext&) = delete;
    SodiumCryptoContext& operator=(const SodiumCryptoContext&) = delete;

    void setMasterKey(const std::vector<uint8_t>& key);

    std::vector<uint8_t> encrypt(const uint8_t* plaintext, size_t length) override;

    std::vector<uint8_t> decrypt(const uint8_t* ciphertext, size_t length) override;

    void encryptInto(const uint8_t* plaintext, size_t length,
                    uint8_t* out_buffer, size_t& out_length) override;

    bool decryptInto(const uint8_t* ciphertext, size_t length,
                    uint8_t* out_buffer, size_t& out_length) override;

private:
    std::vector<uint8_t> master_key_;
    bool initialized_;
};

} // namespace turbo_db