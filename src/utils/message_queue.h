// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

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

// Confirmation dialogs without LVGL (audit CQ-1, CQ-15, ARBEITSPLAN Konzept 3): at most one is
// open, further ones wait in order (an automatic question such as the open fallback AP never
// covers an open one). Every opened dialog gets a new generation; an answer counts only for the
// generation that is open, so a tap on a stale box can never trigger another dialog's action.
// kind and payload are the caller's (Gui::Confirm, e.g. the network slot to delete).
class ConfirmQueue {
public:
    static constexpr uint8_t CAPACITY = 4; // waiting ones, besides the open one
    struct Entry {
        uint8_t kind;
        uint8_t payload;
        uint16_t generation; // 0 = none
    };
    enum class Post : uint8_t { Open, Queued, Duplicate, Full };

    // Open: nothing was open, this one is open now (open()). Duplicate: the same kind and payload
    // is open or waiting already. Full: dropped.
    Post post(uint8_t kind, uint8_t payload = 0);
    bool isOpen() const { return open_.generation != 0; }
    const Entry& open() const { return open_; }
    // The answer of the box opened with generation g: true if it is the open dialog, which is
    // closed then (*answered gets it); false for a stale or unknown generation (ignore the tap).
    bool answer(uint16_t g, Entry* answered = nullptr);
    // After answer() or drop() closed the open one: opens the next waiting one; false if none.
    bool next();
    // The question is no longer needed: waiting ones of this kind go; returns true if the open
    // one has this kind, which is closed then (its box must go without an action).
    bool drop(uint8_t kind);
    // Open or waiting.
    bool pending(uint8_t kind) const;
    uint8_t queued() const { return count_; }

private:
    Entry open_ {};
    Entry queue_[CAPACITY] {};
    uint8_t count_ = 0;
    uint16_t lastGeneration_ = 0;
    uint16_t nextGeneration_();
};
