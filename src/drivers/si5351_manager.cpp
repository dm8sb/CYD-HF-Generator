#include "si5351_manager.h"
#include <si5351.h>
#include <Wire.h>
#include <pins.h>
#include <constants.h>
#include <utils/logger.h>

// Defined in osmanovv/esp32-si5351 (pinned in platformio.ini). The library only sets it in
// si5351_Init(), which also powers down all outputs; writing it directly avoids that re-init.
extern int32_t si5351Correction;

static constexpr uint8_t SI5351_ADDR = 0x60;
static constexpr uint8_t REG_CLK0_CONTROL = 16;
static constexpr uint8_t REG_PLLA_PARAMS = 26;
static constexpr uint8_t REG_PLLB_PARAMS = 34;
static constexpr uint8_t REG_PLL_RESET = 177;
static constexpr uint8_t CLK_CTRL_INVERT = 0x10;

static si5351PLL_t toLib(SI5351Manager::Pll pll) {
    return pll == SI5351Manager::Pll::B ? SI5351_PLL_B : SI5351_PLL_A;
}

static si5351DriveStrength_t driveToLib(uint8_t ma) {
    switch (ma) {
        case 2: return SI5351_DRIVE_STRENGTH_2MA;
        case 6: return SI5351_DRIVE_STRENGTH_6MA;
        case 8: return SI5351_DRIVE_STRENGTH_8MA;
        case 4:
        default: return SI5351_DRIVE_STRENGTH_4MA;
    }
}

static double synthesizedHz(const si5351PLLConfig_t& pll, const si5351OutputConfig_t& out, int32_t correction) {
    double vco = 25000000.0 * ((double)pll.mult + (double)pll.num / (double)pll.denom);
    double div = (double)out.div + (double)out.num / (double)out.denom;
    double hz = vco / div / (double)(1 << out.rdiv);
    // The library lowers the target by `correction` (0.01 ppm) to compensate a fast crystal.
    return hz * (1.0 + (double)correction * 1e-8);
}

