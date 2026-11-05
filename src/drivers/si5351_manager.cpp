#include "si5351_manager.h"
#include <hal/i2c_bus.h>
#include <config.h>
#include <constants.h>
#include <utils/logger.h>
#include <cstring>

using Ratio = SI5351Manager::Ratio;

// 1 = Low noise resets the PLL after every change, not only when the MultiSynth divider changes.
// Fallback in case the PLL does not follow small changes cleanly (measurement D5 / K6).
#ifndef LOW_NOISE_RESET_EACH_STEP
#define LOW_NOISE_RESET_EACH_STEP 0
#endif

// Register map (AN619).
static constexpr uint8_t REG_STATUS = 0;
static constexpr uint8_t REG_OUTPUT_ENABLE = 3;   // 1 = output disabled
static constexpr uint8_t REG_PLL_SOURCE = 15;
static constexpr uint8_t REG_CLK0_CONTROL = 16;
static constexpr uint8_t REG_CLK6_CONTROL = 22;   // bit 6 FBA_INT; CLK7 (23): FBB_INT
static constexpr uint8_t REG_DIS_STATE = 24;      // CLK3..0_DIS_STATE, 2 bits each
static constexpr uint8_t REG_PLLA_PARAMS = 26;
static constexpr uint8_t REG_PLLB_PARAMS = 34;
static constexpr uint8_t REG_MS0_PARAMS = 42;     // 8 bytes per MultiSynth
static constexpr uint8_t REG_SSC_PARAMS = 149;    // 149..161
static constexpr uint8_t REG_CLK0_PHASE = 165;
static constexpr uint8_t REG_PLL_RESET = 177;
static constexpr uint8_t REG_XTAL_LOAD = 183;

static constexpr uint8_t STATUS_SYS_INIT = 0x80;
static constexpr uint8_t CTRL_PDN = 0x80;
static constexpr uint8_t CTRL_MS_INT = 0x40;
static constexpr uint8_t CTRL_FB_INT = 0x40;      // in the CLK6 / CLK7 control registers
static constexpr uint8_t CTRL_PLLB = 0x20;
static constexpr uint8_t CTRL_INV = 0x10;
static constexpr uint8_t CTRL_SRC_MS = 0x0C;
static constexpr uint8_t CTRL_DRIVE_MASK = 0x03;
static constexpr uint8_t RESET_PLLA = 0x20;
static constexpr uint8_t RESET_PLLB = 0x80;
static constexpr uint8_t XTAL_LOAD_10PF = 0xD2;   // bits 5:0 must be 010010b

static constexpr uint64_t OUT_MIN_HZ = 8000;
static constexpr uint64_t OUT_MAX_HZ = 160000000ULL;

// Spread spectrum (AN619 section 5). The modulation rate is fixed at about 31.5 kHz.
static constexpr uint32_t SSC_PFD_HZ = 25000000;  // PLL A input: the crystal
static constexpr uint32_t SSC_MOD_HZ = 31500;
static constexpr uint32_t SSC_P3 = 32767;

// PLL / MultiSynth parameter block (AN619 3.2 and 4.1).
static void encode(const Ratio& r, uint8_t rLog2, bool divBy4, uint8_t out[8]) {
    uint32_t p1 = 0, p2 = 0, p3 = 1;
    if (!divBy4) {
        const uint32_t f = (128 * r.b) / r.c;
        p1 = 128 * r.a + f - 512;
        p2 = 128 * r.b - r.c * f;
        p3 = r.c;
    }
    out[0] = (uint8_t)(p3 >> 8);
    out[1] = (uint8_t)p3;
    out[2] = (uint8_t)((rLog2 << 4) | (divBy4 ? 0x0C : 0x00) | ((p1 >> 16) & 0x03));
    out[3] = (uint8_t)(p1 >> 8);
    out[4] = (uint8_t)p1;
    out[5] = (uint8_t)(((p3 >> 12) & 0xF0) | ((p2 >> 16) & 0x0F));
    out[6] = (uint8_t)(p2 >> 8);
    out[7] = (uint8_t)p2;
}

