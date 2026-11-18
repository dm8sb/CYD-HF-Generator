// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

// JSON text into a fixed buffer without heap allocations (AF54, PERF-9): the state JSON and the
// error bodies of the REST API and the WebSocket push. The output is byte-identical to
// ArduinoJson 7's serializeJson() of the same document (keys in insertion order, no spaces,
// strings escaped like ArduinoJson: \" \\ \b \f \n \r \t, every other byte as it is), which
// test_json_writer checks against ArduinoJson for random states. Pure, host-tested.
//
// Usage: open containers, write keys and values in order, finish(). Nesting is not checked
// (the callers are fixed code paths); a write past the buffer only marks the writer overflowed.
class JsonWriter {
public:
    JsonWriter(char* out, size_t size);

    void beginObject() { open_('{'); }
    void endObject() { close_('}'); }
    void beginArray() { open_('['); }
    void endArray() { close_(']'); }
    // The next value belongs to this key (in an object).
    void key(const char* k);
    void string(const char* s);
    // Text as it is (an already formatted number such as "7100000.0").
    void raw(const char* s);
    void boolean(bool v);
    void null();
    void number(int64_t v);
    void number(uint64_t v);
    void number(int32_t v) { number((int64_t)v); }
    void number(uint32_t v) { number((uint64_t)v); }

    // The length of the text, NUL-terminated in out; 0 (out = "") if it did not fit.
    size_t finish();
    bool overflowed() const { return overflow_; }

private:
    void open_(char c);
    void close_(char c);
    void separator_();
    void put_(char c);
    void puts_(const char* s);
    void quoted_(const char* s);

    char* out_;
    size_t size_;
    size_t n_ = 0;
    bool overflow_ = false;
    bool comma_ = false;    // an element was written in the current container
    bool afterKey_ = false; // a key waits for its value
};
