#include "scpi_usb.h"

#include <constants.h>
#include <utils/logger.h>

ScpiBackend::Outcome LoopScpiBackend::submit(const Command& c) {
    uint32_t ticket = 0;
    const PushResult push = queue_.push(c, &ticket);
    if (push != PushResult::Queued) return Outcome {push, Result::Ok};
    queue_.drain(app_);
    snapshot_.publish(app_);
    Result r = Result::Ok;
    queue_.result(ticket, &r);
    return Outcome {push, r};
}

bool LoopScpiBackend::snapshot(Snapshot* out) {
    // Same task as the controller: always current, never torn.
    snapshot_.publish(app_);
    return snapshot_.read(out);
}

void LoopScpiBackend::heap(uint32_t* freeBytes, uint32_t* minFree, uint32_t* largest) {
    *freeBytes = *minFree = *largest = 0;
    if (heap_) heap_(freeBytes, minFree, largest);
}

void ScpiUsbLink::start_() {
    active_ = true;
    if (debugOutput_) debugOutput_(false);
    Logger::setOutput(Logger::Output::ToBuffer);
    Logger::info("SCPI", "USB session started; log in SYSTem:LOG?");
}

void ScpiUsbLink::end_() {
    active_ = false;
    framer_.reset();
    Logger::setOutput(Logger::Output::ToSerial);
    if (debugOutput_) debugOutput_(true);
    Logger::info("SCPI", "USB session ended (idle); log on serial again");
}

void ScpiUsbLink::receive(const char* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        const LineFramer::Event e = framer_.feed(data[i]);
        if (e == LineFramer::Event::None) continue;
        lastLineMs_ = millis();
        if (!active_) start_();
        if (e == LineFramer::Event::Line) session_.execute(framer_.line(), framer_.length());
        else session_.pushError(framerErrorCode(e));
    }
}

void ScpiUsbLink::update() {
    if (active_ && millis() - lastLineMs_ >= SCPI_IDLE_TIMEOUT_MS) end_();
}
