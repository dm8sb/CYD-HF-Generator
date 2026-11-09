// Network settings (ARBEITSPLAN 3.3, W-E3, W-E5, W-E7, W-S12, W-S14, W-S16, D15): SHA-1/PBKDF2
// against the RFC and IEEE 802.11i vectors (kept for the web password, S55), WiFi keys,
// AP password, device name, static IP plausibility, NVS namespace "hfnet", factory reset.

#include <unity.h>
#include <Preferences.h>
#include <string.h>
#include <string>
#include <net/net_settings.h>
#include <net/pbkdf2.h>
#include <net/request_guard.h>

static const uint8_t MAC[6] = {0x8C, 0x94, 0xDF, 0x93, 0x1A, 0x2B};

void setUp() { Fake::nvs().reset(); }
void tearDown() {}

static std::string hex(const uint8_t* d, size_t n) {
    static const char H[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < n; ++i) {
        s += H[d[i] >> 4];
        s += H[d[i] & 15];
    }
    return s;
}

static std::string sha1(const char* text) {
    uint8_t out[20];
    Crypto::sha1((const uint8_t*)text, strlen(text), out);
    return hex(out, 20);
}

static std::string pbkdf2(const char* pw, const char* salt, uint32_t iterations, size_t len) {
    uint8_t out[32];
    Crypto::pbkdf2HmacSha1((const uint8_t*)pw, strlen(pw), (const uint8_t*)salt, strlen(salt), iterations, out, len);
    return hex(out, len);
}

