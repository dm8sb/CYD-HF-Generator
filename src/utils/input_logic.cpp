#include "input_logic.h"

#include <constants.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <utils/frequency_utils.h>

namespace InputLogic {

const char* const KEY_BACKSPACE = "\xEF\x95\x9A"; // LV_SYMBOL_BACKSPACE (LVGL 8.3)

static bool isUnit(const char* key) { return !strcmp(key, "Hz") || !strcmp(key, "kHz") || !strcmp(key, "MHz"); }

NumpadEvent numpadKey(Numpad& n, const char* key, uint64_t* outHz) {
    if (!key) return NumpadEvent::Ignored;
    n.error = false;
    if (!strcmp(key, KEY_BACKSPACE)) {
        if (n.len == 0) return NumpadEvent::Ignored;
        n.text[--n.len] = '\0';
        return NumpadEvent::Edited;
    }
    if (!strcmp(key, "C")) {
        n.len = 0;
        n.text[0] = '\0';
        return NumpadEvent::Edited;
    }
    if (isUnit(key)) {
        uint64_t hz = 0;
        if (!FrequencyUtils::parseFrequency(n.text, key, &hz)) {
            n.len = 0;
            n.text[0] = '\0';
            n.error = true;
            return NumpadEvent::Rejected;
        }
        if (outHz) *outHz = hz;
        return NumpadEvent::Confirmed;
    }
    const bool digit = key[0] >= '0' && key[0] <= '9' && key[1] == '\0';
    const bool point = key[0] == '.' && key[1] == '\0';
    if (!digit && !point) return NumpadEvent::Ignored;
    // Only one decimal point; the field holds NUMPAD_MAX_LEN characters.
    if ((point && strchr(n.text, '.')) || n.len >= NUMPAD_MAX_LEN) return NumpadEvent::Ignored;
    n.text[n.len++] = key[0];
    n.text[n.len] = '\0';
    return NumpadEvent::Edited;
}

size_t charIndexAt(const char* text, int x, GlyphWidth width, void* ctx) {
    const size_t len = strlen(text);
    int right = 0;
    for (size_t i = 0; i < len; ++i) {
        right += width(text[i], text[i + 1], ctx);
        if (x < right) return i;
    }
    return len - 1; // right of the text = unit
}

TapTarget tapTarget(const char* text, size_t index, uint32_t* step) {
    if (!text || index >= strlen(text)) return TapTarget::None;
    if (isalpha((unsigned char)text[index])) return TapTarget::Numpad;
    const int exponent = FrequencyUtils::digitExponent(text, index);
    if (exponent < 0) return TapTarget::None; // decimal point or space
    if (step) {
        *step = exponent < 10 ? FrequencyUtils::powerOfTen((uint8_t)exponent) : DELTA_STEPS[DELTA_STEP_COUNT - 1];
        if (*step > DELTA_STEPS[DELTA_STEP_COUNT - 1]) *step = DELTA_STEPS[DELTA_STEP_COUNT - 1];
    }
    return TapTarget::Step;
}

void stepMarkup(const char* text, uint32_t step, uint32_t rgb, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    const int stepExp = FrequencyUtils::decimalExponent(step);
    size_t n = 0;
    for (size_t i = 0; text && text[i] && n + 12 < outSize; ++i) {
        if (FrequencyUtils::digitExponent(text, i) == stepExp) {
            n += snprintf(out + n, outSize - n, "#%06lX %c#", (unsigned long)(rgb & 0xFFFFFF), text[i]);
        } else {
            out[n++] = text[i];
        }
    }
    out[n] = '\0';
}

Area edgeArea(const Area& b, int16_t reach, int16_t screenW, int16_t screenH) {
    Area a = b;
    if (a.x1 <= reach) a.x1 = 0;
    if (a.y1 <= reach) a.y1 = 0;
    if (a.x2 >= screenW - 1 - reach) a.x2 = screenW - 1;
    if (a.y2 >= screenH - 1 - reach) a.y2 = screenH - 1;
    return a;
}

bool contains(const Area& a, int16_t x, int16_t y) {
    return x >= a.x1 && x <= a.x2 && y >= a.y1 && y <= a.y2;
}

}
