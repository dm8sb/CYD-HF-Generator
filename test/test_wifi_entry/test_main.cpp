// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// WIFI pages without LVGL (AF47, CQ-4, TEST-12): text entry per purpose (limits, title,
// counter line, what OK does), the status line per WiFi state and the pages of the scan list.

#include <unity.h>
#include <net/net_settings.h>
#include <net/request_guard.h>
#include <utils/input_logic.h>
#include <utils/status_text.h>
#include <string.h>
#include <initializer_list>
#include <string>

using namespace InputLogic;

void setUp() {}
void tearDown() {}

// Sizes of the GUI buffers the texts go into (gui.h, page_wifi.cpp).
static constexpr size_t ERROR_SIZE = 24;  // Gui::entryError_
static constexpr size_t TITLE_SIZE = 48;
static constexpr size_t INFO_SIZE = 48;
static constexpr size_t STATUS_SIZE = 128;

static const EntryPurpose ALL[] = {EntryPurpose::Ssid,       EntryPurpose::Password, EntryPurpose::ApPassword,
                                   EntryPurpose::DeviceName, EntryPurpose::IpField,  EntryPurpose::WebPassword};

static std::string repeat(char c, size_t n) { return std::string(n, c); }

static void test_entry_spec_per_purpose() {
    // Limits as the counter shows them: SSID and name 32, PMK 64, AP passphrase 63, IP 15.
    TEST_ASSERT_EQUAL_UINT8(32, entrySpec(EntryPurpose::Ssid).maxLen);
    TEST_ASSERT_EQUAL_UINT8(64, entrySpec(EntryPurpose::Password).maxLen);
    TEST_ASSERT_EQUAL_UINT8(63, entrySpec(EntryPurpose::ApPassword).maxLen);
    TEST_ASSERT_EQUAL_UINT8(32, entrySpec(EntryPurpose::DeviceName).maxLen);
    TEST_ASSERT_EQUAL_UINT8(15, entrySpec(EntryPurpose::IpField).maxLen);
    TEST_ASSERT_EQUAL_UINT8(WebPassword::MAX_LEN, entrySpec(EntryPurpose::WebPassword).maxLen);
    // Passwords get Show/Hide; the AP password is typed in clear (the AP page shows it).
    TEST_ASSERT_TRUE(entrySpec(EntryPurpose::Password).secret && entrySpec(EntryPurpose::Password).masked);
    TEST_ASSERT_TRUE(entrySpec(EntryPurpose::WebPassword).secret && entrySpec(EntryPurpose::WebPassword).masked);
    TEST_ASSERT_TRUE(entrySpec(EntryPurpose::ApPassword).secret);
    TEST_ASSERT_FALSE(entrySpec(EntryPurpose::ApPassword).masked);
    for (EntryPurpose p : {EntryPurpose::Ssid, EntryPurpose::DeviceName, EntryPurpose::IpField}) {
        TEST_ASSERT_FALSE(entrySpec(p).secret);
        TEST_ASSERT_FALSE(entrySpec(p).masked);
        TEST_ASSERT_TRUE(entrySpec(p).prefill);
    }
    for (EntryPurpose p : {EntryPurpose::Password, EntryPurpose::ApPassword, EntryPurpose::WebPassword}) {
        TEST_ASSERT_FALSE(entrySpec(p).prefill); // a stored key is never shown again
    }
    TEST_ASSERT_TRUE(entrySpec(EntryPurpose::IpField).numeric);
    TEST_ASSERT_EQUAL_STRING("0123456789.", entrySpec(EntryPurpose::IpField).accepted);
    TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrstuvwxyz0123456789-", entrySpec(EntryPurpose::DeviceName).accepted);
    TEST_ASSERT_NULL(entrySpec(EntryPurpose::Ssid).accepted);
    TEST_ASSERT_NULL(entrySpec(EntryPurpose::Password).accepted);
    // Every accepted text fits a field of maxLen: the validators allow no more.
    TEST_ASSERT_TRUE(NetConfig::validSsid(repeat('a', 32).c_str()));
    TEST_ASSERT_TRUE(NetConfig::validNetworkKey(repeat('a', 64).c_str()));
    TEST_ASSERT_TRUE(NetConfig::validPassphrase(repeat('a', 63).c_str()));
    TEST_ASSERT_TRUE(NetConfig::validDeviceName(repeat('a', 32).c_str()));
}

