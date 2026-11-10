#pragma once

#include <stdint.h>

// Display controller detection for the two ESP32-2432S028R variants: the original CYD
// (ILI9341) and the CYD2USB with an additional USB-C port (ST7789). Hardware-independent,
// so it runs in the host tests; drivers/display_panel.* connects it to TFT_eSPI.

enum class PanelType : uint8_t { Unknown = 0, ILI9341 = 1, ST7789 = 2 }; // = DISPLAY_FORCE_PANEL
enum class PanelSource : uint8_t { Build, Detected, Default };

// Raw SPI access to the panel; the host tests use a panel model.
class PanelBus {
public:
    virtual ~PanelBus() {}
    virtual void write(uint8_t cmd, const uint8_t* data, uint8_t len) = 0;
    // Bytes clocked in right after the command (no dummy handling).
    virtual void read(uint8_t cmd, uint8_t* buf, uint8_t len) = 0;
    virtual void delayMs(uint32_t ms) = 0;
};

struct ProbeResult {
    bool readbackOk;   // MADCTL read back: the panel drives MISO
    uint8_t madA[4];   // 0Bh after MADCTL = A8h
    uint8_t madB[4];   // 0Bh after MADCTL = 54h
    uint8_t rddid[4];  // 04h, one dummy bit in front of ID1..ID3
    uint32_t id;       // ID1..ID3 without the dummy bit
    PanelType panel;   // Unknown: no usable answer
};

struct PanelDecision {
    PanelType panel;
    PanelSource source;
};

namespace DisplayDetect {
// Measured 2026-10-28: ILI9341 RDDID 00 00 00, CYD2USB RDDID 81 81 B3.
// ST7789V datasheet value: 85 85 52.
constexpr uint32_t ID_ST7789V = 0x858552;
constexpr uint32_t ID_CYD2USB = 0x8181B3;

// SWRESET, MADCTL read-back and RDDID; leaves MADCTL at 00h. Run before the panel init.
ProbeResult probe(PanelBus& bus);
// probe() until a controller is recognized, at most maxAttempts times (one bad read on the weak
// CYD2USB MISO must not select the wrong controller); *attempts = probes made.
ProbeResult probeWithRetries(PanelBus& bus, uint8_t maxAttempts, uint8_t* attempts);
bool knownId(uint32_t id);
// buildForce: DISPLAY_FORCE_PANEL (1 / 2; anything else detects). Without a recognized
// controller the original CYD (ILI9341) is assumed. There is no override at runtime since
// 1.3.2 (boot gesture removed, audit U-10).
PanelDecision decide(const ProbeResult& p, uint8_t buildForce);
// After the ILI9341 init of TFT_eSPI: nothing for the ILI9341; SWRESET and ST7789_INIT for the ST7789.
void postInit(PanelBus& bus, PanelType panel);

struct InitCmd {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[14];
    uint16_t delayMs; // after the command
};
// From TFT_eSPI 2.5.43 TFT_Drivers/ST7789_Init.h (Bodmer, FreeBSD license, THIRD_PARTY_NOTICES.md),
// as built for the CYD2USB before (TFT_RGB_ORDER=TFT_BGR, TFT_INVERSION_OFF): without
// fillScreen(), INVOFF instead of INVON + INVOFF.
extern const InitCmd ST7789_INIT[];
extern const uint8_t ST7789_INIT_LEN;
// MADCTL per rotation 0..3 (BGR); TFT_eSPI writes the ILI9341 values.
constexpr uint8_t MADCTL_ILI9341[4] = {0x48, 0x28, 0x88, 0xE8};
constexpr uint8_t MADCTL_ST7789[4] = {0x08, 0x68, 0xC8, 0xA8};

const char* name(PanelType t);
const char* sourceName(PanelSource s);
}
