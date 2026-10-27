#pragma once

#include <Arduino.h>

namespace FrequencyUtils {
uint64_t clamp(uint64_t hz);
void format(uint64_t hz, char* out, size_t outSize);
void formatStep(uint32_t hz, char* out, size_t outSize);
void formatPpm(int32_t centiPpm, char* out, size_t outSize);
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
