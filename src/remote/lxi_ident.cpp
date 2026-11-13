// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lxi_ident.h"

#include <stdio.h>
#include <string.h>

namespace LxiIdent {

namespace {

struct Out {
    char* p;
    size_t cap;
    size_t len;
    bool ok;
    void raw(const char* s) {
        const size_t n = strlen(s);
        if (!ok || n >= cap - len) {
            ok = false;
            return;
        }
        memcpy(p + len, s, n + 1);
        len += n;
    }
    // Text content or attribute value: &, <, >, " and ' escaped, control characters dropped.
    void text(const char* s) {
        for (; s && *s && ok; ++s) {
            const unsigned char c = (unsigned char)*s;
            char one[2] = {(char)c, '\0'};
            switch (c) {
                case '&': raw("&amp;"); break;
                case '<': raw("&lt;"); break;
                case '>': raw("&gt;"); break;
                case '"': raw("&quot;"); break;
                case '\'': raw("&apos;"); break;
                default:
                    if (c >= 0x20 && c != 0x7F) raw(one);
                    break;
            }
        }
    }
    void element(const char* tag, const char* value) {
        raw("<");
        raw(tag);
        raw(">");
        text(value);
        raw("</");
        raw(tag);
        raw(">\n");
    }
};

void quad(uint32_t v, char* out, size_t n) {
    snprintf(out, n, "%u.%u.%u.%u", (unsigned)(v >> 24), (unsigned)((v >> 16) & 0xFF), (unsigned)((v >> 8) & 0xFF),
             (unsigned)(v & 0xFF));
}

} // namespace

size_t build(const Info& info, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    out[0] = '\0';
    Out o {out, outSize, 0, true};
    char ip[16], mask[16], gateway[16], buf[96];
    quad(info.ip, ip, sizeof(ip));
    quad(info.mask, mask, sizeof(mask));
    quad(info.gateway, gateway, sizeof(gateway));
    o.raw("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<LXIDevice xmlns=\"http://www.lxistandard.org/InstrumentIdentification/1.0\" "
          "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
          "xsi:schemaLocation=\"http://www.lxistandard.org/InstrumentIdentification/1.0 "
          "http://www.lxistandard.org/InstrumentIdentification/1.0/LXIIdentification.xsd\">\n");
    o.element("Manufacturer", MANUFACTURER);
    o.element("Model", MODEL);
    o.element("SerialNumber", info.serial ? info.serial : "0");
    o.element("FirmwareRevision", info.version ? info.version : "");
    o.element("ManufacturerDescription", DESCRIPTION);
    o.element("HomepageURL", HOMEPAGE);
    o.element("DriverURL", HOMEPAGE);
    o.element("UserDescription", info.name ? info.name : "");
    snprintf(buf, sizeof(buf), "http://%s/lxi/identification", ip);
    o.element("IdentificationURL", buf);
    o.raw("<Interface xsi:type=\"NetworkInformation\" InterfaceType=\"LXI\" IPType=\"IPv4\" InterfaceName=\"WiFi\">\n");
    snprintf(buf, sizeof(buf), "TCPIP::%s::INSTR", ip);
    o.element("InstrumentAddressString", buf);
    snprintf(buf, sizeof(buf), "TCPIP::%s::%u::SOCKET", ip, (unsigned)info.scpiPort);
    o.element("InstrumentAddressString", buf);
    o.raw("<Hostname>");
    o.text(info.name ? info.name : "");
    o.raw(".local</Hostname>\n");
    o.element("IPAddress", ip);
    o.element("SubnetMask", mask);
    o.element("Gateway", gateway);
    o.element("DHCPEnabled", info.dhcp ? "true" : "false");
    o.element("AutoIPEnabled", "false");
    o.raw("</Interface>\n</LXIDevice>\n");
    return o.ok ? o.len : 0;
}

} // namespace LxiIdent
