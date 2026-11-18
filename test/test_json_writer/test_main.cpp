// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fixed-buffer JSON writer (AF54, PERF-9): the state JSON and the error bodies are byte-identical
// to the former ArduinoJson implementation (reference.h) for many random states, foreign texts
// with every byte value, all enum values and every buffer size around the length.

#include <unity.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <string>
#include <remote/json_writer.h>
#include <remote/rest_json.h>
#include "reference.h"

void setUp() {}
void tearDown() {}

// Fixed-seed generator (xorshift32), so a failure repeats.
static uint32_t g_seed = 0x2545F491u;
static uint32_t rnd() {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}
static uint64_t rnd64() { return ((uint64_t)rnd() << 32) | rnd(); }
static uint32_t below(uint32_t n) { return n ? rnd() % n : 0; }

// Random raw text of up to maxLen bytes, any byte value (also quotes, backslash, controls,
// UTF-8 fragments), without the terminating NUL inside.
static void randomText(char* out, size_t size) {
    const size_t len = below((uint32_t)size); // 0..size-1
    static const char SPECIAL[] = "\"\\\b\f\n\r\t/\x01\x1f\x7f\xc3\xa4\xe2\x82\xac '";
    for (size_t i = 0; i < len; ++i) {
        char c;
        switch (below(4)) {
            case 0: c = SPECIAL[below(sizeof(SPECIAL) - 1)]; break;
            case 1: c = (char)(1 + below(255)); break;
            default: c = (char)('a' + below(26)); break;
        }
        out[i] = c;
    }
    out[len] = '\0';
}

static double randomActual() {
    switch (below(8)) {
        case 0: return NAN;
        case 1: return -1.0;
        case 2: return 1e13;
        case 3: return INFINITY;
        case 4: return 0.0;
        default: return (double)below(160000001) + (double)below(1000) / 1000.0;
    }
}

static Snapshot randomSnapshot() {
    Snapshot s;
    memset(&s, 0, sizeof(s));
    s.state.ch1.frequency = below(4) ? below(160000001) : rnd64();
    s.state.ch1.enabled = below(2);
    s.state.ch2.frequency = below(4) ? below(160000001) : rnd64();
    s.state.ch2.enabled = below(2);
    s.state.ch3.enabled = below(2);
    s.state.ch3.master = (uint8_t)below(4);
    s.state.ch3.phase = (uint16_t)(below(5) * 90 + (below(8) == 0 ? 1 : 0));
    s.state.sys.delta_step = below(3) ? DELTA_STEPS[below(DELTA_STEP_COUNT)] : rnd();
    s.state.sys.freq_correction = below(2) ? (int32_t)(below(10001)) - 5000 : (int32_t)rnd();
    s.state.sys.backlight = (uint8_t)rnd();
    s.state.sys.drive_ma = (uint8_t)(below(4) ? 2 + 2 * below(4) : rnd());
    s.state.sys.tuning_mode = (TuningMode)below(4);
    s.state.sys.spread = (Spread)below(5);
    s.state.sys.off_state = (OffState)below(4);
    s.status[0] = (ChannelStatus)below(5);
    s.status[1] = (ChannelStatus)below(5);
    s.ch3Status = (Ch3Status)below(8);
    for (double& a : s.actualHz) a = randomActual();
    s.hardwareReady = below(2);
    s.link = (LinkState)below(5);
    s.iqMinHz = below(2) ? IQ_MIN_HZ : rnd64();
    s.iqMaxHz = below(2) ? FREQ_MAX_HZ : rnd64();
    s.remoteOutputsAllowed = below(2);
    s.revision = rnd();
    s.saveState = (SaveState)below(5);
    s.lastRemote = (Source)below(6);
    s.lastRemoteMs = rnd();
    return s;
}

static RestJson::DeviceInfo randomInfo(char* version, size_t versionSize) {
    RestJson::DeviceInfo d;
    memset(&d, 0, sizeof(d));
    d.mode = (WifiMode)below(4);
    d.connected = below(2);
    randomText(d.ssid, sizeof(d.ssid));
    d.rssi = below(2) ? -(int32_t)below(100) : (int32_t)rnd();
    d.ip = below(3) ? rnd() : 0;
    randomText(d.name, sizeof(d.name));
    d.apActive = below(2);
    randomText(d.apSsid, sizeof(d.apSsid));
    d.apOpen = below(2);
    d.apStations = (uint8_t)rnd();
    if (below(2)) {
        randomText(version, versionSize);
        d.version = version;
    } else {
        d.version = nullptr;
    }
    d.confirmWithin = (uint16_t)rnd();
    d.passwordSet = below(2);
    return d;
}

static void expectSame(const char* what, uint32_t iter, const char* ref, size_t refLen, const char* got, size_t gotLen) {
    if (refLen == gotLen && strcmp(ref, got) == 0) return;
    char msg[96];
    snprintf(msg, sizeof(msg), "%s differs in iter %u (ref %u B, got %u B)", what, (unsigned)iter, (unsigned)refLen,
             (unsigned)gotLen);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(ref, got, msg);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(refLen, gotLen, msg);
}

