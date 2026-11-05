#include "command.h"

#include <string.h>

#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace {
#ifdef ESP_PLATFORM
SemaphoreHandle_t queueMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateMutex();
    return m;
}
struct Lock {
    Lock() { xSemaphoreTake(queueMutex(), portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(queueMutex()); }
};
inline void barrier() { __sync_synchronize(); }
#else
struct Lock {};
inline void barrier() {}
#endif

bool inRange(int64_t v, int64_t lo, int64_t hi) { return v >= lo && v <= hi; }

Result run(AppController& app, const Command& c) {
    const int64_t v = c.value;
    switch (c.type) {
        case CommandType::SetFrequency:
            if (v < 0) return Result::OutOfRange;
            return app.setChannelFrequency(c.channel, (uint64_t)v);
        case CommandType::AdjustFrequency:
            return app.adjustChannelFrequency(c.channel, v);
        case CommandType::SetEnabled:
            if (v != 0 && v != 1) return Result::Invalid;
            return app.setChannelEnabled(c.channel, v == 1);
        case CommandType::Ch3Master:
            if (!inRange(v, 0, 255)) return Result::Invalid;
            return app.setCH3Master((uint8_t)v);
        case CommandType::Ch3Phase:
            if (!inRange(v, 0, 359)) return Result::Invalid;
            return app.setCH3Phase((uint16_t)v);
        case CommandType::Drive:
            if (!inRange(v, 0, 255)) return Result::Invalid;
            return app.setGlobalDriveStrength((uint8_t)v);
        case CommandType::Backlight:
            if (!inRange(v, 0, 255)) return Result::OutOfRange;
            return app.setBacklight((uint8_t)v);
        case CommandType::Step:
            if (!inRange(v, 0, 0xFFFFFFFFLL)) return Result::Invalid;
            return app.setDeltaStep((uint32_t)v);
        case CommandType::Correction:
            if (!inRange(v, -CORRECTION_MAX, CORRECTION_MAX)) return Result::OutOfRange;
            return app.setCorrection((int32_t)v);
        case CommandType::Tuning:
            if (!inRange(v, 0, (int64_t)TuningMode::Integer)) return Result::Invalid;
            return app.setTuningMode((TuningMode)v);
        case CommandType::Spread:
            if (!inRange(v, 0, (int64_t)Spread::Center05)) return Result::Invalid;
            return app.setSpread((Spread)v);
        case CommandType::OffState:
            if (!inRange(v, 0, (int64_t)OffState::HighZ)) return Result::Invalid;
            return app.setOffState((OffState)v);
        case CommandType::ResetRemote:
            app.resetForRemote();
            return Result::Ok;
        case CommandType::SaveNow:
            return app.saveNow() ? Result::Ok : Result::HwError;
    }
    return Result::Invalid;
}
}

Result execute(AppController& app, const Command& c) {
    app.setSource(c.source);
    const Result r = run(app, c);
    app.setSource(Source::Touch);
    return r;
}

bool CommandQueue::push(const Command& c, uint32_t* ticket) {
    Lock lock;
    if (count_ == CAPACITY) return false;
    const uint32_t t = nextTicket_++;
    queue_[(head_ + count_) % CAPACITY] = Entry {c, t};
    ++count_;
    if (ticket) *ticket = t;
    return true;
}

uint8_t CommandQueue::drain(AppController& app) {
    uint8_t n = 0;
    for (;;) {
        Entry e;
        {
            Lock lock;
            if (count_ == 0) break;
            e = queue_[head_];
            head_ = (uint8_t)((head_ + 1) % CAPACITY);
            --count_;
        }
        // Executed outside the lock: the controller may take a while (I2C), pushes go on.
        const Result r = execute(app, e.cmd);
        Lock lock;
        done_[doneNext_] = Done {e.ticket, r};
        doneNext_ = (uint8_t)((doneNext_ + 1) % CAPACITY);
        ++n;
    }
    return n;
}

bool CommandQueue::result(uint32_t ticket, Result* out) const {
    Lock lock;
    for (const Done& d : done_) {
        if (d.ticket == ticket && ticket != 0) {
            if (out) *out = d.result;
            return true;
        }
    }
    return false;
}

void StateSnapshot::publish(const AppController& app) {
    seq_ = seq_ + 1; // odd: write in progress
    barrier();
    Snapshot& s = data_;
    s.state.ch1 = app.channel(1);
    s.state.ch2 = app.channel(2);
    s.state.ch3 = app.ch3();
    s.state.sys = app.system();
    s.status[0] = app.channelStatus(1);
    s.status[1] = app.channelStatus(2);
    s.ch3Status = app.ch3Status();
    for (uint8_t i = 0; i < 3; ++i) s.actualHz[i] = app.actualHz(i + 1);
    s.hardwareReady = app.hardwareReady();
    s.revision = app.revision();
    published_ = true;
    barrier();
    seq_ = seq_ + 1; // even: stable
}

bool StateSnapshot::read(Snapshot* out) const {
    if (!out) return false;
    const uint32_t before = seq_;
    barrier();
    if (!published_ || (before & 1)) return false;
    memcpy(out, (const void*)&data_, sizeof(Snapshot));
    barrier();
    return seq_ == before;
}
