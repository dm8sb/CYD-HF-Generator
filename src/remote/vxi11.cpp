// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vxi11.h"

#include <string.h>

using OncRpc::accepted;
using OncRpc::progMismatch;

// ---------------------------------------------------------------------------
// Portmapper

namespace Portmap {

namespace {
// The mappings this device has: (prog, vers, prot, port).
struct Mapping {
    uint32_t prog, vers, prot, port;
};

size_t mappings(uint16_t corePort, Mapping* out) {
    size_t n = 0;
    out[n++] = Mapping {PROG, VERS, Vxi11::IPPROTO_TCP_NUM, PORT};
    out[n++] = Mapping {PROG, VERS, Vxi11::IPPROTO_UDP_NUM, PORT};
    if (corePort) out[n++] = Mapping {Vxi11::CORE_PROG, Vxi11::CORE_VERS, Vxi11::IPPROTO_TCP_NUM, corePort};
    return n;
}

size_t reply(const uint8_t* msg, size_t len, Transport t, uint16_t corePort, uint8_t* out, size_t cap) {
    Xdr::Writer w(out, cap);
    OncRpc::Call c;
    switch (OncRpc::parseCall(msg, len, &c, &w)) {
        case OncRpc::Parsed::Drop: return 0;
        case OncRpc::Parsed::Reply: return w.ok() ? w.length() : 0;
        case OncRpc::Parsed::Call: break;
    }
    if (c.prog != PROG) {
        accepted(w, c.xid, OncRpc::PROG_UNAVAIL);
    } else if (c.vers != VERS) {
        progMismatch(w, c.xid, VERS, VERS);
    } else if (c.proc == NULLPROC) {
        accepted(w, c.xid, OncRpc::SUCCESS);
    } else if (c.proc == GETPORT) {
        uint32_t prog = 0, vers = 0, prot = 0, port = 0;
        if (!c.args.u32(&prog) || !c.args.u32(&vers) || !c.args.u32(&prot) || !c.args.u32(&port)) {
            accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
        } else {
            Mapping m[3];
            const size_t n = mappings(corePort, m);
            uint32_t found = 0;
            for (size_t i = 0; i < n; ++i) {
                if (m[i].prog == prog && m[i].vers == vers && m[i].prot == prot) found = m[i].port;
            }
            accepted(w, c.xid, OncRpc::SUCCESS);
            w.u32(found);
        }
    } else if (c.proc == DUMP && t == Transport::Tcp) {
        // Over UDP the list would be larger than the request (reflection).
        Mapping m[3];
        const size_t n = mappings(corePort, m);
        accepted(w, c.xid, OncRpc::SUCCESS);
        for (size_t i = 0; i < n; ++i) {
            w.u32(1); // value follows
            w.u32(m[i].prog);
            w.u32(m[i].vers);
            w.u32(m[i].prot);
            w.u32(m[i].port);
        }
        w.u32(0);
    } else {
        accepted(w, c.xid, OncRpc::PROC_UNAVAIL);
    }
    return w.ok() ? w.length() : 0;
}
}

size_t handle(const uint8_t* msg, size_t len, Transport t, uint16_t corePort, uint8_t* out, size_t cap) {
    if (!out) return 0;
    const size_t n = reply(msg, len, t, corePort, out, cap);
    // Over UDP never more than came in: the source address may be forged (reflection).
    return t == Transport::Udp && n > len ? 0 : n;
}

bool UdpBudget::take(uint32_t nowMs) {
    if (!started_ || nowMs - windowMs_ >= 1000) {
        started_ = true;
        windowMs_ = nowMs;
        used_ = 0;
    }
    if (used_ >= PER_SECOND) return false;
    ++used_;
    return true;
}

} // namespace Portmap

// ---------------------------------------------------------------------------
// VXI-11 core channel

namespace {
char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

bool isInst0(const uint8_t* name, uint32_t len) {
    static const char INST0[] = "inst0";
    if (len != sizeof(INST0) - 1) return false;
    for (uint32_t i = 0; i < len; ++i) {
        if (lower((char)name[i]) != INST0[i]) return false;
    }
    return true;
}

void deviceError(Xdr::Writer& w, uint32_t xid, uint32_t error) {
    accepted(w, xid, OncRpc::SUCCESS);
    w.u32(error);
}

constexpr int16_t QUERY_INTERRUPTED = -410;
constexpr int16_t OUT_OF_MEMORY = -225;
}

Vxi11Core::Vxi11Core(ScpiBackend& backend, const char* serial) : session_(backend, Source::Scpi, serial) {
    session_.setWriter(write_, this);
}

void Vxi11Core::write_(const char* data, size_t len, void* ctx) {
    Vxi11Core* self = static_cast<Vxi11Core*>(ctx);
    if (self->overflow_ || len > ANSWER_BUFFER - self->answerLen_) {
        self->overflow_ = true;
        return;
    }
    memcpy(self->answer_ + self->answerLen_, data, len);
    self->answerLen_ += len;
}

