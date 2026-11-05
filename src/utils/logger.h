#pragma once

#include <Arduino.h>
#include <config.h>

// Log lines "[TAG] text" (V-5). The output is switchable because SCPI over the USB serial port
// uses the same interface (C-16, W-E27): Serial, a ring buffer (read by SYSTem:LOG?) or off.
// Thread-safe on the ESP32 (mutex), so network tasks may log.
namespace Logger {
enum class Output : uint8_t { ToSerial, ToBuffer, Off };

static constexpr uint16_t BUFFER_LINES = 32;
static constexpr uint16_t LINE_LEN = 96;   // longer lines are cut

void begin(uint32_t baud);
void setOutput(Output o);
Output output();

void info(const char* tag, const char* msg);
void error(const char* tag, const char* msg);
void value(const char* tag, const char* label, int64_t value);
// A preformatted line (LVGL and ESP-IDF log hooks); a missing newline is added.
void raw(const char* line);

// Buffered lines, oldest first, each ending in '\n', as many whole lines as fit into out;
// they are removed from the buffer. Returns the number of lines copied.
uint16_t readBuffered(char* out, size_t outSize);
uint16_t buffered();
// Lines lost because the buffer was full (since boot).
uint32_t dropped();

// Verbose output, compiled out unless LOG_DEBUG is set.
inline void debugValue(const char* tag, const char* label, int64_t v) { if (LOG_DEBUG) value(tag, label, v); }
}
