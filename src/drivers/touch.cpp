#include "touch.h"
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

#if TEST_TOUCH_LOG
#include <stdio.h>

// The pressed widget, e.g. btn 'CH1', btnm 'MHz', switch, dropdown; "-" for none. In the read
// callback LVGL has not yet processed the release, so act_obj still holds the pressed widget
// (lv_indev_get_obj_act() is set only while LVGL processes the input).
static void describeTarget(char* out, size_t outSize) {
    lv_indev_t* indev = lv_indev_get_act();
    lv_obj_t* obj = indev ? indev->proc.types.pointer.act_obj : nullptr;
    if (!obj) {
        snprintf(out, outSize, "-");
        return;
    }
    const char* cls = "obj";
    const char* text = "";
    if (lv_obj_check_type(obj, &lv_btnmatrix_class)) {
        cls = "btnm";
        const uint16_t id = lv_btnmatrix_get_selected_btn(obj);
        if (id != LV_BTNMATRIX_BTN_NONE) text = lv_btnmatrix_get_btn_text(obj, id);
    } else if (lv_obj_check_type(obj, &lv_switch_class)) {
        cls = "switch";
    } else if (lv_obj_check_type(obj, &lv_dropdown_class)) {
        cls = "dropdown";
    } else if (lv_obj_check_type(obj, &lv_label_class)) {
        cls = "label";
        text = lv_label_get_text(obj);
    } else {
        if (lv_obj_check_type(obj, &lv_btn_class)) cls = "btn";
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); ++i) {
            lv_obj_t* c = lv_obj_get_child(obj, i);
            if (lv_obj_check_type(c, &lv_label_class)) {
                text = lv_label_get_text(c);
                break;
            }
        }
    }
    snprintf(out, outSize, "%s '%.24s'", cls, text ? text : "");
}
#endif

TouchDriver::TouchDriver() : spi_(VSPI), touch_(PIN_TOUCH_CS, PIN_TOUCH_IRQ) {}

bool TouchDriver::begin(const TouchCalibration& cal, bool stored) {
    setCalibration(cal, stored);
    spi_.begin(PIN_TOUCH_SCK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
    bool ok = touch_.begin(spi_);
    touch_.setRotation(1);
    if (ok) Logger::info("TOUCH", "XPT2046 init OK on VSPI");
    else Logger::error("TOUCH", "XPT2046 init failed");
    return ok;
}

void TouchDriver::setCalibration(const TouchCalibration& cal, bool stored) {
    filter_ = TouchFilter::Filter(cal);
    cal_ = cal;
    calibrated_ = stored;
}

bool TouchDriver::readRaw(int16_t* rawX, int16_t* rawY) {
    if (!touch_.touched()) return false;
    const TS_Point p = touch_.getPoint();
    if (!TouchFilter::plausibleRaw(p.x, p.y)) return false;
    *rawX = p.x;
    *rawY = p.y;
    return true;
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
#if TEST_TOUCH_LOG
    TouchTest::Record r;
    if (recorder_.update(s.touched, s.z, o.pressed, o.x, o.y, millis(), &r)) {
        // LVGL handles the release after this read: the pressed widget is still known.
        char rec[96], target[40], line[144];
        TouchTest::formatRecord(r, rec, sizeof(rec));
        describeTarget(target, sizeof(target));
        snprintf(line, sizeof(line), "%s target=%s", rec, target);
        Logger::info("TOUCH", line);
    }
#endif
    data->state = o.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->point.x = o.x;
    data->point.y = o.y;
}
