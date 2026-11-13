// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// VXI-11 and its portmapper (A-78, E-37): XDR and RPC headers, record marking over TCP,
// portmapper over UDP/TCP, the core channel against the real controller and the SI5351
// register model. Calls are built and replies decoded here byte by byte, independently of the
// encoder in remote/onc_rpc.cpp.

#include <unity.h>
#include <Preferences.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <config.h>
#include <constants.h>
#include <core/app_controller.h>
#include <hal/i2c_bus.h>
#include <remote/lxi_ident.h>
#include <remote/onc_rpc.h>
#include <remote/scpi.h>
#include <remote/scpi_usb.h>
#include <remote/vxi11.h>
#include <utils/logger.h>

static Fake::Si5351Chip& chip = Fake::si5351();

static const uint32_t IP_A = 0xC0A80114; // 192.168.1.20
static const uint32_t IP_B = 0xC0A80115;
static const uint16_t CORE_PORT = 9009;

typedef std::vector<uint8_t> Bytes;

// ---------------------------------------------------------------------------
// Independent XDR encoding and decoding

static void put32(Bytes& b, uint32_t v) {
    b.push_back((uint8_t)(v >> 24));
    b.push_back((uint8_t)(v >> 16));
    b.push_back((uint8_t)(v >> 8));
    b.push_back((uint8_t)v);
}
static void putOpaque(Bytes& b, const std::string& s) {
    put32(b, (uint32_t)s.size());
    b.insert(b.end(), s.begin(), s.end());
    while (b.size() % 4) b.push_back(0);
}

struct CallSpec {
    uint32_t xid = 0x1234;
    uint32_t rpcvers = 2;
    uint32_t prog = Vxi11::CORE_PROG;
    uint32_t vers = 1;
    uint32_t proc = 0;
    uint32_t cred = 0;
    std::string credBody;
    uint32_t verf = 0;
    std::string verfBody;
};

static Bytes call(const CallSpec& s, const Bytes& args = Bytes()) {
    Bytes b;
    put32(b, s.xid);
    put32(b, 0); // CALL
    put32(b, s.rpcvers);
    put32(b, s.prog);
    put32(b, s.vers);
    put32(b, s.proc);
    put32(b, s.cred);
    putOpaque(b, s.credBody);
    put32(b, s.verf);
    putOpaque(b, s.verfBody);
    b.insert(b.end(), args.begin(), args.end());
    return b;
}

static Bytes coreCall(uint32_t proc, const Bytes& args, uint32_t xid = 0x1234) {
    CallSpec s;
    s.proc = proc;
    s.xid = xid;
    return call(s, args);
}

static Bytes pmapCall(uint32_t proc, const Bytes& args, uint32_t vers = 2) {
    CallSpec s;
    s.prog = 100000;
    s.vers = vers;
    s.proc = proc;
    return call(s, args);
}

static Bytes getport(uint32_t prog, uint32_t vers, uint32_t prot) {
    Bytes a;
    put32(a, prog);
    put32(a, vers);
    put32(a, prot);
    put32(a, 0);
    return pmapCall(3, a);
}

// A record with one fragment.
static Bytes record(const Bytes& msg) {
    Bytes b;
    put32(b, 0x80000000u | (uint32_t)msg.size());
    b.insert(b.end(), msg.begin(), msg.end());
    return b;
}

struct Reply {
    bool ok = false;     // decoded as a reply at all
    uint32_t xid = 0;
    uint32_t stat = 99;  // 0 accepted, 1 denied
    uint32_t accept = 99;
    uint32_t reject = 99;
    std::vector<uint32_t> words; // after accept_stat / reject_stat
    Bytes raw;
};

static uint32_t get32(const Bytes& b, size_t at) {
    return (uint32_t)b[at] << 24 | (uint32_t)b[at + 1] << 16 | (uint32_t)b[at + 2] << 8 | b[at + 3];
}

static Reply decode(const Bytes& b) {
    Reply r;
    r.raw = b;
    if (b.size() < 12 || b.size() % 4) return r;
    r.xid = get32(b, 0);
    if (get32(b, 4) != 1) return r;
    r.stat = get32(b, 8);
    size_t at = 12;
    if (r.stat == 0) {
        if (b.size() < 24) return r;
        TEST_ASSERT_EQUAL_UINT32(0, get32(b, 12)); // verifier AUTH_NONE
        TEST_ASSERT_EQUAL_UINT32(0, get32(b, 16));
        r.accept = get32(b, 20);
        at = 24;
    } else {
        if (b.size() < 16) return r;
        r.reject = get32(b, 12);
        at = 16;
    }
    for (; at + 4 <= b.size(); at += 4) r.words.push_back(get32(b, at));
    r.ok = true;
    return r;
}

// The opaque data in a device_read reply (words: error, reason, length, data...).
static std::string readData(const Reply& r) {
    if (r.raw.size() < 36) return "";
    const uint32_t n = get32(r.raw, 32);
    return std::string((const char*)&r.raw[36], n);
}

static Reply pmap(const Bytes& msg, Portmap::Transport t = Portmap::Transport::Udp, uint16_t corePort = CORE_PORT) {
    uint8_t out[256];
    const size_t n = Portmap::handle(msg.data(), msg.size(), t, corePort, out, sizeof(out));
    return decode(Bytes(out, out + n));
}

// ---------------------------------------------------------------------------
// Device and TCP link

struct FakeSocket : ScpiTcpSocket {
    Bytes sent;
    int32_t room = -1;
    bool fail = false;
    int closes = 0;
    int32_t trySend(const char* data, size_t len) override {
        if (fail) return -1;
        size_t take = len;
        if (room >= 0 && take > (size_t)room) take = (size_t)room;
        if (room >= 0) room -= (int32_t)take;
        sent.insert(sent.end(), (const uint8_t*)data, (const uint8_t*)data + take);
        return (int32_t)take;
    }
    void closeConnection() override { ++closes; }
};

// Splits the sent bytes into records (all fragments here are last fragments).
static std::vector<Reply> replies(Bytes& sent) {
    std::vector<Reply> out;
    size_t at = 0;
    while (at + 4 <= sent.size()) {
        const uint32_t h = get32(sent, at);
        TEST_ASSERT_TRUE(h & 0x80000000u);
        const size_t n = h & 0x7FFFFFFFu;
        TEST_ASSERT_TRUE(at + 4 + n <= sent.size());
        out.push_back(decode(Bytes(sent.begin() + at + 4, sent.begin() + at + 4 + n)));
        at += 4 + n;
    }
    TEST_ASSERT_EQUAL(sent.size(), at);
    sent.clear();
    return out;
}

