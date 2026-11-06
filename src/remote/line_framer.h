#pragma once

#include <stddef.h>
#include <stdint.h>

// Splits a byte stream (USB serial, TCP 5025) into SCPI lines (W-S17, S-11). Pure logic, no
// Arduino dependency: host-tested and fuzzed.
// - A line ends with LF; CR is ignored, blank lines are dropped.
// - At most MAX_LINE bytes: a longer line is discarded up to its LF (TooLong, SCPI -363).
// - NUL, other control characters except TAB, and bytes >= 0x80 discard the line (BadChar, -101).
// - At most MAX_COMMANDS program message units, i.e. MAX_COMMANDS - 1 ';' outside quotes
//   (TooManyCommands, -223).
class LineFramer {
public:
    static constexpr size_t MAX_LINE = 256;
    static constexpr uint8_t MAX_COMMANDS = 16;

    enum class Event : uint8_t { None, Line, TooLong, BadChar, TooManyCommands };

    // One byte; Line: line()/length() hold the complete line (NUL-terminated, without LF)
    // until the next call.
    Event feed(char c);
    const char* line() const { return buf_; }
    size_t length() const { return len_; }
    // Drops a partial line (connection closed).
    void reset();

private:
    Event finish_();

    char buf_[MAX_LINE + 1] {};
    size_t len_ = 0;
    bool tooLong_ = false;
    bool bad_ = false;
    bool complete_ = false; // the previous call returned Line: start a new one
};

// TCP 5025 (W-S18, S-6): an HTTP request line ("GET / HTTP/1.1") or header line ("Host: x")
// sent by a browser or a cross-site request; the server closes without executing it.
bool looksLikeHttp(const char* line, size_t len);

// SCPI error code for a framing event (0 for None and Line).
int16_t framerErrorCode(LineFramer::Event e);
