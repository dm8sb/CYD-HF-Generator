#include "frequency_utils.h"
#include <constants.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace FrequencyUtils {

uint64_t clamp(uint64_t hz) {
    if (hz < FREQ_MIN_HZ) return FREQ_MIN_HZ;
    if (hz > FREQ_MAX_HZ) return FREQ_MAX_HZ;
    return hz;
}

void format(uint64_t hz, char* out, size_t outSize) {
    if (!out || outSize == 0) return;

    // HF generator range is 1-100 MHz. Display as MHz with grouped
    // fractional Hz digits: 1.000 000 MHz, 10.700 000 MHz, 100.000 000 MHz.
    uint64_t wholeMHz = hz / 1000000ULL;
    uint64_t fracHz = hz % 1000000ULL;
    uint64_t fracK = fracHz / 1000ULL;
    uint64_t fracR = fracHz % 1000ULL;
    snprintf(out, outSize, "%llu.%03llu %03llu MHz",
             (unsigned long long)wholeMHz,
             (unsigned long long)fracK,
             (unsigned long long)fracR);
}

void formatStep(uint32_t hz, char* out, size_t outSize) {
    if (hz >= 1000000UL && hz % 1000000UL == 0) snprintf(out, outSize, "%lu MHz", (unsigned long)(hz / 1000000UL));
    else if (hz >= 1000UL && hz % 1000UL == 0) snprintf(out, outSize, "%lu kHz", (unsigned long)(hz / 1000UL));
    else snprintf(out, outSize, "%lu Hz", (unsigned long)hz);
}

void formatPpm(int32_t centiPpm, char* out, size_t outSize) {
    int32_t a = centiPpm < 0 ? -centiPpm : centiPpm;
    snprintf(out, outSize, "%c%ld.%02ld ppm", centiPpm < 0 ? '-' : '+', (long)(a / 100), (long)(a % 100));
}

bool parseFrequency(const char* text, const char* unit, uint64_t* outHz) {
    if (!text || !outHz) return false;
    char* end = nullptr;
    double v = strtod(text, &end);
    if (end == text || *end != '\0' || v <= 0) return false;
    double scale = 1.0;
    if (unit) {
        if (strcmp(unit, "kHz") == 0) scale = 1000.0;
        else if (strcmp(unit, "MHz") == 0) scale = 1000000.0;
    }
    double hz = v * scale + 0.5;
    if (hz < (double)FREQ_MIN_HZ || hz >= (double)FREQ_MAX_HZ + 1.0) return false;
    *outHz = (uint64_t)hz;
    return true;
}

uint8_t stepIndex(uint32_t step) {
    uint8_t best = 0;
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) {
        if (DELTA_STEPS[i] <= step) best = i;
    }
    return best;
}

uint32_t snapStep(uint32_t step) { return DELTA_STEPS[stepIndex(step)]; }

int digitExponent(const char* text, size_t index) {
    if (!text || index >= strlen(text) || !isdigit((unsigned char)text[index])) return -1;
    int exponent = 0;
    for (const char* p = text + index + 1; *p; ++p) {
        if (isdigit((unsigned char)*p)) ++exponent;
    }
    return exponent;
}

uint8_t decimalExponent(uint32_t value) {
    uint8_t exponent = 0;
    while (value >= 10) {
        value /= 10;
        ++exponent;
    }
    return exponent;
}

uint32_t powerOfTen(uint8_t exponent) {
    uint32_t v = 1;
    while (exponent--) v *= 10;
    return v;
}

}
