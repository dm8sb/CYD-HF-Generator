#pragma once

#include <stddef.h>
#include <stdint.h>

// SHA-1, HMAC-SHA1 and PBKDF2-HMAC-SHA1 (RFC 3174, 2104, 8018) without platform dependency,
// so the WPA2 PMK (W-S14) is computed the same on the ESP32 and in the host tests, which check
// it against the RFC 6070 and IEEE 802.11i vectors. Not for anything needing speed or
// side-channel resistance: it runs once when a network is saved.
namespace Crypto {

static constexpr size_t SHA1_LEN = 20;

void sha1(const uint8_t* data, size_t len, uint8_t out[SHA1_LEN]);
void hmacSha1(const uint8_t* key, size_t keyLen, const uint8_t* data, size_t len, uint8_t out[SHA1_LEN]);
void pbkdf2HmacSha1(const uint8_t* password, size_t passwordLen, const uint8_t* salt, size_t saltLen,
                    uint32_t iterations, uint8_t* out, size_t outLen);

// WPA2-PSK: PMK = PBKDF2(passphrase, SSID, 4096, 32) as 64 lower-case hex digits + NUL.
void wpaPmkHex(const char* ssid, size_t ssidLen, const char* passphrase, char out[65]);

}
