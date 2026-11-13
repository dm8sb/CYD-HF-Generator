// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "scpi_tcp.h"

// ONC RPC version 2 (RFC 5531, formerly 1831) with XDR (RFC 4506, formerly 1832) for VXI-11
// and its portmapper (A-78, E-37). Pure logic without Arduino or lwIP: host-tested
// (test_vxi11) and fuzzed (fuzz_vxi11). Only what a server of fixed programs needs: decode a
// call, encode the reply; record marking on TCP in RpcTcpLink.

namespace Xdr {

// Bounded big-endian reader; every failed read leaves ok() false and returns false.
class Reader {
public:
    Reader() {}
    Reader(const uint8_t* data, size_t len) : p_(data), n_(len) {}
    bool u32(uint32_t* out);
    bool i32(int32_t* out);
    // XDR bool: exactly 0 or 1.
    bool boolean(bool* out);
    // Variable-length opaque or string: length, data, padding to 4 bytes. More than max bytes,
    // or more than the input holds, fails.
    bool opaque(const uint8_t** data, uint32_t* len, uint32_t max);
    bool ok() const { return ok_; }
    size_t remaining() const { return n_ - pos_; }

private:
    bool fail_() {
        ok_ = false;
        return false;
    }
    const uint8_t* p_ = nullptr;
    size_t n_ = 0;
    size_t pos_ = 0;
    bool ok_ = true;
};

// Bounded writer; an overflow leaves ok() false and writes nothing more.
class Writer {
public:
    Writer(uint8_t* buf, size_t cap) : p_(buf), cap_(cap) {}
    void u32(uint32_t v);
    void opaque(const uint8_t* data, uint32_t len);
    bool ok() const { return ok_; }
    size_t length() const { return pos_; }
    size_t room() const { return cap_ - pos_; }

private:
    uint8_t* p_;
    size_t cap_;
    size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace Xdr

namespace OncRpc {

static constexpr uint32_t RPC_VERSION = 2;
static constexpr uint32_t MSG_CALL = 0;
static constexpr uint32_t MSG_REPLY = 1;
static constexpr uint32_t MSG_ACCEPTED = 0;
static constexpr uint32_t MSG_DENIED = 1;
static constexpr uint32_t AUTH_NONE = 0;
static constexpr uint32_t AUTH_SYS = 1; // AUTH_UNIX
static constexpr uint32_t MAX_AUTH_BYTES = 400;

enum AcceptStat : uint32_t {
    SUCCESS = 0,
    PROG_UNAVAIL = 1,
    PROG_MISMATCH = 2,
    PROC_UNAVAIL = 3,
    GARBAGE_ARGS = 4,
    SYSTEM_ERR = 5,
};
enum RejectStat : uint32_t { RPC_MISMATCH = 0, AUTH_ERROR = 1 };
enum AuthStat : uint32_t { AUTH_BADCRED = 1, AUTH_REJECTEDCRED = 2, AUTH_BADVERF = 3, AUTH_REJECTEDVERF = 4 };

struct Call {
    uint32_t xid = 0;
    uint32_t prog = 0;
    uint32_t vers = 0;
    uint32_t proc = 0;
    Xdr::Reader args; // the procedure's arguments (trailing bytes after them are ignored)
};

enum class Parsed : uint8_t {
    Call,  // a call with acceptable credentials: decode the arguments, answer
    Reply, // refused (RPC version, credentials): the reply is in the writer
    Drop,  // no call (a reply, truncated header): no answer at all
};
// Decodes the call header of one RPC message. Accepted credentials: AUTH_NONE and AUTH_SYS
// (body not interpreted, at most 400 bytes) with an AUTH_NONE verifier; another flavor gives
// AUTH_ERROR, an RPC version other than 2 RPC_MISMATCH 2..2.
Parsed parseCall(const uint8_t* msg, size_t len, Call* call, Xdr::Writer* reply);

// Reply header MSG_ACCEPTED with an AUTH_NONE verifier and the status; SUCCESS is followed by
// the results, PROG_MISMATCH by low/high (progMismatch writes both).
void accepted(Xdr::Writer& w, uint32_t xid, AcceptStat stat);
void progMismatch(Xdr::Writer& w, uint32_t xid, uint32_t low, uint32_t high);

} // namespace OncRpc

// One RPC program behind a TCP connection (portmapper, VXI-11 core channel).
class RpcProgram {
public:
    virtual ~RpcProgram() {}
    // One complete record (an RPC message). The reply goes to out (at most cap bytes); returns
    // its length, 0 for none.
    virtual size_t call(const uint8_t* msg, size_t len, uint8_t* out, size_t cap) = 0;
    // A connection was opened or closed: state per connection (the VXI-11 link) ends with it.
    virtual void connectionChanged(bool open) { (void)open; }
};

// ONC RPC over TCP with record marking (RFC 5531 section 11): a 4-byte header per fragment
// (bit 31 = last fragment, 31-bit length), fragments joined into one record. Pure logic like
// ScpiTcpLink, the socket comes from the glue (ScpiTcpSocket).
// - One connection; a second one is refused while one is active (the glue closes it). A
//   stateless program (the portmapper) uses several links through offerPool() instead.
// - A record larger than the input buffer closes the connection (TooLarge): it cannot be
//   skipped safely, and an HTTP request ("POST" read as a length of 1.3 GB) ends here.
// - idleMs without a complete record closes it.
// - The reply of a record is sent at once, what the stack does not take is kept; while some is
//   waiting no input is read. The glue reads at most wantBytes() so that a read never goes past
//   the end of a record: there is never more than one reply waiting.
// Log lines (when logName is set) name the client address and the reason, never contents.
class RpcTcpLink {
public:
    enum class Accept : uint8_t { Accepted, Busy, NotAllowed };
    enum class Close : uint8_t { Peer, Idle, Local, NotAllowed, SendFailed, TooLarge, Overflow, Replaced };

