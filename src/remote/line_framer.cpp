#include "line_framer.h"

namespace {
bool upper(char c) { return c >= 'A' && c <= 'Z'; }
bool alnum(char c) { return upper(c) || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
bool space(char c) { return c == ' ' || c == '\t'; }

// Program message units = ';' outside quoted strings + 1.
uint16_t commandCount(const char* s, size_t n) {
    uint16_t count = 1;
    char quote = 0;
    for (size_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (quote) {
            if (c == quote) quote = 0; // a doubled quote reopens at the next character
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == ';') {
            ++count;
        }
    }
    return count;
}
}

void LineFramer::reset() {
    len_ = 0;
    buf_[0] = '\0';
    tooLong_ = bad_ = complete_ = false;
}

LineFramer::Event LineFramer::finish_() {
    const bool tooLong = tooLong_;
    const bool bad = bad_;
    tooLong_ = bad_ = false;
    if (tooLong) {
        reset();
        return Event::TooLong;
    }
    if (bad) {
        reset();
        return Event::BadChar;
    }
    // Only blanks: nothing to do.
    size_t i = 0;
    while (i < len_ && space(buf_[i])) ++i;
    if (i == len_) {
        reset();
        return Event::None;
    }
    if (commandCount(buf_, len_) > MAX_COMMANDS) {
        reset();
        return Event::TooManyCommands;
    }
    buf_[len_] = '\0';
    complete_ = true;
    return Event::Line;
}

LineFramer::Event LineFramer::feed(char c) {
    if (complete_) reset();
    if (c == '\n') return finish_();
    if (c == '\r') return Event::None;
    const unsigned char u = (unsigned char)c;
    if (u >= 0x80 || (u < 0x20 && c != '\t') || u == 0x7F) {
        bad_ = true;
        return Event::None;
    }
    if (tooLong_ || bad_) return Event::None; // discarding up to the LF
    if (len_ == MAX_LINE) {
        tooLong_ = true;
        return Event::None;
    }
    buf_[len_++] = c;
    return Event::None;
}

bool looksLikeHttp(const char* s, size_t n) {
    if (!s) return false;
    // Request line: ^[A-Z]+ \S+ HTTP/1\.[01]
    size_t i = 0;
    while (i < n && upper(s[i])) ++i;
    if (i > 0 && i < n && s[i] == ' ') {
        size_t j = i + 1;
        const size_t target = j;
        while (j < n && s[j] != ' ' && !space(s[j])) ++j;
        if (j > target && j + 9 <= n && s[j] == ' ') {
            const char* v = s + j + 1;
            if (v[0] == 'H' && v[1] == 'T' && v[2] == 'T' && v[3] == 'P' && v[4] == '/' && v[5] == '1' &&
                v[6] == '.' && (v[7] == '0' || v[7] == '1')) {
                return true;
            }
        }
    }
    // Header line: ^[A-Za-z0-9-]+:\s
    i = 0;
    while (i < n && (alnum(s[i]) || s[i] == '-')) ++i;
    return i > 0 && i + 1 < n && s[i] == ':' && space(s[i + 1]);
}

int16_t framerErrorCode(LineFramer::Event e) {
    switch (e) {
        case LineFramer::Event::TooLong: return -363;         // Input buffer overrun
        case LineFramer::Event::BadChar: return -101;         // Invalid character
        case LineFramer::Event::TooManyCommands: return -223; // Too much data
        default: return 0;
    }
}
