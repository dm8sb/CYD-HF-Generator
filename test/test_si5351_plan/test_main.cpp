// planStandalone() / planIQ() as pure functions: divider choice per tuning mode, hysteresis,
// raster points and limits. Register encoding and I2C are covered by test_si5351.

#include <unity.h>
#include <constants.h>
#include <drivers/si5351_plan.h>
#include <math.h>
#include <stdio.h>

static constexpr uint64_t XTAL4 = 100000000ULL; // 25 MHz in 0.25 Hz
static const uint64_t XTALS[] = {XTAL4, XTAL4 + CORRECTION_MAX, XTAL4 - CORRECTION_MAX, XTAL4 + 970};
static char msg[160];

void setUp() {}
void tearDown() {}

static double value(const SynthRatio& r) { return r.a + (double)r.b / r.c; }
// Output frequency of a plan, computed here from its ratios.
static double outHz(const SynthPlan& p, uint64_t xtal4) {
    return xtal4 / 4.0 * value(p.pll) / value(p.ms) / (double)(1U << p.rLog2);
}
static double vcoHz(const SynthPlan& p, uint64_t xtal4) { return xtal4 / 4.0 * value(p.pll); }

static uint32_t lcgState = 1;
static uint64_t randomHz(uint64_t lo, uint64_t hi) {
    lcgState = lcgState * 1664525u + 1013904223u;
    const uint32_t a = lcgState;
    lcgState = lcgState * 1664525u + 1013904223u;
    return lo + (((uint64_t)a << 32) | lcgState) % (hi - lo + 1);
}

// Distance to the nearest point crystal * a / div by brute force; a is limited to a VCO of
// 600..900 MHz with the corrected crystal (24..36 at correction 0).
static double rasterErr(uint64_t msHz, uint64_t xtal4, uint32_t divMin, uint32_t divMax, uint32_t divStep) {
    double best = 1e18;
    for (uint32_t a = 23; a <= 37; ++a) {
        const double vco = xtal4 / 4.0 * a;
        if (vco < 600e6 || vco > 900e6) continue;
        for (uint32_t div = divMin; div <= divMax; div += divStep) {
            best = fmin(best, fabs(xtal4 / 4.0 * a / div - (double)msHz));
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Fractional

static void test_fractional_uses_a_fixed_pll() {
    SynthPlan p = planStandalone(10000000, TuningMode::Fractional, nullptr, nullptr, XTAL4);
    TEST_ASSERT_TRUE(p.ok);
    TEST_ASSERT_EQUAL_UINT32(36, p.pll.a);
    TEST_ASSERT_EQUAL_UINT32(0, p.pll.b);
    TEST_ASSERT_EQUAL_UINT32(90, p.ms.a);
    TEST_ASSERT_EQUAL_UINT32(0, p.ms.b);
    TEST_ASSERT_TRUE(p.msInt);
    TEST_ASSERT_EQUAL_DOUBLE(10e6, p.actualHz);
    p = planStandalone(14123457, TuningMode::Fractional, nullptr, nullptr, XTAL4);
    TEST_ASSERT_TRUE(value(p.pll) == 36.0 || value(p.pll) == 35.5);
    TEST_ASSERT_FALSE(p.msInt);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 14123457.0, p.actualHz);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, outHz(p, XTAL4), p.actualHz);
}

static void test_fractional_keeps_current_pll_while_good_enough() {
    // 10 MHz = 900 MHz / 90 = 887.5 MHz / 88.75, both exact: the current PLL stays.
    const SynthRatio pll887 {35, 1, 2};
    SynthPlan p = planStandalone(10000000, TuningMode::Fractional, nullptr, nullptr, XTAL4);
    TEST_ASSERT_EQUAL_DOUBLE(36.0, value(p.pll));
    p = planStandalone(10000000, TuningMode::Fractional, &pll887, nullptr, XTAL4);
    TEST_ASSERT_EQUAL_DOUBLE(35.5, value(p.pll));
    TEST_ASSERT_EQUAL_DOUBLE(88.75, value(p.ms));
    TEST_ASSERT_FALSE(p.msInt);
    TEST_ASSERT_EQUAL_DOUBLE(10e6, p.actualHz);
}

static void test_fractional_above_112_5_mhz_uses_an_integer_divider() {
    SynthPlan p = planStandalone(140000000, TuningMode::Fractional, nullptr, nullptr, XTAL4);
    TEST_ASSERT_EQUAL_DOUBLE(6.0, value(p.ms));
    TEST_ASSERT_TRUE(p.msInt);
    TEST_ASSERT_FALSE(p.divBy4);
    p = planStandalone(155000000, TuningMode::Fractional, nullptr, nullptr, XTAL4);
    TEST_ASSERT_EQUAL_DOUBLE(4.0, value(p.ms));
    TEST_ASSERT_TRUE(p.divBy4);
    TEST_ASSERT_DOUBLE_WITHIN(3.0, 155e6, p.actualHz);
}

// ---------------------------------------------------------------------------
// R divider, all modes

static void test_r_divider_below_1_mhz() {
    const TuningMode modes[] = {TuningMode::Fractional, TuningMode::LowNoise, TuningMode::Integer};
    for (TuningMode m : modes) {
        TEST_ASSERT_EQUAL_UINT8(7, planStandalone(8000, m, nullptr, nullptr, XTAL4).rLog2); // MultiSynth 1.024 MHz
        TEST_ASSERT_EQUAL_UINT8(1, planStandalone(999999, m, nullptr, nullptr, XTAL4).rLog2);
        TEST_ASSERT_EQUAL_UINT8(0, planStandalone(1000000, m, nullptr, nullptr, XTAL4).rLog2);
        const SynthPlan p = planStandalone(123456, m, nullptr, nullptr, XTAL4);
        TEST_ASSERT_TRUE(p.ok);
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, outHz(p, XTAL4), p.actualHz);
        if (m != TuningMode::Integer) TEST_ASSERT_DOUBLE_WITHIN(0.5, 123456.0, p.actualHz);
    }
    TEST_ASSERT_FALSE(planStandalone(0, TuningMode::LowNoise, nullptr, nullptr, XTAL4).ok);
}

