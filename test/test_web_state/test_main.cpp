// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// State machines of the web server (S55): two-stage network change with fallback (W-S12),
// scan semantics (ARBEITSPLAN 3.6 point 6, W-S11), one password check at a time outside the
// async_tcp task (W-S23), and the byte and connection limits (W-S6, W-S10).

#include <unity.h>
#include <string.h>
#include <string>
#include <net/net_settings.h>
#include <net/request_guard.h>
#include <net/web_limits.h>
#include <net/web_state.h>

static const uint8_t MAC[6] = {0x8C, 0x94, 0xDF, 0x93, 0x1A, 0x2B};

void setUp() {}
void tearDown() {}

static NetSettings settings(WifiMode m, const char* name) {
    NetSettings s;
    NetConfig::defaults(s, MAC);
    s.mode = m;
    snprintf(s.deviceName, sizeof(s.deviceName), "%s", name);
    return s;
}

// --- NetChange -----------------------------------------------------------------------------

static void test_change_applies_after_the_answer_and_reverts_without_confirmation() {
    NetChange c;
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::AccessPoint, "new");
    TEST_ASSERT_EQUAL(NetChange::Start::Started, c.request(a, b, false, false, 1000));
    TEST_ASSERT_TRUE(c.busy());
    TEST_ASSERT_EQUAL(NetChange::Start::Busy, c.request(a, b, false, false, 1001)); // one at a time
    TEST_ASSERT_EQUAL_UINT16(121, c.confirmWithinS(1000));                         // 120 s + the delay
    TEST_ASSERT_EQUAL(NetChange::Confirm::NothingPending, c.confirmWeb());           // not applied yet
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(1000 + NetChange::APPLY_DELAY_MS - 1));
    TEST_ASSERT_EQUAL(NetChange::Action::Apply, c.update(1000 + NetChange::APPLY_DELAY_MS));
    TEST_ASSERT_EQUAL_STRING("new", c.next().deviceName);
    const uint32_t applied = 1000 + NetChange::APPLY_DELAY_MS;
    TEST_ASSERT_EQUAL_UINT16(120, c.confirmWithinS(applied));
    TEST_ASSERT_EQUAL_UINT16(1, c.confirmWithinS(applied + NetChange::CONFIRM_MS - 1));
    TEST_ASSERT_FALSE(c.waitingForTouch()); // web password set
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(applied + NetChange::CONFIRM_MS - 1));
    TEST_ASSERT_EQUAL(NetChange::Action::Revert, c.update(applied + NetChange::CONFIRM_MS));
    TEST_ASSERT_EQUAL_STRING("old", c.previous().deviceName); // readable for the action
    TEST_ASSERT_EQUAL_UINT16(0, c.confirmWithinS(applied + NetChange::CONFIRM_MS));
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(applied + NetChange::CONFIRM_MS + 5));
    TEST_ASSERT_FALSE(c.busy());
    TEST_ASSERT_EQUAL_STRING("", c.previous().deviceName); // wiped
    TEST_ASSERT_EQUAL(NetChange::Confirm::NothingPending, c.confirmWeb());
}

static void test_change_confirmed_over_the_web() {
    NetChange c;
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::Client, "new");
    c.request(a, b, false, false, 0);
    TEST_ASSERT_EQUAL(NetChange::Action::Apply, c.update(NetChange::APPLY_DELAY_MS));
    TEST_ASSERT_EQUAL(NetChange::Confirm::Committed, c.confirmWeb());
    TEST_ASSERT_EQUAL_UINT16(0, c.confirmWithinS(1000));
    TEST_ASSERT_EQUAL(NetChange::Confirm::NothingPending, c.confirmWeb());
    TEST_ASSERT_EQUAL(NetChange::Action::Save, c.update(1000));
    TEST_ASSERT_EQUAL_STRING("new", c.next().deviceName);
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(1001));
    TEST_ASSERT_FALSE(c.busy());
    // Long after the deadline nothing reverts any more.
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(NetChange::CONFIRM_MS * 2));
}

