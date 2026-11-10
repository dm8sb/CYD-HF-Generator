// Host replacements for the hardware classes of the GUI (see screens.h). Sample values only.
#include "screens.h"
#include <Arduino.h>
#include <app_fatal.h>
#include <config.h>
#include <esp_heap_caps.h>
#include <drivers/display_panel.h>
#include <drivers/status_led.h>
#include <drivers/touch.h>
#include <net/net_settings.h>
#include <utils/build_info.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace Screens {
uint16_t frame[H * W];
Touch touch {false, 0, 0};
WifiManager::Status wifiStatus {};
const char* buildBranch = "main";
const char* buildCommit = "unknown";
const char* buildTime = "unknown";
}

extern "C" uint32_t host_millis(void) { return millis(); }

extern "C" void app_fatal_restart(const char* reason) {
    fprintf(stderr, "fatal: %s\n", reason);
    exit(3);
}

// CONFIG info card: plausible numbers of a running device.
HostEsp ESP;
uint32_t HostEsp::getFreeHeap() const { return 118u * 1024u; }
uint32_t HostEsp::getMinFreeHeap() const { return 96u * 1024u; }
size_t heap_caps_get_largest_free_block(uint32_t) { return 72u * 1024u; }

// --- Display: the frame buffer ------------------------------------------------

void DisplayPanel::begin() {
    panel_ = PanelType::ST7789; // CYD2USB, as in the case
    source_ = PanelSource::Detected;
}

void DisplayPanel::flush(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* pixels) {
    for (int16_t row = 0; row < h; ++row) {
        const int py = y + row;
        if (py < 0 || py >= Screens::H) continue;
        for (int16_t col = 0; col < w; ++col) {
            const int px = x + col;
            if (px >= 0 && px < Screens::W) Screens::frame[py * Screens::W + px] = pixels[row * w + col];
        }
    }
}

// --- Touch: scripted taps -------------------------------------------------------

TouchDriver::TouchDriver() : spi_(VSPI), touch_(0, 0) {}

bool TouchDriver::begin(const TouchCalibration& cal, bool stored) {
    setCalibration(cal, stored);
    return true;
}

void TouchDriver::setCalibration(const TouchCalibration& cal, bool stored) {
    cal_ = cal;
    calibrated_ = stored;
}

bool TouchDriver::readRaw(int16_t*, int16_t*) { return false; }

void TouchDriver::readLvgl(lv_indev_drv_t*, lv_indev_data_t* data) {
    data->point.x = Screens::touch.x;
    data->point.y = Screens::touch.y;
    data->state = Screens::touch.pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

bool TouchDriver::pressed() { return Screens::touch.pressed; }

// --- Status LED and build info --------------------------------------------------

namespace StatusLed {
void begin() {}
void showPage(PageId) {}
void showAll(bool) {}
}

namespace BuildInfo {
const char* version() { return APP_VERSION; }
const char* branch() { return Screens::buildBranch; }
const char* commit() { return Screens::buildCommit; }
const char* time() { return Screens::buildTime; }
}

// --- WiFi: fixed sample state ----------------------------------------------------

void WifiManager::begin(const NetSettings& s, const uint8_t mac[6], SaveFn save) {
    settings_ = s;
    memcpy(mac_, mac, sizeof(mac_));
    save_ = save;
    status_ = Screens::wifiStatus;
}

void WifiManager::apply(const NetSettings& s) { settings_ = s; }
void WifiManager::applyAp(const NetSettings& s) { settings_ = s; }
void WifiManager::applyWebPassword(const char* record) {
    snprintf(settings_.webPassword, sizeof(settings_.webPassword), "%s", record ? record : "");
}

void WifiManager::newApPassword(char out[NetConfig::AP_PASSWORD_LEN + 1]) {
    snprintf(out, NetConfig::AP_PASSWORD_LEN + 1, "%s", "Demo-Pass-26");
}

void WifiManager::randomBytes(uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; ++i) out[i] = (uint8_t)(i * 37u + 11u);
}

void WifiManager::update() {}

// The scan page shows the sample networks right away.
void WifiManager::startScan() {
    scanList_.clear();
    scanList_.add("Lab", 3, -48, true);
    scanList_.add("Workshop", 8, -61, true);
    scanList_.add("Guest", 5, -74, false);
    uiScan_ = ScanState::Done;
}
