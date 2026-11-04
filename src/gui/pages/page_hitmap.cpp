#include "../gui.h"

#if TEST_HIT_MAP
#include "../gui_layout.h"
#include <stdio.h>
#include <utils/logger.h>
#include <utils/touch_test.h>

// Hit map of the touch test build (MESSPLAN B8): 20 target crosses, a marker at the last
// release point and the deviation from the nearest target, also in the log. Leave by restart.
static constexpr int CROSS = 11;
static lv_obj_t* marker;
static lv_obj_t* result;

static lv_obj_t* addLine(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

void Gui::createHitMap_() {
    root_ = lv_scr_act();
    lv_obj_clean(root_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root_, Colors::screenBg(), 0);
    char buf[48];
    for (uint8_t i = 0; i < TouchTest::TARGET_COUNT; ++i) {
        const TouchTest::Point& t = TouchTest::TARGETS[i];
        addLine(root_, t.x - CROSS / 2, t.y, CROSS, 1, Colors::text());
        addLine(root_, t.x, t.y - CROSS / 2, 1, CROSS, Colors::text());
        // Number towards the screen center.
        snprintf(buf, sizeof(buf), "%u", (unsigned)i);
        lv_obj_t* n = addLabel_(root_, buf, &lv_font_montserrat_12, Colors::muted(), 0, 0);
        lv_obj_set_pos(n, t.x < SCREEN_WIDTH / 2 ? t.x + 4 : t.x - 18, t.y < SCREEN_HEIGHT / 2 ? t.y + 4 : t.y - 18);
    }
    result = addLabel_(root_, "Tap the crosses", &lv_font_montserrat_16, Colors::text(), 0, 0);
    lv_obj_align(result, LV_ALIGN_CENTER, 0, -20);
    marker = addLine(root_, 0, 0, 5, 5, Colors::error());
    lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);

    // Catches every tap on the whole screen.
    lv_obj_t* area = lv_obj_create(root_);
    lv_obj_remove_style_all(area);
    lv_obj_set_pos(area, 0, 0);
    lv_obj_set_size(area, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_add_flag(area, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(area, hitMapCb_, LV_EVENT_RELEASED, nullptr);
    Logger::info("HITMAP", "Hit map ready");
}

void Gui::hitMapCb_(lv_event_t*) {
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int16_t dx = 0, dy = 0;
    const uint8_t i = TouchTest::nearestTarget((int16_t)p.x, (int16_t)p.y, &dx, &dy);
    const TouchTest::Point& t = TouchTest::TARGETS[i];
    char buf[96];
    snprintf(buf, sizeof(buf), "target %u (%d,%d) hit (%d,%d) dx %+d dy %+d", (unsigned)i, t.x, t.y, (int)p.x,
             (int)p.y, dx, dy);
    Logger::info("HITMAP", buf);
    snprintf(buf, sizeof(buf), "#%u  dx %+d  dy %+d", (unsigned)i, dx, dy);
    lv_label_set_text(result, buf);
    lv_obj_set_pos(marker, p.x - 2, p.y - 2);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_HIDDEN);
}
#endif
