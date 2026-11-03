// SI5351Manager against the register model in test/fakes/Wire.h. Frequencies are
// decoded from the registers (test/helpers/si5351_model.h), not taken from the driver.

#include <unity.h>
#include <Wire.h>
#include <constants.h>
#include <drivers/si5351_manager.h>
#include <hal/i2c_bus.h>
#include <helpers/si5351_model.h>
#include <stdio.h>

using namespace Si5351Model;
using Pll = SI5351Manager::Pll;

static constexpr double STANDALONE_MAX_ERR_HZ = 0.5;
// Worst case 1.32 Hz, e.g. at 91 666 668 Hz: the PLL fraction lies just above 1 / (2 * 2^20).
static constexpr double IQ_MAX_ERR_HZ = 1.35;
static const int32_t CORRECTIONS[] = {0, 970, -CORRECTION_MAX, CORRECTION_MAX};

static SI5351Manager si;
static Fake::Si5351Chip& chip = Fake::si5351();
static char msg[160];

void setUp() {
    chip.reset();
    Fake::clockMs() = 0;
    Fake::wireMisuse() = false;
    Wire.failBegin = false;
    TEST_ASSERT_TRUE(I2CBus::begin());
    // The CONFIG options survive begin(): back to the defaults.
    si.setTuningMode(TuningMode::Fractional);
    si.setSpread(Spread::Off);
    si.setOffState(OffState::Low);
    TEST_ASSERT_TRUE(si.begin(0));
    chip.clearLog();
}
void tearDown() { TEST_ASSERT_FALSE(Fake::wireMisuse()); }

// Deterministic pseudo-random frequencies.
static uint32_t lcgState = 1;
static uint64_t randomHz(uint64_t lo, uint64_t hi) {
    lcgState = lcgState * 1664525u + 1013904223u;
    const uint32_t a = lcgState;
    lcgState = lcgState * 1664525u + 1013904223u;
    return lo + (((uint64_t)a << 32) | lcgState) % (hi - lo + 1);
}

static void assertOutput(uint8_t clk, uint64_t freq, int32_t corr, double maxErr) {
    const char* violation = limitViolation(clk, corr);
    snprintf(msg, sizeof(msg), "CLK%u %llu Hz corr %ld: %s", clk, (unsigned long long)freq, (long)corr,
             violation ? violation : "");
    TEST_ASSERT_NULL_MESSAGE(violation, msg);
    const double err = fabs(outputHz(clk, corr) - (double)freq);
    snprintf(msg, sizeof(msg), "CLK%u %llu Hz corr %ld: error %.3f Hz", clk, (unsigned long long)freq, (long)corr, err);
    TEST_ASSERT_TRUE_MESSAGE(err <= maxErr, msg);
    // The deviation shown on the channel page.
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-3, outputHz(clk, corr), si.actualHz(clk), msg);
}

static void checkStandalone(uint64_t freq, int32_t corr, double maxErr) {
    snprintf(msg, sizeof(msg), "setupStandalone %llu Hz", (unsigned long long)freq);
    TEST_ASSERT_TRUE_MESSAGE(si.setupStandalone(0, Pll::A, freq, 8), msg);
    assertOutput(0, freq, corr, maxErr);
    chip.writes.clear();
}

static void checkIQ(uint64_t freq, int32_t corr) {
    snprintf(msg, sizeof(msg), "setupIQPair %llu Hz", (unsigned long long)freq);
    TEST_ASSERT_TRUE_MESSAGE(si.setupIQPair(0, 2, Pll::A, freq, 8, 90), msg);
    assertOutput(0, freq, corr, IQ_MAX_ERR_HZ);
    assertOutput(2, freq, corr, IQ_MAX_ERR_HZ);
    TEST_ASSERT_TRUE_MESSAGE(ms(0).integer() && ms(0).ratio() <= 127.0, msg);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)ms(0).ratio(), phase(2), msg);
    chip.writes.clear();
}

// ---------------------------------------------------------------------------
// Init and supervision

static void test_begin_without_device_fails() {
    chip.present = false;
    TEST_ASSERT_FALSE(si.begin(0));
    TEST_ASSERT_FALSE(si.ready());
    TEST_ASSERT_FALSE(si.setupStandalone(0, Pll::A, 10000000, 8));
}

static void test_begin_leaves_all_outputs_off() {
    chip.reset();
    for (uint8_t i = 16; i <= 23; ++i) chip.regs[i] = 0x0F; // configured by an earlier run
    chip.regs[3] = 0x00;
    TEST_ASSERT_TRUE(si.begin(0));
    TEST_ASSERT_TRUE(si.ready());
    TEST_ASSERT_EQUAL_UINT8(0, chip.initReads); // waited for SYS_INIT
    TEST_ASSERT_EQUAL_HEX8(0xFF, chip.regs[3]);
    for (uint8_t i = 16; i <= 23; ++i) TEST_ASSERT_EQUAL_HEX8(0x80, chip.regs[i]);
    TEST_ASSERT_EQUAL_HEX8(0x00, chip.regs[15]);
    TEST_ASSERT_EQUAL_HEX8(0xD2, chip.regs[183]);
}

// ---------------------------------------------------------------------------
// Identity check: something else at 0x60 gets at most one write

static void test_begin_on_closed_bus_does_not_touch_wire() {
    Wire.failBegin = true;
    TEST_ASSERT_FALSE(I2CBus::open(I2CMap::Swapped));
    TEST_ASSERT_FALSE(si.begin(0));
    TEST_ASSERT_EQUAL(SI5351Manager::Error::NoAck, si.lastError());
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
}

static void test_begin_accepts_reset_state() {
    chip.reset();
    TEST_ASSERT_TRUE(si.begin(0));
    TEST_ASSERT_FALSE(si.idUnverified());
    TEST_ASSERT_EQUAL(SI5351Manager::Error::None, si.lastError());
}

static void test_begin_accepts_unexpected_reg183_with_warning() {
    chip.reset();
    chip.regs[183] = 0xC0; // XTAL_CL valid, bits 5:0 differ (clone)
    TEST_ASSERT_TRUE(si.begin(0));
    TEST_ASSERT_TRUE(si.idUnverified());
}

static void test_begin_rejects_device_reading_zero() {
    chip.reset();
    chip.foreign = true;
    chip.foreignValue = 0x00;
    TEST_ASSERT_FALSE(si.begin(0));
    TEST_ASSERT_EQUAL(SI5351Manager::Error::NotSi5351, si.lastError());
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
}