// Without a web password the touch screen confirms as well (W-S12), in either order.
static void test_change_without_password_needs_the_touch() {
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::Client, "new");
    {
        NetChange c;
        c.request(a, b, true, false, 0);
        TEST_ASSERT_FALSE(c.waitingForTouch()); // not applied yet: nothing to ask
        c.update(NetChange::APPLY_DELAY_MS);
        TEST_ASSERT_TRUE(c.waitingForTouch());
        TEST_ASSERT_EQUAL(NetChange::Confirm::NeedsTouch, c.confirmWeb());
        TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(2000));
        TEST_ASSERT_TRUE(c.confirmWithinS(2000) > 0);
        c.confirmTouch(true);
        TEST_ASSERT_FALSE(c.waitingForTouch());
        TEST_ASSERT_EQUAL(NetChange::Action::Save, c.update(3000));
    }
    {
        NetChange c;
        c.request(a, b, true, false, 0);
        c.update(NetChange::APPLY_DELAY_MS);
        c.confirmTouch(true);
        TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(2000)); // the web confirmation is still missing
        TEST_ASSERT_EQUAL(NetChange::Confirm::Committed, c.confirmWeb());
        TEST_ASSERT_EQUAL(NetChange::Action::Save, c.update(3000));
    }
    {
        // Touch only: still reverts at the deadline.
        NetChange c;
        c.request(a, b, true, false, 0);
        c.update(NetChange::APPLY_DELAY_MS);
        c.confirmTouch(true);
        TEST_ASSERT_EQUAL(NetChange::Action::Revert, c.update(NetChange::APPLY_DELAY_MS + NetChange::CONFIRM_MS));
    }
    {
        // Refused at the touch: back at once.
        NetChange c;
        c.request(a, b, true, false, 0);
        c.update(NetChange::APPLY_DELAY_MS);
        c.confirmTouch(false);
        TEST_ASSERT_EQUAL_UINT16(0, c.confirmWithinS(1000));
        TEST_ASSERT_EQUAL(NetChange::Confirm::NothingPending, c.confirmWeb());
        TEST_ASSERT_EQUAL(NetChange::Action::Revert, c.update(1000));
    }
    {
        // With a password the touch need not confirm, Keep changes nothing, but Revert works
        // (AF16, concept 2: the device shows a hint with Revert).
        NetChange c;
        c.request(a, b, false, false, 0);
        c.update(NetChange::APPLY_DELAY_MS);
        c.confirmTouch(true);
        TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(1000));
        TEST_ASSERT_TRUE(c.busy());
        c.confirmTouch(false);
        TEST_ASSERT_EQUAL(NetChange::Action::Revert, c.update(1100));
    }
}

// --- AF16: one owner of the network settings (concept 2) ----------------------------------

// While a web change is open the touch screen may not change the network settings ("Web change
// pending"); afterwards it may again.
static void test_touch_change_refused_while_pending() {
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::Client, "new");
    NetChange c;
    TEST_ASSERT_TRUE(c.acceptsTouchChange());
    c.request(a, b, false, false, 0);
    TEST_ASSERT_FALSE(c.acceptsTouchChange()); // scheduled
    TEST_ASSERT_EQUAL_UINT16(121, c.pendingS(0));
    c.update(NetChange::APPLY_DELAY_MS);
    TEST_ASSERT_FALSE(c.acceptsTouchChange()); // waiting for the confirmation
    TEST_ASSERT_EQUAL(NetChange::Confirm::Committed, c.confirmWeb());
    TEST_ASSERT_FALSE(c.acceptsTouchChange()); // until the save is done
    TEST_ASSERT_TRUE(c.pendingS(1000) >= 1);
    TEST_ASSERT_EQUAL(NetChange::Action::Save, c.update(1000));
    c.update(1001);
    TEST_ASSERT_TRUE(c.acceptsTouchChange());
    TEST_ASSERT_EQUAL_UINT16(0, c.pendingS(1001));
    // After a timeout as well.
    c.request(a, b, false, false, 2000);
    c.update(2000 + NetChange::APPLY_DELAY_MS);
    TEST_ASSERT_EQUAL(NetChange::Action::Revert, c.update(2000 + NetChange::APPLY_DELAY_MS + NetChange::CONFIRM_MS));
    c.update(2000 + NetChange::APPLY_DELAY_MS + NetChange::CONFIRM_MS + 1);
    TEST_ASSERT_TRUE(c.acceptsTouchChange());
}

// Revert at the touch before the change was even applied: nothing is applied, nothing saved.
static void test_touch_revert_before_apply() {
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::Client, "new");
    NetChange c;
    c.request(a, b, false, false, 0);
    c.confirmTouch(false);
    TEST_ASSERT_FALSE(c.busy());
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(NetChange::APPLY_DELAY_MS));
    TEST_ASSERT_TRUE(c.acceptsTouchChange());
}

