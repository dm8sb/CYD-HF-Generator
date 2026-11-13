// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// REST JSON mapping (ARBEITSPLAN 3.6): bodies to Commands with strict checks (unknown keys and
// wrong types refused, W-S19), Result/PushResult to HTTP status and SCPI error codes (W-S21),
// password fields (W-S1), the state JSON with escaped foreign strings (W-S8, W-S9), and
// random-input tests with a fixed seed (W-S25). Values run through the real controller.

#include <unity.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>
#include <string.h>
#include <string>
#include <config.h>
#include <constants.h>
#include <core/app_controller.h>
#include <hal/i2c_bus.h>
#include <remote/rest_json.h>
#include <utils/sanitize.h>

using namespace RestJson;

static Fake::Si5351Chip& chip = Fake::si5351();

struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;
    StateSnapshot snap;
    Device() : app(settings, si, backlight) {
        I2CBus::begin();
        settings.begin();
        PersistentState loaded;
        settings.load(loaded);
        app.begin(loaded);
        backlight.begin();
        app.connectHardware();
        app.applyStoredChannelSettings();
    }
    // Runs the commands like the server: the first refused one answers.
    Reply run(const Commands& c) {
        for (uint8_t i = 0; i < c.count; ++i) {
            const Reply r = fromResult(execute(app, c.list[i]));
            if (!r.ok()) return r;
        }
        return Reply {200, 0, nullptr};
    }
    Snapshot snapshot() {
        snap.publish(app);
        Snapshot s {};
        TEST_ASSERT_TRUE(snap.read(&s));
        return s;
    }
};

static uint32_t seed = 1;
// xorshift32: rnd() % k is used a lot, the low bits of an LCG would repeat too soon.
static uint32_t rnd() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

void setUp() {
    chip.reset();
    Fake::nvs().reset();
    Fake::clockMs() = 0;
    seed = 1;
}
void tearDown() {}

static std::string err(const Reply& r) {
    char buf[ERROR_SIZE];
    TEST_ASSERT_TRUE(errorJson(r, buf, sizeof(buf)) > 0);
    return buf;
}

static Reply channel(uint8_t ch, const char* body, Commands* out) { return parseChannel(ch, body, strlen(body), out); }
static Reply config(const char* body, Commands* out) { return parseConfig(body, strlen(body), out); }
static Reply ch3(const char* body, Commands* out) { return parseCh3(body, strlen(body), out); }

#define ASSERT_REPLY(status_, code_, reply_)                    \
    do {                                                        \
        const Reply r_ = (reply_);                              \
        TEST_ASSERT_EQUAL_UINT16((status_), r_.status);         \
        TEST_ASSERT_EQUAL_INT16((code_), r_.code);              \
    } while (0)

static void assertCommand(const Command& c, CommandType type, uint8_t ch, int64_t value) {
    TEST_ASSERT_EQUAL((int)type, (int)c.type);
    TEST_ASSERT_EQUAL_UINT8(ch, c.channel);
    TEST_ASSERT_EQUAL_INT64(value, c.value);
    TEST_ASSERT_EQUAL((int)Source::Web, (int)c.source);
}

static void test_channel_bodies() {
    Commands c;
    ASSERT_REPLY(200, 0, channel(1, "{\"enabled\":true,\"frequency\":10000000}", &c));
    TEST_ASSERT_EQUAL_UINT8(2, c.count);
    // Frequency before enabled, whatever the key order.
    assertCommand(c.list[0], CommandType::SetFrequency, 1, 10000000);
    assertCommand(c.list[1], CommandType::SetEnabled, 1, 1);
    ASSERT_REPLY(200, 0, channel(2, " {\n\t\"frequency\" : 7100000 , \"enabled\" : false } \r\n", &c));
    TEST_ASSERT_EQUAL_UINT8(2, c.count);
    assertCommand(c.list[0], CommandType::SetFrequency, 2, 7100000);
    assertCommand(c.list[1], CommandType::SetEnabled, 2, 0);
    // frequencyText through FrequencyUtils::parseRemoteFrequency.
    const struct {
        const char* body;
        int64_t hz;
    } texts[] = {{"{\"frequencyText\":\"10.7M\"}", 10700000},  {"{\"frequencyText\":\"455k\"}", 455000},
                 {"{\"frequencyText\":\"7 100 000\"}", 7100000}, {"{\"frequencyText\":\"10,7 MHz\"}", 10700000},
                 {"{\"frequencyText\":\"14.2 mhz\"}", 14200000}};
    for (const auto& t : texts) {
        ASSERT_REPLY(200, 0, channel(1, t.body, &c));
        TEST_ASSERT_EQUAL_UINT8(1, c.count);
        assertCommand(c.list[0], CommandType::SetFrequency, 1, t.hz);
    }
    ASSERT_REPLY(400, -224, channel(1, "{\"frequencyText\":\"10.7\"}", &c));
    ASSERT_REPLY(400, -222, channel(1, "{\"frequencyText\":\"200 MHz\"}", &c)); // S55: range, not syntax
    ASSERT_REPLY(400, -222, channel(1, "{\"frequencyText\":\"7.9k\"}", &c));
    ASSERT_REPLY(400, -224, channel(1, "{\"frequencyText\":\"1e7\"}", &c));
    ASSERT_REPLY(400, -224, channel(1, "{\"frequencyText\":\"10M\\u0000garbage\"}", &c)); // NUL
    ASSERT_REPLY(400, -104, channel(1, "{\"frequencyText\":10000000}", &c));
    // frequency and frequencyText exclude each other; nothing at all is missing.
    ASSERT_REPLY(400, -108, channel(1, "{\"frequency\":10000000,\"frequencyText\":\"10M\"}", &c));
    TEST_ASSERT_EQUAL_UINT8(0, c.count);
    ASSERT_REPLY(400, -109, channel(1, "{}", &c));
    // Channel from the path: 1 and 2 only.
    ASSERT_REPLY(404, -114, channel(3, "{\"enabled\":true}", &c));
    ASSERT_REPLY(404, -114, channel(0, "{\"enabled\":true}", &c));
    TEST_ASSERT_EQUAL_UINT8(1, channelFromPath("/api/channel/1"));
    TEST_ASSERT_EQUAL_UINT8(2, channelFromPath("/api/channel/2"));
    TEST_ASSERT_EQUAL_UINT8(0, channelFromPath("/api/channel/3"));
    TEST_ASSERT_EQUAL_UINT8(0, channelFromPath("/api/channel/1/"));
    TEST_ASSERT_EQUAL_UINT8(0, channelFromPath("/api/channel/01"));
    TEST_ASSERT_EQUAL_UINT8(0, channelFromPath(nullptr));
}