static void test_sha1_and_hmac_vectors() {
    TEST_ASSERT_EQUAL_STRING("da39a3ee5e6b4b0d3255bfef95601890afd80709", sha1("").c_str());
    TEST_ASSERT_EQUAL_STRING("a9993e364706816aba3e25717850c26c9cd0d89d", sha1("abc").c_str());
    TEST_ASSERT_EQUAL_STRING("84983e441c3bd26ebaae4aa1f95129e5e54670f1",
                             sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq").c_str());
    // RFC 2202, test cases 2 and 6 (key longer than a block).
    uint8_t out[20];
    const char* data = "what do ya want for nothing?";
    Crypto::hmacSha1((const uint8_t*)"Jefe", 4, (const uint8_t*)data, strlen(data), out);
    TEST_ASSERT_EQUAL_STRING("effcdf6ae5eb2fa2d27416d5f184df9c259a7c79", hex(out, 20).c_str());
    uint8_t key[80];
    memset(key, 0xAA, sizeof(key));
    const char* data6 = "Test Using Larger Than Block-Size Key - Hash Key First";
    Crypto::hmacSha1(key, sizeof(key), (const uint8_t*)data6, strlen(data6), out);
    TEST_ASSERT_EQUAL_STRING("aa4ae5e15272d00e95705637ce8a3b55ed402112", hex(out, 20).c_str());
}

static void test_pbkdf2_and_wpa_vectors() {
    // RFC 6070
    TEST_ASSERT_EQUAL_STRING("0c60c80f961f0e71f3a9b524af6012062fe037a6", pbkdf2("password", "salt", 1, 20).c_str());
    TEST_ASSERT_EQUAL_STRING("ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", pbkdf2("password", "salt", 2, 20).c_str());
    TEST_ASSERT_EQUAL_STRING("4b007901b765489abead49d926f721d065a429c1", pbkdf2("password", "salt", 4096, 20).c_str());
    TEST_ASSERT_EQUAL_STRING("3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038",
                             pbkdf2("passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 25).c_str());
    // IEEE 802.11i, annex H.4
    char pmk[65];
    Crypto::wpaPmkHex("IEEE", 4, "password", pmk);
    TEST_ASSERT_EQUAL_STRING("f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e", pmk);
    Crypto::wpaPmkHex("ThisIsASSID", 11, "ThisIsAPassword", pmk);
    TEST_ASSERT_EQUAL_STRING("0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af", pmk);
}

static void test_names_from_the_mac() {
    char buf[40];
    NetConfig::defaultDeviceName(MAC, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("cyd-hf-1a2b", buf);
    NetConfig::apSsid(MAC, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("CYD-HF-1A2B", buf);
    TEST_ASSERT_TRUE(NetConfig::validDeviceName("cyd-hf-1a2b"));
    TEST_ASSERT_TRUE(NetConfig::validDeviceName("a"));
    TEST_ASSERT_TRUE(NetConfig::validDeviceName("abcdefghijabcdefghijabcdefghij12"));   // 32
    TEST_ASSERT_FALSE(NetConfig::validDeviceName("abcdefghijabcdefghijabcdefghij123")); // 33
    TEST_ASSERT_FALSE(NetConfig::validDeviceName(""));
    TEST_ASSERT_FALSE(NetConfig::validDeviceName("-lab"));
    TEST_ASSERT_FALSE(NetConfig::validDeviceName("lab-"));
    TEST_ASSERT_FALSE(NetConfig::validDeviceName("Lab"));
    TEST_ASSERT_FALSE(NetConfig::validDeviceName("lab_1"));
    TEST_ASSERT_FALSE(NetConfig::validDeviceName("lab.local"));
    TEST_ASSERT_FALSE(NetConfig::validDeviceName(nullptr));
}

static uint32_t seq[64];
static size_t seqPos;
static uint32_t nextRand() { return seq[seqPos++ % 64]; }

static void test_ap_password() {
    for (size_t i = 0; i < 64; ++i) seq[i] = (uint32_t)(i * 2654435761u);
    seqPos = 0;
    char pw[NetConfig::AP_PASSWORD_LEN + 1];
    NetConfig::generateApPassword(nextRand, pw);
    TEST_ASSERT_EQUAL_size_t(12, strlen(pw));
    TEST_ASSERT_TRUE(NetConfig::validPassphrase(pw));
    for (const char* p = pw; *p; ++p) TEST_ASSERT_NULL(strchr("0O1lI", *p));
    // Values in the uneven top range are skipped, not folded onto the first characters.
    for (size_t i = 0; i < 64; ++i) seq[i] = (i % 2) ? 0xFFFFFFFFu : 0u;
    seqPos = 0;
    NetConfig::generateApPassword(nextRand, pw);
    TEST_ASSERT_EQUAL_STRING("aaaaaaaaaaaa", pw);
    TEST_ASSERT_EQUAL_size_t(23, seqPos); // 12 accepted, 11 skipped in between
}

// D15: the passphrase is stored as it is (WPA3-SAE needs it), a PMK in hex is accepted too.
static void test_network_keys() {
    NetSettings s;
    NetConfig::defaults(s, MAC);
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::Ok, NetConfig::setNetwork(s, 0, "IEEE", "pass word$&"));
    TEST_ASSERT_EQUAL_STRING("pass word$&", s.networks[0].key);
    // A PMK is taken as it is (lower case), an empty passphrase is an open network.
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::Ok,
                      NetConfig::setNetwork(s, 1, "Lab", "0DC0D6EB90555ED6419756B9A15EC3E3209B63DF707DD508D14581F8982721AF"));
    TEST_ASSERT_EQUAL_STRING("0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af", s.networks[1].key);
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::Ok, NetConfig::setNetwork(s, 1, "Guest", ""));
    TEST_ASSERT_EQUAL_STRING("", s.networks[1].key);
    // Refused: nothing changes.
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::BadPassphrase, NetConfig::setNetwork(s, 1, "Lab", "short"));
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::BadPassphrase, NetConfig::setNetwork(s, 1, "Lab", std::string(64, 'x').c_str()));
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::BadPassphrase, NetConfig::setNetwork(s, 1, "Lab", "pass\nword"));
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::BadSsid, NetConfig::setNetwork(s, 1, "", "password"));
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::BadSsid, NetConfig::setNetwork(s, 1, std::string(33, 'x').c_str(), "password"));
    TEST_ASSERT_EQUAL(NetConfig::KeyResult::BadSsid, NetConfig::setNetwork(s, 2, "Lab", "password"));
    TEST_ASSERT_EQUAL_STRING("Guest", s.networks[1].ssid);
    Storage st;
    st.begin(NetConfig::NAMESPACE);
    TEST_ASSERT_TRUE(NetConfig::save(st, s));
    NetSettings r;
    TEST_ASSERT_TRUE(NetConfig::load(st, r, MAC));
    TEST_ASSERT_EQUAL_STRING("pass word$&", r.networks[0].key);
    NetConfig::clearNetwork(s, 0);
    TEST_ASSERT_EQUAL_STRING("", s.networks[0].ssid);
}

