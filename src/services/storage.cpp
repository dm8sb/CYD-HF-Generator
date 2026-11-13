// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storage.h"
#include <utils/logger.h>
#include <string.h>

bool Storage::begin(const char* ns, bool readOnly) {
    opened_ = prefs_.begin(ns, readOnly);
    if (opened_) Logger::info("STORAGE", "Preferences opened");
    else Logger::error("STORAGE", "Preferences open failed");
    return opened_;
}

// Preferences::put*() returns the number of bytes written, 0 on failure.
uint64_t Storage::getU64(const char* key, uint64_t def) { return prefs_.getULong64(key, def); }
bool Storage::putU64(const char* key, uint64_t value) { return prefs_.putULong64(key, value) > 0; }
uint32_t Storage::getU32(const char* key, uint32_t def) { return prefs_.getUInt(key, def); }
bool Storage::putU32(const char* key, uint32_t value) { return prefs_.putUInt(key, value) > 0; }
int32_t Storage::getI32(const char* key, int32_t def) { return prefs_.getInt(key, def); }
bool Storage::putI32(const char* key, int32_t value) { return prefs_.putInt(key, value) > 0; }
uint8_t Storage::getU8(const char* key, uint8_t def) { return prefs_.getUChar(key, def); }
bool Storage::putU8(const char* key, uint8_t value) { return prefs_.putUChar(key, value) > 0; }
bool Storage::getString(const char* key, char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    if (!opened_ || !prefs_.isKey(key)) return false;
    if (prefs_.getString(key, out, outSize) == 0) {
        out[0] = '\0';
        return false;
    }
    return true;
}
bool Storage::putString(const char* key, const char* value) {
    if (!opened_ || !value) return false;
    if (!*value) return !prefs_.isKey(key) || prefs_.remove(key);
    return prefs_.putString(key, value) == strlen(value);
}
bool Storage::clear() { return opened_ && prefs_.clear(); }
bool Storage::remove(const char* key) { return prefs_.isKey(key) && prefs_.remove(key); }
