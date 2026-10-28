#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "display_detect.h"

// The display of both board variants. TFT_eSPI is built for the ILI9341; on the ST7789
// this class replaces the init sequence and the MADCTL values. Everything else (CASET,
// RASET, RAMWR, 16-bit color) is the same on both controllers.
// Never call setRotation(), readPixel() or readRect() on TFT_eSPI directly.
class DisplayPanel {
public:
    // Detects the controller (unless forced or overridden), initializes it in landscape
    // and clears the screen. Never fails; the backlight should still be off.
    PanelDecision begin(PanelType stored, bool gesture);
    void flush(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* pixels);
    PanelType panel() const { return panel_; }
    const ProbeResult& probeResult() const { return probe_; }

private:
    void setRotation_(uint8_t r);

    TFT_eSPI tft_;
    PanelType panel_ = PanelType::ILI9341;
    ProbeResult probe_ {};
};