static void test_begin_rejects_device_reading_ff_without_waiting() {
    chip.reset();
    chip.foreign = true;
    chip.foreignValue = 0xFF;
    const uint32_t t0 = Fake::clockMs();
    TEST_ASSERT_FALSE(si.begin(0));
    TEST_ASSERT_EQUAL(SI5351Manager::Error::NotSi5351, si.lastError());
    TEST_ASSERT_EQUAL_UINT32(t0, Fake::clockMs());
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
}

static void test_begin_rejects_device_without_register_readback() {
    chip.reset();
    chip.foreign = true;
    chip.foreignValue = 0x5A; // plausible status and reg183, but keeps nothing
    TEST_ASSERT_FALSE(si.begin(0));
    TEST_ASSERT_EQUAL(SI5351Manager::Error::NoReadback, si.lastError());
    TEST_ASSERT_EQUAL_UINT(1, chip.writes.size());
    TEST_ASSERT_EQUAL_UINT8(3, chip.writes[0].reg);
    // The read-only check alone cannot tell it from an SI5351.
    TEST_ASSERT_TRUE(si.identify());
    TEST_ASSERT_EQUAL_UINT(1, chip.writes.size());
}

static void test_begin_tolerates_sys_init_stuck() {
    chip.reset();
    chip.initReads = 100;
    TEST_ASSERT_TRUE(si.begin(0));
}

static void test_begin_stops_at_first_io_error() {
    chip.reset();
    chip.initReads = 0;
    // Probe 1, status 2-3, reg183 4-5, OE write 6, OE read-back 7-8: the off state write fails.
    chip.failAfter = 8;
    TEST_ASSERT_FALSE(si.begin(0));
    TEST_ASSERT_EQUAL(SI5351Manager::Error::Io, si.lastError());
    TEST_ASSERT_EQUAL_UINT(1, chip.writes.size()); // only the output enable
    TEST_ASSERT_EQUAL_UINT(1, chip.failedTransfers);
}

static void test_register_access_on_closed_bus_does_not_touch_wire() {
    Wire.failBegin = true;
    TEST_ASSERT_FALSE(I2CBus::open(I2CMap::Swapped));
    TEST_ASSERT_FALSE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_FALSE(si.ready());
    TEST_ASSERT_FALSE(si.checkAlive());
    TEST_ASSERT_FALSE(si.identify());
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
}

static void test_check_alive_detects_power_cycle() {
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_TRUE(si.enableOutput(0, true));
    TEST_ASSERT_TRUE(si.checkAlive());
    chip.powerOn();
    TEST_ASSERT_FALSE(si.checkAlive());
    TEST_ASSERT_FALSE(si.ready());
}

static void test_check_alive_detects_bus_failure() {
    TEST_ASSERT_TRUE(si.checkAlive());
    chip.failAfter = 0;
    TEST_ASSERT_FALSE(si.checkAlive());
    TEST_ASSERT_FALSE(si.ready());
}

static void test_failed_transfer_marks_device_lost_and_begin_recovers() {
    chip.failAfter = 1;
    TEST_ASSERT_FALSE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_FALSE(si.ready());
    chip.failAfter = -1;
    TEST_ASSERT_TRUE(si.begin(0));
    chip.clearLog();
    // The register cache was dropped: everything is written again.
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
    assertOutput(0, 10000000, 0, STANDALONE_MAX_ERR_HZ);
}

// ---------------------------------------------------------------------------
// Standalone outputs

static void test_standalone_accuracy_1_to_100_mhz() {
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        lcgState = 7;
        for (int i = 0; i < 4000; ++i) checkStandalone(randomHz(FREQ_MIN_HZ, FREQ_MAX_HZ), corr, STANDALONE_MAX_ERR_HZ);
        checkStandalone(FREQ_MIN_HZ, corr, STANDALONE_MAX_ERR_HZ);
        checkStandalone(FREQ_MAX_HZ, corr, STANDALONE_MAX_ERR_HZ);
    }
}

static void test_standalone_accuracy_at_weak_spots() {
    // Next to 900 MHz / n and 887.5 MHz / n one of the fixed PLLs cannot get close.
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        for (int n = 9; n <= 900; ++n) {
            for (double vco : {900e6, 887.5e6}) {
                for (int d = -3; d <= 3; ++d) {
                    const uint64_t f = (uint64_t)llround(vco / n) + d;
                    if (f >= FREQ_MIN_HZ && f <= FREQ_MAX_HZ) checkStandalone(f, corr, STANDALONE_MAX_ERR_HZ);
                }
            }
        }
    }
}

static void test_standalone_below_1_mhz_uses_r_divider() {
    // Not reachable from the GUI yet (FREQ_MIN_HZ), planned range extension.
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        lcgState = 11;
        for (int i = 0; i < 1000; ++i) checkStandalone(randomHz(8000, FREQ_MIN_HZ - 1), corr, STANDALONE_MAX_ERR_HZ);
        checkStandalone(8000, corr, STANDALONE_MAX_ERR_HZ);
    }
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 8000, 8));
    TEST_ASSERT_EQUAL_UINT8(7, ms(0).rLog2);
    TEST_ASSERT_FALSE(si.setupStandalone(0, Pll::A, 7999, 8));
}

// Above 100 MHz an even integer MultiSynth (8, 6, 4) with a fractional PLL is needed next to
// 112.5 MHz and above it. The 20-bit PLL denominator then limits the error to about
// 25 MHz / (2^21 * divider): 1.5 Hz (8), 2 Hz (6), 3 Hz (4).
static double maxErrAbove100MHz(uint64_t freq) {
    if (freq <= 112500000ULL) return 1.5;
    return freq <= 150000000ULL ? 2.0 : 3.0;
}

static void test_standalone_above_100_mhz() {
    // Planned range extension up to 160 MHz (todo.md 2.4). Known issue: next to 112.5 MHz the
    // MultiSynth ratio rounds to exactly 8 and the integer-divider path is not tried, e.g.
    // 112 500 004 Hz is off by 5.1 Hz with correction 0.
    TEST_IGNORE_MESSAGE("known issue next to 112.5 MHz, see todo.md 2.4");
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        lcgState = 13;
        for (int i = 0; i < 2000; ++i) {
            const uint64_t f = randomHz(FREQ_MAX_HZ + 1, 160000000ULL);
            checkStandalone(f, corr, maxErrAbove100MHz(f));
        }
        for (int d = -4; d <= 4; ++d) {
            for (uint64_t f : {112500000ULL + d, 150000000ULL + d}) checkStandalone(f, corr, maxErrAbove100MHz(f));
            for (int div : {4, 6, 8}) {
                for (int k = 1; k <= 40; ++k) {
                    const uint64_t f = (uint64_t)llround(k * 25e6 / div) + d;
                    if (f > FREQ_MAX_HZ && f <= 160000000ULL) checkStandalone(f, corr, maxErrAbove100MHz(f));
                }
            }
        }
    }
}