static void test_ch3_and_config_bodies() {
    Commands c;
    ASSERT_REPLY(200, 0, ch3("{\"enabled\":true,\"phase\":270,\"master\":2}", &c));
    TEST_ASSERT_EQUAL_UINT8(3, c.count);
    assertCommand(c.list[0], CommandType::Ch3Master, 3, 2);
    assertCommand(c.list[1], CommandType::Ch3Phase, 3, 270);
    assertCommand(c.list[2], CommandType::SetEnabled, 3, 1);
    ASSERT_REPLY(200, 0,
                 config("{\"backlight\":80,\"step\":1000,\"correction\":-250,\"drive\":8,\"offState\":\"hiz\","
                        "\"spread\":\"center05\",\"tuning\":\"lownoise\"}",
                        &c));
    TEST_ASSERT_EQUAL_UINT8(7, c.count);
    // Tuning before spread (spread in Integer is a conflict), then the rest.
    assertCommand(c.list[0], CommandType::Tuning, 0, (int64_t)TuningMode::LowNoise);
    assertCommand(c.list[1], CommandType::Spread, 0, (int64_t)Spread::Center05);
    assertCommand(c.list[2], CommandType::OffState, 0, (int64_t)OffState::HighZ);
    assertCommand(c.list[3], CommandType::Drive, 0, 8);
    assertCommand(c.list[4], CommandType::Correction, 0, -250);
    assertCommand(c.list[5], CommandType::Step, 0, 1000);
    assertCommand(c.list[6], CommandType::Backlight, 0, 80);
    const struct {
        const char* body;
        CommandType type;
        int64_t value;
    } names[] = {{"{\"tuning\":\"fractional\"}", CommandType::Tuning, 0}, {"{\"tuning\":\"integer\"}", CommandType::Tuning, 2},
                 {"{\"spread\":\"off\"}", CommandType::Spread, 0},        {"{\"spread\":\"down05\"}", CommandType::Spread, 1},
                 {"{\"spread\":\"down15\"}", CommandType::Spread, 2},     {"{\"offState\":\"low\"}", CommandType::OffState, 0},
                 {"{\"offState\":\"high\"}", CommandType::OffState, 1}};
    for (const auto& n : names) {
        ASSERT_REPLY(200, 0, config(n.body, &c));
        assertCommand(c.list[0], n.type, 0, n.value);
    }
    const char* const unknownNames[] = {"{\"tuning\":\"Integer\"}", "{\"tuning\":\"lowNoise\"}", "{\"spread\":\"down5\"}",
                                        "{\"offState\":\"High-Z\"}", "{\"offState\":\"\"}", "{\"tuning\":\"integer \"}"};
    for (const char* b : unknownNames) ASSERT_REPLY(400, -224, config(b, &c));
    // Remote outputs can only be changed on the device (W-E25).
    const Reply r = config("{\"remoteOutputs\":\"local\"}", &c);
    ASSERT_REPLY(403, -221, r);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-221,Remote outputs can only be changed at the device\"}", err(r).c_str());
    ASSERT_REPLY(403, -221, config("{\"drive\":4,\"remoteOutputs\":\"allowed\"}", &c));
    TEST_ASSERT_EQUAL_UINT8(0, c.count);
    // Save and identify take {}.
    ASSERT_REPLY(200, 0, parseAction(CommandType::SaveNow, " { } ", 5, &c));
    TEST_ASSERT_EQUAL_UINT8(1, c.count);
    assertCommand(c.list[0], CommandType::SaveNow, 0, 0);
    ASSERT_REPLY(200, 0, parseAction(CommandType::Identify, "{}", 2, &c));
    assertCommand(c.list[0], CommandType::Identify, 0, 0);
    ASSERT_REPLY(400, -108, parseAction(CommandType::SaveNow, "{\"now\":true}", 12, &c));
    ASSERT_REPLY(400, -102, parseAction(CommandType::SaveNow, "", 0, &c));
    ASSERT_REPLY(400, -102, parseAction(CommandType::SaveNow, nullptr, 0, &c));
    ASSERT_REPLY(400, -104, parseAction(CommandType::SaveNow, "[]", 2, &c));
    ASSERT_REPLY(400, -224, parseAction(CommandType::Drive, "{}", 2, &c));
}

static void test_wrong_types_are_refused() {
    Commands c;
    // ARBEITSPLAN 3.6: {"drive":"4"}, {"drive":4.5} -> 400.
    const char* const config104[] = {"{\"drive\":\"4\"}", "{\"drive\":4.5}", "{\"drive\":4.0}", "{\"drive\":4e0}",
                                     "{\"drive\":40E-1}", "{\"drive\":true}", "{\"drive\":false}", "{\"drive\":null}",
                                     "{\"drive\":[4]}", "{\"drive\":{\"v\":4}}", "{\"correction\":-2.5}",
                                     "{\"backlight\":\"80\"}", "{\"tuning\":2}", "{\"tuning\":null}", "{\"spread\":[]}",
                                     "{\"offState\":true}", "{\"step\":1e3}"};
    for (const char* b : config104) {
        const Reply r = config(b, &c);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, r.status, b);
        TEST_ASSERT_EQUAL_INT16_MESSAGE(-104, r.code, b);
        TEST_ASSERT_EQUAL_UINT8(0, c.count);
    }
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-104,Data type error\"}", err(config("{\"drive\":\"4\"}", &c)).c_str());
    const char* const channel104[] = {"{\"enabled\":1}", "{\"enabled\":0}", "{\"enabled\":\"true\"}", "{\"enabled\":null}",
                                      "{\"frequency\":\"10000000\"}", "{\"frequency\":10000000.5}",
                                      "{\"frequency\":1.0E7}", "{\"frequency\":true}"};
    for (const char* b : channel104) {
        const Reply r = channel(1, b, &c);
        TEST_ASSERT_EQUAL_INT16_MESSAGE(-104, r.code, b);
    }
    ASSERT_REPLY(400, -104, ch3("{\"master\":\"1\"}", &c));
    ASSERT_REPLY(400, -104, ch3("{\"phase\":90.0}", &c));
    // Integers that do not fit into int64 are out of range, not cut (W-S19).
    ASSERT_REPLY(400, -222, channel(1, "{\"frequency\":9223372036854775808}", &c));
    ASSERT_REPLY(400, -222, channel(1, "{\"frequency\":18446744073709551616}", &c));
    ASSERT_REPLY(400, -222, channel(1, "{\"frequency\":-9223372036854775809}", &c));
    ASSERT_REPLY(400, -222, channel(1, "{\"frequency\":1e30}", &c));
    ASSERT_REPLY(400, -222, channel(1, "{\"frequency\":1e400}", &c));
    ASSERT_REPLY(400, -222, config("{\"drive\":-1e19}", &c));
    ASSERT_REPLY(200, 0, channel(1, "{\"frequency\":9223372036854775807}", &c));
    assertCommand(c.list[0], CommandType::SetFrequency, 1, INT64_MAX);
    ASSERT_REPLY(200, 0, channel(1, "{\"frequency\":-9223372036854775808}", &c));
    assertCommand(c.list[0], CommandType::SetFrequency, 1, INT64_MIN);
}

