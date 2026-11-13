// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "line_framer.h"
#include "onc_rpc.h"
#include "scpi.h"

// VXI-11 (VXIbus TCP/IP Instrument Protocol 1.0) and its portmapper for LXI discovery
// (A-78, E-37). Pure logic, host-tested (test_vxi11) and fuzzed (fuzz_vxi11); the sockets are
// in vxi11_server.* (device glue). Like SCPI over TCP it runs in loop() through the
// LoopScpiBackend with Source::Scpi, and it has no authentication (W-E12): access only while
// ScpiTcpLink::allowedFor() (never over an open AP), `Remote outputs: local only` applies.

namespace Vxi11 {

static constexpr uint32_t CORE_PROG = 0x0607AF;  // DEVICE_CORE
static constexpr uint32_t CORE_VERS = 1;
static constexpr uint32_t IPPROTO_TCP_NUM = 6;
static constexpr uint32_t IPPROTO_UDP_NUM = 17;

enum Proc : uint32_t {
    NULLPROC = 0,
    CREATE_LINK = 10,
    DEVICE_WRITE = 11,
    DEVICE_READ = 12,
    DEVICE_READSTB = 13,
    DEVICE_TRIGGER = 14,
    DEVICE_CLEAR = 15,
    DEVICE_REMOTE = 16,
    DEVICE_LOCAL = 17,
    DEVICE_LOCK = 18,
    DEVICE_UNLOCK = 19,
    DEVICE_ENABLE_SRQ = 20,
    DEVICE_DOCMD = 22,
    DESTROY_LINK = 23,
    CREATE_INTR_CHAN = 25,
    DESTROY_INTR_CHAN = 26,
};

// Device_ErrorCode (VXI-11 B.5.1).
enum Error : uint32_t {
    NO_ERROR = 0,
    SYNTAX_ERROR = 1,
    INVALID_LINK = 4,
    PARAMETER_ERROR = 5,
    NOT_SUPPORTED = 8,
    OUT_OF_RESOURCES = 9,
    NO_LOCK_HELD = 12,
    IO_TIMEOUT = 15,
    INVALID_ADDRESS = 21,
};

// Device_Flags and the reason bits of device_read.
static constexpr uint32_t FLAG_WAITLOCK = 0x01;
static constexpr uint32_t FLAG_END = 0x08;
static constexpr uint32_t FLAG_TERMCHRSET = 0x80;
static constexpr uint32_t REASON_REQCNT = 0x01;
static constexpr uint32_t REASON_CHR = 0x02;
static constexpr uint32_t REASON_END = 0x04;

} // namespace Vxi11

// Portmapper (program 100000 version 2, RFC 1833) on UDP and TCP 111: NULL, GETPORT and, on
// TCP only, DUMP (rpcinfo -p). GETPORT knows the VXI-11 core channel (0x0607AF v1 TCP) and the
// portmapper itself; every other mapping is port 0. SET, UNSET and CALLIT are refused
// (PROC_UNAVAIL; CALLIT would make the device a UDP reflector), rpcbind versions 3 and 4 get
// PROG_MISMATCH 2..2 so that a client falls back to version 2.
namespace Portmap {

static constexpr uint32_t PROG = 100000;
static constexpr uint32_t VERS = 2;
static constexpr uint16_t PORT = 111;
enum Proc : uint32_t { NULLPROC = 0, SET = 1, UNSET = 2, GETPORT = 3, DUMP = 4, CALLIT = 5 };
enum class Transport : uint8_t { Tcp, Udp };

// One call; returns the reply length in out (0: no reply). corePort 0: the core channel is
// not listening (GETPORT gives 0). Over UDP a reply longer than the request is not sent.
size_t handle(const uint8_t* msg, size_t len, Transport t, uint16_t corePort, uint8_t* out, size_t cap);

// Replies over UDP per second at most (broadcast discovery from many hosts, spoofed sources).
class UdpBudget {
public:
    static constexpr uint8_t PER_SECOND = 20;
    bool take(uint32_t nowMs);

private:
    uint32_t windowMs_ = 0;
    uint8_t used_ = 0;
    bool started_ = false;
};

} // namespace Portmap

