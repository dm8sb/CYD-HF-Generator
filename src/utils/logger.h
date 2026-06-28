#pragma once

#include <Arduino.h>

namespace Logger {
void begin(uint32_t baud);
void info(const char* tag, const char* msg);
void error(const char* tag, const char* msg);
void value(const char* tag, const char* label, int64_t value);
void valueU64(const char* tag, const char* label, uint64_t value);
}