struct Device {
    SettingsService settings;
    SI5351Manager si;
    BacklightDriver backlight;
    AppController app;
    CommandQueue queue;
    StateSnapshot snap;
    LoopScpiBackend backend;
    Vxi11Core core;
    FakeSocket socket;
    uint8_t in[Vxi11Core::MAX_RECORD];
    uint8_t out[Vxi11Core::MAX_REPLY];
    RpcTcpLink link;
    uint32_t xid = 100;
    uint32_t lid = 0;
    Device()
        : app(settings, si, backlight), backend(queue, app, snap), core(backend, "0123456789AB"),
          link(core, socket, in, sizeof(in), out, sizeof(out), SCPI_IDLE_TIMEOUT_MS, "VXI-11") {
        I2CBus::begin();
        settings.begin();
        PersistentState loaded;
        settings.load(loaded);
        app.begin(loaded);
        backlight.begin();
        app.connectHardware();
        app.applyStoredChannelSettings();
    }
    void connect(uint32_t ip = IP_A) {
        link.setAllowed(true);
        TEST_ASSERT_EQUAL(RpcTcpLink::Accept::Accepted, link.offer(ip, 0));
    }
    // One call as one record; its reply.
    Reply rpc(uint32_t proc, const Bytes& args, uint32_t now = 0) {
        const Bytes r = record(coreCall(proc, args, ++xid));
        link.receive(r.data(), r.size(), now);
        std::vector<Reply> rs = replies(socket.sent);
        TEST_ASSERT_EQUAL(1, rs.size());
        TEST_ASSERT_EQUAL_UINT32(xid, rs[0].xid);
        return rs[0];
    }
    Reply createLink(const char* name = "inst0", bool lock = false) {
        Bytes a;
        put32(a, 42);
        put32(a, lock ? 1 : 0);
        put32(a, 0);
        putOpaque(a, name);
        Reply r = rpc(Vxi11::CREATE_LINK, a);
        if (r.words.size() == 4 && r.words[0] == 0) lid = r.words[1];
        return r;
    }
    Reply write(const std::string& data, uint32_t flags = Vxi11::FLAG_END, uint32_t link = 0) {
        Bytes a;
        put32(a, link ? link : lid);
        put32(a, 1000);
        put32(a, 0);
        put32(a, flags);
        putOpaque(a, data);
        return rpc(Vxi11::DEVICE_WRITE, a);
    }
    Reply read(uint32_t requestSize = 0x100000, uint32_t flags = 0, uint32_t termChar = 0) {
        Bytes a;
        put32(a, lid);
        put32(a, requestSize);
        put32(a, 1000);
        put32(a, 0);
        put32(a, flags);
        put32(a, termChar);
        return rpc(Vxi11::DEVICE_READ, a);
    }
    Reply generic(uint32_t proc, uint32_t link = 0) {
        Bytes a;
        put32(a, link ? link : lid);
        put32(a, 0);
        put32(a, 0);
        put32(a, 1000);
        return rpc(proc, a);
    }
    Reply linkOnly(uint32_t proc, uint32_t link = 0) {
        Bytes a;
        put32(a, link ? link : lid);
        return rpc(proc, a);
    }
    // Write, then read until END like a VISA client; the answer.
    std::string query(const std::string& q) {
        Reply w = write(q);
        TEST_ASSERT_EQUAL_UINT32(0, w.words[0]);
        std::string all;
        for (int i = 0; i < 100; ++i) {
            Reply r = read();
            TEST_ASSERT_EQUAL_UINT32(0, r.accept);
            if (r.words[0] != 0) {
                char e[24];
                snprintf(e, sizeof(e), "<error %u>", (unsigned)r.words[0]);
                return all + e;
            }
            all += readData(r);
            if (r.words[1] & Vxi11::REASON_END) return all;
        }
        return all + "<no END>";
    }
};

static std::string logText() {
    std::string all;
    char buf[512];
    while (Logger::readBuffered(buf, sizeof(buf)) > 0) all += buf;
    return all;
}

