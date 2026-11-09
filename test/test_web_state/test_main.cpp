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
        // With a password the touch has no say.
        NetChange c;
        c.request(a, b, false, false, 0);
        c.update(NetChange::APPLY_DELAY_MS);
        c.confirmTouch(false);
        TEST_ASSERT_EQUAL(NetChange::Action::None, c.update(1000));
    }
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

// --- Limits --------------------------------------------------------------------------------

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
    TEST_ASSERT_EQUAL_UINT32(0, w.duePing(WsClients::PING_MS - 1));
    TEST_ASSERT_EQUAL_UINT32(11, w.duePing(WsClients::PING_MS));
    TEST_ASSERT_EQUAL_UINT32(12, w.duePing(WsClients::PING_MS));
    TEST_ASSERT_EQUAL_UINT32(13, w.duePing(WsClients::PING_MS));
    TEST_ASSERT_EQUAL_UINT32(0, w.duePing(WsClients::PING_MS));
    w.pong(11, 20000);
    w.pong(12, 20000);
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
    RUN_TEST(test_change_across_the_millis_wrap);
    RUN_TEST(test_scan_semantics);
    RUN_TEST(test_login_queue);
    RUN_TEST(test_prefilter_limits);
    RUN_TEST(test_connection_tables);
    return UNITY_END();
}
