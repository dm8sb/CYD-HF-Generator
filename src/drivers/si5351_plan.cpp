#include "si5351_plan.h"
#include <math.h>

static constexpr uint32_t MAX_DENOM = 1048575;    // 20-bit fraction denominator
static constexpr uint64_t VCO_MIN_HZ = 600000000ULL;
static constexpr uint64_t VCO_MAX_HZ = 900000000ULL;
static constexpr SynthRatio FIXED_PLLS[] = {{36, 0, 1}, {35, 1, 2}}; // x 25 MHz = 900 / 887.5 MHz
static constexpr double FIXED_PLL_KEEP_ERR_HZ = 0.25;
static constexpr uint32_t MS_MIN_HZ = 1000000;    // lower outputs use the R divider
static constexpr uint32_t MS_MAX_DIV = 2048;
static constexpr uint32_t PLL_INT_MIN = 24;       // integer PLL: 600..900 MHz
static constexpr uint32_t PLL_INT_MAX = 36;
static constexpr uint32_t IQ_MIN_DIV = 9;         // fractional-mode MultiSynth
static constexpr uint32_t IQ_MAX_DIV = 127;       // 7-bit phase offset register

static uint64_t absDiff(uint64_t a, uint64_t b) { return a > b ? a - b : b - a; }

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

// The point xtal * a / div nearest to msHz with an integer PLL a = 24..36 and div = divMin,
// divMin + divStep, ... <= divMax. On a tie the even a wins (FBx_INT).
static void nearestIntegerPoint(SynthPlan& p, uint64_t msHz, uint64_t xtal4, uint32_t divMin, uint32_t divMax,
                                uint32_t divStep) {
    uint64_t bestErr = 0;
    uint32_t bestA = 0, bestDiv = 0;
    for (uint32_t div = divMin; div <= divMax; div += divStep) {
        const uint64_t vco4 = 4 * msHz * div; // wanted VCO in 0.25 Hz
        const uint64_t nearest = (2 * vco4 + xtal4) / (2 * xtal4);
        const uint32_t a = nearest < PLL_INT_MIN ? PLL_INT_MIN : nearest > PLL_INT_MAX ? PLL_INT_MAX : (uint32_t)nearest;
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
        if (nearest > PLL_INT_MAX) break;
    }
    p.pll = SynthRatio {bestA, 0, 1};
    p.ms = SynthRatio {bestDiv, 0, 1};
    p.divBy4 = bestDiv == 4;
}

SynthPlan planStandalone(uint64_t freq, TuningMode mode, const SynthRatio* currentPll, const SynthRatio* currentMs,
                         uint64_t xtal4) {
    SynthPlan p {};
    p.ms = p.pll = SynthRatio {0, 0, 1};
    if (freq == 0) return p;
    p.ok = true;

    // R divider for low frequencies keeps the MultiSynth input >= 1 MHz.
    uint64_t msHz = freq;
    while (msHz < MS_MIN_HZ && p.rLog2 < 7) {
        msHz <<= 1;
        ++p.rLog2;
    }

    if (mode == TuningMode::LowNoise) {
        // Keep the divider while the VCO stays in range: then only the PLL changes, without a reset.
        uint64_t div = 0;
        if (currentMs && isEvenInteger(*currentMs) && currentMs->a >= 4) {
            const uint64_t vco = currentMs->a * msHz;
            if (vco >= VCO_MIN_HZ && vco <= VCO_MAX_HZ) div = currentMs->a;
        }
        if (div == 0) div = (VCO_MAX_HZ / msHz) & ~1ULL; // largest even divider
        if (div < 4 || div > MS_MAX_DIV || div * msHz < VCO_MIN_HZ) {
            p.ok = false;
            return p;
        }
        integerDivider(p, (uint32_t)div, msHz, xtal4);
    } else if (mode == TuningMode::Integer) {
        nearestIntegerPoint(p, msHz, xtal4, 4, MS_MAX_DIV, 2);
        p.msInt = true;
    } else {
        // Fixed PLL, fractional MultiSynth. Near 900 MHz / n the MultiSynth fraction cannot get
        // close enough (up to 5 Hz error at 100 MHz); the second PLL value has its weak spots
        // elsewhere. The current PLL is kept while it is good enough, so the PLL rarely changes.
        double bestErr = 1e9;
        for (const SynthRatio& cand : FIXED_PLLS) {
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
            integerDivider(p, (6 * msHz <= VCO_MAX_HZ) ? 6 : 4, msHz, xtal4);
        }
        p.msInt = isEvenInteger(p.ms);
    }
    p.actualHz = synthesizedHz(p.pll, p.ms, p.rLog2, xtal4);
    return p;
}

SynthPlan planIQ(uint64_t freq, TuningMode mode, uint64_t xtal4) {
    SynthPlan p {};
    p.ms = p.pll = SynthRatio {0, 0, 1};
    if (freq == 0) return p;
    if (mode == TuningMode::Integer) {
        nearestIntegerPoint(p, freq, xtal4, IQ_MIN_DIV, IQ_MAX_DIV, 1);
    } else {
        // Integer divider (fractional mode) with the VCO as close to 900 MHz as possible.
        uint64_t div = VCO_MAX_HZ / freq;
        if (div > IQ_MAX_DIV) div = IQ_MAX_DIV;
        if (div < IQ_MIN_DIV || div * freq < VCO_MIN_HZ) return p;
        p.ms = SynthRatio {(uint32_t)div, 0, 1};
        p.pll = approxRatio(4 * div * freq, xtal4);
    }
    p.ok = true;
    p.actualHz = synthesizedHz(p.pll, p.ms, 0, xtal4);
    return p;
}