static uint8_t driveBits(uint8_t ma) {
    switch (ma) {
        case 2: return 0;
        case 6: return 2;
        case 8: return 3;
        default: return 1;
    }
}

static uint8_t control(SI5351Manager::Pll pll, uint8_t driveMa, bool msInt, bool invert) {
    return CTRL_SRC_MS | driveBits(driveMa) | (msInt ? CTRL_MS_INT : 0) |
           (pll == SI5351Manager::Pll::B ? CTRL_PLLB : 0) | (invert ? CTRL_INV : 0);
}

// Register 24: the same level for CLK0..CLK2 (00 low, 01 high, 10 high-Z), CLK3 low.
static uint8_t disableStates(OffState s) {
    const uint8_t bits = (uint8_t)s & 0x03;
    return (uint8_t)(bits | (bits << 2) | (bits << 4));
}

// Splits a spread spectrum parameter into P1 + P2 / P3 with P3 = 32767 (AN619 5.1 / 5.2).
static void sscParams(double x, uint32_t& p1, uint32_t& p2) {
    p1 = (uint32_t)x;
    p2 = (uint32_t)(SSC_P3 * (x - p1));
}

// Registers 149..161 for the PLL A ratio (AN619 section 5); Off: all 0, i.e. SSC_EN = 0.
// SSUDP = floor(f_PFD / (4 * 31.5 kHz)), the modulation rate is f_PFD / (4 * SSUDP).
// Down spread:   SSDN = 64 * ratio * amp / ((1 + amp) * SSUDP), SSUP = 0 / 1.
// Center spread: SSUP = 128 * ratio * amp / ((1 - amp) * SSUDP),
//                SSDN = 128 * ratio * amp / ((1 + amp) * SSUDP).
static void spreadRegisters(const Ratio& pll, Spread s, uint8_t out[13]) {
    memset(out, 0, 13);
    if (s == Spread::Off) return;
    const bool center = isCenterSpread(s);
    const double amp = spreadAmplitude(s);
    const double ratio = (double)pll.a + (double)pll.b / (double)pll.c;
    const uint32_t udp = SSC_PFD_HZ / (4 * SSC_MOD_HZ); // 198: 31.57 kHz
    uint32_t dn1, dn2, up1 = 0, up2 = 0, up3 = 1;
    sscParams((center ? 128.0 : 64.0) * ratio * amp / ((1.0 + amp) * udp), dn1, dn2);
    if (center) {
        sscParams(128.0 * ratio * amp / ((1.0 - amp) * udp), up1, up2);
        up3 = SSC_P3;
    }
    out[0] = (uint8_t)(0x80 | ((dn2 >> 8) & 0x7F));                   // SSC_EN, SSDN_P2[14:8]
    out[1] = (uint8_t)dn2;                                            // SSDN_P2[7:0]
    out[2] = (uint8_t)((center ? 0x80 : 0) | ((SSC_P3 >> 8) & 0x7F)); // SSC_MODE, SSDN_P3[14:8]
    out[3] = (uint8_t)SSC_P3;                                         // SSDN_P3[7:0]
    out[4] = (uint8_t)dn1;                                            // SSDN_P1[7:0]
    out[5] = (uint8_t)(((udp >> 4) & 0xF0) | ((dn1 >> 8) & 0x0F));    // SSUDP[11:8], SSDN_P1[11:8]
    out[6] = (uint8_t)udp;                                            // SSUDP[7:0]
    out[7] = (uint8_t)((up2 >> 8) & 0x7F);                            // SSUP_P2[14:8]
    out[8] = (uint8_t)up2;                                            // SSUP_P2[7:0]
    out[9] = (uint8_t)((up3 >> 8) & 0x7F);                            // SSUP_P3[14:8]
    out[10] = (uint8_t)up3;                                           // SSUP_P3[7:0]
    out[11] = (uint8_t)up1;                                           // SSUP_P1[7:0]
    out[12] = (uint8_t)((up1 >> 8) & 0x0F);                           // SS_NCLK = 0, SSUP_P1[11:8]
}

