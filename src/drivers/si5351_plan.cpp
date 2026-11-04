#include "si5351_plan.h"
#include <constants.h>
#include <math.h>

static constexpr uint32_t MAX_DENOM = 1048575;    // 20-bit fraction denominator
static constexpr uint64_t XTAL_HZ = 25000000;
static constexpr uint64_t VCO_MIN_HZ = 600000000ULL;
static constexpr uint64_t VCO_MAX_HZ = 900000000ULL;
// x 25 MHz = 900 / 887.5 / 875 MHz; the first two inside the VCO range are used.
static constexpr SynthRatio FIXED_PLLS[] = {{36, 0, 1}, {35, 1, 2}, {35, 0, 1}};
static constexpr double FIXED_PLL_KEEP_ERR_HZ = 0.25;
static constexpr uint32_t MS_MIN_HZ = 1000000;    // lower outputs use the R divider
static constexpr uint32_t MS_MAX_DIV = 2048;
static constexpr uint32_t IQ_MIN_DIV = 9;         // fractional-mode MultiSynth
static constexpr uint32_t IQ_MAX_DIV = 127;       // 7-bit phase offset register

static uint64_t absDiff(uint64_t a, uint64_t b) { return a > b ? a - b : b - a; }

// Range of the unmodulated VCO, so that the spread spectrum modulation stays within 600..900 MHz.
struct VcoRange {
    uint64_t min;
    uint64_t max;
    bool contains(uint64_t vco) const { return vco >= min && vco <= max; }
};

static VcoRange vcoRange(Spread s) {
    const double amp = spreadAmplitude(s);
    if (amp == 0.0) return VcoRange {VCO_MIN_HZ, VCO_MAX_HZ};
    return VcoRange {(uint64_t)ceil(VCO_MIN_HZ / (1.0 - amp)),
                     isCenterSpread(s) ? (uint64_t)floor(VCO_MAX_HZ / (1.0 + amp)) : VCO_MAX_HZ};
}

// num / den as a + b / c, with b / c the best rational approximation for c <= MAX_DENOM
// (continued fractions). Error is far below 1 mHz at the output.
static SynthRatio approxRatio(uint64_t num, uint64_t den) {
    SynthRatio r {(uint32_t)(num / den), 0, 1};
    const uint64_t xn = num % den;
    const uint64_t xd = den;
    if (xn == 0) return r;

    uint64_t n = xn, d = xd;
    uint64_t h2 = 0, k2 = 1, h1 = 1, k1 = 0;
    while (d != 0) {
        const uint64_t q = n / d;
        const uint64_t k = q * k1 + k2;
        if (k > MAX_DENOM) {
            // Semiconvergent with the largest allowed denominator, if it is closer.
            const uint64_t m = (MAX_DENOM - k2) / k1;
            const uint64_t hs = m * h1 + h2;
            const uint64_t ks = m * k1 + k2;
            if (m > 0 && absDiff(hs * xd, xn * ks) * k1 < absDiff(h1 * xd, xn * k1) * ks) {
                h1 = hs;
                k1 = ks;
            }
            break;
        }
        const uint64_t h = q * h1 + h2;
        h2 = h1;
        k2 = k1;
        h1 = h;
        k1 = k;
        const uint64_t rem = n % d;
        n = d;
        d = rem;
    }
    r.b = (uint32_t)h1;
    r.c = (uint32_t)k1;
    if (r.b >= r.c) {
        r.a += 1;
        r.b = 0;
        r.c = 1;
    }
    return r;
}

static double synthesizedHz(const SynthRatio& pll, const SynthRatio& ms, uint8_t rLog2, uint64_t xtal4) {
    const double vco = (double)xtal4 / 4.0 * ((double)pll.a + (double)pll.b / (double)pll.c);
    return vco / ((double)ms.a + (double)ms.b / (double)ms.c) / (double)(1U << rLog2);
}

static bool sameRatio(const SynthRatio& x, const SynthRatio& y) { return x.a == y.a && x.b == y.b && x.c == y.c; }

// Plan with an even integer MultiSynth divider `div` and a fractional PLL for msHz.
static void integerDivider(SynthPlan& p, uint32_t div, uint64_t msHz, uint64_t xtal4) {
    p.ms = SynthRatio {div, 0, 1};
    p.divBy4 = div == 4;
    p.msInt = true;
    p.pll = approxRatio(4 * (uint64_t)div * msHz, xtal4);
}

// The point xtal * a / div nearest to msHz with an integer PLL a (24..36 x 25 MHz within the VCO
// range) and div = divMin, divMin + divStep, ... <= divMax. On a tie the even a wins (FBx_INT).
static void nearestIntegerPoint(SynthPlan& p, uint64_t msHz, uint64_t xtal4, const VcoRange& range, uint32_t divMin,
                                uint32_t divMax, uint32_t divStep) {
    // From the corrected crystal (xtal4 / 4), so the VCO stays in range at +-50 ppm (audit C-7).
    const uint32_t aMin = (uint32_t)((4 * range.min + xtal4 - 1) / xtal4);
    const uint32_t aMax = (uint32_t)(4 * range.max / xtal4);
    uint64_t bestErr = 0;
    uint32_t bestA = 0, bestDiv = 0;
    for (uint32_t div = divMin; div <= divMax; div += divStep) {
        const uint64_t vco4 = 4 * msHz * div; // wanted VCO in 0.25 Hz
        const uint64_t nearest = (2 * vco4 + xtal4) / (2 * xtal4);
        const uint32_t a = nearest < aMin ? aMin : nearest > aMax ? aMax : (uint32_t)nearest;
        // Output error = err / (4 * div); compared without division.
        const uint64_t err = absDiff(xtal4 * a, vco4);
        const uint64_t lhs = err * bestDiv;
        const uint64_t rhs = bestErr * div;
        if (bestDiv == 0 || lhs < rhs || (lhs == rhs && a % 2 == 0 && bestA % 2 != 0)) {
            bestErr = err;
            bestA = a;
            bestDiv = div;
        }
        // Larger dividers only move further below msHz.
        if (nearest > aMax) break;
    }
    p.pll = SynthRatio {bestA, 0, 1};
    p.ms = SynthRatio {bestDiv, 0, 1};
    p.divBy4 = bestDiv == 4;
}