static bool contains(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

void setUp() {
    chip.reset();
    Fake::nvs().reset();
    Fake::clockMs() = 0;
    Logger::setOutput(Logger::Output::ToBuffer);
    logText();
}
void tearDown() { Logger::setOutput(Logger::Output::ToSerial); }

// ---------------------------------------------------------------------------
// XDR and RPC headers

static void test_xdr_reader() {
    const uint8_t d[] = {0, 0, 0, 3, 'a', 'b', 'c', 0, 0, 0, 0, 2, 0xFF, 0xFF, 0xFF, 0xFE};
    Xdr::Reader r(d, sizeof(d));
    const uint8_t* p = nullptr;
    uint32_t n = 0;
    TEST_ASSERT_TRUE(r.opaque(&p, &n, 8));
    TEST_ASSERT_EQUAL_UINT32(3, n);
    TEST_ASSERT_EQUAL_MEMORY("abc", p, 3);
    bool b = true;
    TEST_ASSERT_FALSE(r.boolean(&b)); // 2 is no XDR bool
    TEST_ASSERT_FALSE(r.ok());
    int32_t i = 0;
    TEST_ASSERT_FALSE(r.i32(&i)); // stays failed

    Xdr::Reader s(d + 12, 4);
    TEST_ASSERT_TRUE(s.i32(&i));
    TEST_ASSERT_EQUAL_INT32(-2, i);
    uint32_t u = 0;
    TEST_ASSERT_FALSE(s.u32(&u)); // end of input

    // Length over the maximum, and data plus padding beyond the input.
    Xdr::Reader t(d, sizeof(d));
    TEST_ASSERT_FALSE(t.opaque(&p, &n, 2));
    const uint8_t cut[] = {0, 0, 0, 5, 'a', 'b', 'c', 'd', 'e', 0, 0};
    Xdr::Reader v(cut, sizeof(cut));
    TEST_ASSERT_FALSE(v.opaque(&p, &n, 100));
    const uint8_t huge[] = {0xFF, 0xFF, 0xFF, 0xFF, 0};
    Xdr::Reader h(huge, sizeof(huge));
    TEST_ASSERT_FALSE(h.opaque(&p, &n, 0xFFFFFFFFu));
}

static void test_xdr_writer() {
    uint8_t buf[12];
    memset(buf, 0xAA, sizeof(buf));
    Xdr::Writer w(buf, sizeof(buf));
    w.opaque((const uint8_t*)"hello", 5);
    TEST_ASSERT_TRUE(w.ok());
    TEST_ASSERT_EQUAL(12, w.length());
    const uint8_t expect[] = {0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o', 0, 0, 0};
    TEST_ASSERT_EQUAL_MEMORY(expect, buf, sizeof(expect));
    w.u32(1);
    TEST_ASSERT_FALSE(w.ok());
    TEST_ASSERT_EQUAL(12, w.length());

    Xdr::Writer x(buf, 8);
    x.opaque((const uint8_t*)"hello", 5); // needs 12
    TEST_ASSERT_FALSE(x.ok());
    TEST_ASSERT_EQUAL(0, x.length());
}

static void test_rpc_header_refusals() {
    // RPC version 3: MSG_DENIED RPC_MISMATCH 2..2.
    CallSpec s;
    s.prog = 100000;
    s.vers = 2;
    s.rpcvers = 3;
    Reply r = pmap(call(s));
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT32(0x1234, r.xid);
    TEST_ASSERT_EQUAL_UINT32(1, r.stat);
    TEST_ASSERT_EQUAL_UINT32(0, r.reject);
    TEST_ASSERT_EQUAL(2, r.words.size());
    TEST_ASSERT_EQUAL_UINT32(2, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(2, r.words[1]);

    // AUTH_SYS is accepted, its body not interpreted.
    s.rpcvers = 2;
    s.cred = 1;
    s.credBody = std::string(20, 'x');
    TEST_ASSERT_EQUAL_UINT32(0, pmap(call(s)).accept);
    // AUTH_SHORT, AUTH_DES, RPCSEC_GSS: AUTH_ERROR AUTH_REJECTEDCRED.
    const uint32_t flavors[] = {2, 3, 6, 0xFFFFFFFFu};
    for (uint32_t f : flavors) {
        s.cred = f;
        r = pmap(call(s));
        TEST_ASSERT_EQUAL_UINT32(1, r.stat);
        TEST_ASSERT_EQUAL_UINT32(1, r.reject);
        TEST_ASSERT_EQUAL(1, r.words.size());
        TEST_ASSERT_EQUAL_UINT32(2, r.words[0]);
    }
    // A verifier other than AUTH_NONE: AUTH_REJECTEDVERF.
    s.cred = 0;
    s.verf = 1;
    r = pmap(call(s));
    TEST_ASSERT_EQUAL_UINT32(1, r.reject);
    TEST_ASSERT_EQUAL_UINT32(4, r.words[0]);
    // Credentials over 400 bytes: AUTH_BADCRED; a broken verifier: AUTH_BADVERF.
    s.verf = 0;
    s.cred = 1;
    s.credBody = std::string(401, 'x');
    r = pmap(call(s));
    TEST_ASSERT_EQUAL_UINT32(1, r.words[0]);
    s.credBody = std::string(400, 'x');
    TEST_ASSERT_EQUAL_UINT32(0, pmap(call(s)).accept);
    s.credBody.clear();
    Bytes m = call(s);
    m.resize(m.size() - 4); // verifier length missing
    r = pmap(m);
    TEST_ASSERT_EQUAL_UINT32(3, r.words[0]);

    // No reply at all: a reply message, a truncated header, nothing.
    Bytes reply = call(CallSpec());
    reply[7] = 1;
    TEST_ASSERT_FALSE(pmap(reply).ok);
    Bytes full = pmapCall(0, Bytes());
    for (size_t n = 0; n < 28; ++n) TEST_ASSERT_FALSE(pmap(Bytes(full.begin(), full.begin() + n)).ok);
    uint8_t out[64];
    TEST_ASSERT_EQUAL(0, Portmap::handle(nullptr, 0, Portmap::Transport::Udp, CORE_PORT, out, sizeof(out)));
    TEST_ASSERT_EQUAL(0, Portmap::handle(full.data(), full.size(), Portmap::Transport::Udp, CORE_PORT, nullptr, 64));
    // Over UDP no reply is longer than its request (reflection): a 12-byte call with RPC
    // version 3 would get 24 bytes; over TCP it is answered.
    Bytes tiny;
    put32(tiny, 7);
    put32(tiny, 0);
    put32(tiny, 3);
    TEST_ASSERT_FALSE(pmap(tiny).ok);
    TEST_ASSERT_EQUAL_UINT32(0, pmap(tiny, Portmap::Transport::Tcp).reject);
    // A reply that does not fit is not sent.
    TEST_ASSERT_EQUAL(0, Portmap::handle(full.data(), full.size(), Portmap::Transport::Udp, CORE_PORT, out, 20));
}

// ---------------------------------------------------------------------------
// Portmapper

static void test_portmap_getport() {
    Reply r = pmap(getport(Vxi11::CORE_PROG, 1, 6));
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_UINT32(0, r.stat);
    TEST_ASSERT_EQUAL_UINT32(0, r.accept);
    TEST_ASSERT_EQUAL(1, r.words.size());
    TEST_ASSERT_EQUAL_UINT32(CORE_PORT, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(CORE_PORT, pmap(getport(Vxi11::CORE_PROG, 1, 6), Portmap::Transport::Tcp).words[0]);
    // UDP, another version, the abort and interrupt programs: not mapped.
    TEST_ASSERT_EQUAL_UINT32(0, pmap(getport(Vxi11::CORE_PROG, 1, 17)).words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, pmap(getport(Vxi11::CORE_PROG, 2, 6)).words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, pmap(getport(0x0607B0, 1, 6)).words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, pmap(getport(0x0607B1, 1, 6)).words[0]);
    // The portmapper itself.
    TEST_ASSERT_EQUAL_UINT32(111, pmap(getport(100000, 2, 6)).words[0]);
    TEST_ASSERT_EQUAL_UINT32(111, pmap(getport(100000, 2, 17)).words[0]);
    // Core channel not listening.
    TEST_ASSERT_EQUAL_UINT32(0, pmap(getport(Vxi11::CORE_PROG, 1, 6), Portmap::Transport::Udp, 0).words[0]);
    // Truncated arguments: GARBAGE_ARGS.
    Bytes cut = getport(Vxi11::CORE_PROG, 1, 6);
    cut.resize(cut.size() - 4);
    TEST_ASSERT_EQUAL_UINT32(4, pmap(cut).accept);
    // Trailing bytes after the arguments are ignored.
    Bytes more = getport(Vxi11::CORE_PROG, 1, 6);
    put32(more, 7);
    TEST_ASSERT_EQUAL_UINT32(CORE_PORT, pmap(more).words[0]);
}

// The exact request liblxi (lxi discover) broadcasts.
static void test_portmap_lxi_discover_request() {
    const uint8_t msg[] = {0x00, 0x00, 0x03, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01,
                           0x86, 0xa0, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06,
                           0x07, 0xaf, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00};
    uint8_t out[64];
    const size_t n = Portmap::handle(msg, sizeof(msg), Portmap::Transport::Udp, CORE_PORT, out, sizeof(out));
    const uint8_t expect[] = {0x00, 0x00, 0x03, 0xe8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x23, 0x31};
    TEST_ASSERT_EQUAL(sizeof(expect), n);
    TEST_ASSERT_EQUAL_MEMORY(expect, out, sizeof(expect));
}

static void test_portmap_other_procedures() {
    // NULL: success without results.
    Reply r = pmap(pmapCall(0, Bytes()));
    TEST_ASSERT_EQUAL_UINT32(0, r.accept);
    TEST_ASSERT_EQUAL(0, r.words.size());
    // SET, UNSET, CALLIT, unknown: PROC_UNAVAIL (CALLIT never forwards).
    const uint32_t procs[] = {1, 2, 5, 6, 1000};
    for (uint32_t p : procs) {
        TEST_ASSERT_EQUAL_UINT32(3, pmap(pmapCall(p, Bytes(64, 0))).accept);
        TEST_ASSERT_EQUAL_UINT32(3, pmap(pmapCall(p, Bytes(64, 0)), Portmap::Transport::Tcp).accept);
    }
    // DUMP over TCP lists the three mappings; over UDP it is refused (reflection).
    r = pmap(pmapCall(4, Bytes()), Portmap::Transport::Tcp);
    TEST_ASSERT_EQUAL_UINT32(0, r.accept);
    const uint32_t list[] = {1, 100000, 2, 6, 111, 1, 100000, 2, 17, 111, 1, 0x0607AF, 1, 6, CORE_PORT, 0};
    TEST_ASSERT_EQUAL(16, r.words.size());
    TEST_ASSERT_EQUAL_UINT32_ARRAY(list, r.words.data(), 16);
    TEST_ASSERT_EQUAL_UINT32(3, pmap(pmapCall(4, Bytes())).accept);
    // rpcbind 3 and 4: PROG_MISMATCH 2..2, so the client falls back to version 2.
    r = pmap(pmapCall(3, Bytes(16, 0), 4));
    TEST_ASSERT_EQUAL_UINT32(2, r.accept);
    TEST_ASSERT_EQUAL_UINT32(2, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(2, r.words[1]);
    TEST_ASSERT_EQUAL_UINT32(2, pmap(pmapCall(0, Bytes(), 3)).accept);
    // Another program: PROG_UNAVAIL.
    CallSpec s;
    s.prog = 100003; // NFS
    s.vers = 3;
    TEST_ASSERT_EQUAL_UINT32(1, pmap(call(s)).accept);
}

static void test_udp_budget() {
    Portmap::UdpBudget b;
    for (int i = 0; i < Portmap::UdpBudget::PER_SECOND; ++i) TEST_ASSERT_TRUE(b.take(5000));
    TEST_ASSERT_FALSE(b.take(5000));
    TEST_ASSERT_FALSE(b.take(5999));
    TEST_ASSERT_TRUE(b.take(6000));
    // Across the 32-bit wrap of millis().
    Portmap::UdpBudget w;
    for (int i = 0; i < Portmap::UdpBudget::PER_SECOND; ++i) TEST_ASSERT_TRUE(w.take(0xFFFFFF00u));
    TEST_ASSERT_FALSE(w.take(0xFFFFFFF0u));
    TEST_ASSERT_FALSE(w.take(0x000002E7u));
    TEST_ASSERT_TRUE(w.take(0x000002E8u));
}

// ---------------------------------------------------------------------------
// Record marking

struct PmapConn {
    PortmapProgram program {CORE_PORT};
    FakeSocket socket;
    uint8_t in[512];
    uint8_t out[128];
    RpcTcpLink link {program, socket, in, sizeof(in), out, sizeof(out), PORTMAP_TCP_IDLE_MS, nullptr};
    PmapConn() {
        link.setAllowed(true);
        TEST_ASSERT_EQUAL(RpcTcpLink::Accept::Accepted, link.offer(IP_A, 0));
    }
};

static void test_record_fragments_and_byte_by_byte() {
    PmapConn c;
    const Bytes msg = getport(Vxi11::CORE_PROG, 1, 6);
    // Three fragments, every byte on its own; the glue reads at most wantBytes().
    Bytes stream;
    put32(stream, 10);
    stream.insert(stream.end(), msg.begin(), msg.begin() + 10);
    put32(stream, 0); // an empty middle fragment
    put32(stream, 0x80000000u | (uint32_t)(msg.size() - 10));
    stream.insert(stream.end(), msg.begin() + 10, msg.end());
    for (size_t i = 0; i < stream.size(); ++i) {
        TEST_ASSERT_TRUE(c.link.wantBytes() >= 1);
        c.link.receive(&stream[i], 1, 0);
    }
    std::vector<Reply> rs = replies(c.socket.sent);
    TEST_ASSERT_EQUAL(1, rs.size());
    TEST_ASSERT_EQUAL_UINT32(CORE_PORT, rs[0].words[0]);
    // wantBytes: a header, then exactly the fragment.
    TEST_ASSERT_EQUAL(4, c.link.wantBytes());
    const Bytes r = record(msg);
    c.link.receive(r.data(), 4, 0);
    TEST_ASSERT_EQUAL(msg.size(), c.link.wantBytes());
    c.link.receive(r.data() + 4, r.size() - 4, 0);
    TEST_ASSERT_EQUAL(1, replies(c.socket.sent).size());
    // Two records in one read are both answered.
    Bytes two = record(msg);
    const Bytes second = record(pmapCall(0, Bytes()));
    two.insert(two.end(), second.begin(), second.end());
    c.link.receive(two.data(), two.size(), 0);
    TEST_ASSERT_EQUAL(2, replies(c.socket.sent).size());
    // An empty record and a reply message get no answer, the connection stays.
    const Bytes empty = record(Bytes());
    c.link.receive(empty.data(), empty.size(), 0);
    TEST_ASSERT_EQUAL(0, c.socket.sent.size());
    TEST_ASSERT_TRUE(c.link.active());
}

static void test_record_too_large_closes() {
    PmapConn c;
    // One byte over the buffer, split into fragments: closed when the fragment is announced.
    Bytes s;
    put32(s, 500);
    s.insert(s.end(), 500, 0);
    put32(s, 0x80000000u | 13);
    c.link.receive(s.data(), s.size(), 0);
    TEST_ASSERT_FALSE(c.link.active());
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::TooLarge, c.link.lastClose());
    TEST_ASSERT_EQUAL(1, c.socket.closes);
    // An HTTP request: "POST" is a fragment of 1.3 GB.
    PmapConn h;
    const char* http = "POST / HTTP/1.1\r\nHost: x\r\n\r\n";
    h.link.receive((const uint8_t*)http, strlen(http), 0);
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::TooLarge, h.link.lastClose());
    TEST_ASSERT_EQUAL(0, h.socket.sent.size());
    // The largest length.
    PmapConn m;
    const uint8_t max[] = {0xFF, 0xFF, 0xFF, 0xFF};
    m.link.receive(max, 4, 0);
    TEST_ASSERT_FALSE(m.link.active());
    // Exactly the buffer size fits.
    PmapConn e;
    Bytes big = pmapCall(0, Bytes(512 - 40, 0));
    TEST_ASSERT_EQUAL(512, big.size());
    const Bytes rb = record(big);
    e.link.receive(rb.data(), rb.size(), 0);
    TEST_ASSERT_TRUE(e.link.active());
    TEST_ASSERT_EQUAL(1, replies(e.socket.sent).size());
}

static void test_record_partial_send_and_failures() {
    PmapConn c;
    c.socket.room = 5;
    const Bytes r = record(getport(Vxi11::CORE_PROG, 1, 6));
    c.link.receive(r.data(), r.size(), 0);
    TEST_ASSERT_EQUAL(5, c.socket.sent.size());
    TEST_ASSERT_FALSE(c.link.wantsInput());
    TEST_ASSERT_EQUAL(0, c.link.wantBytes());
    c.link.update(10); // the stack takes nothing more yet
    TEST_ASSERT_EQUAL(5, c.socket.sent.size());
    c.socket.room = -1;
    c.link.update(20);
    TEST_ASSERT_TRUE(c.link.wantsInput());
    std::vector<Reply> rs = replies(c.socket.sent);
    TEST_ASSERT_EQUAL(1, rs.size());
    TEST_ASSERT_EQUAL_UINT32(CORE_PORT, rs[0].words[0]);
    // A failed send closes.
    c.socket.fail = true;
    c.link.receive(r.data(), r.size(), 30);
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::SendFailed, c.link.lastClose());
    TEST_ASSERT_FALSE(c.link.active());
}

static void test_record_link_lifecycle() {
    PmapConn c;
    // One connection: the second offer is busy.
    TEST_ASSERT_EQUAL(RpcTcpLink::Accept::Busy, c.link.offer(IP_B, 0));
    TEST_ASSERT_EQUAL_HEX32(IP_A, c.link.clientIp());
    // Idle without a complete record (bytes of a partial one do not count).
    const uint8_t part[] = {0x80, 0, 0, 40, 1, 2};
    c.link.receive(part, sizeof(part), PORTMAP_TCP_IDLE_MS - 1);
    c.link.update(PORTMAP_TCP_IDLE_MS - 1);
    TEST_ASSERT_TRUE(c.link.active());
    c.link.update(PORTMAP_TCP_IDLE_MS);
    TEST_ASSERT_FALSE(c.link.active());
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::Idle, c.link.lastClose());
    TEST_ASSERT_EQUAL_HEX32(0, c.link.clientIp());
    // A new connection starts with a clean record state.
    TEST_ASSERT_EQUAL(RpcTcpLink::Accept::Accepted, c.link.offer(IP_B, 50000));
    const Bytes r = record(pmapCall(0, Bytes()));
    c.link.receive(r.data(), r.size(), 50000);
    TEST_ASSERT_EQUAL(1, replies(c.socket.sent).size());
    // Not allowed: closes, refuses new ones.
    c.link.setAllowed(false);
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::NotAllowed, c.link.lastClose());
    TEST_ASSERT_EQUAL(RpcTcpLink::Accept::NotAllowed, c.link.offer(IP_A, 0));
    c.link.setAllowed(true);
    TEST_ASSERT_EQUAL(RpcTcpLink::Accept::Accepted, c.link.offer(IP_A, 0));
    c.link.peerClosed();
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::Peer, c.link.lastClose());
    // Without a log name nothing is logged (the portmapper on TCP).
    TEST_ASSERT_EQUAL_STRING("", logText().c_str());
}