static void test_state_matches_arduinojson() {
    static char ref[RestJson::STATE_SIZE], got[RestJson::STATE_SIZE];
    for (uint32_t iter = 0; iter < 20000; ++iter) {
        const Snapshot s = randomSnapshot();
        char version[40];
        const RestJson::DeviceInfo d = randomInfo(version, sizeof(version));
        // Inside and outside the REMOTE indicator window, also across the 32-bit wrap.
        const uint32_t now = below(2) ? s.lastRemoteMs + below(2 * REMOTE_INDICATOR_MS) : rnd();
        const size_t r = Reference::buildState(s, d, now, ref, sizeof(ref));
        const size_t g = RestJson::buildState(s, d, now, got, sizeof(got));
        TEST_ASSERT_TRUE(r > 0);
        expectSame("state", iter, ref, r, got, g);
    }
}

// Every buffer size around the length: both fit or both give 0 and "".
static void test_state_buffer_sizes() {
    static char ref[RestJson::STATE_SIZE], got[RestJson::STATE_SIZE];
    for (uint32_t iter = 0; iter < 200; ++iter) {
        const Snapshot s = randomSnapshot();
        char version[40];
        const RestJson::DeviceInfo d = randomInfo(version, sizeof(version));
        const size_t full = Reference::buildState(s, d, 0, ref, sizeof(ref));
        for (size_t size = full - 3; size <= full + 2; ++size) {
            memset(got, '#', sizeof(got));
            const size_t r = Reference::buildState(s, d, 0, ref, size);
            const size_t g = RestJson::buildState(s, d, 0, got, size);
            TEST_ASSERT_EQUAL_UINT(r, g);
            TEST_ASSERT_EQUAL_STRING(ref, got);
            if (size < sizeof(got)) TEST_ASSERT_EQUAL_CHAR('#', got[size]); // nothing past the buffer
        }
    }
}

static void test_error_bodies_match_arduinojson() {
    char ref[RestJson::ERROR_SIZE], got[RestJson::ERROR_SIZE];
    static const int16_t CODES[] = {0,    -102, -104, -108, -109, -114, -200, -221,
                                    -222, -223, -224, -240, -999, 1,    SHRT_MIN};
    for (uint32_t iter = 0; iter < 20000; ++iter) {
        char info[64];
        randomText(info, sizeof(info));
        const RestJson::Reply rep {(uint16_t)rnd(), CODES[below(sizeof(CODES) / sizeof(CODES[0]))],
                                   below(4) ? info : nullptr};
        const size_t size = below(4) ? sizeof(ref) : 1 + below(sizeof(ref));
        const size_t r = Reference::errorJson(rep, ref, size);
        const size_t g = RestJson::errorJson(rep, got, size);
        expectSame("error", iter, ref, r, got, g);
        const uint32_t retry = below(2) ? below(3600) : rnd();
        expectSame("locked", iter, ref, Reference::loginLockedJson(retry, ref, size), got,
                   RestJson::loginLockedJson(retry, got, size));
    }
}

static void test_writer_basics() {
    char buf[128];
    JsonWriter w(buf, sizeof(buf));
    w.beginObject();
    w.key("a");
    w.number((int64_t)INT64_MIN);
    w.key("b");
    w.beginArray();
    w.number((uint64_t)UINT64_MAX);
    w.boolean(true);
    w.null();
    w.beginObject();
    w.endObject();
    w.endArray();
    w.key("c");
    w.string("q\"\\\n");
    w.endObject();
    const size_t n = w.finish(); // terminates the text
    TEST_ASSERT_EQUAL_STRING("{\"a\":-9223372036854775808,\"b\":[18446744073709551615,true,null,{}],\"c\":\"q\\\"\\\\\\n\"}",
                             buf);
    TEST_ASSERT_EQUAL_UINT(strlen(buf), n);
    // Too small: 0 and "", never past the buffer.
    char small[8];
    memset(small, '#', sizeof(small));
    JsonWriter t(small, 5);
    t.beginObject();
    t.key("long");
    t.endObject();
    TEST_ASSERT_TRUE(t.overflowed());
    TEST_ASSERT_EQUAL_UINT(0, t.finish());
    TEST_ASSERT_EQUAL_STRING("", small);
    TEST_ASSERT_EQUAL_CHAR('#', small[5]);
    JsonWriter none(nullptr, 0);
    none.null();
    TEST_ASSERT_EQUAL_UINT(0, none.finish());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_writer_basics);
    RUN_TEST(test_state_matches_arduinojson);
    RUN_TEST(test_state_buffer_sizes);
    RUN_TEST(test_error_bodies_match_arduinojson);
    return UNITY_END();
}
