// Host screen renderer: boots the AppController on the fakes like main.cpp, sets the sample
// state of the documentation (sample networks and passwords only), shows the pages with the
// real GUI code and writes each frame as raw RGB565 (<out>/<name>.rgb565, 320 x 240, little
// endian). tools/render_images.py turns them into PNG files.
#include "screens.h"
#include <Arduino.h>
#include <Wire.h>
#include <core/app_controller.h>
#include <drivers/backlight.h>
#include <drivers/display_panel.h>
#include <drivers/si5351_manager.h>
#include <drivers/touch.h>
#include <gui/gui.h>
#include <hal/i2c_bus.h>
#include <net/net_settings.h>
#include <net/wifi_manager.h>
#include <services/settings.h>
#include <stdio.h>
#include <string.h>

static AppController* gApp = nullptr;
static Gui* gGui = nullptr;
static const char* gOut = ".";
static bool gNetConfirm = false;
static int gShots = 0;

static uint32_t ip4(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d;
}

// The main loop for ms milliseconds (5 ms per pass like loop()).
static void run(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 5) {
        Fake::advanceMs(5);
        lv_timer_handler();
        gApp->update();
        gGui->update();
    }
}

static void tap(int16_t x, int16_t y) {
    Screens::touch = {true, x, y};
    run(100);
    Screens::touch.pressed = false;
    run(400);
}

static void shot(const char* name) {
    run(300);
    lv_obj_invalidate(lv_scr_act());
    lv_obj_invalidate(lv_layer_top());
    lv_refr_now(nullptr);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.rgb565", gOut, name);
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(2);
    }
    for (int i = 0; i < Screens::W * Screens::H; ++i) {
        const uint16_t p = Screens::frame[i];
        fputc(p & 0xFF, f);
        fputc(p >> 8, f);
    }
    fclose(f);
    ++gShots;
    printf("%s\n", path);
}

static void show(PageId page, const char* name) {
    gGui->navigate(page);
    shot(name);
}

static bool netConfirmNeeded() { return gNetConfirm; }
static void netConfirmAnswer(bool) { gNetConfirm = false; }
static uint8_t webClients() { return 1; }
static uint32_t scpiClient() { return ip4(192, 168, 1, 20); }
static void disconnectAll() {}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--out") && i + 1 < argc) gOut = argv[++i];
        else if (!strcmp(argv[i], "--branch") && i + 1 < argc) Screens::buildBranch = argv[++i];
        else if (!strcmp(argv[i], "--commit") && i + 1 < argc) Screens::buildCommit = argv[++i];
        else if (!strcmp(argv[i], "--time") && i + 1 < argc) Screens::buildTime = argv[++i];
    }

    // Boot like main.cpp, on the SI5351 model of test/fakes/Wire.h.
    static SettingsService settings;
    static SI5351Manager si5351;
    static BacklightDriver backlight;
    static AppController app(settings, si5351, backlight);
    gApp = &app;
    I2CBus::begin();
    settings.begin();
    PersistentState loaded;
    settings.load(loaded);
    app.begin(loaded);
    backlight.begin();
    app.connectHardware();
    backlight.setPercent(loaded.sys.backlight);
    app.applyStoredChannelSettings();

    // Sample network settings: invented names and passwords, a locally administered MAC.
    const uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x12, 0x34};
    static NetSettings net;
    NetConfig::defaults(net, mac);
    net.mode = WifiMode::Client;
    NetConfig::setNetwork(net, 0, "Lab", "sample-key-lab");
    NetConfig::setNetwork(net, 1, "Workshop", "sample-key-workshop");
    snprintf(net.apPassword, sizeof(net.apPassword), "%s", "Demo-Pass-26");
    WifiManager::Status& w = Screens::wifiStatus;
    w.mode = WifiMode::Client;
    w.staConnected = true;
    snprintf(w.ssid, sizeof(w.ssid), "%s", "Lab");
    w.rssi = -52;
    w.staIp = ip4(192, 168, 1, 50);
    w.staMask = ip4(255, 255, 255, 0);
    w.staGateway = ip4(192, 168, 1, 1);
    w.staDns = ip4(192, 168, 1, 1);
    w.apIp = ip4(192, 168, 4, 1);
    NetConfig::apSsid(mac, w.apSsid, sizeof(w.apSsid));
    static WifiManager wifi;
    wifi.begin(net, mac, nullptr);

    static TouchDriver touch;
    touch.begin(TouchCalibration {312, 3768, 285, 3790}, true);
    static DisplayPanel display;
    display.begin();

    static Gui gui;
    gGui = &gui;
    gui.begin(app, touch, display);
    gui.setWifi(wifi, nullptr);
    gui.setNetConfirm(netConfirmNeeded, netConfirmAnswer);
    gui.setRemoteClients(webClients, scpiClient, disconnectAll);
    app.addListener(Gui::onAppEvent, &gui);

    // Documentation state: CH1 7.1 MHz, CH2 14 MHz, CH3 IQ +90 deg to CH1, all on.
    app.setChannelFrequency(1, 7100000ULL);
    app.setChannelFrequency(2, 14000000ULL);
    app.setCH3Master(1);
    app.setCH3Phase(90);
    app.setChannelEnabled(1, true);
    app.setChannelEnabled(2, true);
    app.setChannelEnabled(3, true);
    run(6000); // autosave: "Saved"

    show(PageId::Main, "main");
    show(PageId::CH1, "ch1");
    // Numpad from CH1 with a typed entry (keys 1 0 . 1 2 5), left without confirming.
    gui.navigate(PageId::Numpad);
    run(300);
    static const int16_t COL[4] = {43, 121, 199, 277};
    static const int16_t ROW[4] = {84, 128, 172, 215};
    const int16_t keys[][2] = {{0, 2}, {1, 3}, {0, 3}, {0, 2}, {1, 2}, {1, 1}};
    for (const auto& k : keys) tap(COL[k[0]], ROW[k[1]]);
    shot("numpad");
    show(PageId::CH3, "ch3");
    show(PageId::Config, "config");
    // CONFIG scrolled to the end: the info card.
    lv_obj_t* list = lv_obj_get_child(lv_scr_act(), 1);
    lv_obj_scroll_by(list, 0, -lv_obj_get_scroll_bottom(list), LV_ANIM_OFF);
    shot("config-info");
    show(PageId::Wifi, "wifi");
    wifi.startScan();
    show(PageId::WifiScan, "wifi-scan");
    show(PageId::WifiDevice, "wifi-device");
    show(PageId::WifiAp, "wifi-ap");
    // Network change from the web without web password: the touch confirmation (W-S12).
    gui.navigate(PageId::Main);
    gNetConfirm = true;
    shot("dialog-network-change");

    printf("%d screens\n", gShots);
    return gShots == 11 ? 0 : 1;
}
