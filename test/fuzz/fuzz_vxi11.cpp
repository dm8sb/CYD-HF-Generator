// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target vxi11: ONC RPC record marking (RpcTcpLink), the VXI-11 core channel (Vxi11Core
// with its ScpiSession behind the LineFramer) and the portmapper (Portmap::handle) as the device
// glue runs them (A-78, E-37).
// Input: 5 configuration bytes, then a script of operations.
//   byte 0: bit 0 the portmapper as the TCP program instead of the core channel, bit 1 the
//           socket takes only part of a reply, bit 2 a send may fail, bit 3 log lines
//   bytes 1-4: seed (fake backend outcomes, socket room)
//   then per operation: opcode, mutation byte, arguments from the input (see run()). The
//   harness encodes valid calls and corrupts them per the mutation byte (truncated arguments,
//   another link id, RPC version, program, version or credential flavor, fragment splits);
//   opcode 0 feeds raw bytes.
// The whole input is also handed to Portmap::handle as one datagram and one TCP record.
// Invariants: the glue contract holds (never more than wantBytes() read, one reply waiting);
// every reply is a whole record (last-fragment bit, length a multiple of 4, at most
// MAX_REPLY - 4), decodes as an RPC reply and answers the harness's calls in order; device_read
// data is at most MAX_READ and requestSize bytes of printable text and LF with reason bits only
// from REQCNT/CHR/END and at least one of them; at most ANSWER_BUFFER bytes wait; commands carry
// Source::Scpi; the error queue holds at most 16 entries; over UDP no reply is longer than its
// request, every reply echoes the xid.

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <remote/onc_rpc.h>
#include <remote/scpi.h>
#include <remote/vxi11.h>

namespace fuzz_vxi11 {

struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

class Backend : public ScpiBackend {
public:
    Rng rng {1};
    Outcome submit(const Command& c) override {
        FUZZ_CHECK(c.source == Source::Scpi);
        Outcome o;
        const uint32_t r = rng.next();
        o.push = (r & 15) == 0 ? PushResult::Full : (r & 15) == 1 ? PushResult::RateLimited : PushResult::Queued;
        o.result = (Result)((r >> 4) % 7);
        return o;
    }
    bool snapshot(Snapshot* out) override {
        *out = Snapshot {};
        out->state.ch1.frequency = 7000000;
        out->state.ch2.frequency = 10000000;
        out->state.sys.backlight = 80;
        out->state.sys.drive_ma = 8;
        out->state.sys.delta_step = 1000;
        return true;
    }
    void heap(uint32_t* f, uint32_t* m, uint32_t* l) override {
        *f = 100000;
        *m = 90000;
        *l = 60000;
    }
};

struct Socket : ScpiTcpSocket {
    std::vector<uint8_t> sent;
    bool partial = false;
    bool mayFail = false;
    Rng* rng = nullptr;
    int closes = 0;
    // The connection ends: its complete replies are checked, a partial one is dropped.
    void (*onClose)(void*) = nullptr;
    void* ctx = nullptr;
    int32_t trySend(const char* data, size_t len) override {
        const uint32_t r = rng->next();
        if (mayFail && (r & 63) == 0) return -1;
        size_t take = len;
        if (partial) take = (size_t)(r >> 8) % (len + 1);
        sent.insert(sent.end(), (const uint8_t*)data, (const uint8_t*)data + take);
        return (int32_t)take;
    }
    void closeConnection() override {
        ++closes;
        if (onClose) onClose(ctx);
        sent.clear();
    }
};

uint32_t get32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
void put32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((uint8_t)(v >> 24));
    b.push_back((uint8_t)(v >> 16));
    b.push_back((uint8_t)(v >> 8));
    b.push_back((uint8_t)v);
}
void putOpaque(std::vector<uint8_t>& b, const uint8_t* d, size_t n) {
    put32(b, (uint32_t)n);
    b.insert(b.end(), d, d + n);
    while (b.size() % 4) b.push_back(0);
}

// Reads the input front to back; past the end everything is 0.
struct Input {
    const uint8_t* d;
    size_t n;
    size_t pos;
    bool more() const { return pos < n; }
    uint8_t byte() { return pos < n ? d[pos++] : 0; }
    uint16_t word() { return (uint16_t)(byte() << 8 | byte()); }
    const uint8_t* take(size_t* len) {
        if (*len > n - pos) *len = n - pos;
        const uint8_t* p = d + pos;
        pos += *len;
        return p;
    }
};

