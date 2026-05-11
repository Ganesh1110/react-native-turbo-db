// ─────────────────────────────────────────────────────────────────────────────
// HMACProvider — Portable HMAC-SHA256 (R5: Security & Enterprise)
//
// Self-contained implementation. No external dependencies.
// SHA-256 core adapted from the public-domain reference implementation.
// ─────────────────────────────────────────────────────────────────────────────

#include "HMACProvider.h"
#include <cstring>
#include <algorithm>

namespace turbo_db {

// ─────────────────────────────────────────────────────────────────────────────
// Portable SHA-256 Core (FIPS 180-4)
// ─────────────────────────────────────────────────────────────────────────────

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct SHA256State {
    uint32_t h[8];
    uint64_t total_bits;
    uint8_t  buf[64];
    uint32_t buf_len;
};

static inline uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

static void sha256_init(SHA256State& s) {
    s.h[0] = 0x6a09e667u;
    s.h[1] = 0xbb67ae85u;
    s.h[2] = 0x3c6ef372u;
    s.h[3] = 0xa54ff53au;
    s.h[4] = 0x510e527fu;
    s.h[5] = 0x9b05688cu;
    s.h[6] = 0x1f83d9abu;
    s.h[7] = 0x5be0cd19u;
    s.total_bits = 0;
    s.buf_len    = 0;
}

static void sha256_compress(SHA256State& s, const uint8_t* block) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i*4+0] << 24) | ((uint32_t)block[i*4+1] << 16)
             | ((uint32_t)block[i*4+2] <<  8) | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2],  17) ^ rotr32(w[i-2],  19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=s.h[0], b=s.h[1], c=s.h[2], d=s.h[3];
    uint32_t e=s.h[4], f=s.h[5], g=s.h[6], h=s.h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1    = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch    = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + SHA256_K[i] + w[i];
        uint32_t S0    = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    s.h[0]+=a; s.h[1]+=b; s.h[2]+=c; s.h[3]+=d;
    s.h[4]+=e; s.h[5]+=f; s.h[6]+=g; s.h[7]+=h;
}

static void sha256_update(SHA256State& s, const uint8_t* data, size_t len) {
    s.total_bits += (uint64_t)len * 8;
    while (len > 0) {
        uint32_t to_copy = 64 - s.buf_len;
        if (to_copy > (uint32_t)len) to_copy = (uint32_t)len;
        std::memcpy(s.buf + s.buf_len, data, to_copy);
        s.buf_len += to_copy;
        data += to_copy;
        len  -= to_copy;
        if (s.buf_len == 64) {
            sha256_compress(s, s.buf);
            s.buf_len = 0;
        }
    }
}

static std::array<uint8_t, 32> sha256_final(SHA256State& s) {
    uint64_t total = s.total_bits;
    // Append 0x80 padding
    uint8_t pad = 0x80;
    sha256_update(s, &pad, 1);
    // Pad with zeros until buf_len == 56
    uint8_t zero = 0;
    while (s.buf_len != 56) {
        sha256_update(s, &zero, 1);
    }
    // Append 64-bit big-endian bit count
    uint8_t len_bytes[8];
    for (int i = 7; i >= 0; i--) {
        len_bytes[i] = (uint8_t)(total & 0xFF);
        total >>= 8;
    }
    sha256_update(s, len_bytes, 8);

    std::array<uint8_t, 32> digest;
    for (int i = 0; i < 8; i++) {
        digest[i*4+0] = (uint8_t)(s.h[i] >> 24);
        digest[i*4+1] = (uint8_t)(s.h[i] >> 16);
        digest[i*4+2] = (uint8_t)(s.h[i] >>  8);
        digest[i*4+3] = (uint8_t)(s.h[i]       );
    }
    return digest;
}

static std::array<uint8_t, 32> sha256(const uint8_t* data, size_t len) {
    SHA256State s;
    sha256_init(s);
    sha256_update(s, data, len);
    return sha256_final(s);
}

// ─────────────────────────────────────────────────────────────────────────────
// HMAC-SHA256
// HMAC(K,m) = SHA256((K' XOR opad) || SHA256((K' XOR ipad) || m))
// where K' = SHA256(K) if len(K)>64, else K padded with zeros to 64 bytes.
// ─────────────────────────────────────────────────────────────────────────────

HMACTag hmac_sha256(const uint8_t* key, size_t key_len,
                    const uint8_t* msg, size_t msg_len)
{
    uint8_t k_prime[64];
    std::memset(k_prime, 0, 64);

    if (key_len > 64) {
        // Hash the key if it's too long
        auto hk = sha256(key, key_len);
        std::memcpy(k_prime, hk.data(), 32);
    } else {
        std::memcpy(k_prime, key, key_len);
    }

    // ipad = k_prime XOR 0x36
    uint8_t ipad[64], opad[64];
    for (int i = 0; i < 64; i++) {
        ipad[i] = k_prime[i] ^ 0x36;
        opad[i] = k_prime[i] ^ 0x5c;
    }

    // inner = SHA256(ipad || msg)
    SHA256State inner;
    sha256_init(inner);
    sha256_update(inner, ipad, 64);
    sha256_update(inner, msg, msg_len);
    auto inner_hash = sha256_final(inner);

    // outer = SHA256(opad || inner_hash)
    SHA256State outer;
    sha256_init(outer);
    sha256_update(outer, opad, 64);
    sha256_update(outer, inner_hash.data(), 32);
    return sha256_final(outer);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constant-time comparison to prevent timing attacks
// ─────────────────────────────────────────────────────────────────────────────
bool hmac_sha256_verify(const uint8_t* key, size_t key_len,
                        const uint8_t* msg, size_t msg_len,
                        const uint8_t* stored_tag)
{
    auto computed = hmac_sha256(key, key_len, msg, msg_len);
    // Constant-time comparison: XOR all bytes, any difference sets diff
    uint8_t diff = 0;
    for (size_t i = 0; i < HMAC_SHA256_SIZE; i++) {
        diff |= (computed[i] ^ stored_tag[i]);
    }
    return diff == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Derive a fixed-length HMAC key from an arbitrary string
// ─────────────────────────────────────────────────────────────────────────────
HMACTag derive_hmac_key(const std::string& key_material) {
    return sha256(reinterpret_cast<const uint8_t*>(key_material.data()),
                  key_material.size());
}

} // namespace turbo_db
