// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "onc_rpc.h"

#include <stdio.h>
#include <string.h>
#include <utils/logger.h>

// ---------------------------------------------------------------------------
// XDR

namespace Xdr {

bool Reader::u32(uint32_t* out) {
    if (!ok_ || remaining() < 4) return fail_();
    const uint8_t* b = p_ + pos_;
    *out = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    pos_ += 4;
    return true;
}

bool Reader::i32(int32_t* out) {
    uint32_t v = 0;
    if (!u32(&v)) return false;
    *out = (int32_t)v;
    return true;
}

bool Reader::boolean(bool* out) {
    uint32_t v = 0;
    if (!u32(&v)) return false;
    if (v > 1) return fail_();
    *out = v == 1;
    return true;
}

bool Reader::opaque(const uint8_t** data, uint32_t* len, uint32_t max) {
    uint32_t n = 0;
    if (!u32(&n)) return false;
    // n first: n + 3 must not wrap (size_t is 32 bits on the ESP32).
    if (n > max || n > remaining()) return fail_();
    const size_t padded = ((size_t)n + 3) & ~(size_t)3;
    if (padded > remaining()) return fail_();
    *data = p_ + pos_;
    *len = n;
    pos_ += padded;
    return true;
}

void Writer::u32(uint32_t v) {
    if (!ok_ || room() < 4) {
        ok_ = false;
        return;
    }
    uint8_t* b = p_ + pos_;
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)v;
    pos_ += 4;
}

void Writer::opaque(const uint8_t* data, uint32_t len) {
    if (!ok_ || room() < 4 || len > room() - 4 || (((size_t)len + 3) & ~(size_t)3) > room() - 4) {
        ok_ = false;
        return;
    }
    const size_t padded = ((size_t)len + 3) & ~(size_t)3;
    u32(len);
    if (len) memcpy(p_ + pos_, data, len);
    memset(p_ + pos_ + len, 0, padded - len);
    pos_ += padded;
}

} // namespace Xdr

// ---------------------------------------------------------------------------
// RPC messages

namespace OncRpc {

namespace {
void denied(Xdr::Writer& w, uint32_t xid, RejectStat stat) {
    w.u32(xid);
    w.u32(MSG_REPLY);
    w.u32(MSG_DENIED);
    w.u32(stat);
}
}

Parsed parseCall(const uint8_t* msg, size_t len, Call* call, Xdr::Writer* reply) {
    if (!msg || !call || !reply) return Parsed::Drop;
    Xdr::Reader r(msg, len);
    uint32_t xid = 0, type = 0, rpcvers = 0;
    if (!r.u32(&xid) || !r.u32(&type) || type != MSG_CALL || !r.u32(&rpcvers)) return Parsed::Drop;
    if (rpcvers != RPC_VERSION) {
        denied(*reply, xid, RPC_MISMATCH);
        reply->u32(RPC_VERSION);
        reply->u32(RPC_VERSION);
        return Parsed::Reply;
    }
    Call c;
    c.xid = xid;
    if (!r.u32(&c.prog) || !r.u32(&c.vers) || !r.u32(&c.proc)) return Parsed::Drop;
    uint32_t credFlavor = 0, verfFlavor = 0, n = 0;
    const uint8_t* body = nullptr;
    if (!r.u32(&credFlavor)) return Parsed::Drop;
    if (!r.opaque(&body, &n, MAX_AUTH_BYTES)) {
        denied(*reply, xid, AUTH_ERROR);
        reply->u32(AUTH_BADCRED);
        return Parsed::Reply;
    }
    if (!r.u32(&verfFlavor) || !r.opaque(&body, &n, MAX_AUTH_BYTES)) {
        denied(*reply, xid, AUTH_ERROR);
        reply->u32(AUTH_BADVERF);
        return Parsed::Reply;
    }
    if (credFlavor != AUTH_NONE && credFlavor != AUTH_SYS) {
        denied(*reply, xid, AUTH_ERROR);
        reply->u32(AUTH_REJECTEDCRED);
        return Parsed::Reply;
    }
    if (verfFlavor != AUTH_NONE) {
        denied(*reply, xid, AUTH_ERROR);
        reply->u32(AUTH_REJECTEDVERF);
        return Parsed::Reply;
    }
    c.args = r;
    *call = c;
    return Parsed::Call;
}

void accepted(Xdr::Writer& w, uint32_t xid, AcceptStat stat) {
    w.u32(xid);
    w.u32(MSG_REPLY);
    w.u32(MSG_ACCEPTED);
    w.u32(AUTH_NONE); // verifier
    w.u32(0);
    w.u32(stat);
}

void progMismatch(Xdr::Writer& w, uint32_t xid, uint32_t low, uint32_t high) {
    accepted(w, xid, PROG_MISMATCH);
    w.u32(low);
    w.u32(high);
}

} // namespace OncRpc

// ---------------------------------------------------------------------------
// Record marking over TCP

namespace {
const char* closeText(RpcTcpLink::Close why) {
    switch (why) {
        case RpcTcpLink::Close::Idle: return "closed (idle)";
        case RpcTcpLink::Close::Local: return "closed (disconnect)";
        case RpcTcpLink::Close::NotAllowed: return "closed (WiFi)";
        case RpcTcpLink::Close::SendFailed: return "closed (send failed)";
        case RpcTcpLink::Close::TooLarge: return "closed (record too large)";
        case RpcTcpLink::Close::Overflow: return "closed (not reading)";
        case RpcTcpLink::Close::Replaced: return "closed (replaced)";
        default: return "closed";
    }
}
constexpr uint32_t LAST_FRAGMENT = 0x80000000u;
}