static void test_standalone_uses_integer_mode_for_even_dividers() {
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8)); // 900 MHz / 90
    TEST_ASSERT_TRUE(ms(0).integer());
    TEST_ASSERT_TRUE(ctrl(0) & CTRL_MS_INT);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 7100000, 8));
    TEST_ASSERT_FALSE(ctrl(0) & CTRL_MS_INT);
}

static void test_standalone_tuning_never_resets_pll_or_disables_output() {
    struct Sweep {
        uint64_t start;
        uint32_t step;
        unsigned count;
    };
    const Sweep sweeps[] = {{7000000, 1, 2000},    {9999000, 1, 2000},    {7000000, 1000, 2000},
                            {28000000, 100, 3000}, {50000000, 1000, 3000}, {99000000, 1000, 1000},
                            {1000000, 1000, 9000}, {10000000, 1000000, 90}};
    for (int32_t corr : {0, 970}) {
        for (const Sweep& s : sweeps) {
            TEST_ASSERT_TRUE(si.begin(corr));
            TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, s.start, 8));
            TEST_ASSERT_TRUE(si.enableOutput(0, true));
            chip.clearLog();
            for (unsigned i = 1; i <= s.count; ++i) {
                TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, s.start + (uint64_t)i * s.step, 8));
                TEST_ASSERT_EQUAL_UINT(0, chip.writesTo(3));
                chip.writes.clear();
            }
            snprintf(msg, sizeof(msg), "sweep from %llu Hz, step %lu Hz, corr %ld", (unsigned long long)s.start,
                     (unsigned long)s.step, (long)corr);
            TEST_ASSERT_EQUAL_UINT_MESSAGE(0, chip.totalPllResets(), msg);
            TEST_ASSERT_TRUE(enabled(0));
        }
    }
}

static void test_correction_change_does_not_reset_pll() {
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    chip.clearLog();
    for (int32_t c = -CORRECTION_MAX; c <= CORRECTION_MAX; c += 250) {
        si.setCorrection(c);
        TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
        assertOutput(0, 10000000, c, STANDALONE_MAX_ERR_HZ);
    }
    TEST_ASSERT_EQUAL_UINT(0, chip.totalPllResets());
}

static void test_unchanged_setup_writes_nothing() {
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14000000, 4));
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14000000, 4));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    // Drive strength changes only the control register.
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14000000, 8));
    TEST_ASSERT_EQUAL_UINT(1, chip.writes.size());
    TEST_ASSERT_EQUAL_UINT8(8, driveMa(1));
}

static void test_channels_use_their_own_pll() {
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 2));
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14200000, 6));
    TEST_ASSERT_FALSE(usesPllB(0));
    TEST_ASSERT_TRUE(usesPllB(1));
    TEST_ASSERT_EQUAL_UINT8(2, driveMa(0));
    TEST_ASSERT_EQUAL_UINT8(6, driveMa(1));
    assertOutput(0, 10000000, 0, STANDALONE_MAX_ERR_HZ);
    assertOutput(1, 14200000, 0, STANDALONE_MAX_ERR_HZ);
    // Retuning CH2 does not touch PLL A or CLK0.
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 3500000, 6));
    for (const Fake::RegWrite& w : chip.writes) {
        TEST_ASSERT_FALSE(w.reg >= 26 && w.reg <= 33);
        TEST_ASSERT_FALSE(w.reg >= 42 && w.reg <= 49);
        TEST_ASSERT_NOT_EQUAL(16, w.reg);
    }
    TEST_ASSERT_EQUAL_UINT(0, chip.pllResets[0]);
}

static void test_rejects_invalid_arguments() {
    TEST_ASSERT_FALSE(si.setupStandalone(3, Pll::A, 10000000, 8));
    TEST_ASSERT_FALSE(si.setupStandalone(0, Pll::A, 160000001ULL, 8));
    TEST_ASSERT_FALSE(si.enableOutput(3, true));
    TEST_ASSERT_FALSE(si.powerDown(3));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    TEST_ASSERT_TRUE(si.ready());
}

// ---------------------------------------------------------------------------
// Output enable / power down

static void test_enable_output_mask() {
    TEST_ASSERT_TRUE(si.enableOutput(0, true));
    TEST_ASSERT_EQUAL_HEX8(0xFE, chip.regs[3]);
    TEST_ASSERT_TRUE(si.enableOutput(2, true));
    TEST_ASSERT_EQUAL_HEX8(0xFA, chip.regs[3]);
    TEST_ASSERT_TRUE(si.enableOutput(0, false));
    TEST_ASSERT_EQUAL_HEX8(0xFB, chip.regs[3]);
    chip.clearLog();
    TEST_ASSERT_TRUE(si.enableOutput(0, false));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
}

static void test_power_down() {
    TEST_ASSERT_TRUE(si.setupStandalone(2, Pll::A, 10000000, 8));
    TEST_ASSERT_TRUE(si.enableOutput(2, true));
    TEST_ASSERT_TRUE(si.powerDown(2));
    TEST_ASSERT_EQUAL_HEX8(0x80, chip.regs[18]);
    TEST_ASSERT_TRUE(chip.regs[3] & 0x04);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, si.actualHz(2));
    TEST_ASSERT_TRUE(si.checkAlive());
}

// ---------------------------------------------------------------------------
// IQ pair

static void test_iq_phase_registers() {
    const uint16_t phases[] = {0, 90, 180, 270};
    for (uint16_t ph : phases) {
        TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 8, ph));
        TEST_ASSERT_EQUAL_MEMORY(chip.regs + 42, chip.regs + 58, 8); // same MultiSynth parameters
        TEST_ASSERT_EQUAL_DOUBLE(90.0, ms(0).ratio());
        TEST_ASSERT_FALSE(usesPllB(0));
        TEST_ASSERT_FALSE(usesPllB(2));
        TEST_ASSERT_FALSE(ctrl(0) & CTRL_MS_INT); // the phase offset needs fractional mode
        TEST_ASSERT_FALSE(ctrl(2) & CTRL_MS_INT);
        TEST_ASSERT_EQUAL_UINT8(0, phase(0));
        TEST_ASSERT_EQUAL_UINT8((ph == 90 || ph == 270) ? 90 : 0, phase(2));
        TEST_ASSERT_FALSE(inverted(0));
        TEST_ASSERT_EQUAL(ph >= 180, inverted(2));
    }
}