// ---------------------------------------------------------------------------
// Low noise

static void test_low_noise_picks_the_largest_even_divider() {
    struct Case {
        uint64_t freq;
        uint32_t div;
    };
    const Case cases[] = {{10000000, 90},  {10000001, 88},  {7100000, 126},   {1000000, 900},     {99999999, 8},
                          {100000000, 8},  {112500000, 8},  {150000000, 6},   {150000001, 4},     {160000000, 4}};
    for (const Case& c : cases) {
        for (uint64_t xtal4 : XTALS) {
            const SynthPlan p = planStandalone(c.freq, TuningMode::LowNoise, nullptr, nullptr, xtal4);
            snprintf(msg, sizeof(msg), "%llu Hz, xtal4 %llu: divider %u", (unsigned long long)c.freq,
                     (unsigned long long)xtal4, p.ms.a);
            TEST_ASSERT_TRUE_MESSAGE(p.ok, msg);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(c.div, p.ms.a, msg);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, p.ms.b, msg);
            TEST_ASSERT_TRUE_MESSAGE(p.msInt, msg);
            TEST_ASSERT_EQUAL_MESSAGE(c.div == 4, p.divBy4, msg);
            // The PLL takes frequency and correction: VCO = divider x frequency.
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(15.0, (double)c.div * c.freq, vcoHz(p, xtal4), msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(c.freq <= 100000000 ? 1.5 : 3.0, (double)c.freq, p.actualHz, msg);
        }
    }
}

