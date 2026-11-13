// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "net_settings.h"
#include "request_guard.h"

// State machines of the web server (S55). Pure and host-tested; the device glue
// (net/web_server.*) runs them in loop() (NetChange, ScanSession) or under its lock
// (LoginQueue). Time comes in as nowMs.

// Two-stage network change from the web (W-E9, W-S12): the new settings are applied
// APPLY_DELAY_MS after the request (so the 202 answer leaves before the radio restarts), then
// they must be confirmed within CONFIRM_MS over the new connection, and without a web password
// also at the touch screen; otherwise the previous settings return. A final change (mode Off:
// no new connection to confirm over) is applied and saved without confirmation.
// Holds both settings (with keys) in RAM while a change runs; wiped when it ends.
class NetChange {
public:
    static constexpr uint32_t APPLY_DELAY_MS = 500;
    static constexpr uint32_t CONFIRM_MS = 120000;

    enum class Start : uint8_t { Started, Busy };
    enum class Action : uint8_t {
        None,
        Apply,        // apply next() (not saved yet)
        ApplyAndSave, // final change: apply and save next()
        Save,         // confirmed: save next() (already applied)
        Revert        // not confirmed or refused at the touch: apply and save previous()
    };
    enum class Confirm : uint8_t { Committed, NeedsTouch, NothingPending };

    // prev: the settings now (saved); next: the new ones. needsTouch: no web password is set.
    Start request(const NetSettings& prev, const NetSettings& next, bool needsTouch, bool final, uint32_t nowMs);
    // In loop(); the caller carries out the action (the settings stay readable until the next
    // call to update()).
    Action update(uint32_t nowMs);
    // POST /api/wifi/confirm.
    Confirm confirmWeb();
    // Answer at the touch screen: keep = confirm, otherwise back to the previous settings now.
    void confirmTouch(bool keep);
    // A request is scheduled or waits for confirmation (a second one is refused).
    bool busy() const { return state_ != State::Idle; }
    // The touch screen should ask now (applied, needs the touch, not answered yet).
    bool waitingForTouch() const;
    // Seconds left to confirm for the state JSON (wifi.confirmWithin), 0 = nothing to confirm.
    uint16_t confirmWithinS(uint32_t nowMs) const;
    const NetSettings& next() const { return next_; }
    const NetSettings& previous() const { return prev_; }

private:
    enum class State : uint8_t { Idle, Scheduled, Waiting, Ending };
    void end_();

    State state_ = State::Idle;
    bool final_ = false;
    bool needsTouch_ = false;
    bool webConfirmed_ = false;
    bool touchConfirmed_ = false;
    Action pendingEnd_ = Action::None; // Save or Revert, returned by the next update()
    uint32_t applyAtMs_ = 0;
    uint32_t deadlineMs_ = 0;
    NetSettings prev_ {};
    NetSettings next_ {};
};

// GET /api/wifi/scan (W-S11, ARBEITSPLAN 3.6 point 6): a GET starts a scan unless one runs,
// GETs during the scan answer "running", the first GET after it finished gets the result once
// ("done" / "failed"), and a new start within INTERVAL_MS of the previous start is refused (429).
// A scan the touch screen started is reported the same way.
class ScanSession {
public:
    static constexpr uint32_t INTERVAL_MS = 10000;
    enum class Radio : uint8_t { Idle, Running, Done, Failed }; // WifiManager::ScanState
    enum class Answer : uint8_t { Start, Running, Done, Failed, TooSoon };

    // One GET with the manager's scan state now. Start: the caller starts a scan and answers
    // "running" (or "failed" if it could not start).
    Answer get(Radio radio, uint32_t nowMs);

private:
    bool started_ = false;  // a web scan was ever started (for the interval)
    bool waiting_ = false;  // ... and its result was not delivered yet
    uint32_t lastStartMs_ = 0;
};

// One web password check at a time (W-S23): the async_tcp task submits it, a worker task runs
// the slow PBKDF2 (WebPassword::verify, ~0.2-0.3 s) outside of it, and the async_tcp task
// collects the result. Ids tell a late result of a cancelled check from the current one.
class LoginQueue {
public:
    static constexpr size_t MAX_PASSWORD = 64;
    static constexpr size_t RECORD_SIZE = 96;
    typedef bool (*Verify)(const char* password, size_t len, const char* record);

    struct Job {
        uint32_t id;
        char password[MAX_PASSWORD + 1];
        size_t len;
        char record[RECORD_SIZE];
    };

    // A new check; 0 when one is queued or running (the caller answers "busy") or the input
    // does not fit.
    uint32_t submit(const char* password, size_t len, const char* record);
    // Worker: takes the queued check into *job (false if none) and marks it running.
    bool take(Job* job);
    // Worker: the result of the check with this id; wipes the queued password.
    void finish(uint32_t id, bool ok);
    // Runs take(), verify and finish() at once (host tests, or a worker without a lock).
    bool runOne(Verify verify);
    // Worker step without the lock: verifies a job from take(), wipes it and returns its id for
    // finish() (the id must be read before the wipe; 0 for a null job or verify).
    static uint32_t check(Job* job, Verify verify, bool* ok);
    // async_tcp: true once the check with this id is done (*ok = password right); frees it.
    bool collect(uint32_t id, bool* ok);
    // The request went away: a queued check is dropped, a running one is discarded on finish.
    void cancel(uint32_t id);
    bool idle() const { return state_ == State::Idle; }
    // Overwrites a job copy (the worker's stack copy).
    static void wipe(Job* job);

private:
    enum class State : uint8_t { Idle, Queued, Running, Done };
    State state_ = State::Idle;
    bool ok_ = false;
    bool cancelled_ = false;
    uint32_t nextId_ = 1;
    Job job_ {};
};

// Setting the web password at the device (S55b): PBKDF2 with 10 000 iterations takes about
// 1.3 s on the ESP32, too long for loop() (GUI, SCPI and the web server would stall). loop()
// submits the password with a salt it drew from the true RNG, a worker task hashes it, loop()
// collects the record and stores it. One job at a time; the password is wiped as soon as the
// worker took it, the record once it was collected.
class WebPasswordJob {
public:
    struct Job {
        char password[WebPassword::MAX_LEN + 1];
        size_t len;
        uint8_t salt[WebPassword::SALT_LEN];
    };
    enum class Poll : uint8_t { Idle, Busy, Done, Failed };

    // false while a job runs or is not collected yet, or if the password is not
    // WebPassword::valid() (8..64 bytes UTF-8 without control characters).
    bool submit(const char* password, size_t len, const uint8_t salt[WebPassword::SALT_LEN]);
    // Worker: takes the queued job into *job (false if none) and marks it running.
    bool take(Job* job);
    // Worker step without the lock: hashes a job from take() into record and wipes the job.
    static bool hash(Job* job, char record[WebPassword::RECORD_SIZE]);
    // Worker: the result (record "" or ok false = failed).
    void finish(const char* record, bool ok);
    // loop(): Done copies the record into out and frees the job; Failed frees it as well.
    Poll collect(char out[WebPassword::RECORD_SIZE]);
    bool busy() const { return state_ != State::Idle; }
    // take(), hash() and finish() at once (host tests).
    bool runOne();
    static void wipe(Job* job);

private:
    enum class State : uint8_t { Idle, Queued, Running, Done };
    State state_ = State::Idle;
    bool ok_ = false;
    Job job_ {};
    char record_[WebPassword::RECORD_SIZE] {};
};
