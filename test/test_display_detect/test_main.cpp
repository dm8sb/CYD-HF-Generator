// Display controller detection (drivers/display_detect.*) against the panel model in
// test/helpers/panel_model.h, and the NVS key for the boot gesture.

#include <unity.h>
#include <Preferences.h>
#include <drivers/display_detect.h>
#include <helpers/panel_model.h>
#include <services/settings.h>

using namespace PanelModel;
using namespace DisplayDetect;

static const uint8_t ILI9341_INIT_IDSET[] = {0x10}; // C1h 10h in the ILI9341 init = IDSET on the ST7789

void setUp() { Fake::nvs().reset(); }
void tearDown() {}

static ProbeResult probeOf(Panel& p) { return probe(p); }

// ---------------------------------------------------------------------------
// probe()

static void test_probe_sends_only_safe_commands() {
    Panel p(Kind::St7789);
    probe(p);
    TEST_ASSERT_EQUAL_HEX8(0x01, p.writes.front().cmd);
    TEST_ASSERT_TRUE(p.delayAfter.front() >= 5); // SWRESET -> next command
    for (const Write& w : p.writes) TEST_ASSERT_TRUE(w.cmd == 0x01 || w.cmd == 0x36);
    for (uint8_t c : p.reads) TEST_ASSERT_TRUE(c == 0x0B || c == 0x04); // never D9h / D3h
    TEST_ASSERT_EQUAL_HEX8(0x36, p.writes.back().cmd);
    TEST_ASSERT_EQUAL_HEX8(0x00, p.writes.back().data[0]);
}

static void test_probe_ili9341_as_measured() {
    Panel p(Kind::Ili9341);
    const ProbeResult r = probeOf(p);
    TEST_ASSERT_TRUE(r.readbackOk);
    TEST_ASSERT_EQUAL_HEX8(0xA8, r.madA[0]);
    TEST_ASSERT_EQUAL_HEX8(0x54, r.madB[0]);
    TEST_ASSERT_EQUAL_HEX32(0, r.id);
    TEST_ASSERT_EQUAL(PanelType::ILI9341, r.panel);
}

static void test_probe_cyd2usb_as_measured() {
    Panel p(Kind::St7789, ID_CYD2USB);
    const ProbeResult r = probeOf(p);
    const uint8_t raw[4] = {0x40, 0xC0, 0xD9, 0xFF}; // read on the device
    TEST_ASSERT_EQUAL_HEX8_ARRAY(raw, r.rddid, 4);
    TEST_ASSERT_EQUAL_HEX32(ID_CYD2USB, r.id);
    TEST_ASSERT_EQUAL(PanelType::ST7789, r.panel);
    TEST_ASSERT_TRUE(knownId(r.id));
}

static void test_probe_st7789v_datasheet_id() {
    Panel p(Kind::St7789, ID_ST7789V);
    const ProbeResult r = probeOf(p);
    TEST_ASSERT_EQUAL_HEX32(ID_ST7789V, r.id);
    TEST_ASSERT_EQUAL(PanelType::ST7789, r.panel);
}

static void test_probe_unknown_id_is_assumed_st7789() {
    Panel p(Kind::St7789, 0x123456);
    const ProbeResult r = probeOf(p);
    TEST_ASSERT_EQUAL(PanelType::ST7789, r.panel);
    TEST_ASSERT_FALSE(knownId(r.id));
}

static void test_probe_after_ili9341_init_on_st7789() {
    // Warm start: the previous boot sent the ILI9341 init (IDSET) to the ST7789.
    Panel p(Kind::St7789, ID_CYD2USB);
    p.write(0xC1, ILI9341_INIT_IDSET, 1);
    const ProbeResult r = probeOf(p);
    TEST_ASSERT_EQUAL_HEX32(ID_CYD2USB, r.id); // SWRESET restores ID1
    TEST_ASSERT_EQUAL(PanelType::ST7789, r.panel);
}

static void test_probe_dummy_bit_in_madctl_read() {
    Panel p(Kind::Ili9341, 0, 1);
    TEST_ASSERT_TRUE(probeOf(p).readbackOk);
    TEST_ASSERT_EQUAL(PanelType::ILI9341, probeOf(p).panel);
}

static void test_probe_without_answer_is_unknown() {
    const Kind kinds[] = {Kind::Floating, Kind::StuckLow, Kind::StuckHigh, Kind::Noisy};
    for (Kind k : kinds) {
        Panel p(k);
        const ProbeResult r = probeOf(p);
        TEST_ASSERT_EQUAL(PanelType::Unknown, r.panel);
    }
}

