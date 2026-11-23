// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// NVS limits of the Preferences fake and the upgrade from the last release (AF38, TEST-11):
// the fake rejects names and strings the real NVS rejects, and the complete key set that
// firmware 2.0.0 leaves in both namespaces ("hfgen", "hfnet") loads into the same state.

#include <unity.h>
#include <Preferences.h>
#include <string.h>
#include <string>
#include <constants.h>
#include <services/settings.h>
#include <services/storage.h>
#include <net/net_settings.h>
#include <net/request_guard.h>
#include <utils/ipv4.h>

static const uint8_t MAC[6] = {0x8C, 0x94, 0xDF, 0x93, 0x1A, 0x2B};

void setUp() { Fake::nvs().reset(); }
void tearDown() {}

static uint32_t ip(const char* t) {
    uint32_t v = 0;
    TEST_ASSERT_TRUE_MESSAGE(Ipv4::parse(t, &v), t);
    return v;
}

// Key and namespace names: at most 15 characters (NVS_KEY_NAME_MAX_SIZE 16 with the NUL).
static void test_fake_rejects_long_names() {
    Preferences p;
    TEST_ASSERT_FALSE(p.begin("sixteen-chars-ns"));
    TEST_ASSERT_EQUAL_size_t(0, p.putUChar("k", 1)); // no namespace open
    TEST_ASSERT_TRUE(p.begin("fifteen-chars-n"));
    TEST_ASSERT_EQUAL_size_t(1, p.putUChar("fifteen-chars-k", 7));
    TEST_ASSERT_EQUAL_UINT8(7, p.getUChar("fifteen-chars-k", 0));
    TEST_ASSERT_EQUAL_size_t(0, p.putUChar("sixteen-chars-ky", 7));
    TEST_ASSERT_EQUAL_size_t(0, p.putUInt("sixteen-chars-ky", 7));
    TEST_ASSERT_EQUAL_size_t(0, p.putString("sixteen-chars-ky", "x"));
    TEST_ASSERT_EQUAL_UINT8(9, p.getUChar("sixteen-chars-ky", 9));
    TEST_ASSERT_FALSE(p.isKey("sixteen-chars-ky"));
    TEST_ASSERT_FALSE(p.remove("sixteen-chars-ky"));
    TEST_ASSERT_EQUAL_size_t(0, p.putUChar("", 1));
    TEST_ASSERT_EQUAL_size_t(1, Fake::nvs().count("fifteen-chars-n"));
    // Storage passes the failure on.
    Storage st;
    TEST_ASSERT_FALSE(st.begin("sixteen-chars-ns"));
    TEST_ASSERT_TRUE(st.begin("t"));
    TEST_ASSERT_FALSE(st.putU8("sixteen-chars-ky", 1));
    TEST_ASSERT_FALSE(st.putString("sixteen-chars-ky", "x"));
}

