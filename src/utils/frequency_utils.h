#pragma once

#include <stddef.h>
#include <stdint.h>
#include <types.h>

namespace FrequencyUtils {
// Deviations of the actual output frequency from this size on are shown as a warning.
constexpr double DEVIATION_WARN_HZ = 1.0;

uint64_t clamp(uint64_t hz);
void format(uint64_t hz, char* out, size_t outSize);
// MHz with a fixed number of decimals (0..6, rounded): 4730000, 3 -> "4.730"; 100000000, 0 -> "100".
void formatMHz(uint64_t hz, uint8_t decimals, char* out, size_t outSize);
void formatStep(uint32_t hz, char* out, size_t outSize);
void formatPpm(int32_t centiPpm, char* out, size_t outSize);
// "Off", "-0.5 %", "-1.5 %", "+/-0.5 %"
const char* spreadLabel(Spread spread);
// actual - requested as shown: rounded to 0.1 Hz, never -0.0.
double deviation(double actualHz, uint64_t requestedHz);
// Info line of the channel pages: "Out 10.000 000 MHz (+0.3 Hz)", the actual frequency rounded
// to 1 Hz and its deviation(). With spread spectrum the mean frequency instead:
// "Out 9.975 000 MHz mean, SSC -0.5 %". "Out -" without an actual frequency (0).
// Returns the deviation shown (0 with spread spectrum or without an actual frequency).
double formatOutput(double actualHz, uint64_t requestedHz, Spread spread, char* out, size_t outSize);
// `text`: digits with at most one decimal point; `unit`: "Hz" (or nullptr), "kHz", "MHz".
// Returns false for anything else or a result outside FREQ_MIN_HZ..FREQ_MAX_HZ.
bool parseFrequency(const char* text, const char* unit, uint64_t* outHz);
// Remote input (V-11: REST, SCPI, web field): an integer in Hz ("10000000", "7 100 000") or a
// decimal number with a unit (Hz, kHz, MHz, k, M, any case: "10.7 MHz", "10,7MHZ", "455k").
// Spaces only between groups of three digits and around the unit; no sign, exponent or hex.
// Computed in integers, rounded half up to 1 Hz. false outside FREQ_MIN_HZ..FREQ_MAX_HZ; then
// *outOfRange (if given) is true, for text that is no frequency at all false (REST: -222 / -224).
bool parseRemoteFrequency(const char* text, uint64_t* outHz, bool* outOfRange = nullptr);
uint8_t stepIndex(uint32_t step);
// A limit of the CH3 IQ range in MHz with `decimals` places, rounded towards the inside of the
// range (lower limit up, upper limit down), so the shown value is always reachable.
// trimWhole: a whole number of MHz without decimals ("100" instead of "100.000").
void formatIqLimit(uint64_t hz, bool lowerLimit, uint8_t decimals, bool trimWhole, char* out, size_t outSize);
// For text from format(): decimal exponent of the digit at index (= digits to its right),
// -1 for anything else.
int digitExponent(const char* text, size_t index);
// Exponent of the largest power of ten <= value (value >= 1).
uint8_t decimalExponent(uint32_t value);
uint32_t powerOfTen(uint8_t exponent);
uint32_t snapStep(uint32_t step);
}
