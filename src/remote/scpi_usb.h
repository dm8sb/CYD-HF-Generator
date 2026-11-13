// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "command.h"
#include "line_framer.h"
#include "scpi.h"

// Backend for sessions that run in the loop() task (SCPI over USB): a command goes through
// the queue (rate limit per source) and is executed at once, the snapshot is published again,
// so a query later in the same line sees the change.
class LoopScpiBackend : public ScpiBackend {
public:
    typedef void (*HeapFn)(uint32_t* freeBytes, uint32_t* minFree, uint32_t* largest);
    LoopScpiBackend(CommandQueue& queue, AppController& app, StateSnapshot& snapshot, HeapFn heap = nullptr)
        : queue_(queue), app_(app), snapshot_(snapshot), heap_(heap) {}
    Outcome submit(const Command& c) override;
    bool snapshot(Snapshot* out) override;
    void heap(uint32_t* freeBytes, uint32_t* minFree, uint32_t* largest) override;

private:
    CommandQueue& queue_;
    AppController& app_;
    StateSnapshot& snapshot_;
    HeapFn heap_;
};

// SCPI over the USB serial port (W-E27, W-S22). The first line received starts a session:
// from then on log lines go only into the ring buffer (SYSTem:LOG?) and the core's own
// messages are off, so answers are not mixed with log text. SCPI_IDLE_TIMEOUT_MS without a
// line ends the session; the log goes back to the serial port.
class ScpiUsbLink {
public:
    // Switches the Arduino core's debug output (Serial.setDebugOutput) on the device.
    typedef void (*DebugOutputFn)(bool enabled);

    explicit ScpiUsbLink(ScpiSession& session, DebugOutputFn debugOutput = nullptr)
        : session_(session), debugOutput_(debugOutput) {}
    // Bytes from the serial port.
    void receive(const char* data, size_t len);
    // In loop(): ends an idle session.
    void update();
    bool active() const { return active_; }

private:
    void start_();
    void end_();

    ScpiSession& session_;
    DebugOutputFn debugOutput_;
    LineFramer framer_;
    bool active_ = false;
    uint32_t lastLineMs_ = 0;
};