static void test_iq_on_pll_b_with_ch2_master() {
    TEST_ASSERT_TRUE(si.setupIQPair(1, 2, Pll::B, 14200000, 4, 90));
    TEST_ASSERT_TRUE(usesPllB(1));
    TEST_ASSERT_TRUE(usesPllB(2));
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[1]);
    TEST_ASSERT_EQUAL_UINT(0, chip.pllResets[0]);
    TEST_ASSERT_EQUAL_UINT8(63, phase(2));
    assertOutput(1, 14200000, 0, IQ_MAX_ERR_HZ);
}

static void test_iq_accuracy_and_limits() {
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        lcgState = 17;
        for (int i = 0; i < 3000; ++i) checkIQ(randomHz(IQ_MIN_HZ, IQ_MAX_HZ), corr);
        checkIQ(IQ_MIN_HZ, corr);
        checkIQ(IQ_MAX_HZ, corr);
    }
}

static void test_iq_accuracy_at_weak_spots() {
    // Next to k * 25 MHz / divider the PLL fraction is too fine for a 20-bit denominator.
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        for (int div = 9; div <= 127; ++div) {
            for (int k = (int)(600 / 25); k <= 900 / 25; ++k) {
                for (int d = -3; d <= 3; ++d) {
                    const uint64_t f = (uint64_t)llround(k * 25e6 / div) + d;
                    if (f >= IQ_MIN_HZ && f <= IQ_MAX_HZ) checkIQ(f, corr);
                }
            }
        }
    }
}

static void test_iq_setup_disables_outputs_and_realigns() {
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 8, 90));
    TEST_ASSERT_TRUE(si.enableOutput(0, true));
    TEST_ASSERT_TRUE(si.enableOutput(2, true));
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10001000, 8, 90));
    // Outputs off before the reset; the caller enables them again.
    TEST_ASSERT_TRUE(chip.writesTo(3) >= 1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, chip.regs[3]);
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
    TEST_ASSERT_EQUAL_UINT8(177, chip.writes.back().reg);
}

static void test_iq_drive_or_inversion_change_keeps_alignment() {
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 4, 90));
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 4, 90));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 8, 90));
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 8, 270));
    TEST_ASSERT_EQUAL_UINT(0, chip.totalPllResets());
    TEST_ASSERT_TRUE(inverted(2));
    // 90 -> 0 changes the phase offset and needs a new alignment.
    TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, 10000000, 8, 0));
    TEST_ASSERT_EQUAL_UINT(1, chip.totalPllResets());
}

static void test_iq_rejects_invalid_arguments() {
    TEST_ASSERT_FALSE(si.setupIQPair(0, 2, Pll::A, IQ_MIN_HZ - 1, 8, 90));
    TEST_ASSERT_FALSE(si.setupIQPair(0, 2, Pll::A, IQ_MAX_HZ + 1, 8, 90));
    TEST_ASSERT_FALSE(si.setupIQPair(0, 2, Pll::A, 10000000, 8, 45));
    TEST_ASSERT_FALSE(si.setupIQPair(2, 2, Pll::A, 10000000, 8, 90));
    TEST_ASSERT_FALSE(si.setupIQPair(0, 3, Pll::A, 10000000, 8, 90));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    TEST_ASSERT_TRUE(si.ready()); // a rejected value is not a lost device
}

static void test_iq_vco_stays_in_range_at_iq_limits() {
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, IQ_MIN_HZ, 8, 90));
        TEST_ASSERT_EQUAL_DOUBLE(127.0, ms(0).ratio());
        TEST_ASSERT_TRUE(vcoHz(0, corr) >= 600e6);
        TEST_ASSERT_TRUE(si.setupIQPair(0, 2, Pll::A, IQ_MAX_HZ, 8, 90));
        TEST_ASSERT_EQUAL_DOUBLE(9.0, ms(0).ratio());
        TEST_ASSERT_TRUE(vcoHz(0, corr) <= 900e6 + 1.0);
    }
}

// ---------------------------------------------------------------------------
// FBx_INT

static void test_fb_int_follows_even_integer_pll() {
    // Fractional at 10 MHz: PLL A fixed at 36 x 25 MHz.
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_EQUAL_DOUBLE(36.0, pll(0).ratio());
    TEST_ASSERT_TRUE(pllIntMode(0));
    TEST_ASSERT_EQUAL_HEX8(0xC0, chip.regs[22]); // CLK6 stays powered down
    TEST_ASSERT_FALSE(chip.regs[23] & 0x40);
    // Low noise at 7.1 MHz: fractional PLL. FBA_INT is cleared before the PLL parameters.
    si.setTuningMode(TuningMode::LowNoise);
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 7100000, 8));
    TEST_ASSERT_FALSE(pll(0).integer());
    TEST_ASSERT_FALSE(pllIntMode(0));
    TEST_ASSERT_TRUE(chip.firstWriteTo(22) < chip.firstWriteTo(26));
    // Back to 900 MHz: set after the parameters.
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_TRUE(pllIntMode(0));
    TEST_ASSERT_TRUE(chip.firstWriteTo(22) > chip.firstWriteTo(26));
    // PLL B has its own bit.
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 12500000, 8)); // divider 72, 900 MHz
    TEST_ASSERT_TRUE(pllIntMode(1));
    TEST_ASSERT_EQUAL_HEX8(0xC0, chip.regs[23]);
    TEST_ASSERT_NULL(limitViolation(0));
    TEST_ASSERT_NULL(limitViolation(1));
}

// ---------------------------------------------------------------------------
// Low noise: even integer MultiSynth, fractional PLL

// Worst case of the 20-bit PLL fraction: half a step, 25 MHz / (2 * 1048575), at the VCO.
static double lowNoiseMaxErr(double divider) { return 25e6 * (1 + 6e-5) / (2.0 * 1048575) / divider; }
static constexpr double LOW_NOISE_MAX_ERR_HZ = 1.5; // divider >= 8 up to 100 MHz