const uint32_t XID_BASE = 0xA5000000u;

struct Harness {
    Backend backend;
    Socket socket;
    Vxi11Core core {backend, "0123456789AB"};
    PortmapProgram portmap {9009};
    uint8_t in[Vxi11Core::MAX_RECORD];
    uint8_t out[Vxi11Core::MAX_REPLY];
    RpcTcpLink* link = nullptr;
    bool coreProgram = true;
    uint32_t now = 0;
    uint32_t xid = XID_BASE;
    uint32_t lastReplied = 0;
    // device_read calls by xid: requestSize, so the reply can be checked.
    std::vector<std::pair<uint32_t, uint32_t> > reads;

    // Feeds like the glue: only while input is wanted, at most wantBytes() per read.
    void feed(const uint8_t* d, size_t n, uint32_t chunkSeed) {
        size_t i = 0;
        Rng r {chunkSeed | 1};
        while (i < n && link->active()) {
            if (!link->wantsInput()) {
                link->update(now);
                if (!link->wantsInput()) {
                    socket.partial = false; // the client reads again
                    link->update(now);
                    socket.partial = true;
                }
                if (!link->wantsInput()) return;
                continue;
            }
            const size_t want = link->wantBytes();
            FUZZ_CHECK(want >= 1);
            size_t len = 1 + r.next() % 160;
            if (len > want) len = want;
            if (len > n - i) len = n - i;
            link->receive(d + i, len, now);
            i += len;
            checkState();
        }
    }

    void checkState() {
        FUZZ_CHECK(core.pending() <= Vxi11Core::ANSWER_BUFFER);
        FUZZ_CHECK(core.session().errorCount() <= 16);
        if (core.linked()) FUZZ_CHECK(core.linkId() != 0);
    }

    // Sends one call as a record, split into fragments per the mutation byte.
    void send(const std::vector<uint8_t>& msg, uint8_t mutation) {
        std::vector<uint8_t> stream;
        size_t pos = 0;
        const unsigned splits = (mutation >> 4) & 3;
        for (unsigned k = 0; k <= splits; ++k) {
            const bool last = k == splits;
            const size_t part = last ? msg.size() - pos : (msg.size() - pos) / 2;
            put32(stream, (last ? 0x80000000u : 0) | (uint32_t)part);
            stream.insert(stream.end(), msg.begin() + pos, msg.begin() + pos + part);
            pos += part;
        }
        feed(stream.data(), stream.size(), mutation * 2654435761u);
    }

    std::vector<uint8_t> header(uint32_t prog, uint32_t vers, uint32_t proc, uint8_t mutation, uint8_t arg) {
        std::vector<uint8_t> b;
        put32(b, ++xid);
        put32(b, 0);
        const unsigned kind = (mutation & 3) == 3 ? arg % 6 : 99;
        put32(b, kind == 0 ? arg : 2);
        put32(b, kind == 1 ? prog ^ arg : prog);
        put32(b, kind == 2 ? vers + arg : vers);
        put32(b, proc);
        put32(b, kind == 3 ? arg % 8 : (kind == 4 ? 1 : 0));
        if (kind == 4) {
            put32(b, 20); // AUTH_SYS body (not interpreted)
            for (int i = 0; i < 5; ++i) put32(b, arg);
        } else {
            put32(b, 0);
        }
        put32(b, kind == 5 ? 1 : 0);
        put32(b, 0);
        return b;
    }

    void finish(std::vector<uint8_t>& msg, uint8_t mutation) {
        if ((mutation & 3) == 1) {
            const size_t cut = (mutation >> 2) % 16;
            msg.resize(msg.size() > cut ? msg.size() - cut : 0);
        }
        send(msg, mutation);
    }

    uint32_t lid(uint8_t mutation) const { return (mutation & 3) == 2 ? core.linkId() ^ mutation : core.linkId(); }

