#include "touch.h"
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

TouchDriver::TouchDriver() : spi_(VSPI), touch_(PIN_TOUCH_CS, PIN_TOUCH_IRQ) {}

bool TouchDriver::begin(const TouchCalibration& cal) {
    cal_ = cal;
    spi_.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    bool ok = touch_.begin(spi_);
    touch_.setRotation(1);
    if (ok) Logger::info("TOUCH", "XPT2046 init OK on VSPI");
    else Logger::error("TOUCH", "XPT2046 init failed");
    return ok;
}

bool TouchDriver::mapPoint(int16_t rx, int16_t ry, lv_coord_t* x, lv_coord_t* y) {
    if (cal_.x_max == cal_.x_min || cal_.y_max == cal_.y_min) return false;
    long mx = map(rx, cal_.x_min, cal_.x_max, 0, SCREEN_WIDTH - 1);
    long my = map(ry, cal_.y_min, cal_.y_max, 0, SCREEN_HEIGHT - 1);
    if (mx < 0) mx = 0; if (mx >= SCREEN_WIDTH) mx = SCREEN_WIDTH - 1;
    if (my < 0) my = 0; if (my >= SCREEN_HEIGHT) my = SCREEN_HEIGHT - 1;
    *x = (lv_coord_t)mx;
    *y = (lv_coord_t)my;
    return true;
}

bool TouchDriver::readLvgl(lv_indev_drv_t*, lv_indev_data_t* data) {
    const uint32_t now = millis();
    bool pressed = touch_.touched();

    if (!pressed) {
        if (now - lastTouchMs_ < 35 && lastPressed_) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = filtX_;
            data->point.y = filtY_;
            return false;
        }
        lastPressed_ = false;
        data->state = LV_INDEV_STATE_REL;
        data->point.x = filtX_;
        data->point.y = filtY_;
        return false;
    }

    TS_Point p = touch_.getPoint();
    if (p.z < 150) {
        data->state = LV_INDEV_STATE_REL;
        return false;
    }

    lv_coord_t x = 0, y = 0;
    if (!mapPoint(p.x, p.y, &x, &y)) {
        Logger::error("TOUCH", "Calibration invalid");
        data->state = LV_INDEV_STATE_REL;
        return false;
    }

    // IIR jitter filter.
    if (!lastPressed_) {
        filtX_ = x; filtY_ = y;
    } else {
        filtX_ = (lv_coord_t)((3 * filtX_ + x) / 4);
        filtY_ = (lv_coord_t)((3 * filtY_ + y) / 4);
    }

    lastTouchMs_ = now;
    lastPressed_ = true;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = filtX_;
    data->point.y = filtY_;
    return false;
}