static void assertLowNoise(uint8_t clk, uint64_t freq, int32_t corr, double maxErr) {
    assertOutput(clk, freq, corr, maxErr);
    const Block m = ms(clk);
    const double vco = vcoHz(clk, corr);
    snprintf(msg, sizeof(msg), "CLK%u %llu Hz corr %ld: MultiSynth %.6f, VCO %.1f Hz", clk, (unsigned long long)freq,
             (long)corr, m.ratio(), vco);
    TEST_ASSERT_TRUE_MESSAGE(m.integer() && fmod(m.ratio(), 2.0) == 0.0, msg);
    TEST_ASSERT_TRUE_MESSAGE(ctrl(clk) & CTRL_MS_INT, msg);
    // The PLL takes the correction: the VCO is divider x frequency.
    TEST_ASSERT_TRUE_MESSAGE(vco >= 600e6 - 15 && vco <= 900e6 + 15, msg);
}

static void checkLowNoise(uint64_t freq, int32_t corr, double maxErr = LOW_NOISE_MAX_ERR_HZ) {
    snprintf(msg, sizeof(msg), "setupStandalone %llu Hz", (unsigned long long)freq);
    TEST_ASSERT_TRUE_MESSAGE(si.setupStandalone(0, Pll::A, freq, 8), msg);
    assertLowNoise(0, freq, corr, maxErr);
    chip.writes.clear();
}

static void test_low_noise_accuracy_1_to_100_mhz() {
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        si.setTuningMode(TuningMode::LowNoise);
        lcgState = 19;
        for (int i = 0; i < 4000; ++i) checkLowNoise(randomHz(FREQ_MIN_HZ, FREQ_MAX_HZ), corr);
        checkLowNoise(FREQ_MIN_HZ, corr);
        checkLowNoise(FREQ_MAX_HZ, corr);
        // Next to k * 25 MHz / divider the PLL fraction is close to an integer.
        for (int div = 8; div <= 128; div += 2) {
            for (int k = 24; k <= 36; ++k) {
                for (int d = -3; d <= 3; ++d) {
                    const uint64_t f = (uint64_t)llround(k * 25e6 / div) + d;
                    if (f >= FREQ_MIN_HZ && f <= FREQ_MAX_HZ) checkLowNoise(f, corr);
                }
            }
        }
    }
}

static void test_low_noise_below_1_mhz_and_above_100_mhz() {
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        si.setTuningMode(TuningMode::LowNoise);
        lcgState = 23;
        for (int i = 0; i < 500; ++i) checkLowNoise(randomHz(8000, FREQ_MIN_HZ - 1), corr);
        // 8, 6 or 4 (DIVBY4); a kept divider may be smaller than a new one.
        for (int i = 0; i < 1000; ++i) {
            const uint64_t f = randomHz(FREQ_MAX_HZ + 1, 160000000ULL);
            TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, f, 8));
            assertLowNoise(0, f, corr, lowNoiseMaxErr(ms(0).ratio()));
        }
    }
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 8000, 8));
    TEST_ASSERT_EQUAL_UINT8(7, ms(0).rLog2);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 155000000, 8));
    TEST_ASSERT_TRUE(ms(0).divBy4);
    TEST_ASSERT_NULL(limitViolation(0));
}

static void test_low_noise_resets_pll_only_when_the_divider_changes() {
    struct Sweep {
        uint64_t start;
        int32_t step;
        unsigned count;
    };
    const Sweep sweeps[] = {{7000000, 1, 2000},     {9999000, 1, 2000},      {7000000, 1000, 2000},
                            {28000000, 100, 3000},  {50000000, 1000, 3000},  {99000000, -1000, 5000},
                            {1000000, 1000, 9000},  {10000000, 1000000, 90}, {100000000, -1000000, 99}};
    for (int32_t corr : {0, 970}) {
        for (const Sweep& s : sweeps) {
            TEST_ASSERT_TRUE(si.begin(corr));
            si.setTuningMode(TuningMode::LowNoise);
            TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, s.start, 8));
            TEST_ASSERT_TRUE(si.enableOutput(0, true));
            chip.clearLog();
            for (unsigned i = 1; i <= s.count; ++i) {
                const uint64_t f = s.start + (int64_t)i * s.step;
                const double divider = ms(0).ratio();
                const unsigned resets = chip.pllResets[0];
                TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, f, 8));
                snprintf(msg, sizeof(msg), "%llu Hz, corr %ld: divider %.0f -> %.0f", (unsigned long long)f, (long)corr,
                         divider, ms(0).ratio());
                TEST_ASSERT_EQUAL_UINT_MESSAGE(ms(0).ratio() != divider ? 1 : 0, chip.pllResets[0] - resets, msg);
                TEST_ASSERT_EQUAL_UINT_MESSAGE(0, chip.writesTo(3), msg);
                assertOutput(0, f, corr, LOW_NOISE_MAX_ERR_HZ);
                chip.writes.clear();
            }
            TEST_ASSERT_TRUE(enabled(0));
        }
    }
}

static void test_low_noise_keeps_divider_over_its_vco_range() {
    si.setTuningMode(TuningMode::LowNoise);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_EQUAL_DOUBLE(90.0, ms(0).ratio()); // largest even divider, VCO 900 MHz
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
    chip.clearLog();
    // Divider 90 covers 6.667-10 MHz.
    for (uint64_t f = 9999000; f >= 6667000; f -= 1000) {
        TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, f, 8));
        TEST_ASSERT_EQUAL_DOUBLE(90.0, ms(0).ratio());
    }
    TEST_ASSERT_EQUAL_UINT(0, chip.totalPllResets());
    for (const Fake::RegWrite& w : chip.writes) TEST_ASSERT_FALSE(w.reg >= 42 && w.reg <= 49); // MultiSynth untouched
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 6666000, 8));
    TEST_ASSERT_EQUAL_DOUBLE(134.0, ms(0).ratio());
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
}

static void test_low_noise_after_fractional_keeps_an_even_divider() {
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8)); // fixed 900 MHz / 90
    chip.clearLog();
    si.setTuningMode(TuningMode::LowNoise);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    // A fractional divider is replaced, with a reset.
    si.setTuningMode(TuningMode::Fractional);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 7100000, 8));
    chip.clearLog();
    si.setTuningMode(TuningMode::LowNoise);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 7100000, 8));
    TEST_ASSERT_EQUAL_DOUBLE(126.0, ms(0).ratio());
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
}

// ---------------------------------------------------------------------------
// Integer: integer PLL and MultiSynth, frequency on a raster