// rpcbind GETADDR (versions 3 and 4) as libtirpc sends it: rpcb {prog, vers, netid, addr, owner}.
static Bytes rpcbGetaddr(uint32_t vers) {
    Bytes a;
    put32(a, Vxi11::CORE_PROG);
    put32(a, 1);
    putOpaque(a, "tcp");
    putOpaque(a, "");
    putOpaque(a, "");
    return pmapCall(3, a, vers);
}

static void test_portmap_tcp_pool() {
    // Two links of the portmapper on TCP, as in vxi11_server.cpp.
    PortmapProgram program(CORE_PORT);
    FakeSocket sock[2];
    uint8_t in[2][512], out[2][128];
    RpcTcpLink a(program, sock[0], in[0], sizeof(in[0]), out[0], sizeof(out[0]), PORTMAP_TCP_IDLE_MS, nullptr);
    RpcTcpLink b(program, sock[1], in[1], sizeof(in[1]), out[1], sizeof(out[1]), PORTMAP_TCP_IDLE_MS, nullptr);
    RpcTcpLink* pool[] = {&a, &b};
    // Not allowed: nobody gets a link.
    TEST_ASSERT_EQUAL(2, RpcTcpLink::offerPool(pool, 2, IP_A, 0));
    TEST_ASSERT_FALSE(a.active() || b.active());
    a.setAllowed(true);
    b.setAllowed(true);
    // lxi-tools (libtirpc): connection 1 asks rpcbind v4 and v3 and stays open...
    TEST_ASSERT_EQUAL(0, RpcTcpLink::offerPool(pool, 2, IP_A, 100));
    for (uint32_t v = 4; v >= 3; --v) {
        const Bytes r = record(rpcbGetaddr(v));
        a.receive(r.data(), r.size(), 100);
        std::vector<Reply> rs = replies(sock[0].sent);
        TEST_ASSERT_EQUAL(1, rs.size());
        TEST_ASSERT_EQUAL_UINT32(OncRpc::PROG_MISMATCH, rs[0].accept);
        TEST_ASSERT_EQUAL_UINT32(2, rs[0].words[0]);
        TEST_ASSERT_EQUAL_UINT32(2, rs[0].words[1]);
    }
    // ...while connection 2 asks GETPORT version 2: it must get a link of its own.
    TEST_ASSERT_EQUAL(1, RpcTcpLink::offerPool(pool, 2, IP_A, 110));
    TEST_ASSERT_TRUE(a.active());
    const Bytes g = record(getport(Vxi11::CORE_PROG, 1, 6));
    b.receive(g.data(), g.size(), 120);
    std::vector<Reply> rs = replies(sock[1].sent);
    TEST_ASSERT_EQUAL(1, rs.size());
    TEST_ASSERT_EQUAL_UINT32(CORE_PORT, rs[0].words[0]);
    // A third connection replaces the link idle the longest (a: last record at 100), so an
    // idle client cannot lock the portmapper.
    TEST_ASSERT_EQUAL(0, RpcTcpLink::offerPool(pool, 2, IP_B, 5000));
    TEST_ASSERT_EQUAL(1, sock[0].closes);
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::Replaced, a.lastClose());
    TEST_ASSERT_EQUAL_HEX32(IP_B, a.clientIp());
    TEST_ASSERT_TRUE(b.active());
    // Then b (record at 120) is the idlest.
    TEST_ASSERT_EQUAL(1, RpcTcpLink::offerPool(pool, 2, IP_A, 5001));
    TEST_ASSERT_EQUAL(1, sock[1].closes);
    // A free link is taken before any is replaced.
    a.peerClosed();
    TEST_ASSERT_EQUAL(0, RpcTcpLink::offerPool(pool, 2, IP_A, 5002));
    TEST_ASSERT_EQUAL(1, sock[1].closes);
    // Not allowed again: both close, no link.
    a.setAllowed(false);
    b.setAllowed(false);
    TEST_ASSERT_EQUAL(2, RpcTcpLink::offerPool(pool, 2, IP_A, 6000));
    TEST_ASSERT_EQUAL(0, RpcTcpLink::offerPool(nullptr, 0, IP_A, 6000));
}

