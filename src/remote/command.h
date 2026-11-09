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
    SaveNow,         // SYSTem:SAVE
    Identify         // SYSTem:IDENtify
};

struct Command {
    CommandType type;
    uint8_t channel;
    int64_t value; // full type: checked before it is narrowed (W-S19)
    Source source;
};

// Runs one command with its source set on the controller (events carry it), then back to Touch.
Result execute(AppController& app, const Command& c);

enum class PushResult : uint8_t {
    Queued,
    Full,       // CAPACITY commands waiting (the server answers "busy")
    RateLimited // more than REMOTE_WRITES_PER_S from this source (W-S11)
};

// Bounded queue for network tasks; thread-safe on the ESP32 (mutex), never blocks the caller.
class CommandQueue {
public:
    static constexpr uint8_t CAPACITY = 16;
    // *ticket identifies the result when Queued.
    PushResult push(const Command& c, uint32_t* ticket = nullptr);
    // A REST body (S55): n commands of one source, queued together or not at all (capacity and
    // n rate tokens); executed in order, and after the first refused one the rest is skipped
    // with that result ("stops at the first error", RestJson). *firstTicket is the ticket of
    // list[0], the others follow consecutively.
    PushResult pushBatch(const Command* list, uint8_t n, uint32_t* firstTicket = nullptr);
    // In loop(): executes every queued command; returns how many.
    uint8_t drain(AppController& app);
    // Result of an executed command (the last CAPACITY results are kept).
    bool result(uint32_t ticket, Result* out) const;
    // In loop() after the StateSnapshot was published (S55): every command executed so far is
    // now visible in the snapshot. visible() lets a waiting network task answer with a state
    // that already holds its command.
    void markVisible();
    bool visible(uint32_t ticket) const;

private:
    struct Entry {
        Command cmd;
        uint32_t ticket;
        bool chained; // not the first command of a batch: skipped if the one before failed
    };
    struct Done {
        uint32_t ticket;
        Result result;
    };
    // Token bucket per source (index = Source): REMOTE_WRITES_PER_S tokens, one back every
    // 1000 / REMOTE_WRITES_PER_S ms.
    struct Bucket {
        uint8_t tokens;
        uint32_t lastMs;
        bool started;
    };
    bool take_(Source s, uint8_t n);
    Bucket buckets_[5] {};
    Entry queue_[CAPACITY] {};
    Done done_[CAPACITY] {};
    uint8_t head_ = 0;
    uint8_t count_ = 0;
    uint8_t doneNext_ = 0;
    uint32_t nextTicket_ = 1;
    uint32_t lastDone_ = 0;  // ticket of the last executed command
    uint32_t visible_ = 0;   // ... of the last one in the published snapshot
    bool lastFailed_ = false;
    Result lastResult_ = Result::Ok;
};

// What remote readers see: state, status and actual frequencies at one revision.
struct Snapshot {
    PersistentState state;
    ChannelStatus status[2];
    Ch3Status ch3Status;
    double actualHz[3];
    bool hardwareReady;
    LinkState link;
    uint64_t iqMinHz;
    uint64_t iqMaxHz;
    bool remoteOutputsAllowed;
    uint32_t revision;
    // Save state and REMOTE indicator for the web state (S55a); not counted by the revision,
    // so they are only as fresh as the last publish().
    SaveState saveState;
    Source lastRemote; // Source::Touch while no remote command came
    uint32_t lastRemoteMs;
};

// Seqlock: the loop() task publishes (odd sequence while writing), readers retry on a change,
// because uint64 and double fields are not copied atomically.
class StateSnapshot {
public:
    // In loop(), e.g. when the revision changed.
    void publish(const AppController& app);
    // false while nothing is published or a write is in progress (the caller retries).
    bool read(Snapshot* out) const;
    void beginWriteForTest() { seq_ = seq_ + 1; } // C++20: no ++ on volatile
    void endWriteForTest() { seq_ = seq_ + 1; }

private:
    Snapshot data_ {};
    volatile uint32_t seq_ = 0;
    bool published_ = false;
};