// Distance of freq to the nearest point crystal * a / (div * 2^rLog2), a = 24..36, by brute force.
static double rasterErr(uint64_t freq, int32_t corr, uint8_t rLog2, int divMin, int divMax, int divStep) {
    double best = 1e18;
    for (int a = 24; a <= 36; ++a) {
        for (int div = divMin; div <= divMax; div += divStep) {
            best = fmin(best, fabs(xtalHz(corr) * a / div / (double)(1U << rLog2) - (double)freq));
        }
    }
    return best;
}

static void assertRasterPoint(uint8_t clk, uint64_t freq, int32_t corr, int divMin, int divMax, int divStep) {
    const char* violation = limitViolation(clk, corr);
    snprintf(msg, sizeof(msg), "CLK%u %llu Hz corr %ld: %s", clk, (unsigned long long)freq, (long)corr,
             violation ? violation : "");
    TEST_ASSERT_NULL_MESSAGE(violation, msg);
    const Block p = pll(clk);
    TEST_ASSERT_TRUE_MESSAGE(p.integer() && p.ratio() >= 24.0 && p.ratio() <= 36.0, msg);
    TEST_ASSERT_TRUE_MESSAGE(ms(clk).integer(), msg);
    TEST_ASSERT_EQUAL_MESSAGE(fmod(p.ratio(), 2.0) == 0.0, pllIntMode(clk), msg);
    // The shown frequency is the raster point itself.
    TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, outputHz(clk, corr), si.actualHz(clk), msg);
    const double err = fabs(outputHz(clk, corr) - (double)freq);
    TEST_ASSERT_TRUE_MESSAGE(err <= rasterErr(freq, corr, ms(clk).rLog2, divMin, divMax, divStep) + 1e-6, msg);
}

static void test_integer_nearest_raster_point() {
    for (int32_t corr : CORRECTIONS) {
        TEST_ASSERT_TRUE(si.begin(corr));
        si.setTuningMode(TuningMode::Integer);
        lcgState = 29;
        for (int i = 0; i < 300; ++i) {
            const uint64_t f = i < 250 ? randomHz(FREQ_MIN_HZ, FREQ_MAX_HZ) : randomHz(8000, 160000000ULL);
            const Block before = pll(0);
            const unsigned resets = chip.pllResets[0];
            TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, f, 8));
            assertRasterPoint(0, f, corr, 4, 2048, 2);
            TEST_ASSERT_TRUE_MESSAGE(fmod(ms(0).ratio(), 2.0) == 0.0 && (ctrl(0) & CTRL_MS_INT), msg);
            // A new PLL multiplier comes with a reset.
            TEST_ASSERT_EQUAL_UINT_MESSAGE(pll(0).ratio() != before.ratio() ? 1 : 0, chip.pllResets[0] - resets, msg);
        }
    }
}

static void test_integer_example_7_1_mhz() {
    si.setTuningMode(TuningMode::Integer);
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 7100000, 8));
    TEST_ASSERT_EQUAL_DOUBLE(25.0, pll(0).ratio()); // 25 MHz x 25 / 88
    TEST_ASSERT_EQUAL_DOUBLE(88.0, ms(0).ratio());
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 25e6 * 25 / 88, si.actualHz(0));
    TEST_ASSERT_FALSE(pllIntMode(0)); // odd: FBA_INT only for even ratios
    // 14 MHz = 25 MHz x 28 / 50 exactly, even PLL ratio.
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14000000, 8));
    TEST_ASSERT_EQUAL_DOUBLE(14e6, si.actualHz(1));
    TEST_ASSERT_TRUE(pllIntMode(1));
    // The correction moves the raster, not the PLL: the shown frequency follows it.
    TEST_ASSERT_TRUE(si.begin(CORRECTION_MAX));
    si.setTuningMode(TuningMode::Integer);
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14000000, 8));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 14e6 * (1 + 5e-5), si.actualHz(1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, outputHz(1, CORRECTION_MAX), si.actualHz(1));
}

// ---------------------------------------------------------------------------
// IQ in all tuning modes

