// Executor, command queue and state snapshot (V-6): network tasks never call the controller,
// LVGL, I2C or NVS directly; they queue commands (results by ticket) and read a snapshot.

#include <unity.h>
#include <Preferences.h>
#include <Wire.h>
#include <constants.h>
#include <core/app_controller.h>
#include <hal/i2c_bus.h>
#include <remote/command.h>

static Fake::Si5351Chip& chip = Fake::si5351();

struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;
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
};

static Source lastSource;
static void onEvent(const Event& e, void*) { lastSource = e.source; }

void setUp() {
    chip.reset();
    Fake::nvs().reset();
    Fake::clockMs() = 0;
    lastSource = Source::Touch;
}
void tearDown() {}

static Command cmd(CommandType t, uint8_t ch, int64_t v, Source s = Source::Remote) {
    Command c {};
    c.type = t;
    c.channel = ch;
    c.value = v;
    c.source = s;
    return c;
}

static void test_execute_maps_to_the_setters() {
    Device d;
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::SetFrequency, 1, 7100000)));
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL(Result::OutOfRange, execute(d.app, cmd(CommandType::SetFrequency, 1, 200000000LL)));
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::SetEnabled, 1, 1)));
    TEST_ASSERT_TRUE(d.app.channel(1).enabled);
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::Ch3Phase, 3, 270)));
    TEST_ASSERT_EQUAL(Result::Invalid, execute(d.app, cmd(CommandType::Ch3Phase, 3, 45)));
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::Drive, 0, 8)));
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::Tuning, 0, (int64_t)TuningMode::Integer)));
    TEST_ASSERT_EQUAL(Result::Conflict, execute(d.app, cmd(CommandType::Spread, 0, (int64_t)Spread::Down05)));
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::Correction, 0, -250)));
    TEST_ASSERT_EQUAL_INT32(-250, d.app.system().freq_correction);
    // Values outside the type range are rejected in the full type (W-S19), never cut.
    TEST_ASSERT_EQUAL(Result::Invalid, execute(d.app, cmd(CommandType::Drive, 0, 258)));
    TEST_ASSERT_EQUAL(Result::Invalid, execute(d.app, cmd(CommandType::Ch3Phase, 3, 65626)));
    TEST_ASSERT_EQUAL(Result::OutOfRange, execute(d.app, cmd(CommandType::Backlight, 0, -1)));
    TEST_ASSERT_EQUAL(Result::Invalid, execute(d.app, cmd((CommandType)99, 0, 0)));
}

static void test_execute_sets_the_source_for_events() {
    Device d;
    d.app.addListener(onEvent, nullptr);
    d.app.setChannelEnabled(1, true);
    chip.failAfter = 0;
    execute(d.app, cmd(CommandType::SetFrequency, 1, 7200000, Source::Remote));
    chip.failAfter = -1;
    TEST_ASSERT_EQUAL(Source::Remote, lastSource);
    // Afterwards the controller is back at Touch: a touch error opens a dialog again.
    for (uint32_t t = 0; t < HW_CHECK_INTERVAL_MS + 100; t += 50) {
        Fake::advanceMs(50);
        d.app.update(); // reconnects
    }
    TEST_ASSERT_TRUE(d.app.hardwareReady());
    chip.failAfter = 0;
    d.app.setChannelFrequency(1, 7300000);
    chip.failAfter = -1;
    TEST_ASSERT_EQUAL(Source::Touch, lastSource);
}

static void test_queue_order_limit_and_results() {
    Device d;
    CommandQueue q;
    uint32_t t1 = 0, t2 = 0;
    TEST_ASSERT_TRUE(q.push(cmd(CommandType::SetFrequency, 1, 7100000), &t1));
    TEST_ASSERT_TRUE(q.push(cmd(CommandType::SetFrequency, 1, 1), &t2)); // out of range
    TEST_ASSERT_TRUE(t1 != t2);
    Result r = Result::Ok;
    TEST_ASSERT_FALSE(q.result(t1, &r)); // not executed yet
    TEST_ASSERT_EQUAL_UINT8(2, q.drain(d.app));
    TEST_ASSERT_TRUE(q.result(t1, &r));
    TEST_ASSERT_EQUAL(Result::Ok, r);
    TEST_ASSERT_TRUE(q.result(t2, &r));
    TEST_ASSERT_EQUAL(Result::OutOfRange, r);
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    // Bounded: a full queue refuses instead of blocking the network task.
    for (uint8_t i = 0; i < CommandQueue::CAPACITY; ++i) TEST_ASSERT_TRUE(q.push(cmd(CommandType::Drive, 0, 8)));
    TEST_ASSERT_FALSE(q.push(cmd(CommandType::Drive, 0, 8)));
    q.drain(d.app);
    TEST_ASSERT_TRUE(q.push(cmd(CommandType::Drive, 0, 8)));
}

static void test_snapshot_publish_and_read() {
    Device d;
    StateSnapshot snap;
    Snapshot s {};
    TEST_ASSERT_FALSE(snap.read(&s)); // nothing published yet
    d.app.setChannelFrequency(2, 21000000);
    d.app.setChannelEnabled(2, true);
    snap.publish(d.app);
    TEST_ASSERT_TRUE(snap.read(&s));
    TEST_ASSERT_EQUAL_UINT64(21000000, s.state.ch2.frequency);
    TEST_ASSERT_TRUE(s.state.ch2.enabled);
    TEST_ASSERT_EQUAL(ChannelStatus::On, s.status[1]);
    TEST_ASSERT_EQUAL_UINT32(d.app.revision(), s.revision);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 21000000.0, s.actualHz[1]);
    TEST_ASSERT_TRUE(s.hardwareReady);
    // A reader that meets a write in progress gets no torn copy.
    snap.beginWriteForTest();
    TEST_ASSERT_FALSE(snap.read(&s));
    snap.endWriteForTest();
    TEST_ASSERT_TRUE(snap.read(&s));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_execute_maps_to_the_setters);
    RUN_TEST(test_execute_sets_the_source_for_events);
    RUN_TEST(test_queue_order_limit_and_results);
    RUN_TEST(test_snapshot_publish_and_read);
    return UNITY_END();
}