RpcTcpLink::RpcTcpLink(RpcProgram& program, ScpiTcpSocket& socket, uint8_t* in, size_t inCap, uint8_t* out,
                       size_t outCap, uint32_t idleMs, const char* logName)
    : program_(program), socket_(socket), in_(in), inCap_(inCap), out_(out), outCap_(outCap), idleMs_(idleMs),
      logName_(logName) {}

void RpcTcpLink::log_(uint32_t ip, const char* what) const {
    if (!logName_) return;
    char line[80];
    snprintf(line, sizeof(line), "%s client %u.%u.%u.%u %s", logName_, (unsigned)(ip >> 24),
             (unsigned)((ip >> 16) & 0xFF), (unsigned)((ip >> 8) & 0xFF), (unsigned)(ip & 0xFF), what);
    Logger::info("SCPI", line);
}

void RpcTcpLink::setAllowed(bool allowed) {
    allowed_ = allowed;
    if (!allowed) close_(Close::NotAllowed);
}

RpcTcpLink::Accept RpcTcpLink::offer(uint32_t ip, uint32_t nowMs) {
    if (!allowed_) return Accept::NotAllowed;
    if (active_) {
        log_(ip, "refused (busy)");
        return Accept::Busy;
    }
    active_ = true;
    ip_ = ip;
    lastRecordMs_ = nowMs;
    headerLen_ = 0;
    inFragment_ = false;
    lastFragment_ = false;
    fragmentLeft_ = 0;
    recordLen_ = 0;
    outLen_ = 0;
    program_.connectionChanged(true);
    log_(ip, "connected");
    return Accept::Accepted;
}

size_t RpcTcpLink::offerPool(RpcTcpLink* const* links, size_t n, uint32_t ip, uint32_t nowMs) {
    if (!links || n == 0) return n;
    for (size_t i = 0; i < n; ++i) {
        if (!links[i]->allowed_) return n;
    }
    size_t pick = n;
    uint32_t idlest = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!links[i]->active_) {
            pick = i;
            break;
        }
        const uint32_t idle = nowMs - links[i]->lastRecordMs_;
        if (pick == n || idle > idlest) {
            pick = i;
            idlest = idle;
        }
    }
    links[pick]->close_(Close::Replaced); // no-op for a free link
    return links[pick]->offer(ip, nowMs) == Accept::Accepted ? pick : n;
}

size_t RpcTcpLink::wantBytes() const {
    if (!wantsInput()) return 0;
    return inFragment_ ? fragmentLeft_ : (size_t)(4 - headerLen_);
}

void RpcTcpLink::receive(const uint8_t* data, size_t len, uint32_t nowMs) {
    size_t i = 0;
    while (i < len && active_) {
        if (!inFragment_) {
            header_[headerLen_++] = data[i++];
            if (headerLen_ < 4) continue;
            headerLen_ = 0;
            const uint32_t h = (uint32_t)header_[0] << 24 | (uint32_t)header_[1] << 16 | (uint32_t)header_[2] << 8 | header_[3];
            lastFragment_ = (h & LAST_FRAGMENT) != 0;
            fragmentLeft_ = h & ~LAST_FRAGMENT;
            if (fragmentLeft_ > inCap_ - recordLen_) {
                close_(Close::TooLarge);
                return;
            }
            inFragment_ = true;
        } else {
            size_t take = len - i;
            if (take > fragmentLeft_) take = fragmentLeft_;
            memcpy(in_ + recordLen_, data + i, take);
            recordLen_ += take;
            fragmentLeft_ -= take;
            i += take;
        }
        if (inFragment_ && fragmentLeft_ == 0) {
            inFragment_ = false;
            if (lastFragment_) record_(nowMs);
        }
    }
}

void RpcTcpLink::record_(uint32_t nowMs) {
    lastRecordMs_ = nowMs;
    const size_t len = recordLen_;
    recordLen_ = 0;
    if (outCap_ - outLen_ <= 4) {
        close_(Close::Overflow); // the glue read past a record while a reply was waiting
        return;
    }
    const size_t n = program_.call(in_, len, out_ + outLen_ + 4, outCap_ - outLen_ - 4);
    if (!active_ || n == 0) return;
    const uint32_t mark = LAST_FRAGMENT | (uint32_t)n;
    uint8_t* h = out_ + outLen_;
    h[0] = (uint8_t)(mark >> 24);
    h[1] = (uint8_t)(mark >> 16);
    h[2] = (uint8_t)(mark >> 8);
    h[3] = (uint8_t)mark;
    outLen_ += 4 + n;
    flush_();
}

void RpcTcpLink::update(uint32_t nowMs) {
    if (!active_) return;
    flush_();
    if (active_ && nowMs - lastRecordMs_ >= idleMs_) close_(Close::Idle);
}

void RpcTcpLink::flush_() {
    if (!active_ || outLen_ == 0) return;
    const int32_t n = socket_.trySend((const char*)out_, outLen_);
    if (n < 0) {
        close_(Close::SendFailed);
        return;
    }
    const size_t sent = (size_t)n < outLen_ ? (size_t)n : outLen_;
    if (sent == 0) return;
    memmove(out_, out_ + sent, outLen_ - sent);
    outLen_ -= sent;
}

void RpcTcpLink::peerClosed() { close_(Close::Peer); }

void RpcTcpLink::disconnect() { close_(Close::Local); }

void RpcTcpLink::close_(Close why) {
    if (!active_) return;
    socket_.closeConnection();
    active_ = false;
    outLen_ = 0;
    recordLen_ = 0;
    headerLen_ = 0;
    inFragment_ = false;
    lastClose_ = why;
    program_.connectionChanged(false);
    log_(ip_, closeText(why));
}