void Vxi11Core::connectionChanged(bool open) {
    (void)open;
    endLink_(); // a link lives only as long as its connection
}

void Vxi11Core::endLink_() {
    linked_ = false;
    locked_ = false;
    inMessage_ = false;
    answerLen_ = 0;
    overflow_ = false;
    framer_.reset();
}

void Vxi11Core::line_(const char* line, size_t len) {
    const size_t before = answerLen_;
    overflow_ = false;
    session_.execute(line, len);
    if (overflow_) {
        answerLen_ = before; // no partial answer of this line
        overflow_ = false;
        session_.pushError(OUT_OF_MEMORY);
    }
}

void Vxi11Core::feed_(const uint8_t* data, size_t len, bool end) {
    for (size_t i = 0; i <= len; ++i) {
        LineFramer::Event e;
        if (i < len) {
            e = framer_.feed((char)data[i]);
        } else {
            // END terminates the message like LF; a message that already ended in LF adds nothing.
            if (!end || (len > 0 && data[len - 1] == '\n')) break;
            e = framer_.feed('\n');
        }
        if (e == LineFramer::Event::Line) line_(framer_.line(), framer_.length());
        else if (e != LineFramer::Event::None) session_.pushError(framerErrorCode(e));
    }
}

uint32_t Vxi11Core::read_(uint32_t requestSize, uint32_t flags, uint32_t termChar, Xdr::Writer& w, size_t room) {
    // Data that fits: error, reason, length (12 bytes) and the padded data.
    size_t n = answerLen_;
    if (n > requestSize) n = requestSize;
    if (n > MAX_READ) n = MAX_READ;
    const size_t fit = room >= 12 ? (room - 12) & ~(size_t)3 : 0;
    if (n > fit) n = fit;
    // One answer message per read: up to and including its LF.
    size_t messageEnd = answerLen_;
    const void* lf = memchr(answer_, '\n', answerLen_);
    if (lf) messageEnd = (size_t)((const char*)lf - answer_) + 1;
    if (n > messageEnd) n = messageEnd;
    uint32_t reason = 0;
    if (flags & Vxi11::FLAG_TERMCHRSET) {
        const void* t = memchr(answer_, (int)(termChar & 0xFF), n);
        if (t) {
            n = (size_t)((const char*)t - answer_) + 1;
            reason |= Vxi11::REASON_CHR;
        }
    }
    if (n == messageEnd) reason |= Vxi11::REASON_END;
    if (n == requestSize) reason |= Vxi11::REASON_REQCNT;
    w.u32(Vxi11::NO_ERROR);
    w.u32(reason);
    w.opaque((const uint8_t*)answer_, (uint32_t)n);
    if (!w.ok()) return 0;
    memmove(answer_, answer_ + n, answerLen_ - n);
    answerLen_ -= n;
    return (uint32_t)n;
}