static void test_probe_rejects_unstable_id() {
    Panel p(Kind::St7789);
    p.unstableId = true; // MADCTL read-back passes, the two RDDID reads differ
    const ProbeResult r = probeOf(p);
    TEST_ASSERT_TRUE(r.readbackOk);
    TEST_ASSERT_EQUAL(PanelType::Unknown, r.panel);
}

static void test_probe_rejects_id_ffffff() {
    Panel p(Kind::St7789, 0xFFFFFF);
    const ProbeResult r = probeOf(p);
    TEST_ASSERT_TRUE(r.readbackOk);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, r.id);
    TEST_ASSERT_EQUAL(PanelType::Unknown, r.panel);
}

// ---------------------------------------------------------------------------
// decide()

static ProbeResult detected(PanelType t) {
    ProbeResult r {};
    r.readbackOk = t != PanelType::Unknown;
    r.panel = t;
    return r;
}

// Detection decides; without a usable answer the original CYD (ILI9341) is assumed (U-10: no
// boot gesture since 1.3.2).
static void test_decide_detected_or_default() {
    PanelDecision d = decide(detected(PanelType::ST7789), 0);
    TEST_ASSERT_EQUAL(PanelType::ST7789, d.panel);
    TEST_ASSERT_EQUAL(PanelSource::Detected, d.source);
    d = decide(detected(PanelType::ILI9341), 0);
    TEST_ASSERT_EQUAL(PanelType::ILI9341, d.panel);
    TEST_ASSERT_EQUAL(PanelSource::Detected, d.source);
    d = decide(detected(PanelType::Unknown), 0);
    TEST_ASSERT_EQUAL(PanelType::ILI9341, d.panel);
    TEST_ASSERT_EQUAL(PanelSource::Default, d.source);
}

static void test_decide_build_flag_wins() {
    PanelDecision d = decide(detected(PanelType::ILI9341), 2);
    TEST_ASSERT_EQUAL(PanelType::ST7789, d.panel);
    TEST_ASSERT_EQUAL(PanelSource::Build, d.source);
    d = decide(detected(PanelType::ST7789), 1);
    TEST_ASSERT_EQUAL(PanelType::ILI9341, d.panel);
    TEST_ASSERT_EQUAL(PanelSource::Build, d.source);
    // Invalid values detect.
    d = decide(detected(PanelType::ST7789), 3);
    TEST_ASSERT_EQUAL(PanelSource::Detected, d.source);
}

// ---------------------------------------------------------------------------
// Init sequence and tables

static void test_madctl_tables() {
    for (uint8_t r = 0; r < 4; ++r) TEST_ASSERT_EQUAL_HEX8(MADCTL_ILI9341[r] ^ 0x40, MADCTL_ST7789[r]);
    TEST_ASSERT_EQUAL_HEX8(0x68, MADCTL_ST7789[1]); // landscape as built for the CYD2USB before
}

static void test_st7789_init_table() {
    bool colmod = false, invoff = false, caset = false, raset = false;
    uint32_t sinceSlpout = 0;
    bool slpout = false;
    for (uint8_t i = 0; i < ST7789_INIT_LEN; ++i) {
        const InitCmd& c = ST7789_INIT[i];
        TEST_ASSERT_TRUE(c.cmd != 0x01 && c.cmd != 0x21 && c.cmd != 0xD3 && c.cmd != 0xD9);
        if (c.cmd == 0x3A) colmod = c.len == 1 && c.data[0] == 0x55;
        if (c.cmd == 0x20) invoff = true;
        if (c.cmd == 0x2A) caset = c.len == 4 && c.data[2] == 0x00 && c.data[3] == 0xEF;
        if (c.cmd == 0x2B) raset = c.len == 4 && c.data[2] == 0x01 && c.data[3] == 0x3F;
        if (c.cmd == 0x36) TEST_ASSERT_EQUAL_HEX8(MADCTL_ST7789[0], c.data[0]);
        if (slpout && c.cmd != 0x29) sinceSlpout += c.delayMs;
        if (c.cmd == 0x11) {
            slpout = true;
            sinceSlpout = c.delayMs;
        }
    }
    TEST_ASSERT_TRUE(colmod && invoff && caset && raset);
    TEST_ASSERT_EQUAL_HEX8(0x11, ST7789_INIT[0].cmd);
    TEST_ASSERT_EQUAL_HEX8(0x29, ST7789_INIT[ST7789_INIT_LEN - 1].cmd);
    TEST_ASSERT_TRUE(sinceSlpout >= 120); // SLPOUT -> DISPON
}

static void test_post_init_ili9341_writes_nothing() {
    Panel p(Kind::Ili9341);
    postInit(p, PanelType::ILI9341);
    TEST_ASSERT_EQUAL_UINT(0, p.writes.size());
}

