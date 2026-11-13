// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace Logger {

namespace {
Output out_ = Output::ToSerial;
char ring_[BUFFER_LINES][LINE_LEN];
uint16_t head_ = 0;   // oldest line
uint16_t count_ = 0;
uint32_t dropped_ = 0;

#ifdef ESP_PLATFORM
SemaphoreHandle_t mutex_ = nullptr;
struct Lock {
    Lock() {
        if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    }
    ~Lock() {
        if (mutex_) xSemaphoreGive(mutex_);
    }
};

// ESP-IDF log lines (ESP_LOGx) go through the logger as well.
int idfVprintf(const char* fmt, va_list args) {
    char line[LINE_LEN];
    const int n = vsnprintf(line, sizeof(line), fmt, args);
    raw(line);
    return n;
}
#else
// Host: no task switch. User-provided constructor and destructor, so newer GCC does not
// warn about an unused variable.
struct Lock {
    Lock() {}
    ~Lock() {}
};
#endif

// SEC-6: foreign text (SSIDs, names) must not forge lines or send terminal escapes: every
// control character (C0, DEL) before the final LF becomes '?'. UTF-8 stays as it is.
void clean(char* s) {
    for (; *s; ++s) {
        const unsigned char c = (unsigned char)*s;
        if ((c < 0x20 || c == 0x7F) && !(c == '\n' && s[1] == '\0')) *s = '?';
    }
}

void emit(char* line) {
    clean(line);
    Lock lock;
    switch (out_) {
        case Output::ToSerial:
            Serial.printf("%s", line);
            break;
        case Output::ToBuffer: {
            if (count_ == BUFFER_LINES) {
                head_ = (uint16_t)((head_ + 1) % BUFFER_LINES);
                --count_;
                ++dropped_;
            }
            char* slot = ring_[(head_ + count_) % BUFFER_LINES];
            size_t n = 0;
            while (n + 1 < LINE_LEN && line[n]) {
                slot[n] = line[n];
                ++n;
            }
            slot[n] = '\0';
            // A cut line still ends with a newline.
            if (n == 0 || slot[n - 1] != '\n') {
                if (n + 1 < LINE_LEN) {
                    slot[n] = '\n';
                    slot[n + 1] = '\0';
                } else {
                    slot[LINE_LEN - 2] = '\n';
                }
            }
            ++count_;
            break;
        }
        case Output::Off:
            break;
    }
}

void emitf(const char* fmt, ...) {
    char line[LINE_LEN + 32];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    emit(line);
}
}

void begin(uint32_t baud) {
#ifdef ESP_PLATFORM
    if (!mutex_) mutex_ = xSemaphoreCreateMutex();
    esp_log_set_vprintf(idfVprintf);
#endif
    Serial.begin(baud);
    uint32_t start = millis();
    while (!Serial && millis() - start < 500) { delay(1); }
    Serial.println();
    Serial.println("[LOGGER] Serial ready");
}

void setOutput(Output o) {
    Lock lock;
    out_ = o;
}

Output output() { return out_; }

void info(const char* tag, const char* msg) { emitf("[%s] %s\n", tag, msg); }

void error(const char* tag, const char* msg) { emitf("[ERROR][%s] %s\n", tag, msg); }

void value(const char* tag, const char* label, int64_t value) { emitf("[%s] %s: %lld\n", tag, label, (long long)value); }

void raw(const char* line) {
    if (!line || !line[0]) return;
    const size_t n = strlen(line);
    if (line[n - 1] == '\n') emitf("%s", line);
    else emitf("%s\n", line);
}

uint16_t readBuffered(char* out, size_t outSize, uint16_t maxLines) {
    if (!out || outSize == 0) return 0;
    Lock lock;
    out[0] = '\0';
    size_t used = 0;
    uint16_t lines = 0;
    while (count_ > 0 && lines < maxLines) {
        const char* slot = ring_[head_];
        const size_t n = strlen(slot);
        if (used + n + 1 > outSize) break;
        memcpy(out + used, slot, n + 1);
        used += n;
        head_ = (uint16_t)((head_ + 1) % BUFFER_LINES);
        --count_;
        ++lines;
    }
    return lines;
}

uint16_t buffered() { return count_; }

uint32_t dropped() { return dropped_; }

}
