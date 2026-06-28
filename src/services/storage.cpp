#include "storage.h"
#include <utils/logger.h>

bool Storage::begin(const char* ns, bool readOnly) {
    opened_ = prefs_.begin(ns, readOnly);
    if (opened_) Logger::info("STORAGE", "Preferences opened");
    else Logger::error("STORAGE", "Preferences open failed");
    return opened_;
}

void Storage::end() { if (opened_) prefs_.end(); opened_ = false; }
uint64_t Storage::getU64(const char* key, uint64_t def) { return prefs_.getULong64(key, def); }
void Storage::putU64(const char* key, uint64_t value) { prefs_.putULong64(key, value); }
uint32_t Storage::getU32(const char* key, uint32_t def) { return prefs_.getUInt(key, def); }
void Storage::putU32(const char* key, uint32_t value) { prefs_.putUInt(key, value); }
int32_t Storage::getI32(const char* key, int32_t def) { return prefs_.getInt(key, def); }
void Storage::putI32(const char* key, int32_t value) { prefs_.putInt(key, value); }
uint8_t Storage::getU8(const char* key, uint8_t def) { return prefs_.getUChar(key, def); }
void Storage::putU8(const char* key, uint8_t value) { prefs_.putUChar(key, value); }
bool Storage::getBool(const char* key, bool def) { return prefs_.getBool(key, def); }
void Storage::putBool(const char* key, bool value) { prefs_.putBool(key, value); }
