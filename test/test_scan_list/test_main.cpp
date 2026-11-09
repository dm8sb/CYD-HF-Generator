// ScanList (ARBEITSPLAN 3.4, W-S9): hidden networks dropped, one entry per SSID, sorted by
// signal, at most 15, the shown SSID sanitized.

#include <unity.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <net/scan_list.h>

void setUp() {}
void tearDown() {}

static void add(ScanList& l, const char* ssid, int8_t rssi, bool secure = true) { l.add(ssid, strlen(ssid), rssi, secure); }

static void test_sorted_by_signal_and_deduplicated() {
    ScanList l;
    add(l, "Lab", -70);
    add(l, "Guest", -50, false);
    add(l, "Lab", -60); // stronger access point of the same network
    add(l, "Lab", -80); // weaker: ignored
    add(l, "Office", -90);
    l.add("", 0, -40, true); // hidden
    TEST_ASSERT_EQUAL_UINT8(3, l.count());
    TEST_ASSERT_EQUAL_STRING("Guest", l.at(0).ssid);
    TEST_ASSERT_FALSE(l.at(0).secure);
    TEST_ASSERT_EQUAL_STRING("Lab", l.at(1).ssid);
    TEST_ASSERT_EQUAL_INT8(-60, l.at(1).rssi);
    TEST_ASSERT_EQUAL_STRING("Office", l.at(2).ssid);
}

static void test_at_most_15_strongest() {
    ScanList l;
    for (int i = 0; i < 40; ++i) {
        char name[8];
        snprintf(name, sizeof(name), "n%d", i);
        add(l, name, (int8_t)(-90 + (i * 7) % 50));
    }
    TEST_ASSERT_EQUAL_UINT8(ScanList::MAX, l.count());
    for (uint8_t i = 1; i < l.count(); ++i) TEST_ASSERT_TRUE(l.at(i - 1).rssi >= l.at(i).rssi);
    // A weaker one than all kept is not added.
    add(l, "weak", -100);
    for (uint8_t i = 0; i < l.count(); ++i) TEST_ASSERT_TRUE(strcmp(l.at(i).ssid, "weak") != 0);
}

static void test_shown_ssid_is_sanitized_raw_kept() {
    ScanList l;
    const std::string raw = std::string("x\ny\xC3", 4);
    l.add(raw.data(), raw.size(), -50, true);
    TEST_ASSERT_EQUAL_STRING("x?y?", l.at(0).shown);
    TEST_ASSERT_EQUAL_MEMORY(raw.data(), l.at(0).ssid, raw.size());
    // Longer than 32 bytes: cut to 32.
    const std::string longName(40, 'a');
    l.add(longName.data(), longName.size(), -40, true);
    TEST_ASSERT_EQUAL_size_t(32, strlen(l.at(0).ssid));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_sorted_by_signal_and_deduplicated);
    RUN_TEST(test_at_most_15_strongest);
    RUN_TEST(test_shown_ssid_is_sanitized_raw_kept);
    return UNITY_END();
}
