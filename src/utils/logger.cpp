#include "logger.h"

namespace Logger {

void begin(uint32_t baud) {
    Serial.begin(baud);
    uint32_t start = millis();
    while (!Serial && millis() - start < 500) { delay(1); }
    Serial.println();
    Serial.println("[LOGGER] Serial ready");
}

void info(const char* tag, const char* msg) {
    Serial.printf("[%s] %s\n", tag, msg);
}

void error(const char* tag, const char* msg) {
    Serial.printf("[ERROR][%s] %s\n", tag, msg);
}

void value(const char* tag, const char* label, int64_t value) {
    Serial.printf("[%s] %s: %lld\n", tag, label, (long long)value);
}

void valueU64(const char* tag, const char* label, uint64_t value) {
    Serial.printf("[%s] %s: %llu\n", tag, label, (unsigned long long)value);
}

}