// The NVS model: only a confirmed state is ever saved (W-S12). A power loss at any moment of
// the window finds the previous settings; the manager's own saves (AP password) wait.
static void test_power_loss_finds_the_previous_settings() {
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::AccessPoint, "new");
    for (int ending = 0; ending < 3; ++ending) { // 0 timeout, 1 touch revert, 2 web confirm
        NetSettings nvs = a;
        NetChange c;
        c.request(a, b, false, false, 0);
        uint32_t t = 0;
        bool sawApply = false;
        while (t <= NetChange::APPLY_DELAY_MS + NetChange::CONFIRM_MS + 300) {
            if (t == 5000) {
                TEST_ASSERT_FALSE(c.managerMaySave()); // an AP password created now is not saved yet
                if (ending == 1) c.confirmTouch(false);
                if (ending == 2) TEST_ASSERT_EQUAL(NetChange::Confirm::Committed, c.confirmWeb());
            }
            const NetChange::Action act = c.update(t);
            if (act == NetChange::Action::Apply) sawApply = true;
            if (NetChange::saves(act)) nvs = c.next();
            // "Power loss" here: NVS holds the previous settings until the confirmation.
            if (ending != 2 || t < 5000) TEST_ASSERT_EQUAL_STRING("old", nvs.deviceName);
            t += 100;
        }
        TEST_ASSERT_TRUE(sawApply);
        TEST_ASSERT_TRUE(c.managerMaySave());
        TEST_ASSERT_EQUAL_STRING(ending == 2 ? "new" : "old", nvs.deviceName);
    }
    TEST_ASSERT_FALSE(NetChange::saves(NetChange::Action::Revert)); // NVS still has prev
    TEST_ASSERT_FALSE(NetChange::saves(NetChange::Action::Apply));
    TEST_ASSERT_TRUE(NetChange::saves(NetChange::Action::ApplyAndSave));
}

// Mode Off over the web (ARBEITSPLAN 3.6 point 2): final, applied and saved, nothing to confirm.
static void test_final_change() {
    NetChange c;
    const NetSettings a = settings(WifiMode::Client, "x"), b = settings(WifiMode::Off, "x");
    c.request(a, b, true, true, 0);
    TEST_ASSERT_EQUAL_UINT16(0, c.confirmWithinS(0));
    TEST_ASSERT_FALSE(c.waitingForTouch());
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(10));
    TEST_ASSERT_EQUAL(NetChange::Action::ApplyAndSave, c.update(NetChange::APPLY_DELAY_MS));
    TEST_ASSERT_EQUAL(WifiMode::Off, c.next().mode);
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(NetChange::APPLY_DELAY_MS + 1));
    TEST_ASSERT_FALSE(c.busy());
}

// The millisecond counter wraps after 49.7 days.
static void test_change_across_the_millis_wrap() {
    NetChange c;
    const NetSettings a = settings(WifiMode::Client, "old"), b = settings(WifiMode::Client, "new");
    const uint32_t t0 = 0xFFFFFF00u;
    c.request(a, b, false, false, t0);
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(t0 + 100));
    TEST_ASSERT_EQUAL(NetChange::Action::Apply, c.update(t0 + NetChange::APPLY_DELAY_MS));
    TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(t0 + NetChange::APPLY_DELAY_MS + 1000));
    TEST_ASSERT_EQUAL_UINT16(119, c.confirmWithinS(t0 + NetChange::APPLY_DELAY_MS + 1000));
    TEST_ASSERT_EQUAL(NetChange::Action::Revert, c.update(t0 + NetChange::APPLY_DELAY_MS + NetChange::CONFIRM_MS));
}

// --- ScanSession ---------------------------------------------------------------------------

static void test_scan_semantics() {
    typedef ScanSession::Radio R;
    typedef ScanSession::Answer A;
    ScanSession s;
    TEST_ASSERT_EQUAL(A::Start, s.get(R::Idle, 1000));
    TEST_ASSERT_EQUAL(A::Running, s.get(R::Running, 1500));
    TEST_ASSERT_EQUAL(A::Running, s.get(R::Running, 2500));
    TEST_ASSERT_EQUAL(A::Done, s.get(R::Done, 3000));      // the first GET after the end
    TEST_ASSERT_EQUAL(A::TooSoon, s.get(R::Done, 4000));   // a new start within 10 s
    TEST_ASSERT_EQUAL(A::TooSoon, s.get(R::Done, 10999));
    TEST_ASSERT_EQUAL(A::Start, s.get(R::Done, 11000));
    TEST_ASSERT_EQUAL(A::Failed, s.get(R::Failed, 12000));
    // A scan of the touch screen: running, then its result once the web asked during it.
    TEST_ASSERT_EQUAL(A::Running, s.get(R::Running, 30000));
    TEST_ASSERT_EQUAL(A::Start, s.get(R::Done, 31000)); // not waiting: a new scan
    TEST_ASSERT_EQUAL(A::Done, s.get(R::Done, 32000));
    // Started but never ran: failed instead of hanging in "running".
    TEST_ASSERT_EQUAL(A::Start, s.get(R::Idle, 50000));
    TEST_ASSERT_EQUAL(A::Failed, s.get(R::Idle, 50100));
}

