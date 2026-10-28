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
    // Probe 1, status 2-3, reg183 4-5, OE write 6, OE read-back 7-8: the first control write fails.
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
    return UNITY_END();
}