    // All replies sent so far.
    void checkReplies() {
        size_t at = 0;
        const std::vector<uint8_t>& s = socket.sent;
        while (s.size() - at >= 4) {
            const uint32_t h = get32(&s[at]);
            const size_t len = h & 0x7FFFFFFFu;
            FUZZ_CHECK(h & 0x80000000u);
            FUZZ_CHECK(len % 4 == 0 && len >= 12);
            FUZZ_CHECK(len <= Vxi11Core::MAX_REPLY - 4);
            if (s.size() - at - 4 < len) break; // the rest comes later
            const uint8_t* r = &s[at + 4];
            const uint32_t x = get32(r);
            FUZZ_CHECK(get32(r + 4) == 1); // REPLY
            if ((x & 0xFF000000u) == XID_BASE) {
                FUZZ_CHECK(x > lastReplied && x <= xid);
                lastReplied = x;
                checkRead(x, r, len);
            }
            at += 4 + len;
        }
        socket.sent.erase(socket.sent.begin(), socket.sent.begin() + at);
    }

    void checkRead(uint32_t x, const uint8_t* r, size_t len) {
        for (size_t i = 0; i < reads.size(); ++i) {
            if (reads[i].first != x) continue;
            if (get32(r + 8) != 0 || get32(r + 20) != 0 || len < 36) return; // refused or not SUCCESS
            const uint32_t error = get32(r + 24), reason = get32(r + 28), n = get32(r + 32);
            FUZZ_CHECK(36 + ((n + 3) & ~3u) == len);
            FUZZ_CHECK((reason & ~7u) == 0);
            if (error != 0) {
                FUZZ_CHECK(n == 0 && reason == 0);
                return;
            }
            FUZZ_CHECK(reason != 0);
            FUZZ_CHECK(n <= Vxi11Core::MAX_READ && n <= reads[i].second);
            for (uint32_t k = 0; k < n; ++k) {
                const uint8_t c = r[36 + k];
                FUZZ_CHECK((c >= 0x20 && c <= 0x7E) || c == '\n');
            }
            return;
        }
    }
};

void pmapDirect(const uint8_t* data, size_t size) {
    uint8_t out[256];
    for (int t = 0; t < 2; ++t) {
        const Portmap::Transport tr = t ? Portmap::Transport::Tcp : Portmap::Transport::Udp;
        const size_t n = Portmap::handle(data, size, tr, 9009, out, sizeof(out));
        FUZZ_CHECK(n <= sizeof(out) && n % 4 == 0);
        if (n == 0) continue;
        FUZZ_CHECK(size >= 12 && get32(out) == get32(data) && get32(out + 4) == 1);
        if (tr == Portmap::Transport::Udp) FUZZ_CHECK(n <= size);
    }
}

void closed(void* ctx) { static_cast<Harness*>(ctx)->checkReplies(); }