// --- LoginQueue ----------------------------------------------------------------------------

static bool fakeVerify(const char* pw, size_t len, const char* record) {
    return len == strlen(record) && memcmp(pw, record, len) == 0;
}

static void test_login_queue() {
    LoginQueue q;
    TEST_ASSERT_TRUE(q.idle());
    const uint32_t id = q.submit("secret12", 8, "secret12");
    TEST_ASSERT_TRUE(id != 0);
    TEST_ASSERT_EQUAL_UINT32(0, q.submit("other123", 8, "x")); // one at a time
    bool ok = false;
    TEST_ASSERT_FALSE(q.collect(id, &ok)); // not run yet
    TEST_ASSERT_TRUE(q.runOne(fakeVerify));
    TEST_ASSERT_FALSE(q.runOne(fakeVerify)); // nothing queued
    TEST_ASSERT_FALSE(q.collect(id + 1, &ok));
    TEST_ASSERT_TRUE(q.collect(id, &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(q.idle());
    const uint32_t id2 = q.submit("wrongpw1", 8, "secret12");
    q.runOne(fakeVerify);
    TEST_ASSERT_TRUE(q.collect(id2, &ok));
    TEST_ASSERT_FALSE(ok);
    // Cancelled while queued: dropped; while running: the late result is discarded.
    const uint32_t id3 = q.submit("secret12", 8, "secret12");
    q.cancel(id3);
    TEST_ASSERT_TRUE(q.idle());
    TEST_ASSERT_FALSE(q.runOne(fakeVerify));
    const uint32_t id4 = q.submit("secret12", 8, "secret12");
    LoginQueue::Job j;
    TEST_ASSERT_TRUE(q.take(&j));
    TEST_ASSERT_EQUAL_STRING("secret12", j.password);
    TEST_ASSERT_EQUAL_UINT32(0, q.submit("x2345678", 8, "y")); // still busy while running
    q.cancel(id4);
    q.finish(id4, true);
    TEST_ASSERT_TRUE(q.idle());
    TEST_ASSERT_FALSE(q.collect(id4, &ok));
    LoginQueue::wipe(&j);
    TEST_ASSERT_EQUAL_STRING("", j.password);
    // The worker path of the device: check() wipes the job but hands back its id, so
    // finish() still finds it (device test 2026-11-09: wiping first left it running forever).
    const uint32_t idChk = q.submit("secret12", 8, "secret12");
    TEST_ASSERT_TRUE(q.take(&j));
    bool right = false;
    TEST_ASSERT_EQUAL_UINT32(idChk, LoginQueue::check(&j, fakeVerify, &right));
    TEST_ASSERT_TRUE(right);
    TEST_ASSERT_EQUAL_STRING("", j.password);
    TEST_ASSERT_EQUAL_UINT32(0, j.id);
    q.finish(idChk, right);
    TEST_ASSERT_TRUE(q.collect(idChk, &ok));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(q.idle());
    TEST_ASSERT_EQUAL_UINT32(0, LoginQueue::check(nullptr, fakeVerify, &right));
    // Too long or no record: refused.
    char longPw[80];
    memset(longPw, 'a', sizeof(longPw));
    TEST_ASSERT_EQUAL_UINT32(0, q.submit(longPw, sizeof(longPw), "x"));
    TEST_ASSERT_EQUAL_UINT32(0, q.submit("secret12", 8, nullptr));
    // With the real PBKDF2 record.
    char record[WebPassword::RECORD_SIZE];
    TEST_ASSERT_TRUE(WebPassword::hash("bench-secret", 12, [] { return 0x12345678u; }, record));
    const uint32_t id5 = q.submit("bench-secret", 12, record);
    q.runOne(WebPassword::verify);
    TEST_ASSERT_TRUE(q.collect(id5, &ok));
    TEST_ASSERT_TRUE(ok);
}

// --- WebPasswordJob ------------------------------------------------------------------------

static void test_web_password_job() {
    WebPasswordJob j;
    const uint8_t salt[WebPassword::SALT_LEN] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    char record[WebPassword::RECORD_SIZE] = "x";
    TEST_ASSERT_FALSE(j.busy());
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Idle, j.collect(record));
    // Invalid passwords never reach the worker: too short, too long, control characters.
    TEST_ASSERT_FALSE(j.submit("short", 5, salt));
    char longPw[WebPassword::MAX_LEN + 1];
    memset(longPw, 'a', sizeof(longPw));
    TEST_ASSERT_FALSE(j.submit(longPw, sizeof(longPw), salt));
    TEST_ASSERT_TRUE(j.submit(longPw, WebPassword::MAX_LEN, salt)); // 64 is the maximum
    TEST_ASSERT_TRUE(j.runOne());
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Done, j.collect(record));
    TEST_ASSERT_FALSE(j.submit("bad\npass", 8, salt));
    TEST_ASSERT_FALSE(j.submit("secret12", 8, nullptr));
    TEST_ASSERT_FALSE(j.busy());

    TEST_ASSERT_TRUE(j.submit("secret12", 8, salt));
    TEST_ASSERT_TRUE(j.busy());
    TEST_ASSERT_FALSE(j.submit("other123", 8, salt)); // one at a time
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Busy, j.collect(record));
    // The worker path of the device: take() wipes the queued copy, hash() wipes the worker's.
    WebPasswordJob::Job w;
    TEST_ASSERT_TRUE(j.take(&w));
    TEST_ASSERT_FALSE(j.take(&w)); // nothing queued any more
    TEST_ASSERT_EQUAL_STRING("secret12", w.password);
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Busy, j.collect(record)); // running
    char out[WebPassword::RECORD_SIZE];
    TEST_ASSERT_TRUE(WebPasswordJob::hash(&w, out));
    TEST_ASSERT_EQUAL_STRING("", w.password);
    TEST_ASSERT_EQUAL_UINT32(0, w.len);
    j.finish(out, true);
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Done, j.collect(record));
    TEST_ASSERT_FALSE(j.busy());
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Idle, j.collect(record)); // collected once
    // The record is the stored form with this salt and verifies the password.
    TEST_ASSERT_TRUE(WebPassword::wellFormed(record));
    TEST_ASSERT_NOT_NULL(strstr(record, "$0102030405060708090a0b0c0d0e0f10$"));
    TEST_ASSERT_TRUE(WebPassword::verify("secret12", 8, record));
    TEST_ASSERT_FALSE(WebPassword::verify("secret13", 8, record));
    // Same as hash() with the same salt bytes from the random source.
    char viaHash[WebPassword::RECORD_SIZE];
    TEST_ASSERT_TRUE(WebPassword::hash("secret12", 8, [] { return 0x04030201u; }, viaHash));
    char viaSalt[WebPassword::RECORD_SIZE];
    const uint8_t salt2[WebPassword::SALT_LEN] = {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4};
    TEST_ASSERT_TRUE(WebPassword::hashWithSalt("secret12", 8, salt2, viaSalt));
    TEST_ASSERT_EQUAL_STRING(viaHash, viaSalt);

    // A failed or malformed result frees the job without a record.
    TEST_ASSERT_TRUE(j.submit("secret12", 8, salt));
    TEST_ASSERT_TRUE(j.take(&w));
    j.finish("not-a-record", true);
    record[0] = 'x';
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Failed, j.collect(record));
    TEST_ASSERT_EQUAL_CHAR('x', record[0]);
    TEST_ASSERT_TRUE(j.submit("secret12", 8, salt));
    TEST_ASSERT_TRUE(j.take(&w));
    j.finish("", false);
    TEST_ASSERT_EQUAL(WebPasswordJob::Poll::Failed, j.collect(record));
    j.finish("x", true); // no job running: ignored
    TEST_ASSERT_FALSE(j.busy());
    TEST_ASSERT_FALSE(WebPasswordJob::hash(nullptr, out));
    TEST_ASSERT_EQUAL_STRING("", out);
    WebPasswordJob::wipe(&w);
}

