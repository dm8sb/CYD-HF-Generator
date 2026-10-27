#include "si5351_manager.h"
#include <Wire.h>
#include <constants.h>
#include <utils/logger.h>
#include <cstring>
#include <math.h>

using Ratio = SI5351Manager::Ratio;

static constexpr uint8_t SI5351_ADDR = 0x60;

// Register map (AN619).
static constexpr uint8_t REG_STATUS = 0;
static constexpr uint8_t REG_OUTPUT_ENABLE = 3;   // 1 = output disabled
static constexpr uint8_t REG_PLL_SOURCE = 15;
static constexpr uint8_t REG_CLK0_CONTROL = 16;
static constexpr uint8_t REG_PLLA_PARAMS = 26;
static constexpr uint8_t REG_PLLB_PARAMS = 34;
static constexpr uint8_t REG_MS0_PARAMS = 42;     // 8 bytes per MultiSynth
static constexpr uint8_t REG_CLK0_PHASE = 165;
static constexpr uint8_t REG_PLL_RESET = 177;
static constexpr uint8_t REG_XTAL_LOAD = 183;

static constexpr uint8_t STATUS_SYS_INIT = 0x80;
static constexpr uint8_t CTRL_PDN = 0x80;
static constexpr uint8_t CTRL_MS_INT = 0x40;
static constexpr uint8_t CTRL_PLLB = 0x20;
static constexpr uint8_t CTRL_INV = 0x10;
static constexpr uint8_t CTRL_SRC_MS = 0x0C;
static constexpr uint8_t CTRL_DRIVE_MASK = 0x03;
static constexpr uint8_t RESET_PLLA = 0x20;
static constexpr uint8_t RESET_PLLB = 0x80;
static constexpr uint8_t XTAL_LOAD_10PF = 0xD2;   // bits 5:0 must be 010010b

static constexpr uint32_t MAX_DENOM = 1048575;    // 20-bit fraction denominator
static constexpr uint64_t VCO_MIN_HZ = 600000000ULL;
static constexpr uint64_t VCO_MAX_HZ = 900000000ULL;
static constexpr Ratio FIXED_PLLS[] = {{36, 0, 1}, {35, 1, 2}}; // x 25 MHz = 900 / 887.5 MHz
static constexpr double FIXED_PLL_KEEP_ERR_HZ = 0.25;
static constexpr uint32_t MS_MIN_HZ = 1000000;    // lower outputs use the R divider
static constexpr uint64_t OUT_MIN_HZ = 8000;
static constexpr uint64_t OUT_MAX_HZ = 160000000ULL;
static constexpr uint32_t IQ_MAX_DIV = 127;       // 7-bit phase offset register

static uint64_t absDiff(uint64_t a, uint64_t b) { return a > b ? a - b : b - a; }

