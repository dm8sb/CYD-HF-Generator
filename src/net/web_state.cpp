// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "web_state.h"

#include <stdio.h>
#include <string.h>

namespace {
void wipeBytes(void* p, size_t n) {
    volatile uint8_t* b = (volatile uint8_t*)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}
}

// ---------------------------------------------------------------------------
// NetChange

NetChange::Start NetChange::request(const NetSettings& prev, const NetSettings& next, bool needsTouch, bool final,
                                    uint32_t nowMs) {
    if (state_ != State::Idle) return Start::Busy;
    prev_ = prev;
    next_ = next;
    needsTouch_ = needsTouch;
    final_ = final;
    webConfirmed_ = touchConfirmed_ = false;
    pendingEnd_ = Action::None;
    applyAtMs_ = nowMs + APPLY_DELAY_MS;
    state_ = State::Scheduled;
    return Start::Started;
}

void NetChange::end_() {
    wipeBytes(&prev_, sizeof(prev_));
    wipeBytes(&next_, sizeof(next_));
    state_ = State::Idle;
    pendingEnd_ = Action::None;
    webConfirmed_ = touchConfirmed_ = final_ = needsTouch_ = false;
}

NetChange::Action NetChange::update(uint32_t nowMs) {
    switch (state_) {
        case State::Idle: return Action::None;
        case State::Ending:
            // The caller used the settings of the last action; now they go.
            end_();
            return Action::None;
        case State::Scheduled:
            if ((int32_t)(nowMs - applyAtMs_) < 0) return Action::None;
            if (final_) {
                state_ = State::Ending;
                return Action::ApplyAndSave;
            }
            state_ = State::Waiting;
            deadlineMs_ = nowMs + CONFIRM_MS;
            return Action::Apply;
        case State::Waiting:
            if (pendingEnd_ != Action::None) {
                const Action a = pendingEnd_;
                state_ = State::Ending;
                return a;
            }
            if ((int32_t)(nowMs - deadlineMs_) >= 0) {
                state_ = State::Ending;
                return Action::Revert;
            }
            return Action::None;
    }
    return Action::None;
}

NetChange::Confirm NetChange::confirmWeb() {
    if (state_ != State::Waiting || pendingEnd_ != Action::None) return Confirm::NothingPending;
    webConfirmed_ = true;
    if (needsTouch_ && !touchConfirmed_) return Confirm::NeedsTouch;
    pendingEnd_ = Action::Save;
    return Confirm::Committed;
}

void NetChange::confirmTouch(bool keep) {
    if (state_ != State::Waiting || pendingEnd_ != Action::None || !needsTouch_) return;
    if (!keep) {
        pendingEnd_ = Action::Revert;
        return;
    }
    touchConfirmed_ = true;
    if (webConfirmed_) pendingEnd_ = Action::Save;
}

bool NetChange::waitingForTouch() const {
    return state_ == State::Waiting && needsTouch_ && !touchConfirmed_ && pendingEnd_ == Action::None;
}

uint16_t NetChange::confirmWithinS(uint32_t nowMs) const {
    if (final_ || pendingEnd_ != Action::None) return 0;
    uint32_t left = 0;
    if (state_ == State::Scheduled) {
        const int32_t delay = (int32_t)(applyAtMs_ - nowMs);
        left = CONFIRM_MS + (delay > 0 ? (uint32_t)delay : 0);
    } else if (state_ == State::Waiting) {
        const int32_t d = (int32_t)(deadlineMs_ - nowMs);
        left = d > 0 ? (uint32_t)d : 0;
    } else {
        return 0;
    }
    const uint32_t s = (left + 999) / 1000;
    return (uint16_t)(s == 0 ? 1 : s); // 1 until update() reverts
}

// ---------------------------------------------------------------------------
// ScanSession

ScanSession::Answer ScanSession::get(Radio radio, uint32_t nowMs) {
    if (radio == Radio::Running) return Answer::Running;
    // The first GET after the scan the web started (or joined) ended gets its result.
    if (waiting_ && (radio == Radio::Done || radio == Radio::Failed)) {
        waiting_ = false;
        return radio == Radio::Done ? Answer::Done : Answer::Failed;
    }
    if (waiting_ && radio == Radio::Idle) {
        // Started, but the manager never ran it (or its result went to nobody): report a failure.
        waiting_ = false;
        return Answer::Failed;
    }
    if (started_ && nowMs - lastStartMs_ < INTERVAL_MS) return Answer::TooSoon;
    started_ = true;
    waiting_ = true;
    lastStartMs_ = nowMs;
    return Answer::Start;
}

