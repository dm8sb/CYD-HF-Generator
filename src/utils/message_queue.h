#pragma once

#include <stddef.h>
#include <stdint.h>

// Dialog bookkeeping without LVGL (audit T-12): at most one dialog is open, further ones wait
// in a short queue, identical texts are dropped (A-38). Gui shows the dialogs.
class MessageQueue {
public:
    static constexpr uint8_t CAPACITY = 3;
    struct Message {
        char title[24];
        char text[160];
        uint8_t tag; // category of the source event (e.g. connection), for dropTagged()
    };

    // Result of post(): open it now, it was queued, or it was dropped (duplicate or queue full).
    enum class Post : uint8_t { Open, Queued, Duplicate, Full };

    // Open: no dialog was open, the message is the open one now (openTitle(), openText()).
    Post post(const char* title, const char* text, uint8_t tag = 0);
    // The open dialog was closed; returns the next message to open, or nullptr.
    const Message* close();
    // Drops queued messages with this tag; returns true if the open one has it.
    bool dropTagged(uint8_t tag);

    bool isOpen() const { return open_; }
    const char* openText() const { return current_.text; }
    const char* openTitle() const { return current_.title; }
    uint8_t queued() const { return count_; }

private:
    Message current_ {};
    Message queue_[CAPACITY] {};
    uint8_t count_ = 0;
    bool open_ = false;
};