static void test_low_noise_keeps_divider_within_vco_range() {
    // Divider 90: VCO 600..900 MHz for 6.666 667..10 MHz.
    const SynthRatio d90 {90, 0, 1};
    TEST_ASSERT_EQUAL_UINT32(90, planStandalone(10000000, TuningMode::LowNoise, nullptr, &d90, XTAL4).ms.a);
    TEST_ASSERT_EQUAL_UINT32(90, planStandalone(6666667, TuningMode::LowNoise, nullptr, &d90, XTAL4).ms.a);
    TEST_ASSERT_EQUAL_UINT32(90, planStandalone(8000000, TuningMode::LowNoise, nullptr, &d90, XTAL4).ms.a);
    // Outside: the largest even divider again.
    TEST_ASSERT_EQUAL_UINT32(134, planStandalone(6666666, TuningMode::LowNoise, nullptr, &d90, XTAL4).ms.a);
    TEST_ASSERT_EQUAL_UINT32(88, planStandalone(10000001, TuningMode::LowNoise, nullptr, &d90, XTAL4).ms.a);
    // Without a current divider the same 8 MHz gets 112.
    TEST_ASSERT_EQUAL_UINT32(112, planStandalone(8000000, TuningMode::LowNoise, nullptr, nullptr, XTAL4).ms.a);
    // Only an even integer divider is kept.
    const SynthRatio odd {91, 0, 1};
    const SynthRatio fraction {90, 1, 2};
    TEST_ASSERT_EQUAL_UINT32(112, planStandalone(8000000, TuningMode::LowNoise, nullptr, &odd, XTAL4).ms.a);
    TEST_ASSERT_EQUAL_UINT32(112, planStandalone(8000000, TuningMode::LowNoise, nullptr, &fraction, XTAL4).ms.a);
    // 4 (DIVBY4) is kept down to 150 MHz, 6 up to 150 MHz.
    const SynthRatio d4 {4, 0, 1};
    const SynthRatio d6 {6, 0, 1};
    TEST_ASSERT_TRUE(planStandalone(150000000, TuningMode::LowNoise, nullptr, &d4, XTAL4).divBy4);
    TEST_ASSERT_EQUAL_UINT32(6, planStandalone(149999999, TuningMode::LowNoise, nullptr, &d4, XTAL4).ms.a);
    TEST_ASSERT_EQUAL_UINT32(4, planStandalone(150000001, TuningMode::LowNoise, nullptr, &d6, XTAL4).ms.a);
}

static void test_low_noise_accuracy_with_hysteresis() {
    for (uint64_t xtal4 : XTALS) {
        SynthRatio current {0, 0, 1};
        uint64_t f = 10000000;
        lcgState = 3;
        for (int i = 0; i < 20000; ++i) {
            // Mostly small steps, so the divider is often kept.
            const int64_t step = (int64_t)(randomHz(0, 4) == 0 ? randomHz(1, 30000000) : randomHz(1, 200000));
            const int64_t next = (int64_t)f + (randomHz(0, 1) ? step : -step);
            f = (next < (int64_t)FREQ_MIN_HZ || next > (int64_t)FREQ_MAX_HZ) ? randomHz(FREQ_MIN_HZ, FREQ_MAX_HZ)
                                                                             : (uint64_t)next;
            const SynthPlan p = planStandalone(f, TuningMode::LowNoise, nullptr, i ? &current : nullptr, xtal4);
            snprintf(msg, sizeof(msg), "%llu Hz, xtal4 %llu: divider %u", (unsigned long long)f,
                     (unsigned long long)xtal4, p.ms.a);
            TEST_ASSERT_TRUE_MESSAGE(p.ok && isEvenInteger(p.ms) && p.msInt, msg);
            const double vco = (double)p.ms.a * f;
            TEST_ASSERT_TRUE_MESSAGE(vco >= 600e6 && vco <= 900e6, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1.5, (double)f, p.actualHz, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, outHz(p, xtal4), p.actualHz, msg);
            if (i && current.a != p.ms.a) {
                // A new divider only when the old one left the VCO range.
                const double old = (double)current.a * f;
                TEST_ASSERT_TRUE_MESSAGE(old < 600e6 || old > 900e6, msg);
            }
            current = p.ms;
        }
    }
}

