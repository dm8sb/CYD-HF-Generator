// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target scan_list: ScanList::add (W-S9, scan list of the WIFI page).
// Input: records [header][rssi][ssid bytes]; header bits 0-5 = SSID length (0..63, 0 = hidden),
// bit 6 = secure, bit 7 = clear() first. SSIDs are cut at the first NUL, because the WiFi driver
// hands them over as C strings (wifi_manager.cpp: ssid.c_str(), ssid.length()).
// Invariants, against a model (strongest RSSI per SSID): at most MAX entries, sorted by RSSI,
// one entry per SSID with the strongest RSSI seen for it, the RSSI values are exactly the
// strongest MAX of the model; ssid is the input cut to 32 bytes, shown is Sanitize::text of it
// (valid UTF-8 without control characters, <= 32 bytes); hidden networks never appear.

#include "fuzz_common.h"

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <net/scan_list.h>
#include <utils/sanitize.h>

namespace fuzz_scan_list {

void check(const ScanList& list, const std::map<std::string, int8_t>& model) {
    FUZZ_CHECK(list.count() <= ScanList::MAX);
    std::vector<int> strongest;
    for (std::map<std::string, int8_t>::const_iterator it = model.begin(); it != model.end(); ++it) {
        strongest.push_back(it->second);
    }
    std::sort(strongest.begin(), strongest.end(), std::greater<int>());
    if (strongest.size() > ScanList::MAX) strongest.resize(ScanList::MAX);
    FUZZ_CHECK(list.count() == strongest.size());
    for (uint8_t i = 0; i < list.count(); ++i) {
        const ScanEntry& e = list.at(i);
        FUZZ_CHECK(memchr(e.ssid, 0, sizeof(e.ssid)) && memchr(e.shown, 0, sizeof(e.shown)));
        const std::string ssid = e.ssid;
        FUZZ_CHECK(!ssid.empty() && ssid.size() <= 32);
        FUZZ_CHECK(e.rssi == strongest[i]);
        if (i > 0) FUZZ_CHECK(list.at(i - 1).rssi >= e.rssi);
        std::map<std::string, int8_t>::const_iterator m = model.find(ssid);
        FUZZ_CHECK(m != model.end() && m->second == e.rssi);
        for (uint8_t k = 0; k < i; ++k) FUZZ_CHECK(ssid != list.at(k).ssid);
        const size_t shownLen = strlen(e.shown);
        FUZZ_CHECK(shownLen <= 32 && fuzzValidUtf8(e.shown, shownLen, true));
        char expected[33];
        Sanitize::text(ssid.data(), ssid.size(), expected, sizeof(expected));
        FUZZ_CHECK(strcmp(expected, e.shown) == 0);
    }
}

int run(const uint8_t* data, size_t size) {
    ScanList list;
    std::map<std::string, int8_t> model;
    size_t i = 0;
    while (i + 2 <= size) {
        const uint8_t header = data[i];
        const int8_t rssi = (int8_t)data[i + 1];
        i += 2;
        size_t len = header & 0x3F;
        if (len > size - i) len = size - i;
        // An exact-size copy (ASan), cut at NUL like a C string.
        std::vector<char> ssid(data + i, data + i + len);
        i += len;
        size_t cLen = 0;
        while (cLen < ssid.size() && ssid[cLen] != '\0') ++cLen;
        if (header & 0x80) {
            list.clear();
            model.clear();
        }
        list.add(ssid.empty() ? "" : ssid.data(), cLen, rssi, (header & 0x40) != 0);
        if (cLen > 0) {
            const std::string key(ssid.data(), cLen > 32 ? 32 : cLen);
            std::map<std::string, int8_t>::iterator m = model.find(key);
            if (m == model.end()) model[key] = rssi;
            else if (rssi > m->second) m->second = rssi;
        }
        check(list, model);
    }
    list.add(nullptr, 5, 0, false); // ignored
    check(list, model);
    return 0;
}

} // namespace fuzz_scan_list

FUZZ_TARGET(scan_list, fuzz_scan_list::run)