static void test_post_init_st7789() {
    Panel p(Kind::St7789);
    postInit(p, PanelType::ST7789);
    TEST_ASSERT_EQUAL_UINT(1u + ST7789_INIT_LEN, p.writes.size());
    TEST_ASSERT_EQUAL_HEX8(0x01, p.writes[0].cmd);
    TEST_ASSERT_TRUE(p.delayAfter[0] >= 120); // SWRESET -> SLPOUT
    for (uint8_t i = 0; i < ST7789_INIT_LEN; ++i) TEST_ASSERT_EQUAL_HEX8(ST7789_INIT[i].cmd, p.writes[1 + i].cmd);
}

// ---------------------------------------------------------------------------
// NVS key "disp_panel" of the removed boot gesture (1.3.0-1.3.1): it overrode the detection,
// so it is deleted once at boot.

static void test_stored_display_choice_is_removed_once() {
    Preferences p;
    p.putUChar("disp_panel", 2);
    SettingsService s;
    TEST_ASSERT_TRUE(s.begin());
    TEST_ASSERT_TRUE(s.removeDisplayPanel());
    TEST_ASSERT_FALSE(p.isKey("disp_panel"));
    const unsigned writes = Fake::nvs().writeCount;
    TEST_ASSERT_FALSE(s.removeDisplayPanel()); // nothing left: no flash write
    TEST_ASSERT_EQUAL_UINT(writes, Fake::nvs().writeCount);
}

static void test_display_choice_without_nvs() {
    Fake::nvs().available = false;
    SettingsService s;
    TEST_ASSERT_FALSE(s.begin());
    TEST_ASSERT_FALSE(s.removeDisplayPanel());
}

// Audit T-15: a disturbed read gives Unknown; the retry finds the controller, a floating bus
// stays Unknown after DISPLAY_PROBE_ATTEMPTS probes.
class FlakyBus : public PanelBus {
public:
    FlakyBus(Panel& p, unsigned badReads) : p_(p), bad_(badReads) {}
    void write(uint8_t cmd, const uint8_t* data, uint8_t len) override { p_.write(cmd, data, len); }
    void read(uint8_t cmd, uint8_t* buf, uint8_t len) override {
        p_.read(cmd, buf, len);
        if (bad_ > 0) {
            --bad_;
            for (uint8_t i = 0; i < len; ++i) buf[i] = 0xFF;
        }
    }
    void delayMs(uint32_t ms) override { p_.delayMs(ms); }

private:
    Panel& p_;
    unsigned bad_;
};

static void test_probe_retries_after_a_bad_read() {
    Panel st(Kind::St7789);
    FlakyBus bus(st, 1);
    uint8_t attempts = 0;
    const ProbeResult r = probeWithRetries(bus, 3, &attempts);
    TEST_ASSERT_EQUAL(PanelType::ST7789, r.panel);
    TEST_ASSERT_EQUAL_UINT8(2, attempts);

    Panel ili(Kind::Ili9341);
    TEST_ASSERT_EQUAL(PanelType::ILI9341, probeWithRetries(ili, 3, &attempts).panel);
    TEST_ASSERT_EQUAL_UINT8(1, attempts);

    Panel floating(Kind::Floating);
    TEST_ASSERT_EQUAL(PanelType::Unknown, probeWithRetries(floating, 3, &attempts).panel);
    TEST_ASSERT_EQUAL_UINT8(3, attempts);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_probe_sends_only_safe_commands);
    RUN_TEST(test_probe_ili9341_as_measured);
    RUN_TEST(test_probe_cyd2usb_as_measured);
    RUN_TEST(test_probe_st7789v_datasheet_id);
    RUN_TEST(test_probe_unknown_id_is_assumed_st7789);
    RUN_TEST(test_probe_after_ili9341_init_on_st7789);
    RUN_TEST(test_probe_dummy_bit_in_madctl_read);
    RUN_TEST(test_probe_without_answer_is_unknown);
    RUN_TEST(test_probe_rejects_unstable_id);
    RUN_TEST(test_probe_rejects_id_ffffff);
    RUN_TEST(test_madctl_tables);
    RUN_TEST(test_st7789_init_table);
    RUN_TEST(test_post_init_ili9341_writes_nothing);
    RUN_TEST(test_post_init_st7789);
    RUN_TEST(test_decide_detected_or_default);
    RUN_TEST(test_decide_build_flag_wins);
    RUN_TEST(test_stored_display_choice_is_removed_once);
    RUN_TEST(test_display_choice_without_nvs);
    RUN_TEST(test_probe_retries_after_a_bad_read);
    return UNITY_END();
}