// ---------------------------------------------------------------------------

bool SI5351Manager::begin(int32_t correction) {
    correction_ = correction;
    ready_ = false;
    idUnverified_ = false;
    outputMask_ = 0;
    for (uint8_t i = 0; i < 3; ++i) {
        out_[i] = OutputCache {CTRL_PDN, {}, 0, {0, 0, 1}, false};
        actualHz_[i] = 0;
    }
    for (PllCache& c : pll_) c = PllCache {{}, {0, 0, 1}, CTRL_PDN, false};
    memset(ssc_, 0, sizeof(ssc_));
    sscApplied_ = Spread::Off;

    if (!probe_()) return fail_(Error::NoAck);
    uint8_t status = 0;
    if (!identify_(status)) return false;

    // Wait until the chip has finished its power-up initialization. Clones may keep
    // SYS_INIT set; the output enable read-back below decides.
    for (uint8_t i = 0; i < 20 && (status & STATUS_SYS_INIT); ++i) {
        delay(1);
        if (!readRegs_(REG_STATUS, &status, 1)) return fail_(Error::Io);
    }
    if (status & STATUS_SYS_INIT) Logger::error("SI5351", "SYS_INIT still set; continuing");
    if (idUnverified_) Logger::error("SI5351", "Unexpected register 183 (clone?); continuing");

    // A register that keeps what was written. Outputs off is the first init write anyway.
    uint8_t oe = 0;
    if (!writeReg_(REG_OUTPUT_ENABLE, 0xFF) || !readRegs_(REG_OUTPUT_ENABLE, &oe, 1)) return fail_(Error::Io);
    if (oe != 0xFF) return fail_(Error::NoReadback);

    // Level of disabled outputs, before any output can be enabled.
    bool ok = writeReg_(REG_DIS_STATE, disableStates(offState_));
    // Includes CLK6 / CLK7 (not bonded out) with FBA_INT / FBB_INT = 0.
    for (uint8_t clk = 0; clk < 8 && ok; ++clk) ok = writeReg_(REG_CLK0_CONTROL + clk, CTRL_PDN);
    ok = ok && writeReg_(REG_PLL_SOURCE, 0x00);   // both PLLs from the crystal
    // Spread spectrum off (SSC_EN = 0), also on modules with a programmed NVM. The option
    // follows with the first PLL A setup.
    ok = ok && writeRegs_(REG_SSC_PARAMS, ssc_, sizeof(ssc_));
    ok = ok && writeReg_(REG_XTAL_LOAD, XTAL_LOAD_10PF);
    if (!ok) return fail_(Error::Io);

    ready_ = true;
    lastError_ = Error::None;
    Logger::debugValue("SI5351", "Device status reg0", status);
    Logger::info("SI5351", "Init OK; all outputs off");
    return true;
}

bool SI5351Manager::identify() {
    uint8_t status = 0;
    return identify_(status);
}

// Reads only: an open bus or another device reads 0x00 / 0xFF. A status of 0xFF also
// has SYS_INIT set and would only run into the wait in begin().
bool SI5351Manager::identify_(uint8_t& status) {
    if (!readRegs_(REG_STATUS, &status, 1)) return fail_(Error::Io);
    if (status == 0xFF) return fail_(Error::NotSi5351);
    // XTAL_CL (bits 7:6) 00 is reserved (AN619), reset value 11.
    uint8_t xtal = 0;
    if (!readRegs_(REG_XTAL_LOAD, &xtal, 1)) return fail_(Error::Io);
    if (xtal == 0x00 || xtal == 0xFF || (xtal & 0xC0) == 0) return fail_(Error::NotSi5351);
    idUnverified_ = (xtal & 0x3F) != (XTAL_LOAD_10PF & 0x3F);
    return true;
}

