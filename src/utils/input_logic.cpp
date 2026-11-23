// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input_logic.h"

#include <constants.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_settings.h>
#include <net/request_guard.h>
#include <utils/frequency_utils.h>
#include <utils/ipv4.h>
#include <utils/sanitize.h>

namespace InputLogic {

const char* const KEY_BACKSPACE = "\xEF\x95\x9A"; // LV_SYMBOL_BACKSPACE (LVGL 8.3)
const char* const KEY_SIGN = "+/-";

static_assert(FREQ_MIN_HZ == 8000ULL && FREQ_MAX_HZ == 160000000ULL, "numpadErrorText() names the range");
static_assert(NUMPAD_MAX_LEN == 10, "numpadErrorText() names the length");

const char* numpadErrorText(NumpadError e) {
    switch (e) {
        case NumpadError::Empty: return "Enter a number";
        // Short enough to fit next to "Now 160.000 000 MHz" on the numpad line.
        case NumpadError::OutOfRange: return "8 kHz - 160 MHz";
        case NumpadError::TooLong: return "Max. 10 characters";
        case NumpadError::PpmRange: return "-50.00 to +50.00 ppm";
        case NumpadError::OffsetRange: return "-160 to +160 MHz";
        case NumpadError::None: break;
    }
    return "";
}

static bool isUnit(const char* key) { return !strcmp(key, "Hz") || !strcmp(key, "kHz") || !strcmp(key, "MHz"); }

NumpadEvent numpadKey(Numpad& n, const char* key, uint64_t* outHz) {
    if (!key) return NumpadEvent::Ignored;
    n.error = NumpadError::None;
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
            // The input stays, so it can be corrected (U-9).
            n.error = strpbrk(n.text, "0123456789") ? NumpadError::OutOfRange : NumpadError::Empty;
            return NumpadEvent::Rejected;
        }
        if (outHz) *outHz = hz;
        return NumpadEvent::Confirmed;
    }
    const bool digit = key[0] >= '0' && key[0] <= '9' && key[1] == '\0';
    const bool point = key[0] == '.' && key[1] == '\0';
    if (!digit && !point) return NumpadEvent::Ignored;
    // Only one decimal point; the field holds NUMPAD_MAX_LEN characters.
    if (point && strchr(n.text, '.')) return NumpadEvent::Ignored;
    if (n.len >= NUMPAD_MAX_LEN) {
        n.error = NumpadError::TooLong;
        return NumpadEvent::Rejected;
    }
    n.text[n.len++] = key[0];
    n.text[n.len] = '\0';
    return NumpadEvent::Edited;
}

static_assert(CORRECTION_MAX == 5000, "numpadErrorText() names the correction range");

NumpadEvent numpadPpmKey(Numpad& n, const char* key, int32_t* outCentiPpm) {
    if (!key || isUnit(key)) return NumpadEvent::Ignored;
    if (!strcmp(key, KEY_SIGN)) {
        n.error = NumpadError::None;
        n.negative = !n.negative;
        return NumpadEvent::Edited;
    }
    if (!strcmp(key, "ppm")) {
        n.error = NumpadError::None;
        if (!strpbrk(n.text, "0123456789")) {
            n.error = NumpadError::Empty;
            return NumpadEvent::Rejected;
        }
        // The editing allows only digits and one point, so strtod reads all of it.
        const double v = strtod(n.text, nullptr) * 100.0;
        if (!(v <= (double)CORRECTION_MAX + 0.5)) {
            n.error = NumpadError::PpmRange;
            return NumpadEvent::Rejected;
        }
        const int32_t centi = (int32_t)(v + 0.5);
        if (outCentiPpm) *outCentiPpm = n.negative ? -centi : centi;
        return NumpadEvent::Confirmed;
    }
    if (!strcmp(key, "C")) n.negative = false;
    return numpadKey(n, key, nullptr);
}

static_assert(CH2_OFFSET_MAX_HZ == 160000000, "numpadErrorText() names the offset range");