// String values: at most 4000 bytes including the NUL.
static void test_fake_rejects_long_strings() {
    Storage st;
    TEST_ASSERT_TRUE(st.begin("t"));
    const std::string fits(3999, 'a');
    const std::string tooLong(4000, 'a');
    TEST_ASSERT_TRUE(st.putString("s", fits.c_str()));
    TEST_ASSERT_FALSE(st.putString("s", tooLong.c_str()));
    static char buf[4100];
    TEST_ASSERT_TRUE(st.getString("s", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_size_t(3999, strlen(buf)); // the old value stays
}

// Every key the firmware writes is a valid NVS name: saving all settings stores all of them.
static void test_all_written_keys_are_valid_names() {
    SettingsService settings;
    TEST_ASSERT_TRUE(settings.begin());
    PersistentState s;
    SettingsService::defaults(s);
    settings.markDirty(SettingKey::All);
    TEST_ASSERT_TRUE(settings.saveNow(s));
    TEST_ASSERT_TRUE(settings.saveTouchCalibration(TouchCalibration {500, 3900, 300, 3800}));
    TEST_ASSERT_TRUE(settings.saveRemoteOutputs(false));
    TEST_ASSERT_EQUAL_size_t(11 + 4 + 1, Fake::nvs().count(SETTINGS_NAMESPACE));

    Storage net;
    TEST_ASSERT_TRUE(net.begin(NetConfig::NAMESPACE));
    NetSettings n;
    NetConfig::defaults(n, MAC);
    NetConfig::setNetwork(n, 0, "Lab", "labpassword");
    NetConfig::setNetwork(n, 1, "Guest", "guestpassword");
    snprintf(n.apPassword, sizeof(n.apPassword), "%s", "abcdefghjkmn");
    TEST_ASSERT_TRUE(NetConfig::save(net, n));
    // mode, 2 x (ssid, key), ap_pw, ap_open, name, 5 x sip; web_pw only when set.
    TEST_ASSERT_EQUAL_size_t(1 + 4 + 3 + 5, Fake::nvs().count(NetConfig::NAMESPACE));
}

static uint32_t counterValue = 1;
static uint32_t counter() { return counterValue++; }

// The complete key set of firmware 2.0.0, written with the types and values that firmware
// writes, plus the keys of older firmware that 2.0.0 still finds (ch*_en from 1.1.0, ignored;
// disp_panel from 1.3.0-1.3.1, removed at boot).
static void writeRelease200Keys(const char* webRecord) {
    Preferences g;
    TEST_ASSERT_TRUE(g.begin("hfgen"));
    TEST_ASSERT_TRUE(g.putULong64("ch1_freq", 7074000ULL) > 0);
    TEST_ASSERT_TRUE(g.putULong64("ch2_freq", 21000000ULL) > 0);
    TEST_ASSERT_TRUE(g.putUChar("ch3_master", 2) > 0);
    TEST_ASSERT_TRUE(g.putUInt("ch3_phase", 270) > 0);
    TEST_ASSERT_TRUE(g.putUChar("backlight", 40) > 0);
    TEST_ASSERT_TRUE(g.putUInt("delta", 100) > 0);
    TEST_ASSERT_TRUE(g.putInt("corr", -1234) > 0);
    TEST_ASSERT_TRUE(g.putUChar("drv_def", 8) > 0);
    TEST_ASSERT_TRUE(g.putUChar("tune", 1) > 0);
    TEST_ASSERT_TRUE(g.putUChar("ssc", 2) > 0);
    TEST_ASSERT_TRUE(g.putUChar("off_st", 2) > 0);
    TEST_ASSERT_TRUE(g.putInt("tc_x0", 3900) > 0);
    TEST_ASSERT_TRUE(g.putInt("tc_x1", 250) > 0);
    TEST_ASSERT_TRUE(g.putInt("tc_y0", 300) > 0);
    TEST_ASSERT_TRUE(g.putInt("tc_y1", 3800) > 0);
    TEST_ASSERT_TRUE(g.putUChar("rem_out", 0) > 0);
    TEST_ASSERT_TRUE(g.putUChar("ch1_en", 1) > 0);
    TEST_ASSERT_TRUE(g.putUChar("ch2_en", 1) > 0);
    TEST_ASSERT_TRUE(g.putUChar("ch3_en", 1) > 0);
    TEST_ASSERT_TRUE(g.putUChar("disp_panel", 1) > 0);

    Preferences n;
    TEST_ASSERT_TRUE(n.begin("hfnet"));
    TEST_ASSERT_TRUE(n.putUChar("mode", 1) > 0);
    TEST_ASSERT_TRUE(n.putString("n0_ssid", "Lab 2.4") > 0);
    TEST_ASSERT_TRUE(n.putString("n0_key", "labpassword") > 0);
    TEST_ASSERT_TRUE(n.putString("n1_ssid", "Field") > 0);
    TEST_ASSERT_TRUE(n.putString("n1_key", "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff") > 0);
    TEST_ASSERT_TRUE(n.putString("ap_pw", "abcdefghjkmn") > 0);
    TEST_ASSERT_TRUE(n.putUChar("ap_open", 0) > 0);
    TEST_ASSERT_TRUE(n.putString("name", "bench-7") > 0);
    TEST_ASSERT_TRUE(n.putUChar("sip_en", 1) > 0);
    TEST_ASSERT_TRUE(n.putUInt("sip_ip", ip("192.168.4.20")) > 0);
    TEST_ASSERT_TRUE(n.putUInt("sip_mask", ip("255.255.255.0")) > 0);
    TEST_ASSERT_TRUE(n.putUInt("sip_gw", ip("192.168.4.1")) > 0);
    TEST_ASSERT_TRUE(n.putUInt("sip_dns", ip("192.168.4.2")) > 0);
    TEST_ASSERT_TRUE(n.putString("web_pw", webRecord) > 0);

    // The WiFi driver's namespace exists too (erased by the factory reset only).
    Preferences d;
    TEST_ASSERT_TRUE(d.begin("nvs.net80211"));
    TEST_ASSERT_TRUE(d.putUChar("sta.ssid", 1) > 0);
}

static void test_upgrade_from_old_keys() {
    NetSettings pw;
    NetConfig::defaults(pw, MAC);
    TEST_ASSERT_TRUE(NetConfig::setWebPassword(pw, "bench-secret", 12, counter));
    writeRelease200Keys(pw.webPassword);

    // Boot order of main.cpp: settings, network settings, touch calibration, display choice.
    SettingsService settings;
    TEST_ASSERT_TRUE(settings.begin());
    PersistentState s;
    settings.load(s);
    TEST_ASSERT_EQUAL_UINT64(7074000ULL, s.ch1.frequency);
    TEST_ASSERT_EQUAL_UINT64(21000000ULL, s.ch2.frequency);
    TEST_ASSERT_FALSE(s.ch1.enabled); // outputs always start off, ch*_en is ignored
    TEST_ASSERT_FALSE(s.ch2.enabled);
    TEST_ASSERT_FALSE(s.ch3.enabled);
    TEST_ASSERT_EQUAL_UINT8(2, s.ch3.master);
    TEST_ASSERT_EQUAL_UINT32(270, s.ch3.phase);
    TEST_ASSERT_EQUAL_UINT8(40, s.sys.backlight);
    TEST_ASSERT_EQUAL_UINT32(100, s.sys.delta_step);
    TEST_ASSERT_EQUAL_INT32(-1234, s.sys.freq_correction);
    TEST_ASSERT_EQUAL_UINT8(8, s.sys.drive_ma);
    TEST_ASSERT_EQUAL(TuningMode::LowNoise, s.sys.tuning_mode);
    TEST_ASSERT_EQUAL(Spread::Down15, s.sys.spread);
    TEST_ASSERT_EQUAL(OffState::HighZ, s.sys.off_state);
    TEST_ASSERT_FALSE(settings.loadRemoteOutputs());
    TouchCalibration cal {};
    TEST_ASSERT_TRUE(settings.loadTouchCalibration(&cal));
    TEST_ASSERT_EQUAL_INT32(3900, cal.x_min);
    TEST_ASSERT_EQUAL_INT32(250, cal.x_max);
    TEST_ASSERT_EQUAL_INT32(300, cal.y_min);
    TEST_ASSERT_EQUAL_INT32(3800, cal.y_max);
    TEST_ASSERT_TRUE(settings.removeDisplayPanel());
    TEST_ASSERT_FALSE(settings.removeDisplayPanel());

    Storage netStorage;
    TEST_ASSERT_TRUE(netStorage.begin(NetConfig::NAMESPACE));
    NetSettings n;
    TEST_ASSERT_TRUE(NetConfig::load(netStorage, n, MAC));
    TEST_ASSERT_EQUAL(WifiMode::Client, n.mode);
    TEST_ASSERT_EQUAL_STRING("Lab 2.4", n.networks[0].ssid);
    TEST_ASSERT_EQUAL_STRING("labpassword", n.networks[0].key);
    TEST_ASSERT_EQUAL_STRING("Field", n.networks[1].ssid);
    TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", n.networks[1].key);
    TEST_ASSERT_EQUAL_STRING("abcdefghjkmn", n.apPassword);
    TEST_ASSERT_FALSE(n.apOpen);
    TEST_ASSERT_EQUAL_STRING("bench-7", n.deviceName);
    TEST_ASSERT_TRUE(n.staticIp.enabled);
    TEST_ASSERT_EQUAL_HEX32(ip("192.168.4.20"), n.staticIp.ip);
    TEST_ASSERT_EQUAL_HEX32(ip("255.255.255.0"), n.staticIp.mask);
    TEST_ASSERT_EQUAL_HEX32(ip("192.168.4.1"), n.staticIp.gateway);
    TEST_ASSERT_EQUAL_HEX32(ip("192.168.4.2"), n.staticIp.dns);
    TEST_ASSERT_TRUE(WebPassword::verify("bench-secret", 12, n.webPassword));

    // Saving again rewrites the same keys: nothing new appears, nothing old is lost.
    const size_t genKeys = Fake::nvs().count("hfgen");
    const size_t netKeys = Fake::nvs().count("hfnet");
    settings.markDirty(SettingKey::All);
    TEST_ASSERT_TRUE(settings.saveNow(s));
    TEST_ASSERT_TRUE(NetConfig::save(netStorage, n));
    TEST_ASSERT_EQUAL_size_t(genKeys, Fake::nvs().count("hfgen"));
    TEST_ASSERT_EQUAL_size_t(netKeys, Fake::nvs().count("hfnet"));
    PersistentState again;
    settings.load(again);
    TEST_ASSERT_EQUAL_UINT64(s.ch1.frequency, again.ch1.frequency);
    TEST_ASSERT_EQUAL_INT32(s.sys.freq_correction, again.sys.freq_correction);
    TEST_ASSERT_EQUAL(s.sys.spread, again.sys.spread);
    TEST_ASSERT_EQUAL(s.sys.off_state, again.sys.off_state);
    TEST_ASSERT_EQUAL_size_t(1, Fake::nvs().count("nvs.net80211"));
}

// AF35 (TEST-4, M13): the autosave runs exactly quietMs after the last change, the retry
// exactly SETTINGS_RETRY_MS after a failure.
static void test_autosave_boundaries() {
    SettingsService settings;
    TEST_ASSERT_TRUE(settings.begin());
    PersistentState s;
    SettingsService::defaults(s);
    Fake::clockMs() = 1000;
    settings.markDirty(SettingKey::Ch1Freq);
    Fake::clockMs() = 1000 + SETTINGS_AUTOSAVE_MS - 1;
    TEST_ASSERT_EQUAL(SaveResult::None, settings.update(s));
    TEST_ASSERT_TRUE(settings.isDirty());
    Fake::clockMs() = 1000 + SETTINGS_AUTOSAVE_MS;
    TEST_ASSERT_EQUAL(SaveResult::Saved, settings.update(s));
    TEST_ASSERT_FALSE(settings.isDirty());

    // Remote changes wait longer.
    settings.markDirty(SettingKey::Delta, SETTINGS_REMOTE_AUTOSAVE_MS);
    const uint32_t t0 = Fake::clockMs();
    Fake::clockMs() = t0 + SETTINGS_REMOTE_AUTOSAVE_MS - 1;
    TEST_ASSERT_EQUAL(SaveResult::None, settings.update(s));
    Fake::clockMs() = t0 + SETTINGS_REMOTE_AUTOSAVE_MS;
    TEST_ASSERT_EQUAL(SaveResult::Saved, settings.update(s));

    // A failed save is reported once and retried after SETTINGS_RETRY_MS.
    settings.markDirty(SettingKey::Corr);
    Fake::nvs().failWrites = true;
    const uint32_t t1 = Fake::clockMs();
    Fake::clockMs() = t1 + SETTINGS_AUTOSAVE_MS;
    TEST_ASSERT_EQUAL(SaveResult::Failed, settings.update(s));
    TEST_ASSERT_TRUE(settings.lastSaveFailed());
    const uint32_t t2 = Fake::clockMs();
    Fake::clockMs() = t2 + SETTINGS_RETRY_MS - 1;
    TEST_ASSERT_EQUAL(SaveResult::None, settings.update(s));
    Fake::clockMs() = t2 + SETTINGS_RETRY_MS;
    TEST_ASSERT_EQUAL(SaveResult::None, settings.update(s)); // failed again: not reported twice
    Fake::nvs().failWrites = false;
    Fake::clockMs() += SETTINGS_RETRY_MS;
    TEST_ASSERT_EQUAL(SaveResult::Saved, settings.update(s));
    TEST_ASSERT_FALSE(settings.lastSaveFailed());
    Fake::clockMs() = 0;
}

// AF35 (TEST-10): NVS write errors on every write path of SettingsService.
static void test_settings_write_errors() {
    SettingsService settings;
    TEST_ASSERT_TRUE(settings.begin());
    PersistentState s;
    SettingsService::defaults(s);
    s.ch1.frequency = 7100000;
    settings.markDirty(SettingKey::Ch1Freq);
    Fake::nvs().failWrites = true;
    // SYSTem:SAVE fails and leaves the keys to the autosave.
    TEST_ASSERT_FALSE(settings.saveNow(s));
    TEST_ASSERT_TRUE(settings.isDirty());
    TEST_ASSERT_FALSE(settings.saveTouchCalibration(TouchCalibration {500, 3900, 300, 3800}));
    TEST_ASSERT_FALSE(settings.saveRemoteOutputs(false));
    TEST_ASSERT_EQUAL_size_t(0, Fake::nvs().count(SETTINGS_NAMESPACE));
    Fake::nvs().failWrites = false;
    TEST_ASSERT_TRUE(settings.loadRemoteOutputs()); // nothing stored: allowed
    Fake::clockMs() += SETTINGS_RETRY_MS;
    TEST_ASSERT_EQUAL(SaveResult::Saved, settings.update(s));
    PersistentState r;
    settings.load(r);
    TEST_ASSERT_EQUAL_UINT64(7100000, r.ch1.frequency);
    // A failing factory reset keeps the stored keys.
    Fake::nvs().failWrites = true;
    TEST_ASSERT_FALSE(settings.eraseAll());
    Fake::nvs().failWrites = false;
    TEST_ASSERT_EQUAL_size_t(1, Fake::nvs().count(SETTINGS_NAMESPACE));
    // removeDisplayPanel(): only an existing key counts, a failing delete is reported.
    TEST_ASSERT_FALSE(settings.removeDisplayPanel());
    Preferences p;
    TEST_ASSERT_TRUE(p.begin(SETTINGS_NAMESPACE));
    p.putUChar("disp_panel", 1);
    Fake::nvs().failWrites = true;
    TEST_ASSERT_FALSE(settings.removeDisplayPanel());
    Fake::nvs().failWrites = false;
    TEST_ASSERT_TRUE(settings.removeDisplayPanel());
    // Without NVS nothing is written and the defaults apply.
    Fake::nvs().available = false;
    SettingsService none;
    TEST_ASSERT_FALSE(none.begin());
    TEST_ASSERT_FALSE(none.saveNow(s));
    TEST_ASSERT_FALSE(none.saveRemoteOutputs(true));
    TEST_ASSERT_TRUE(none.loadRemoteOutputs());
    TEST_ASSERT_FALSE(none.eraseAll());
    TEST_ASSERT_FALSE(none.removeDisplayPanel());
    none.markDirty();
    Fake::clockMs() += SETTINGS_AUTOSAVE_MS;
    TEST_ASSERT_EQUAL(SaveResult::None, none.update(s));
    TEST_ASSERT_FALSE(none.isDirty());
    Fake::clockMs() = 0;
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fake_rejects_long_names);
    RUN_TEST(test_fake_rejects_long_strings);
    RUN_TEST(test_all_written_keys_are_valid_names);
    RUN_TEST(test_upgrade_from_old_keys);
    RUN_TEST(test_autosave_boundaries);
    RUN_TEST(test_settings_write_errors);
    return UNITY_END();
}