// ---------------------------------------------------------------------------
// Integer

static void test_integer_example_7_1_mhz() {
    const SynthPlan p = planStandalone(7100000, TuningMode::Integer, nullptr, nullptr, XTAL4);
    TEST_ASSERT_TRUE(p.ok);
    TEST_ASSERT_EQUAL_UINT32(25, p.pll.a); // 25 MHz x 25 / 88 = 7.102 273 MHz
    TEST_ASSERT_EQUAL_UINT32(0, p.pll.b);
    TEST_ASSERT_EQUAL_UINT32(88, p.ms.a);
    TEST_ASSERT_TRUE(p.msInt);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 25e6 * 25 / 88, p.actualHz);
}

static void test_integer_picks_the_nearest_raster_point() {
    for (uint64_t xtal4 : XTALS) {
        lcgState = 5;
        for (int i = 0; i < 400; ++i) {
            const uint64_t f = i == 0 ? FREQ_MIN_HZ : i == 1 ? 160000000ULL : randomHz(FREQ_MIN_HZ, 160000000ULL);
            const SynthPlan p = planStandalone(f, TuningMode::Integer, nullptr, nullptr, xtal4);
            snprintf(msg, sizeof(msg), "%llu Hz, xtal4 %llu: %u / %u", (unsigned long long)f, (unsigned long long)xtal4,
                     p.pll.a, p.ms.a);
            TEST_ASSERT_TRUE_MESSAGE(p.ok, msg);
            TEST_ASSERT_TRUE_MESSAGE(p.pll.b == 0 && p.pll.a >= 24 && p.pll.a <= 36, msg);
            TEST_ASSERT_TRUE_MESSAGE(isEvenInteger(p.ms) && p.ms.a >= 4 && p.msInt, msg);
            TEST_ASSERT_EQUAL_MESSAGE(p.ms.a == 4, p.divBy4, msg);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, p.rLog2, msg);
            // actualHz is the raster point itself, from the corrected crystal.
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, xtal4 / 4.0 * p.pll.a / p.ms.a, p.actualHz, msg);
            TEST_ASSERT_TRUE_MESSAGE(fabs(p.actualHz - (double)f) <= rasterErr(f, xtal4, 4, 2048, 2) + 1e-6, msg);
        }
    }
}

// Audit C-7 / T-9: the integer PLL ratio is limited with the corrected crystal, so the VCO
// stays within 600..900 MHz also at +-50 ppm (25 MHz x 36 at +50 ppm would be 900.045 MHz).
static void test_integer_vco_in_range_with_correction() {
    for (uint64_t xtal4 : XTALS) {
        lcgState = 11;
        for (int i = 0; i < 300; ++i) {
            const uint64_t f = i == 0 ? 25000000ULL : i == 1 ? 6250000ULL : randomHz(FREQ_MIN_HZ, 160000000ULL);
            const SynthPlan p = planStandalone(f, TuningMode::Integer, nullptr, nullptr, xtal4);
            snprintf(msg, sizeof(msg), "%llu Hz, xtal4 %llu: a %u, VCO %.0f", (unsigned long long)f,
                     (unsigned long long)xtal4, p.pll.a, vcoHz(p, xtal4));
            TEST_ASSERT_TRUE_MESSAGE(vcoHz(p, xtal4) >= 600e6 && vcoHz(p, xtal4) <= 900e6, msg);
            if (f < IQ_MIN_HZ || f > IQ_MAX_HZ) continue;
            const SynthPlan q = planIQ(f, TuningMode::Integer, xtal4);
            snprintf(msg, sizeof(msg), "IQ %llu Hz, xtal4 %llu: VCO %.0f", (unsigned long long)f,
                     (unsigned long long)xtal4, vcoHz(q, xtal4));
            TEST_ASSERT_TRUE_MESSAGE(!q.ok || (vcoHz(q, xtal4) >= 600e6 && vcoHz(q, xtal4) <= 900e6), msg);
        }
    }
}

