#include "message_queue.h"

#include <stdio.h>
#include <string.h>

static void copy(MessageQueue::Message& m, const char* title, const char* text) {
    snprintf(m.title, sizeof(m.title), "%s", title ? title : "");
    snprintf(m.text, sizeof(m.text), "%s", text ? text : "");
}

MessageQueue::Post MessageQueue::post(const char* title, const char* text) {
    if (!open_) {
        copy(current_, title, text);
        open_ = true;
        return Post::Open;
    }
    // Compared as stored (truncated), so a long repeated text is still recognized.
    Message m;
    copy(m, title, text);
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

bool MessageQueue::dropMatching(bool (*drop)(const char* text)) {
    uint8_t kept = 0;
    for (uint8_t i = 0; i < count_; ++i) {
        if (!drop(queue_[i].text)) queue_[kept++] = queue_[i];
    }
    count_ = kept;
    return open_ && drop(current_.text);
}