static uint32_t ip(const char* t) {
    uint32_t v = 0;
    TEST_ASSERT_TRUE_MESSAGE(NetConfig::parseIpv4(t, &v), t);
    return v;
}

static void test_static_ip_plausibility() {
    using NetConfig::IpCheck;
    StaticIp c {true, ip("192.168.1.50"), ip("255.255.255.0"), ip("192.168.1.1"), 0};
    TEST_ASSERT_EQUAL(IpCheck::Ok, NetConfig::checkStaticIp(c));
    StaticIp x = c;
    x.gateway = x.ip;
    TEST_ASSERT_EQUAL(IpCheck::SameAsGateway, NetConfig::checkStaticIp(x));
    x = c;
    x.gateway = ip("192.168.2.1");
    TEST_ASSERT_EQUAL(IpCheck::OutsideSubnet, NetConfig::checkStaticIp(x));
    x = c;
    x.ip = ip("192.168.1.0");
    TEST_ASSERT_EQUAL(IpCheck::NetworkAddress, NetConfig::checkStaticIp(x));
    x = c;
    x.ip = ip("192.168.1.255");
    TEST_ASSERT_EQUAL(IpCheck::BroadcastAddress, NetConfig::checkStaticIp(x));
    x = c;
    x.gateway = ip("192.168.1.255");
    TEST_ASSERT_EQUAL(IpCheck::BroadcastAddress, NetConfig::checkStaticIp(x));
    x = c;
    x.mask = ip("255.0.255.0");
    TEST_ASSERT_EQUAL(IpCheck::BadMask, NetConfig::checkStaticIp(x));
    x.mask = ip("255.255.255.254");
    TEST_ASSERT_EQUAL(IpCheck::BadMask, NetConfig::checkStaticIp(x));
    x.mask = 0;
    TEST_ASSERT_EQUAL(IpCheck::BadMask, NetConfig::checkStaticIp(x));
    x = c;
    x.mask = ip("255.255.255.252");
    x.ip = ip("192.168.1.1");
    x.gateway = ip("192.168.1.2");
    TEST_ASSERT_EQUAL(IpCheck::Ok, NetConfig::checkStaticIp(x)); // /30
    x = c;
    x.ip = 0;
    TEST_ASSERT_EQUAL(IpCheck::ZeroAddress, NetConfig::checkStaticIp(x));
    // Every result has a reason text.
    for (uint8_t i = 0; i <= (uint8_t)IpCheck::BroadcastAddress; ++i) {
        TEST_ASSERT_TRUE(strlen(NetConfig::ipCheckText((IpCheck)i)) > 1);
    }
}