bool SI5351Manager::fail_(Error e) {
    lastError_ = e;
    Logger::debugValue("SI5351", "Init failed, error", (int64_t)e);
    return false;
}

void SI5351Manager::setSpread(Spread s) {
    // The VCO range depends on it, so PLL A may need other values: applied by the next setup.
    spread_ = s;
}

bool SI5351Manager::setOffState(OffState s) {
    offState_ = s;
    if (!ready_) return true;
    return finish_(writeReg_(REG_DIS_STATE, disableStates(s)));
}

bool SI5351Manager::checkAlive() {
    if (!ready_) return false;
    uint8_t ctrl[3];
    uint8_t oe = 0;
    bool ok = readRegs_(REG_OUTPUT_ENABLE, &oe, 1) && readRegs_(REG_CLK0_CONTROL, ctrl, 3);
    if (ok) {
        ok = oe == (uint8_t)~outputMask_;
        for (uint8_t i = 0; i < 3; ++i) ok &= ctrl[i] == out_[i].ctrl;
    }
    if (!ok) {
        Logger::error("SI5351", "Device lost or reset");
        ready_ = false;
    }
    return ok;
}

uint64_t SI5351Manager::xtalQuarterHz_() const {
    // 25 MHz * (1 + correction * 1e-8) in units of 0.25 Hz.
    return 100000000ULL + (int64_t)correction_;
}

bool SI5351Manager::setupStandalone(uint8_t clk, Pll pll, uint64_t freq, uint8_t driveMa) {
    if (!ready_ || clk > 2) return false;
    if (freq < OUT_MIN_HZ || freq > OUT_MAX_HZ) return false;

    const PllCache& pc = pll_[(uint8_t)pll];
    const OutputCache& oc = out_[clk];
    const SynthPlan p = planStandalone(freq, tuningMode_, pc.valid ? &pc.ratio : nullptr,
                                       oc.msValid ? &oc.msRatio : nullptr, xtalQuarterHz_(), spreadOn_(pll));
    if (!p.ok) return false;

    uint8_t pllRegs[8];
    uint8_t msRegs[8];
    encode(p.pll, 0, false, pllRegs);
    encode(p.ms, p.rLog2, p.divBy4, msRegs);
    // Low noise: the PLL follows the frequency without a reset while the divider stays.
    const bool reset = spreadChanges_(pll) ||
                       (pllDiffers_(pll, pllRegs) && (tuningMode_ != TuningMode::LowNoise ||
                                                      LOW_NOISE_RESET_EACH_STEP || !pc.valid ||
                                                      msDiffers_(clk, msRegs, 0)));

    bool ok = writePll_(pll, p.pll) && writeOutput_(clk, control(pll, driveMa, p.msInt, false), msRegs, 0, p.ms);
    if (ok && reset) ok = resetPll_(pll);
    actualHz_[clk] = p.actualHz;

    Logger::debugValue("SI5351", "Standalone CLK", clk);
    Logger::debugValue("SI5351", "Frequency Hz", (int64_t)freq);
    Logger::debugValue("SI5351", "PLL reset", reset);
    return finish_(ok);
}

