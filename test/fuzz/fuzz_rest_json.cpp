// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target rest_json: the REST JSON mapping (remote/rest_json.*, ARBEITSPLAN 3.6, W-S1, W-S8,
// W-S9, W-S19, W-S21) on ArduinoJson 7.4.3.
// Input: byte 0 bits 0-3 the function (mod 12), bit 4 pad the body with blanks to MAX_BODY,
// bit 5 to MAX_BODY + 1, bit 6 pass the body as nullptr (with its length); then:
//   0 / 1   parseChannel(1 / 2), body
//   2       parseChannel(byte 1), body
//   3       parseCh3, body
//   4       parseConfig, body
//   5 / 6   parseAction(SaveNow / Identify), body
//   7       parseAction(CommandType byte 1 % 16), body
//   8       parseLogin with a password buffer of byte 1 % 80 bytes, body
//   9       carriesSecret only, body
//   10      buildState from the bytes (Snapshot, DeviceInfo, time, buffer size)
//   11      fromPush/fromResult/fromOutcome (bytes 1, 2), channelFromPath (rest)
// Every body also goes through carriesSecret() and every Reply through errorJson().
//
// Invariants (against an independent RFC 8259 parser that decodes strings as ArduinoJson does):
// - > MAX_BODY 413/-223; invalid JSON (also nesting > 10) and duplicate keys 400/-102; top level
//   not an object and wrong types 400/-104; unknown keys 400/-108 (frequency with frequencyText
//   too); no key 400/-109; integers outside int64 (and floats >= 9.2e18) 400/-222; unknown
//   enum text, NUL in a string, bad frequencyText 400/-224 (outside the range -222, S55);
//   remoteOutputs 403/-221; channel
//   not 1/2 404/-114; the first problem in key order answers;
// - Ok results carry at most 8 commands, exactly the expected ones in the documented fixed
//   order, source Web, values inside their types (bool 0/1, enums in range, frequencyText in
//   8 kHz..160 MHz); refused results carry none; parseLogin copies exactly the password, never
//   past its buffer;
// - carriesSecret finds any key containing pass/psk/secret/key (ASCII case-insensitive, after
//   unescaping) at any depth of what ArduinoJson keeps, false for invalid JSON;
// - errorJson always fits ERROR_SIZE and is {"error":"<code>,<text>"};
// - the state JSON fits STATE_SIZE for every snapshot with values the device can produce (other
//   values: 0 when they do not fit), parses again (independent parser and
//   ArduinoJson), has no raw control bytes, every string strict UTF-8 without control
//   characters, foreign strings = Sanitize::text of the raw bytes, numbers and enums as in the
//   snapshot; a smaller buffer gives 0 (or the same text when it fits) and is never overrun.
// Fixed in S55: carriesSecret() looks at the raw keys, so a secret key inside a value that a
// later duplicate key replaces ({"a":{"password":"x"},"a":1}) is seen as well.

#include "fuzz_common.h"

#include <math.h>
#include <string>
#include <vector>
#include <ArduinoJson.h>
#include <config.h>
#include <constants.h>
#include <remote/rest_json.h>
#include <utils/frequency_utils.h>
#include <utils/sanitize.h>