static void test_ipv4_text() {
    uint32_t v = 0;
    TEST_ASSERT_TRUE(NetConfig::parseIpv4("0.0.0.0", &v));
    TEST_ASSERT_EQUAL_HEX32(0xC0A80132, ip("192.168.1.50"));
    const char* bad[] = {"192.168.1", "192.168.1.50.1", "256.1.1.1", "01.2.3.4", "1.2.3.4 ", " 1.2.3.4", "1..2.3", "a.b.c.d", ""};
    for (const char* b : bad) TEST_ASSERT_FALSE_MESSAGE(NetConfig::parseIpv4(b, &v), b);
    char buf[16];
    NetConfig::formatIpv4(0xC0A80132, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("192.168.1.50", buf);
    NetConfig::formatIpv4(0xFFFFFFFF, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("255.255.255.255", buf);
}

static void test_load_save_in_own_namespace() {
    Storage st;
    st.begin(NetConfig::NAMESPACE);
    NetSettings s;
    TEST_ASSERT_FALSE(NetConfig::load(st, s, MAC)); // nothing stored: defaults
    TEST_ASSERT_EQUAL(WifiMode::Off, s.mode);
    TEST_ASSERT_EQUAL_STRING("cyd-hf-1a2b", s.deviceName);
    TEST_ASSERT_EQUAL_STRING("", s.apPassword);
    s.mode = WifiMode::Client;
    NetConfig::setNetwork(s, 0, "Lab", "labpassword");
    NetConfig::setNetwork(s, 1, "Guest", "");
    snprintf(s.apPassword, sizeof(s.apPassword), "%s", "abcdefghjkmn");
    snprintf(s.deviceName, sizeof(s.deviceName), "%s", "bench-1");
    s.staticIp = StaticIp {true, ip("10.0.0.20"), ip("255.255.255.0"), ip("10.0.0.1"), ip("10.0.0.2")};
    TEST_ASSERT_TRUE(NetConfig::save(st, s));
    // The generator's own namespace is not touched.
    Storage gen;
    gen.begin("hfgen");
    gen.putU8("tune", 1);
    NetSettings r;
    TEST_ASSERT_TRUE(NetConfig::load(st, r, MAC));
    TEST_ASSERT_EQUAL(WifiMode::Client, r.mode);
    TEST_ASSERT_EQUAL_STRING("Lab", r.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING(s.networks[0].key, r.networks[0].key);
    TEST_ASSERT_EQUAL_STRING("Guest", r.networks[1].ssid);
    TEST_ASSERT_EQUAL_STRING("", r.networks[1].key);
    TEST_ASSERT_EQUAL_STRING("abcdefghjkmn", r.apPassword);
    TEST_ASSERT_EQUAL_STRING("bench-1", r.deviceName);
    TEST_ASSERT_TRUE(r.staticIp.enabled);
    TEST_ASSERT_EQUAL_HEX32(ip("10.0.0.2"), r.staticIp.dns);
    TEST_ASSERT_EQUAL_UINT8(1, gen.getU8("tune", 0));
    // Clearing a network removes its keys.
    NetConfig::clearNetwork(r, 1);
    TEST_ASSERT_TRUE(NetConfig::save(st, r));
    NetSettings r2;
    NetConfig::load(st, r2, MAC);
    TEST_ASSERT_EQUAL_STRING("", r2.networks[1].ssid);
}

static void test_load_replaces_invalid_values() {
    Storage st;
    st.begin(NetConfig::NAMESPACE);
    st.putU8("mode", 9);
    st.putString("n0_ssid", "Lab");
    st.putString("n0_key", "short");
    st.putString("ap_pw", "short");
    st.putString("name", "Bad_Name");
    st.putU8("sip_en", 1);
    st.putU32("sip_ip", ip("10.0.0.1"));
    st.putU32("sip_mask", ip("255.255.255.0"));
    st.putU32("sip_gw", ip("10.0.0.1")); // same as the address
    NetSettings s;
    TEST_ASSERT_TRUE(NetConfig::load(st, s, MAC));
    TEST_ASSERT_EQUAL(WifiMode::Off, s.mode);
    TEST_ASSERT_EQUAL_STRING("", s.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("", s.apPassword);
    TEST_ASSERT_EQUAL_STRING("cyd-hf-1a2b", s.deviceName);
    TEST_ASSERT_FALSE(s.staticIp.enabled);
}

static uint32_t counterValue = 1;
static uint32_t counter() { return counterValue++; }

// W-E8, W-S13 (S55): the web password is stored as a PBKDF2 record only, set at the device.
static void test_web_password() {
    Storage st;
    st.begin(NetConfig::NAMESPACE);
    NetSettings s;
    NetConfig::defaults(s, MAC);
    TEST_ASSERT_FALSE(NetConfig::webPasswordSet(s));
    TEST_ASSERT_FALSE(NetConfig::setWebPassword(s, "short", 5, counter)); // < 8
    TEST_ASSERT_FALSE(NetConfig::webPasswordSet(s));
    TEST_ASSERT_TRUE(NetConfig::setWebPassword(s, "bench-secret", 12, counter));
    TEST_ASSERT_TRUE(NetConfig::webPasswordSet(s));
    TEST_ASSERT_NULL(strstr(s.webPassword, "bench-secret"));
    TEST_ASSERT_TRUE(WebPassword::verify("bench-secret", 12, s.webPassword));
    TEST_ASSERT_TRUE(NetConfig::save(st, s));
    NetSettings r;
    TEST_ASSERT_TRUE(NetConfig::load(st, r, MAC));
    TEST_ASSERT_EQUAL_STRING(s.webPassword, r.webPassword);
    TEST_ASSERT_TRUE(WebPassword::verify("bench-secret", 12, r.webPassword));
    // A damaged record is dropped: no password instead of a login nobody can pass.
    st.putString("web_pw", "pbkdf2-sha1$10000$zz$00");
    TEST_ASSERT_TRUE(NetConfig::load(st, r, MAC));
    TEST_ASSERT_FALSE(NetConfig::webPasswordSet(r));
    TEST_ASSERT_FALSE(WebPassword::wellFormed("pbkdf2-sha1$10000$zz$00"));
    TEST_ASSERT_TRUE(WebPassword::wellFormed(s.webPassword));
    // Cleared: the key goes.
    NetConfig::clearWebPassword(s);
    TEST_ASSERT_FALSE(NetConfig::webPasswordSet(s));
    TEST_ASSERT_TRUE(NetConfig::save(st, s));
    char buf[96];
    TEST_ASSERT_FALSE(st.getString("web_pw", buf, sizeof(buf)));
}

// Factory reset: both network namespaces are erased, the generator's settings are left to
// SettingsService::eraseAll().
static void test_erase_all() {
    Storage st;
    st.begin(NetConfig::NAMESPACE);
    NetSettings s;
    NetConfig::defaults(s, MAC);
    NetConfig::setNetwork(s, 0, "Lab", "labpassword");
    NetConfig::save(st, s);
    Storage drv;
    drv.begin(NetConfig::DRIVER_NAMESPACE);
    drv.putU8("sta.ssid", 1);
    Storage gen;
    gen.begin("hfgen");
    gen.putU8("tune", 1);
    TEST_ASSERT_TRUE(NetConfig::eraseAll());
    TEST_ASSERT_EQUAL_size_t(0, Fake::nvs().count(NetConfig::NAMESPACE));
    TEST_ASSERT_EQUAL_size_t(0, Fake::nvs().count(NetConfig::DRIVER_NAMESPACE));
    TEST_ASSERT_EQUAL_size_t(1, Fake::nvs().count("hfgen"));
}

static void test_storage_strings() {
    Storage st;
    st.begin("t");
    char buf[8];
    TEST_ASSERT_FALSE(st.getString("k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_TRUE(st.putString("k", "1234567"));
    TEST_ASSERT_TRUE(st.getString("k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("1234567", buf);
    TEST_ASSERT_TRUE(st.putString("k", "12345678")); // one too long for buf
    TEST_ASSERT_FALSE(st.getString("k", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("", buf);
    TEST_ASSERT_TRUE(st.putString("k", "")); // removes
    TEST_ASSERT_EQUAL_size_t(0, Fake::nvs().count("t"));
    TEST_ASSERT_TRUE(st.putString("k", "")); // nothing to remove: still fine
    Fake::nvs().failWrites = true;
    TEST_ASSERT_FALSE(st.putString("k", "x"));
}

static void test_wifi_qr_text() {
    char out[128];
    TEST_ASSERT_TRUE(NetConfig::wifiQrText("CYD-HF-1A2B", "abcdefghjkmn", false, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:CYD-HF-1A2B;P:abcdefghjkmn;;", out);
    TEST_ASSERT_TRUE(NetConfig::wifiQrText("CYD-HF-1A2B", "ignored", true, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:nopass;S:CYD-HF-1A2B;;", out);
    // Special characters are escaped, so the phone reads the right password.
    TEST_ASSERT_TRUE(NetConfig::wifiQrText("a;b", "p:w,\"x\\y", false, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("WIFI:T:WPA;S:a\\;b;P:p\\:w\\,\\\"x\\\\y;;", out);
    // Too small: false and an empty text, never a cut password.
    char small[20];
    TEST_ASSERT_FALSE(NetConfig::wifiQrText("CYD-HF-1A2B", "abcdefghjkmn", false, small, sizeof(small)));
    TEST_ASSERT_EQUAL_STRING("", small);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sha1_and_hmac_vectors);
    RUN_TEST(test_pbkdf2_and_wpa_vectors);
    RUN_TEST(test_names_from_the_mac);
    RUN_TEST(test_ap_password);
    RUN_TEST(test_network_keys);
    RUN_TEST(test_static_ip_plausibility);
    RUN_TEST(test_ipv4_text);
    RUN_TEST(test_load_save_in_own_namespace);
    RUN_TEST(test_load_replaces_invalid_values);
    RUN_TEST(test_erase_all);
    RUN_TEST(test_storage_strings);
    RUN_TEST(test_wifi_qr_text);
    RUN_TEST(test_web_password);
    return UNITY_END();
}
