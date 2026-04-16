#pragma once

#include "SecureCryptoContext.h"
#include "sodium.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <mutex>

namespace secure_db {

/**
 * @brief Unified Crypto Context using Libsodium XChaCha20-Poly1305.
 * Provides AEAD (Authenticated Encryption with Associated Data) for 
 * block-level tamper resistance across all platforms.
 */
class SodiumCryptoContext : public SecureCryptoContext {
public:
    SodiumCryptoContext();
    ~SodiumCryptoContext() override;

    // Prevents accidental copying of sensitive key material
    SodiumCryptoContext(const SodiumCryptoContext&) = delete;
    SodiumCryptoContext& operator=(const SodiumCryptoContext&) = delete;

    /**
     * @brief Sets the 32-byte master key retrieved from OS Keystore/Keychain.
     */
    void setMasterKey(const std::vector<uint8_t>& key);

    /**
     * @brief Encrypts plaintext and prepends a random 24-byte nonce.
     * Output format: [NONCE (24b)][CIPHERTEXT (nb)][MAC (16b)]
     */
    std::vector<uint8_t> encrypt(const uint8_t* plaintext, size_t length) override;

    /**
     * @brief Decrypts and authenticates the provided ciphertext.
     * @throw std::runtime_error if authentication tag verification fails (tampering).
     */
    std::vector<uint8_t> decrypt(const uint8_t* ciphertext, size_t length) override;

    void encryptInto(const uint8_t* plaintext, size_t length, 
                     uint8_t* out_buffer, size_t& out_length) override;
    
    bool decryptInto(const uint8_t* ciphertext, size_t length,
                     uint8_t* out_buffer, size_t& out_length) override;

private:
    static void ensureInitialized();
    
    uint8_t master_key_[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    bool key_is_set_ = false;
    mutable std::mutex key_mutex_;
};

} // namespace secure_db