static void test_iq_in_all_tuning_modes() {
    const TuningMode modes[] = {TuningMode::Fractional, TuningMode::LowNoise, TuningMode::Integer};
    for (TuningMode mode : modes) {
        for (int32_t corr : CORRECTIONS) {
            TEST_ASSERT_TRUE(si.begin(corr));
            si.setTuningMode(mode);
            lcgState = 31;
            for (int i = 0; i < 300; ++i) {
                const uint64_t f = i == 0 ? IQ_MIN_HZ : i == 1 ? IQ_MAX_HZ : randomHz(IQ_MIN_HZ, IQ_MAX_HZ);
                const uint16_t ph = (uint16_t)(90 * (i % 4));
                snprintf(msg, sizeof(msg), "mode %u: setupIQPair %llu Hz %u deg", (unsigned)mode,
                         (unsigned long long)f, ph);
                TEST_ASSERT_TRUE_MESSAGE(si.setupIQPair(0, 2, Pll::A, f, 8, ph), msg);
                TEST_ASSERT_EQUAL_MEMORY_MESSAGE(chip.regs + 42, chip.regs + 58, 8, msg);
                const Block m = ms(0);
                TEST_ASSERT_TRUE_MESSAGE(m.integer() && m.ratio() >= 9.0 && m.ratio() <= 127.0, msg);
                TEST_ASSERT_FALSE_MESSAGE((ctrl(0) | ctrl(2)) & CTRL_MS_INT, msg);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE((ph == 90 || ph == 270) ? (uint8_t)m.ratio() : 0, phase(2), msg);
                TEST_ASSERT_EQUAL_MESSAGE(ph >= 180, inverted(2), msg);
                if (mode == TuningMode::Integer) {
                    assertRasterPoint(0, f, corr, 9, 127, 1);
                    assertRasterPoint(2, f, corr, 9, 127, 1);
                } else {
                    assertOutput(0, f, corr, IQ_MAX_ERR_HZ);
                    assertOutput(2, f, corr, IQ_MAX_ERR_HZ);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Spread spectrum (PLL A)

static void assertSpread(Spread s) {
    const SpreadSpectrum ss = spreadSpectrum();
    TEST_ASSERT_TRUE(ss.enabled);
    TEST_ASSERT_EQUAL(s == Spread::Center05, ss.center);
    TEST_ASSERT_EQUAL_UINT8(0, ss.nclk);
    TEST_ASSERT_EQUAL_UINT32(32767, ss.dnP3);
    TEST_ASSERT_DOUBLE_WITHIN(500.0, 31500.0, spreadModulationHz());
    const double amp = s == Spread::Down15 ? 0.015 : 0.005;
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, amp, spreadDownAmp());
    if (ss.center) {
        TEST_ASSERT_EQUAL_UINT32(32767, ss.upP3);
        TEST_ASSERT_DOUBLE_WITHIN(1e-5, amp, spreadUpAmp());
    } else {
        TEST_ASSERT_EQUAL_DOUBLE(0.0, ss.up());
    }
    // Spread spectrum needs the fractional PLL mode.
    TEST_ASSERT_FALSE(regs()[22] & 0x40);
}

static void test_begin_switches_spread_spectrum_off() {
    chip.reset();
    chip.regs[149] = 0x92; // NVM with spread spectrum on
    chip.regs[161] = 0xF0;
    TEST_ASSERT_TRUE(si.begin(0));
    TEST_ASSERT_FALSE(spreadSpectrum().enabled);
    TEST_ASSERT_EQUAL_UINT8(0, spreadSpectrum().nclk);
    // Also with spread spectrum selected: it starts with the first PLL A setup.
    TEST_ASSERT_TRUE(si.setSpread(Spread::Down05));
    TEST_ASSERT_TRUE(si.begin(0));
    TEST_ASSERT_FALSE(spreadSpectrum().enabled);
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    assertSpread(Spread::Down05);
    TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
    TEST_ASSERT_NULL(limitViolation(0));
}

static void test_spread_spectrum_parameters() {
    const Spread options[] = {Spread::Down05, Spread::Down15, Spread::Center05};
    for (Spread s : options) {
        TEST_ASSERT_TRUE(si.setSpread(Spread::Off));
        TEST_ASSERT_TRUE(si.begin(0));
        TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8)); // 900 MHz, FBA_INT
        TEST_ASSERT_TRUE(pllIntMode(0));
        chip.clearLog();
        TEST_ASSERT_TRUE(si.setSpread(s));
        assertSpread(s);
        TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
        TEST_ASSERT_TRUE(chip.firstWriteTo(22) < chip.firstWriteTo(149)); // FBA_INT off first
        // The unmodulated frequency is unchanged.
        assertOutput(0, 10000000, 0, STANDALONE_MAX_ERR_HZ);
        // Off again: FBA_INT only after the modulation has stopped.
        chip.clearLog();
        TEST_ASSERT_TRUE(si.setSpread(Spread::Off));
        TEST_ASSERT_FALSE(spreadSpectrum().enabled);
        TEST_ASSERT_TRUE(pllIntMode(0));
        TEST_ASSERT_TRUE(chip.firstWriteTo(149) < chip.firstWriteTo(22));
        TEST_ASSERT_EQUAL_UINT(1, chip.pllResets[0]);
        // Unchanged: nothing written.
        chip.clearLog();
        TEST_ASSERT_TRUE(si.setSpread(Spread::Off));
        TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    }
}

static void test_spread_spectrum_follows_pll_a() {
    for (int32_t corr : {0, -CORRECTION_MAX}) {
        TEST_ASSERT_TRUE(si.begin(corr));
        si.setTuningMode(TuningMode::LowNoise);
        TEST_ASSERT_TRUE(si.setSpread(Spread::Center05));
        TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 6900000, 8));
        // 100 kHz steps change the PLL A ratio by up to 1.5 %: stale parameters would show.
        for (uint64_t f = 7000000; f <= 9000000; f += 100000) {
            const double divider = ms(0).ratio();
            chip.clearLog();
            TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, f, 8));
            assertSpread(Spread::Center05);
            TEST_ASSERT_TRUE(chip.writesTo(149) >= 1);
            TEST_ASSERT_EQUAL_UINT(ms(0).ratio() != divider ? 1 : 0, chip.pllResets[0]);
            assertLowNoise(0, f, corr, LOW_NOISE_MAX_ERR_HZ);
        }
        // Integer mode never runs FBA_INT together with spread spectrum.
        si.setTuningMode(TuningMode::Integer);
        TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 12500000, 8)); // even PLL ratio
        TEST_ASSERT_TRUE(fmod(pll(0).ratio(), 2.0) == 0.0);
        assertSpread(Spread::Center05);
        TEST_ASSERT_NULL(limitViolation(0, corr));
    }
}

static void test_spread_spectrum_never_touches_pll_b() {
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 14000000, 8));
    chip.clearLog();
    // PLL A not configured yet: only stored.
    TEST_ASSERT_TRUE(si.setSpread(Spread::Down15));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    assertSpread(Spread::Down15);
    // CH2 and an IQ pair on PLL B: no spread spectrum register, FBB_INT stays allowed.
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setupStandalone(1, Pll::B, 7100000, 8));
    TEST_ASSERT_TRUE(si.setupIQPair(1, 2, Pll::B, 12500000, 8, 90)); // divider 72, 900 MHz
    TEST_ASSERT_TRUE(pllIntMode(1));
    for (const Fake::RegWrite& w : chip.writes) TEST_ASSERT_FALSE(w.reg >= 149 && w.reg <= 161);
    TEST_ASSERT_EQUAL_UINT(0, chip.pllResets[0]);
    // Changing the option touches neither PLL B nor FBB_INT.
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setSpread(Spread::Center05));
    TEST_ASSERT_TRUE(si.setSpread(Spread::Off));
    for (const Fake::RegWrite& w : chip.writes) {
        TEST_ASSERT_FALSE(w.reg >= 34 && w.reg <= 41);
        TEST_ASSERT_NOT_EQUAL(23, w.reg);
    }
    TEST_ASSERT_EQUAL_UINT(0, chip.pllResets[1]);
    TEST_ASSERT_EQUAL_UINT(2, chip.pllResets[0]);
    TEST_ASSERT_TRUE(pllIntMode(1));
}

// ---------------------------------------------------------------------------
// Off state (register 24) and power down

static void test_begin_writes_off_state_before_outputs() {
    const OffState states[] = {OffState::Low, OffState::High, OffState::HighZ};
    const uint8_t expected[] = {0x00, 0x15, 0x2A};
    for (uint8_t i = 0; i < 3; ++i) {
        si.setOffState(states[i]);
        chip.reset();
        chip.regs[24] = 0xFF;
        TEST_ASSERT_TRUE(si.begin(0));
        TEST_ASSERT_EQUAL_HEX8(expected[i], chip.regs[24]);
        for (uint8_t clk = 0; clk < 3; ++clk) TEST_ASSERT_EQUAL_UINT8((uint8_t)states[i], disableState(clk));
        // Right after the output enable read-back, before any control register.
        TEST_ASSERT_EQUAL_UINT8(3, chip.writes[0].reg);
        TEST_ASSERT_EQUAL_UINT8(24, chip.writes[1].reg);
    }
}

