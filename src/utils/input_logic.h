#pragma once

#include <stddef.h>
#include <stdint.h>

// Input logic of the GUI as pure functions (no LVGL), host-tested in test_input_logic
// (audit T-10, T-11). The pages keep only the widgets.
namespace InputLogic {

// ---------------------------------------------------------------------------
// Numpad: digits, one decimal point, backspace, clear and the unit keys Hz / kHz / MHz.

static constexpr size_t NUMPAD_MAX_LEN = 9;

struct Numpad {
    char text[NUMPAD_MAX_LEN + 1] = {};
    size_t len = 0;
    // The last unit key found the input invalid (shown as a red border).
    bool error = false;
};

enum class NumpadEvent : uint8_t {
    Edited,    // text changed (digit, point, backspace, clear)
    Ignored,   // key without effect (second point, full field, unknown key)
    Rejected,  // unit key with invalid input: text cleared, error set
    Confirmed, // unit key with a valid frequency in *outHz
};

// One key press with the text of the key (as on the button matrix; backspace = LV_SYMBOL_BACKSPACE).
NumpadEvent numpadKey(Numpad& n, const char* key, uint64_t* outHz);
// UTF-8 text of LV_SYMBOL_BACKSPACE, so the logic needs no LVGL header.
extern const char* const KEY_BACKSPACE;

// ---------------------------------------------------------------------------
// Frequency label on the channel page ("10.700 000 MHz").

// Width in px of character c followed by next (kerning); ctx is passed through.
typedef int (*GlyphWidth)(char c, char next, void* ctx);
// Index of the character under x (relative to the text start); right of the text the last
// character (the unit). text must not be empty.
size_t charIndexAt(const char* text, int x, GlyphWidth width, void* ctx);

enum class TapTarget : uint8_t { None, Step, Numpad };
// What a tap on character `index` does: the unit opens the numpad, a digit selects its step
// (digits above the largest step select the largest), point and spaces do nothing.
TapTarget tapTarget(const char* text, size_t index, uint32_t* step);

// Copy of text with the digit that +/- changes (step) wrapped in an LVGL recolor command
// "#RRGGBB d#". Stops before out would overflow.
void stepMarkup(const char* text, uint32_t step, uint32_t rgb, char* out, size_t outSize);

}
