#include "touch.h"
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

TouchDriver::TouchDriver() : spi_(VSPI), touch_(PIN_TOUCH_CS, PIN_TOUCH_IRQ) {}

bool TouchDriver::begin(const TouchCalibration& cal) {
    filter_ = TouchFilter::Filter(cal);
    spi_.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    bool ok = touch_.begin(spi_);
    touch_.setRotation(1);
    if (ok) Logger::info("TOUCH", "XPT2046 init OK on VSPI");
    else Logger::error("TOUCH", "XPT2046 init failed");
    return ok;
}

bool TouchDriver::pressed() {
    if (!touch_.touched()) return false;
    const TS_Point p = touch_.getPoint();
    return TouchFilter::plausibleRaw(p.x, p.y);
}

void TouchDriver::readLvgl(lv_indev_drv_t*, lv_indev_data_t* data) {
    TouchFilter::Sample s {touch_.touched(), 0, 0, 0};
    if (s.touched) {
        const TS_Point p = touch_.getPoint();
        s.rawX = p.x;
        s.rawY = p.y;
        s.z = p.z;
    }
    const TouchFilter::Output o = filter_.update(s, millis());
    if (o.calibrationError) Logger::error("TOUCH", "Calibration invalid");
    data->state = o.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->point.x = o.x;
    data->point.y = o.y;
}
