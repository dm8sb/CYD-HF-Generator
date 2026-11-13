// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target line_framer: LineFramer::feed, looksLikeHttp, framerErrorCode (W-S17, W-S18).
// The input is the raw byte stream of a connection.
// Invariants: a Line is at most MAX_LINE bytes, NUL-terminated at length(), only printable
// ASCII and TAB, not blank, at most MAX_COMMANDS program units; error codes match the event;
// looksLikeHttp agrees with an independent matcher of the two W-S18 patterns and never reads
// past len; reset() behaves like a new framer.

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <remote/line_framer.h>

namespace fuzz_line_framer {

// ^[A-Z]+ \S+ HTTP/1\.[01]  or  ^[A-Za-z0-9-]+:\s  (lines here hold no whitespace but ' ' and TAB)
bool httpReference(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && s[i] >= 'A' && s[i] <= 'Z') ++i;
    if (i > 0 && i < s.size() && s[i] == ' ') {
        size_t j = i + 1;
        while (j < s.size() && s[j] != ' ' && s[j] != '\t') ++j;
        if (j > i + 1 && s.compare(j, 9, " HTTP/1.0") == 0) return true;
        if (j > i + 1 && s.compare(j, 9, " HTTP/1.1") == 0) return true;
    }
    i = 0;
    while (i < s.size() && ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') ||
                            (s[i] >= '0' && s[i] <= '9') || s[i] == '-')) {
        ++i;
    }
    return i > 0 && i + 1 < s.size() && s[i] == ':' && (s[i + 1] == ' ' || s[i + 1] == '\t');
}

int units(const std::string& s) {
    int n = 1;
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (quote) {
            if (s[i] == quote) quote = 0;
        } else if (s[i] == '"' || s[i] == '\'') {
            quote = s[i];
        } else if (s[i] == ';') {
            ++n;
        }
    }
    return n;
}

struct Rec {
    LineFramer::Event event;
    std::string line;
    bool operator==(const Rec& o) const { return event == o.event && line == o.line; }
};

void feed(LineFramer& f, const uint8_t* d, size_t n, std::vector<Rec>* log) {
    for (size_t i = 0; i < n; ++i) {
        const LineFramer::Event e = f.feed((char)d[i]);
        const int16_t code = framerErrorCode(e);
        switch (e) {
            case LineFramer::Event::None:
            case LineFramer::Event::Line: FUZZ_CHECK(code == 0); break;
            case LineFramer::Event::TooLong: FUZZ_CHECK(code == -363); break;
            case LineFramer::Event::BadChar: FUZZ_CHECK(code == -101); break;
            case LineFramer::Event::TooManyCommands: FUZZ_CHECK(code == -223); break;
        }
        // Events other than None only come with the LF that ends a line.
        if (e != LineFramer::Event::None) FUZZ_CHECK(d[i] == '\n');
        if (e == LineFramer::Event::Line) {
            const size_t len = f.length();
            FUZZ_CHECK(len >= 1 && len <= LineFramer::MAX_LINE);
            FUZZ_CHECK(f.line()[len] == '\0');
            const std::string s(f.line(), len);
            bool blank = true;
            for (size_t k = 0; k < len; ++k) {
                const unsigned char c = (unsigned char)s[k];
                FUZZ_CHECK((c >= 0x20 && c <= 0x7E) || c == '\t');
                if (c != ' ' && c != '\t') blank = false;
            }
            FUZZ_CHECK(!blank);
            FUZZ_CHECK(units(s) <= LineFramer::MAX_COMMANDS);
            // An exact-size copy: ASan reports any read past len.
            std::vector<char> exact(s.begin(), s.end());
            FUZZ_CHECK(looksLikeHttp(exact.data(), exact.size()) == httpReference(s));
        }
        if (log && e != LineFramer::Event::None) {
            Rec r;
            r.event = e;
            if (e == LineFramer::Event::Line) r.line.assign(f.line(), f.length());
            log->push_back(r);
        }
    }
}

int run(const uint8_t* data, size_t size) {
    // The whole stream through one framer.
    LineFramer whole;
    feed(whole, data, size, nullptr);

    // reset() after a split point equals a fresh framer on the rest.
    const size_t split = size ? data[0] % (size + 1) : 0;
    LineFramer a;
    feed(a, data, split, nullptr);
    a.reset();
    FUZZ_CHECK(a.length() == 0);
    std::vector<Rec> afterReset;
    feed(a, data + split, size - split, &afterReset);
    LineFramer b;
    std::vector<Rec> fresh;
    feed(b, data + split, size - split, &fresh);
    FUZZ_CHECK(afterReset == fresh);

    // looksLikeHttp on arbitrary bytes (TCP before framing): memory safety only.
    looksLikeHttp(reinterpret_cast<const char*>(data), size);
    looksLikeHttp(nullptr, size);
    return 0;
}

} // namespace fuzz_line_framer

FUZZ_TARGET(line_framer, fuzz_line_framer::run)