static void test_unknown_keys_and_syntax() {
    Commands c;
    // Unknown keys: -108, also for keys of another endpoint and with a NUL inside.
    const char* const unknown[] = {"{\"drive\":4,\"foo\":1}", "{\"Drive\":4}", "{\"drive \":4}", "{\"drive\\u0000x\":4}",
                                   "{\"enabled\":true}", "{\"\":1}", "{\"frequency\":10000000}"};
    for (const char* b : unknown) {
        const Reply r = config(b, &c);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, r.status, b);
        TEST_ASSERT_EQUAL_INT16_MESSAGE(-108, r.code, b);
    }
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-108,Parameter not allowed\"}", err(config("{\"x\":1}", &c)).c_str());
    // An escaped key is the same key.
    ASSERT_REPLY(200, 0, config("{\"dr\\u0069ve\":4}", &c));
    assertCommand(c.list[0], CommandType::Drive, 0, 4);
    // Invalid JSON, also what ArduinoJson alone would accept: -102.
    const char* const syntax[] = {"", " ", "{", "}", "{\"drive\":4", "{drive:4}", "{'drive':4}", "{\"drive\":'x'}",
                                  "{\"drive\":4} x", "{\"drive\":4}{}", "{\"drive\":4}}", "{\"drive\":04}",
                                  "{\"drive\":4,}", "{,\"drive\":4}", "{\"drive\":+4}", "{\"drive\":.5}", "{\"drive\":5.}",
                                  "{\"drive\":-}", "{\"drive\":1e}", "{\"drive\":NaN}", "{\"drive\":Infinity}",
                                  "{\"drive\":-Infinity}", "{\"drive\":0x10}", "{\"drive\":4 /*c*/}", "{\"drive\":4 // c\n}",
                                  "{\"tuning\":\"a\\x\"}", "{\"tuning\":\"a\\u12\"}", "{\"tuning\":\"a\nb\"}",
                                  "{\"tuning\":\"a\tb\"}", "{\"drive\":tru}", "{\"drive\":True}", "{\"drive\":nul}",
                                  "{\"drive\" 4}", "{\"drive\"::4}", "\xEF\xBB\xBF{\"drive\":4}", "{\"drive\":4,\"drive\":8}",
                                  "{\"drive\":4,\"dr\\u0069ve\":8}"};
    for (const char* b : syntax) {
        const Reply r = config(b, &c);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, r.status, b);
        TEST_ASSERT_EQUAL_INT16_MESSAGE(-102, r.code, b);
        TEST_ASSERT_EQUAL_UINT8(0, c.count);
    }
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-102,Duplicate key\"}",
                             err(config("{\"drive\":4,\"drive\":8}", &c)).c_str());
    // A NUL byte inside the body ends nothing early: the rest must be valid too.
    const char nul[] = "{\"drive\":4}\0x";
    ASSERT_REPLY(400, -102, parseConfig(nul, sizeof(nul) - 1, &c));
    // Nesting deeper than 10: -102; 10 levels are still JSON (then refused for the key).
    std::string deep = "{\"a\":";
    for (int i = 0; i < 9; ++i) deep += "[";
    deep += "1";
    for (int i = 0; i < 9; ++i) deep += "]";
    deep += "}";
    ASSERT_REPLY(400, -108, config(deep.c_str(), &c));
    deep = "{\"a\":[" + deep.substr(5) + "]}";
    ASSERT_REPLY(400, -102, config(deep.c_str(), &c));
    std::string flood(MAX_BODY, '[');
    ASSERT_REPLY(400, -102, parseConfig(flood.c_str(), flood.size(), &c));
    // Top level not an object: -104.
    const char* const notObject[] = {"[]", "4", "\"drive\"", "null", "true", "[{\"drive\":4}]"};
    for (const char* b : notObject) ASSERT_REPLY(400, -104, config(b, &c));
    // Larger than the guard lets through: -223, 413.
    std::string big = "{\"drive\":4" + std::string(MAX_BODY, ' ') + "}";
    ASSERT_REPLY(413, -223, parseConfig(big.c_str(), big.size(), &c));
    std::string fits = "{\"drive\":4" + std::string(MAX_BODY - 11, ' ') + "}";
    ASSERT_REPLY(200, 0, parseConfig(fits.c_str(), fits.size(), &c));
    ASSERT_REPLY(400, -102, parseConfig(nullptr, 0, &c));
    ASSERT_REPLY(400, -102, parseConfig("{}", 2, nullptr));
}

static void test_values_are_checked_by_the_controller() {
    Device d;
    Commands c;
    // Valid values run, ranges and allowed values are the controller's (like SCPI).
    ASSERT_REPLY(200, 0, channel(1, "{\"frequency\":7100000,\"enabled\":true}", &c));
    ASSERT_REPLY(200, 0, d.run(c));
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    TEST_ASSERT_TRUE(d.app.channel(1).enabled);
    const struct {
        const char* body;
        uint16_t status;
        int16_t code;
        const char* json;
    } cases[] = {
        {"{\"drive\":5}", 400, -224, "{\"error\":\"-224,Illegal parameter value\"}"},
        {"{\"drive\":258}", 400, -224, nullptr},
        {"{\"correction\":5001}", 400, -222, "{\"error\":\"-222,Data out of range\"}"},
        {"{\"correction\":-5001}", 400, -222, nullptr},
        {"{\"backlight\":5}", 400, -222, nullptr},
        {"{\"backlight\":256}", 400, -222, nullptr},
        {"{\"step\":3}", 400, -224, nullptr},
        {"{\"step\":4294967297}", 400, -224, nullptr},
        {"{\"tuning\":\"integer\",\"spread\":\"down05\"}", 400, -221, "{\"error\":\"-221,Spread spectrum is not possible in Integer tuning\"}"},
    };
    for (const auto& k : cases) {
        ASSERT_REPLY(200, 0, config(k.body, &c));
        const Reply r = d.run(c);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(k.status, r.status, k.body);
        TEST_ASSERT_EQUAL_INT16_MESSAGE(k.code, r.code, k.body);
        if (k.json) TEST_ASSERT_EQUAL_STRING(k.json, err(r).c_str());
    }
    TEST_ASSERT_EQUAL((int)TuningMode::Integer, (int)d.app.system().tuning_mode); // applied before the conflict
    const char* const channel222[] = {"{\"frequency\":7999}", "{\"frequency\":160000001}", "{\"frequency\":-1}",
                                      "{\"frequency\":9223372036854775807}"};
    for (const char* b : channel222) {
        ASSERT_REPLY(200, 0, channel(2, b, &c));
        ASSERT_REPLY(400, -222, d.run(c));
    }
    ASSERT_REPLY(200, 0, ch3("{\"phase\":45}", &c));
    ASSERT_REPLY(400, -224, d.run(c));
    ASSERT_REPLY(200, 0, ch3("{\"master\":3}", &c));
    ASSERT_REPLY(400, -224, d.run(c));
    ASSERT_REPLY(200, 0, ch3("{\"master\":2,\"phase\":180}", &c));
    ASSERT_REPLY(200, 0, d.run(c));
    TEST_ASSERT_EQUAL_UINT8(2, d.app.ch3().master);
    // Remote outputs local only (W-S21): switching on 403/-221, switching off allowed.
    TEST_ASSERT_EQUAL((int)Result::Ok, (int)d.app.setRemoteOutputsAllowed(false));
    ASSERT_REPLY(200, 0, channel(2, "{\"enabled\":true}", &c));
    const Reply denied = d.run(c);
    ASSERT_REPLY(403, -221, denied);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-221,Remote outputs: local only\"}", err(denied).c_str());
    TEST_ASSERT_FALSE(d.app.channel(2).enabled);
    // D22: off state High is refused the same way, High-Z stays allowed.
    ASSERT_REPLY(200, 0, config("{\"offState\":\"high\"}", &c));
    ASSERT_REPLY(403, -221, d.run(c));
    TEST_ASSERT_EQUAL((int)OffState::Low, (int)d.app.system().off_state);
    ASSERT_REPLY(200, 0, config("{\"offState\":\"hiz\"}", &c));
    ASSERT_REPLY(200, 0, d.run(c));
    TEST_ASSERT_EQUAL((int)OffState::HighZ, (int)d.app.system().off_state);
    ASSERT_REPLY(200, 0, channel(1, "{\"enabled\":false}", &c));
    ASSERT_REPLY(200, 0, d.run(c));
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    // Save and identify.
    ASSERT_REPLY(200, 0, parseAction(CommandType::SaveNow, "{}", 2, &c));
    ASSERT_REPLY(200, 0, d.run(c));
    // Through the queue: the source is Web, the write limit is the queue's (W-S11).
    CommandQueue q;
    ASSERT_REPLY(200, 0, config("{\"drive\":6}", &c));
    for (int i = 0; i < REMOTE_WRITES_PER_S; ++i) {
        if (i == CommandQueue::CAPACITY) q.drain(d.app);
        ASSERT_REPLY(200, 0, fromPush(q.push(c.list[0])));
    }
    const Reply limited = fromPush(q.push(c.list[0]));
    ASSERT_REPLY(429, -200, limited);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Too many commands\"}", err(limited).c_str());
    q.drain(d.app);
    TEST_ASSERT_EQUAL_UINT8(6, d.app.system().drive_ma);
    TEST_ASSERT_EQUAL((int)Source::Web, (int)d.app.lastRemote());
}