static void test_integer_tie_prefers_an_even_pll_ratio() {
    // 15.625 MHz = 25 MHz x 25 / 40 = x 30 / 48 = x 35 / 56: only the even ratio allows FBx_INT.
    const SynthPlan p = planStandalone(15625000, TuningMode::Integer, nullptr, nullptr, XTAL4);
    TEST_ASSERT_EQUAL_UINT32(30, p.pll.a);
    TEST_ASSERT_EQUAL_UINT32(48, p.ms.a);
    TEST_ASSERT_EQUAL_DOUBLE(15625000.0, p.actualHz);
    TEST_ASSERT_TRUE(isEvenInteger(p.pll));
}

static void test_integer_limits() {
    // Above 150 MHz only divider 4 (DIVBY4) reaches: 25 MHz x 26 / 4 = 162.5 MHz is nearest.
    SynthPlan p = planStandalone(160000000, TuningMode::Integer, nullptr, nullptr, XTAL4);
    TEST_ASSERT_TRUE(p.divBy4);
    TEST_ASSERT_EQUAL_UINT32(26, p.pll.a);
    // 1 MHz = 25 MHz x 24 / 600: divider far above 128.
    p = planStandalone(1000000, TuningMode::Integer, nullptr, nullptr, XTAL4);
    TEST_ASSERT_EQUAL_DOUBLE(1e6, p.actualHz);
    TEST_ASSERT_TRUE(p.ms.a >= 600 && p.ms.a <= 900);
}

// ---------------------------------------------------------------------------
// IQ

static void test_iq_fractional_and_low_noise_are_the_same() {
    struct Case {
        uint64_t freq;
        uint32_t div;
    };
    const Case cases[] = {{IQ_MIN_HZ, 127}, {10000000, 90}, {14200000, 63}, {IQ_MAX_HZ, 9}};
    for (const Case& c : cases) {
        for (uint64_t xtal4 : XTALS) {
            const SynthPlan f = planIQ(c.freq, TuningMode::Fractional, xtal4);
            const SynthPlan l = planIQ(c.freq, TuningMode::LowNoise, xtal4);
            TEST_ASSERT_TRUE(f.ok && l.ok);
            TEST_ASSERT_EQUAL_UINT32(c.div, f.ms.a);
            TEST_ASSERT_EQUAL_UINT32(0, f.ms.b);
            TEST_ASSERT_FALSE(f.msInt); // the phase offset needs fractional mode
            TEST_ASSERT_EQUAL_MEMORY(&f.pll, &l.pll, sizeof(f.pll));
            TEST_ASSERT_EQUAL_MEMORY(&f.ms, &l.ms, sizeof(f.ms));
            TEST_ASSERT_DOUBLE_WITHIN(1.35, (double)c.freq, f.actualHz);
        }
    }
}

static void test_iq_limits() {
    TEST_ASSERT_FALSE(planIQ(4720000, TuningMode::Fractional, XTAL4).ok);   // 127 x 4.72 MHz < 600 MHz
    TEST_ASSERT_FALSE(planIQ(100000001, TuningMode::LowNoise, XTAL4).ok);   // divider 8
    TEST_ASSERT_FALSE(planIQ(0, TuningMode::Integer, XTAL4).ok);
}