// --- Limits --------------------------------------------------------------------------------

// AF18 (SEC-V-2, PERF-18): unsolicited PONGs do not keep a dead client alive, so its slot is
// freed (displaced) after PONG_TIMEOUT_MS; a PING flood closes the client (the web UI never
// pings; every PING queues a PONG in the library's unbounded control queue).
static void test_ws_unsolicited_pongs_do_not_hold_a_slot() {
    WsClients w;
    TEST_ASSERT_TRUE(w.add(1, 0));
    TEST_ASSERT_TRUE(w.add(2, 0));
    TEST_ASSERT_TRUE(w.add(3, 0));
    TEST_ASSERT_FALSE(w.add(4, 0)); // full
    // Client 3 sends a PONG every second on its own and never answers a PING.
    const uint8_t junk[4] = {'c', 'y', 'd', 0};
    uint8_t answer[3][4] {};
    uint32_t t = 0;
    for (; t <= WsClients::PONG_TIMEOUT_MS + 1000; t += 1000) {
        w.pong(3, t, junk, sizeof(junk)); // unsolicited: not the payload of our PING
        uint32_t id = 0;
        uint8_t payload[WsClients::PING_PAYLOAD_LEN];
        while ((id = w.duePing(t, 0x5A000000u + t + id, payload)) != 0) {
            if (id != 3) memcpy(answer[id - 1], payload, sizeof(payload));
        }
        w.pong(1, t, answer[0], 4); // 1 and 2 answer (repeating an answer changes nothing)
        w.pong(2, t, answer[1], 4);
        if (w.expired(t) == 3) break;
    }
    TEST_ASSERT_TRUE(t <= WsClients::PONG_TIMEOUT_MS + 1000);
    TEST_ASSERT_EQUAL_UINT8(2, w.count());
    TEST_ASSERT_TRUE(w.add(4, t)); // the freed slot takes a new client
    TEST_ASSERT_EQUAL_UINT32(0, w.expired(t));
}

