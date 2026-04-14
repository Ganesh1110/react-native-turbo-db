#pragma once
#include "../cpp/SecureCryptoContext.h"
#include <string>

namespace secure_db {

// The iOS wrapper connecting C++ interfaces directly into Apple's CommonCrypto frameworks.
class SecureCryptoIOS : public SecureCryptoContext {
public:
    SecureCryptoIOS();
    ~SecureCryptoIOS() override;

    std::vector<uint8_t> encrypt(const uint8_t* plaintext, size_t length) override;
    std::vector<uint8_t> decrypt(const uint8_t* ciphertext, size_t length) override;

private:
    uint8_t static_key_[32]; // AES-256 Key
    uint8_t static_iv_[16];  // Initialization Vector
};

}
