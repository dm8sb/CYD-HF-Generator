// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <scpi/scpi.h>
#include "command.h"
#include "line_framer.h"

// SCPI (W-E12, ARBEITSPLAN 3.5) on j123b567/scpi-parser: one ScpiSession per connection (USB
// serial, later TCP 5025). The session never touches the controller: it hands Commands to its
// backend and answers queries from the snapshot (V-6). Numbers are parsed here as exact
// integers (W-S19), not through the library's double conversion.

// What a session needs from its server.
class ScpiBackend {
public:
    struct Outcome {
        PushResult push; // Queued, or why the command was refused
        Result result;   // valid when Queued
    };
    virtual ~ScpiBackend() {}
    // Executes the command (synchronously or by waiting at most 1 s for its ticket).
    virtual Outcome submit(const Command& c) = 0;
    // The current state; false if none is available.
    virtual bool snapshot(Snapshot* out) = 0;
    // Free heap, its minimum since boot and the largest free block (SYSTem:HEAP?).
    virtual void heap(uint32_t* freeBytes, uint32_t* minFree, uint32_t* largest) = 0;
};

namespace ScpiNumber {
enum class Parse : uint8_t { Ok, Syntax, Range };
struct Unit {
    const char* name; // upper case
    int8_t exp10;     // value * 10^exp10
};
// "[+|-]digits[.digits][E[+|-]digits]", optional blanks, then one of `units` (any case; nullptr
// or none matching: no unit allowed). Gives value * 10^scaleExp as int64, rounded half away
// from zero. Range: does not fit into int64. Syntax: anything else (also > 18 significant digits).
Parse decimal(const char* s, size_t n, int8_t scaleExp, const Unit* units, int64_t* out);
// Frequency in Hz with HZ/KHZ/MHZ; Range outside FREQ_MIN_HZ..FREQ_MAX_HZ.
Parse frequency(const char* s, size_t n, uint64_t* hz);
}

class ScpiSession {
public:
    // Output of the session (answers end in LF); ctx is passed through.
    typedef void (*WriteFn)(const char* data, size_t len, void* ctx);

    // serial: *IDN? field 3 (MAC without separators), also settable later.
    ScpiSession(ScpiBackend& backend, Source source, const char* serial = nullptr);
    void setSerial(const char* serial);
    void setWriter(WriteFn fn, void* ctx) {
        writer_ = fn;
        writerCtx_ = ctx;
    }
    Source source() const { return source_; }
    // SYSTem:LOG? reads the log buffer only in the session that owns it, the USB session
    // (SEC-6); any other answers "". A test may hand it to another session.
    bool readsLog() const { return readsLog_; }
    void setReadsLog(bool on) { readsLog_ = on; }

    // One complete line from the LineFramer (without LF).
    void execute(const char* line, size_t len);
    // A framing error (-363, -101, -223) for SYSTem:ERRor?.
    void pushError(int16_t code);
    // Errors waiting in the queue.
    int32_t errorCount();
    // A new connection (TCP): error queue, event and enable registers empty as after power-on.
    void clearStatus();
    // IEEE 488.2 status byte as *STB? reads it (VXI-11 device_readstb).
    uint8_t statusByte();

    // Library callbacks and command handlers (scpi.cpp) reach the session through these.
    ScpiBackend& backend() { return backend_; }
    void write(const char* data, size_t len);

private:
    static constexpr size_t ERROR_QUEUE = 16;
    static constexpr size_t ERROR_INFO_HEAP = 256;

    ScpiBackend& backend_;
    Source source_;
    bool readsLog_;
    WriteFn writer_ = nullptr;
    void* writerCtx_ = nullptr;
    char serial_[18] {};
    scpi_t ctx_ {};
    scpi_interface_t iface_ {};
    char input_[16] {};
    char line_[LineFramer::MAX_LINE + 2] {};
    scpi_error_t errors_[ERROR_QUEUE] {};
    char errorInfo_[ERROR_INFO_HEAP] {};
};