static void test_entry_titles() {
    char t[TITLE_SIZE];
    entryTitle(EntryPurpose::Ssid, 0, "", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Network name", t);
    entryTitle(EntryPurpose::DeviceName, 0, "x", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Device name", t);
    entryTitle(EntryPurpose::ApPassword, 0, "x", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("AP password", t);
    entryTitle(EntryPurpose::WebPassword, 0, "x", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Web password", t);
    static const char* const FIELDS[] = {"Address", "Mask", "Gateway", "DNS", "Address"};
    for (uint8_t f = 0; f < 5; ++f) {
        entryTitle(EntryPurpose::IpField, f, "", t, sizeof(t));
        TEST_ASSERT_EQUAL_STRING(FIELDS[f], t); // field & 3
    }
    entryTitle(EntryPurpose::Password, 0, "Lab", t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING("Password Lab", t);
    // A foreign SSID is sanitized (W-S9) and the longest one fits the title buffer unshortened.
    entryTitle(EntryPurpose::Password, 0, "a\x01" "b", t, sizeof(t));
    TEST_ASSERT_NULL(strchr(t, '\x01'));
    const std::string longest = repeat('W', 32);
    entryTitle(EntryPurpose::Password, 0, longest.c_str(), t, sizeof(t));
    TEST_ASSERT_EQUAL_STRING(("Password " + longest).c_str(), t);
}

static void test_entry_info_line() {
    char b[INFO_SIZE];
    entryInfo(EntryPurpose::Ssid, 0, "", false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0/32", b);
    entryInfo(EntryPurpose::Password, 64, "", false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("64/64", b);
    entryInfo(EntryPurpose::IpField, 7, nullptr, false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("7/15", b);
    // The error wins until the text changes.
    entryInfo(EntryPurpose::ApPassword, 3, "8-63 characters", false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("8-63 characters", b);
    // Empty web password while one is stored: OK removes it (after a confirmation).
    entryInfo(EntryPurpose::WebPassword, 0, "", true, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("empty + OK: remove", b);
    entryInfo(EntryPurpose::WebPassword, 0, "", false, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0/64", b);
    entryInfo(EntryPurpose::WebPassword, 1, "", true, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("1/64", b);
    entryInfo(EntryPurpose::Password, 0, "", true, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("0/64", b); // only the web password can be removed this way
}

static void expectInvalid(EntryPurpose p, const char* text, const char* error) {
    const EntryResult r = submitEntry(p, text, false);
    TEST_ASSERT_EQUAL(EntryStep::Invalid, r.step);
    TEST_ASSERT_EQUAL_STRING(error, r.error);
    TEST_ASSERT_TRUE(strlen(r.error) < ERROR_SIZE);
}

static void test_submit_device_name() {
    TEST_ASSERT_EQUAL(EntryStep::StoreName, submitEntry(EntryPurpose::DeviceName, "cyd-hf-1234", false).step);
    expectInvalid(EntryPurpose::DeviceName, "", "a-z 0-9 - (1-32)");
    expectInvalid(EntryPurpose::DeviceName, repeat('a', 33).c_str(), "a-z 0-9 - (1-32)");
    expectInvalid(EntryPurpose::DeviceName, "-lab", "a-z 0-9 - (1-32)");
}

static void test_submit_ip_field() {
    const EntryResult r = submitEntry(EntryPurpose::IpField, "192.168.1.50", false);
    TEST_ASSERT_EQUAL(EntryStep::StoreIp, r.step);
    TEST_ASSERT_EQUAL_HEX32(0xC0A80132u, r.ip);
    expectInvalid(EntryPurpose::IpField, "", "e.g. 192.168.1.50");
    expectInvalid(EntryPurpose::IpField, "192.168.1", "e.g. 192.168.1.50");
    expectInvalid(EntryPurpose::IpField, "192.168.1.256", "e.g. 192.168.1.50");
}

static void test_submit_web_password() {
    // Empty: remove (with confirmation) only while one is stored.
    TEST_ASSERT_EQUAL(EntryStep::ConfirmRemove, submitEntry(EntryPurpose::WebPassword, "", true).step);
    expectInvalid(EntryPurpose::WebPassword, "", "8-64 characters");
    expectInvalid(EntryPurpose::WebPassword, "short", "8-64 characters");
    expectInvalid(EntryPurpose::WebPassword, repeat('a', 65).c_str(), "8-64 characters");
    TEST_ASSERT_EQUAL(EntryStep::HashWebPassword, submitEntry(EntryPurpose::WebPassword, "12345678", false).step);
    TEST_ASSERT_EQUAL(EntryStep::HashWebPassword, submitEntry(EntryPurpose::WebPassword, "12345678", true).step);
    TEST_ASSERT_EQUAL(EntryStep::HashWebPassword,
                      submitEntry(EntryPurpose::WebPassword, repeat('a', 64).c_str(), true).step);
}

static void test_submit_ap_password() {
    TEST_ASSERT_EQUAL(EntryStep::StoreApPassword, submitEntry(EntryPurpose::ApPassword, "Demo-Pass-26", false).step);
    expectInvalid(EntryPurpose::ApPassword, "", "8-63 characters"); // the AP has an Open button instead
    expectInvalid(EntryPurpose::ApPassword, "1234567", "8-63 characters");
    expectInvalid(EntryPurpose::ApPassword, repeat('a', 64).c_str(), "8-63 characters"); // no PMK for the AP
}

static void test_submit_ssid_then_password() {
    // Hidden network: the name first, then the password entry follows.
    TEST_ASSERT_EQUAL(EntryStep::AskPassword, submitEntry(EntryPurpose::Ssid, "Hidden", false).step);
    TEST_ASSERT_EQUAL(EntryStep::AskPassword, submitEntry(EntryPurpose::Ssid, repeat('s', 32).c_str(), false).step);
    expectInvalid(EntryPurpose::Ssid, "", "1-32 characters");
    expectInvalid(EntryPurpose::Ssid, repeat('s', 33).c_str(), "1-32 characters");
}

static void test_submit_network_key() {
    TEST_ASSERT_EQUAL(EntryStep::StoreNetwork, submitEntry(EntryPurpose::Password, "sample-key", false).step);
    // A hidden open network: empty password.
    TEST_ASSERT_EQUAL(EntryStep::StoreNetwork, submitEntry(EntryPurpose::Password, "", false).step);
    // 64 hex digits are a PMK (D31), 64 other characters are not.
    TEST_ASSERT_EQUAL(EntryStep::StoreNetwork, submitEntry(EntryPurpose::Password, repeat('a', 64).c_str(), false).step);
    expectInvalid(EntryPurpose::Password, repeat('z', 64).c_str(), "8-63 chars or 64 hex");
    expectInvalid(EntryPurpose::Password, "1234567", "8-63 chars or 64 hex");
    // Errors of the steps the page carries out fit the error buffer as well.
    TEST_ASSERT_TRUE(strlen(ENTRY_BUSY) < ERROR_SIZE);
    TEST_ASSERT_TRUE(strlen(ENTRY_NOT_ACCEPTED) < ERROR_SIZE);
    // No purpose returns a step of another one.
    for (EntryPurpose p : ALL) {
        const EntryResult r = submitEntry(p, "", false);
        TEST_ASSERT_TRUE(r.step != EntryStep::ConfirmRemove);
    }
}

static StatusText::WifiLine line() {
    StatusText::WifiLine w;
    memset(&w, 0, sizeof(w));
    w.mode = WifiMode::Client;
    w.ssid = "Lab";
    w.staIp = 0xC0A80132u;
    w.deviceName = "cyd-hf-1234";
    w.apSsid = "CYD-HF-1234";
    w.failure = WifiPolicy::Failure::None;
    w.networkStored = true;
    return w;
}

static void expectLine(const StatusText::WifiLine& w, const char* text, StatusText::Tone tone) {
    char b[STATUS_SIZE];
    TEST_ASSERT_EQUAL((int)tone, (int)StatusText::wifiLine(w, b, sizeof(b)));
    TEST_ASSERT_EQUAL_STRING(text, b);
}

static void test_wifi_line_per_state() {
    using StatusText::Tone;
    StatusText::WifiLine w = line();
    expectLine(w, "Searching...", Tone::Muted);
    w.networkStored = false;
    expectLine(w, "No network stored. Tap Change.", Tone::Muted);
    w = line();
    w.failure = WifiPolicy::Failure::Other;
    expectLine(w, "Connection failed", Tone::Warn);
    w.failure = WifiPolicy::Failure::NotFound;
    expectLine(w, "Network not found", Tone::Warn);
    w.failure = WifiPolicy::Failure::WrongPassword;
    expectLine(w, "Wrong password", Tone::Error);
    w.connecting = true; // a new attempt hides the old failure
    expectLine(w, "Connecting...", Tone::Muted);
    w.apActive = true;
    expectLine(w, "Fallback: AP CYD-HF-1234 active\nStill searching", Tone::Warn);
    w.mode = WifiMode::AccessPoint;
    expectLine(w, "Access point CYD-HF-1234 active", Tone::Muted);
    w.staConnected = true; // connected wins over the AP lines
    w.mode = WifiMode::Client;
    expectLine(w, "Connected Lab\n192.168.1.50, cyd-hf-1234.local", Tone::Ok);
    w.mode = WifiMode::Off;
    expectLine(w, "WiFi is Off. Choose Client in CONFIG > WiFi.", Tone::Muted);
    w.pendingS = 87; // a pending web change wins over everything
    expectLine(w, "Web change pending (87 s)\nConfirm in the browser", Tone::Warn);
}

static void test_wifi_line_fits() {
    // Longest inputs: 32-byte foreign SSID (sanitized), longest address and device name. The
    // text is never cut and keeps at most one line break (two lines plus wrapping on the page).
    StatusText::WifiLine w = line();
    w.staConnected = true;
    const std::string ssid = repeat('S', 32);
    const std::string name = repeat('n', 32);
    w.ssid = ssid.c_str();
    w.deviceName = name.c_str();
    w.staIp = 0xFFFFFFFFu;
    char b[STATUS_SIZE];
    StatusText::wifiLine(w, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING(("Connected " + ssid + "\n255.255.255.255, " + name + ".local").c_str(), b);
    w.ssid = "\x1b[2J\x7f";
    StatusText::wifiLine(w, b, sizeof(b));
    for (const char* c = b; *c; ++c) TEST_ASSERT_TRUE(*c == '\n' || (unsigned char)*c >= 0x20);
    w = line();
    const std::string ap = repeat('A', 32);
    w.apSsid = ap.c_str();
    w.apActive = true;
    StatusText::wifiLine(w, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING(("Fallback: AP " + ap + " active\nStill searching").c_str(), b);
    w.pendingS = 65535;
    StatusText::wifiLine(w, b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("Web change pending (65535 s)\nConfirm in the browser", b);
}

static void test_scan_pages() {
    TEST_ASSERT_EQUAL_UINT8(0, scanPages(0, 4));
    TEST_ASSERT_EQUAL_UINT8(1, scanPages(1, 4));
    TEST_ASSERT_EQUAL_UINT8(1, scanPages(4, 4));
    TEST_ASSERT_EQUAL_UINT8(2, scanPages(5, 4));
    TEST_ASSERT_EQUAL_UINT8(5, scanPages(20, 4));
    // A shorter list moves the page to its last one; an empty list shows page 0.
    TEST_ASSERT_EQUAL_UINT8(1, clampScanPage(3, 5, 4));
    TEST_ASSERT_EQUAL_UINT8(0, clampScanPage(3, 0, 4));
    TEST_ASSERT_EQUAL_UINT8(2, clampScanPage(2, 12, 4));
    // Back stops at the first page, forward at the last one.
    TEST_ASSERT_EQUAL_UINT8(0, stepScanPage(0, false, 9, 4));
    TEST_ASSERT_EQUAL_UINT8(1, stepScanPage(2, false, 9, 4));
    TEST_ASSERT_EQUAL_UINT8(1, stepScanPage(0, true, 9, 4));
    TEST_ASSERT_EQUAL_UINT8(2, stepScanPage(1, true, 9, 4));
    TEST_ASSERT_EQUAL_UINT8(2, stepScanPage(2, true, 9, 4));
    TEST_ASSERT_EQUAL_UINT8(0, stepScanPage(0, true, 4, 4));
    TEST_ASSERT_EQUAL_UINT8(0, stepScanPage(0, true, 0, 4));
    // Rows below the end of the list are empty.
    uint8_t idx = 0xFF;
    TEST_ASSERT_TRUE(scanIndex(1, 2, 9, 4, &idx));
    TEST_ASSERT_EQUAL_UINT8(6, idx);
    TEST_ASSERT_TRUE(scanIndex(2, 0, 9, 4, &idx));
    TEST_ASSERT_EQUAL_UINT8(8, idx);
    TEST_ASSERT_FALSE(scanIndex(2, 1, 9, 4, &idx));
    TEST_ASSERT_FALSE(scanIndex(0, 0, 0, 4, &idx));
    TEST_ASSERT_FALSE(scanIndex(255, 3, 255, 4, &idx)); // no wrap-around in 8 bits
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_entry_spec_per_purpose);
    RUN_TEST(test_entry_titles);
    RUN_TEST(test_entry_info_line);
    RUN_TEST(test_submit_device_name);
    RUN_TEST(test_submit_ip_field);
    RUN_TEST(test_submit_web_password);
    RUN_TEST(test_submit_ap_password);
    RUN_TEST(test_submit_ssid_then_password);
    RUN_TEST(test_submit_network_key);
    RUN_TEST(test_wifi_line_per_state);
    RUN_TEST(test_wifi_line_fits);
    RUN_TEST(test_scan_pages);
    return UNITY_END();
}
