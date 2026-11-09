// REST mapping of the WiFi settings (S55, ARBEITSPLAN 3.6, W-E9, W-S12): POST /api/wifi bodies
// with strict types, merged with the NetConfig rules (all or nothing), GET /api/wifi without
// any key, the scan answer, and random bodies with a fixed seed (W-S25).

#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>
#include <string>
#include <net/net_settings.h>
#include <remote/rest_wifi.h>

using RestJson::Reply;
using namespace RestWifi;

static const uint8_t MAC[6] = {0x8C, 0x94, 0xDF, 0x93, 0x1A, 0x2B};

void setUp() {}
void tearDown() {}

static uint32_t ip(const char* t) {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(NetConfig::parseIpv4(t, &v));
    return v;
}

static NetSettings base() {
    NetSettings s;
    NetConfig::defaults(s, MAC);
    s.mode = WifiMode::Client;
    NetConfig::setNetwork(s, 0, "Lab", "labpassword");
    snprintf(s.apPassword, sizeof(s.apPassword), "%s", "Xk4mP9tR2wQz");
    snprintf(s.webPassword, sizeof(s.webPassword), "%s", "pbkdf2-sha1$10000$00$00");
    return s;
}

// Parse and merge like the server; returns the reply and the merged settings.
static Reply apply(const char* body, const NetSettings& cur, NetSettings* next) {
    Change c;
    Reply r = parseChange(body, strlen(body), &c);
    if (r.ok()) r = merge(c, cur, next);
    else *next = cur;
    wipe(&c);
    return r;
}

static void assertError(int16_t code, const char* body) {
    NetSettings cur = base(), next;
    const Reply r = apply(body, cur, &next);
    TEST_ASSERT_EQUAL_INT_MESSAGE(code, r.code, body);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(400, r.status, body);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&cur, &next, sizeof(cur), body); // all or nothing
}