// ---------------------------------------------------------------------------
// VXI-11 core channel

static void test_core_create_link_and_idn() {
    Device d;
    d.connect();
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] VXI-11 client 192.168.1.20 connected\n"));
    Reply r = d.createLink();
    TEST_ASSERT_EQUAL_UINT32(0, r.accept);
    TEST_ASSERT_EQUAL(4, r.words.size());
    TEST_ASSERT_EQUAL_UINT32(0, r.words[0]);
    TEST_ASSERT_TRUE(r.words[1] != 0);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[2]); // no abort channel
    TEST_ASSERT_EQUAL_UINT32(1024, r.words[3]);
    TEST_ASSERT_TRUE(d.core.linked());
    TEST_ASSERT_EQUAL_STRING("CYD,HF-Generator,0123456789AB," APP_VERSION "\n", d.query("*IDN?\n").c_str());
    // A setting; the source is SCPI; contents never reach the log.
    Reply w = d.write("FREQ 7.1MHZ\n");
    TEST_ASSERT_EQUAL_UINT32(0, w.words[0]);
    TEST_ASSERT_EQUAL_UINT32(12, w.words[1]);
    TEST_ASSERT_EQUAL_UINT64(7100000, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL(Source::Scpi, d.app.lastRemote());
    TEST_ASSERT_EQUAL_STRING("7100000\n", d.query("FREQ?\n").c_str());
    TEST_ASSERT_FALSE(contains(logText(), "FREQ"));
    // destroy_link ends the link, the connection stays.
    r = d.linkOnly(Vxi11::DESTROY_LINK);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[0]);
    TEST_ASSERT_FALSE(d.core.linked());
    TEST_ASSERT_TRUE(d.link.active());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_LINK, d.write("*IDN?\n").words[0]);
}

