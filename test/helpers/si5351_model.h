#pragma once

// Decodes the registers of the fake SI5351 (test/fakes/Wire.h) back into frequencies,
// following AN619 independently of the driver's own arithmetic.

#include <Wire.h>

namespace Si5351Model {

static constexpr double XTAL_HZ = 25e6;
static constexpr uint8_t CTRL_PDN = 0x80;
static constexpr uint8_t CTRL_MS_INT = 0x40;
static constexpr uint8_t CTRL_PLLB = 0x20;
static constexpr uint8_t CTRL_INV = 0x10;

struct Block {
    uint32_t p1;
    uint32_t p2;
    uint32_t p3;
    uint8_t rLog2;
    bool divBy4;
    // a + b / c
    double ratio() const { return divBy4 ? 4.0 : (p1 + 512.0 + (double)p2 / p3) / 128.0; }
    bool integer() const { return divBy4 || p2 == 0; }
};

inline const uint8_t* regs() { return Fake::si5351().regs; }

inline Block block(uint8_t firstReg) {
    const uint8_t* r = regs() + firstReg;
    Block b;
    b.p3 = ((uint32_t)(r[5] & 0xF0) << 12) | ((uint32_t)r[0] << 8) | r[1];
    b.p1 = ((uint32_t)(r[2] & 0x03) << 16) | ((uint32_t)r[3] << 8) | r[4];
    b.p2 = ((uint32_t)(r[5] & 0x0F) << 16) | ((uint32_t)r[6] << 8) | r[7];
    b.rLog2 = (r[2] >> 4) & 0x07;
    b.divBy4 = (r[2] & 0x0C) == 0x0C;
    return b;
}

inline uint8_t ctrl(uint8_t clk) { return regs()[16 + clk]; }
inline bool usesPllB(uint8_t clk) { return ctrl(clk) & CTRL_PLLB; }
inline Block pll(uint8_t clk) { return block(usesPllB(clk) ? 34 : 26); }
inline Block ms(uint8_t clk) { return block(42 + 8 * clk); }
inline uint8_t phase(uint8_t clk) { return regs()[165 + clk] & 0x7F; }
inline bool inverted(uint8_t clk) { return ctrl(clk) & CTRL_INV; }
inline bool poweredDown(uint8_t clk) { return ctrl(clk) & CTRL_PDN; }
inline bool enabled(uint8_t clk) { return !(regs()[3] & (1U << clk)) && !poweredDown(clk); }
inline uint8_t driveMa(uint8_t clk) { return 2 + 2 * (ctrl(clk) & 0x03); }

// The real crystal as the driver models it: 25 MHz * (1 + correction * 1e-8).
inline double xtalHz(int32_t correction) { return XTAL_HZ * (1.0 + correction * 1e-8); }
inline double vcoHz(uint8_t clk, int32_t correction) { return xtalHz(correction) * pll(clk).ratio(); }
inline double outputHz(uint8_t clk, int32_t correction = 0) {
    const Block m = ms(clk);
    return vcoHz(clk, correction) / m.ratio() / (double)(1U << m.rLog2);
}

// AN619 register limits. Returns nullptr, or the first violated rule.
inline const char* limitViolation(uint8_t clk, int32_t correction = 0) {
    const Block p = pll(clk);
    const Block m = ms(clk);
    if (p.p3 == 0 || (!m.divBy4 && m.p3 == 0)) return "P3 = 0";
    if (p.p2 >= p.p3) return "PLL P2 >= P3";
    if (!m.divBy4 && m.p2 >= m.p3) return "MS P2 >= P3";
    if (p.ratio() < 15.0 || p.ratio() > 90.0) return "PLL ratio outside 15..90";
    // The crystal tolerance (correction) is allowed on top of the 600-900 MHz VCO range.
    const double vco = vcoHz(clk, correction);
    if (vco < 600e6 * (1 - 6e-5) || vco > 900e6 * (1 + 6e-5)) return "VCO outside 600..900 MHz";
    const double r = m.ratio();
    const bool evenInt = m.integer() && fmod(r, 2.0) == 0.0;
    if (r < 4.0 || r > 2048.0) return "MultiSynth ratio outside 4..2048";
    if (r < 8.0 && !evenInt) return "MultiSynth ratio < 8 is not 4 or 6";
    if (r < 8.0 && !(ctrl(clk) & CTRL_MS_INT)) return "MultiSynth 4/6 without MS_INT";
    if ((ctrl(clk) & CTRL_MS_INT) && !evenInt) return "MS_INT with a non-even-integer divider";
    if (vco / r > 150e6 * (1 + 6e-5) && !m.divBy4) return "> 150 MHz without DIVBY4";
    if (phase(clk) != 0 && (ctrl(clk) & CTRL_MS_INT)) return "phase offset in integer mode";
    if ((ctrl(clk) & 0x0C) != 0x0C) return "CLK source is not its MultiSynth";
    return nullptr;
}

} // namespace Si5351Model