namespace fuzz_rest_json {

using namespace RestJson;

// --- independent JSON parser ----------------------------------------------------------------

struct JV {
    enum Kind { Obj, Arr, Str, Num, True, False, Null } k = Null;
    std::string s;                 // decoded string, or the number token as written
    std::vector<std::string> keys; // object members as written (duplicates kept)
    std::vector<JV> vals;          // object values or array items
};

class Ref {
public:
    Ref(const char* s, size_t n) : s_(s), n_(n) {}
    // RFC 8259: one value, only blanks around it, nesting <= MAX_NESTING.
    bool parse(JV* out) {
        ws();
        if (!value(out, 0)) return false;
        // ArduinoJson refuses a top-level number with anything after it, also blanks.
        if (out->k == JV::Num && i_ < n_) ajFails = true;
        ws();
        return i_ == n_;
    }
    // ArduinoJson reads at most 63 characters of a number: the rest of the digits stays where
    // it expects ',' (or the end) and it fails there.
    bool ajFails = false;

private:
    bool at(char c) const { return i_ < n_ && s_[i_] == c; }
    bool eat(char c) {
        if (!at(c)) return false;
        ++i_;
        return true;
    }
    static bool digit(char c) { return c >= '0' && c <= '9'; }
    void ws() {
        while (i_ < n_ && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }
    bool lit(const char* w) {
        const size_t k = strlen(w);
        if (n_ - i_ < k || memcmp(s_ + i_, w, k) != 0) return false;
        i_ += k;
        return true;
    }
    bool value(JV* v, int depth) {
        if (i_ >= n_) return false;
        const char c = s_[i_];
        if (c == '{') return object(v, depth + 1);
        if (c == '[') return array(v, depth + 1);
        if (c == '"') {
            v->k = JV::Str;
            return str(&v->s);
        }
        if (c == 't') {
            v->k = JV::True;
            return lit("true");
        }
        if (c == 'f') {
            v->k = JV::False;
            return lit("false");
        }
        if (c == 'n') {
            v->k = JV::Null;
            return lit("null");
        }
        v->k = JV::Num;
        return number(&v->s);
    }
    bool object(JV* v, int depth) {
        if (depth > MAX_NESTING) return false;
        v->k = JV::Obj;
        ++i_;
        ws();
        if (eat('}')) return true;
        for (;;) {
            std::string key;
            if (!at('"') || !str(&key)) return false;
            ws();
            if (!eat(':')) return false;
            ws();
            JV child;
            if (!value(&child, depth)) return false;
            v->keys.push_back(key);
            v->vals.push_back(child);
            ws();
            if (eat('}')) return true;
            if (!eat(',')) return false;
            ws();
        }
    }
    bool array(JV* v, int depth) {
        if (depth > MAX_NESTING) return false;
        v->k = JV::Arr;
        ++i_;
        ws();
        if (eat(']')) return true;
        for (;;) {
            JV child;
            if (!value(&child, depth)) return false;
            v->vals.push_back(child);
            ws();
            if (eat(']')) return true;
            if (!eat(',')) return false;
            ws();
        }
    }
    static void utf8(uint32_t cp, std::string* out) {
        if (cp < 0x80) {
            *out += (char)cp;
        } else if (cp < 0x800) {
            *out += (char)(0xC0 | cp >> 6);
            *out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            *out += (char)(0xE0 | cp >> 12);
            *out += (char)(0x80 | (cp >> 6 & 0x3F));
            *out += (char)(0x80 | (cp & 0x3F));
        } else {
            *out += (char)(0xF0 | cp >> 18);
            *out += (char)(0x80 | (cp >> 12 & 0x3F));
            *out += (char)(0x80 | (cp >> 6 & 0x3F));
            *out += (char)(0x80 | (cp & 0x3F));
        }
    }
    static int hexv(char c) {
        if (digit(c)) return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    // Escapes decoded like ArduinoJson 7.4.3 (Utf16::Codepoint): a high surrogate waits for the
    // next \u, a low surrogate combines with the last high one (0 at first), a high surrogate
    // without a following \u is dropped.
    bool str(std::string* out) {
        ++i_;
        uint32_t high = 0;
        while (i_ < n_) {
            const unsigned char c = (unsigned char)s_[i_++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') {
                *out += (char)c;
                continue;
            }
            if (i_ >= n_) return false;
            const char e = s_[i_++];
            switch (e) {
                case '"': *out += '"'; break;
                case '\\': *out += '\\'; break;
                case '/': *out += '/'; break;
                case 'b': *out += '\b'; break;
                case 'f': *out += '\f'; break;
                case 'n': *out += '\n'; break;
                case 'r': *out += '\r'; break;
                case 't': *out += '\t'; break;
                case 'u': {
                    uint32_t cu = 0;
                    for (int k = 0; k < 4; ++k) {
                        if (i_ >= n_ || hexv(s_[i_]) < 0) return false;
                        cu = cu << 4 | (uint32_t)hexv(s_[i_++]);
                    }
                    if (cu >= 0xD800 && cu < 0xDC00) {
                        high = cu & 0x3FF;
                    } else if (cu >= 0xDC00 && cu < 0xE000) {
                        utf8(0x10000 + ((high << 10) | (cu & 0x3FF)), out);
                    } else {
                        utf8(cu, out);
                    }
                    break;
                }
                default: return false;
            }
        }
        return false;
    }
    bool number(std::string* tok) {
        const size_t start = i_;
        eat('-');
        if (!eat('0')) {
            if (!(i_ < n_ && s_[i_] >= '1' && s_[i_] <= '9')) return false;
            while (i_ < n_ && digit(s_[i_])) ++i_;
        }
        if (eat('.')) {
            if (!(i_ < n_ && digit(s_[i_]))) return false;
            while (i_ < n_ && digit(s_[i_])) ++i_;
        }
        if (at('e') || at('E')) {
            ++i_;
            if (!eat('+')) eat('-');
            if (!(i_ < n_ && digit(s_[i_]))) return false;
            while (i_ < n_ && digit(s_[i_])) ++i_;
        }
        tok->assign(s_ + start, i_ - start);
        if (tok->size() >= 64) ajFails = true;
        return true;
    }

    const char* s_;
    size_t n_;
    size_t i_ = 0;
};

const JV* member(const JV& o, const char* key) {
    for (size_t i = 0; i < o.keys.size(); ++i) {
        if (o.keys[i] == key) return &o.vals[i];
    }
    return nullptr;
}

bool hasDuplicate(const JV& o) {
    for (size_t i = 0; i < o.keys.size(); ++i) {
        for (size_t j = i + 1; j < o.keys.size(); ++j) {
            if (o.keys[i] == o.keys[j]) return true;
        }
    }
    return false;
}

bool secretKey(const std::string& key) {
    std::string k(key);
    for (size_t i = 0; i < k.size(); ++i) {
        if (k[i] >= 'A' && k[i] <= 'Z') k[i] = (char)(k[i] - 'A' + 'a');
    }
    return k.find("pass") != std::string::npos || k.find("psk") != std::string::npos ||
           k.find("secret") != std::string::npos || k.find("key") != std::string::npos;
}

// Any key at any depth, as written.
bool secretRaw(const JV& v) {
    for (size_t i = 0; i < v.vals.size(); ++i) {
        if (v.k == JV::Obj && secretKey(v.keys[i])) return true;
        if (secretRaw(v.vals[i])) return true;
    }
    return false;
}

// What ArduinoJson keeps: a repeated key replaces the value at the first key's place.
bool secretKept(const JV& v) {
    if (v.k == JV::Arr) {
        for (const JV& e : v.vals) {
            if (secretKept(e)) return true;
        }
        return false;
    }
    if (v.k != JV::Obj) return false;
    std::vector<std::string> keys;
    std::vector<const JV*> vals;
    for (size_t i = 0; i < v.keys.size(); ++i) {
        size_t j = 0;
        while (j < keys.size() && keys[j] != v.keys[i]) ++j;
        if (j == keys.size()) {
            keys.push_back(v.keys[i]);
            vals.push_back(&v.vals[i]);
        } else {
            vals[j] = &v.vals[i];
        }
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        if (secretKey(keys[i]) || secretKept(*vals[i])) return true;
    }
    return false;
}

// --- expected replies -----------------------------------------------------------------------

struct Want {
    uint16_t status;
    int16_t code;
    int16_t alt; // another acceptable code with the same status (ArduinoJson float rounding), 0
    int64_t value;
};
Want okv(int64_t v) { return Want {200, 0, 0, v}; }
Want err(int16_t code, int16_t alt = 0) { return Want {400, code, alt, 0}; }

enum class Kind { Bool, Integer, FrequencyText, Tuning, Spread, OffState, DeviceOnly };
struct Field {
    const char* key;
    Kind kind;
    CommandType type;
};
// The documented order (rest_json.h): channel frequency before enabled; CH3 master and phase
// before enabled; config tuning before spread, then off state, drive, correction, step,
// backlight.
const Field CHANNEL[] = {{"frequency", Kind::Integer, CommandType::SetFrequency},
                         {"frequencyText", Kind::FrequencyText, CommandType::SetFrequency},
                         {"enabled", Kind::Bool, CommandType::SetEnabled}};
const Field CH3[] = {{"master", Kind::Integer, CommandType::Ch3Master},
                     {"phase", Kind::Integer, CommandType::Ch3Phase},
                     {"enabled", Kind::Bool, CommandType::SetEnabled}};
const Field CONFIG[] = {{"tuning", Kind::Tuning, CommandType::Tuning},
                        {"spread", Kind::Spread, CommandType::Spread},
                        {"offState", Kind::OffState, CommandType::OffState},
                        {"drive", Kind::Integer, CommandType::Drive},
                        {"correction", Kind::Integer, CommandType::Correction},
                        {"step", Kind::Integer, CommandType::Step},
                        {"backlight", Kind::Integer, CommandType::Backlight},
                        {"remoteOutputs", Kind::DeviceOnly, CommandType::SaveNow}};
const char* const TUNING[] = {"fractional", "lownoise", "integer"};
const char* const SPREAD[] = {"off", "down05", "down15", "center05"};
const char* const OFFSTATE[] = {"low", "high", "hiz"};

bool isInteger(const std::string& tok) { return tok.find_first_of(".eE") == std::string::npos; }

// An integer token in int64 (exact, no floating point).
bool int64Of(const std::string& tok, int64_t* out) {
    const bool neg = tok[0] == '-';
    const std::string d = tok.substr(neg ? 1 : 0);
    const char* limit = neg ? "9223372036854775808" : "9223372036854775807";
    if (d.size() > 19 || (d.size() == 19 && d > limit)) return false;
    uint64_t m = 0;
    for (size_t i = 0; i < d.size(); ++i) m = m * 10 + (uint64_t)(d[i] - '0');
    *out = neg ? (int64_t)(0 - m) : (int64_t)m;
    return true;
}

Want integer(const JV& v) {
    if (v.k != JV::Num) return err(DATA_TYPE_ERROR);
    if (isInteger(v.s)) {
        int64_t x = 0;
        return int64Of(v.s, &x) ? okv(x) : err(DATA_OUT_OF_RANGE);
    }
    // A float: a type error below 9.2e18, out of range from there. ArduinoJson computes the
    // value with its own rounding and gives inf/0 for huge exponents (also "0e400"): near the
    // limit and with exponents beyond 240 both answers count.
    const double x = fabs(strtod(v.s.c_str(), nullptr));
    const size_t e = v.s.find_first_of("eE");
    long exp = 0;
    if (e != std::string::npos) {
        const std::string digits = v.s.substr(v.s.find_first_of("0123456789", e));
        exp = digits.size() > 6 ? 999999 : atol(digits.c_str());
    }
    if (exp > 240 || (x > 9.19e18 && x < 9.21e18)) return err(DATA_TYPE_ERROR, DATA_OUT_OF_RANGE);
    return x >= 9.2e18 ? err(DATA_OUT_OF_RANGE) : err(DATA_TYPE_ERROR);
}

Want choice(const JV& v, const char* const* names, int count) {
    if (v.k != JV::Str) return err(DATA_TYPE_ERROR);
    for (int i = 0; i < count; ++i) {
        if (v.s == names[i]) return okv(i); // a NUL inside never matches
    }
    return err(ILLEGAL_PARAMETER_VALUE);
}

Want convert(const Field& f, const JV& v) {
    switch (f.kind) {
        case Kind::Bool:
            if (v.k == JV::True) return okv(1);
            if (v.k == JV::False) return okv(0);
            return err(DATA_TYPE_ERROR);
        case Kind::Integer: return integer(v);
        case Kind::FrequencyText: {
            if (v.k != JV::Str) return err(DATA_TYPE_ERROR);
            if (v.s.find('\0') != std::string::npos) return err(ILLEGAL_PARAMETER_VALUE);
            uint64_t hz = 0;
            bool outOfRange = false;
            if (!FrequencyUtils::parseRemoteFrequency(v.s.c_str(), &hz, &outOfRange)) {
                return err(outOfRange ? DATA_OUT_OF_RANGE : ILLEGAL_PARAMETER_VALUE);
            }
            return okv((int64_t)hz);
        }
        case Kind::Tuning: return choice(v, TUNING, 3);
        case Kind::Spread: return choice(v, SPREAD, 4);
        case Kind::OffState: return choice(v, OFFSTATE, 3);
        case Kind::DeviceOnly: return Want {403, SETTINGS_CONFLICT, 0, 0};
    }
    return err(DATA_TYPE_ERROR);
}

struct Body {
    const char* ptr; // what the function gets (may be nullptr)
    size_t len;
    bool valid;      // grammar ok and ArduinoJson reads it
    JV top;
};

// The common part of every request body: size, grammar, object, duplicates.
bool loadBody(const Body& b, Want* w) {
    if (b.len > MAX_BODY) {
        *w = Want {413, -223, 0, 0};
        return false;
    }
    if (!b.valid) {
        *w = err(SYNTAX_ERROR);
        return false;
    }
    if (b.top.k != JV::Obj) {
        *w = err(DATA_TYPE_ERROR);
        return false;
    }
    if (hasDuplicate(b.top)) {
        *w = err(SYNTAX_ERROR);
        return false;
    }
    return true;
}

// Expected reply and commands for a field table.
Want expectFields(const Field* fields, size_t count, uint8_t channel, const Body& b, std::vector<Command>* cmds) {
    cmds->clear();
    Want w {};
    if (!loadBody(b, &w)) return w;
    int64_t values[8] = {};
    unsigned seen = 0;
    for (size_t i = 0; i < b.top.keys.size(); ++i) {
        size_t f = 0;
        while (f < count && b.top.keys[i] != fields[f].key) ++f;
        if (f == count) return err(PARAMETER_NOT_ALLOWED);
        const Want c = convert(fields[f], b.top.vals[i]);
        if (c.status != 200) return c;
        values[f] = c.value;
        seen |= 1u << f;
    }
    if (seen == 0) return err(MISSING_PARAMETER);
    if (fields == CHANNEL && (seen & 3u) == 3u) return err(PARAMETER_NOT_ALLOWED);
    for (size_t f = 0; f < count; ++f) {
        if (!(seen & (1u << f))) continue;
        Command c {};
        c.type = fields[f].type;
        c.channel = channel;
        c.value = values[f];
        c.source = Source::Web;
        cmds->push_back(c);
    }
    return okv(0);
}

// --- checks ---------------------------------------------------------------------------------

void checkErrorJson(const Reply& r) {
    char buf[ERROR_SIZE + 8];
    memset(buf, '#', sizeof(buf));
    const size_t n = errorJson(r, buf, ERROR_SIZE);
    FUZZ_CHECK(n > 0 && n < ERROR_SIZE && buf[n] == '\0' && strlen(buf) == n);
    for (size_t i = ERROR_SIZE; i < sizeof(buf); ++i) FUZZ_CHECK(buf[i] == '#');
    JV doc;
    Ref p(buf, n);
    FUZZ_CHECK(p.parse(&doc) && doc.k == JV::Obj && doc.keys.size() == 1 && doc.keys[0] == "error");
    const char* text = r.info ? r.info : errorText(r.code);
    FUZZ_CHECK(text && text[0]);
    char want[128];
    snprintf(want, sizeof(want), "%d,%s", (int)r.code, text);
    FUZZ_CHECK(doc.vals[0].k == JV::Str && doc.vals[0].s == want);
    // Exactly n + 1 bytes are enough, n are not (and leave "").
    char tight[ERROR_SIZE + 8];
    memset(tight, '#', sizeof(tight));
    FUZZ_CHECK(errorJson(r, tight, n + 1) == n && memcmp(tight, buf, n + 1) == 0 && tight[n + 1] == '#');
    memset(tight, '#', sizeof(tight));
    FUZZ_CHECK(errorJson(r, tight, n) == 0 && tight[0] == '\0' && tight[n] == '#');
    FUZZ_CHECK(errorJson(r, nullptr, ERROR_SIZE) == 0);
}

void checkReply(const Reply& r, const Want& w, const Commands& out, const std::vector<Command>& cmds) {
    FUZZ_CHECK(r.status == w.status);
    FUZZ_CHECK(r.code == w.code || (w.alt != 0 && r.code == w.alt));
    FUZZ_CHECK(r.ok() == (r.status == 200) && (r.code == 0) == r.ok());
    if (!r.ok()) {
        FUZZ_CHECK(out.count == 0);
        checkErrorJson(r);
        return;
    }
    FUZZ_CHECK(out.count <= MAX_COMMANDS && out.count == cmds.size());
    for (uint8_t i = 0; i < out.count; ++i) {
        const Command& c = out.list[i];
        FUZZ_CHECK(c.type == cmds[i].type && c.channel == cmds[i].channel && c.value == cmds[i].value);
        FUZZ_CHECK(c.source == Source::Web);
        if (i > 0) FUZZ_CHECK(c.type != out.list[i - 1].type); // one command per setting
        switch (c.type) {
            case CommandType::SetEnabled: FUZZ_CHECK(c.value == 0 || c.value == 1); break;
            case CommandType::Tuning: FUZZ_CHECK(c.value >= 0 && c.value <= 2); break;
            case CommandType::Spread: FUZZ_CHECK(c.value >= 0 && c.value <= 3); break;
            case CommandType::OffState: FUZZ_CHECK(c.value >= 0 && c.value <= 2); break;
            case CommandType::SaveNow:
            case CommandType::Identify: FUZZ_CHECK(c.value == 0 && c.channel == 0); break;
            default: break; // integers in the full type: the controller checks the range
        }
    }
}

Commands freshCommands() {
    Commands c;
    memset(&c, 0xA5, sizeof(c)); // garbage: count must be set on every path
    return c;
}

void requestBody(uint8_t op, uint8_t param, const Body& b) {
    Commands out = freshCommands();
    std::vector<Command> cmds;
    Want w {};
    Reply r {};
    switch (op) {
        case 0:
        case 1:
        case 2: {
            const uint8_t ch = op == 2 ? param : (uint8_t)(op + 1);
            r = parseChannel(ch, b.ptr, b.len, &out);
            w = (ch == 1 || ch == 2) ? expectFields(CHANNEL, 3, ch, b, &cmds) : Want {404, -114, 0, 0};
            break;
        }
        case 3:
            r = parseCh3(b.ptr, b.len, &out);
            w = expectFields(CH3, 3, 3, b, &cmds);
            break;
        case 4:
            r = parseConfig(b.ptr, b.len, &out);
            w = expectFields(CONFIG, 8, 0, b, &cmds);
            break;
        default: {
            const CommandType type = op == 5 ? CommandType::SaveNow
                                   : op == 6 ? CommandType::Identify
                                             : (CommandType)(param % 16);
            r = parseAction(type, b.ptr, b.len, &out);
            if (type != CommandType::SaveNow && type != CommandType::Identify) {
                w = err(ILLEGAL_PARAMETER_VALUE);
            } else if (loadBody(b, &w)) {
                if (b.top.keys.empty()) {
                    w = okv(0);
                    Command c {};
                    c.type = type;
                    c.source = Source::Web;
                    cmds.push_back(c);
                } else {
                    w = err(PARAMETER_NOT_ALLOWED);
                }
            }
            break;
        }
    }
    checkReply(r, w, out, cmds);
    // Without an output: refused, nothing written.
    if (op <= 2) FUZZ_CHECK(!parseChannel(1, b.ptr, b.len, nullptr).ok());
    if (op == 3) FUZZ_CHECK(!parseCh3(b.ptr, b.len, nullptr).ok());
    if (op == 4) FUZZ_CHECK(!parseConfig(b.ptr, b.len, nullptr).ok());
    if (r.ok() && op <= 2) {
        // frequencyText only yields frequencies inside the device range (parseRemoteFrequency).
        const JV* t = member(b.top, "frequencyText");
        if (t) {
            FUZZ_CHECK(out.count >= 1 && out.list[0].type == CommandType::SetFrequency);
            FUZZ_CHECK(out.list[0].value >= (int64_t)FREQ_MIN_HZ && out.list[0].value <= (int64_t)FREQ_MAX_HZ);
        }
    }
}

void login(uint8_t param, const Body& b) {
    const size_t size = param % 80;
    std::vector<char> buf(size + 8, '#');
    size_t len = 12345;
    const Reply r = parseLogin(b.ptr, b.len, size ? buf.data() : nullptr, size, &len);
    for (size_t i = size; i < buf.size(); ++i) FUZZ_CHECK(buf[i] == '#');
    Want w {};
    std::string password;
    if (size == 0) {
        w = err(SYNTAX_ERROR);
    } else if (loadBody(b, &w)) {
        w = err(MISSING_PARAMETER);
        for (size_t i = 0; i < b.top.keys.size(); ++i) {
            const JV& v = b.top.vals[i];
            if (b.top.keys[i] != "password") {
                w = err(PARAMETER_NOT_ALLOWED);
                break;
            }
            if (v.k != JV::Str) {
                w = err(DATA_TYPE_ERROR);
                break;
            }
            if (v.s.find('\0') != std::string::npos) {
                w = err(ILLEGAL_PARAMETER_VALUE);
                break;
            }
            if (v.s.size() >= size) {
                w = err(DATA_OUT_OF_RANGE);
                break;
            }
            password = v.s;
            w = okv(0);
        }
    }
    FUZZ_CHECK(r.status == w.status && r.code == w.code);
    if (r.ok()) {
        FUZZ_CHECK(len == password.size() && memcmp(buf.data(), password.data(), len) == 0 && buf[len] == '\0');
    } else {
        checkErrorJson(r);
        if (size) FUZZ_CHECK(memchr(buf.data(), '\0', size) != nullptr);
        if (size == 0) FUZZ_CHECK(len == 0);
    }
}

void secrets(const Body& b, bool rawSecret) {
    const bool got = carriesSecret(b.ptr, b.len);
    // Every key as written, at any depth (also under duplicate keys); what ArduinoJson keeps is
    // a part of it.
    const bool want = b.len <= MAX_BODY && rawSecret;
    FUZZ_CHECK(got == want);
    FUZZ_CHECK(!(b.len <= MAX_BODY && b.valid && secretKept(b.top)) || got);
}

// --- state ----------------------------------------------------------------------------------

struct Reader {
    const uint8_t* d;
    size_t n, i;
    uint8_t u8() { return i < n ? d[i++] : 0; }
    uint16_t u16() { return (uint16_t)(u8() | u8() << 8); }
    uint32_t u32() { return (uint32_t)u16() | (uint32_t)u16() << 16; }
    uint64_t u64() { return (uint64_t)u32() | (uint64_t)u32() << 32; }
    double f64() {
        const uint64_t b = u64();
        double v;
        memcpy(&v, &b, sizeof(v));
        return v;
    }
    void raw(char* out, size_t k) {
        for (size_t j = 0; j < k; ++j) out[j] = (char)u8();
    }
};

std::string u64s(uint64_t v) {
    char b[24];
    snprintf(b, sizeof(b), "%llu", (unsigned long long)v);
    return b;
}
std::string i64s(int64_t v) {
    char b[24];
    snprintf(b, sizeof(b), "%lld", (long long)v);
    return b;
}

std::string sanitized(const char* in, size_t maxIn) {
    size_t n = 0;
    while (n < maxIn && in[n]) ++n;
    char out[33];
    Sanitize::text(in, n, out, sizeof(out), 32);
    return out;
}

std::string actualOf(double hz) {
    if (!(hz >= 0.0 && hz < 1e12)) hz = 0.0;
    char b[32];
    snprintf(b, sizeof(b), "%.1f", hz);
    return b;
}

// Every string of the document: strict UTF-8 without control characters.
void checkStrings(const JV& v) {
    if (v.k == JV::Str) FUZZ_CHECK(fuzzValidUtf8(v.s.data(), v.s.size(), true));
    for (size_t i = 0; i < v.vals.size(); ++i) {
        if (v.k == JV::Obj) FUZZ_CHECK(fuzzValidUtf8(v.keys[i].data(), v.keys[i].size(), true));
        checkStrings(v.vals[i]);
    }
}

const JV& need(const JV& o, const char* key, JV::Kind k) {
    const JV* v = o.k == JV::Obj ? member(o, key) : nullptr;
    if (!v || v->k != k) fuzzFail(__FILE__, __LINE__, key);
    return *v;
}
void needNum(const JV& o, const char* key, const std::string& value) { FUZZ_CHECK(need(o, key, JV::Num).s == value); }
void needBool(const JV& o, const char* key, bool value) {
    const JV* v = member(o, key);
    FUZZ_CHECK(v && v->k == (value ? JV::True : JV::False));
}
void needStr(const JV& o, const char* key, const std::string& value) { FUZZ_CHECK(need(o, key, JV::Str).s == value); }
bool oneOf(const std::string& s, const char* const* list, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (s == list[i]) return true;
    }
    return false;
}

void checkModes(const JV& modes, bool ch1) {
    FUZZ_CHECK(modes.k == JV::Arr && modes.vals.size() <= 4);
    static const char* const FIXED[] = {"OFF=High", "OFF=High-Z", "Low noise", "Integer"};
    static const char* const SSC[] = {"SSC -0.5%", "SSC -1.5%", "SSC +/-0.5%"};
    for (const JV& m : modes.vals) {
        FUZZ_CHECK(m.k == JV::Str);
        const bool drive = m.s.size() >= 3 && m.s.compare(m.s.size() - 2, 2, "mA") == 0;
        FUZZ_CHECK(oneOf(m.s, FIXED, 4) || (ch1 && oneOf(m.s, SSC, 3)) || drive);
    }
}

// Values within what the device can produce (frequencies <= 160 MHz, drive 2..8 mA, step and
// correction from their lists, version APP_VERSION): the state must fit STATE_SIZE. Any other
// values must only give 0 when they do not fit.
void deviceRanges(Snapshot& s, DeviceInfo& di) {
    static const uint16_t PHASES[] = {0, 90, 180, 270};
    s.state.ch1.frequency %= FREQ_MAX_HZ + 1;
    s.state.ch2.frequency %= FREQ_MAX_HZ + 1;
    s.state.ch3.master = (uint8_t)(1 + s.state.ch3.master % 2);
    s.state.ch3.phase = PHASES[s.state.ch3.phase % 4];
    s.state.sys.delta_step = DELTA_STEPS[s.state.sys.delta_step % DELTA_STEP_COUNT];
    s.state.sys.freq_correction %= CORRECTION_MAX + 1;
    s.state.sys.backlight = (uint8_t)(10 + s.state.sys.backlight % 91);
    s.state.sys.drive_ma = (uint8_t)(2 + 2 * (s.state.sys.drive_ma % 4));
    for (double& a : s.actualHz) {
        if (!(a >= 0.0 && a <= 2.0 * FREQ_MAX_HZ)) a = fmod(fabs(a), 2.0 * FREQ_MAX_HZ);
        if (!(a >= 0.0)) a = 0.0; // NaN
    }
    s.iqMinHz %= FREQ_MAX_HZ + 1;
    s.iqMaxHz %= FREQ_MAX_HZ + 1;
    di.rssi = -(int32_t)((uint32_t)di.rssi % 128);
    di.version = nullptr;
}

void state(const uint8_t* d, size_t n) {
    Reader rd {d, n, 0};
    const bool device = !(rd.u8() & 1);
    Snapshot s {};
    s.state.ch1.frequency = rd.u64();
    s.state.ch1.enabled = rd.u8() & 1;
    s.state.ch2.frequency = rd.u64();
    s.state.ch2.enabled = rd.u8() & 1;
    s.state.ch3.enabled = rd.u8() & 1;
    s.state.ch3.master = rd.u8();
    s.state.ch3.phase = rd.u16();
    s.state.sys.delta_step = rd.u32();
    s.state.sys.freq_correction = (int32_t)rd.u32();
    s.state.sys.backlight = rd.u8();
    s.state.sys.drive_ma = rd.u8();
    // Enums as the controller sets them (in range).
    s.state.sys.tuning_mode = (TuningMode)(rd.u8() % 3);
    s.state.sys.spread = (Spread)(rd.u8() % 4);
    s.state.sys.off_state = (OffState)(rd.u8() % 3);
    s.status[0] = (ChannelStatus)(rd.u8() % 4);
    s.status[1] = (ChannelStatus)(rd.u8() % 4);
    s.ch3Status = (Ch3Status)(rd.u8() % 7);
    for (double& a : s.actualHz) a = rd.f64(); // any bits: NaN, inf, negative
    s.hardwareReady = rd.u8() & 1;
    s.link = (LinkState)(rd.u8() % 4);
    s.iqMinHz = rd.u64();
    s.iqMaxHz = rd.u64();
    s.remoteOutputsAllowed = rd.u8() & 1;
    s.revision = rd.u32();
    s.saveState = (SaveState)(rd.u8() % 4);
    s.lastRemote = (Source)(rd.u8() % 5);
    s.lastRemoteMs = rd.u32();
    DeviceInfo di {};
    di.mode = (WifiMode)(rd.u8() % 3);
    di.connected = rd.u8() & 1;
    rd.raw(di.ssid, sizeof(di.ssid)); // raw bytes, not necessarily terminated
    di.rssi = (int32_t)rd.u32();
    di.ip = rd.u32();
    rd.raw(di.name, sizeof(di.name));
    di.apActive = rd.u8() & 1;
    rd.raw(di.apSsid, sizeof(di.apSsid));
    di.apOpen = rd.u8() & 1;
    di.apStations = rd.u8();
    const uint8_t vlen = rd.u8();
    std::string version;
    for (uint8_t k = 0; k < vlen % 48; ++k) version += (char)rd.u8();
    di.version = (vlen & 0x80) ? nullptr : version.c_str();
    const uint32_t now = rd.u32();
    const uint8_t sizeSel = rd.u8();
    if (device) deviceRanges(s, di);

    // Built into a large buffer first (the size only limits the output); with device values the
    // documented STATE_SIZE is always enough.
    const size_t BIG = 4096;
    std::vector<char> buf(BIG + 16, '#');
    const size_t len = buildState(s, di, now, buf.data(), BIG);
    FUZZ_CHECK(len > 0 && len < BIG && buf[len] == '\0' && strlen(buf.data()) == len);
    for (size_t i = BIG; i < buf.size(); ++i) FUZZ_CHECK(buf[i] == '#');
    if (device) FUZZ_CHECK(len < STATE_SIZE);
    for (size_t i = 0; i < len; ++i) FUZZ_CHECK((unsigned char)buf[i] >= 0x20); // no raw control bytes

    JV doc;
    Ref p(buf.data(), len);
    FUZZ_CHECK(p.parse(&doc) && !p.ajFails && doc.k == JV::Obj && !hasDuplicate(doc));
    {
        JsonDocument again;
        FUZZ_CHECK(deserializeJson(again, buf.data(), len) == DeserializationError::Ok);
        FUZZ_CHECK(again.is<JsonObjectConst>() && again.as<JsonObjectConst>().size() == doc.keys.size());
    }
    checkStrings(doc);
    static const char* const ORDER[] = {"rev", "hw", "save", "remote", "ch", "ch3", "config", "wifi", "device"};
    FUZZ_CHECK(doc.keys.size() == 9);
    for (size_t i = 0; i < 9; ++i) FUZZ_CHECK(doc.keys[i] == ORDER[i]);

    needNum(doc, "rev", u64s(s.revision));
    static const char* const HW[] = {"CONN", "BLOC", "UNKN", "NFO"};
    FUZZ_CHECK(oneOf(need(doc, "hw", JV::Str).s, HW, 4));
    static const char* const SAVE[] = {"saved", "pending", "failed", "unavailable"};
    FUZZ_CHECK(oneOf(need(doc, "save", JV::Str).s, SAVE, 4));
    const char* remote = s.lastRemote == Source::Web ? "web"
                       : s.lastRemote == Source::Scpi ? "scpi"
                       : s.lastRemote == Source::Usb  ? "usb"
                                                      : nullptr;
    if (remote && now - s.lastRemoteMs < REMOTE_INDICATOR_MS) needStr(doc, "remote", remote);
    else need(doc, "remote", JV::Null);

    static const char* const CH_STATUS[] = {"ON", "OFF", "ERR", "NOHW"};
    const JV& ch = need(doc, "ch", JV::Arr);
    FUZZ_CHECK(ch.vals.size() == 2);
    for (int k = 0; k < 2; ++k) {
        const JV& o = ch.vals[k];
        const Channel& c = k == 0 ? s.state.ch1 : s.state.ch2;
        FUZZ_CHECK(o.k == JV::Obj && o.keys.size() == 6);
        needNum(o, "n", u64s((uint64_t)k + 1));
        needBool(o, "enabled", c.enabled);
        needNum(o, "frequency", u64s(c.frequency));
        needNum(o, "actual", actualOf(s.actualHz[k]));
        FUZZ_CHECK(oneOf(need(o, "status", JV::Str).s, CH_STATUS, 4));
        checkModes(need(o, "modes", JV::Arr), k == 0);
    }
    const JV& ch3 = need(doc, "ch3", JV::Obj);
    needBool(ch3, "enabled", s.state.ch3.enabled);
    needNum(ch3, "master", u64s(s.state.ch3.master));
    needNum(ch3, "phase", u64s(s.state.ch3.phase));
    static const char* const CH3_STATUS[] = {"ON", "WAIT", "ERR", "NOHW", "OFF"};
    FUZZ_CHECK(oneOf(need(ch3, "status", JV::Str).s, CH3_STATUS, 5));
    needNum(ch3, "frequency", u64s(s.state.ch3.master == 2 ? s.state.ch2.frequency : s.state.ch1.frequency));
    needNum(ch3, "actual", actualOf(s.actualHz[2]));
    needNum(ch3, "iqMin", u64s(s.iqMinHz));
    needNum(ch3, "iqMax", u64s(s.iqMaxHz));

    const JV& cfg = need(doc, "config", JV::Obj);
    needNum(cfg, "drive", u64s(s.state.sys.drive_ma));
    needStr(cfg, "tuning", TUNING[(int)s.state.sys.tuning_mode]);
    needStr(cfg, "spread", SPREAD[(int)s.state.sys.spread]);
    needStr(cfg, "offState", OFFSTATE[(int)s.state.sys.off_state]);
    needNum(cfg, "correction", i64s(s.state.sys.freq_correction));
    needNum(cfg, "step", u64s(s.state.sys.delta_step));
    needNum(cfg, "backlight", u64s(s.state.sys.backlight));
    needStr(cfg, "remoteOutputs", s.remoteOutputsAllowed ? "allowed" : "local");

    const JV& wifi = need(doc, "wifi", JV::Obj);
    static const char* const MODES[] = {"off", "client", "ap"};
    needStr(wifi, "mode", MODES[(int)di.mode]);
    needBool(wifi, "connected", di.connected);
    needStr(wifi, "ssid", sanitized(di.ssid, sizeof(di.ssid)));
    needNum(wifi, "rssi", i64s(di.rssi));
    char ip[16] = "";
    if (di.ip) {
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", (unsigned)(di.ip >> 24), (unsigned)(di.ip >> 16 & 255),
                 (unsigned)(di.ip >> 8 & 255), (unsigned)(di.ip & 255));
    }
    needStr(wifi, "ip", ip);
    needStr(wifi, "name", sanitized(di.name, sizeof(di.name)));
    const JV& ap = need(wifi, "ap", JV::Obj);
    needBool(ap, "active", di.apActive);
    needStr(ap, "ssid", sanitized(di.apSsid, sizeof(di.apSsid)));
    needBool(ap, "open", di.apOpen);
    needNum(ap, "stations", u64s(di.apStations));
    const JV& dev = need(doc, "device", JV::Obj);
    needStr(dev, "version", sanitized(di.version ? di.version : APP_VERSION, 32));

