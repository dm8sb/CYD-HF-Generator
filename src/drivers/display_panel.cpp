#include "display_panel.h"
#include <SPI.h>
#include <driver/gpio.h>
#include <config.h>
#include <utils/logger.h>

namespace {

// Raw access on the TFT_eSPI SPI instance (HSPI), each transfer in its own transaction.
// 1 MHz: the CYD2USB drives MISO weakly, reads fail from 16 MHz on (measured: 8 MHz is fine).
class SpiPanelBus : public PanelBus {
public:
    explicit SpiPanelBus(SPIClass& spi) : spi_(spi) {}

    void write(uint8_t cmd, const uint8_t* data, uint8_t len) override {
        begin_(cmd);
        for (uint8_t i = 0; i < len; ++i) spi_.transfer(data[i]);
        end_();
    }
    void read(uint8_t cmd, uint8_t* buf, uint8_t len) override {
        begin_(cmd);
        for (uint8_t i = 0; i < len; ++i) buf[i] = spi_.transfer(0x00);
        end_();
    }
    void delayMs(uint32_t ms) override { delay(ms); }

private:
    void begin_(uint8_t cmd) {
        spi_.beginTransaction(SPISettings(1000000, MSBFIRST, TFT_SPI_MODE));
        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, LOW);
        spi_.transfer(cmd);
        digitalWrite(TFT_DC, HIGH);
    }
    void end_() {
        digitalWrite(TFT_CS, HIGH);
        spi_.endTransaction();
    }

    SPIClass& spi_;
};

void hex(char* out, const uint8_t* b) { snprintf(out, 9, "%02X%02X%02X%02X", b[0], b[1], b[2], b[3]); }

}

PanelDecision DisplayPanel::begin(PanelType stored, bool gesture) {
    // Same pins as TFT_eSPI uses; its own begin() later keeps the running bus.
    SPIClass& spi = TFT_eSPI::getSPIinstance();
    spi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
    // Even the internal pull-up overrides the CYD2USB's MISO signal.
    gpio_set_pull_mode((gpio_num_t)TFT_MISO, GPIO_FLOATING);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(TFT_DC, OUTPUT);
    digitalWrite(TFT_DC, HIGH);

    SpiPanelBus bus(spi);
    // One bad read on the weak CYD2USB MISO must not select the wrong controller.
    uint8_t attempts = 0;
    do {
        probe_ = DisplayDetect::probe(bus);
    } while (probe_.panel == PanelType::Unknown && ++attempts < 3);
    const PanelDecision d = DisplayDetect::decide(probe_, stored, gesture, DISPLAY_FORCE_PANEL);
    panel_ = d.panel;

    tft_.init(); // ILI9341 sequence
    DisplayDetect::postInit(bus, panel_);
    setRotation_(1);
    tft_.fillScreen(TFT_BLACK);

    char a[9], b[9], id[9], line[128];
    hex(a, probe_.madA);
    hex(b, probe_.madB);
    hex(id, probe_.rddid);
    snprintf(line, sizeof(line), "%s (%s); detected %s: ID %06lX, MADCTL read %s/%s, RDDID %s, attempts %u",
             DisplayDetect::name(panel_), DisplayDetect::sourceName(d.source), DisplayDetect::name(probe_.panel),
             (unsigned long)probe_.id, a, b, id, attempts + (probe_.panel == PanelType::Unknown ? 0 : 1));
    Logger::info("DISPLAY", line);
    if (probe_.panel == PanelType::ST7789 && !DisplayDetect::knownId(probe_.id)) {
        Logger::error("DISPLAY", "Unknown controller ID; assuming ST7789");
    }
    return d;
}

void DisplayPanel::setRotation_(uint8_t r) {
    tft_.setRotation(r); // ILI9341 MADCTL, width and height
    if (panel_ == PanelType::ST7789) {
        tft_.writecommand(0x36);
        tft_.writedata(DisplayDetect::MADCTL_ST7789[r & 3]);
    }
}

void DisplayPanel::flush(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* pixels) {
    tft_.startWrite();
    tft_.setAddrWindow(x, y, w, h);
    tft_.pushColors(pixels, (uint32_t)w * h, true);
    tft_.endWrite();
}
