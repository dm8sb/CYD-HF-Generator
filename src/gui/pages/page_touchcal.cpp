// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../gui.h"
#include "../gui_layout.h"
#include <stdio.h>
#include <utils/logger.h>

// Touch calibration (U-26): one cross at a time, held 0.5 s each; the logic is in
// TouchCal::Wizard (host-tested). Raw samples come straight from the touch driver, so a wrong
// calibration cannot disturb it; the page has nothing to tap.
static constexpr int CROSS = 25;

void Gui::calibrateTouch(bool firstStart) {
    calFirstStart_ = firstStart;
    navigate(PageId::TouchCal);
}

void Gui::createTouchCal_() {
    for (uint8_t i = 0; i < 2; ++i) {
        lv_obj_t* o = lv_obj_create(root_);
        lv_obj_remove_style_all(o);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(o, i == 0 ? CROSS : 3, i == 0 ? 3 : CROSS);
        w_.calCross[i] = o;
    }
    lv_obj_t* title = addLabel_(root_, "Touch calibration", &lv_font_montserrat_20, Colors::text(), 0, 0);
    // Texts above and below the center cross (108..132), clear of the corner crosses.
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);
    w_.calText = addLabel_(root_, "", &lv_font_montserrat_16, Colors::text(), 0, 0);
    lv_obj_set_width(w_.calText, 240);
    lv_obj_set_style_text_align(w_.calText, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(w_.calText, LV_LABEL_LONG_CLIP); // one line, ends at y 80
    lv_obj_align(w_.calText, LV_ALIGN_TOP_MID, 0, 62);
    w_.calStatus = addLabel_(root_, "", &lv_font_montserrat_14, Colors::warn(), 0, 0);
    lv_obj_set_width(w_.calStatus, 240);
    lv_obj_set_style_text_align(w_.calStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(w_.calStatus, LV_LABEL_LONG_WRAP);
    lv_obj_align(w_.calStatus, LV_ALIGN_TOP_MID, 0, 156);
    calWizard_.start(millis());
    calShownTarget_ = 0xFF;
    showCalTarget_();
    Logger::info("TOUCH", "Calibration started");
}

void Gui::showCalTarget_() {
    const uint8_t t = calWizard_.target();
    const TouchCal::Phase ph = calWizard_.phase();
    if (t == calShownTarget_ && ph == calShownPhase_) return;
    calShownTarget_ = t;
    calShownPhase_ = ph;
    if (t >= TouchCal::POINT_COUNT) return;
    const TouchCal::Point& p = TouchCal::TARGETS[t];
    lv_obj_set_pos(w_.calCross[0], p.x - CROSS / 2, p.y - 1);
    lv_obj_set_pos(w_.calCross[1], p.x - 1, p.y - CROSS / 2);
    // Green once the cross is taken: release and go to the next.
    const lv_color_t c = ph == TouchCal::Phase::Release ? Colors::ok() : Colors::warn();
    for (lv_obj_t* o : w_.calCross) lv_obj_set_style_bg_color(o, c, 0);
    char buf[64];
    if (ph == TouchCal::Phase::Release) snprintf(buf, sizeof(buf), "Release");
    else if (t == TouchCal::POINT_COUNT - 1) snprintf(buf, sizeof(buf), "Check: hold the cross");
    else snprintf(buf, sizeof(buf), "Hold the cross (%u/%u)", (unsigned)(t + 1), (unsigned)(TouchCal::POINT_COUNT - 1));
    lv_label_set_text(w_.calText, buf);
}

void Gui::tickTouchCal_() {
    int16_t rx = 0, ry = 0;
    const bool touched = touch_ && touch_->readRaw(&rx, &ry);
    const uint32_t now = millis();
    calWizard_.update(touched, rx, ry, now);
    char buf[112];
    switch (calWizard_.phase()) {
        case TouchCal::Phase::Done: {
            const TouchCalibration& c = calWizard_.result();
            const bool saved = storeTouchCal_ && storeTouchCal_(c);
            touch_->setCalibration(c, saved);
            snprintf(buf, sizeof(buf), "Calibrated: x %ld..%ld, y %ld..%ld, check %+d/%+d px%s", (long)c.x_min,
                     (long)c.x_max, (long)c.y_min, (long)c.y_max, calWizard_.checkDx(), calWizard_.checkDy(),
                     saved ? "" : " (not saved)");
            Logger::info("TOUCH", buf);
            if (!saved) showMessage("Touch", "Calibration applied but not saved; it is repeated at the next start.");
            navigate(calFirstStart_ ? PageId::Main : PageId::Config);
            return;
        }
        case TouchCal::Phase::Failed:
            if (calWizard_.failure() == TouchCal::Failure::Check) {
                snprintf(buf, sizeof(buf), "Check cross off by %+d / %+d px. Again, please.", calWizard_.checkDx(),
                         calWizard_.checkDy());
            } else {
                snprintf(buf, sizeof(buf), "The corners did not match. Again, please.");
            }
            Logger::error("TOUCH", buf);
            lv_label_set_text(w_.calStatus, buf);
            calWizard_.start(now);
            break;
        case TouchCal::Phase::Touch:
        case TouchCal::Phase::Release:
            if (calWizard_.timedOut(now)) {
                Logger::info("TOUCH", calFirstStart_ ? "Calibration skipped; default calibration" : "Calibration cancelled");
                navigate(calFirstStart_ ? PageId::Main : PageId::Config);
                return;
            }
            break;
    }
    showCalTarget_();
}