static void test_ws_ping_flood_closes() {
    WsClients w;
    w.add(7, 0);
    for (uint8_t i = 0; i < WsClients::PING_BUDGET; ++i) TEST_ASSERT_TRUE(w.clientPing(7, 100 * i));
    TEST_ASSERT_FALSE(w.clientPing(7, 1000)); // one more within the window
    // Slow PINGs (a proxy keeping the link) stay allowed.
    WsClients s;
    s.add(8, 0);
    for (uint32_t t = 0; t < 10 * WsClients::PING_WINDOW_MS; t += WsClients::PING_WINDOW_MS / 2) {
        TEST_ASSERT_TRUE(s.clientPing(8, t));
    }
    TEST_ASSERT_FALSE(s.clientPing(99, 0)); // unknown client
}

// AF19 (PERF-6): WebSocket pushes at most every 100 ms; the first change goes at once, changes
// in between are merged and the latest state follows at the end of the interval.
static void test_push_pacer() {
    PushPacer p;
    TEST_ASSERT_FALSE(p.due(false, 0));      // nothing changed
    TEST_ASSERT_TRUE(p.due(true, 1000));     // first change at once
    TEST_ASSERT_FALSE(p.due(true, 1010));    // within 100 ms: merged
    TEST_ASSERT_FALSE(p.due(true, 1050));
    TEST_ASSERT_FALSE(p.due(false, 1099));
    TEST_ASSERT_TRUE(p.due(false, 1100));    // the latest state at the end of the interval
    TEST_ASSERT_FALSE(p.due(false, 1200));   // nothing left
    TEST_ASSERT_TRUE(p.due(true, 1500));     // quiet before: at once again
    // A flood of 80 changes/s gives at most 10 pushes/s.
    int pushes = 0;
    for (uint32_t t = 2000; t < 3000; t += 12) pushes += p.due(true, t) ? 1 : 0;
    TEST_ASSERT_TRUE(pushes <= 10);
    TEST_ASSERT_TRUE(pushes >= 9);
    // Across the millis() wrap.
    PushPacer w;
    TEST_ASSERT_TRUE(w.due(true, 0xFFFFFFC0u));
    TEST_ASSERT_FALSE(w.due(true, 0xFFFFFFF0u));
    TEST_ASSERT_TRUE(w.due(false, 0xFFFFFFC0u + PushPacer::MIN_INTERVAL_MS));
    p.reset();
    TEST_ASSERT_FALSE(p.due(false, 5000));
}