int run(const uint8_t* data, size_t size) {
    pmapDirect(data, size);
    if (size < 5) return 0;
    Input in {data, size, 0};
    const uint8_t cfg = in.byte();
    const uint32_t seed = (uint32_t)in.byte() << 24 | (uint32_t)in.byte() << 16 | (uint32_t)in.byte() << 8 | in.byte();

    Harness h;
    h.backend.rng.s = seed | 1;
    Rng socketRng {seed ^ 0x9E3779B9u};
    if (!socketRng.s) socketRng.s = 1;
    h.socket.rng = &socketRng;
    h.socket.onClose = closed;
    h.socket.ctx = &h;
    h.socket.partial = cfg & 2;
    h.socket.mayFail = cfg & 4;
    h.coreProgram = !(cfg & 1);
    RpcProgram& program = h.coreProgram ? (RpcProgram&)h.core : (RpcProgram&)h.portmap;
    const size_t inCap = h.coreProgram ? sizeof(h.in) : 512;
    const size_t outCap = h.coreProgram ? sizeof(h.out) : 128;
    RpcTcpLink link(program, h.socket, h.in, inCap, h.out, outCap, 600000, (cfg & 8) ? "VXI-11" : nullptr);
    h.link = &link;
    link.setAllowed(true);
    FUZZ_CHECK(link.offer(0x0A000001, 0) == RpcTcpLink::Accept::Accepted);

    const uint32_t CORE = Vxi11::CORE_PROG;
    while (in.more() && link.active()) {
        const uint8_t op = in.byte();
        const uint8_t m = in.byte();
        const uint8_t arg = in.byte();
        std::vector<uint8_t> msg;
        switch (op % 16) {
            case 0: { // raw bytes
                size_t n = arg;
                const uint8_t* p = in.take(&n);
                h.feed(p, n, m);
                break;
            }
            case 1: { // create_link
                msg = h.header(CORE, 1, Vxi11::CREATE_LINK, m, arg);
                put32(msg, arg);
                put32(msg, (m & 4) ? 1 : 0);
                put32(msg, 0);
                static const char* const NAMES[] = {"inst0", "INST0", "inst1", "", "gpib0,1"};
                if (arg % 6 < 5) {
                    putOpaque(msg, (const uint8_t*)NAMES[arg % 6], strlen(NAMES[arg % 6]));
                } else {
                    size_t n = in.byte() % 80;
                    const uint8_t* p = in.take(&n);
                    putOpaque(msg, p, n);
                }
                h.finish(msg, m);
                break;
            }
            case 2: { // device_write
                msg = h.header(CORE, 1, Vxi11::DEVICE_WRITE, m, arg);
                put32(msg, h.lid(m));
                put32(msg, 1000);
                put32(msg, 0);
                put32(msg, arg); // flags (END = 8)
                size_t n = in.word() % 1500;
                const uint8_t* p = in.take(&n);
                putOpaque(msg, p, n);
                h.finish(msg, m);
                break;
            }
            case 3: { // device_read
                msg = h.header(CORE, 1, Vxi11::DEVICE_READ, m, arg);
                const uint32_t requestSize = (m & 8) ? 0x100000 : in.word();
                put32(msg, h.lid(m));
                put32(msg, requestSize);
                put32(msg, 1000);
                put32(msg, 0);
                put32(msg, (m & 4) ? Vxi11::FLAG_TERMCHRSET : 0);
                put32(msg, arg);
                h.reads.push_back(std::make_pair(h.xid, requestSize));
                h.finish(msg, m);
                break;
            }
            case 4: case 5: case 6: case 7: case 8: { // readstb, trigger, clear, remote, local
                msg = h.header(CORE, 1, Vxi11::DEVICE_READSTB + (op % 16 - 4), m, arg);
                put32(msg, h.lid(m));
                put32(msg, 0);
                put32(msg, 0);
                put32(msg, 1000);
                h.finish(msg, m);
                break;
            }
            case 9: { // lock
                msg = h.header(CORE, 1, Vxi11::DEVICE_LOCK, m, arg);
                put32(msg, h.lid(m));
                put32(msg, arg);
                put32(msg, 0);
                h.finish(msg, m);
                break;
            }
            case 10: { // unlock or destroy_link
                msg = h.header(CORE, 1, (arg & 1) ? Vxi11::DESTROY_LINK : Vxi11::DEVICE_UNLOCK, m, arg);
                put32(msg, h.lid(m));
                h.finish(msg, m);
                break;
            }
            case 11: { // any procedure with any arguments
                msg = h.header(CORE, 1, arg, m, 0);
                size_t n = in.byte();
                const uint8_t* p = in.take(&n);
                msg.insert(msg.end(), p, p + n);
                h.finish(msg, m);
                break;
            }
            case 12: { // portmapper call
                msg = h.header(100000, 2, arg % 6, m, 0);
                put32(msg, (m & 4) ? CORE : 100000);
                put32(msg, (m >> 3) & 3);
                put32(msg, (m & 32) ? 17 : 6);
                put32(msg, 0);
                h.finish(msg, m);
                break;
            }
            case 13: // time passes
                h.now += (uint32_t)arg * 5000u;
                link.update(h.now);
                break;
            case 14: // the connection ends and a new one starts
                if (m & 1) link.peerClosed();
                else link.disconnect();
                FUZZ_CHECK(!h.core.linked());
                FUZZ_CHECK(link.offer(0x0A000002, h.now) == RpcTcpLink::Accept::Accepted);
                break;
            default: { // interrupt channel, SRQ, docmd
                static const uint32_t PROCS[] = {Vxi11::DEVICE_ENABLE_SRQ, Vxi11::DEVICE_DOCMD,
                                                 Vxi11::CREATE_INTR_CHAN, Vxi11::DESTROY_INTR_CHAN};
                msg = h.header(CORE, 1, PROCS[arg % 4], m, 0);
                for (int i = 0; i < arg % 8; ++i) put32(msg, arg);
                h.finish(msg, m);
                break;
            }
        }
        h.checkState();
        h.checkReplies();
    }
    // Let the client read everything.
    h.socket.partial = false;
    h.socket.mayFail = false;
    link.update(h.now);
    h.checkReplies();
    if (link.active()) FUZZ_CHECK(link.wantsInput());
    return 0;
}

} // namespace fuzz_vxi11

FUZZ_TARGET(vxi11, fuzz_vxi11::run)
