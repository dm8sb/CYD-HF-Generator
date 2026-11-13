// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "message_queue.h"

#include <stdio.h>
#include <string.h>

static void copy(MessageQueue::Message& m, const char* title, const char* text, uint8_t tag) {
    snprintf(m.title, sizeof(m.title), "%s", title ? title : "");
    snprintf(m.text, sizeof(m.text), "%s", text ? text : "");
    m.tag = tag;
}

MessageQueue::Post MessageQueue::post(const char* title, const char* text, uint8_t tag) {
    if (!open_) {
        copy(current_, title, text, tag);
        open_ = true;
        return Post::Open;
    }
    // Compared as stored (truncated), so a long repeated text is still recognized.
    Message m;
    copy(m, title, text, tag);
    if (strcmp(current_.text, m.text) == 0) return Post::Duplicate;
    for (uint8_t i = 0; i < count_; ++i) {
        if (strcmp(queue_[i].text, m.text) == 0) return Post::Duplicate;
    }
    if (count_ == CAPACITY) return Post::Full;
    queue_[count_++] = m;
    return Post::Queued;
}

const MessageQueue::Message* MessageQueue::close() {
    open_ = false;
    current_.title[0] = current_.text[0] = '\0';
    if (count_ == 0) return nullptr;
    current_ = queue_[0];
    --count_;
    memmove(&queue_[0], &queue_[1], count_ * sizeof(Message));
    open_ = true;
    return &current_;
}

// ---------------------------------------------------------------------------
// ConfirmQueue

uint16_t ConfirmQueue::nextGeneration_() {
    if (++lastGeneration_ == 0) lastGeneration_ = 1; // 0 means none
    return lastGeneration_;
}

ConfirmQueue::Post ConfirmQueue::post(uint8_t kind, uint8_t payload) {
    if (!isOpen()) {
        open_ = Entry {kind, payload, nextGeneration_()};
        return Post::Open;
    }
    if (open_.kind == kind && open_.payload == payload) return Post::Duplicate;
    for (uint8_t i = 0; i < count_; ++i) {
        if (queue_[i].kind == kind && queue_[i].payload == payload) return Post::Duplicate;
    }
    if (count_ == CAPACITY) return Post::Full;
    queue_[count_++] = Entry {kind, payload, 0};
    return Post::Queued;
}

bool ConfirmQueue::answer(uint16_t g, Entry* answered) {
    if (g == 0 || g != open_.generation) return false;
    if (answered) *answered = open_;
    open_ = Entry {};
    return true;
}

bool ConfirmQueue::next() {
    if (isOpen() || count_ == 0) return false;
    open_ = queue_[0];
    open_.generation = nextGeneration_();
    --count_;
    memmove(&queue_[0], &queue_[1], count_ * sizeof(Entry));
    return true;
}

bool ConfirmQueue::drop(uint8_t kind) {
    uint8_t kept = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (queue_[i].kind != kind) queue_[kept++] = queue_[i];
    }
    count_ = kept;
    if (!isOpen() || open_.kind != kind) return false;
    open_ = Entry {};
    return true;
}

bool ConfirmQueue::pending(uint8_t kind) const {
    if (isOpen() && open_.kind == kind) return true;
    for (uint8_t i = 0; i < count_; ++i) {
        if (queue_[i].kind == kind) return true;
    }
    return false;
}

bool MessageQueue::dropTagged(uint8_t tag) {
    uint8_t kept = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (queue_[i].tag != tag) queue_[kept++] = queue_[i];
    }
    count_ = kept;
    return open_ && current_.tag == tag;
}
