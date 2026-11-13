// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// I2CBus against the Wire fake (S39, C-14). The tests named test_core_* pin down what the
// firmware assumes about arduino-esp32 Wire in 2.0.x and 3.x; the fake models exactly that.
// If a core update changes one of these, the device behaves differently than the host tests.

#include <unity.h>
#include <Wire.h>
#include <constants.h>
#include <hal/i2c_bus.h>
#include <pins.h>

static Fake::Si5351Chip& chip = Fake::si5351();
static constexpr uint8_t ADDR = 0x60;

void setUp() {
    chip.reset();
    Fake::clockMs() = 0;
    Fake::wireMisuse() = false;
    Wire.failBegin = false;
    Wire.failEnd = false;
    TEST_ASSERT_TRUE(I2CBus::begin());
    Wire.resetCounters();
}

void tearDown() {}

// begin() on a running bus keeps the old pins (both cores: "Bus already started"), so a
// change of the assignment must end() first.
static void test_core_begin_on_running_bus_keeps_pins() {
    TEST_ASSERT_TRUE(Wire.begin(PIN_I2C_B, PIN_I2C_A, I2C_DETECT_HZ));
    TEST_ASSERT_EQUAL_INT(PIN_I2C_A, Wire.sda());
    TEST_ASSERT_TRUE(I2CBus::open(I2CMap::Swapped));
    TEST_ASSERT_EQUAL_UINT(1, Wire.endCalls);
    TEST_ASSERT_EQUAL_INT(PIN_I2C_B, Wire.sda());
    TEST_ASSERT_EQUAL_INT(PIN_I2C_A, Wire.scl());
}

// The same assignment again keeps the driver: no end()/begin(), only the clock.
static void test_open_same_assignment_keeps_driver() {
    TEST_ASSERT_TRUE(I2CBus::open(I2CMap::Default));
    TEST_ASSERT_EQUAL_UINT(0, Wire.endCalls);
    TEST_ASSERT_EQUAL_UINT(0, Wire.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(I2C_DETECT_HZ, Wire.clockHz);
}

// A failed end() (2.0.x: driver left without buffers) counts as closed.
static void test_core_failed_end_counts_as_closed() {
    Wire.failEnd = true;
    TEST_ASSERT_FALSE(I2CBus::open(I2CMap::Swapped));
    TEST_ASSERT_FALSE(I2CBus::isOpen());
    uint8_t v = 0;
    TEST_ASSERT_FALSE(I2CBus::readRegs(ADDR, 0, &v, 1));
    TEST_ASSERT_FALSE(Fake::wireMisuse());
}

// A transfer on a closed bus takes the Wire lock for good (both cores); I2CBus refuses it
// before Wire sees it.
static void test_core_closed_bus_is_never_touched() {
    Wire.failBegin = true;
    TEST_ASSERT_FALSE(I2CBus::open(I2CMap::Swapped));
    const uint8_t data[2] = {1, 2};
    uint8_t rx[2] = {0, 0};
    TEST_ASSERT_EQUAL_UINT8(4, I2CBus::probe(ADDR));
    TEST_ASSERT_FALSE(I2CBus::writeRegs(ADDR, 16, data, 2));
    TEST_ASSERT_FALSE(I2CBus::readRegs(ADDR, 0, rx, 2));
    TEST_ASSERT_FALSE(Fake::wireMisuse());
    TEST_ASSERT_EQUAL_UINT(0, chip.writes.size());
}

// The fake flags a transfer on a closed bus, so the test above would catch a missing check.
static void test_fake_flags_closed_bus_transfer() {
    Wire.end();
    Wire.beginTransmission(ADDR);
    TEST_ASSERT_EQUAL_UINT8(4, Wire.endTransmission(true));
    TEST_ASSERT_TRUE(Fake::wireMisuse());
}

// A line held low gives code 5 (timeout); AppController reports "line held low" from it.
static void test_core_line_held_low_is_code_5() {
    chip.lineLow = Fake::Si5351Chip::Line::Sda;
    TEST_ASSERT_EQUAL_UINT8(5, I2CBus::probe(ADDR));
    uint8_t v = 0;
    TEST_ASSERT_FALSE(I2CBus::writeRegs(ADDR, 3, &v, 1));
    TEST_ASSERT_FALSE(I2CBus::readRegs(ADDR, 0, &v, 1));
}

// No device at the address: probe 2 (NACK), write false.
static void test_core_nack_is_code_2() {
    TEST_ASSERT_EQUAL_UINT8(2, I2CBus::probe(0x61));
    const uint8_t v = 0;
    TEST_ASSERT_FALSE(I2CBus::writeRegs(0x61, 3, &v, 1));
    TEST_ASSERT_EQUAL_UINT8(0, I2CBus::probe(ADDR));
}

// Repeated start: endTransmission(false) only marks the pointer write, the read sends it.
// A device that went away is noticed by the short read, not by endTransmission(false).
static void test_core_repeated_start_fails_in_the_read() {
    const uint8_t w[2] = {0x12, 0x34};
    TEST_ASSERT_TRUE(I2CBus::writeRegs(ADDR, 26, w, 2));
    uint8_t r[2] = {0, 0};
    TEST_ASSERT_TRUE(I2CBus::readRegs(ADDR, 26, r, 2));
    TEST_ASSERT_EQUAL_HEX8(0x12, r[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, r[1]);
    TEST_ASSERT_EQUAL_UINT(1, Wire.deferredWrites);

    TEST_ASSERT_FALSE(I2CBus::readRegs(0x61, 26, r, 2));
    TEST_ASSERT_EQUAL_UINT(2, Wire.deferredWrites);
    TEST_ASSERT_FALSE(Fake::wireMisuse());
}

// A write with STOP reaches the chip register by register.
static void test_write_regs_writes_the_block() {
    const uint8_t w[3] = {0xA1, 0xB2, 0xC3};
    TEST_ASSERT_TRUE(I2CBus::writeRegs(ADDR, 42, w, 3));
    TEST_ASSERT_EQUAL_HEX8(0xA1, chip.regs[42]);
    TEST_ASSERT_EQUAL_HEX8(0xB2, chip.regs[43]);
    TEST_ASSERT_EQUAL_HEX8(0xC3, chip.regs[44]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_core_begin_on_running_bus_keeps_pins);
    RUN_TEST(test_open_same_assignment_keeps_driver);
    RUN_TEST(test_core_failed_end_counts_as_closed);
    RUN_TEST(test_core_closed_bus_is_never_touched);
    RUN_TEST(test_fake_flags_closed_bus_transfer);
    RUN_TEST(test_core_line_held_low_is_code_5);
    RUN_TEST(test_core_nack_is_code_2);
    RUN_TEST(test_core_repeated_start_fails_in_the_read);
    RUN_TEST(test_write_regs_writes_the_block);
    return UNITY_END();
}
