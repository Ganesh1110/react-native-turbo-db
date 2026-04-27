#pragma once

#include <string>
#include <vector>

namespace turbo_db {

class KeyManagerIOS {
public:
    static std::vector<uint8_t> getOrGenerateMasterKey(const std::string& alias);
    static bool deleteMasterKey(const std::string& alias);

    // ── Hardware Secure Enclave Item Storage ──────────────────────────────
    // Stores/retrieves opaque string blobs (e.g. PIN hashes) directly in the
    // iOS Keychain, protected by the Secure Enclave EC key using ECIES.
    // Data is accessible only on this device when unlocked.
    static bool setSecureItem(const std::string& key, const std::string& value);
    static std::string getSecureItem(const std::string& key);   // "" if not found
    static bool deleteSecureItem(const std::string& key);

private:
    static SecKeyRef getSecureEnclaveKey();
    static std::vector<uint8_t> wrapMasterKey(const std::vector<uint8_t>& masterKey, SecKeyRef publicKey);
    static std::vector<uint8_t> unwrapMasterKey(const std::vector<uint8_t>& wrappedKey, SecKeyRef privateKey);
};

}