    // STATE_SIZE and a smaller buffer: the same text when it fits, else 0 and ""; never
    // written past.
    const size_t sizes[] = {STATE_SIZE, sizeSel < 128 ? len + 1 - sizeSel % 2 : (size_t)(sizeSel - 128) * 9 % (STATE_SIZE + 1)};
    for (size_t outSize : sizes) {
        std::vector<char> small(outSize + 16, '#');
        const size_t k = buildState(s, di, now, outSize ? small.data() : nullptr, outSize);
        if (outSize > len) {
            FUZZ_CHECK(k == len && memcmp(small.data(), buf.data(), len + 1) == 0);
        } else {
            FUZZ_CHECK(k == 0);
            if (outSize) FUZZ_CHECK(small[0] == '\0');
        }
        for (size_t i = outSize; i < small.size(); ++i) FUZZ_CHECK(small[i] == '#');
    }
    // Zero-sized but present buffer: nothing written.
    char none = '#';
    FUZZ_CHECK(buildState(s, di, now, &none, 0) == 0 && none == '#');
}

void outcomes(const uint8_t* d, size_t n) {
    const PushResult p = (PushResult)(d[0] % 3);
    const Result res = (Result)(d[1] % 7);
    struct Row {
        uint16_t status;
        int16_t code;
    };
    static const Row PUSH[] = {{200, 0}, {503, EXECUTION_ERROR}, {429, EXECUTION_ERROR}};
    static const Row RES[] = {{200, 0},
                              {200, 0},
                              {400, DATA_OUT_OF_RANGE},
                              {400, ILLEGAL_PARAMETER_VALUE},
                              {400, SETTINGS_CONFLICT},
                              {403, SETTINGS_CONFLICT},
                              {500, HARDWARE_ERROR}};
    const Reply a = fromPush(p);
    const Reply b = fromResult(res);
    const Reply c = fromOutcome(p, res);
    FUZZ_CHECK(a.status == PUSH[(int)p].status && a.code == PUSH[(int)p].code);
    FUZZ_CHECK(b.status == RES[(int)res].status && b.code == RES[(int)res].code);
    const Row& want = a.ok() ? RES[(int)res] : PUSH[(int)p];
    FUZZ_CHECK(c.status == want.status && c.code == want.code);
    for (const Reply* r : {&a, &b, &c}) {
        if (!r->ok()) checkErrorJson(*r);
    }
    const std::string path(reinterpret_cast<const char*>(d + 2), n - 2);
    const std::string cpath(path.c_str());
    const uint8_t ch = channelFromPath(path.c_str());
    FUZZ_CHECK(ch == (cpath == "/api/channel/1" ? 1 : cpath == "/api/channel/2" ? 2 : 0));
    FUZZ_CHECK(channelFromPath(nullptr) == 0);
}

int run(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    const uint8_t op = (uint8_t)((data[0] & 0x0F) % 12);
    if (op == 10) {
        state(data + 1, size - 1);
        return 0;
    }
    if (op == 11) {
        if (size >= 3) outcomes(data + 1, size - 1);
        return 0;
    }
    const bool hasParam = op == 2 || op == 7 || op == 8;
    if (hasParam && size < 2) return 0;
    const uint8_t param = hasParam ? data[1] : 0;
    const size_t start = hasParam ? 2 : 1;
    std::string text(reinterpret_cast<const char*>(data + start), size - start);
    if (data[0] & 0x20) text.resize(MAX_BODY + 1, ' ');
    else if (data[0] & 0x10) text.resize(MAX_BODY, ' ');
    // Exactly the body bytes on the heap, no terminating NUL (ASan sees reads past the end).
    std::vector<char> bytes(text.begin(), text.end());
    Body b;
    b.len = bytes.size();
    b.ptr = (data[0] & 0x40) || bytes.empty() ? nullptr : bytes.data();
    const char* src = b.ptr ? b.ptr : "";
    const size_t srcLen = b.ptr ? b.len : 0;
    Ref p(src, srcLen);
    const bool grammar = p.parse(&b.top);
    b.valid = grammar && !p.ajFails;
    // carriesSecret() checks the own strict grammar only: a body ArduinoJson cannot read (a
    // number over 63 characters) still counts as carrying a secret and is refused (CI 2026-11-09).
    const bool raw = grammar && secretRaw(b.top);

    secrets(b, raw);
    if (op == 8) login(param, b);
    else if (op != 9) requestBody(op, param, b);
    return 0;
}

} // namespace fuzz_rest_json

FUZZ_TARGET(rest_json, fuzz_rest_json::run)
