#include "freq_display.h"
#include <utils/frequency_utils.h>

lv_obj_t* freqDisplayCreate(lv_obj_t* parent, uint64_t hz) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_28, 0);
    freqDisplaySet(l, hz);
    return l;
}

void freqDisplaySet(lv_obj_t* label, uint64_t hz) {
    char buf[40];
    FrequencyUtils::format(hz, buf, sizeof(buf));
    lv_label_set_text(label, buf);
}
