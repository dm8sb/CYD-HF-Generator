#include "pbkdf2.h"

#include <string.h>

namespace Crypto {

namespace {

uint32_t rol(uint32_t v, unsigned n) { return (v << n) | (v >> (32 - n)); }

struct Sha1 {
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    uint8_t block[64] {};
    size_t used = 0;
    uint64_t total = 0;

    void compress() {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t)block[4 * i] << 24 | (uint32_t)block[4 * i + 1] << 16 | (uint32_t)block[4 * i + 2] << 8 |
                   (uint32_t)block[4 * i + 3];
        }
        for (int i = 16; i < 80; ++i) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rol(b, 30);
            b = a;
            a = t;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    }

    void update(const uint8_t* p, size_t n) {
        total += n;
        while (n > 0) {
            const size_t take = (64 - used) < n ? (64 - used) : n;
            memcpy(block + used, p, take);
            used += take;
            p += take;
            n -= take;
            if (used == 64) {
                compress();
                used = 0;
            }
        }
    }

    void finish(uint8_t out[SHA1_LEN]) {
        const uint64_t bits = total * 8;
        const uint8_t one = 0x80;
        update(&one, 1);
        const uint8_t zero = 0;
        while (used != 56) update(&zero, 1);
        uint8_t len[8];
        for (int i = 0; i < 8; ++i) len[i] = (uint8_t)(bits >> (56 - 8 * i));
        update(len, 8);
        for (int i = 0; i < 5; ++i) {
            out[4 * i] = (uint8_t)(h[i] >> 24);
            out[4 * i + 1] = (uint8_t)(h[i] >> 16);
            out[4 * i + 2] = (uint8_t)(h[i] >> 8);
            out[4 * i + 3] = (uint8_t)h[i];
        }
    }
};

} // namespace

void sha1(const uint8_t* data, size_t len, uint8_t out[SHA1_LEN]) {
    Sha1 s;
    s.update(data, len);
    s.finish(out);
}

void hmacSha1(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t len, uint8_t out[SHA1_LEN]) {
    uint8_t k[64] {};
    if (keyLen > 64) sha1(key, keyLen, k);
    else memcpy(k, key, keyLen);
    uint8_t pad[64];
    for (int i = 0; i < 64; ++i) pad[i] = k[i] ^ 0x36;
    uint8_t inner[SHA1_LEN];
    Sha1 s;
    s.update(pad, 64);
    s.update(data, len);
    s.finish(inner);
    for (int i = 0; i < 64; ++i) pad[i] = k[i] ^ 0x5C;
    Sha1 o;
    o.update(pad, 64);
    o.update(inner, SHA1_LEN);
    o.finish(out);
}

void pbkdf2HmacSha1(const uint8_t* password, size_t passwordLen, const uint8_t* salt, size_t saltLen,
                    uint32_t iterations, uint8_t* out, size_t outLen) {
    uint8_t msg[64 + 4];
    if (saltLen > 64) saltLen = 64; // SSIDs have at most 32 bytes
    memcpy(msg, salt, saltLen);
    for (uint32_t block = 1; outLen > 0; ++block) {
        msg[saltLen] = (uint8_t)(block >> 24);
        msg[saltLen + 1] = (uint8_t)(block >> 16);
        msg[saltLen + 2] = (uint8_t)(block >> 8);
        msg[saltLen + 3] = (uint8_t)block;
        uint8_t u[SHA1_LEN], t[SHA1_LEN];
        hmacSha1(password, passwordLen, msg, saltLen + 4, u);
        memcpy(t, u, SHA1_LEN);
        for (uint32_t i = 1; i < iterations; ++i) {
            hmacSha1(password, passwordLen, u, SHA1_LEN, u);
            for (size_t j = 0; j < SHA1_LEN; ++j) t[j] ^= u[j];
        }
        const size_t take = outLen < SHA1_LEN ? outLen : SHA1_LEN;
        memcpy(out, t, take);
        out += take;
        outLen -= take;
    }
}

void wpaPmkHex(const char* ssid, size_t ssidLen, const char* passphrase, char out[65]) {
    uint8_t pmk[32];
    pbkdf2HmacSha1((const uint8_t*)passphrase, strlen(passphrase), (const uint8_t*)ssid, ssidLen, 4096, pmk, 32);
    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out[2 * i] = HEX[pmk[i] >> 4];
        out[2 * i + 1] = HEX[pmk[i] & 15];
    }
    out[64] = '\0';
    memset(pmk, 0, sizeof(pmk));
}

}