static void test_iq_integer_raster() {
    for (uint64_t xtal4 : XTALS) {
        lcgState = 7;
        for (int i = 0; i < 400; ++i) {
            const uint64_t f = i == 0 ? IQ_MIN_HZ : i == 1 ? IQ_MAX_HZ : randomHz(IQ_MIN_HZ, IQ_MAX_HZ);
            const SynthPlan p = planIQ(f, TuningMode::Integer, xtal4);
            snprintf(msg, sizeof(msg), "%llu Hz, xtal4 %llu: %u / %u", (unsigned long long)f, (unsigned long long)xtal4,
                     p.pll.a, p.ms.a);
            TEST_ASSERT_TRUE_MESSAGE(p.ok, msg);
            TEST_ASSERT_TRUE_MESSAGE(p.pll.b == 0 && p.pll.a >= 24 && p.pll.a <= 36, msg);
            // Any integer divider: MS_INT stays off for the phase offset.
            TEST_ASSERT_TRUE_MESSAGE(p.ms.b == 0 && p.ms.a >= 9 && p.ms.a <= 127 && !p.msInt, msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, xtal4 / 4.0 * p.pll.a / p.ms.a, p.actualHz, msg);
            TEST_ASSERT_TRUE_MESSAGE(fabs(p.actualHz - (double)f) <= rasterErr(f, xtal4, 9, 127, 1) + 1e-6, msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Spread spectrum: the modulated VCO stays within 600..900 MHz

static const Spread SPREADS[] = {Spread::Down05, Spread::Down15, Spread::Center05};

// vco * (1 - amp) .. vco * (1 + amp) (center) within 600..900 MHz; a fixed PLL ratio carries the
// crystal deviation on top.
static bool modulatedVcoInRange(const SynthPlan& p, uint64_t xtal4, Spread s) {
    const double vco = vcoHz(p, xtal4);
    const double amp = spreadAmplitude(s);
    const double tol = 20.0 + fabs((double)xtal4 - (double)XTAL4) / XTAL4 * 900e6;
    return vco * (1 - amp) >= 600e6 - tol && vco * (1 + (isCenterSpread(s) ? amp : 0.0)) <= 900e6 + tol;
}

static void test_spread_limits_the_vco() {
    const TuningMode modes[] = {TuningMode::Fractional, TuningMode::LowNoise, TuningMode::Integer};
    for (Spread s : SPREADS) {
        for (TuningMode m : modes) {
            for (uint64_t xtal4 : XTALS) {
                SynthPlan last {};
                lcgState = 9;
                for (int i = 0; i < 2000; ++i) {
                    const uint64_t f = i % 4 == 0 ? randomHz(8000, 160000000ULL) : randomHz(FREQ_MIN_HZ, FREQ_MAX_HZ);
                    const SynthPlan p = planStandalone(f, m, last.ok ? &last.pll : nullptr, last.ok ? &last.ms : nullptr,
                                                       xtal4, s);
                    snprintf(msg, sizeof(msg), "spread %u mode %u, %llu Hz, xtal4 %llu: %u + %u / %u, ms %u",
                             (unsigned)s, (unsigned)m, (unsigned long long)f, (unsigned long long)xtal4, p.pll.a,
                             p.pll.b, p.pll.c, p.ms.a);
                    if (!p.ok) {
                        // No even divider fits next to 150 MHz (6 or 4) once the range is narrower.
                        TEST_ASSERT_TRUE_MESSAGE(m != TuningMode::Integer && f > 149000000 && f < 153000000, msg);
                        continue;
                    }
                    TEST_ASSERT_TRUE_MESSAGE(modulatedVcoInRange(p, xtal4, s), msg);
                    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, outHz(p, xtal4), p.actualHz, msg);
                    if (f <= FREQ_MAX_HZ && m != TuningMode::Integer) {
                        const double maxErr = m == TuningMode::Fractional ? 0.5 : 1.5;
                        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(maxErr, (double)f, p.actualHz, msg);
                    }
                    last = p;
                }
                for (int i = 0; i < 500; ++i) {
                    const uint64_t f = randomHz(IQ_MIN_HZ, IQ_MAX_HZ);
                    const SynthPlan p = planIQ(f, m, xtal4, s);
                    if (!p.ok) continue; // range: test_spread_shrinks_the_iq_range
                    snprintf(msg, sizeof(msg), "spread %u mode %u, IQ %llu Hz, xtal4 %llu", (unsigned)s, (unsigned)m,
                             (unsigned long long)f, (unsigned long long)xtal4);
                    TEST_ASSERT_TRUE_MESSAGE(modulatedVcoInRange(p, xtal4, s), msg);
                    if (m != TuningMode::Integer) TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1.35, (double)f, p.actualHz, msg);
                }
            }
        }
    }
}

static void test_spread_fractional_pll_choice() {
    const SynthRatio pll900 {36, 0, 1};
    const SynthRatio pll875 {35, 0, 1};
    // Center spread: 900 MHz would reach 904.5 MHz. 10 MHz is exact with 887.5 and 875 MHz.
    SynthPlan p = planStandalone(10000000, TuningMode::Fractional, nullptr, nullptr, XTAL4, Spread::Center05);
    TEST_ASSERT_EQUAL_DOUBLE(35.5, value(p.pll));
    p = planStandalone(10000000, TuningMode::Fractional, &pll900, nullptr, XTAL4, Spread::Center05);
    TEST_ASSERT_EQUAL_DOUBLE(35.5, value(p.pll));
    p = planStandalone(10000000, TuningMode::Fractional, &pll875, nullptr, XTAL4, Spread::Center05);
    TEST_ASSERT_EQUAL_DOUBLE(35.0, value(p.pll));
    // Down spread only lowers the VCO: 900 / 887.5 MHz as without spread spectrum, never 875 MHz.
    p = planStandalone(10000000, TuningMode::Fractional, &pll900, nullptr, XTAL4, Spread::Down15);
    TEST_ASSERT_EQUAL_DOUBLE(36.0, value(p.pll));
    p = planStandalone(10000000, TuningMode::Fractional, &pll875, nullptr, XTAL4, Spread::Off);
    TEST_ASSERT_EQUAL_DOUBLE(36.0, value(p.pll));
}

static void test_spread_shrinks_the_low_noise_window() {
    const SynthRatio d90 {90, 0, 1};
    // Center spread: VCO <= 895.522 MHz. Divider 90 now covers 6.700 2..9.950 2 MHz.
    TEST_ASSERT_EQUAL_UINT32(88, planStandalone(10000000, TuningMode::LowNoise, nullptr, nullptr, XTAL4, Spread::Center05).ms.a);
    TEST_ASSERT_EQUAL_UINT32(90, planStandalone(9950000, TuningMode::LowNoise, nullptr, &d90, XTAL4, Spread::Center05).ms.a);
    TEST_ASSERT_EQUAL_UINT32(88, planStandalone(9951000, TuningMode::LowNoise, nullptr, &d90, XTAL4, Spread::Center05).ms.a);
    // Down spread -0.5 %: VCO >= 603.015 MHz.
    TEST_ASSERT_EQUAL_UINT32(90, planStandalone(6701000, TuningMode::LowNoise, nullptr, &d90, XTAL4, Spread::Down05).ms.a);
    TEST_ASSERT_EQUAL_UINT32(134, planStandalone(6700000, TuningMode::LowNoise, nullptr, &d90, XTAL4, Spread::Down05).ms.a);
    TEST_ASSERT_EQUAL_UINT32(90, planStandalone(6700000, TuningMode::LowNoise, nullptr, &d90, XTAL4, Spread::Off).ms.a);
    // Integer: 25..35 x 25 MHz with center spread.
    const SynthPlan p = planStandalone(4166667, TuningMode::Integer, nullptr, nullptr, XTAL4, Spread::Center05);
    TEST_ASSERT_TRUE(p.pll.a >= 25 && p.pll.a <= 35);
}

static void test_spread_shrinks_the_iq_range() {
    struct Case {
        Spread spread;
        uint64_t freq;
        bool ok;
    };
    const Case cases[] = {{Spread::Center05, 99502487, true},  {Spread::Center05, 99502488, false},
                          {Spread::Down05, 100000000, true},   {Spread::Down05, 4748151, true},
                          {Spread::Down05, 4748150, false},    {Spread::Down15, 4796355, true},
                          {Spread::Down15, 4796354, false},    {Spread::Center05, 4748151, true},
                          {Spread::Off, 4730000, true}};
    const TuningMode modes[] = {TuningMode::Fractional, TuningMode::LowNoise};
    for (const Case& c : cases) {
        for (TuningMode m : modes) {
            const SynthPlan p = planIQ(c.freq, m, XTAL4, c.spread);
            snprintf(msg, sizeof(msg), "spread %u, %llu Hz", (unsigned)c.spread, (unsigned long long)c.freq);
            TEST_ASSERT_EQUAL_MESSAGE(c.ok, p.ok, msg);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(9, planIQ(99502487, TuningMode::Fractional, XTAL4, Spread::Center05).ms.a);
    TEST_ASSERT_EQUAL_UINT32(89, planIQ(10000000, TuningMode::Fractional, XTAL4, Spread::Center05).ms.a);
    // Integer keeps a raster point, with the PLL inside the narrower range.
    const SynthPlan p = planIQ(IQ_MAX_HZ, TuningMode::Integer, XTAL4, Spread::Center05);
    TEST_ASSERT_TRUE(p.ok && p.pll.a >= 25 && p.pll.a <= 35);
}

static void test_iq_range_matches_plan_iq() {
    const Spread spreads[] = {Spread::Off, Spread::Down05, Spread::Down15, Spread::Center05};
    const TuningMode modes[] = {TuningMode::Fractional, TuningMode::LowNoise};
    for (Spread s : spreads) {
        const uint64_t lo = iqMinHz(s), hi = iqMaxHz(s);
        for (TuningMode m : modes) {
            TEST_ASSERT_TRUE(planIQ(lo, m, XTAL4, s).ok);
            TEST_ASSERT_TRUE(planIQ(hi, m, XTAL4, s).ok);
            if (lo > IQ_MIN_HZ) TEST_ASSERT_FALSE(planIQ(lo - 1, m, XTAL4, s).ok);
            if (hi < IQ_MAX_HZ) TEST_ASSERT_FALSE(planIQ(hi + 1, m, XTAL4, s).ok);
        }
    }
    TEST_ASSERT_EQUAL_UINT64(IQ_MIN_HZ, iqMinHz(Spread::Off));
    TEST_ASSERT_EQUAL_UINT64(IQ_MAX_HZ, iqMaxHz(Spread::Off));
    TEST_ASSERT_EQUAL_UINT64(4748151, iqMinHz(Spread::Down05));
    TEST_ASSERT_EQUAL_UINT64(99502487, iqMaxHz(Spread::Center05));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_fractional_uses_a_fixed_pll);
    RUN_TEST(test_fractional_keeps_current_pll_while_good_enough);
    RUN_TEST(test_fractional_above_112_5_mhz_uses_an_integer_divider);
    RUN_TEST(test_r_divider_below_1_mhz);
    RUN_TEST(test_low_noise_picks_the_largest_even_divider);
    RUN_TEST(test_low_noise_keeps_divider_within_vco_range);
    RUN_TEST(test_low_noise_accuracy_with_hysteresis);
    RUN_TEST(test_integer_example_7_1_mhz);
    RUN_TEST(test_integer_picks_the_nearest_raster_point);
    RUN_TEST(test_integer_vco_in_range_with_correction);
    RUN_TEST(test_integer_tie_prefers_an_even_pll_ratio);
    RUN_TEST(test_integer_limits);
    RUN_TEST(test_iq_fractional_and_low_noise_are_the_same);
    RUN_TEST(test_iq_limits);
    RUN_TEST(test_iq_integer_raster);
    RUN_TEST(test_spread_limits_the_vco);
    RUN_TEST(test_spread_fractional_pll_choice);
    RUN_TEST(test_spread_shrinks_the_low_noise_window);
    RUN_TEST(test_spread_shrinks_the_iq_range);
    RUN_TEST(test_iq_range_matches_plan_iq);
    return UNITY_END();
}