static void test_result_mapping() {
    ASSERT_REPLY(200, 0, fromPush(PushResult::Queued));
    const Reply full = fromPush(PushResult::Full);
    ASSERT_REPLY(503, -200, full);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Busy, try again\"}", err(full).c_str());
    ASSERT_REPLY(429, -200, fromPush(PushResult::RateLimited));
    ASSERT_REPLY(200, 0, fromResult(Result::Ok));
    ASSERT_REPLY(200, 0, fromResult(Result::NoChange));
    ASSERT_REPLY(400, -222, fromResult(Result::OutOfRange));
    ASSERT_REPLY(400, -224, fromResult(Result::Invalid));
    ASSERT_REPLY(400, -221, fromResult(Result::Conflict));
    ASSERT_REPLY(403, -221, fromResult(Result::Denied));
    const Reply hw = fromResult(Result::HwError);
    ASSERT_REPLY(500, -240, hw);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-240,SI5351 setup failed\"}", err(hw).c_str());
    ASSERT_REPLY(503, -200, fromOutcome(PushResult::Full, Result::Ok));
    ASSERT_REPLY(403, -221, fromOutcome(PushResult::Queued, Result::Denied));
    ASSERT_REPLY(200, 0, fromOutcome(PushResult::Queued, Result::NoChange));
    TEST_ASSERT_EQUAL_STRING("Data out of range", errorText(-222));
    TEST_ASSERT_EQUAL_STRING("Unknown error", errorText(-999));
    // Too small a buffer: nothing.
    char tiny[20] = "x";
    TEST_ASSERT_EQUAL(0, (int)errorJson(full, tiny, sizeof(tiny)));
    TEST_ASSERT_EQUAL_STRING("", tiny);
    TEST_ASSERT_EQUAL(0, (int)errorJson(full, nullptr, 10));
}

static void test_login_body_and_password_fields() {
    char pw[65];
    size_t len = 99;
    ASSERT_REPLY(200, 0, parseLogin("{\"password\":\"correct horse\"}", 28, pw, sizeof(pw), &len));
    TEST_ASSERT_EQUAL_STRING("correct horse", pw);
    TEST_ASSERT_EQUAL(13, (int)len);
    ASSERT_REPLY(200, 0, parseLogin("{\"password\":\"p\\u00e4ss\\\"\"}", 26, pw, sizeof(pw), &len));
    TEST_ASSERT_EQUAL_STRING("p\xC3\xA4ss\"", pw);
    ASSERT_REPLY(400, -104, parseLogin("{\"password\":12345678}", 21, pw, sizeof(pw), &len));
    ASSERT_REPLY(400, -109, parseLogin("{}", 2, pw, sizeof(pw), &len));
    ASSERT_REPLY(400, -108, parseLogin("{\"password\":\"x\",\"user\":\"y\"}", 27, pw, sizeof(pw), &len));
    ASSERT_REPLY(400, -224, parseLogin("{\"password\":\"a\\u0000b\"}", 23, pw, sizeof(pw), &len));
    const std::string longBody = "{\"password\":\"" + std::string(65, 'x') + "\"}";
    ASSERT_REPLY(400, -222, parseLogin(longBody.c_str(), longBody.size(), pw, sizeof(pw), &len));
    TEST_ASSERT_EQUAL(0, (int)len);
    TEST_ASSERT_EQUAL_STRING("", pw);
    // W-S1: bodies with password or key fields, at any depth and in any case.
    const char* const secret[] = {"{\"password\":\"x\"}", "{\"networks\":[{\"ssid\":\"a\",\"key\":\"b\"}]}",
                                  "{\"apPassword\":\"x\"}", "{\"PASSWORD\":1}", "{\"wifi\":{\"psk\":\"x\"}}",
                                  "{\"secret\":null}", "{\"pass\\u0077ord\":\"x\"}", "[{\"a\":[{\"newPassphrase\":1}]}]"};
    for (const char* b : secret) TEST_ASSERT_TRUE_MESSAGE(carriesSecret(b, strlen(b)), b);
    // Duplicate keys hide nothing (ArduinoJson keeps the last value; fuzz finding S57c), nor do
    // escapes or long keys.
    const char* const hidden[] = {"{\"a\":{\"password\":\"x\"},\"a\":1}",
                                  "{\"networks\":[{\"slot\":0,\"ssid\":\"a\",\"password\":\"b\"}],\"networks\":[]}",
                                  "{\"ap\":{\"x\":{\"psk\":1},\"x\":2}}", "{\"\\u0050ASS\":1}",
                                  // A lone high surrogate vanishes in ArduinoJson: "password" (CI fuzz 2026-11-09).
                                  "{\"pas\\ud800sword\":\"x\"}", "{\"pa\\uDBFF\\uD83Dss\":1}"};
    for (const char* b : hidden) TEST_ASSERT_TRUE_MESSAGE(carriesSecret(b, strlen(b)), b);
    const std::string longKey = "{\"" + std::string(70, 'x') + "Password\":1}";
    TEST_ASSERT_TRUE(carriesSecret(longKey.c_str(), longKey.size()));
    const char* const plain[] = {"{\"drive\":4}", "{\"ssid\":\"password\"}", "[\"password\"]", "{}", "{password:1}", "",
                                 "{\"mode\":\"client\",\"ssid\":\"Lab\"}"};
    for (const char* b : plain) TEST_ASSERT_FALSE_MESSAGE(carriesSecret(b, strlen(b)), b);
    TEST_ASSERT_FALSE(carriesSecret(nullptr, 0));
}

