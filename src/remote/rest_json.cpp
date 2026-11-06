#include "rest_json.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ArduinoJson.h>
#include <config.h>
#include <constants.h>
#include <utils/frequency_utils.h>
#include <utils/sanitize.h>
#include <utils/status_text.h>

namespace RestJson {

namespace {

static constexpr int16_t HEADER_SUFFIX_OUT_OF_RANGE = -114;
static constexpr int16_t TOO_MUCH_DATA = -223;

Reply ok() { return Reply {200, 0, nullptr}; }
Reply fail(uint16_t status, int16_t code, const char* info = nullptr) { return Reply {status, code, info}; }
Reply bad(int16_t code, const char* info = nullptr) { return fail(400, code, info); }

// ---------------------------------------------------------------------------
// Strict JSON grammar (RFC 8259) before ArduinoJson: one value, only whitespace after it,
// nesting <= MAX_NESTING, keys and strings in double quotes, escapes as specified.

class Grammar {
public:
    Grammar(const char* s, size_t n) : s_(s), n_(n) {}
    bool check() {
        ws();
        if (!value(0)) return false;
        ws();
        return i_ == n_;
    }
    // Members of a top-level object as written (ArduinoJson merges duplicates).
    size_t topKeys() const { return topKeys_; }

private:
    static bool digit(char c) { return c >= '0' && c <= '9'; }
    static bool hex(char c) { return digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
    bool at(char c) const { return i_ < n_ && s_[i_] == c; }
    bool eat(char c) {
        if (!at(c)) return false;
        ++i_;
        return true;
    }
    void ws() {
        while (i_ < n_ && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }
    bool digits() {
        const size_t start = i_;
        while (i_ < n_ && digit(s_[i_])) ++i_;
        return i_ > start;
    }
    bool literal(const char* w) {
        const size_t k = strlen(w);
        if (n_ - i_ < k || memcmp(s_ + i_, w, k) != 0) return false;
        i_ += k;
        return true;
    }
    bool value(uint8_t depth) {
        if (i_ >= n_) return false;
        switch (s_[i_]) {
            case '{': return object((uint8_t)(depth + 1));
            case '[': return array((uint8_t)(depth + 1));
            case '"': return string();
            case 't': return literal("true");
            case 'f': return literal("false");
            case 'n': return literal("null");
            default: return number();
        }
    }
    bool object(uint8_t depth) {
        if (depth > MAX_NESTING) return false;
        ++i_;
        ws();
        if (eat('}')) return true;
        for (;;) {
            if (!at('"') || !string()) return false;
            if (depth == 1) ++topKeys_;
            ws();
            if (!eat(':')) return false;
            ws();
            if (!value(depth)) return false;
            ws();
            if (eat('}')) return true;
            if (!eat(',')) return false;
            ws();
        }
    }
    bool array(uint8_t depth) {
        if (depth > MAX_NESTING) return false;
        ++i_;
        ws();
        if (eat(']')) return true;
        for (;;) {
            if (!value(depth)) return false;
            ws();
            if (eat(']')) return true;
            if (!eat(',')) return false;
            ws();
        }
    }
    bool string() {
        ++i_; // opening quote
        while (i_ < n_) {
            const unsigned char c = (unsigned char)s_[i_++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') continue;
            if (i_ >= n_) return false;
            const char e = s_[i_++];
            if (e == 'u') {
                for (int k = 0; k < 4; ++k) {
                    if (i_ >= n_ || !hex(s_[i_])) return false;
                    ++i_;
                }
            } else if (e == '\0' || !strchr("\"\\/bfnrt", e)) {
                return false;
            }
        }
        return false;
    }
    bool number() {
        eat('-');
        if (!eat('0')) {
            if (!(i_ < n_ && s_[i_] >= '1' && s_[i_] <= '9')) return false;
            digits();
        }
        if (eat('.') && !digits()) return false;
        if (at('e') || at('E')) {
            ++i_;
            if (!eat('+')) eat('-');
            if (!digits()) return false;
        }
        return true;
    }

    const char* s_;
    size_t n_;
    size_t i_ = 0;
    size_t topKeys_ = 0;
};

// Grammar, then ArduinoJson; any JSON value.
Reply parse(const char* body, size_t len, JsonDocument& doc, size_t* topKeys) {
    if (len > MAX_BODY) return fail(413, TOO_MUCH_DATA);
    const char* src = body ? body : "";
    if (!body) len = 0;
    Grammar g(src, len);
    if (!g.check()) return bad(SYNTAX_ERROR);
    const DeserializationError e = deserializeJson(doc, src, len, DeserializationOption::NestingLimit(MAX_NESTING));
    if (e == DeserializationError::NoMemory) return fail(503, EXECUTION_ERROR, "Out of memory");
    if (e) return bad(SYNTAX_ERROR);
    if (topKeys) *topKeys = g.topKeys();
    return ok();
}

// A request body: an object without duplicate keys.
Reply load(const char* body, size_t len, JsonDocument& doc) {
    size_t keys = 0;
    const Reply r = parse(body, len, doc, &keys);
    if (!r.ok()) return r;
    if (!doc.is<JsonObjectConst>()) return bad(DATA_TYPE_ERROR);
    if (doc.as<JsonObjectConst>().size() != keys) return bad(SYNTAX_ERROR, "Duplicate key");
    return ok();
}

bool keyIs(JsonString key, const char* name) {
    const size_t n = strlen(name);
    return key.size() == n && memcmp(key.c_str(), name, n) == 0; // a NUL inside never matches
}

// A JSON integer in the full type (W-S19): 4.5, 4.0, 1e3, "4", true, null are no integers.
Reply integer(JsonVariantConst v, int64_t* out) {
    if (v.is<bool>()) return bad(DATA_TYPE_ERROR);
    if (v.is<int64_t>()) {
        *out = v.as<int64_t>();
        return ok();
    }
    if (v.is<uint64_t>()) return bad(DATA_OUT_OF_RANGE); // above INT64_MAX
    // Integers beyond 64 bit arrive as float, rounded (ArduinoJson drops digits, so 2^63 + 1
    // may come out just below 2^63): from 9.2e18 on a float counts as such an integer.
    if (v.is<double>() && fabs(v.as<double>()) >= 9.2e18) return bad(DATA_OUT_OF_RANGE);
    return bad(DATA_TYPE_ERROR);
}

Reply text(JsonVariantConst v, JsonString* out) {
    if (!v.is<JsonString>()) return bad(DATA_TYPE_ERROR);
    const JsonString s = v.as<JsonString>();
    if (!s.c_str() || strlen(s.c_str()) != s.size()) return bad(ILLEGAL_PARAMETER_VALUE); // "\u0000"
    *out = s;
    return ok();
}

const char* const TUNING_NAMES[] = {"fractional", "lownoise", "integer"};
const char* const SPREAD_NAMES[] = {"off", "down05", "down15", "center05"};
const char* const OFFSTATE_NAMES[] = {"low", "high", "hiz"};

// Index in names = enum value (TuningMode, Spread, OffState).
Reply choice(JsonVariantConst v, const char* const* names, uint8_t count, int64_t* out) {
    JsonString s;
    const Reply r = text(v, &s);
    if (!r.ok()) return r;
    for (uint8_t i = 0; i < count; ++i) {
        if (keyIs(s, names[i])) {
            *out = i;
            return ok();
        }
    }
    return bad(ILLEGAL_PARAMETER_VALUE);
}

enum class Kind : uint8_t { Bool, Integer, FrequencyText, Tuning, Spread, OffState, DeviceOnly };

struct Field {
    const char* key;
    Kind kind;
    CommandType type;
};

// In execution order.
const Field CHANNEL_FIELDS[] = {
    {"frequency", Kind::Integer, CommandType::SetFrequency},
    {"frequencyText", Kind::FrequencyText, CommandType::SetFrequency},
    {"enabled", Kind::Bool, CommandType::SetEnabled},
};
const Field CH3_FIELDS[] = {
    {"master", Kind::Integer, CommandType::Ch3Master},
    {"phase", Kind::Integer, CommandType::Ch3Phase},
    {"enabled", Kind::Bool, CommandType::SetEnabled},
};
const Field CONFIG_FIELDS[] = {
    {"tuning", Kind::Tuning, CommandType::Tuning},
    {"spread", Kind::Spread, CommandType::Spread},
    {"offState", Kind::OffState, CommandType::OffState},
    {"drive", Kind::Integer, CommandType::Drive},
    {"correction", Kind::Integer, CommandType::Correction},
    {"step", Kind::Integer, CommandType::Step},
    {"backlight", Kind::Integer, CommandType::Backlight},
    {"remoteOutputs", Kind::DeviceOnly, CommandType::SaveNow}, // never a command
};

template <size_t N>
constexpr uint8_t countOf(const Field (&)[N]) {
    return (uint8_t)N;
}
static_assert(sizeof(CONFIG_FIELDS) / sizeof(CONFIG_FIELDS[0]) <= MAX_COMMANDS, "MAX_COMMANDS");

Reply convert(const Field& f, JsonVariantConst v, int64_t* out) {
    switch (f.kind) {
        case Kind::Bool:
            if (!v.is<bool>()) return bad(DATA_TYPE_ERROR);
            *out = v.as<bool>() ? 1 : 0;
            return ok();
        case Kind::Integer: return integer(v, out);
        case Kind::FrequencyText: {
            JsonString s;
            const Reply r = text(v, &s);
            if (!r.ok()) return r;
            uint64_t hz = 0;
            if (!FrequencyUtils::parseRemoteFrequency(s.c_str(), &hz)) return bad(ILLEGAL_PARAMETER_VALUE);
            *out = (int64_t)hz;
            return ok();
        }
        case Kind::Tuning: return choice(v, TUNING_NAMES, 3, out);
        case Kind::Spread: return choice(v, SPREAD_NAMES, 4, out);
        case Kind::OffState: return choice(v, OFFSTATE_NAMES, 3, out);
        case Kind::DeviceOnly: return fail(403, SETTINGS_CONFLICT, "Remote outputs can only be changed at the device");
    }
    return bad(DATA_TYPE_ERROR);
}

struct Values {
    int64_t value[MAX_COMMANDS];
    uint16_t seen; // bit per field
};

// Every key known, typed and converted (first problem in key order answers); at least one.
Reply read(const Field* fields, uint8_t count, const char* body, size_t len, Values* v) {
    v->seen = 0;
    JsonDocument doc;
    Reply r = load(body, len, doc);
    if (!r.ok()) return r;
    for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
        int idx = -1;
        for (uint8_t i = 0; i < count && idx < 0; ++i) {
            if (keyIs(kv.key(), fields[i].key)) idx = i;
        }
        if (idx < 0) return bad(PARAMETER_NOT_ALLOWED);
        if (v->seen & (1u << idx)) return bad(SYNTAX_ERROR, "Duplicate key");
        v->seen = (uint16_t)(v->seen | (1u << idx));
        r = convert(fields[idx], kv.value(), &v->value[idx]);
        if (!r.ok()) return r;
    }
    if (v->seen == 0) return bad(MISSING_PARAMETER);
    return ok();
}

void emit(const Field* fields, uint8_t count, const Values& v, uint8_t channel, Commands* out) {
    out->count = 0;
    for (uint8_t i = 0; i < count && out->count < MAX_COMMANDS; ++i) {
        if (!(v.seen & (1u << i))) continue;
        Command& c = out->list[out->count++];
        c = Command {};
        c.type = fields[i].type;
        c.channel = channel;
        c.value = v.value[i];
        c.source = Source::Web;
    }
}

Reply parseWith(const Field* fields, uint8_t count, uint8_t channel, const char* body, size_t len, Commands* out) {
    if (!out) return bad(SYNTAX_ERROR);
    out->count = 0;
    Values v {};
    const Reply r = read(fields, count, body, len, &v);
    if (!r.ok()) return r;
    emit(fields, count, v, channel, out);
    return ok();
}

bool containsCi(const char* s, size_t n, const char* needle) {
    const size_t k = strlen(needle);
    for (size_t i = 0; i + k <= n; ++i) {
        size_t j = 0;
        while (j < k) {
            char c = s[i + j];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            if (c != needle[j]) break;
            ++j;
        }
        if (j == k) return true;
    }
    return false;
}

bool secretKey(JsonString key) {
    static const char* const NAMES[] = {"pass", "psk", "secret", "key"};
    for (const char* n : NAMES) {
        if (containsCi(key.c_str(), key.size(), n)) return true;
    }
    return false;
}

// Depth is bounded by the grammar (MAX_NESTING).
bool hasSecret(JsonVariantConst v) {
    if (v.is<JsonObjectConst>()) {
        for (JsonPairConst kv : v.as<JsonObjectConst>()) {
            if (secretKey(kv.key()) || hasSecret(kv.value())) return true;
        }
    } else if (v.is<JsonArrayConst>()) {
        for (JsonVariantConst e : v.as<JsonArrayConst>()) {
            if (hasSecret(e)) return true;
        }
    }
    return false;
}

// --- state ------------------------------------------------------------------

const char* linkName(LinkState l) {
    switch (l) {
        case LinkState::Connected: return "CONN";
        case LinkState::Blocked: return "BLOC";
        case LinkState::UnknownDevice: return "UNKN";
        case LinkState::NotFound: break;
    }
    return "NFO";
}

const char* saveName(SaveState s) {
    switch (s) {
        case SaveState::Pending: return "pending";
        case SaveState::Failed: return "failed";
        case SaveState::Unavailable: return "unavailable";
        case SaveState::Saved: break;
    }
    return "saved";
}

const char* remoteName(Source s) {
    switch (s) {
        case Source::Web: return "web";
        case Source::Scpi: return "scpi";
        case Source::Usb: return "usb";
        case Source::Touch:
        case Source::Internal: break;
    }
    return nullptr;
}

const char* channelStatusName(ChannelStatus s) {
    switch (s) {
        case ChannelStatus::On: return "ON";
        case ChannelStatus::Error: return "ERR";
        case ChannelStatus::NoDevice: return "NOHW";
        case ChannelStatus::Off: break;
    }
    return "OFF";
}

const char* ch3StatusName(Ch3Status s) {
    switch (s) {
        case Ch3Status::Active: return "ON";
        case Ch3Status::MasterOff:
        case Ch3Status::FreqTooLow:
        case Ch3Status::FreqTooHigh: return "WAIT";
        case Ch3Status::Error: return "ERR";
        case Ch3Status::NoDevice: return "NOHW";
        case Ch3Status::Off: break;
    }
    return "OFF";
}

const char* wifiModeName(WifiMode m) {
    switch (m) {
        case WifiMode::Client: return "client";
        case WifiMode::AccessPoint: return "ap";
        case WifiMode::Off: break;
    }
    return "off";
}

const char* nameAt(const char* const* names, uint8_t count, uint8_t i) { return names[i < count ? i : 0]; }

// Like FREQ:ACT? (0.1 Hz); 0.0 for anything not finite.
void actualText(double hz, char* out, size_t outSize) {
    if (!(hz >= 0.0 && hz < 1e12)) hz = 0.0;
    snprintf(out, outSize, "%.1f", hz);
}

// Foreign text (SSID, names) through Sanitize (W-S9); the source is a char[33] or NUL-terminated.
void clean(const char* in, size_t maxIn, char* out, size_t outSize) {
    size_t n = 0;
    while (in && n < maxIn && in[n]) ++n;
    Sanitize::text(in, n, out, outSize, 32);
}

// The mode line entries as an array (StatusText::modeSummary separates them by " | ").
void addModes(JsonArray modes, const SystemSettings& sys, Spread spread) {
    char buf[64];
    StatusText::modeSummary(sys.tuning_mode, spread, sys.drive_ma, sys.off_state, buf, sizeof(buf));
    char* p = buf;
    while (*p) {
        char* sep = strstr(p, " | ");
        if (sep) *sep = '\0';
        modes.add(p);
        if (!sep) break;
        p = sep + 3;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Requests

Reply parseChannel(uint8_t channel, const char* body, size_t len, Commands* out) {
    if (out) out->count = 0;
    if (channel != 1 && channel != 2) return fail(404, HEADER_SUFFIX_OUT_OF_RANGE);
    if (!out) return bad(SYNTAX_ERROR);
    Values v {};
    const Reply r = read(CHANNEL_FIELDS, countOf(CHANNEL_FIELDS), body, len, &v);
    if (!r.ok()) return r;
    if ((v.seen & 3u) == 3u) return bad(PARAMETER_NOT_ALLOWED, "frequency or frequencyText, not both");
    emit(CHANNEL_FIELDS, countOf(CHANNEL_FIELDS), v, channel, out);
    return ok();
}

Reply parseCh3(const char* body, size_t len, Commands* out) {
    return parseWith(CH3_FIELDS, countOf(CH3_FIELDS), 3, body, len, out);
}

Reply parseConfig(const char* body, size_t len, Commands* out) {
    return parseWith(CONFIG_FIELDS, countOf(CONFIG_FIELDS), 0, body, len, out);
}

Reply parseAction(CommandType type, const char* body, size_t len, Commands* out) {
    if (out) out->count = 0;
    if (!out || (type != CommandType::SaveNow && type != CommandType::Identify)) return bad(ILLEGAL_PARAMETER_VALUE);
    JsonDocument doc;
    const Reply r = load(body, len, doc);
    if (!r.ok()) return r;
    if (doc.as<JsonObjectConst>().size() != 0) return bad(PARAMETER_NOT_ALLOWED);
    Command& c = out->list[0];
    c = Command {};
    c.type = type;
    c.source = Source::Web;
    out->count = 1;
    return ok();
}

Reply parseLogin(const char* body, size_t len, char* password, size_t passwordSize, size_t* passwordLen) {
    if (passwordLen) *passwordLen = 0;
    if (!password || passwordSize == 0) return bad(SYNTAX_ERROR);
    password[0] = '\0';
    JsonDocument doc;
    const Reply r = load(body, len, doc);
    if (!r.ok()) return r;
    bool found = false;
    for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
        if (!keyIs(kv.key(), "password")) return bad(PARAMETER_NOT_ALLOWED);
        JsonString s;
        const Reply t = text(kv.value(), &s);
        if (!t.ok()) return t;
        if (s.size() >= passwordSize) return bad(DATA_OUT_OF_RANGE);
        memcpy(password, s.c_str(), s.size());
        password[s.size()] = '\0';
        if (passwordLen) *passwordLen = s.size();
        found = true;
    }
    if (!found) return bad(MISSING_PARAMETER);
    return ok();
}

bool carriesSecret(const char* body, size_t len) {
    JsonDocument doc;
    if (!parse(body, len, doc, nullptr).ok()) return false;
    return hasSecret(doc.as<JsonVariantConst>());
}

uint8_t channelFromPath(const char* path) {
    if (!path) return 0;
    if (strcmp(path, "/api/channel/1") == 0) return 1;
    if (strcmp(path, "/api/channel/2") == 0) return 2;
    return 0;
}

// ---------------------------------------------------------------------------
// Results

Reply fromPush(PushResult p) {
    switch (p) {
        case PushResult::Queued: break;
        case PushResult::Full: return fail(503, EXECUTION_ERROR, "Busy, try again");
        case PushResult::RateLimited: return fail(429, EXECUTION_ERROR, "Too many commands");
    }
    return ok();
}

Reply fromResult(Result r) {
    switch (r) {
        case Result::Ok:
        case Result::NoChange: break;
        case Result::OutOfRange: return bad(DATA_OUT_OF_RANGE);
        case Result::Invalid: return bad(ILLEGAL_PARAMETER_VALUE);
        case Result::Conflict: return bad(SETTINGS_CONFLICT, "Spread spectrum is not possible in Integer tuning");
        case Result::Denied: return fail(403, SETTINGS_CONFLICT, "Remote outputs: local only");
        case Result::HwError: return fail(500, HARDWARE_ERROR, "SI5351 setup failed");
    }
    return ok();
}

Reply fromOutcome(PushResult p, Result r) {
    const Reply q = fromPush(p);
    return q.ok() ? fromResult(r) : q;
}

const char* errorText(int16_t code) {
    switch (code) {
        case 0: return "No error";
        case SYNTAX_ERROR: return "Syntax error";
        case DATA_TYPE_ERROR: return "Data type error";
        case PARAMETER_NOT_ALLOWED: return "Parameter not allowed";
        case MISSING_PARAMETER: return "Missing parameter";
        case HEADER_SUFFIX_OUT_OF_RANGE: return "Header suffix out of range";
        case EXECUTION_ERROR: return "Execution error";
        case SETTINGS_CONFLICT: return "Settings conflict";
        case DATA_OUT_OF_RANGE: return "Data out of range";
        case TOO_MUCH_DATA: return "Too much data";
        case ILLEGAL_PARAMETER_VALUE: return "Illegal parameter value";
        case HARDWARE_ERROR: return "Hardware error";
        default: break;
    }
    return "Unknown error";
}

size_t errorJson(const Reply& r, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    char text[80];
    snprintf(text, sizeof(text), "%d,%s", (int)r.code, r.info ? r.info : errorText(r.code));
    JsonDocument doc;
    doc["error"] = text;
    if (doc.overflowed() || measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}

// ---------------------------------------------------------------------------
// State

size_t buildState(const Snapshot& s, const DeviceInfo& d, uint32_t nowMs, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    const SystemSettings& sys = s.state.sys;
    JsonDocument doc;
    doc["rev"] = s.revision;
    doc["hw"] = linkName(s.link);
    doc["save"] = saveName(s.saveState);
    const char* remote = remoteName(s.lastRemote);
    if (remote && nowMs - s.lastRemoteMs < REMOTE_INDICATOR_MS) doc["remote"] = remote;
    else doc["remote"] = nullptr;

    // Raw numbers must stay alive until serializeJson().
    char actual[3][24];
    JsonArray chs = doc["ch"].to<JsonArray>();
    for (uint8_t n = 1; n <= 2; ++n) {
        const Channel& c = n == 1 ? s.state.ch1 : s.state.ch2;
        JsonObject o = chs.add<JsonObject>();
        o["n"] = (unsigned)n;
        o["enabled"] = c.enabled;
        o["frequency"] = c.frequency;
        actualText(s.actualHz[n - 1], actual[n - 1], sizeof(actual[0]));
        o["actual"] = serialized(actual[n - 1]);
        o["status"] = channelStatusName(s.status[n - 1]);
        // Spread spectrum is on PLL A: CH1 only (AppController::spreadOf).
        addModes(o["modes"].to<JsonArray>(), sys, n == 1 ? sys.spread : Spread::Off);
    }

    JsonObject ch3 = doc["ch3"].to<JsonObject>();
    ch3["enabled"] = s.state.ch3.enabled;
    ch3["master"] = (unsigned)s.state.ch3.master;
    ch3["phase"] = (unsigned)s.state.ch3.phase;
    ch3["status"] = ch3StatusName(s.ch3Status);
    ch3["frequency"] = s.state.ch3.master == 2 ? s.state.ch2.frequency : s.state.ch1.frequency;
    actualText(s.actualHz[2], actual[2], sizeof(actual[2]));
    ch3["actual"] = serialized(actual[2]);
    ch3["iqMin"] = s.iqMinHz;
    ch3["iqMax"] = s.iqMaxHz;

    JsonObject cfg = doc["config"].to<JsonObject>();
    cfg["drive"] = (unsigned)sys.drive_ma;
    cfg["tuning"] = nameAt(TUNING_NAMES, 3, (uint8_t)sys.tuning_mode);
    cfg["spread"] = nameAt(SPREAD_NAMES, 4, (uint8_t)sys.spread);
    cfg["offState"] = nameAt(OFFSTATE_NAMES, 3, (uint8_t)sys.off_state);
    cfg["correction"] = sys.freq_correction;
    cfg["step"] = sys.delta_step;
    cfg["backlight"] = (unsigned)sys.backlight;
    cfg["remoteOutputs"] = s.remoteOutputsAllowed ? "allowed" : "local";

    char ssid[33], name[33], apSsid[33], version[33], ip[16];
    clean(d.ssid, sizeof(d.ssid), ssid, sizeof(ssid));
    clean(d.name, sizeof(d.name), name, sizeof(name));
    clean(d.apSsid, sizeof(d.apSsid), apSsid, sizeof(apSsid));
    clean(d.version ? d.version : APP_VERSION, 32, version, sizeof(version));
    ip[0] = '\0';
    if (d.ip) {
        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", (unsigned)(d.ip >> 24), (unsigned)((d.ip >> 16) & 255),
                 (unsigned)((d.ip >> 8) & 255), (unsigned)(d.ip & 255));
    }
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["mode"] = wifiModeName(d.mode);
    wifi["connected"] = d.connected;
    wifi["ssid"] = ssid;
    wifi["rssi"] = d.rssi;
    wifi["ip"] = ip;
    wifi["name"] = name;
    JsonObject ap = wifi["ap"].to<JsonObject>();
    ap["active"] = d.apActive;
    ap["ssid"] = apSsid;
    ap["open"] = d.apOpen;
    ap["stations"] = (unsigned)d.apStations;

    doc["device"]["version"] = version;

    if (doc.overflowed()) return 0;
    if (measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}

}