static void test_core_link_rules() {
    Device d;
    d.connect();
    // Device name inst0 in any case; others are an invalid address.
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_ADDRESS, d.createLink("gpib0,1").words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_ADDRESS, d.createLink("inst1").words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_ADDRESS, d.createLink("").words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, d.createLink("INST0").words[0]);
    const uint32_t first = d.lid;
    // One link per connection.
    Reply r = d.createLink();
    TEST_ASSERT_EQUAL_UINT32(Vxi11::OUT_OF_RESOURCES, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[1]);
    TEST_ASSERT_EQUAL_UINT32(1024, r.words[3]);
    d.lid = first;
    // A wrong link id.
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_LINK, d.write("*IDN?\n", Vxi11::FLAG_END, first + 7).words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_LINK, d.generic(Vxi11::DEVICE_CLEAR, first + 7).words[0]);
    r = d.generic(Vxi11::DEVICE_READSTB, first + 7);
    TEST_ASSERT_EQUAL(2, r.words.size());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_LINK, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::INVALID_LINK, d.linkOnly(Vxi11::DESTROY_LINK, first + 7).words[0]);
    TEST_ASSERT_TRUE(d.core.linked());
    // The connection closes: the link ends with it; a new connection gets a new id.
    d.link.peerClosed();
    TEST_ASSERT_FALSE(d.core.linked());
    TEST_ASSERT_TRUE(contains(logText(), "[SCPI] VXI-11 client 192.168.1.20 closed\n"));
    d.connect(IP_B);
    TEST_ASSERT_EQUAL_UINT32(0, d.createLink().words[0]);
    TEST_ASSERT_TRUE(d.lid != first);
    // A long device name is garbage.
    Bytes a;
    put32(a, 1);
    put32(a, 0);
    put32(a, 0);
    putOpaque(a, std::string(65, 'i'));
    d.link.peerClosed();
    d.connect();
    TEST_ASSERT_EQUAL_UINT32(4, d.rpc(Vxi11::CREATE_LINK, a).accept);
    // lockDevice must be an XDR bool.
    Bytes b;
    put32(b, 1);
    put32(b, 2);
    put32(b, 0);
    putOpaque(b, "inst0");
    TEST_ASSERT_EQUAL_UINT32(4, d.rpc(Vxi11::CREATE_LINK, b).accept);
    TEST_ASSERT_FALSE(d.core.linked());
}

static void test_core_write_end_and_framing() {
    Device d;
    d.connect();
    d.createLink();
    // A message in two writes, the first without END: nothing runs before the end.
    TEST_ASSERT_EQUAL_UINT32(0, d.write("FREQ 3.5", 0).words[0]);
    TEST_ASSERT_EQUAL_UINT64(0, d.core.pending());
    TEST_ASSERT_EQUAL_UINT32(0, d.write("MHZ;:FREQ?", Vxi11::FLAG_END).words[0]); // END without LF
    TEST_ASSERT_EQUAL_UINT64(3500000, d.app.channel(1).frequency);
    TEST_ASSERT_EQUAL_STRING("3500000\n", readData(d.read()).c_str());
    TEST_ASSERT_EQUAL_STRING("3500000\n", d.query("FREQ?").c_str());
    // LF and END together end the line once.
    TEST_ASSERT_EQUAL_STRING("1\n", d.query("*OPC?\n").c_str());
    TEST_ASSERT_EQUAL_STRING("0,\"No error\"\n", d.query("SYST:ERR?\r\n").c_str());
    // Framing limits of SCPI over TCP (W-S17): an overlong line is -363.
    d.write(std::string(300, 'A') + "\n");
    TEST_ASSERT_EQUAL_STRING("-363,\"Input buffer overrun\"\n", d.query("SYST:ERR?\n").c_str());
    d.write(std::string("*IDN?\x01\n", 7));
    TEST_ASSERT_EQUAL_STRING("-101,\"Invalid character\"\n", d.query("SYST:ERR?\n").c_str());
    // Two queries on two lines of one write: two answer messages, each read up to its END.
    d.write("FREQ?\n*OPC?\n");
    Reply r = d.read();
    TEST_ASSERT_EQUAL_STRING("3500000\n", readData(r).c_str());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_END, r.words[1]);
    r = d.read();
    TEST_ASSERT_EQUAL_STRING("1\n", readData(r).c_str());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_END, r.words[1]);
    // Nothing waiting: I/O timeout at once.
    r = d.read();
    TEST_ASSERT_EQUAL_UINT32(Vxi11::IO_TIMEOUT, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[1]);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[2]);
    // Data beyond maxRecvSize is still taken.
    std::string many;
    while (many.size() < 1500) many += "*OPC\n";
    Reply w = d.write(many);
    TEST_ASSERT_EQUAL_UINT32(0, w.words[0]);
    TEST_ASSERT_EQUAL_UINT32(many.size(), w.words[1]);
}

static void test_core_read_sizes_and_termchar() {
    Device d;
    d.connect();
    d.createLink();
    d.write("*IDN?\n");
    // requestSize 4: pieces with REQCNT, the last one with END.
    std::string all;
    for (int i = 0; i < 40; ++i) {
        Reply r = d.read(4);
        TEST_ASSERT_EQUAL_UINT32(0, r.words[0]);
        const std::string piece = readData(r);
        TEST_ASSERT_TRUE(piece.size() <= 4);
        all += piece;
        if (r.words[1] & Vxi11::REASON_END) break;
        TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_REQCNT, r.words[1]);
    }
    TEST_ASSERT_EQUAL_STRING("CYD,HF-Generator,0123456789AB," APP_VERSION "\n", all.c_str());
    // termChar ',' with TERMCHRSET stops after it (CHR); without the flag it is ignored.
    d.write("*IDN?\n");
    Reply r = d.read(0x100000, Vxi11::FLAG_TERMCHRSET, ',');
    TEST_ASSERT_EQUAL_STRING("CYD,", readData(r).c_str());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_CHR, r.words[1]);
    r = d.read(0x100000, 0, ',');
    TEST_ASSERT_EQUAL_STRING("HF-Generator,0123456789AB," APP_VERSION "\n", readData(r).c_str());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_END, r.words[1]);
    // termChar LF: END and CHR together; requestSize exactly the answer: REQCNT as well.
    d.write("*OPC?\n");
    r = d.read(2, Vxi11::FLAG_TERMCHRSET, '\n');
    TEST_ASSERT_EQUAL_STRING("1\n", readData(r).c_str());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_END | Vxi11::REASON_CHR | Vxi11::REASON_REQCNT, r.words[1]);
    // requestSize 0: no data, REQCNT, the answer stays.
    d.write("*OPC?\n");
    r = d.read(0);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::REASON_REQCNT, r.words[1]);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[2]);
    TEST_ASSERT_EQUAL(2, d.core.pending());
    // A long answer (several lines of one write) comes in reads of at most MAX_READ bytes.
    std::string q;
    for (int i = 0; i < 15; ++i) q += i ? ";*IDN?" : "*IDN?";
    d.generic(Vxi11::DEVICE_CLEAR);
    d.write(q + "\n" + q + "\n");
    size_t total = 0;
    for (int i = 0; i < 10 && d.core.pending(); ++i) {
        r = d.read();
        TEST_ASSERT_TRUE(readData(r).size() <= Vxi11Core::MAX_READ);
        total += readData(r).size();
    }
    TEST_ASSERT_EQUAL(2 * 15 * (strlen("CYD,HF-Generator,0123456789AB," APP_VERSION) + 1), total);
}

