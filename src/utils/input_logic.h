#pragma once

#include <stddef.h>
#include <stdint.h>

// Input logic of the GUI as pure functions (no LVGL), host-tested in test_input_logic
// (audit T-10, T-11). The pages keep only the widgets.
namespace InputLogic {

// ---------------------------------------------------------------------------
// Numpad: digits, one decimal point, backspace, clear and the unit keys Hz / kHz / MHz.

static constexpr size_t NUMPAD_MAX_LEN = 9;

// Why the last key was rejected; shown as a text and a red border until the next key (U-9).
enum class NumpadError : uint8_t { None, Empty, OutOfRange, TooLong };

struct Numpad {
    char text[NUMPAD_MAX_LEN + 1] = {};
    size_t len = 0;
    NumpadError error = NumpadError::None;
};

enum class NumpadEvent : uint8_t {
    Edited,    // text changed (digit, point, backspace, clear)
    Ignored,   // key without effect (second point, full field, unknown key)
    Rejected,  // unit key with invalid input or a character beyond the field: text kept, error set
    Confirmed, // unit key with a valid frequency in *outHz
};

// One key press with the text of the key (as on the button matrix; backspace = LV_SYMBOL_BACKSPACE).
NumpadEvent numpadKey(Numpad& n, const char* key, uint64_t* outHz);
// UTF-8 text of LV_SYMBOL_BACKSPACE, so the logic needs no LVGL header.
extern const char* const KEY_BACKSPACE;
// Short text for the error line of the numpad ("" for None).
const char* numpadErrorText(NumpadError e);

// ---------------------------------------------------------------------------
// Frequency label on the channel page ("10.700 000 MHz").

// Width in px of character c followed by next (kerning); ctx is passed through.
typedef int (*GlyphWidth)(char c, char next, void* ctx);
// Index of the character under x (relative to the text start); right of the text the last
// character (the unit). text must not be empty.
size_t charIndexAt(const char* text, int x, GlyphWidth width, void* ctx);
// As charIndexAt, but a point or space selects the nearer neighbor (digit or unit): the
// separators are only 1.8-3.4 mm wide targets (audit U-15).
size_t tapIndexAt(const char* text, int x, GlyphWidth width, void* ctx);

enum class TapTarget : uint8_t { None, Step, Numpad };
// What a tap on character `index` does: the unit opens the numpad, a digit selects its step
// (digits above the largest step select the largest), point and spaces do nothing.
TapTarget tapTarget(const char* text, size_t index, uint32_t* step);

// Copy of text with the digit that +/- changes (step) wrapped in an LVGL recolor command
// "#RRGGBB d#". Stops before out would overflow.
void stepMarkup(const char* text, uint32_t step, uint32_t rgb, char* out, size_t outSize);

// ---------------------------------------------------------------------------
// Buttons at the screen edge (audit U-5). The touch mapping clamps taps beyond the panel to its
// outermost pixel, so a button a few px from the edge misses taps at the very edge.

// Inclusive screen area, as lv_area_t.
struct Area {
    int16_t x1, y1, x2, y2;
};

// Buttons at most this far from an edge reach it (the page margin).
static constexpr int16_t EDGE_REACH = 8;

// Tap area of a button: its own area, extended to the screen edge on each side at most `reach`
// px from it. The sides towards neighbors stay, so the gaps between buttons stay free.
Area edgeArea(const Area& button, int16_t reach, int16_t screenW, int16_t screenH);
bool contains(const Area& a, int16_t x, int16_t y);

}
