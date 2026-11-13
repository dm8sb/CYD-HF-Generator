// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Preferences.h>

class Storage {
public:
    bool begin(const char* ns, bool readOnly = false);

    uint64_t getU64(const char* key, uint64_t def);
    bool putU64(const char* key, uint64_t value);
    uint32_t getU32(const char* key, uint32_t def);
    bool putU32(const char* key, uint32_t value);
    int32_t getI32(const char* key, int32_t def);
    bool putI32(const char* key, int32_t value);
    uint8_t getU8(const char* key, uint8_t def);
    bool putU8(const char* key, uint8_t value);
    // Text up to outSize - 1 bytes; false (out = "") if missing or longer.
    bool getString(const char* key, char* out, size_t outSize);
    // An empty text removes the key (NVS cannot tell "" from a failed write).
    bool putString(const char* key, const char* value);
    // Deletes the key; false if it did not exist or the delete failed.
    bool remove(const char* key);
    // Deletes every key of the namespace.
    bool clear();

private:
    Preferences prefs_;
    bool opened_ = false;
};
