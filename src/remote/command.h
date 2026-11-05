#pragma once

#include <stdint.h>
#include <core/app_controller.h>

// Remote commands (V-6): network tasks never call the AppController, LVGL, I2C or NVS directly.
// They queue a Command (results by ticket) and read the StateSnapshot. The loop() task drains
// the queue; touch actions keep calling the controller directly in the same task.

enum class CommandType : uint8_t {
    SetFrequency,    // channel 1/2, value Hz
    AdjustFrequency, // channel 1/2, value delta Hz (clamped)
    SetEnabled,      // channel 1..3, value 0/1
    Ch3Master,       // value 1/2
    Ch3Phase,        // value 0/90/180/270
    Drive,           // value 2/4/6/8 mA
    Backlight,       // value percent
    Step,            // value Hz (one of DELTA_STEPS)
    Correction,      // value 0.01 ppm
    Tuning,          // value TuningMode
    Spread,          // value Spread
    OffState,        // value OffState
    ResetRemote,     // *RST
    SaveNow          // SYSTem:SAVE
};

struct Command {
    CommandType type;
    uint8_t channel;
    int64_t value; // full type: checked before it is narrowed (W-S19)
    Source source;
};

// Runs one command with its source set on the controller (events carry it), then back to Touch.
Result execute(AppController& app, const Command& c);

// Bounded queue for network tasks; thread-safe on the ESP32 (mutex), never blocks the caller.
class CommandQueue {
public:
    static constexpr uint8_t CAPACITY = 16;
    // false if full (the server answers "busy"); *ticket identifies the result.
    bool push(const Command& c, uint32_t* ticket = nullptr);
    // In loop(): executes every queued command; returns how many.
    uint8_t drain(AppController& app);
    // Result of an executed command (the last CAPACITY results are kept).
    bool result(uint32_t ticket, Result* out) const;

private:
    struct Entry {
        Command cmd;
        uint32_t ticket;
    };
    struct Done {
        uint32_t ticket;
        Result result;
    };
    Entry queue_[CAPACITY] {};
    Done done_[CAPACITY] {};
    uint8_t head_ = 0;
    uint8_t count_ = 0;
    uint8_t doneNext_ = 0;
    uint32_t nextTicket_ = 1;
};

// What remote readers see: state, status and actual frequencies at one revision.
struct Snapshot {
    PersistentState state;
    ChannelStatus status[2];
    Ch3Status ch3Status;
    double actualHz[3];
    bool hardwareReady;
    uint32_t revision;
};

// Seqlock: the loop() task publishes (odd sequence while writing), readers retry on a change,
// because uint64 and double fields are not copied atomically.
class StateSnapshot {
public:
    // In loop(), e.g. when the revision changed.
    void publish(const AppController& app);
    // false while nothing is published or a write is in progress (the caller retries).
    bool read(Snapshot* out) const;
    void beginWriteForTest() { ++seq_; }
    void endWriteForTest() { ++seq_; }

private:
    Snapshot data_ {};
    volatile uint32_t seq_ = 0;
    bool published_ = false;
};
