// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "json_writer.h"

JsonWriter::JsonWriter(char* out, size_t size) : out_(out), size_(size) {
    if (!out_ || size_ == 0) overflow_ = true;
    else out_[0] = '\0';
}

void JsonWriter::put_(char c) {
    // One byte stays free for the NUL.
    if (overflow_ || n_ + 1 >= size_) {
        overflow_ = true;
        return;
    }
    out_[n_++] = c;
}

void JsonWriter::puts_(const char* s) {
    while (s && *s) put_(*s++);
}

void JsonWriter::separator_() {
    if (comma_ && !afterKey_) put_(',');
    afterKey_ = false;
}

void JsonWriter::open_(char c) {
    separator_();
    put_(c);
    comma_ = false;
}

void JsonWriter::close_(char c) {
    put_(c);
    comma_ = true;
    afterKey_ = false;
}

// As ArduinoJson's TextFormatter::writeChar() (EscapeSequence): only these get a backslash.
void JsonWriter::quoted_(const char* s) {
    put_('"');
    for (; s && *s; ++s) {
        char e = 0;
        switch (*s) {
            case '"': e = '"'; break;
            case '\\': e = '\\'; break;
            case '\b': e = 'b'; break;
            case '\f': e = 'f'; break;
            case '\n': e = 'n'; break;
            case '\r': e = 'r'; break;
            case '\t': e = 't'; break;
            default: break;
        }
        if (e) {
            put_('\\');
            put_(e);
        } else {
            put_(*s);
        }
    }
    put_('"');
}

void JsonWriter::key(const char* k) {
    if (comma_) put_(',');
    quoted_(k);
    put_(':');
    afterKey_ = true;
    comma_ = false;
}

void JsonWriter::string(const char* s) {
    separator_();
    quoted_(s ? s : "");
    comma_ = true;
}

void JsonWriter::raw(const char* s) {
    separator_();
    puts_(s);
    comma_ = true;
}

void JsonWriter::boolean(bool v) { raw(v ? "true" : "false"); }

void JsonWriter::null() { raw("null"); }

void JsonWriter::number(uint64_t v) {
    char buf[21];
    size_t i = sizeof(buf) - 1;
    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    raw(buf + i);
}

void JsonWriter::number(int64_t v) {
    if (v >= 0) return number((uint64_t)v);
    char buf[22];
    // Magnitude without overflow for INT64_MIN.
    uint64_t m = (uint64_t)(-(v + 1)) + 1;
    size_t i = sizeof(buf) - 1;
    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + m % 10);
        m /= 10;
    } while (m);
    buf[--i] = '-';
    raw(buf + i);
}

size_t JsonWriter::finish() {
    if (overflow_) {
        if (out_ && size_) out_[0] = '\0';
        return 0;
    }
    out_[n_] = '\0';
    return n_;
}