static void test_core_unread_answer_and_overflow() {
    Device d;
    d.connect();
    d.createLink();
    // A new message while an answer is unread: -410, the old answer is gone.
    d.write("FREQ?\n");
    d.write("*OPC?\n");
    Reply r = d.read();
    TEST_ASSERT_EQUAL_STRING("1\n", readData(r).c_str());
    TEST_ASSERT_EQUAL_STRING("-410,\"Query INTERRUPTED\"\n", d.query("SYST:ERR?\n").c_str());
    // A message continued without END keeps the answers of its earlier lines.
    d.write("*OPC?\n", 0);
    d.write("*OPC?\n", Vxi11::FLAG_END);
    TEST_ASSERT_EQUAL(4, d.core.pending());
    d.generic(Vxi11::DEVICE_CLEAR);
    // Answers that do not fit the buffer: the line's answers dropped, -225.
    std::string q;
    for (int i = 0; i < 15; ++i) q += i ? ";*IDN?" : "*IDN?";
    const size_t perLine = 15 * (strlen("CYD,HF-Generator,0123456789AB," APP_VERSION) + 1);
    const size_t fit = Vxi11Core::ANSWER_BUFFER / perLine;
    std::string msg;
    for (size_t i = 0; i <= fit; ++i) msg += q + "\n";
    d.write(msg);
    TEST_ASSERT_EQUAL(fit * perLine, d.core.pending());
    d.generic(Vxi11::DEVICE_CLEAR);
    TEST_ASSERT_EQUAL_STRING("-225,\"Out of memory\"\n", d.query("SYST:ERR?\n").c_str());
}

static void test_core_status_clear_lock() {
    Device d;
    d.connect();
    d.createLink();
    // readstb: MAV while an answer waits; ESB after an error with *ESE.
    Reply r = d.generic(Vxi11::DEVICE_READSTB);
    TEST_ASSERT_EQUAL(2, r.words.size());
    TEST_ASSERT_EQUAL_UINT32(0, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[1]);
    d.write("*OPC?\n");
    TEST_ASSERT_EQUAL_UINT32(0x10, d.generic(Vxi11::DEVICE_READSTB).words[1]);
    // device_clear drops the answer and a partial line, keeps the error queue.
    d.write("FREQ 1", 0);
    TEST_ASSERT_EQUAL_UINT32(0, d.generic(Vxi11::DEVICE_CLEAR).words[0]);
    TEST_ASSERT_EQUAL(0, d.core.pending());
    d.write("FOO\n");
    d.write("MHZ\n"); // not "FREQ 1MHZ": the partial line is gone
    TEST_ASSERT_TRUE(d.app.channel(1).frequency != 1000000);
    d.generic(Vxi11::DEVICE_CLEAR);
    // -410 (the *OPC? answer was not read before "FREQ 1"), then -113 twice.
    TEST_ASSERT_EQUAL_STRING("3\n", d.query("SYST:ERR:COUN?\n").c_str());
    TEST_ASSERT_EQUAL_STRING("-410,\"Query INTERRUPTED\"\n", d.query("SYST:ERR?\n").c_str());
    d.write("*CLS\n");
    d.write("*ESE 32\n");
    d.write("BAD\n");
    TEST_ASSERT_EQUAL_UINT32(0x04 | 0x20, d.generic(Vxi11::DEVICE_READSTB).words[1] & 0x24);
    // Lock and unlock; unlock without a lock is error 12.
    TEST_ASSERT_EQUAL_UINT32(Vxi11::NO_LOCK_HELD, d.linkOnly(Vxi11::DEVICE_UNLOCK).words[0]);
    Bytes a;
    put32(a, d.lid);
    put32(a, Vxi11::FLAG_WAITLOCK);
    put32(a, 1000);
    TEST_ASSERT_EQUAL_UINT32(0, d.rpc(Vxi11::DEVICE_LOCK, a).words[0]);
    TEST_ASSERT_TRUE(d.core.locked());
    TEST_ASSERT_EQUAL_UINT32(0, d.rpc(Vxi11::DEVICE_LOCK, a).words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, d.linkOnly(Vxi11::DEVICE_UNLOCK).words[0]);
    TEST_ASSERT_FALSE(d.core.locked());
    // create_link with lockDevice holds the lock.
    d.linkOnly(Vxi11::DESTROY_LINK);
    d.createLink("inst0", true);
    TEST_ASSERT_TRUE(d.core.locked());
    // A new link starts with cleared status (error queue empty).
    TEST_ASSERT_EQUAL_STRING("0\n", d.query("SYST:ERR:COUN?\n").c_str());
    // remote/local: no-ops; trigger and the interrupt channel: not supported.
    TEST_ASSERT_EQUAL_UINT32(0, d.generic(Vxi11::DEVICE_REMOTE).words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, d.generic(Vxi11::DEVICE_LOCAL).words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::NOT_SUPPORTED, d.generic(Vxi11::DEVICE_TRIGGER).words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::NOT_SUPPORTED, d.rpc(Vxi11::DEVICE_ENABLE_SRQ, Bytes()).words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::NOT_SUPPORTED, d.rpc(Vxi11::CREATE_INTR_CHAN, Bytes(20, 0)).words[0]);
    TEST_ASSERT_EQUAL_UINT32(Vxi11::NOT_SUPPORTED, d.rpc(Vxi11::DESTROY_INTR_CHAN, Bytes()).words[0]);
    r = d.rpc(Vxi11::DEVICE_DOCMD, Bytes(32, 0));
    TEST_ASSERT_EQUAL(2, r.words.size());
    TEST_ASSERT_EQUAL_UINT32(Vxi11::NOT_SUPPORTED, r.words[0]);
    TEST_ASSERT_EQUAL_UINT32(0, r.words[1]);
}

static void test_core_rpc_errors() {
    Device d;
    d.connect();
    d.createLink();
    // NULL procedure; unknown procedures: PROC_UNAVAIL (device_abort lives on the abort program).
    TEST_ASSERT_EQUAL_UINT32(0, d.rpc(0, Bytes()).accept);
    TEST_ASSERT_EQUAL(0, d.rpc(0, Bytes()).words.size());
    const uint32_t procs[] = {1, 9, 21, 24, 27, 0xFFFFFFFFu};
    for (uint32_t p : procs) TEST_ASSERT_EQUAL_UINT32(3, d.rpc(p, Bytes(32, 0)).accept);
    // Truncated arguments of every procedure: GARBAGE_ARGS, the link stays.
    const uint32_t withArgs[] = {Vxi11::CREATE_LINK, Vxi11::DEVICE_WRITE, Vxi11::DEVICE_READ, Vxi11::DEVICE_READSTB,
                                 Vxi11::DEVICE_TRIGGER, Vxi11::DEVICE_CLEAR, Vxi11::DEVICE_REMOTE, Vxi11::DEVICE_LOCAL,
                                 Vxi11::DEVICE_LOCK, Vxi11::DEVICE_UNLOCK, Vxi11::DESTROY_LINK};
    for (uint32_t p : withArgs) TEST_ASSERT_EQUAL_UINT32(4, d.rpc(p, Bytes(0)).accept);
    // device_write whose data length exceeds the record.
    Bytes a;
    put32(a, d.lid);
    put32(a, 0);
    put32(a, 0);
    put32(a, Vxi11::FLAG_END);
    put32(a, 100);
    a.insert(a.end(), 8, 'x');
    TEST_ASSERT_EQUAL_UINT32(4, d.rpc(Vxi11::DEVICE_WRITE, a).accept);
    TEST_ASSERT_TRUE(d.core.linked());
    // Another version or program on the core port.
    CallSpec s;
    s.vers = 2;
    Bytes r = record(call(s));
    d.link.receive(r.data(), r.size(), 0);
    std::vector<Reply> rs = replies(d.socket.sent);
    TEST_ASSERT_EQUAL_UINT32(2, rs[0].accept);
    TEST_ASSERT_EQUAL_UINT32(1, rs[0].words[0]);
    TEST_ASSERT_EQUAL_UINT32(1, rs[0].words[1]);
    r = record(getport(Vxi11::CORE_PROG, 1, 6));
    d.link.receive(r.data(), r.size(), 0);
    TEST_ASSERT_EQUAL_UINT32(1, replies(d.socket.sent)[0].accept);
    // Refused credentials on the core port as well.
    s.vers = 1;
    s.cred = 3;
    r = record(call(s));
    d.link.receive(r.data(), r.size(), 0);
    TEST_ASSERT_EQUAL_UINT32(1, replies(d.socket.sent)[0].stat);
    // A full record of the largest size the link takes.
    Bytes big;
    put32(big, d.lid);
    put32(big, 0);
    put32(big, 0);
    put32(big, Vxi11::FLAG_END);
    putOpaque(big, std::string(Vxi11Core::MAX_RECORD - 40 - 20, ' '));
    Bytes m = coreCall(Vxi11::DEVICE_WRITE, big);
    TEST_ASSERT_EQUAL(Vxi11Core::MAX_RECORD, m.size());
    r = record(m);
    d.link.receive(r.data(), r.size(), 0);
    TEST_ASSERT_EQUAL_UINT32(0, replies(d.socket.sent)[0].words[0]);
    m.push_back(0);
    m.push_back(0);
    m.push_back(0);
    m.push_back(0);
    r = record(m);
    d.link.receive(r.data(), r.size(), 0);
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::TooLarge, d.link.lastClose());
}