    RpcTcpLink(RpcProgram& program, ScpiTcpSocket& socket, uint8_t* in, size_t inCap, uint8_t* out, size_t outCap,
               uint32_t idleMs, const char* logName);

    void setAllowed(bool allowed);
    bool allowed() const { return allowed_; }
    Accept offer(uint32_t ip, uint32_t nowMs);
    // A new connection for one of n links of a stateless program (the portmapper on TCP): a
    // free link takes it, otherwise the link idle the longest is closed for it (Replaced).
    // libtirpc (lxi-tools) keeps its rpcbind v4/v3 connection open while it opens a second one
    // for the version 2 GETPORT, and an idle client must not lock the others out. Returns the
    // index of the link that accepted, n when not allowed.
    static size_t offerPool(RpcTcpLink* const* links, size_t n, uint32_t ip, uint32_t nowMs);
    // Bytes the glue may read now: up to the end of the current fragment header or fragment.
    size_t wantBytes() const;
    void receive(const uint8_t* data, size_t len, uint32_t nowMs);
    void update(uint32_t nowMs);
    void peerClosed();
    void disconnect();

    bool active() const { return active_; }
    bool wantsInput() const { return active_ && outLen_ == 0; }
    uint32_t clientIp() const { return active_ ? ip_ : 0; }
    Close lastClose() const { return lastClose_; }

private:
    void record_(uint32_t nowMs);
    void flush_();
    void close_(Close why);
    void log_(uint32_t ip, const char* what) const;

    RpcProgram& program_;
    ScpiTcpSocket& socket_;
    uint8_t* in_;
    size_t inCap_;
    uint8_t* out_;
    size_t outCap_;
    uint32_t idleMs_;
    const char* logName_;
    bool allowed_ = false;
    bool active_ = false;
    Close lastClose_ = Close::Peer;
    uint32_t ip_ = 0;
    uint32_t lastRecordMs_ = 0;
    // Record marking state.
    uint8_t header_[4] {};
    uint8_t headerLen_ = 0;
    bool inFragment_ = false;
    bool lastFragment_ = false;
    size_t fragmentLeft_ = 0;
    size_t recordLen_ = 0;
    size_t outLen_ = 0;
};
