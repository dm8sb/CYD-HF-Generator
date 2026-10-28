#pragma once

// Minimal Arduino core for the host unit tests (env:native / env:native-win).
// Header-only: function-local statics give one shared instance per test program.

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

namespace Fake {

inline uint32_t& clockMs() {
    static uint32_t ms = 0;
    return ms;
}
inline void advanceMs(uint32_t ms) { clockMs() += ms; }

struct Ledc {
    uint8_t channel;
    uint32_t freq;
    uint8_t bits;
    int pin;
    uint32_t duty;
};
inline Ledc& ledc() {
    static Ledc l {0xFF, 0, 0, -1, 0};
    return l;
}

// Serial output is captured instead of printed; tests can look for log lines.
struct SerialLog {
    char last[256];
    unsigned errors;
    unsigned lines;
};
inline SerialLog& serialLog() {
    static SerialLog log {};
    return log;
}

} // namespace Fake

inline uint32_t millis() { return Fake::clockMs(); }
inline void delay(uint32_t ms) { Fake::advanceMs(ms); }
inline void delayMicroseconds(uint32_t) {}

// GPIO: pins in lowMask read LOW (e.g. an I2C line held by a device). Writes and
// falling edges (written LOW after HIGH or for the first time) are counted.
#define LOW 0
#define HIGH 1
#define PULLUP 0x04
#define INPUT_PULLUP 0x05
#define OUTPUT_OPEN_DRAIN 0x13
namespace Fake {
struct Gpio {
    uint32_t lowMask;
    unsigned writes;
    uint32_t drivenLow;
    unsigned falls[40];
};
inline Gpio& gpio() {
    static Gpio g {};
    return g;
}
} // namespace Fake
inline void pinMode(int, uint8_t) {}
inline int digitalRead(int pin) { return (Fake::gpio().lowMask >> pin) & 1 ? LOW : HIGH; }
inline void digitalWrite(int pin, uint8_t level) {
    Fake::Gpio& g = Fake::gpio();
    ++g.writes;
    const uint32_t bit = 1u << pin;
    if (level == LOW && !(g.drivenLow & bit)) ++g.falls[pin];
    g.drivenLow = level == LOW ? (g.drivenLow | bit) : (g.drivenLow & ~bit);
}

class FakeSerial {
public:
    void begin(unsigned long) {}
    explicit operator bool() const { return true; }
    size_t println(const char* s = "") { return printf("%s\n", s); }
    size_t printf(const char* fmt, ...) {
        Fake::SerialLog& log = Fake::serialLog();
        va_list args;
        va_start(args, fmt);
        const int n = vsnprintf(log.last, sizeof(log.last), fmt, args);
        va_end(args);
        ++log.lines;
        if (strncmp(log.last, "[ERROR]", 7) == 0) ++log.errors;
        return n < 0 ? 0 : (size_t)n;
    }
};
inline FakeSerial& fakeSerial() {
    static FakeSerial serial;
    return serial;
}
#define Serial fakeSerial()

inline bool ledcSetup(uint8_t channel, uint32_t freq, uint8_t bits) {
    Fake::ledc().channel = channel;
    Fake::ledc().freq = freq;
    Fake::ledc().bits = bits;
    return true;
}
inline void ledcAttachPin(int pin, uint8_t) { Fake::ledc().pin = pin; }
inline void ledcWrite(uint8_t, uint32_t duty) { Fake::ledc().duty = duty; }