static void test_core_remote_outputs_local_only() {
    Device d;
    d.connect();
    d.createLink();
    TEST_ASSERT_EQUAL(Result::Ok, d.app.setRemoteOutputsAllowed(false));
    d.write("OUTP1 ON\n");
    TEST_ASSERT_FALSE(d.app.channel(1).enabled);
    TEST_ASSERT_EQUAL_STRING("-221,\"Settings conflict;Remote outputs: local only\"\n", d.query("SYST:ERR?\n").c_str());
}

static void test_core_idle_and_not_allowed() {
    Device d;
    d.connect();
    d.createLink();
    d.link.update(SCPI_IDLE_TIMEOUT_MS - 1);
    TEST_ASSERT_TRUE(d.link.active());
    d.link.update(SCPI_IDLE_TIMEOUT_MS);
    TEST_ASSERT_EQUAL(RpcTcpLink::Close::Idle, d.link.lastClose());
    TEST_ASSERT_FALSE(d.core.linked());
    TEST_ASSERT_TRUE(contains(logText(), "VXI-11 client 192.168.1.20 closed (idle)\n"));
    d.connect();
    d.createLink();
    d.link.setAllowed(false);
    TEST_ASSERT_FALSE(d.core.linked());
    TEST_ASSERT_TRUE(contains(logText(), "closed (WiFi)\n"));
    d.link.setAllowed(true);
    d.connect();
    d.link.disconnect();
    TEST_ASSERT_TRUE(contains(logText(), "closed (disconnect)\n"));
}

// ---------------------------------------------------------------------------
// LXI identification document

static void test_lxi_identification() {
    LxiIdent::Info i;
    i.serial = "0123456789AB";
    i.version = "2.0.0";
    i.name = "cyd-hf-1a2b";
    i.ip = 0xC0A80114;
    i.mask = 0xFFFFFF00;
    i.gateway = 0xC0A80101;
    i.dhcp = false;
    char buf[LxiIdent::DOCUMENT_SIZE];
    const size_t n = LxiIdent::build(i, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    TEST_ASSERT_EQUAL(strlen(buf), n);
    const std::string x(buf);
    TEST_ASSERT_TRUE(contains(x, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<LXIDevice xmlns=\"http://www.lxistandard.org/InstrumentIdentification/1.0\""));
    TEST_ASSERT_TRUE(contains(x, "<Manufacturer>DM8SB</Manufacturer>\n<Model>CYD HF Generator</Model>\n"
                                 "<SerialNumber>0123456789AB</SerialNumber>\n<FirmwareRevision>2.0.0</FirmwareRevision>\n"));
    TEST_ASSERT_TRUE(contains(x, "<IdentificationURL>http://192.168.1.20/lxi/identification</IdentificationURL>"));
    TEST_ASSERT_TRUE(contains(x, "<InstrumentAddressString>TCPIP::192.168.1.20::INSTR</InstrumentAddressString>"));
    TEST_ASSERT_TRUE(contains(x, "<InstrumentAddressString>TCPIP::192.168.1.20::5025::SOCKET</InstrumentAddressString>"));
    TEST_ASSERT_TRUE(contains(x, "<Hostname>cyd-hf-1a2b.local</Hostname>\n<IPAddress>192.168.1.20</IPAddress>\n"
                                 "<SubnetMask>255.255.255.0</SubnetMask>\n<Gateway>192.168.1.1</Gateway>\n"
                                 "<DHCPEnabled>false</DHCPEnabled>"));
    TEST_ASSERT_TRUE(contains(x, "</Interface>\n</LXIDevice>\n"));
    TEST_ASSERT_FALSE(contains(x, "MACAddress"));
    // Every size up to the length is too small; nothing is written past outSize.
    for (size_t cap = 0; cap <= n; ++cap) {
        std::vector<char> small(cap + 8, '#');
        TEST_ASSERT_EQUAL(0, LxiIdent::build(i, small.data(), cap));
        for (size_t k = cap; k < small.size(); ++k) TEST_ASSERT_EQUAL('#', small[k]);
    }
    std::vector<char> exact(n + 1);
    TEST_ASSERT_EQUAL(n, LxiIdent::build(i, exact.data(), exact.size()));
    // Markup in a value is escaped, control characters dropped; null strings are empty.
    i.name = "a<b>&\"c'\x01";
    i.version = nullptr;
    i.serial = nullptr;
    TEST_ASSERT_TRUE(LxiIdent::build(i, buf, sizeof(buf)) > 0);
    const std::string y(buf);
    TEST_ASSERT_TRUE(contains(y, "<UserDescription>a&lt;b&gt;&amp;&quot;c&apos;</UserDescription>"));
    TEST_ASSERT_TRUE(contains(y, "<FirmwareRevision></FirmwareRevision>"));
    TEST_ASSERT_TRUE(contains(y, "<SerialNumber>0</SerialNumber>"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_lxi_identification);
    RUN_TEST(test_xdr_reader);
    RUN_TEST(test_xdr_writer);
    RUN_TEST(test_rpc_header_refusals);
    RUN_TEST(test_portmap_getport);
    RUN_TEST(test_portmap_lxi_discover_request);
    RUN_TEST(test_portmap_other_procedures);
    RUN_TEST(test_udp_budget);
    RUN_TEST(test_record_fragments_and_byte_by_byte);
    RUN_TEST(test_record_too_large_closes);
    RUN_TEST(test_record_partial_send_and_failures);
    RUN_TEST(test_record_link_lifecycle);
    RUN_TEST(test_portmap_tcp_pool);
    RUN_TEST(test_core_create_link_and_idn);
    RUN_TEST(test_core_link_rules);
    RUN_TEST(test_core_write_end_and_framing);
    RUN_TEST(test_core_read_sizes_and_termchar);
    RUN_TEST(test_core_unread_answer_and_overflow);
    RUN_TEST(test_core_status_clear_lock);
    RUN_TEST(test_core_rpc_errors);
    RUN_TEST(test_core_remote_outputs_local_only);
    RUN_TEST(test_core_idle_and_not_allowed);
    return UNITY_END();
}