bool SI5351Manager::begin(int32_t correction) {
    correction_ = correction;
    Serial.printf("[SI5351] Init start (explicit SDA=%d SCL=%d)\n", PIN_I2C_SDA, PIN_I2C_SCL);

    if (!probeI2C_()) {
        initialized_ = false;
        Logger::error("SI5351", "No I2C ACK at address 0x60 before init. Check configured SDA/SCL pins, VCC, GND, pullups and module address.");
        return false;
    }

    si5351_Init(correction_, PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(I2C_FREQ_HZ);

    if (!probeI2C_()) {
        initialized_ = false;
        Logger::error("SI5351", "No I2C ACK at address 0x60 after init");
        return false;
    }

    si5351_EnableOutputs(0);
    outputMask_ = 0;
    initialized_ = true;

    bool ok = false;
    uint8_t status = readReg_(0, &ok);
    if (ok) {
        Logger::value("SI5351", "Device status reg0", status);
        Logger::value("SI5351", "SYS_INIT bit", status & 0x80 ? 1 : 0);
        Logger::value("SI5351", "LOL_B bit", status & 0x40 ? 1 : 0);
        Logger::value("SI5351", "LOL_A bit", status & 0x20 ? 1 : 0);
        Logger::value("SI5351", "LOS bit", status & 0x10 ? 1 : 0);
    } else {
        Logger::error("SI5351", "Could not read status register 0");
    }

    Logger::info("SI5351", "Init OK; all outputs High-Z");
    return true;
}

bool SI5351Manager::probeI2C_() {
    Wire.beginTransmission(SI5351_ADDR);
    uint8_t err = Wire.endTransmission(true);
    if (err != 0) Logger::value("SI5351", "I2C probe 0x60 error", err);
    return err == 0;
}

bool SI5351Manager::writeReg_(uint8_t reg, uint8_t value) {
    return writeRegs_(reg, &value, 1);
}

bool SI5351Manager::writeRegs_(uint8_t reg, const uint8_t* data, uint8_t len) {
    Wire.beginTransmission(SI5351_ADDR);
    Wire.write(reg);
    Wire.write(data, len);
    return Wire.endTransmission(true) == 0;
}

uint8_t SI5351Manager::readReg_(uint8_t reg, bool* ok) {
    if (ok) *ok = false;
    Wire.beginTransmission(SI5351_ADDR);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission(false);
    if (err != 0) {
        Logger::value("SI5351", "ReadReg address phase error", err);
        return 0;
    }
    int n = Wire.requestFrom(SI5351_ADDR, (uint8_t)1, (uint8_t)true);
    if (n != 1 || !Wire.available()) {
        Logger::value("SI5351", "ReadReg bytes", n);
        return 0;
    }
    if (ok) *ok = true;
    return Wire.read();
}

// Replaces si5351_SetupPLL(), which always resets both PLLs and thereby
// interrupts the channel running on the other PLL.
bool SI5351Manager::setupPll_(Pll pll, int32_t mult, int32_t num, int32_t denom) {
    const int32_t p1 = 128 * mult + (128 * num) / denom - 512;
    const int32_t p2 = (128 * num) % denom;
    const int32_t p3 = denom;
    const uint8_t regs[8] = {
        (uint8_t)((p3 >> 8) & 0xFF),
        (uint8_t)(p3 & 0xFF),
        (uint8_t)((p1 >> 16) & 0x03),
        (uint8_t)((p1 >> 8) & 0xFF),
        (uint8_t)(p1 & 0xFF),
        (uint8_t)(((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F)),
        (uint8_t)((p2 >> 8) & 0xFF),
        (uint8_t)(p2 & 0xFF),
    };
    bool ok = writeRegs_(pll == Pll::A ? REG_PLLA_PARAMS : REG_PLLB_PARAMS, regs, sizeof(regs));
    ok &= writeReg_(REG_PLL_RESET, pll == Pll::A ? 0x20 : 0x80);
    return ok;
}

void SI5351Manager::setCorrection(int32_t correction) {
    correction_ = correction;
    si5351Correction = correction;
    Logger::value("SI5351", "Correction (0.01 ppm)", correction);
}

bool SI5351Manager::setupStandalone(uint8_t clk, Pll pll, uint64_t freq, uint8_t driveMa) {
    if (!initialized_ || clk > 2) return false;
    enableOutput(clk, false);

    si5351PLLConfig_t pllConf;
    si5351OutputConfig_t out;
    si5351_Calc((int32_t)freq, &pllConf, &out);
    int rc = si5351_SetupOutput(clk, toLib(pll), driveToLib(driveMa), &out, 0);
    bool ok = setupPll_(pll, pllConf.mult, pllConf.num, pllConf.denom);
    actualHz_[clk] = synthesizedHz(pllConf, out, correction_);

    Logger::debugValue("SI5351", "Standalone CLK", clk);
    Logger::debugValue("SI5351", "Frequency Hz", (int64_t)freq);
    if (!probeI2C_()) {
        Logger::error("SI5351", "I2C probe failed after setupStandalone");
        return false;
    }
    return ok && rc == 0;
}

bool SI5351Manager::setupIQPair(uint8_t masterClk, uint8_t followerClk, Pll pll, uint64_t freq, uint8_t driveMa, uint16_t phaseDeg) {
    if (!initialized_ || masterClk > 2 || followerClk > 2 || masterClk == followerClk) return false;
    if (!isValidPhase(phaseDeg) || freq < IQ_MIN_HZ) return false;
    enableOutput(masterClk, false);
    enableOutput(followerClk, false);

    si5351PLLConfig_t pllConf;
    si5351OutputConfig_t out;
    si5351_CalcIQ((int32_t)freq, &pllConf, &out);
    if (out.div < 9) {
        // CalcIQ applies the correction after clamping to 100 MHz; a negative correction then
        // yields divider 8, which fractional mode rejects. Use 9 (VCO a few ppm above 900 MHz).
        const int32_t fxtal = 25000000;
        int32_t fclk = (int32_t)freq - (((int32_t)freq / 1000000) * correction_) / 100;
        int32_t fpll = fclk * 9;
        out.div = 9;
        pllConf.mult = fpll / fxtal;
        pllConf.num = (fpll % fxtal) / 24;
        pllConf.denom = fxtal / 24;
    }
    // The phase register is 7 bit; one unit is a quarter VCO period, so 90 deg = MultiSynth divider.
    if (out.div > 127) {
        Logger::error("SI5351", "IQ divider exceeds phase register");
        return false;
    }
    const uint8_t offset = (phaseDeg == 90 || phaseDeg == 270) ? (uint8_t)out.div : 0;
    const bool invert = phaseDeg >= 180;

    int rc1 = si5351_SetupOutput(masterClk, toLib(pll), driveToLib(driveMa), &out, 0);
    int rc2 = si5351_SetupOutput(followerClk, toLib(pll), driveToLib(driveMa), &out, offset);
    bool ok = rc1 == 0 && rc2 == 0;
    if (ok && invert) {
        // Same value si5351_SetupOutput() writes (fractional mode in IQ), plus CLK_INV.
        uint8_t ctrl = 0x0C | driveToLib(driveMa) | (pll == Pll::B ? 0x20 : 0x00) | CLK_CTRL_INVERT;
        ok &= writeReg_(REG_CLK0_CONTROL + followerClk, ctrl);
    }
    // PLL reset after writing the phase offsets aligns both outputs.
    ok &= setupPll_(pll, pllConf.mult, pllConf.num, pllConf.denom);
    actualHz_[masterClk] = actualHz_[followerClk] = synthesizedHz(pllConf, out, correction_);

    Logger::debugValue("SI5351", "IQ frequency Hz", (int64_t)freq);
    Logger::debugValue("SI5351", "IQ phase deg", phaseDeg);
    Logger::debugValue("SI5351", "IQ phase register", offset);
    if (!probeI2C_()) {
        Logger::error("SI5351", "I2C probe failed after setupIQPair");
        return false;
    }
    return ok;
}

bool SI5351Manager::enableOutput(uint8_t clk, bool enable) {
    if (!initialized_ || clk > 2) return false;
    uint8_t mask = enable ? (outputMask_ | (1U << clk)) : (outputMask_ & ~(1U << clk));
    if (mask == outputMask_) return true;
    outputMask_ = mask;
    applyOutputMask_();
    return true;
}

void SI5351Manager::applyOutputMask_() {
    si5351_EnableOutputs(outputMask_);
    Logger::debugValue("SI5351", "Output mask", outputMask_);
}
