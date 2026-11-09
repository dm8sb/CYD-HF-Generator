#pragma once

#include <stddef.h>
#include <stdint.h>
#include "web_state.h"

// Hashes the web password typed at the device (S55b) in a short-lived worker task, so the GUI
// keeps running during the ~1.3 s of PBKDF2 (the logic is in WebPasswordJob, host-tested).
// start() and poll() run in loop(); the task does nothing but the hash (no NVS, LVGL or I2C)
// and ends itself. Created on the heap (E-31); the task stack exists only while it hashes.
class WebPasswordWorker {
public:
    WebPasswordWorker();
    // false while a job runs, for an invalid password or without memory for the task.
    bool start(const char* password, size_t len, const uint8_t salt[WebPassword::SALT_LEN]);
    // Done: the record is in out (the caller stores and wipes it).
    WebPasswordJob::Poll poll(char out[WebPassword::RECORD_SIZE]);

private:
    static void task_(void* arg);
    WebPasswordJob job_;
    void* mutex_ = nullptr; // SemaphoreHandle_t
};