static DeviceInfo info() {
    DeviceInfo d {};
    d.mode = WifiMode::Client;
    d.connected = true;
    strcpy(d.ssid, "Lab");
    d.rssi = -60;
    d.ip = (192u << 24) | (168u << 16) | (1u << 8) | 50u;
    strcpy(d.name, "cyd-hf-1a2b");
    strcpy(d.apSsid, "CYD-HF-1A2B");
    d.version = "2.0.0";
    return d;
}

static std::string state(const Snapshot& s, const DeviceInfo& d, uint32_t now = 0) {
    char buf[STATE_SIZE];
    const size_t n = buildState(s, d, now, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL(n, strlen(buf));
    return buf;
}

static void parse(const std::string& json, JsonDocument& doc) {
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
}

static size_t keyCount(JsonObjectConst o) { return o.size(); }

static void test_state_json_contract() {
    Device d;
    d.app.setChannelFrequency(1, 7100000);
    d.app.setChannelEnabled(1, true);
    d.app.setGlobalDriveStrength(8);
    d.app.setSpread(Spread::Down05);
    d.app.setOffState(OffState::High);
    d.app.setCorrection(-125);
    const Snapshot s = d.snapshot();
    const std::string json = state(s, info());
    JsonDocument doc;
    parse(json, doc);
    // Exactly the keys of the contract; MAC and commit are not in it (S-26).
    TEST_ASSERT_EQUAL(9, (int)keyCount(doc.as<JsonObjectConst>()));
    TEST_ASSERT_EQUAL_UINT32(s.revision, doc["rev"].as<uint32_t>());
    TEST_ASSERT_EQUAL_STRING("CONN", doc["hw"]);
    TEST_ASSERT_EQUAL_STRING("pending", doc["save"]);
    TEST_ASSERT_TRUE(doc["remote"].isNull());
    TEST_ASSERT_TRUE(doc["remote"].is<JsonVariantConst>() || doc["remote"].isNull());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"remote\":null"));
    TEST_ASSERT_NULL(strstr(json.c_str(), "mac"));
    TEST_ASSERT_NULL(strstr(json.c_str(), "commit"));
    JsonArrayConst ch = doc["ch"];
    TEST_ASSERT_EQUAL(2, (int)ch.size());
    JsonObjectConst c1 = ch[0];
    TEST_ASSERT_EQUAL(6, (int)keyCount(c1));
    TEST_ASSERT_EQUAL(1, c1["n"].as<int>());
    TEST_ASSERT_TRUE(c1["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT64(7100000, c1["frequency"].as<uint64_t>());
    TEST_ASSERT_TRUE(c1["frequency"].is<uint64_t>());
    TEST_ASSERT_EQUAL_STRING("ON", c1["status"]);
    // actual with 0.1 Hz, written like FREQ:ACT? (no exponent).
    char actual[32];
    snprintf(actual, sizeof(actual), "\"actual\":%.1f", s.actualHz[0]);
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), actual));
    // Mode line entries: spread only on CH1 (PLL A).
    JsonArrayConst m1 = c1["modes"];
    TEST_ASSERT_EQUAL(3, (int)m1.size());
    TEST_ASSERT_EQUAL_STRING("OFF=High", m1[0]);
    TEST_ASSERT_EQUAL_STRING("SSC -0.5%", m1[1]);
    TEST_ASSERT_EQUAL_STRING("8mA", m1[2]);
    JsonObjectConst c2 = ch[1];
    TEST_ASSERT_EQUAL(2, c2["n"].as<int>());
    TEST_ASSERT_FALSE(c2["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_UINT64(DEFAULT_FREQ_CH2, c2["frequency"].as<uint64_t>());
    TEST_ASSERT_EQUAL_STRING("OFF", c2["status"]);
    TEST_ASSERT_EQUAL(2, (int)c2["modes"].size());
    TEST_ASSERT_EQUAL_STRING("8mA", c2["modes"][1]);
    JsonObjectConst c3 = doc["ch3"];
    TEST_ASSERT_EQUAL(8, (int)keyCount(c3));
    TEST_ASSERT_FALSE(c3["enabled"].as<bool>());
    TEST_ASSERT_EQUAL(1, c3["master"].as<int>());
    TEST_ASSERT_EQUAL(90, c3["phase"].as<int>());
    TEST_ASSERT_EQUAL_STRING("OFF", c3["status"]);
    TEST_ASSERT_EQUAL_UINT64(7100000, c3["frequency"].as<uint64_t>());
    TEST_ASSERT_EQUAL_UINT64(s.iqMinHz, c3["iqMin"].as<uint64_t>());
    TEST_ASSERT_EQUAL_UINT64(s.iqMaxHz, c3["iqMax"].as<uint64_t>());
    TEST_ASSERT_TRUE(c3["actual"].is<double>());
    JsonObjectConst cfg = doc["config"];
    TEST_ASSERT_EQUAL(8, (int)keyCount(cfg));
    TEST_ASSERT_EQUAL(8, cfg["drive"].as<int>());
    TEST_ASSERT_EQUAL_STRING("fractional", cfg["tuning"]);
    TEST_ASSERT_EQUAL_STRING("down05", cfg["spread"]);
    TEST_ASSERT_EQUAL_STRING("high", cfg["offState"]);
    TEST_ASSERT_EQUAL(-125, cfg["correction"].as<int>());
    TEST_ASSERT_EQUAL(1000, cfg["step"].as<int>());
    TEST_ASSERT_EQUAL(80, cfg["backlight"].as<int>());
    TEST_ASSERT_EQUAL_STRING("allowed", cfg["remoteOutputs"]);
    JsonObjectConst w = doc["wifi"];
    TEST_ASSERT_EQUAL(8, (int)keyCount(w));
    TEST_ASSERT_EQUAL(0, w["confirmWithin"].as<int>()); // S55: 0 = no network change to confirm
    TEST_ASSERT_EQUAL_STRING("client", w["mode"]);
    TEST_ASSERT_TRUE(w["connected"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("Lab", w["ssid"]);
    TEST_ASSERT_EQUAL(-60, w["rssi"].as<int>());
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", w["ip"]);
    TEST_ASSERT_EQUAL_STRING("cyd-hf-1a2b", w["name"]);
    JsonObjectConst ap = w["ap"];
    TEST_ASSERT_EQUAL(4, (int)keyCount(ap));
    TEST_ASSERT_FALSE(ap["active"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("CYD-HF-1A2B", ap["ssid"]);
    TEST_ASSERT_FALSE(ap["open"].as<bool>());
    TEST_ASSERT_EQUAL(0, ap["stations"].as<int>());
    TEST_ASSERT_EQUAL_STRING("2.0.0", doc["device"]["version"]);
    TEST_ASSERT_FALSE(doc["device"]["passwordSet"].as<bool>());
    TEST_ASSERT_TRUE(doc["device"]["passwordSet"].is<bool>());
    TEST_ASSERT_EQUAL(2, (int)keyCount(doc["device"]));
    DeviceInfo pending = info();
    pending.confirmWithin = 117;
    pending.passwordSet = true;
    JsonDocument pd;
    parse(state(s, pending), pd);
    TEST_ASSERT_EQUAL(117, pd["wifi"]["confirmWithin"].as<int>());
    TEST_ASSERT_TRUE(pd["device"]["passwordSet"].as<bool>());
    // Without a version the firmware's.
    DeviceInfo noVersion = info();
    noVersion.version = nullptr;
    JsonDocument v;
    parse(state(s, noVersion), v);
    TEST_ASSERT_EQUAL_STRING(APP_VERSION, v["device"]["version"]);
    // The state fits STATE_SIZE with the longest texts.
    DeviceInfo longest = info();
    memset(longest.ssid, '"', 32);
    memset(longest.name, '\\', 32);
    memset(longest.apSsid, '"', 32);
    const std::string slashes(32, '\\');
    longest.version = slashes.c_str();
    longest.confirmWithin = 65535;
    longest.passwordSet = true;
    const std::string big = state(s, longest);
    TEST_ASSERT_TRUE(big.size() < STATE_SIZE);
    char small[200];
    TEST_ASSERT_EQUAL(0, (int)buildState(s, info(), 0, small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("", small);
}

static void test_state_save_and_remote_indicator() {
    Device d;
    Snapshot s = d.snapshot();
    JsonDocument doc;
    parse(state(s, info(), 0), doc);
    TEST_ASSERT_EQUAL_STRING("saved", doc["save"]);
    TEST_ASSERT_TRUE(doc["remote"].isNull());
    Fake::clockMs() = 1000;
    Command c {};
    c.type = CommandType::Drive;
    c.value = 6;
    c.source = Source::Usb;
    TEST_ASSERT_EQUAL((int)Result::Ok, (int)execute(d.app, c));
    s = d.snapshot();
    TEST_ASSERT_EQUAL((int)Source::Usb, (int)s.lastRemote);
    TEST_ASSERT_EQUAL_UINT32(1000, s.lastRemoteMs);
    parse(state(s, info(), 1000 + REMOTE_INDICATOR_MS - 1), doc);
    TEST_ASSERT_EQUAL_STRING("usb", doc["remote"]);
    TEST_ASSERT_EQUAL_STRING("pending", doc["save"]);
    parse(state(s, info(), 1000 + REMOTE_INDICATOR_MS), doc);
    TEST_ASSERT_TRUE(doc["remote"].isNull());
    s.lastRemote = Source::Web;
    parse(state(s, info(), 1000), doc);
    TEST_ASSERT_EQUAL_STRING("web", doc["remote"]);
    s.lastRemote = Source::Scpi;
    parse(state(s, info(), 1000), doc);
    TEST_ASSERT_EQUAL_STRING("scpi", doc["remote"]);
    s.lastRemote = Source::Touch;
    parse(state(s, info(), 1000), doc);
    TEST_ASSERT_TRUE(doc["remote"].isNull());
    // After the autosave the snapshot says saved.
    Fake::clockMs() = 1000 + SETTINGS_AUTOSAVE_MS + SETTINGS_REMOTE_AUTOSAVE_MS;
    d.app.update();
    parse(state(d.snapshot(), info()), doc);
    TEST_ASSERT_EQUAL_STRING("saved", doc["save"]);
}

// S55: the WebSocket push goes out when anything of the push key changed, including the
// REMOTE indicator expiring and a pending network change, but not for the RSSI alone.
static void test_push_key() {
    Device d;
    Snapshot s = d.snapshot();
    DeviceInfo i = info();
    const PushKey k0 = pushKey(s, i, 0);
    TEST_ASSERT_TRUE(k0 == pushKey(s, i, 0));
    DeviceInfo rssi = i;
    rssi.rssi = -80;
    TEST_ASSERT_TRUE(k0 == pushKey(s, rssi, 0));
    DeviceInfo pending = i;
    pending.confirmWithin = 120;
    TEST_ASSERT_TRUE(k0 != pushKey(s, pending, 0));
    pending.confirmWithin = 119;
    TEST_ASSERT_TRUE(pushKey(s, pending, 0) == pushKey(s, pending, 1000)); // the countdown itself is not pushed
    DeviceInfo other = i;
    strcpy(other.ssid, "Lab2");
    TEST_ASSERT_TRUE(k0 != pushKey(s, other, 0));
    other = i;
    other.apStations = 1;
    TEST_ASSERT_TRUE(k0 != pushKey(s, other, 0));
    other = i;
    other.passwordSet = true;
    TEST_ASSERT_TRUE(k0 != pushKey(s, other, 0));
    // Revision and save state.
    Snapshot t = s;
    t.revision++;
    TEST_ASSERT_TRUE(k0 != pushKey(t, i, 0));
    t = s;
    t.saveState = SaveState::Pending;
    TEST_ASSERT_TRUE(k0 != pushKey(t, i, 0));
    // REMOTE: on with a remote command, off again after REMOTE_INDICATOR_MS without a change.
    t = s;
    t.lastRemote = Source::Web;
    t.lastRemoteMs = 5000;
    const PushKey on = pushKey(t, i, 5000);
    TEST_ASSERT_TRUE(on.remote);
    TEST_ASSERT_TRUE(on == pushKey(t, i, 5000 + REMOTE_INDICATOR_MS - 1));
    TEST_ASSERT_TRUE(on != pushKey(t, i, 5000 + REMOTE_INDICATOR_MS));
    TEST_ASSERT_FALSE(pushKey(t, i, 5000 + REMOTE_INDICATOR_MS).remote);
}

// S55: refusals of the guard and the server as {"error":"-200,<text>"} (ARBEITSPLAN 3.6 point 4).
static void test_guard_error_bodies() {
    char buf[ERROR_SIZE];
    TEST_ASSERT_TRUE(guardErrorJson(403, "Bad token", buf, sizeof(buf)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Invalid token: reload the page\"}", buf);
    guardErrorJson(401, "Login required", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Login required\"}", buf);
    guardErrorJson(421, "Host not allowed", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Host not allowed\"}", buf);
    guardErrorJson(403, "Foreign origin", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Foreign origin\"}", buf);
    guardErrorJson(503, nullptr, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Refused\"}", buf);
    TEST_ASSERT_TRUE(loginLockedJson(30, buf, sizeof(buf)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"-200,Too many failed logins\",\"retryAfter\":30}", buf);
    char tiny[8];
    TEST_ASSERT_EQUAL(0, (int)loginLockedJson(30, tiny, sizeof(tiny)));
}

static void test_state_status_texts() {
    Device d;
    Snapshot s = d.snapshot();
    JsonDocument doc;
    const struct {
        ChannelStatus st;
        const char* text;
    } chs[] = {{ChannelStatus::Off, "OFF"}, {ChannelStatus::On, "ON"}, {ChannelStatus::Error, "ERR"}, {ChannelStatus::NoDevice, "NOHW"}};
    for (const auto& k : chs) {
        s.status[1] = k.st;
        parse(state(s, info()), doc);
        TEST_ASSERT_EQUAL_STRING(k.text, doc["ch"][1]["status"]);
    }
    const struct {
        Ch3Status st;
        const char* text;
    } c3[] = {{Ch3Status::Off, "OFF"},  {Ch3Status::Active, "ON"},     {Ch3Status::MasterOff, "WAIT"},
              {Ch3Status::FreqTooLow, "WAIT"}, {Ch3Status::FreqTooHigh, "WAIT"}, {Ch3Status::Error, "ERR"},
              {Ch3Status::NoDevice, "NOHW"}};
    for (const auto& k : c3) {
        s.ch3Status = k.st;
        parse(state(s, info()), doc);
        TEST_ASSERT_EQUAL_STRING(k.text, doc["ch3"]["status"]);
    }
    const struct {
        LinkState l;
        const char* text;
    } links[] = {{LinkState::Connected, "CONN"}, {LinkState::NotFound, "NFO"}, {LinkState::Blocked, "BLOC"},
                 {LinkState::UnknownDevice, "UNKN"}};
    for (const auto& k : links) {
        s.link = k.l;
        parse(state(s, info()), doc);
        TEST_ASSERT_EQUAL_STRING(k.text, doc["hw"]);
    }
    const struct {
        SaveState st;
        const char* text;
    } saves[] = {{SaveState::Saved, "saved"}, {SaveState::Pending, "pending"}, {SaveState::Failed, "failed"},
                 {SaveState::Unavailable, "unavailable"}};
    for (const auto& k : saves) {
        s.saveState = k.st;
        parse(state(s, info()), doc);
        TEST_ASSERT_EQUAL_STRING(k.text, doc["save"]);
    }
    s.state.sys.tuning_mode = TuningMode::Integer;
    s.state.sys.spread = Spread::Center05;
    s.state.sys.off_state = OffState::HighZ;
    s.remoteOutputsAllowed = false;
    s.state.ch3.master = 2;
    s.actualHz[2] = 14000000.26;
    s.actualHz[0] = -1.0; // never shown as such
    parse(state(s, info()), doc);
    TEST_ASSERT_EQUAL_STRING("integer", doc["config"]["tuning"]);
    TEST_ASSERT_EQUAL_STRING("center05", doc["config"]["spread"]);
    TEST_ASSERT_EQUAL_STRING("hiz", doc["config"]["offState"]);
    TEST_ASSERT_EQUAL_STRING("local", doc["config"]["remoteOutputs"]);
    TEST_ASSERT_EQUAL_UINT64(s.state.ch2.frequency, doc["ch3"]["frequency"].as<uint64_t>());
    TEST_ASSERT_EQUAL_STRING("OFF=High-Z", doc["ch"][1]["modes"][0]);
    TEST_ASSERT_EQUAL_STRING("Integer", doc["ch"][1]["modes"][1]);
    const std::string json = state(s, info());
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"actual\":14000000.3"));
    TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"actual\":0.0"));
    // Defaults: no mode entries.
    Device fresh;
    parse(state(fresh.snapshot(), info()), doc);
    TEST_ASSERT_EQUAL(0, (int)doc["ch"][0]["modes"].size());
    // WiFi modes and the AP.
    DeviceInfo i = info();
    i.mode = WifiMode::AccessPoint;
    i.connected = false;
    i.ssid[0] = '\0';
    i.rssi = 0;
    i.ip = (192u << 24) | (168u << 16) | (4u << 8) | 1u;
    i.apActive = true;
    i.apOpen = true;
    i.apStations = 2;
    parse(state(s, i), doc);
    TEST_ASSERT_EQUAL_STRING("ap", doc["wifi"]["mode"]);
    TEST_ASSERT_EQUAL_STRING("", doc["wifi"]["ssid"]);
    TEST_ASSERT_EQUAL_STRING("192.168.4.1", doc["wifi"]["ip"]);
    TEST_ASSERT_TRUE(doc["wifi"]["ap"]["active"].as<bool>());
    TEST_ASSERT_TRUE(doc["wifi"]["ap"]["open"].as<bool>());
    TEST_ASSERT_EQUAL(2, doc["wifi"]["ap"]["stations"].as<int>());
    i.mode = WifiMode::Off;
    i.ip = 0;
    parse(state(s, i), doc);
    TEST_ASSERT_EQUAL_STRING("off", doc["wifi"]["mode"]);
    TEST_ASSERT_EQUAL_STRING("", doc["wifi"]["ip"]);
}

// Foreign strings are sanitized and escaped by ArduinoJson (W-S8, W-S9): the JSON stays valid
// and gives back exactly the sanitized text.
static void assertNoControlBytes(const std::string& json) {
    for (char ch : json) TEST_ASSERT_TRUE((unsigned char)ch >= 0x20);
}

static void test_state_escapes_hostile_ssids() {
    Device d;
    const Snapshot s = d.snapshot();
    const struct {
        const char* raw;
        const char* shown;
        const char* inJson;
    } cases[] = {
        {"<svg onload=alert(1)>", "<svg onload=alert(1)>", "\"ssid\":\"<svg onload=alert(1)>\""},
        {"a\"b\\c", "a\"b\\c", "\"ssid\":\"a\\\"b\\\\c\""},
        {"a\x01" "b\nc\x7F" "d\x1B", "a?b?c?d?", "\"ssid\":\"a?b?c?d?\""},
        {"\xF0" "A", "?A", "\"ssid\":\"?A\""},
        {"\xC3", "?", "\"ssid\":\"?\""},
        {"\xC2\x85x", "?x", "\"ssid\":\"?x\""}, // C1 control
        {"Caf\xC3\xA9", "Caf\xC3\xA9", "\"ssid\":\"Caf\xC3\xA9\""},
        {"</script><script>", "</script><script>", "\"ssid\":\"</script><script>\""},
    };
    for (const auto& k : cases) {
        DeviceInfo i = info();
        snprintf(i.ssid, sizeof(i.ssid), "%s", k.raw);
        snprintf(i.apSsid, sizeof(i.apSsid), "%s", k.raw);
        snprintf(i.name, sizeof(i.name), "%s", k.raw);
        const std::string json = state(s, i);
        assertNoControlBytes(json);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(json.c_str(), k.inJson), json.c_str());
        JsonDocument doc;
        parse(json, doc);
        TEST_ASSERT_EQUAL_STRING(k.shown, doc["wifi"]["ssid"]);
        TEST_ASSERT_EQUAL_STRING(k.shown, doc["wifi"]["ap"]["ssid"]);
        TEST_ASSERT_EQUAL_STRING(k.shown, doc["wifi"]["name"]);
    }
    // A full 33-byte buffer without NUL is read up to 32 bytes.
    DeviceInfo i = info();
    memset(i.ssid, 'x', sizeof(i.ssid));
    JsonDocument doc;
    parse(state(s, i), doc);
    TEST_ASSERT_EQUAL(32, (int)strlen(doc["wifi"]["ssid"].as<const char*>()));
}

// --- random inputs with a fixed seed (W-S25; fuzzing proper in S57) ---------------------

static const char* const PIECES[] = {"{", "}", "[", "]", ":", ",", " ", "\"enabled\"", "\"frequency\"", "\"frequencyText\"",
                                     "\"drive\"", "\"tuning\"", "\"spread\"", "\"offState\"", "\"correction\"", "\"step\"",
                                     "\"backlight\"", "\"master\"", "\"phase\"", "\"password\"", "\"remoteOutputs\"",
                                     "\"x\"", "true", "false", "null", "4", "8", "4.5", "-1", "1e3", "90", "7100000",
                                     "99999999999999999999", "\"10.7M\"", "\"lownoise\"", "\"down05\"", "\"hiz\"",
                                     "\"\\u0000\"", "\"a\\\"b\"", "'", "\\", "\n"};

static std::string randomBody() {
    std::string b;
    // Mostly well-formed objects of known keys, the rest token soup and raw bytes.
    const uint32_t mode = rnd() % 4;
    if (mode < 2) {
        b = "{";
        const uint32_t n = rnd() % 4;
        for (uint32_t i = 0; i < n; ++i) {
            if (i) b += ",";
            const uint32_t key = rnd() % 14;
            b += PIECES[7 + key];
            b += ":";
            // Half of the values fit the key's type, so the success paths are reached too.
            static const char* const typed[] = {"true", "7100000", "\"10.7M\"", "8", "\"lownoise\"", "\"down05\"",
                                                "\"hiz\"", "-250", "1000", "80", "2", "90", "\"a\\\"b\"", "\"x\""};
            b += rnd() % 2 ? typed[key] : PIECES[22 + rnd() % 18];
        }
        b += "}";
        if (mode == 1 && !b.empty()) b[rnd() % b.size()] = (char)(rnd() % 256);
    } else if (mode == 2) {
        const uint32_t n = rnd() % 12;
        for (uint32_t i = 0; i < n; ++i) b += PIECES[rnd() % (sizeof(PIECES) / sizeof(PIECES[0]))];
    } else {
        const uint32_t n = rnd() % 40;
        for (uint32_t i = 0; i < n; ++i) b += (char)(rnd() % 256);
    }
    return b;
}

static void checkReply(const Reply& r, const Commands& c, uint8_t channel, bool action = false) {
    static const uint16_t statuses[] = {200, 400, 403, 404, 413, 503};
    bool known = false;
    for (uint16_t s : statuses) known = known || r.status == s;
    TEST_ASSERT_TRUE(known);
    TEST_ASSERT_EQUAL(r.ok(), r.code == 0);
    TEST_ASSERT_TRUE(c.count <= MAX_COMMANDS);
    if (!r.ok()) {
        TEST_ASSERT_EQUAL_UINT8(0, c.count);
        char buf[ERROR_SIZE];
        TEST_ASSERT_TRUE(errorJson(r, buf, sizeof(buf)) > 0);
        JsonDocument doc;
        TEST_ASSERT_FALSE(deserializeJson(doc, buf));
        char code[8];
        snprintf(code, sizeof(code), "%d,", (int)r.code);
        TEST_ASSERT_EQUAL_STRING_LEN(code, doc["error"].as<const char*>(), strlen(code));
        return;
    }
    TEST_ASSERT_TRUE(c.count >= 1);
    for (uint8_t i = 0; i < c.count; ++i) {
        TEST_ASSERT_EQUAL((int)Source::Web, (int)c.list[i].source);
        if (!action) TEST_ASSERT_EQUAL_UINT8(channel, c.list[i].channel);
        for (uint8_t k = 0; k < i; ++k) TEST_ASSERT_TRUE(c.list[k].type != c.list[i].type);
    }
}

static void test_random_bodies_keep_the_rules() {
    unsigned accepted = 0;
    Commands c;
    char pw[65];
    for (int round = 0; round < 100000; ++round) {
        const std::string body = randomBody();
        const char* b = body.data();
        const size_t n = body.size();
        Reply r = parseChannel(1 + rnd() % 2, b, n, &c);
        checkReply(r, c, c.count ? c.list[0].channel : 0);
        for (uint8_t i = 0; i < c.count; ++i) {
            TEST_ASSERT_TRUE(c.list[i].type == CommandType::SetFrequency || c.list[i].type == CommandType::SetEnabled);
            if (c.list[i].type == CommandType::SetEnabled) TEST_ASSERT_TRUE(c.list[i].value == 0 || c.list[i].value == 1);
        }
        accepted += r.ok();
        r = parseCh3(b, n, &c);
        checkReply(r, c, 3);
        accepted += r.ok();
        r = parseConfig(b, n, &c);
        checkReply(r, c, 0);
        accepted += r.ok();
        for (uint8_t i = 0; i < c.count; ++i) {
            if (c.list[i].type == CommandType::Tuning) TEST_ASSERT_TRUE(c.list[i].value >= 0 && c.list[i].value <= 2);
            if (c.list[i].type == CommandType::Spread) TEST_ASSERT_TRUE(c.list[i].value >= 0 && c.list[i].value <= 3);
            if (c.list[i].type == CommandType::OffState) TEST_ASSERT_TRUE(c.list[i].value >= 0 && c.list[i].value <= 2);
        }
        r = parseAction(CommandType::SaveNow, b, n, &c);
        checkReply(r, c, 0, true);
        size_t len = 0;
        r = parseLogin(b, n, pw, sizeof(pw), &len);
        TEST_ASSERT_EQUAL(r.ok(), r.code == 0);
        if (r.ok()) TEST_ASSERT_EQUAL(len, strlen(pw));
        carriesSecret(b, n);
    }
    TEST_ASSERT_TRUE(accepted > 3000); // the generator reaches the success paths
}

static void test_random_ssids_stay_valid_json() {
    Device d;
    const Snapshot s = d.snapshot();
    for (int round = 0; round < 20000; ++round) {
        DeviceInfo i = info();
        for (size_t k = 0; k < sizeof(i.ssid); ++k) i.ssid[k] = (char)(rnd() % 256);
        const size_t len = rnd() % sizeof(i.ssid);
        i.ssid[len] = '\0';
        const std::string json = state(s, i);
        assertNoControlBytes(json);
        JsonDocument doc;
        parse(json, doc);
        char expect[33];
        Sanitize::text(i.ssid, strlen(i.ssid), expect, sizeof(expect), 32);
        TEST_ASSERT_EQUAL_STRING(expect, doc["wifi"]["ssid"]);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_channel_bodies);
    RUN_TEST(test_ch3_and_config_bodies);
    RUN_TEST(test_wrong_types_are_refused);
    RUN_TEST(test_unknown_keys_and_syntax);
    RUN_TEST(test_values_are_checked_by_the_controller);
    RUN_TEST(test_result_mapping);
    RUN_TEST(test_login_body_and_password_fields);
    RUN_TEST(test_state_json_contract);
    RUN_TEST(test_state_save_and_remote_indicator);
    RUN_TEST(test_state_status_texts);
    RUN_TEST(test_push_key);
    RUN_TEST(test_guard_error_bodies);
    RUN_TEST(test_state_escapes_hostile_ssids);
    RUN_TEST(test_random_bodies_keep_the_rules);
    RUN_TEST(test_random_ssids_stay_valid_json);
    return UNITY_END();
}
