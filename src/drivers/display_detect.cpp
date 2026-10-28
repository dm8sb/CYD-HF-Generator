#include "display_detect.h"
#include <string.h>

namespace DisplayDetect {

static constexpr uint8_t CMD_SWRESET = 0x01;
static constexpr uint8_t CMD_RDDID = 0x04;
static constexpr uint8_t CMD_RDDMADCTL = 0x0B;
static constexpr uint8_t CMD_MADCTL = 0x36;

const InitCmd ST7789_INIT[] = {
    {0x11, 0, {}, 120},                                    // SLPOUT
    {0x13, 0, {}, 0},                                      // NORON
    {0x36, 1, {0x08}, 0},                                  // MADCTL: BGR, rotation 0
    {0xB6, 2, {0x0A, 0x82}, 0},
    {0xB0, 2, {0x00, 0xE0}, 0},                            // RAMCTRL
    {0x3A, 1, {0x55}, 10},                                 // COLMOD: 16 bit
    {0xB2, 5, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 0},          // PORCTRL
    {0xB7, 1, {0x35}, 0},                                  // GCTRL
    {0xBB, 1, {0x28}, 0},                                  // VCOMS
    {0xC0, 1, {0x0C}, 0},                                  // LCMCTRL
    {0xC2, 2, {0x01, 0xFF}, 0},                            // VDVVRHEN
    {0xC3, 1, {0x10}, 0},                                  // VRHS
    {0xC4, 1, {0x20}, 0},                                  // VDVSET
    {0xC6, 1, {0x0F}, 0},                                  // FRCTR2
    {0xD0, 2, {0xA4, 0xA1}, 0},                            // PWCTRL1
    {0xE0, 14, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0E, 0x12, 0x14, 0x17}, 0},
    {0xE1, 14, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E}, 0},
    {0x20, 0, {}, 0},                                      // INVOFF
    {0x2A, 4, {0x00, 0x00, 0x00, 0xEF}, 0},                // CASET 0..239
    {0x2B, 4, {0x00, 0x00, 0x01, 0x3F}, 120},              // RASET 0..319
    {0x29, 0, {}, 0},                                      // DISPON
};
const uint8_t ST7789_INIT_LEN = sizeof(ST7789_INIT) / sizeof(ST7789_INIT[0]);

static uint32_t be32(const uint8_t* b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

// Two MADCTL values read back differ in bits 7:2 (A8h ^ 54h = FCh). A floating or stuck
// MISO reads the same twice. Shift 1 allows for a panel that sends a dummy bit first.
static bool readbackOk(const uint8_t* a, const uint8_t* b) {
    const uint32_t x = be32(a) ^ be32(b);
    for (uint8_t s = 0; s <= 1; ++s) {
        if ((((x << s) >> 24) & 0xFC) == 0xFC) return true;
    }
    return false;
}

ProbeResult probe(PanelBus& bus) {
    ProbeResult p {};
    // Known state, also after a warm start (RST is tied to EN). Restores ID1 after an
    // ILI9341 init on the ST7789, where C1h is IDSET.
    bus.write(CMD_SWRESET, nullptr, 0);
    bus.delayMs(10);
    uint8_t v = 0xA8;
    bus.write(CMD_MADCTL, &v, 1);
    bus.read(CMD_RDDMADCTL, p.madA, 4);
    v = 0x54;
    bus.write(CMD_MADCTL, &v, 1);
    bus.read(CMD_RDDMADCTL, p.madB, 4);
    uint8_t again[4];
    bus.read(CMD_RDDID, p.rddid, 4);
    bus.read(CMD_RDDID, again, 4);
    v = 0x00;
    bus.write(CMD_MADCTL, &v, 1);

    p.readbackOk = readbackOk(p.madA, p.madB);
    p.id = (be32(p.rddid) << 1) >> 8;
    if (!p.readbackOk || memcmp(p.rddid, again, 4) != 0 || p.id == 0xFFFFFF) {
        p.panel = PanelType::Unknown;
    } else {
        // The ILI9341 answers 00 00 00; any ID means another controller, so far always an ST7789.
        p.panel = p.id == 0 ? PanelType::ILI9341 : PanelType::ST7789;
    }
    return p;
}

bool knownId(uint32_t id) { return id == 0 || id == ID_ST7789V || id == ID_CYD2USB; }

static PanelType other(PanelType t) { return t == PanelType::ST7789 ? PanelType::ILI9341 : PanelType::ST7789; }

PanelDecision decide(const ProbeResult& p, PanelType stored, bool gesture, uint8_t buildForce) {
    if (buildForce == 1 || buildForce == 2) {
        return PanelDecision {(PanelType)buildForce, PanelSource::Build, false, PanelType::Unknown};
    }
    const PanelType automatic = p.panel != PanelType::Unknown ? p.panel : PanelType::ILI9341;
    PanelDecision d {automatic, p.panel != PanelType::Unknown ? PanelSource::Detected : PanelSource::Default,
                     false, PanelType::Unknown};
    if (stored == PanelType::ILI9341 || stored == PanelType::ST7789) {
        d.panel = stored;
        d.source = PanelSource::Stored;
    }
    if (gesture) {
        d.panel = other(d.panel);
        d.source = PanelSource::Gesture;
        d.save = true;
        // Back to automatic when the user undoes a wrong choice.
        d.saveValue = d.panel == automatic ? PanelType::Unknown : d.panel;
    }
    return d;
}

void postInit(PanelBus& bus, PanelType panel) {
    if (panel != PanelType::ST7789) return;
    bus.write(CMD_SWRESET, nullptr, 0);
    bus.delayMs(150); // >= 120 ms before SLPOUT
    for (uint8_t i = 0; i < ST7789_INIT_LEN; ++i) {
        const InitCmd& c = ST7789_INIT[i];
        bus.write(c.cmd, c.data, c.len);
        if (c.delayMs) bus.delayMs(c.delayMs);
    }
}

const char* name(PanelType t) {
    switch (t) {
        case PanelType::ILI9341: return "ILI9341";
        case PanelType::ST7789: return "ST7789";
        default: return "unknown";
    }
}

const char* sourceName(PanelSource s) {
    switch (s) {
        case PanelSource::Build: return "build flag";
        case PanelSource::Detected: return "detected";
        case PanelSource::Stored: return "stored";
        case PanelSource::Gesture: return "boot gesture";
        default: return "default";
    }
}

}