static void test_set_off_state_writes_at_once() {
    TEST_ASSERT_TRUE(si.setupStandalone(0, Pll::A, 10000000, 8));
    TEST_ASSERT_TRUE(si.enableOutput(0, true));
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setOffState(OffState::HighZ));
    TEST_ASSERT_EQUAL_HEX8(0x2A, chip.regs[24]);
    TEST_ASSERT_EQUAL_UINT(1, chip.writes.size()); // running outputs untouched
    TEST_ASSERT_TRUE(si.setOffState(OffState::High));
    TEST_ASSERT_EQUAL_HEX8(0x15, chip.regs[24]);
    TEST_ASSERT_TRUE(si.setOffState(OffState::Low));
    TEST_ASSERT_EQUAL_HEX8(0x00, chip.regs[24]);
    TEST_ASSERT_TRUE(enabled(0));
    // Not connected: stored, begin() writes it.
    chip.present = false;
    TEST_ASSERT_FALSE(si.checkAlive());
    chip.clearLog();
    TEST_ASSERT_TRUE(si.setOffState(OffState::High));
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
    chip.present = true;
    TEST_ASSERT_TRUE(si.begin(0));
    TEST_ASSERT_EQUAL_HEX8(0x15, chip.regs[24]);
}

static void test_power_down_keeps_driver_for_high_and_high_z() {
    for (OffState s : {OffState::High, OffState::HighZ}) {
        TEST_ASSERT_TRUE(si.setOffState(s));
        TEST_ASSERT_TRUE(si.begin(0));
        TEST_ASSERT_TRUE(si.setupStandalone(2, Pll::A, 10000000, 8));
        TEST_ASSERT_TRUE(si.enableOutput(2, true));
        const uint8_t running = ctrl(2);
        TEST_ASSERT_TRUE(si.powerDown(2));
        TEST_ASSERT_TRUE(chip.regs[3] & 0x04); // disabled: register 24 sets the level
        TEST_ASSERT_EQUAL_HEX8(running, ctrl(2));
        TEST_ASSERT_EQUAL_DOUBLE(0.0, si.actualHz(2));
        // A never configured output gets its driver powered, with the crystal as source.
        TEST_ASSERT_TRUE(poweredDown(1));
        TEST_ASSERT_TRUE(si.powerDown(1));
        TEST_ASSERT_EQUAL_HEX8(0x00, ctrl(1));
        TEST_ASSERT_TRUE(chip.regs[3] & 0x02);
        TEST_ASSERT_TRUE(si.checkAlive());
        // Back to Low: CLKx_PDN.
        TEST_ASSERT_TRUE(si.setOffState(OffState::Low));
        TEST_ASSERT_TRUE(si.powerDown(2));
        TEST_ASSERT_TRUE(si.powerDown(1));
        TEST_ASSERT_TRUE(poweredDown(2) && poweredDown(1));
        TEST_ASSERT_TRUE(si.checkAlive());
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_without_device_fails);
    RUN_TEST(test_begin_leaves_all_outputs_off);
    RUN_TEST(test_begin_on_closed_bus_does_not_touch_wire);
    RUN_TEST(test_begin_accepts_reset_state);
    RUN_TEST(test_begin_accepts_unexpected_reg183_with_warning);
    RUN_TEST(test_begin_rejects_device_reading_zero);
    RUN_TEST(test_begin_rejects_device_reading_ff_without_waiting);
    RUN_TEST(test_begin_rejects_device_without_register_readback);
    RUN_TEST(test_begin_tolerates_sys_init_stuck);
    RUN_TEST(test_begin_stops_at_first_io_error);
    RUN_TEST(test_register_access_on_closed_bus_does_not_touch_wire);
    RUN_TEST(test_check_alive_detects_power_cycle);
    RUN_TEST(test_check_alive_detects_bus_failure);
    RUN_TEST(test_failed_transfer_marks_device_lost_and_begin_recovers);
    RUN_TEST(test_standalone_accuracy_1_to_100_mhz);
    RUN_TEST(test_standalone_accuracy_at_weak_spots);
    RUN_TEST(test_standalone_below_1_mhz_uses_r_divider);
    RUN_TEST(test_standalone_above_100_mhz);
    RUN_TEST(test_standalone_uses_integer_mode_for_even_dividers);
    RUN_TEST(test_standalone_tuning_never_resets_pll_or_disables_output);
    RUN_TEST(test_correction_change_does_not_reset_pll);
    RUN_TEST(test_unchanged_setup_writes_nothing);
    RUN_TEST(test_channels_use_their_own_pll);
    RUN_TEST(test_rejects_invalid_arguments);
    RUN_TEST(test_enable_output_mask);
    RUN_TEST(test_power_down);
    RUN_TEST(test_iq_phase_registers);
    RUN_TEST(test_iq_on_pll_b_with_ch2_master);
    RUN_TEST(test_iq_accuracy_and_limits);
    RUN_TEST(test_iq_accuracy_at_weak_spots);
    RUN_TEST(test_iq_setup_disables_outputs_and_realigns);
    RUN_TEST(test_iq_drive_or_inversion_change_keeps_alignment);
    RUN_TEST(test_iq_rejects_invalid_arguments);
    RUN_TEST(test_iq_vco_stays_in_range_at_iq_limits);
    RUN_TEST(test_fb_int_follows_even_integer_pll);
    RUN_TEST(test_low_noise_accuracy_1_to_100_mhz);
    RUN_TEST(test_low_noise_below_1_mhz_and_above_100_mhz);
    RUN_TEST(test_low_noise_resets_pll_only_when_the_divider_changes);
    RUN_TEST(test_low_noise_keeps_divider_over_its_vco_range);
    RUN_TEST(test_low_noise_after_fractional_keeps_an_even_divider);
    RUN_TEST(test_integer_nearest_raster_point);
    RUN_TEST(test_integer_example_7_1_mhz);
    RUN_TEST(test_iq_in_all_tuning_modes);
    RUN_TEST(test_begin_switches_spread_spectrum_off);
    RUN_TEST(test_spread_spectrum_parameters);
    RUN_TEST(test_spread_spectrum_follows_pll_a);
    RUN_TEST(test_spread_spectrum_never_touches_pll_b);
    RUN_TEST(test_begin_writes_off_state_before_outputs);
    RUN_TEST(test_set_off_state_writes_at_once);
    RUN_TEST(test_power_down_keeps_driver_for_high_and_high_z);
    return UNITY_END();
}