// AF19 (PERF-8): allocations depend on the largest free block, not on the free heap.
static void test_heap_block_limits() {
    TEST_ASSERT_FALSE(HeapLimits::acceptOk(200000, HeapLimits::MIN_BLOCK_ACCEPT - 1));
    TEST_ASSERT_TRUE(HeapLimits::acceptOk(200000, HeapLimits::MIN_BLOCK_ACCEPT));
    TEST_ASSERT_FALSE(HeapLimits::acceptOk(RequestGuard::MIN_FREE_HEAP - 1, 100000)); // W-S10 stays
    TEST_ASSERT_FALSE(HeapLimits::pushOk(1000, 1000 + HeapLimits::PUSH_RESERVE - 1));
    TEST_ASSERT_TRUE(HeapLimits::pushOk(1000, 1000 + HeapLimits::PUSH_RESERVE));
}

static HttpPreFilter::Verdict feedAll(HttpPreFilter& f, const std::string& s, uint32_t now = 0) {
    return f.feed((const uint8_t*)s.data(), s.size(), now);
}

static void test_prefilter_limits() {
    typedef HttpPreFilter::Verdict V;
    HttpPreFilter f;
    f.begin(0);
    const std::string ok = "POST /api/config HTTP/1.1\r\nHost: cyd-hf-1a2b.local\r\nContent-Type: application/json\r\n"
                           "Content-Length: 12\r\n\r\n{\"drive\":8}";
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, ok));
    TEST_ASSERT_TRUE(f.headComplete());
    // Byte by byte gives the same.
    f.begin(0);
    for (char c : ok) TEST_ASSERT_EQUAL(V::Pass, f.feed((const uint8_t*)&c, 1, 0));
    // URL: 256 bytes pass, 257 are 414.
    f.begin(0);
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, "GET /" + std::string(255, 'a') + " HTTP/1.1\r\n"));
    f.begin(0);
    TEST_ASSERT_EQUAL(V::UriTooLong, feedAll(f, "GET /" + std::string(256, 'a')));
    TEST_ASSERT_EQUAL_UINT16(414, HttpPreFilter::status(V::UriTooLong));
    // A request line without spaces that never ends.
    f.begin(0);
    TEST_ASSERT_EQUAL(V::UriTooLong, feedAll(f, std::string(400, 'G')));
    // Headers: 2048 bytes pass, more are 431.
    f.begin(0);
    const std::string line = "X-A: " + std::string(2048 - 7, 'b') + "\r\n"; // exactly 2048
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, "GET / HTTP/1.1\r\n" + line));
    TEST_ASSERT_EQUAL(V::HeadersTooLarge, feedAll(f, "\r\n"));
    TEST_ASSERT_EQUAL(V::HeadersTooLarge, feedAll(f, "more")); // stays
    TEST_ASSERT_EQUAL_UINT16(431, HttpPreFilter::status(V::HeadersTooLarge));
    // Content-Length above 1024 (any case, blanks), or absurd: 413 at the end of the headers.
    f.begin(0);
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, "POST /api/x HTTP/1.1\r\ncontent-LENGTH:   1025\r\n"));
    TEST_ASSERT_EQUAL(V::PayloadTooLarge, feedAll(f, "\r\n"));
    f.begin(0);
    TEST_ASSERT_EQUAL(V::PayloadTooLarge, feedAll(f, "POST /api/x HTTP/1.1\r\nContent-Length: 99999999999999999999999\r\n\r\n"));
    f.begin(0);
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, "POST /api/x HTTP/1.1\r\nContent-Length: 1024\r\nX-Content-Length: 5000\r\n\r\n"));
    // Repeated: the largest counts.
    f.begin(0);
    TEST_ASSERT_EQUAL(V::PayloadTooLarge, feedAll(f, "POST /x HTTP/1.1\r\nContent-Length: 10\r\nContent-Length: 2000\r\n\r\n"));
    // Bytes after the head (chunked framing, junk) are bounded.
    f.begin(0);
    feedAll(f, "POST /api/x HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n");
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, std::string(HttpPreFilter::MAX_AFTER_HEAD, '1')));
    TEST_ASSERT_EQUAL(V::TooMuchData, feedAll(f, "1"));
    TEST_ASSERT_EQUAL_UINT16(0, HttpPreFilter::status(V::TooMuchData));
    // 10 s for the whole request, also for a sender that keeps trickling.
    f.begin(5000);
    TEST_ASSERT_EQUAL(V::Pass, feedAll(f, "GET / HTTP/1.1\r\n", 5000 + 9000));
    TEST_ASSERT_EQUAL(V::Pass, f.check(5000 + HttpPreFilter::DEADLINE_MS));
    TEST_ASSERT_EQUAL(V::Timeout, feedAll(f, "H", 5000 + HttpPreFilter::DEADLINE_MS + 1));
}

