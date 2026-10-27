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
uint32_t snapStep(uint32_t step);
}