NumpadEvent numpadOffsetKey(Numpad& n, const char* key, int32_t* outHz) {
    if (!key) return NumpadEvent::Ignored;
    if (!strcmp(key, KEY_SIGN)) {
        n.error = NumpadError::None;
        n.negative = !n.negative;
        return NumpadEvent::Edited;
    }
    if (isUnit(key)) {
        n.error = NumpadError::None;
        if (!strpbrk(n.text, "0123456789")) {
            n.error = NumpadError::Empty;
            return NumpadEvent::Rejected;
        }
        uint64_t hz = 0;
        if (!FrequencyUtils::parseDecimalHz(n.text, key, &hz) || hz > (uint64_t)CH2_OFFSET_MAX_HZ) {
            n.error = NumpadError::OffsetRange;
            return NumpadEvent::Rejected;
        }
        if (outHz) *outHz = n.negative ? -(int32_t)hz : (int32_t)hz;
        return NumpadEvent::Confirmed;
    }
    if (!strcmp(key, "C")) n.negative = false;
    return numpadKey(n, key, nullptr);
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

size_t tapIndexAt(const char* text, int x, GlyphWidth width, void* ctx) {
    const size_t i = charIndexAt(text, x, width, ctx);
    if (text[i] != ' ' && text[i] != '.') return i;
    int left = 0;
    for (size_t k = 0; k < i; ++k) left += width(text[k], text[k + 1], ctx);
    const bool leftHalf = x < left + width(text[i], text[i + 1], ctx) / 2;
    const size_t len = strlen(text);
    auto usable = [&](size_t k) { return k < len && isalnum((unsigned char)text[k]); };
    const size_t nearer = leftHalf ? i - 1 : i + 1; // i - 1 wraps for i == 0: not usable
    const size_t other = leftHalf ? i + 1 : i - 1;
    if (usable(nearer)) return nearer;
    if (usable(other)) return other;
    return i;
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

uint8_t repeatSteps(uint32_t heldMs) {
    return heldMs >= REPEAT_FASTER_MS ? 2 : 1;
}

bool repeatDue(uint32_t heldMs, uint32_t sinceStepMs) {
    return heldMs >= REPEAT_FAST_MS || sinceStepMs >= REPEAT_SLOW_INTERVAL_MS;
}

ScreenLock::Event ScreenLock::update(bool pressed, uint32_t nowMs) {
    if (!blocking()) return Event::None;
    if (pressed) {
        if (!pressing_) {
            pressing_ = true;
            pressStartMs_ = nowMs;
            if (locked_) return Event::Pressed;
        }
        if (locked_ && nowMs - pressStartMs_ >= UNLOCK_HOLD_MS) {
            locked_ = false;
            swallow_ = true;
            return Event::Unlocked;
        }
        return Event::None;
    }
    if (!pressing_) return Event::None;
    pressing_ = false;
    swallow_ = false;
    return Event::Released;
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

// ---------------------------------------------------------------------------
// Text entry (AF47)

const char* const ENTRY_BUSY = "Busy, try again";
const char* const ENTRY_NOT_ACCEPTED = "Not accepted";

// A network key may be a 64-digit hex PMK (E-510), the AP password is a passphrase (63).
EntrySpec entrySpec(EntryPurpose p) {
    switch (p) {
        case EntryPurpose::Password: return {64, true, true, false, false, nullptr};
        case EntryPurpose::ApPassword: return {63, true, false, false, false, nullptr};
        case EntryPurpose::WebPassword: return {(uint8_t)WebPassword::MAX_LEN, true, true, false, false, nullptr};
        case EntryPurpose::DeviceName:
            return {32, false, false, false, true, "abcdefghijklmnopqrstuvwxyz0123456789-"};
        case EntryPurpose::IpField: return {15, false, false, true, true, "0123456789."};
        case EntryPurpose::Ssid: break;
    }
    return {32, false, false, false, true, nullptr};
}

void entryTitle(EntryPurpose p, uint8_t field, const char* ssid, char* out, size_t outSize) {
    static const char* const FIELD_NAMES[4] = {"Address", "Mask", "Gateway", "DNS"};
    switch (p) {
        case EntryPurpose::DeviceName: snprintf(out, outSize, "Device name"); return;
        case EntryPurpose::IpField: snprintf(out, outSize, "%s", FIELD_NAMES[field & 3]); return;
        case EntryPurpose::ApPassword: snprintf(out, outSize, "AP password"); return;
        case EntryPurpose::WebPassword: snprintf(out, outSize, "Web password"); return;
        case EntryPurpose::Password: {
            char shown[33];
            Sanitize::text(ssid, strlen(ssid), shown, sizeof(shown));
            snprintf(out, outSize, "Password %s", shown);
            return;
        }
        case EntryPurpose::Ssid: break;
    }
    snprintf(out, outSize, "Network name");
}

void entryInfo(EntryPurpose p, size_t len, const char* error, bool webPasswordSet, char* out, size_t outSize) {
    if (error && error[0]) snprintf(out, outSize, "%s", error);
    else if (p == EntryPurpose::WebPassword && len == 0 && webPasswordSet) snprintf(out, outSize, "empty + OK: remove");
    else snprintf(out, outSize, "%u/%u", (unsigned)len, (unsigned)entrySpec(p).maxLen);
}

static EntryResult entryStep(EntryStep step) { return {step, "", 0}; }
static EntryResult entryInvalid(const char* error) { return {EntryStep::Invalid, error, 0}; }

EntryResult submitEntry(EntryPurpose p, const char* text, bool webPasswordSet) {
    switch (p) {
        case EntryPurpose::DeviceName:
            return NetConfig::validDeviceName(text) ? entryStep(EntryStep::StoreName) : entryInvalid("a-z 0-9 - (1-32)");
        case EntryPurpose::IpField: {
            EntryResult r = entryStep(EntryStep::StoreIp);
            if (!Ipv4::parse(text, &r.ip)) return entryInvalid("e.g. 192.168.1.50");
            return r;
        }
        case EntryPurpose::WebPassword: {
            const size_t len = strlen(text);
            if (len == 0 && webPasswordSet) return entryStep(EntryStep::ConfirmRemove);
            return WebPassword::valid(text, len) ? entryStep(EntryStep::HashWebPassword) : entryInvalid("8-64 characters");
        }
        case EntryPurpose::ApPassword:
            return NetConfig::validPassphrase(text) ? entryStep(EntryStep::StoreApPassword) : entryInvalid("8-63 characters");
        case EntryPurpose::Ssid:
            return NetConfig::validSsid(text) ? entryStep(EntryStep::AskPassword) : entryInvalid("1-32 characters");
        case EntryPurpose::Password: break;
    }
    return NetConfig::validNetworkKey(text) ? entryStep(EntryStep::StoreNetwork) : entryInvalid("8-63 chars or 64 hex");
}

// ---------------------------------------------------------------------------
// Scan list pages (AF47)

uint8_t scanPages(uint8_t count, uint8_t rows) { return rows ? (uint8_t)((count + rows - 1) / rows) : 0; }

uint8_t clampScanPage(uint8_t page, uint8_t count, uint8_t rows) {
    const uint8_t pages = scanPages(count, rows);
    if (page >= pages) page = pages ? (uint8_t)(pages - 1) : 0;
    return page;
}

uint8_t stepScanPage(uint8_t page, bool forward, uint8_t count, uint8_t rows) {
    if (!forward) return page > 0 ? (uint8_t)(page - 1) : page;
    return (unsigned)(page + 1) * rows < count ? (uint8_t)(page + 1) : page;
}

bool scanIndex(uint8_t page, uint8_t row, uint8_t count, uint8_t rows, uint8_t* index) {
    const unsigned idx = (unsigned)page * rows + row;
    if (idx >= count) return false;
    *index = (uint8_t)idx;
    return true;
}

}
