#pragma once

#include <stdint.h>
#include <types.h>

// Frequency planning for the SI5351A (AN619) without I2C: PLL, MultiSynth and R divider for
// a frequency in each TuningMode. Frequencies in Hz; xtal4 is the corrected crystal frequency
// in units of 0.25 Hz (25 MHz = 100 000 000).

// value = a + b / c
struct SynthRatio {
    uint32_t a;
    uint32_t b;
    uint32_t c;
};

struct SynthPlan {
    bool ok;         // false: no valid divider for this frequency
    SynthRatio pll;  // VCO = crystal * pll
    SynthRatio ms;   // MultiSynth divider
    uint8_t rLog2;   // R divider = 2^rLog2
    bool divBy4;     // MultiSynth 4 (special encoding)
    bool msInt;      // MS_INT: even integer MultiSynth without phase offset
    double actualHz; // output frequency, assuming the crystal correction is right
};

// Integer mode (FBx_INT, MSx_INT) is allowed for even integer ratios only (AN619 3.2.1, 4.1.2.1).
inline bool isEvenInteger(const SynthRatio& r) { return r.b == 0 && r.a % 2 == 0; }

// Spread spectrum amplitude (0.005 = 0.5 %) and mode of an option.
inline double spreadAmplitude(Spread s) { return s == Spread::Off ? 0.0 : s == Spread::Down15 ? 0.015 : 0.005; }
inline bool isCenterSpread(Spread s) { return s == Spread::Center05; }

// VCO range: 600..900 MHz. With spread spectrum (`spread`, PLL A only) the modulated VCO must stay
// inside it, so the unmodulated VCO is limited to >= 600 MHz / (1 - amp), and for center spread
// also to <= 900 MHz / (1 + amp). This can shrink the frequency range of LowNoise and IQ.

// One output with its own PLL. Below 1 MHz the R divider keeps the MultiSynth at >= 1 MHz.
// - Fractional: fixed PLL, fractional MultiSynth: the first two of 900, 887.5 and 875 MHz in the
//   VCO range (900 / 887.5 MHz without center spread). Keeps currentPll while its error is
//   <= 0.25 Hz, so tuning rarely changes the PLL. Error <= 0.5 Hz up to 100 MHz.
// - LowNoise: even integer MultiSynth, fractional PLL. Keeps currentMs while the VCO stays in
//   range, otherwise the largest even divider. Error <= 1.5 Hz up to 100 MHz.
// - Integer: integer PLL (24..36 x crystal) and even integer MultiSynth: the point
//   crystal * a / (ms * R) nearest to freq; on a tie the one with an even PLL ratio (FBx_INT).
// currentPll / currentMs: what the PLL and the MultiSynth run with now, nullptr if unknown.
SynthPlan planStandalone(uint64_t freq, TuningMode mode, const SynthRatio* currentPll, const SynthRatio* currentMs,
                         uint64_t xtal4, Spread spread = Spread::Off);

// IQ pair: master and follower share the PLL and an integer MultiSynth divider of 9..127 (the
// 7-bit phase offset register holds the divider for 90 deg; MS_INT stays off). Fractional and
// LowNoise: VCO as close to the top of its range as possible, fractional PLL, error <= 1.35 Hz.
// Integer: integer PLL, the nearest point crystal * a / divider.
SynthPlan planIQ(uint64_t freq, TuningMode mode, uint64_t xtal4, Spread spread = Spread::Off);

// Frequency range in which planIQ() succeeds: IQ_MIN_HZ..IQ_MAX_HZ, narrower with spread spectrum
// (e.g. 4.748 151 MHz with -0.5 %, 99.502 487 MHz at the top with +/-0.5 %).
uint64_t iqMinHz(Spread spread);
uint64_t iqMaxHz(Spread spread);
