// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Preferences (NVS) stand-in for the host unit tests. Every namespace opened by begin() keeps
// its own keys (stored as "<namespace>/<key>"), clear() empties only its own namespace.
// Like the real NVS, a key read with another type than it was written returns the default.

#include <Arduino.h>
#include <map>
#include <string>
#include <string.h>

namespace Fake {

struct Nvs {
    enum class Type : uint8_t { U8, I32, U32, U64, Str };
    struct Entry {
        Type type;
        uint64_t raw;
        std::string str;
    };
    std::map<std::string, Entry> values; // all namespaces
    bool available = true;
    bool failWrites = false;
    unsigned writeCount = 0;

    void reset() {
        values.clear();
        available = true;
        failWrites = false;
        writeCount = 0;
    }
    // Number of keys in one namespace.
    size_t count(const std::string& ns) const {
        size_t n = 0;
        for (const auto& v : values) n += v.first.compare(0, ns.size() + 1, ns + "/") == 0;
        return n;
    }
    // true if any stored value of any namespace contains `text` (plaintext secrets, W-S14).
    bool contains(const std::string& text) const {
        for (const auto& v : values) {
            if (v.second.type == Type::Str && v.second.str.find(text) != std::string::npos) return true;
        }
        return false;
    }
};

inline Nvs& nvs() {
    static Nvs n;
    return n;
}

} // namespace Fake

class Preferences {
public:
    bool begin(const char* ns, bool = false) {
        ns_ = ns ? ns : "";
        return Fake::nvs().available;
    }
    void end() {}

    uint8_t getUChar(const char* key, uint8_t def) { return (uint8_t)get_(key, Type::U8, def); }
    size_t putUChar(const char* key, uint8_t v) { return put_(key, Type::U8, v, 1); }
    int32_t getInt(const char* key, int32_t def) { return (int32_t)(uint32_t)get_(key, Type::I32, (uint32_t)def); }
    size_t putInt(const char* key, int32_t v) { return put_(key, Type::I32, (uint32_t)v, 4); }
    uint32_t getUInt(const char* key, uint32_t def) { return (uint32_t)get_(key, Type::U32, def); }
    size_t putUInt(const char* key, uint32_t v) { return put_(key, Type::U32, v, 4); }
    uint64_t getULong64(const char* key, uint64_t def) { return get_(key, Type::U64, def); }
    size_t putULong64(const char* key, uint64_t v) { return put_(key, Type::U64, v, 8); }
    // Like Arduino-ESP32: the length including the terminating NUL, 0 if missing or too long.
    size_t getString(const char* key, char* value, size_t maxLen) {
        const auto it = Fake::nvs().values.find(k_(key));
        if (it == Fake::nvs().values.end() || it->second.type != Type::Str) return 0;
        const size_t len = it->second.str.size() + 1;
        if (!value || len > maxLen) return 0;
        memcpy(value, it->second.str.c_str(), len);
        return len;
    }
    // strlen(value) on success, 0 on failure (and for "").
    size_t putString(const char* key, const char* value) {
        Fake::Nvs& n = Fake::nvs();
        if (!n.available || n.failWrites || !value) return 0;
        n.values[k_(key)] = Fake::Nvs::Entry {Type::Str, 0, value};
        ++n.writeCount;
        return strlen(value);
    }
    bool isKey(const char* key) { return Fake::nvs().available && Fake::nvs().values.count(k_(key)) > 0; }
    bool clear() {
        Fake::Nvs& n = Fake::nvs();
        if (!n.available || n.failWrites) return false;
        const std::string prefix = ns_ + "/";
        for (auto it = n.values.begin(); it != n.values.end();) {
            if (it->first.compare(0, prefix.size(), prefix) == 0) it = n.values.erase(it);
            else ++it;
        }
        ++n.writeCount;
        return true;
    }
    bool remove(const char* key) {
        Fake::Nvs& n = Fake::nvs();
        if (!n.available || n.failWrites || !n.values.erase(k_(key))) return false;
        ++n.writeCount;
        return true;
    }

private:
    using Type = Fake::Nvs::Type;

    std::string k_(const char* key) const { return ns_ + "/" + key; }
    uint64_t get_(const char* key, Type type, uint64_t def) {
        const auto it = Fake::nvs().values.find(k_(key));
        return (it == Fake::nvs().values.end() || it->second.type != type) ? def : it->second.raw;
    }
    size_t put_(const char* key, Type type, uint64_t raw, size_t size) {
        Fake::Nvs& n = Fake::nvs();
        if (!n.available || n.failWrites) return 0;
        n.values[k_(key)] = Fake::Nvs::Entry {type, raw, std::string()};
        ++n.writeCount;
        return size;
    }

    std::string ns_;
};