SynthPlan planStandalone(uint64_t freq, TuningMode mode, const SynthRatio* currentPll, const SynthRatio* currentMs,
                         uint64_t xtal4, Spread spread) {
    SynthPlan p {};
    p.ms = p.pll = SynthRatio {0, 0, 1};
    if (freq == 0) return p;
    p.ok = true;
    const VcoRange range = vcoRange(spread);

    // R divider for low frequencies keeps the MultiSynth input >= 1 MHz.
    uint64_t msHz = freq;
    while (msHz < MS_MIN_HZ && p.rLog2 < 7) {
        msHz <<= 1;
        ++p.rLog2;
    }

    if (mode == TuningMode::LowNoise) {
        // Keep the divider while the VCO stays in range: then only the PLL changes, without a reset.
        uint64_t div = 0;
        if (currentMs && isEvenInteger(*currentMs) && currentMs->a >= 4 && range.contains(currentMs->a * msHz)) {
            div = currentMs->a;
        }
        if (div == 0) div = (range.max / msHz) & ~1ULL; // largest even divider
        if (div < 4 || div > MS_MAX_DIV || div * msHz < range.min) {
            p.ok = false;
            return p;
        }
        integerDivider(p, (uint32_t)div, msHz, xtal4);
    } else if (mode == TuningMode::Integer) {
        nearestIntegerPoint(p, msHz, xtal4, range, 4, MS_MAX_DIV, 2);
        p.msInt = true;
    } else {
        // Fixed PLL, fractional MultiSynth. Near 900 MHz / n the MultiSynth fraction cannot get
        // close enough (up to 5 Hz error at 100 MHz); the second PLL value has its weak spots
        // elsewhere. The current PLL is kept while it is good enough, so the PLL rarely changes.
        double bestErr = 1e9;
        uint8_t used = 0;
        for (const SynthRatio& cand : FIXED_PLLS) {
            if (!range.contains(XTAL_HZ * ((uint64_t)cand.a * cand.c + cand.b) / cand.c)) continue;
            if (++used > 2) break;
            const SynthRatio ms = approxRatio(((uint64_t)cand.a * cand.c + cand.b) * xtal4, 4 * msHz * cand.c);
            if (ms.a < 8) continue;
            const double err = fabs(synthesizedHz(cand, ms, p.rLog2, xtal4) - (double)freq);
            if (currentPll && sameRatio(*currentPll, cand) && err <= FIXED_PLL_KEEP_ERR_HZ) {
                p.pll = cand;
                p.ms = ms;
                break;
            }
            if (err < bestErr) {
                bestErr = err;
                p.pll = cand;
                p.ms = ms;
            }
        }
        if (p.ms.a < 8) {
            // Above 112.5 MHz: even integer MultiSynth divider, fractional PLL.
            const uint32_t div = (6 * msHz <= range.max) ? 6 : 4;
            if (div * msHz < range.min) {
                p.ok = false; // only with spread spectrum, next to 150 MHz
                return p;
            }
            integerDivider(p, div, msHz, xtal4);
        }
        p.msInt = isEvenInteger(p.ms);
    }
    p.actualHz = synthesizedHz(p.pll, p.ms, p.rLog2, xtal4);
    return p;
}

SynthPlan planIQ(uint64_t freq, TuningMode mode, uint64_t xtal4, Spread spread) {
    SynthPlan p {};
    p.ms = p.pll = SynthRatio {0, 0, 1};
    if (freq == 0) return p;
    const VcoRange range = vcoRange(spread);
    if (mode == TuningMode::Integer) {
        nearestIntegerPoint(p, freq, xtal4, range, IQ_MIN_DIV, IQ_MAX_DIV, 1);
    } else {
        // Integer divider (fractional mode) with the VCO as close to the top of its range as possible.
        uint64_t div = range.max / freq;
        if (div > IQ_MAX_DIV) div = IQ_MAX_DIV;
        if (div < IQ_MIN_DIV || div * freq < range.min) return p;
        p.ms = SynthRatio {(uint32_t)div, 0, 1};
        p.pll = approxRatio(4 * div * freq, xtal4);
    }
    p.ok = true;
    p.actualHz = synthesizedHz(p.pll, p.ms, 0, xtal4);
    return p;
}

uint64_t iqMinHz(Spread spread) {
    // Divider 127 at the bottom of the VCO range.
    const uint64_t lo = (vcoRange(spread).min + IQ_MAX_DIV - 1) / IQ_MAX_DIV;
    return lo > IQ_MIN_HZ ? lo : IQ_MIN_HZ;
}

uint64_t iqMaxHz(Spread spread) {
    // Divider 9 at the top of the VCO range.
    const uint64_t hi = vcoRange(spread).max / IQ_MIN_DIV;
    return hi < IQ_MAX_HZ ? hi : IQ_MAX_HZ;
}
