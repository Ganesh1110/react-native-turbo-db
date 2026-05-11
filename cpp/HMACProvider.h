#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// HMACProvider — Portable HMAC-SHA256 (R5: Security & Enterprise)
//
// Provides a self-contained, zero-dependency HMAC-SHA256 implementation for
// TurboDB's per-record integrity tagging. Replaces CRC32 in secure mode.
//
// Key properties:
//   • SHA-256 core is public domain (FIPS 180-4 compliant)
//   • No external libraries (libsodium, OpenSSL) required
//   • Thread-safe — all state is stack-local
//   • Output: 32-byte (256-bit) HMAC tag
//
// Usage:
//   auto tag = turbo_db::hmac_sha256(key_bytes, key_len, msg, msg_len);
//   bool ok  = turbo_db::hmac_sha256_verify(key_bytes, key_len, msg, msg_len, stored_tag);
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstddef>
#include <array>
#include <vector>
#include <string>

namespace turbo_db {

// Size of an HMAC-SHA256 tag in bytes
static constexpr size_t HMAC_SHA256_SIZE = 32;

// Type alias for a 32-byte HMAC tag
using HMACTag = std::array<uint8_t, HMAC_SHA256_SIZE>;

/**
 * Compute HMAC-SHA256 over a message using the given key.
 *
 * @param key     Raw key bytes (any length)
 * @param key_len Length of key in bytes
 * @param msg     Message data
 * @param msg_len Length of message in bytes
 * @return        32-byte HMAC tag
 */
HMACTag hmac_sha256(const uint8_t* key, size_t key_len,
                    const uint8_t* msg, size_t msg_len);

/**
 * Verify a stored HMAC-SHA256 tag against a freshly computed one.
 * Uses a constant-time comparison to prevent timing attacks.
 *
 * @param key         Raw key bytes
 * @param key_len     Length of key in bytes
 * @param msg         Message data
 * @param msg_len     Length of message in bytes
 * @param stored_tag  32-byte tag to compare against
 * @return            true if tags match, false if tampered/invalid
 */
bool hmac_sha256_verify(const uint8_t* key, size_t key_len,
                        const uint8_t* msg, size_t msg_len,
                        const uint8_t* stored_tag);

/**
 * Helper: derive an HMAC key from an arbitrary string (e.g. from the
 * crypto context key material). Returns 32 bytes suitable for HMAC use.
 * Uses SHA-256 of the input string if it is not already 32 bytes.
 */
HMACTag derive_hmac_key(const std::string& key_material);

} // namespace turbo_db