// ---------------------------------------------------------------------------
// LoginQueue

void LoginQueue::wipe(Job* job) {
    if (job) wipeBytes(job, sizeof(*job));
}

uint32_t LoginQueue::submit(const char* password, size_t len, const char* record) {
    if (state_ != State::Idle || !password || !record || len > MAX_PASSWORD) return 0;
    const size_t rl = strlen(record);
    if (rl >= RECORD_SIZE) return 0;
    job_.id = nextId_++;
    if (nextId_ == 0) nextId_ = 1;
    memcpy(job_.password, password, len);
    job_.password[len] = '\0';
    job_.len = len;
    memcpy(job_.record, record, rl + 1);
    cancelled_ = false;
    ok_ = false;
    state_ = State::Queued;
    return job_.id;
}

bool LoginQueue::take(Job* job) {
    if (state_ != State::Queued || !job) return false;
    *job = job_;
    wipeBytes(job_.password, sizeof(job_.password));
    state_ = State::Running;
    return true;
}

void LoginQueue::finish(uint32_t id, bool ok) {
    if (state_ != State::Running || id != job_.id) return;
    if (cancelled_) {
        state_ = State::Idle;
        return;
    }
    ok_ = ok;
    state_ = State::Done;
}

uint32_t LoginQueue::check(Job* job, Verify verify, bool* ok) {
    if (ok) *ok = false;
    if (!job || !verify) return 0;
    const uint32_t id = job->id;
    const bool right = verify(job->password, job->len, job->record);
    wipe(job);
    if (ok) *ok = right;
    return id;
}

bool LoginQueue::runOne(Verify verify) {
    Job j;
    if (!verify || !take(&j)) return false;
    bool ok = false;
    const uint32_t id = check(&j, verify, &ok);
    finish(id, ok);
    return true;
}

bool LoginQueue::collect(uint32_t id, bool* ok) {
    if (state_ != State::Done || id != job_.id) return false;
    if (ok) *ok = ok_;
    state_ = State::Idle;
    return true;
}

void LoginQueue::cancel(uint32_t id) {
    if (id == 0 || id != job_.id) return;
    if (state_ == State::Queued || state_ == State::Done) {
        wipeBytes(job_.password, sizeof(job_.password));
        state_ = State::Idle;
    } else if (state_ == State::Running) {
        cancelled_ = true;
    }
}

// ---------------------------------------------------------------------------
// WebPasswordJob

void WebPasswordJob::wipe(Job* job) {
    if (job) wipeBytes(job, sizeof(*job));
}

bool WebPasswordJob::submit(const char* password, size_t len, const uint8_t salt[WebPassword::SALT_LEN]) {
    if (state_ != State::Idle || !salt || !WebPassword::valid(password, len)) return false;
    memcpy(job_.password, password, len);
    job_.password[len] = '\0';
    job_.len = len;
    memcpy(job_.salt, salt, sizeof(job_.salt));
    ok_ = false;
    state_ = State::Queued;
    return true;
}

bool WebPasswordJob::take(Job* job) {
    if (state_ != State::Queued || !job) return false;
    *job = job_;
    wipe(&job_);
    state_ = State::Running;
    return true;
}

bool WebPasswordJob::hash(Job* job, char record[WebPassword::RECORD_SIZE]) {
    if (!record) return false;
    record[0] = '\0';
    if (!job) return false;
    const bool ok = WebPassword::hashWithSalt(job->password, job->len, job->salt, record);
    wipe(job);
    return ok;
}

void WebPasswordJob::finish(const char* record, bool ok) {
    if (state_ != State::Running) return;
    ok_ = ok && record && WebPassword::wellFormed(record);
    if (ok_) snprintf(record_, sizeof(record_), "%s", record);
    state_ = State::Done;
}

WebPasswordJob::Poll WebPasswordJob::collect(char out[WebPassword::RECORD_SIZE]) {
    if (state_ == State::Idle) return Poll::Idle;
    if (state_ != State::Done) return Poll::Busy;
    const bool ok = ok_ && out;
    if (ok) memcpy(out, record_, sizeof(record_));
    wipeBytes(record_, sizeof(record_));
    ok_ = false;
    state_ = State::Idle;
    return ok ? Poll::Done : Poll::Failed;
}

bool WebPasswordJob::runOne() {
    Job j;
    if (!take(&j)) return false;
    char record[WebPassword::RECORD_SIZE];
    const bool ok = hash(&j, record);
    finish(record, ok);
    wipeBytes(record, sizeof(record));
    return true;
}
