#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace secure_db {

// The abstract boundary that protects the raw MMap logic from platform-specific
// hardware cryptography endpoints (like CommonCrypto or Java KeyStore JNI bindings).
// Keys NEVER traverse this interface.
class SecureCryptoContext {
public:
    virtual ~SecureCryptoContext() = default;

    // Compiles a plaintext buffer into an encrypted ciphertext vector 
    // including any padding/tags required by the underlying AES hardware.
    virtual std::vector<uint8_t> encrypt(const uint8_t* plaintext, size_t length) = 0;

    // Reverses a ciphertext payload extracted from the database natively 
    // returning the pure binary string.
    virtual std::vector<uint8_t> decrypt(const uint8_t* ciphertext, size_t length) = 0;

    // NEW: Zero-copy output via provided buffer
    virtual void encryptInto(const uint8_t* plaintext, size_t length, 
                             uint8_t* out_buffer, size_t& out_length) = 0;
    virtual bool decryptInto(const uint8_t* ciphertext, size_t length,
                             uint8_t* out_buffer, size_t& out_length) = 0;

    // LRU-cached decryption by record offset
    virtual std::vector<uint8_t> decryptAtOffset(const uint8_t* ciphertext, size_t length, uint64_t record_offset) {
        return decrypt(ciphertext, length);
    }
    virtual bool decryptIntoAtOffset(const uint8_t* ciphertext, size_t length, uint64_t record_offset,
                                     uint8_t* out_buffer, size_t& out_length) {
        return decryptInto(ciphertext, length, out_buffer, out_length);
    }
};

}
