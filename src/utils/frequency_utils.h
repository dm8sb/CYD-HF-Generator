#pragma once

#include <Arduino.h>

namespace FrequencyUtils {
uint64_t clamp(uint64_t hz);
uint64_t applyCorrection(uint64_t hz, int32_t correctionPpmOrHz);
uint64_t nearestSi5351Hz(uint64_t hz);
void format(uint64_t hz, char* out, size_t outSize);
bool parseFrequency(const char* text, const char* unit, uint64_t* outHz);
}