static void test_connection_tables() {
    HttpConnections c;
    int slots[HttpConnections::MAX];
    for (uint8_t i = 0; i < HttpConnections::MAX; ++i) {
        slots[i] = c.acquire(0);
        TEST_ASSERT_TRUE(slots[i] >= 0);
    }
    TEST_ASSERT_EQUAL(-1, c.acquire(0)); // W-S10: <= 4 HTTP connections
    TEST_ASSERT_EQUAL_UINT8(4, c.count());
    c.release(slots[2]);
    TEST_ASSERT_FALSE(c.used(slots[2]));
    TEST_ASSERT_EQUAL(slots[2], c.acquire(100));
    c.release(-1);
    c.release(99);

    WsClients w;
    TEST_ASSERT_TRUE(w.add(11, 0));
    TEST_ASSERT_TRUE(w.add(12, 0));
    TEST_ASSERT_TRUE(w.add(13, 0));
    TEST_ASSERT_TRUE(w.add(12, 0)); // known
    TEST_ASSERT_FALSE(w.add(14, 0)); // W-E10: a fourth is refused
    TEST_ASSERT_FALSE(w.add(0, 0));
    TEST_ASSERT_EQUAL_UINT8(3, w.count());
    // A new client waits for the state.
    TEST_ASSERT_EQUAL_UINT32(11, w.needingState(0));
    w.sent(11);
    TEST_ASSERT_EQUAL_UINT32(0, w.needingState(0));
    w.sent(12);
    w.sent(13);
    w.markAll();
    TEST_ASSERT_EQUAL_UINT32(13, w.needingState(2));
    TEST_ASSERT_EQUAL_UINT32(0, w.needingState(3));
    // Ping every 15 s, closed after 30 s without pong.
    uint8_t p11[4], p12[4], p13[4];
    TEST_ASSERT_EQUAL_UINT32(0, w.duePing(WsClients::PING_MS - 1, 1, p11));
    TEST_ASSERT_EQUAL_UINT32(11, w.duePing(WsClients::PING_MS, 0x11111111u, p11));
    TEST_ASSERT_EQUAL_UINT32(12, w.duePing(WsClients::PING_MS, 0x22222222u, p12));
    TEST_ASSERT_EQUAL_UINT32(13, w.duePing(WsClients::PING_MS, 0x33333333u, p13));
    TEST_ASSERT_EQUAL_UINT32(0, w.duePing(WsClients::PING_MS, 4, p13));
    w.pong(11, 20000, p11, 4);
    w.pong(12, 20000, p12, 4);
    w.pong(13, 20000, p11, 4); // the payload of another client's PING: ignored
    TEST_ASSERT_EQUAL_UINT32(0, w.expired(WsClients::PONG_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT32(13, w.expired(WsClients::PONG_TIMEOUT_MS + 1));
    TEST_ASSERT_EQUAL_UINT8(2, w.count());
    TEST_ASSERT_EQUAL_UINT32(0, w.expired(WsClients::PONG_TIMEOUT_MS + 1));
    w.remove(11);
    TEST_ASSERT_EQUAL_UINT8(1, w.count());
    TEST_ASSERT_TRUE(w.add(14, 50000));
    // W-S6: inbound frames up to 512 bytes.
    TEST_ASSERT_TRUE(WsClients::frameAllowed(512));
    TEST_ASSERT_FALSE(WsClients::frameAllowed(513));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_change_applies_after_the_answer_and_reverts_without_confirmation);
    RUN_TEST(test_change_confirmed_over_the_web);
    RUN_TEST(test_change_without_password_needs_the_touch);
    RUN_TEST(test_final_change);
    RUN_TEST(test_touch_change_refused_while_pending);
    RUN_TEST(test_touch_revert_before_apply);
    RUN_TEST(test_power_loss_finds_the_previous_settings);
    RUN_TEST(test_change_across_the_millis_wrap);
    RUN_TEST(test_scan_semantics);
    RUN_TEST(test_login_queue);
    RUN_TEST(test_web_password_job);
    RUN_TEST(test_prefilter_limits);
    RUN_TEST(test_connection_tables);
    RUN_TEST(test_ws_unsolicited_pongs_do_not_hold_a_slot);
    RUN_TEST(test_ws_ping_flood_closes);
    RUN_TEST(test_push_pacer);
    RUN_TEST(test_heap_block_limits);
    return UNITY_END();
}
