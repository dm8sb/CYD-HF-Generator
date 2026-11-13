// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Strict JSON helpers of rest_json.cpp for the other REST mappings (rest_wifi.cpp). Only for
// .cpp files of remote/: it pulls in ArduinoJson.
#include <ArduinoJson.h>
#include "rest_json.h"

namespace RestJson {
namespace detail {

// A request body: RFC 8259 grammar, an object, no duplicate keys (see rest_json.h).
Reply load(const char* body, size_t len, JsonDocument& doc);
bool keyIs(JsonString key, const char* name);
// A JSON integer in the full type (-104 for 4.5, "4", true, null).
Reply integer(JsonVariantConst v, int64_t* out);
// A JSON string without NUL (-104 other types, -224 NUL).
Reply text(JsonVariantConst v, JsonString* out);

}
}
