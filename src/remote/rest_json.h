#pragma once

#include <stddef.h>
#include <stdint.h>
#include <net/net_settings.h>
#include "command.h"

// REST JSON mapping (ARBEITSPLAN 3.6, W-S8, W-S19, W-S21): request bodies to Commands
// (Source::Web), Result/PushResult to an HTTP status with {"error":"<code>,<text>"} using the
// SCPI codes; <text> is the detail when there is one (the web UI shows it as it is), else the
// standard SCPI text ("-222,Data out of range"), and the state JSON for
// GET /api/state and the WebSocket push. ArduinoJson 7 is used only inside rest_json.cpp.
// Pure, host-tested; nothing is executed here.
//
// Bodies are checked strictly before ArduinoJson sees them (it would accept unquoted keys,
// single quotes and trailing data, and keeps the last of duplicate keys): RFC 8259 grammar,
// one value, nothing after it, nesting <= MAX_NESTING, else -102. Then:
//   top level not an object, wrong JSON type (string for a number, 4.5, 4.0, 1e3, true as
//   number, 1 as bool, null, arrays, objects)                     -104 Data type error
//   unknown key, frequency together with frequencyText            -108 Parameter not allowed
//   duplicate key                                                 -102 Syntax error
//   no key at all where one is needed                             -109 Missing parameter
//   integer that does not fit int64 (or any float >= 9.2e18)      -222 Data out of range
//   unknown enum text, frequencyText not parseable or outside
//   8 kHz-160 MHz (parseRemoteFrequency tells no difference), NUL in a string
//                                                                 -224 Illegal parameter value
//   remoteOutputs in /api/config (only on the device, W-E25)      403, -221
// Numbers are passed on in the full type (int64); ranges and allowed values are the
// controller's (Result, as with SCPI): OutOfRange -222, Invalid -224, Conflict -221.
// Several keys give several Commands in a fixed order (independent of the key order), so a
// body applies like the device would: channel frequency before enabled; CH3 master and phase
// before enabled; config tuning before spread (spread in Integer is a conflict), then off
// state, drive, correction, step, backlight. Not atomic: the server stops at the first
// refused push and answers with the first error.
namespace RestJson {

static constexpr size_t MAX_BODY = 1024;   // W-S10; the guard refuses larger with 413
static constexpr uint8_t MAX_NESTING = 10; // ARBEITSPLAN 3.3
static constexpr uint8_t MAX_COMMANDS = 8;
static constexpr size_t STATE_SIZE = 1024;  // enough for buildState() (the state is ~750 B)
static constexpr size_t ERROR_SIZE = 96;    // enough for errorJson()

// SCPI error codes of the REST API.
static constexpr int16_t SYNTAX_ERROR = -102;
static constexpr int16_t DATA_TYPE_ERROR = -104;
static constexpr int16_t PARAMETER_NOT_ALLOWED = -108;
static constexpr int16_t MISSING_PARAMETER = -109;
static constexpr int16_t EXECUTION_ERROR = -200;
static constexpr int16_t SETTINGS_CONFLICT = -221;
static constexpr int16_t DATA_OUT_OF_RANGE = -222;
static constexpr int16_t ILLEGAL_PARAMETER_VALUE = -224;
static constexpr int16_t HARDWARE_ERROR = -240;

struct Reply {
    uint16_t status;  // HTTP status, 200 = success
    int16_t code;     // SCPI error code, 0 on success
    const char* info; // detail after ';' (static text) or nullptr
    bool ok() const { return status == 200; }
};

struct Commands {
    Command list[MAX_COMMANDS];
    uint8_t count;
};

// POST /api/channel/<1|2>: {"enabled":bool,"frequency":int,"frequencyText":string}.
Reply parseChannel(uint8_t channel, const char* body, size_t len, Commands* out);
// POST /api/ch3: {"enabled":bool,"master":int,"phase":int}.
Reply parseCh3(const char* body, size_t len, Commands* out);
// POST /api/config: {"drive":int,"tuning":"fractional|lownoise|integer",
// "spread":"off|down05|down15|center05","offState":"low|high|hiz","correction":int (0.01 ppm),
// "step":int,"backlight":int}.
Reply parseConfig(const char* body, size_t len, Commands* out);
// POST /api/save (SaveNow) and /api/identify (Identify): the body is {}.
Reply parseAction(CommandType type, const char* body, size_t len, Commands* out);
// POST /api/login: {"password":string}, copied into password (NUL-terminated); longer than
// passwordSize - 1 bytes: -222.
Reply parseLogin(const char* body, size_t len, char* password, size_t passwordSize, size_t* passwordLen);
// W-S1: the body is JSON with a key (any depth) that names a password or key ("pass", "psk",
// "secret", "key" in any case); for RequestGuard::checkSecrets(). false for invalid JSON (the
// endpoint refuses it anyway).
bool carriesSecret(const char* body, size_t len);
// "/api/channel/1" -> 1, "/api/channel/2" -> 2, anything else 0 (404).
uint8_t channelFromPath(const char* path);

// Queue and controller outcome. Queued/Ok/NoChange: 200. Full: 503 (-200 "Busy, try again",
// the queue drains within one loop()). RateLimited: 429 (-200 "Too many commands", W-S11).
// OutOfRange 400/-222, Invalid 400/-224, Conflict 400/-221 (spread spectrum in Integer),
// Denied 403/-221 "Remote outputs: local only" (W-S21), HwError 500/-240 "SI5351 setup failed".
Reply fromPush(PushResult p);
Reply fromResult(Result r);
Reply fromOutcome(PushResult p, Result r);
// Standard SCPI text of a code ("Data out of range"), "" for 0.
const char* errorText(int16_t code);
// {"error":"-221,Remote outputs: local only"}; 0 if it does not fit.
size_t errorJson(const Reply& r, char* out, size_t outSize);

// WiFi state and device info for the state JSON, filled by the server from the WiFi manager
// (no dependency on it here). Strings are raw bytes as received; buildState() sanitizes them.
struct DeviceInfo {
    WifiMode mode;        // wifi.mode: off / client / ap
    bool connected;       // client connected
    char ssid[33];        // the connected network only (S-26), "" without connection
    int32_t rssi;         // dBm, 0 without connection
    uint32_t ip;          // address to show (client, else AP), 0 = none (as in NetSettings)
    char name[33];        // device name
    bool apActive;
    char apSsid[33];
    bool apOpen;
    uint8_t apStations;
    const char* version;  // nullptr: APP_VERSION
};

// The state JSON (contract in ARBEITSPLAN S55): rev, hw, save, remote (the last remote source
// while within REMOTE_INDICATOR_MS of nowMs, else null), ch[2], ch3, config, wifi, device.
// modes: the entries of the device's mode line for this channel (StatusText::modeSummary:
// "OFF=High", "OFF=High-Z", "SSC -0.5%", "SSC -1.5%", "SSC +/-0.5%", "Low noise", "Integer",
// "2mA"/"6mA"/"8mA"), [] with the defaults. actual with 0.1 Hz. Returns the length, 0 if it
// does not fit (out is "" then).
size_t buildState(const Snapshot& s, const DeviceInfo& d, uint32_t nowMs, char* out, size_t outSize);

}
