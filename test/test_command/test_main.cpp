// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

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

static Command cmd(CommandType t, uint8_t ch, int64_t v, Source s = Source::Scpi) {
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
    execute(d.app, cmd(CommandType::SetFrequency, 1, 7200000, Source::Scpi));
    chip.failAfter = -1;
    TEST_ASSERT_EQUAL(Source::Scpi, lastSource);
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
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::SetFrequency, 1, 7100000), &t1));
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::SetFrequency, 1, 1), &t2)); // out of range
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
    // Each source has its own rate limit; the queue is shared.
    for (uint8_t i = 0; i < CommandQueue::CAPACITY; ++i) {
        const Source s = (i % 2) ? Source::Web : Source::Scpi;
        TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::Drive, 0, 8, s)));
    }
    TEST_ASSERT_EQUAL(PushResult::Full, q.push(cmd(CommandType::Drive, 0, 8, Source::Usb)));
    q.drain(d.app);
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::Drive, 0, 8, Source::Usb)));
}

// W-S11: at most REMOTE_WRITES_PER_S write commands per second and source.
static void test_rate_limit_per_source() {
    Device d;
    CommandQueue q;
    for (uint8_t i = 0; i < REMOTE_WRITES_PER_S; ++i) {
        TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::Step, 0, 1000)));
        q.drain(d.app); // never full
    }
    TEST_ASSERT_EQUAL(PushResult::RateLimited, q.push(cmd(CommandType::Step, 0, 1000)));
    // Another source is not affected.
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::Step, 0, 1000, Source::Web)));
    // One token back every 50 ms.
    Fake::advanceMs(1000 / REMOTE_WRITES_PER_S - 1);
    TEST_ASSERT_EQUAL(PushResult::RateLimited, q.push(cmd(CommandType::Step, 0, 1000)));
    Fake::advanceMs(1);
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::Step, 0, 1000)));
    TEST_ASSERT_EQUAL(PushResult::RateLimited, q.push(cmd(CommandType::Step, 0, 1000)));
    q.drain(d.app);
    // After a quiet second the full burst is available again, not more.
    Fake::advanceMs(5000);
    uint8_t queued = 0;
    for (uint8_t i = 0; i < 2 * REMOTE_WRITES_PER_S; ++i) {
        if (q.push(cmd(CommandType::Step, 0, 1000)) == PushResult::Queued) ++queued;
        q.drain(d.app);
    }
    TEST_ASSERT_EQUAL_UINT8(REMOTE_WRITES_PER_S, queued);
}

// W-E25 / W-S21: local only lets the network set everything and switch off, but not on.
static void test_remote_outputs_local_only() {
    {
        Device d;
        TEST_ASSERT_TRUE(d.app.remoteOutputsAllowed()); // default
        TEST_ASSERT_EQUAL(Result::Ok, d.app.setRemoteOutputsAllowed(false));
        TEST_ASSERT_EQUAL(Result::NoChange, d.app.setRemoteOutputsAllowed(false));
        for (uint8_t ch = 1; ch <= 3; ++ch) {
            TEST_ASSERT_EQUAL(Result::Denied, execute(d.app, cmd(CommandType::SetEnabled, ch, 1)));
            TEST_ASSERT_EQUAL(Result::Denied, execute(d.app, cmd(CommandType::SetEnabled, ch, 1, Source::Web)));
        }
        TEST_ASSERT_FALSE(d.app.channel(1).enabled);
        TEST_ASSERT_FALSE(d.app.ch3().enabled);
        TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::SetFrequency, 1, 7100000)));
        // D22: off state High (DC on the switched-off outputs) is refused like switching on;
        // Low and High-Z stay allowed.
        TEST_ASSERT_EQUAL(Result::Denied, execute(d.app, cmd(CommandType::OffState, 0, (int64_t)OffState::High)));
        TEST_ASSERT_EQUAL(Result::Denied,
                          execute(d.app, cmd(CommandType::OffState, 0, (int64_t)OffState::High, Source::Web)));
        TEST_ASSERT_EQUAL(OffState::Low, d.app.system().off_state);
        TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::OffState, 0, (int64_t)OffState::HighZ)));
        TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::OffState, 0, (int64_t)OffState::Low)));
        // Touch switches on, the network may switch off.
        TEST_ASSERT_EQUAL(Result::Ok, d.app.setChannelEnabled(1, true));
        TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::SetEnabled, 1, 0, Source::Usb)));
        TEST_ASSERT_FALSE(d.app.channel(1).enabled);
        // Only the touch source may change the option.
        d.app.setSource(Source::Web);
        TEST_ASSERT_EQUAL(Result::Denied, d.app.setRemoteOutputsAllowed(true));
        d.app.setSource(Source::Touch);
        // Neither *RST nor Defaults change it; the snapshot shows it.
        execute(d.app, cmd(CommandType::ResetRemote, 0, 0));
        d.app.resetToDefaults();
        TEST_ASSERT_FALSE(d.app.remoteOutputsAllowed());
        StateSnapshot snap;
        Snapshot s {};
        snap.publish(d.app);
        TEST_ASSERT_TRUE(snap.read(&s));
        TEST_ASSERT_FALSE(s.remoteOutputsAllowed);
    }
    // Saved at once: still local only after a restart.
    Device d;
    TEST_ASSERT_FALSE(d.app.remoteOutputsAllowed());
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setRemoteOutputsAllowed(true));
    TEST_ASSERT_EQUAL(Result::Ok, execute(d.app, cmd(CommandType::SetEnabled, 2, 1)));
    // A factory reset brings back the default.
    d.app.setRemoteOutputsAllowed(false);
    d.settings.eraseAll();
    TEST_ASSERT_TRUE(d.settings.loadRemoteOutputs());
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

