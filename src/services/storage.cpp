#include "storage.h"
#include <utils/logger.h>

bool Storage::begin(const char* ns, bool readOnly) {
    opened_ = prefs_.begin(ns, readOnly);
    if (opened_) Logger::info("STORAGE", "Preferences opened");
    else Logger::error("STORAGE", "Preferences open failed");
    return opened_;
}

void Storage::end() {
    if (opened_) prefs_.end();
    opened_ = false;
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