// The portmapper as the program of a TCP connection.
class PortmapProgram : public RpcProgram {
public:
    explicit PortmapProgram(uint16_t corePort) : corePort_(corePort) {}
    void setCorePort(uint16_t port) { corePort_ = port; }
    size_t call(const uint8_t* msg, size_t len, uint8_t* out, size_t cap) override {
        return Portmap::handle(msg, len, Portmap::Transport::Tcp, corePort_, out, cap);
    }

private:
    uint16_t corePort_;
};

// The VXI-11 core channel (program 0x0607AF version 1) of one TCP connection, mapped onto an
// own ScpiSession (Source::Scpi; error queue and status registers cleared per link).
// - One link per connection (create_link with "inst0", any case; a second one: error 9).
// - device_write feeds the bytes through the LineFramer (W-S17); the END flag ends a line
//   without LF. Answers are collected (ANSWER_BUFFER); a new message while an answer is
//   unread discards it with -410 (IEEE 488.2 query interrupted); answers of one line that do
//   not fit are dropped with -225.
// - device_read returns at most MAX_READ bytes and never more than one answer message (up to
//   its LF, reason END), stops at termChar with FLAG_TERMCHRSET (reason CHR) or after
//   requestSize (REQCNT). Nothing waiting: error 15 at once, since commands run synchronously
//   and no answer can arrive later; io_timeout and lock_timeout are not needed otherwise.
// - device_readstb: the status byte with MAV while an answer waits. device_clear (IEEE 488.2
//   DCL): input, unread answers and a partial line are dropped; error queue and status
//   registers stay (*CLS clears them). device_lock/unlock with one link always succeed (12
//   without a lock). device_remote/local do nothing (error 0).
// - Not supported (error 8): device_trigger (no trigger system), device_docmd,
//   device_enable_srq, create_intr_chan/destroy_intr_chan (no interrupt channel). No abort
//   channel: create_link announces abortPort 0.
class Vxi11Core : public RpcProgram {
public:
    static constexpr uint32_t MAX_RECV = 1024;      // maxRecvSize announced by create_link
    static constexpr size_t ANSWER_BUFFER = 2048;   // answers waiting for device_read
    static constexpr size_t MAX_READ = 1024;        // data per device_read reply
    static constexpr size_t MAX_DEVICE_NAME = 64;
    // Input record limit and reply buffer for the TCP link: a full device_write with the
    // largest credentials fits, and the largest device_read reply with its record mark.
    static constexpr size_t MAX_RECORD = 2048;
    static constexpr size_t MAX_REPLY = 4 + 24 + 12 + MAX_READ;

    Vxi11Core(ScpiBackend& backend, const char* serial = nullptr);
    void setSerial(const char* serial) { session_.setSerial(serial); }

    size_t call(const uint8_t* msg, size_t len, uint8_t* out, size_t cap) override;
    void connectionChanged(bool open) override;

    bool linked() const { return linked_; }
    uint32_t linkId() const { return linked_ ? lid_ : 0; }
    bool locked() const { return linked_ && locked_; }
    size_t pending() const { return answerLen_; }
    ScpiSession& session() { return session_; }

private:
    static void write_(const char* data, size_t len, void* ctx);
    void endLink_();
    void line_(const char* line, size_t len);
    void feed_(const uint8_t* data, size_t len, bool end);
    uint32_t read_(uint32_t requestSize, uint32_t flags, uint32_t termChar, Xdr::Writer& w, size_t room);

    ScpiSession session_;
    LineFramer framer_;
    bool linked_ = false;
    bool locked_ = false;
    bool inMessage_ = false; // the last device_write had no END: the message goes on
    bool overflow_ = false;
    uint32_t lid_ = 0;
    uint32_t nextLid_ = 1;
    char answer_[ANSWER_BUFFER] {};
    size_t answerLen_ = 0;
};