bool SI5351Manager::setupIQPair(uint8_t masterClk, uint8_t followerClk, Pll pll, uint64_t freq, uint8_t driveMa, uint16_t phaseDeg) {
    if (!ready_ || masterClk > 2 || followerClk > 2 || masterClk == followerClk) return false;
    if (!isValidPhase(phaseDeg) || freq < IQ_MIN_HZ || freq > IQ_MAX_HZ) return false;
    if (TEST_FAULT_IQ && phaseDeg == 270) return false;

    // With spread spectrum on PLL A the range shrinks, e.g. +-0.5 %: up to 99.5 MHz (divider 9).
    const SynthPlan p = planIQ(freq, tuningMode_, xtalQuarterHz_(), spreadOn_(pll));
    if (!p.ok) {
        Logger::error("SI5351", "IQ divider out of range");
        return false;
    }

    uint8_t pllRegs[8];
    uint8_t msRegs[8];
    encode(p.pll, 0, false, pllRegs);
    encode(p.ms, 0, false, msRegs);
    // One phase unit is a quarter VCO period, so 90 deg = MultiSynth divider.
    const uint8_t offset = (phaseDeg == 90 || phaseDeg == 270) ? (uint8_t)p.ms.a : 0;
    const uint8_t ctrlMaster = control(pll, driveMa, false, false);
    const uint8_t ctrlFollower = control(pll, driveMa, false, phaseDeg >= 180);

    // The phase relation needs a PLL reset after any change except drive strength / inversion.
    auto needsAlign = [&](uint8_t clk, uint8_t ctrl) {
        return (out_[clk].ctrl & CTRL_PDN) || ((out_[clk].ctrl ^ ctrl) & ~(CTRL_DRIVE_MASK | CTRL_INV));
    };
    const bool align = spreadChanges_(pll) || pllDiffers_(pll, pllRegs) || msDiffers_(masterClk, msRegs, 0) ||
                       msDiffers_(followerClk, msRegs, offset) ||
                       needsAlign(masterClk, ctrlMaster) || needsAlign(followerClk, ctrlFollower);

    bool ok = true;
    if (align) ok = enableOutput(masterClk, false) && enableOutput(followerClk, false);
    ok = ok && writePll_(pll, p.pll) && writeOutput_(masterClk, ctrlMaster, msRegs, 0, p.ms) &&
         writeOutput_(followerClk, ctrlFollower, msRegs, offset, p.ms);
    if (ok && align) ok = resetPll_(pll);
    actualHz_[masterClk] = actualHz_[followerClk] = p.actualHz;

    Logger::debugValue("SI5351", "IQ frequency Hz", (int64_t)freq);
    Logger::debugValue("SI5351", "IQ phase deg", phaseDeg);
    Logger::debugValue("SI5351", "IQ phase register", offset);
    return finish_(ok);
}

bool SI5351Manager::enableOutput(uint8_t clk, bool enable) {
    if (!ready_ || clk > 2) return false;
    const uint8_t mask = enable ? (outputMask_ | (1U << clk)) : (outputMask_ & ~(1U << clk));
    if (mask == outputMask_) return true;
    if (!writeReg_(REG_OUTPUT_ENABLE, (uint8_t)~mask)) return finish_(false);
    outputMask_ = mask;
    Logger::debugValue("SI5351", "Output mask", outputMask_);
    return true;
}

bool SI5351Manager::powerDown(uint8_t clk) {
    if (!ready_ || clk > 2) return false;
    if (!enableOutput(clk, false)) return false;
    actualHz_[clk] = 0;
    // Not measured yet (hardware test K2): the pin level of a powered-down driver. Assumed: CLKx_PDN
    // pulls it low, so CLKx_PDN only with off state Low. With High and High-Z the driver stays
    // powered and register 24 sets the level; a powered-down output (control 0x80) becomes 0x00:
    // driver on, crystal as its source, 2 mA, no MultiSynth needed.
    const uint8_t ctrl = offState_ == OffState::Low ? CTRL_PDN : (uint8_t)(out_[clk].ctrl & ~CTRL_PDN);
    if (out_[clk].ctrl == ctrl) return true;
    if (!writeReg_(REG_CLK0_CONTROL + clk, ctrl)) return finish_(false);
    out_[clk].ctrl = ctrl;
    return true;
}

// ---------------------------------------------------------------------------

bool SI5351Manager::pllDiffers_(Pll pll, const uint8_t* regs) const {
    const PllCache& c = pll_[(uint8_t)pll];
    return !c.valid || memcmp(c.regs, regs, sizeof(c.regs)) != 0;
}

bool SI5351Manager::msDiffers_(uint8_t clk, const uint8_t* ms, uint8_t phase) const {
    const OutputCache& o = out_[clk];
    return !o.msValid || o.phase != phase || memcmp(o.ms, ms, sizeof(o.ms)) != 0;
}