static void test_mode_name_and_networks() {
    const NetSettings cur = base();
    NetSettings n;
    TEST_ASSERT_TRUE(apply("{\"mode\":\"ap\"}", cur, &n).ok());
    TEST_ASSERT_EQUAL(WifiMode::AccessPoint, n.mode);
    TEST_ASSERT_TRUE(isFinal(n) == false);
    TEST_ASSERT_TRUE(apply("{\"mode\":\"off\"}", cur, &n).ok());
    TEST_ASSERT_TRUE(isFinal(n));
    TEST_ASSERT_TRUE(apply("{\"name\":\"bench-2\"}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("bench-2", n.deviceName);
    // New network with password, slot 1.
    TEST_ASSERT_TRUE(apply("{\"networks\":[{\"slot\":1,\"ssid\":\"Guest\",\"password\":\"guestpass1\"}]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("Guest", n.networks[1].ssid);
    TEST_ASSERT_EQUAL_STRING("guestpass1", n.networks[1].key);
    TEST_ASSERT_EQUAL_STRING("labpassword", n.networks[0].key);
    // Same SSID without password keeps the key; a new SSID without password is open.
    TEST_ASSERT_TRUE(apply("{\"networks\":[{\"slot\":0,\"ssid\":\"Lab\"}]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("labpassword", n.networks[0].key);
    TEST_ASSERT_TRUE(apply("{\"networks\":[{\"slot\":0,\"ssid\":\"Lab2\"}]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("Lab2", n.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("", n.networks[0].key);
    // "" = open on purpose; 64 hex digits = PMK.
    TEST_ASSERT_TRUE(apply("{\"networks\":[{\"slot\":0,\"ssid\":\"Lab\",\"password\":\"\"}]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("", n.networks[0].key);
    const std::string pmk(64, 'a');
    const std::string body = "{\"networks\":[{\"slot\":0,\"ssid\":\"Lab\",\"password\":\"" + pmk + "\"}]}";
    TEST_ASSERT_TRUE(apply(body.c_str(), cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING(pmk.c_str(), n.networks[0].key);
    // Delete first, then networks.
    TEST_ASSERT_TRUE(apply("{\"delete\":[0]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("", n.networks[0].ssid);
    TEST_ASSERT_TRUE(apply("{\"delete\":[0],\"networks\":[{\"slot\":0,\"ssid\":\"New\",\"password\":\"newpass12\"}]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("New", n.networks[0].ssid);
    // The web password is never touched by the web.
    TEST_ASSERT_EQUAL_STRING(cur.webPassword, n.webPassword);
    // Two entries, both slots.
    TEST_ASSERT_TRUE(apply("{\"networks\":[{\"slot\":0,\"ssid\":\"A\",\"password\":\"aaaaaaaa\"},{\"slot\":1,\"ssid\":\"B\",\"password\":\"\"}]}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("A", n.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("B", n.networks[1].ssid);
}

static void test_static_ip_and_ap() {
    const NetSettings cur = base();
    NetSettings n;
    TEST_ASSERT_TRUE(apply("{\"staticIp\":{\"enabled\":true,\"ip\":\"192.168.1.60\",\"mask\":\"255.255.255.0\",\"gateway\":\"192.168.1.1\",\"dns\":\"\"}}", cur, &n).ok());
    TEST_ASSERT_TRUE(n.staticIp.enabled);
    TEST_ASSERT_EQUAL_HEX32(ip("192.168.1.60"), n.staticIp.ip);
    TEST_ASSERT_EQUAL_HEX32(0, n.staticIp.dns);
    // Missing fields keep their values: switching off keeps the addresses.
    NetSettings off;
    TEST_ASSERT_TRUE(apply("{\"staticIp\":{\"enabled\":false}}", n, &off).ok());
    TEST_ASSERT_FALSE(off.staticIp.enabled);
    TEST_ASSERT_EQUAL_HEX32(ip("192.168.1.60"), off.staticIp.ip);
    TEST_ASSERT_TRUE(apply("{\"ap\":{\"open\":true}}", cur, &n).ok());
    TEST_ASSERT_TRUE(n.apOpen);
    TEST_ASSERT_TRUE(apply("{\"ap\":{\"password\":\"newAPpass9\"}}", cur, &n).ok());
    TEST_ASSERT_EQUAL_STRING("newAPpass9", n.apPassword);
    // Plausibility (W-S12): -221 with the reason; bad mask or text -224.
    NetSettings c2 = cur;
    Change c;
    const char* sameGw = "{\"staticIp\":{\"enabled\":true,\"ip\":\"10.0.0.1\",\"mask\":\"255.255.255.0\",\"gateway\":\"10.0.0.1\"}}";
    TEST_ASSERT_TRUE(parseChange(sameGw, strlen(sameGw), &c).ok());
    const Reply r = merge(c, c2, &n);
    TEST_ASSERT_EQUAL_INT(-221, r.code);
    TEST_ASSERT_EQUAL_STRING("The address equals the gateway.", r.info);
    assertError(-221, "{\"staticIp\":{\"enabled\":true,\"ip\":\"10.0.0.5\",\"mask\":\"255.255.255.0\",\"gateway\":\"10.0.1.1\"}}");
    assertError(-221, "{\"staticIp\":{\"enabled\":true,\"ip\":\"10.0.0.255\",\"mask\":\"255.255.255.0\",\"gateway\":\"10.0.0.1\"}}");
    assertError(-224, "{\"staticIp\":{\"enabled\":true,\"ip\":\"10.0.0.5\",\"mask\":\"255.0.255.0\",\"gateway\":\"10.0.0.1\"}}");
    assertError(-224, "{\"staticIp\":{\"enabled\":true,\"ip\":\"10.0.0.05\",\"mask\":\"255.255.255.0\",\"gateway\":\"10.0.0.1\"}}");
    assertError(-224, "{\"staticIp\":{\"ip\":\"999.0.0.1\"}}");
    assertError(-224, "{\"staticIp\":{\"ip\":\"1234567890123456\"}}");
    assertError(-224, "{\"ap\":{\"password\":\"short\"}}");
}

static void test_refusals() {
    assertError(-109, "{}");
    assertError(-102, "{\"mode\":\"ap\"");
    assertError(-102, "{\"mode\":\"ap\",\"mode\":\"off\"}");
    assertError(-104, "[]");
    assertError(-104, "{\"mode\":1}");
    assertError(-224, "{\"mode\":\"station\"}");
    assertError(-108, "{\"ssid\":\"x\"}");
    assertError(-104, "{\"networks\":{}}");
    assertError(-104, "{\"networks\":[1]}");
    assertError(-222, "{\"networks\":[{\"slot\":0,\"ssid\":\"a\"},{\"slot\":1,\"ssid\":\"b\"},{\"slot\":0,\"ssid\":\"c\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":2,\"ssid\":\"a\"}]}");
    assertError(-104, "{\"networks\":[{\"slot\":\"0\",\"ssid\":\"a\"}]}");
    assertError(-104, "{\"networks\":[{\"slot\":0.0,\"ssid\":\"a\"}]}");
    assertError(-224, "{\"networks\":[{\"ssid\":\"a\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0,\"ssid\":\"\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0,\"ssid\":\"123456789012345678901234567890123\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0,\"ssid\":\"a\"},{\"slot\":0,\"ssid\":\"b\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0,\"ssid\":\"a\",\"password\":\"short\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0,\"ssid\":\"a\",\"password\":\"tab\\tinside\"}]}");
    assertError(-224, "{\"networks\":[{\"slot\":0,\"ssid\":\"a\\u0000b\"}]}");
    assertError(-108, "{\"networks\":[{\"slot\":0,\"ssid\":\"a\",\"key\":\"x\"}]}");
    assertError(-224, "{\"delete\":[2]}");
    assertError(-104, "{\"delete\":[true]}");
    assertError(-222, "{\"delete\":[0,1,0]}");
    assertError(-224, "{\"name\":\"Bad_Name\"}");
    assertError(-224, "{\"name\":\"-x\"}");
    assertError(-104, "{\"staticIp\":{\"enabled\":1}}");
    assertError(-108, "{\"staticIp\":{\"gw\":\"1.2.3.4\"}}");
    assertError(-104, "{\"ap\":{\"open\":\"yes\"}}");
    assertError(-108, "{\"ap\":{\"ssid\":\"x\"}}");
    // Partly valid: nothing changes (checked by assertError) even if the first part was fine.
    assertError(-224, "{\"name\":\"bench\",\"ap\":{\"password\":\"short\"}}");
    // Confirm takes {} only.
    TEST_ASSERT_TRUE(parseConfirm("{}", 2).ok());
    TEST_ASSERT_EQUAL_INT(-108, parseConfirm("{\"x\":1}", 7).code);
    TEST_ASSERT_EQUAL_INT(-102, parseConfirm("", 0).code);
}

static void test_settings_json_has_no_keys() {
    NetSettings s = base();
    NetConfig::setNetwork(s, 1, "Guest", "");
    s.staticIp = StaticIp {true, ip("10.0.0.20"), ip("255.255.255.0"), ip("10.0.0.1"), 0};
    char buf[SETTINGS_SIZE];
    TEST_ASSERT_TRUE(buildSettings(s, "CYD-HF-1A2B", buf, sizeof(buf)) > 0);
    TEST_ASSERT_NULL(strstr(buf, "labpassword"));
    TEST_ASSERT_NULL(strstr(buf, "Xk4mP9tR2wQz"));
    TEST_ASSERT_NULL(strstr(buf, "pbkdf2"));
    JsonDocument d;
    TEST_ASSERT_FALSE(deserializeJson(d, buf));
    TEST_ASSERT_EQUAL_STRING("client", d["mode"]);
    TEST_ASSERT_EQUAL_STRING("Lab", d["networks"][0]["ssid"]);
    TEST_ASSERT_EQUAL_STRING("set", d["networks"][0]["key"]);
    TEST_ASSERT_EQUAL_STRING("open", d["networks"][1]["key"]);
    TEST_ASSERT_EQUAL_STRING("CYD-HF-1A2B", d["ap"]["ssid"]);
    TEST_ASSERT_TRUE(d["ap"]["passwordSet"].as<bool>());
    TEST_ASSERT_FALSE(d["ap"]["open"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("cyd-hf-1a2b", d["name"]);
    TEST_ASSERT_TRUE(d["staticIp"]["enabled"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("10.0.0.20", d["staticIp"]["ip"]);
    TEST_ASSERT_EQUAL_STRING("", d["staticIp"]["dns"]);
    NetConfig::clearNetwork(s, 1);
    buildSettings(s, "CYD-HF-1A2B", buf, sizeof(buf));
    TEST_ASSERT_FALSE(deserializeJson(d, buf));
    TEST_ASSERT_EQUAL_STRING("none", d["networks"][1]["key"]);
    // Hostile stored SSIDs stay valid JSON (W-S8, W-S9); the longest form fits.
    memset(s.networks[0].ssid, '"', 32);
    s.networks[0].ssid[32] = '\0';
    memset(s.networks[1].ssid, 0xC3, 32);
    s.networks[1].ssid[32] = '\0';
    char ap[33];
    memset(ap, '\\', 32);
    ap[32] = '\0';
    TEST_ASSERT_TRUE(buildSettings(s, ap, buf, sizeof(buf)) > 0);
    TEST_ASSERT_FALSE(deserializeJson(d, buf));
    char tiny[64];
    TEST_ASSERT_EQUAL(0, (int)buildSettings(s, ap, tiny, sizeof(tiny)));
}

static void test_scan_json() {
    ScanList list;
    char buf[SCAN_SIZE];
    TEST_ASSERT_TRUE(buildScan("running", nullptr, buf, sizeof(buf)) > 0);
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"running\",\"networks\":[]}", buf);
    list.add("Lab", 3, -48, true);
    list.add("open-hotspot", 12, -75, false);
    list.add("<img src=x>", 11, -70, true);
    TEST_ASSERT_TRUE(buildScan("done", &list, buf, sizeof(buf)) > 0);
    JsonDocument d;
    TEST_ASSERT_FALSE(deserializeJson(d, buf));
    TEST_ASSERT_EQUAL_STRING("done", d["state"]);
    TEST_ASSERT_EQUAL(3, (int)d["networks"].size());
    TEST_ASSERT_EQUAL_STRING("Lab", d["networks"][0]["ssid"]);
    TEST_ASSERT_EQUAL(-48, d["networks"][0]["rssi"].as<int>());
    TEST_ASSERT_TRUE(d["networks"][0]["secure"].as<bool>());
    TEST_ASSERT_FALSE(d["networks"][2]["secure"].as<bool>());
    buildScan("failed", &list, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"state\":\"failed\",\"networks\":[]}", buf);
    // 15 hostile SSIDs of 32 quote characters fit SCAN_SIZE.
    ScanList full;
    for (int i = 0; i < 15; ++i) {
        char s[33];
        memset(s, '"', 32);
        s[31] = (char)('A' + i); // distinct
        s[32] = '\0';
        full.add(s, 32, (int8_t)(-40 - i), true);
    }
    TEST_ASSERT_EQUAL(15, full.count());
    TEST_ASSERT_TRUE(buildScan("done", &full, buf, sizeof(buf)) > 0);
    TEST_ASSERT_FALSE(deserializeJson(d, buf));
    TEST_ASSERT_EQUAL(15, (int)d["networks"].size());
}

// W-S25: random bodies from WiFi pieces never crash, never leave a half-merged state, and a
// success always gives settings that pass NetConfig's own checks.
static uint32_t seed = 7;
static uint32_t rnd() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}
static const char* const PIECES[] = {"{", "}", "[", "]", ":", ",", " ", "\"mode\"", "\"networks\"", "\"delete\"", "\"name\"",
                                     "\"staticIp\"", "\"ap\"", "\"slot\"", "\"ssid\"", "\"password\"", "\"enabled\"", "\"ip\"",
                                     "\"mask\"", "\"gateway\"", "\"dns\"", "\"open\"", "\"off\"", "\"client\"", "\"Lab\"",
                                     "\"labpassword\"", "\"10.0.0.5\"", "\"255.255.255.0\"", "\"10.0.0.1\"", "0", "1", "2", "true",
                                     "false", "null", "\"\"", "\"bench-1\"", "1.5", "-1"};

static void test_random_bodies() {
    const NetSettings cur = base();
    for (int i = 0; i < 100000; ++i) {
        std::string b;
        const int n = 1 + (int)(rnd() % 24);
        for (int k = 0; k < n; ++k) b += PIECES[rnd() % (sizeof(PIECES) / sizeof(PIECES[0]))];
        NetSettings next;
        const Reply r = apply(b.c_str(), cur, &next);
        if (!r.ok()) {
            TEST_ASSERT_EQUAL_MEMORY(&cur, &next, sizeof(cur));
            TEST_ASSERT_TRUE(r.code < 0);
            continue;
        }
        TEST_ASSERT_TRUE(NetConfig::validDeviceName(next.deviceName));
        for (int s = 0; s < 2; ++s) {
            if (next.networks[s].ssid[0]) TEST_ASSERT_TRUE(NetConfig::validSsid(next.networks[s].ssid));
        }
        if (next.staticIp.enabled) TEST_ASSERT_TRUE(NetConfig::checkStaticIp(next.staticIp) == NetConfig::IpCheck::Ok);
        TEST_ASSERT_EQUAL_STRING(cur.webPassword, next.webPassword);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mode_name_and_networks);
    RUN_TEST(test_static_ip_and_ap);
    RUN_TEST(test_refusals);
    RUN_TEST(test_settings_json_has_no_keys);
    RUN_TEST(test_scan_json);
    RUN_TEST(test_random_bodies);
    return UNITY_END();
}
