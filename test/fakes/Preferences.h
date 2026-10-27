#pragma once

// Preferences (NVS) stand-in for the host unit tests: one in-memory namespace.
// Like the real NVS, a key read with another type than it was written returns the default.

#include <Arduino.h>
#include <map>
#include <string>

namespace Fake {

struct Nvs {
    enum class Type : uint8_t { U8, I32, U32, U64 };
    struct Entry {
        Type type;
        uint64_t raw;
    };
    std::map<std::string, Entry> values;
    bool available = true;
    bool failWrites = false;
    unsigned writeCount = 0;

    void reset() {
        values.clear();
        available = true;
        failWrites = false;
        writeCount = 0;
    }
};

inline Nvs& nvs() {
    static Nvs n;
    return n;
}

} // namespace Fake

class Preferences {
public:
    bool begin(const char*, bool = false) { return Fake::nvs().available; }
    void end() {}

    uint8_t getUChar(const char* key, uint8_t def) { return (uint8_t)get_(key, Type::U8, def); }
    size_t putUChar(const char* key, uint8_t v) { return put_(key, Type::U8, v, 1); }
    int32_t getInt(const char* key, int32_t def) { return (int32_t)(uint32_t)get_(key, Type::I32, (uint32_t)def); }
    size_t putInt(const char* key, int32_t v) { return put_(key, Type::I32, (uint32_t)v, 4); }
    uint32_t getUInt(const char* key, uint32_t def) { return (uint32_t)get_(key, Type::U32, def); }
    size_t putUInt(const char* key, uint32_t v) { return put_(key, Type::U32, v, 4); }
    uint64_t getULong64(const char* key, uint64_t def) { return get_(key, Type::U64, def); }
    size_t putULong64(const char* key, uint64_t v) { return put_(key, Type::U64, v, 8); }

private:
    using Type = Fake::Nvs::Type;

    uint64_t get_(const char* key, Type type, uint64_t def) {
        const auto it = Fake::nvs().values.find(key);
        return (it == Fake::nvs().values.end() || it->second.type != type) ? def : it->second.raw;
    }
    size_t put_(const char* key, Type type, uint64_t raw, size_t size) {
        Fake::Nvs& n = Fake::nvs();
        if (!n.available || n.failWrites) return 0;
        n.values[key] = Fake::Nvs::Entry {type, raw};
        ++n.writeCount;
        return size;
    }
};