// S55: a REST body is one batch: all or nothing, executed in order, stopped at the first error.
static void test_batch_is_atomic_and_stops_at_the_first_error() {
    Device d;
    CommandQueue q;
    Command list[3] = {cmd(CommandType::SetFrequency, 1, 1, Source::Web), cmd(CommandType::SetEnabled, 1, 1, Source::Web),
                       cmd(CommandType::Drive, 0, 8, Source::Web)};
    uint32_t first = 0;
    TEST_ASSERT_EQUAL(PushResult::Queued, q.pushBatch(list, 3, &first));
    TEST_ASSERT_EQUAL_UINT8(3, q.drain(d.app));
    Result r = Result::Ok;
    TEST_ASSERT_TRUE(q.result(first, &r));
    TEST_ASSERT_EQUAL(Result::OutOfRange, r);
    TEST_ASSERT_TRUE(q.result(first + 1, &r));
    TEST_ASSERT_EQUAL(Result::OutOfRange, r); // skipped with the first error
    TEST_ASSERT_TRUE(q.result(first + 2, &r));
    TEST_ASSERT_EQUAL(Result::OutOfRange, r);
    TEST_ASSERT_FALSE(d.app.channel(1).enabled); // not switched on after the refused frequency
    TEST_ASSERT_EQUAL_UINT8(4, d.app.system().drive_ma);
    // NoChange is no error: the next command runs.
    list[0] = cmd(CommandType::SetFrequency, 1, (int64_t)d.app.channel(1).frequency, Source::Web);
    TEST_ASSERT_EQUAL(PushResult::Queued, q.pushBatch(list, 2, &first));
    q.drain(d.app);
    TEST_ASSERT_TRUE(q.result(first, &r));
    TEST_ASSERT_EQUAL(Result::NoChange, r);
    TEST_ASSERT_TRUE(d.app.channel(1).enabled);
    // A single push after a failed batch is not skipped.
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::SetFrequency, 2, 1, Source::Web)));
    uint32_t t = 0;
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::Drive, 0, 2, Source::Web), &t));
    q.drain(d.app);
    TEST_ASSERT_TRUE(q.result(t, &r));
    TEST_ASSERT_EQUAL(Result::Ok, r);
    // Not enough room or rate tokens: nothing is queued.
    Fake::advanceMs(5000);
    CommandQueue full;
    for (uint8_t i = 0; i < CommandQueue::CAPACITY - 1; ++i) full.push(cmd(CommandType::Step, 0, 1000, (i % 2) ? Source::Usb : Source::Scpi));
    TEST_ASSERT_EQUAL(PushResult::Full, full.pushBatch(list, 2));
    TEST_ASSERT_EQUAL(PushResult::Queued, full.pushBatch(list, 1));
    CommandQueue rate;
    for (uint8_t i = 0; i < REMOTE_WRITES_PER_S - 1; ++i) {
        rate.push(cmd(CommandType::Step, 0, 1000, Source::Web));
        rate.drain(d.app);
    }
    TEST_ASSERT_EQUAL(PushResult::RateLimited, rate.pushBatch(list, 2));
    TEST_ASSERT_EQUAL(PushResult::Queued, rate.pushBatch(list, 1)); // the last token was kept
}

// S55: a waiting request answers once its command is in the published snapshot.
static void test_visible_after_publish() {
    Device d;
    CommandQueue q;
    StateSnapshot snap;
    uint32_t t = 0;
    TEST_ASSERT_FALSE(q.visible(0));
    TEST_ASSERT_EQUAL(PushResult::Queued, q.push(cmd(CommandType::SetFrequency, 2, 3500000, Source::Web), &t));
    TEST_ASSERT_FALSE(q.visible(t));
    q.drain(d.app);
    TEST_ASSERT_FALSE(q.visible(t)); // executed, not yet published
    snap.publish(d.app);
    q.markVisible();
    TEST_ASSERT_TRUE(q.visible(t));
    Snapshot s {};
    TEST_ASSERT_TRUE(snap.read(&s));
    TEST_ASSERT_EQUAL_UINT64(3500000, s.state.ch2.frequency);
    uint32_t t2 = 0;
    q.push(cmd(CommandType::Drive, 0, 6, Source::Web), &t2);
    TEST_ASSERT_TRUE(q.visible(t));
    TEST_ASSERT_FALSE(q.visible(t2));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_execute_maps_to_the_setters);
    RUN_TEST(test_execute_sets_the_source_for_events);
    RUN_TEST(test_queue_order_limit_and_results);
    RUN_TEST(test_rate_limit_per_source);
    RUN_TEST(test_remote_outputs_local_only);
    RUN_TEST(test_snapshot_publish_and_read);
    RUN_TEST(test_batch_is_atomic_and_stops_at_the_first_error);
    RUN_TEST(test_visible_after_publish);
    return UNITY_END();
}
