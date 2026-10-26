#pragma once

#include <Arduino.h>
#include <config.h>

namespace Logger {
void begin(uint32_t baud);
void info(const char* tag, const char* msg);
void error(const char* tag, const char* msg);
void value(const char* tag, const char* label, int64_t value);
void valueU64(const char* tag, const char* label, uint64_t value);

// Verbose output, compiled out unless LOG_DEBUG is set.
inline void debug(const char* tag, const char* msg) { if (LOG_DEBUG) info(tag, msg); }
inline void debugValue(const char* tag, const char* label, int64_t v) { if (LOG_DEBUG) value(tag, label, v); }
}
