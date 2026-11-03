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
// Returns false for unparsable input or a result outside FREQ_MIN_HZ..FREQ_MAX_HZ.
bool parseFrequency(const char* text, const char* unit, uint64_t* outHz);
uint8_t stepIndex(uint32_t step);
// For text from format(): decimal exponent of the digit at index (= digits to its right),
// -1 for anything else.
int digitExponent(const char* text, size_t index);
// Exponent of the largest power of ten <= value (value >= 1).
uint8_t decimalExponent(uint32_t value);
uint32_t powerOfTen(uint8_t exponent);
uint32_t snapStep(uint32_t step);
}
