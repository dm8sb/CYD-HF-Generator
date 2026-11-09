#include "web_password_worker.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>

namespace {
// PBKDF2 needs little stack (the login worker of the web server runs the same code in 4 KB).
constexpr uint32_t STACK_BYTES = 4096;
// Core 0 at the lowest priority above idle, like the login worker: loop() on core 1 keeps
// running, the WiFi and network tasks on core 0 preempt it.
constexpr UBaseType_t PRIORITY = 1;
constexpr BaseType_t CORE = 0;

void wipe(void* p, size_t n) {
    volatile uint8_t* b = (volatile uint8_t*)p;
    for (size_t i = 0; i < n; ++i) b[i] = 0;
}

struct Lock {
    explicit Lock(void* m) : m_((SemaphoreHandle_t)m) { xSemaphoreTake(m_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(m_); }
    SemaphoreHandle_t m_;
};
}

WebPasswordWorker::WebPasswordWorker() : mutex_(xSemaphoreCreateMutex()) {}

bool WebPasswordWorker::start(const char* password, size_t len, const uint8_t salt[WebPassword::SALT_LEN]) {
    if (!mutex_) return false;
    {
        Lock l(mutex_);
        if (!job_.submit(password, len, salt)) return false;
    }
    if (xTaskCreatePinnedToCore(task_, "web_pw", STACK_BYTES, this, PRIORITY, nullptr, CORE) == pdPASS) return true;
    // No task: drop the job again (it was queued, nobody takes it).
    Lock l(mutex_);
    WebPasswordJob::Job j;
    if (job_.take(&j)) {
        WebPasswordJob::wipe(&j);
        job_.finish("", false);
    }
    char none[WebPassword::RECORD_SIZE];
    job_.collect(none);
    return false;
}

WebPasswordJob::Poll WebPasswordWorker::poll(char out[WebPassword::RECORD_SIZE]) {
    if (!mutex_) return WebPasswordJob::Poll::Idle;
    Lock l(mutex_);
    return job_.collect(out);
}

void WebPasswordWorker::task_(void* arg) {
    WebPasswordWorker* w = static_cast<WebPasswordWorker*>(arg);
    WebPasswordJob::Job j;
    bool got;
    {
        Lock l(w->mutex_);
        got = w->job_.take(&j);
    }
    if (got) {
        char record[WebPassword::RECORD_SIZE];
        const bool ok = WebPasswordJob::hash(&j, record); // wipes j
        {
            Lock l(w->mutex_);
            w->job_.finish(record, ok);
        }
        wipe(record, sizeof(record));
    }
    vTaskDelete(nullptr);
}
