// Fuzz target remote_frequency: FrequencyUtils::parseRemoteFrequency (V-11: REST, web field).
// Input: the text (cut at the first NUL, it is a C string).
// Invariants: true only within FREQ_MIN_HZ..FREQ_MAX_HZ, the value equals the number read
// independently (digits, ',' or '.', unit k/M/Hz/kHz/MHz) rounded half up to 1 Hz; no sign,
// exponent or hex is accepted; blanks around the text change nothing; false leaves *out alone.

#include "fuzz_common.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string>
#include <constants.h>
#include <utils/frequency_utils.h>

namespace fuzz_remote_frequency {

// The accepted text read independently: digits with blanks between them, one '.' or ',', blanks,
// a unit. Returns the value in Hz (unrounded) or a negative number if it is not of that form.
long double reference(const std::string& s) {
    std::string number;
    size_t i = 0;
    while (i < s.size() && s[i] == ' ') ++i;
    while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == ' ' || s[i] == '.' || s[i] == ',')) {
        if (s[i] != ' ') number += s[i] == ',' ? '.' : s[i];
        ++i;
    }
    std::string unit;
    while (i < s.size() && s[i] != ' ') unit += (char)tolower((unsigned char)s[i++]);
    while (i < s.size() && s[i] == ' ') ++i;
    if (i != s.size() || number.empty()) return -1;
    long double scale;
    if (unit.empty() || unit == "hz") scale = 1;
    else if (unit == "khz" || unit == "k") scale = 1e3L;
    else if (unit == "mhz" || unit == "m") scale = 1e6L;
    else return -1;
    char* end = nullptr;
    const long double v = strtold(number.c_str(), &end);
    if (!end || *end) return -1;
    return v * scale;
}

int run(const uint8_t* data, size_t size) {
    std::string text(reinterpret_cast<const char*>(data), size);
    const size_t nul = text.find('\0');
    if (nul != std::string::npos) text.resize(nul);

    uint64_t hz = 4242;
    const bool ok = FrequencyUtils::parseRemoteFrequency(text.c_str(), &hz);
    if (!ok) {
        FUZZ_CHECK(hz == 4242);
    } else {
        FUZZ_CHECK(hz >= FREQ_MIN_HZ && hz <= FREQ_MAX_HZ);
        for (size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            FUZZ_CHECK(c != '+' && c != '-' && c != 'e' && c != 'E' && c != 'x' && c != 'X' && c != '\t');
        }
        const long double ref = reference(text);
        FUZZ_CHECK(ref >= 0);
        // Half up: ref - 0.5 < hz <= ref + 0.5 (with room for the long double conversion).
        FUZZ_CHECK((long double)hz > ref - 0.5L - 1e-6L && (long double)hz <= ref + 0.5L + 1e-6L);
        // Blanks around the text change nothing.
        uint64_t again = 0;
        FUZZ_CHECK(FrequencyUtils::parseRemoteFrequency(("  " + text + "  ").c_str(), &again) && again == hz);
    }
    FUZZ_CHECK(!FrequencyUtils::parseRemoteFrequency(nullptr, &hz));
    FUZZ_CHECK(!FrequencyUtils::parseRemoteFrequency(text.c_str(), nullptr));
    return 0;
}

} // namespace fuzz_remote_frequency

FUZZ_TARGET(remote_frequency, fuzz_remote_frequency::run)