bool SI5351Manager::writePll_(Pll pll, const Ratio& ratio) {
    PllCache& c = pll_[(uint8_t)pll];
    uint8_t regs[8];
    encode(ratio, 0, false, regs);
    // FBx_INT for an even integer ratio (AN619 3.2.1), on PLL A not with spread spectrum. It is
    // cleared before and set after the parameters and the spread spectrum registers, so integer
    // mode never meets a fraction or a running modulation.
    const bool fbInt = isEvenInteger(ratio) && !(pll == Pll::A && spread_ != Spread::Off);
    if (!fbInt && !writeFbInt_(pll, false)) return false;
    if (pllDiffers_(pll, regs)) {
        c.valid = false;
        if (!writeRegs_(pll == Pll::A ? REG_PLLA_PARAMS : REG_PLLB_PARAMS, regs, sizeof(c.regs))) return false;
        memcpy(c.regs, regs, sizeof(c.regs));
        c.ratio = ratio;
        c.valid = true;
    }
    // The spread spectrum parameters depend on the PLL A ratio.
    if (pll == Pll::A && !writeSpread_(ratio)) return false;
    return !fbInt || writeFbInt_(pll, true);
}

bool SI5351Manager::writeFbInt_(Pll pll, bool on) {
    PllCache& c = pll_[(uint8_t)pll];
    const uint8_t value = CTRL_PDN | (on ? CTRL_FB_INT : 0); // CLK6 / CLK7 stay powered down
    if (c.fbCtrl == value) return true;
    if (!writeReg_(REG_CLK6_CONTROL + (uint8_t)pll, value)) return false;
    c.fbCtrl = value;
    return true;
}

bool SI5351Manager::writeSpread_(const Ratio& pllA) {
    uint8_t regs[SSC_REG_COUNT];
    spreadRegisters(pllA, spread_, regs);
    if (memcmp(regs, ssc_, sizeof(regs)) != 0) {
        if (!writeRegs_(REG_SSC_PARAMS, regs, sizeof(regs))) return false;
        memcpy(ssc_, regs, sizeof(regs));
    }
    sscApplied_ = spread_;
    return true;
}

bool SI5351Manager::writeOutput_(uint8_t clk, uint8_t ctrl, const uint8_t* ms, uint8_t phase, const Ratio& msRatio) {
    OutputCache& o = out_[clk];
    if (msDiffers_(clk, ms, phase)) {
        o.msValid = false;
        if (!writeRegs_(REG_MS0_PARAMS + 8 * clk, ms, sizeof(o.ms))) return false;
        if (!writeReg_(REG_CLK0_PHASE + clk, phase & 0x7F)) return false;
        memcpy(o.ms, ms, sizeof(o.ms));
        o.phase = phase;
        o.msRatio = msRatio;
        o.msValid = true;
    }
    if (o.ctrl != ctrl) {
        if (!writeReg_(REG_CLK0_CONTROL + clk, ctrl)) return false;
        o.ctrl = ctrl;
    }
    return true;
}

bool SI5351Manager::resetPll_(Pll pll) {
    return writeReg_(REG_PLL_RESET, pll == Pll::A ? RESET_PLLA : RESET_PLLB);
}

bool SI5351Manager::finish_(bool ok) {
    if (!ok && ready_) {
        Logger::error("SI5351", "I2C transfer failed; device lost");
        ready_ = false;
    }
    return ok;
}

bool SI5351Manager::probe_() {
    return I2CBus::probe(I2C_ADDR) == 0;
}

bool SI5351Manager::writeReg_(uint8_t reg, uint8_t value) {
    return writeRegs_(reg, &value, 1);
}

bool SI5351Manager::writeRegs_(uint8_t reg, const uint8_t* data, uint8_t len) {
    return I2CBus::writeRegs(I2C_ADDR, reg, data, len);
}

bool SI5351Manager::readRegs_(uint8_t reg, uint8_t* data, uint8_t len) {
    return I2CBus::readRegs(I2C_ADDR, reg, data, len);
}
