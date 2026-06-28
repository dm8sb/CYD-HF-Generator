#pragma once
#include <lvgl.h>
#include <Arduino.h>

lv_obj_t* freqDisplayCreate(lv_obj_t* parent, uint64_t hz);
void freqDisplaySet(lv_obj_t* label, uint64_t hz);
