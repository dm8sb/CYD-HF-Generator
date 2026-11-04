#pragma once

#include <types.h>

// State texts of MAIN and the page headers (audit U-1): both show the actual state, the
// switches show the request. Pure functions, host-tested in test_input_logic.
namespace StatusText {

enum class Tone : uint8_t { Muted, Ok, Warn, Error };

struct Text {
    const char* text;
    Tone tone;
};

Text channel(ChannelStatus s);
Text ch3(Ch3Status s);

}