size_t Vxi11Core::call(const uint8_t* msg, size_t len, uint8_t* out, size_t cap) {
    if (!out) return 0;
    Xdr::Writer w(out, cap);
    OncRpc::Call c;
    switch (OncRpc::parseCall(msg, len, &c, &w)) {
        case OncRpc::Parsed::Drop: return 0;
        case OncRpc::Parsed::Reply: return w.ok() ? w.length() : 0;
        case OncRpc::Parsed::Call: break;
    }
    if (c.prog != Vxi11::CORE_PROG) {
        accepted(w, c.xid, OncRpc::PROG_UNAVAIL);
        return w.ok() ? w.length() : 0;
    }
    if (c.vers != Vxi11::CORE_VERS) {
        progMismatch(w, c.xid, Vxi11::CORE_VERS, Vxi11::CORE_VERS);
        return w.ok() ? w.length() : 0;
    }
    Xdr::Reader& a = c.args;
    uint32_t lid = 0, flags = 0, lockTimeout = 0, ioTimeout = 0;
    // Device_Link first in every argument structure except create_link's.
    auto linkOk = [&]() { return linked_ && lid == lid_; };
    switch (c.proc) {
        case Vxi11::NULLPROC: accepted(w, c.xid, OncRpc::SUCCESS); break;

        case Vxi11::CREATE_LINK: {
            int32_t clientId = 0;
            bool lockDevice = false;
            const uint8_t* name = nullptr;
            uint32_t nameLen = 0;
            if (!a.i32(&clientId) || !a.boolean(&lockDevice) || !a.u32(&lockTimeout) ||
                !a.opaque(&name, &nameLen, MAX_DEVICE_NAME)) {
                accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
                break;
            }
            uint32_t error = Vxi11::NO_ERROR;
            if (!isInst0(name, nameLen)) error = Vxi11::INVALID_ADDRESS;
            else if (linked_) error = Vxi11::OUT_OF_RESOURCES;
            if (error == Vxi11::NO_ERROR) {
                endLink_();
                linked_ = true;
                locked_ = lockDevice;
                lid_ = nextLid_++;
                if (nextLid_ == 0) nextLid_ = 1;
                session_.clearStatus(); // nothing of the previous link remains
            }
            accepted(w, c.xid, OncRpc::SUCCESS);
            w.u32(error);
            w.u32(error == Vxi11::NO_ERROR ? lid_ : 0);
            w.u32(0); // abortPort: no abort channel
            w.u32(MAX_RECV);
            break;
        }

        case Vxi11::DEVICE_WRITE: {
            const uint8_t* data = nullptr;
            uint32_t dataLen = 0;
            if (!a.u32(&lid) || !a.u32(&ioTimeout) || !a.u32(&lockTimeout) || !a.u32(&flags) ||
                !a.opaque(&data, &dataLen, 0xFFFFFFFFu)) {
                accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
                break;
            }
            accepted(w, c.xid, OncRpc::SUCCESS);
            if (!linkOk()) {
                w.u32(Vxi11::INVALID_LINK);
                w.u32(0);
                break;
            }
            // A new message while an answer is unread (IEEE 488.2 6.3.2.3).
            if (!inMessage_ && answerLen_ > 0) {
                answerLen_ = 0;
                session_.pushError(QUERY_INTERRUPTED);
            }
            const bool end = (flags & Vxi11::FLAG_END) != 0;
            feed_(data, dataLen, end);
            inMessage_ = !end;
            w.u32(Vxi11::NO_ERROR);
            w.u32(dataLen);
            break;
        }

        case Vxi11::DEVICE_READ: {
            uint32_t requestSize = 0, termChar = 0;
            if (!a.u32(&lid) || !a.u32(&requestSize) || !a.u32(&ioTimeout) || !a.u32(&lockTimeout) ||
                !a.u32(&flags) || !a.u32(&termChar)) {
                accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
                break;
            }
            accepted(w, c.xid, OncRpc::SUCCESS);
            if (!linkOk() || answerLen_ == 0) {
                w.u32(!linkOk() ? Vxi11::INVALID_LINK : Vxi11::IO_TIMEOUT);
                w.u32(0);
                w.u32(0); // no data
                break;
            }
            read_(requestSize, flags, termChar, w, w.room());
            break;
        }

        case Vxi11::DEVICE_READSTB:
        case Vxi11::DEVICE_TRIGGER:
        case Vxi11::DEVICE_CLEAR:
        case Vxi11::DEVICE_REMOTE:
        case Vxi11::DEVICE_LOCAL: {
            // Device_GenericParms
            if (!a.u32(&lid) || !a.u32(&flags) || !a.u32(&lockTimeout) || !a.u32(&ioTimeout)) {
                accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
                break;
            }
            if (!linkOk()) {
                deviceError(w, c.xid, Vxi11::INVALID_LINK);
                if (c.proc == Vxi11::DEVICE_READSTB) w.u32(0);
                break;
            }
            if (c.proc == Vxi11::DEVICE_READSTB) {
                const uint8_t stb = (uint8_t)(session_.statusByte() | (answerLen_ ? STB_MAV : 0));
                deviceError(w, c.xid, Vxi11::NO_ERROR);
                w.u32(stb);
            } else if (c.proc == Vxi11::DEVICE_TRIGGER) {
                deviceError(w, c.xid, Vxi11::NOT_SUPPORTED);
            } else {
                if (c.proc == Vxi11::DEVICE_CLEAR) {
                    framer_.reset();
                    answerLen_ = 0;
                    inMessage_ = false;
                }
                deviceError(w, c.xid, Vxi11::NO_ERROR);
            }
            break;
        }

        case Vxi11::DEVICE_LOCK: {
            if (!a.u32(&lid) || !a.u32(&flags) || !a.u32(&lockTimeout)) {
                accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
                break;
            }
            if (linkOk()) locked_ = true;
            deviceError(w, c.xid, linkOk() ? Vxi11::NO_ERROR : Vxi11::INVALID_LINK);
            break;
        }

        case Vxi11::DEVICE_UNLOCK:
        case Vxi11::DESTROY_LINK: {
            if (!a.u32(&lid)) {
                accepted(w, c.xid, OncRpc::GARBAGE_ARGS);
                break;
            }
            uint32_t error = Vxi11::NO_ERROR;
            if (!linkOk()) {
                error = Vxi11::INVALID_LINK;
            } else if (c.proc == Vxi11::DESTROY_LINK) {
                endLink_();
            } else if (!locked_) {
                error = Vxi11::NO_LOCK_HELD;
            } else {
                locked_ = false;
            }
            deviceError(w, c.xid, error);
            break;
        }

        case Vxi11::DEVICE_ENABLE_SRQ:
        case Vxi11::CREATE_INTR_CHAN:
        case Vxi11::DESTROY_INTR_CHAN: deviceError(w, c.xid, Vxi11::NOT_SUPPORTED); break;

        case Vxi11::DEVICE_DOCMD:
            deviceError(w, c.xid, Vxi11::NOT_SUPPORTED);
            w.u32(0); // data_out<>
            break;

        default: accepted(w, c.xid, OncRpc::PROC_UNAVAIL); break;
    }
    return w.ok() ? w.length() : 0;
}
