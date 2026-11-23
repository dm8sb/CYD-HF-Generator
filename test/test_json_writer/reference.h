// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Reference for test_json_writer (AF54): the ArduinoJson implementation of the state JSON and
// the error bodies as rest_json.cpp had it before the fixed-buffer writer, generated from git
// (e5e7344) and kept unchanged; the firmware output must stay byte-identical to it.
#pragma once

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ArduinoJson.h>
#include <config.h>
#include <constants.h>
#include <remote/rest_json.h>
#include <utils/frequency_utils.h>
#include <utils/ipv4.h>
#include <utils/sanitize.h>
#include <utils/status_text.h>

namespace Reference {

using namespace RestJson;

namespace {
const char* const TUNING_NAMES[] = {"fractional", "lownoise", "integer"};
const char* const SPREAD_NAMES[] = {"off", "down05", "down15", "center05"};
const char* const OFFSTATE_NAMES[] = {"low", "high", "hiz"};
template <size_t N> constexpr uint8_t countOf(const char* const (&)[N]) { return (uint8_t)N; }
constexpr uint8_t TUNING_COUNT = countOf(TUNING_NAMES);
constexpr uint8_t SPREAD_COUNT = countOf(SPREAD_NAMES);
constexpr uint8_t OFFSTATE_COUNT = countOf(OFFSTATE_NAMES);

const char* linkName(LinkState l) {
    switch (l) {
        case LinkState::Connected: return "CONN";
        case LinkState::Blocked: return "BLOC";
        case LinkState::UnknownDevice: return "UNKN";
        case LinkState::NotFound: break;
    }
    return "NFO";
}

const char* saveName(SaveState s) {
    switch (s) {
        case SaveState::Pending: return "pending";
        case SaveState::Failed: return "failed";
        case SaveState::Unavailable: return "unavailable";
        case SaveState::Saved: break;
    }
    return "saved";
}

const char* remoteName(Source s) {
    switch (s) {
        case Source::Web: return "web";
        case Source::Scpi: return "scpi";
        case Source::Usb: return "usb";
        case Source::Touch:
        case Source::Internal: break;
    }
    return nullptr;
}

const char* channelStatusName(ChannelStatus s) {
    switch (s) {
        case ChannelStatus::On: return "ON";
        case ChannelStatus::Error: return "ERR";
        case ChannelStatus::NoDevice: return "NOHW";
        case ChannelStatus::FreqTooLow:
        case ChannelStatus::FreqTooHigh: return "WAIT";
        case ChannelStatus::Off: break;
    }
    return "OFF";
}

const char* ch3StatusName(Ch3Status s) {
    switch (s) {
        case Ch3Status::Active: return "ON";
        case Ch3Status::MasterOff:
        case Ch3Status::FreqTooLow:
        case Ch3Status::FreqTooHigh: return "WAIT";
        case Ch3Status::Error: return "ERR";
        case Ch3Status::NoDevice: return "NOHW";
        case Ch3Status::Off: break;
    }
    return "OFF";
}

const char* wifiModeName(WifiMode m) {
    switch (m) {
        case WifiMode::Client: return "client";
        case WifiMode::AccessPoint: return "ap";
        case WifiMode::Off: break;
    }
    return "off";
}

const char* nameAt(const char* const* names, uint8_t count, uint8_t i) { return names[i < count ? i : 0]; }

// Like FREQ:ACT? (0.1 Hz); 0.0 for anything not finite.
void actualText(double hz, char* out, size_t outSize) {
    if (!(hz >= 0.0 && hz < 1e12)) hz = 0.0;
    snprintf(out, outSize, "%.1f", hz);
}

// Foreign text (SSID, names) through Sanitize (W-S9); the source is a char[33] or NUL-terminated.
void clean(const char* in, size_t maxIn, char* out, size_t outSize) {
    size_t n = 0;
    while (in && n < maxIn && in[n]) ++n;
    Sanitize::text(in, n, out, outSize, 32);
}

// The mode line entries as an array (StatusText::modeSummary separates them by " | ").
void addModes(JsonArray modes, const SystemSettings& sys, Spread spread) {
    char buf[64];
    StatusText::modeSummary(sys.tuning_mode, spread, sys.drive_ma, sys.off_state, buf, sizeof(buf));
    char* p = buf;
    while (*p) {
        char* sep = strstr(p, " | ");
        if (sep) *sep = '\0';
        modes.add(p);
        if (!sep) break;
        p = sep + 3;
    }
}


} // namespace

size_t errorJson(const Reply& r, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    char text[80];
    snprintf(text, sizeof(text), "%d,%s", (int)r.code, r.info ? r.info : errorText(r.code));
    JsonDocument doc;
    doc["error"] = text;
    if (doc.overflowed() || measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}


size_t buildState(const Snapshot& s, const DeviceInfo& d, uint32_t nowMs, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    const SystemSettings& sys = s.state.sys;
    JsonDocument doc;
    doc["rev"] = s.revision;
    doc["hw"] = linkName(s.link);
    doc["save"] = saveName(s.saveState);
    const char* remote = remoteName(s.lastRemote);
    if (remote && nowMs - s.lastRemoteMs < REMOTE_INDICATOR_MS) doc["remote"] = remote;
    else doc["remote"] = nullptr;

    // Raw numbers must stay alive until serializeJson().
    char actual[3][24];
    JsonArray chs = doc["ch"].to<JsonArray>();
    for (uint8_t n = 1; n <= 2; ++n) {
        const Channel& c = n == 1 ? s.state.ch1 : s.state.ch2;
        JsonObject o = chs.add<JsonObject>();
        o["n"] = (unsigned)n;
        o["enabled"] = c.enabled;
        o["frequency"] = c.frequency;
        actualText(s.actualHz[n - 1], actual[n - 1], sizeof(actual[0]));
        o["actual"] = serialized(actual[n - 1]);
        o["status"] = channelStatusName(s.status[n - 1]);
        // Spread spectrum is on PLL A: CH1 only (AppController::spreadOf).
        addModes(o["modes"].to<JsonArray>(), sys, n == 1 ? sys.spread : Spread::Off);
        if (n == 2) {
            o["coupled"] = s.state.ch2c.coupled;
            o["offset"] = s.state.ch2c.offset;
        }
    }

    JsonObject ch3 = doc["ch3"].to<JsonObject>();
    ch3["enabled"] = s.state.ch3.enabled;
    ch3["master"] = (unsigned)s.state.ch3.master;
    ch3["phase"] = (unsigned)s.state.ch3.phase;
    ch3["status"] = ch3StatusName(s.ch3Status);
    ch3["frequency"] = s.state.ch3.master == 2 ? s.state.ch2.frequency : s.state.ch1.frequency;
    actualText(s.actualHz[2], actual[2], sizeof(actual[2]));
    ch3["actual"] = serialized(actual[2]);
    ch3["iqMin"] = s.iqMinHz;
    ch3["iqMax"] = s.iqMaxHz;

    // Fixed limits of the firmware (AF48): the web UI takes range and step list from here.
    JsonObject limits = doc["limits"].to<JsonObject>();
    limits["fmin"] = FREQ_MIN_HZ;
    limits["fmax"] = FREQ_MAX_HZ;
    JsonArray steps = limits["steps"].to<JsonArray>();
    for (uint8_t i = 0; i < DELTA_STEP_COUNT; ++i) steps.add(DELTA_STEPS[i]);
    limits["offsetMax"] = CH2_OFFSET_MAX_HZ;

    JsonObject cfg = doc["config"].to<JsonObject>();
    cfg["drive"] = (unsigned)sys.drive_ma;
    cfg["tuning"] = nameAt(TUNING_NAMES, TUNING_COUNT, (uint8_t)sys.tuning_mode);
    cfg["spread"] = nameAt(SPREAD_NAMES, SPREAD_COUNT, (uint8_t)sys.spread);
    cfg["offState"] = nameAt(OFFSTATE_NAMES, OFFSTATE_COUNT, (uint8_t)sys.off_state);
    cfg["correction"] = sys.freq_correction;
    cfg["step"] = sys.delta_step;
    cfg["backlight"] = (unsigned)sys.backlight;
    cfg["rotation"] = (unsigned)sys.rotation;
    cfg["remoteOutputs"] = s.remoteOutputsAllowed ? "allowed" : "local";

    char ssid[33], name[33], apSsid[33], version[33], ip[Ipv4::TEXT_SIZE];
    clean(d.ssid, sizeof(d.ssid), ssid, sizeof(ssid));
    clean(d.name, sizeof(d.name), name, sizeof(name));
    clean(d.apSsid, sizeof(d.apSsid), apSsid, sizeof(apSsid));
    clean(d.version ? d.version : APP_VERSION, 32, version, sizeof(version));
    ip[0] = '\0';
    if (d.ip) Ipv4::format(d.ip, ip, sizeof(ip));
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["mode"] = wifiModeName(d.mode);
    wifi["connected"] = d.connected;
    wifi["ssid"] = ssid;
    wifi["rssi"] = d.rssi;
    wifi["ip"] = ip;
    wifi["name"] = name;
    JsonObject ap = wifi["ap"].to<JsonObject>();
    ap["active"] = d.apActive;
    ap["ssid"] = apSsid;
    ap["open"] = d.apOpen;
    ap["stations"] = (unsigned)d.apStations;
    wifi["confirmWithin"] = (unsigned)d.confirmWithin;

    doc["device"]["version"] = version;
    doc["device"]["passwordSet"] = d.passwordSet;

    if (doc.overflowed()) return 0;
    if (measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}


size_t loginLockedJson(uint32_t retryAfterS, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    JsonDocument doc;
    doc["error"] = "-200,Too many failed logins";
    doc["retryAfter"] = retryAfterS;
    if (doc.overflowed() || measureJson(doc) >= outSize) return 0;
    return serializeJson(doc, out, outSize);
}

}