// num / den as a + b / c, with b / c the best rational approximation for c <= MAX_DENOM
// (continued fractions). Error is far below 1 mHz at the output.
static Ratio approxRatio(uint64_t num, uint64_t den) {
    Ratio r {(uint32_t)(num / den), 0, 1};
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

// ---------------------------------------------------------------------------

bool SI5351Manager::begin(int32_t correction) {
    correction_ = correction;
    ready_ = false;
    outputMask_ = 0;
    for (uint8_t i = 0; i < 3; ++i) {
        out_[i] = OutputCache {CTRL_PDN, {}, 0, false};
        actualHz_[i] = 0;
    }
    pll_[0].valid = pll_[1].valid = false;

    if (!probe_()) return false;

    // Wait until the chip has finished its power-up initialization.
    uint8_t status = STATUS_SYS_INIT;
    for (uint8_t i = 0; i < 20 && (status & STATUS_SYS_INIT); ++i) {
        if (!readRegs_(REG_STATUS, &status, 1)) return false;
        if (status & STATUS_SYS_INIT) delay(1);
    }

    bool ok = writeReg_(REG_OUTPUT_ENABLE, 0xFF);
    for (uint8_t clk = 0; clk < 8; ++clk) ok &= writeReg_(REG_CLK0_CONTROL + clk, CTRL_PDN);
    ok &= writeReg_(REG_PLL_SOURCE, 0x00);   // both PLLs from the crystal
    ok &= writeReg_(REG_XTAL_LOAD, XTAL_LOAD_10PF);
    if (!ok) {
        Logger::error("SI5351", "Init write failed");
        return false;
    }

    ready_ = true;
    Logger::debugValue("SI5351", "Device status reg0", status);
    Logger::info("SI5351", "Init OK; all outputs off");
    return true;
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

double SI5351Manager::synthesizedHz_(const Ratio& pll, const Ratio& ms, uint8_t rLog2) const {
    const double vco = (double)xtalQuarterHz_() / 4.0 * ((double)pll.a + (double)pll.b / (double)pll.c);
    return vco / ((double)ms.a + (double)ms.b / (double)ms.c) / (double)(1U << rLog2);
}

bool SI5351Manager::setupStandalone(uint8_t clk, Pll pll, uint64_t freq, uint8_t driveMa) {
    if (!ready_ || clk > 2) return false;
    if (freq < OUT_MIN_HZ || freq > OUT_MAX_HZ) return false;

    // R divider for low frequencies keeps the MultiSynth input >= 1 MHz.
    uint8_t rLog2 = 0;
    uint64_t msHz = freq;
    while (msHz < MS_MIN_HZ && rLog2 < 7) {
        msHz <<= 1;
        ++rLog2;
    }

    // Fixed PLL, fractional MultiSynth. Near 900 MHz / n the MultiSynth fraction cannot get
    // close enough (up to 5 Hz error at 100 MHz); the second PLL value has its weak spots
    // elsewhere. The current PLL is kept while it is good enough, so the PLL rarely changes.
    const uint64_t xtal4 = xtalQuarterHz_();
    Ratio pllR {0, 0, 1};
    Ratio msR {0, 0, 1};
    double bestErr = 1e9;
    for (const Ratio& cand : FIXED_PLLS) {
        const Ratio ms = approxRatio(((uint64_t)cand.a * cand.c + cand.b) * xtal4, 4 * msHz * cand.c);
        if (ms.a < 8) continue;
        const double err = fabs(synthesizedHz_(cand, ms, rLog2) - (double)freq);
        uint8_t regs[8];
        encode(cand, 0, false, regs);
        if (!pllDiffers_(pll, regs) && err <= FIXED_PLL_KEEP_ERR_HZ) {
            pllR = cand;
            msR = ms;
            break;
        }
        if (err < bestErr) {
            bestErr = err;
            pllR = cand;
            msR = ms;
        }
    }
    bool divBy4 = false;
    if (msR.a < 8) {
        // Above 112.5 MHz: even integer MultiSynth divider, fractional PLL.
        const uint32_t div = (6 * msHz <= VCO_MAX_HZ) ? 6 : 4;
        divBy4 = div == 4;
        msR = Ratio {div, 0, 1};
        pllR = approxRatio(4 * div * msHz, xtal4);
    }
    const bool msInt = msR.b == 0 && msR.a % 2 == 0;

    uint8_t pllRegs[8];
    uint8_t msRegs[8];
    encode(pllR, 0, false, pllRegs);
    encode(msR, rLog2, divBy4, msRegs);
    const bool pllChanged = pllDiffers_(pll, pllRegs);

    bool ok = writePll_(pll, pllRegs) && writeOutput_(clk, control(pll, driveMa, msInt, false), msRegs, 0);
    if (ok && pllChanged) ok = resetPll_(pll);
    actualHz_[clk] = synthesizedHz_(pllR, msR, rLog2);

    Logger::debugValue("SI5351", "Standalone CLK", clk);
    Logger::debugValue("SI5351", "Frequency Hz", (int64_t)freq);
    Logger::debugValue("SI5351", "PLL reset", pllChanged);
    return finish_(ok);
}

bool SI5351Manager::setupIQPair(uint8_t masterClk, uint8_t followerClk, Pll pll, uint64_t freq, uint8_t driveMa, uint16_t phaseDeg) {
    if (!ready_ || masterClk > 2 || followerClk > 2 || masterClk == followerClk) return false;
    if (!isValidPhase(phaseDeg) || freq < IQ_MIN_HZ || freq > IQ_MAX_HZ) return false;

    // Integer divider (fractional mode) with the VCO as close to 900 MHz as possible.
    uint64_t div = VCO_MAX_HZ / freq;
    if (div > IQ_MAX_DIV) div = IQ_MAX_DIV;
    if (div < 9 || div * freq < VCO_MIN_HZ) {
        Logger::error("SI5351", "IQ divider out of range");
        return false;
    }
    const Ratio msR {(uint32_t)div, 0, 1};
    const Ratio pllR = approxRatio(4 * div * freq, xtalQuarterHz_());

    uint8_t pllRegs[8];
    uint8_t msRegs[8];
    encode(pllR, 0, false, pllRegs);
    encode(msR, 0, false, msRegs);
    // One phase unit is a quarter VCO period, so 90 deg = MultiSynth divider.
    const uint8_t offset = (phaseDeg == 90 || phaseDeg == 270) ? (uint8_t)div : 0;
    const uint8_t ctrlMaster = control(pll, driveMa, false, false);
    const uint8_t ctrlFollower = control(pll, driveMa, false, phaseDeg >= 180);

    // The phase relation needs a PLL reset after any change except drive strength / inversion.
    auto needsAlign = [&](uint8_t clk, uint8_t ctrl) {
        return (out_[clk].ctrl & CTRL_PDN) || ((out_[clk].ctrl ^ ctrl) & ~(CTRL_DRIVE_MASK | CTRL_INV));
    };
    const bool align = pllDiffers_(pll, pllRegs) || msDiffers_(masterClk, msRegs, 0) ||
                       msDiffers_(followerClk, msRegs, offset) ||
                       needsAlign(masterClk, ctrlMaster) || needsAlign(followerClk, ctrlFollower);

    bool ok = true;
    if (align) ok = enableOutput(masterClk, false) && enableOutput(followerClk, false);
    ok = ok && writePll_(pll, pllRegs) && writeOutput_(masterClk, ctrlMaster, msRegs, 0) &&
         writeOutput_(followerClk, ctrlFollower, msRegs, offset);
    if (ok && align) ok = resetPll_(pll);
    actualHz_[masterClk] = actualHz_[followerClk] = synthesizedHz_(pllR, msR, 0);

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
    if (out_[clk].ctrl == CTRL_PDN) return true;
    if (!writeReg_(REG_CLK0_CONTROL + clk, CTRL_PDN)) return finish_(false);
    out_[clk].ctrl = CTRL_PDN;
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

bool SI5351Manager::writePll_(Pll pll, const uint8_t* regs) {
    if (!pllDiffers_(pll, regs)) return true;
    PllCache& c = pll_[(uint8_t)pll];
    c.valid = false;
    if (!writeRegs_(pll == Pll::A ? REG_PLLA_PARAMS : REG_PLLB_PARAMS, regs, sizeof(c.regs))) return false;
    memcpy(c.regs, regs, sizeof(c.regs));
    c.valid = true;
    return true;
}

bool SI5351Manager::writeOutput_(uint8_t clk, uint8_t ctrl, const uint8_t* ms, uint8_t phase) {
    OutputCache& o = out_[clk];
    if (msDiffers_(clk, ms, phase)) {
        o.msValid = false;
        if (!writeRegs_(REG_MS0_PARAMS + 8 * clk, ms, sizeof(o.ms))) return false;
        if (!writeReg_(REG_CLK0_PHASE + clk, phase & 0x7F)) return false;
        memcpy(o.ms, ms, sizeof(o.ms));
        o.phase = phase;
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
    Wire.beginTransmission(SI5351_ADDR);
    return Wire.endTransmission(true) == 0;
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

bool SI5351Manager::readRegs_(uint8_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(SI5351_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(SI5351_ADDR, len, (uint8_t)true) != len) return false;
    for (uint8_t i = 0; i < len; ++i) data[i] = (uint8_t)Wire.read();
    return true;
}
