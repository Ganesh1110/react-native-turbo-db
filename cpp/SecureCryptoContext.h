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
};

}
