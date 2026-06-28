#pragma once

#include <Arduino.h>
#include <Preferences.h>

class Storage {
public:
    bool begin(const char* ns, bool readOnly = false);
    void end();

    uint64_t getU64(const char* key, uint64_t def);
    void putU64(const char* key, uint64_t value);
    uint32_t getU32(const char* key, uint32_t def);
    void putU32(const char* key, uint32_t value);
    int32_t getI32(const char* key, int32_t def);
    void putI32(const char* key, int32_t value);
    uint8_t getU8(const char* key, uint8_t def);
    void putU8(const char* key, uint8_t value);
    bool getBool(const char* key, bool def);
    void putBool(const char* key, bool value);

private:
    Preferences prefs_;
    bool opened_ = false;
};
