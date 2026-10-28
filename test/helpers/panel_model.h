#pragma once

// Display controller model for the display detection tests. Read behavior as measured on
// both boards on 2026-10-28 (diagnostic sketch, SPI 1 MHz, MISO without pull):
// - 0Bh (RDDMADCTL): the MADCTL value without dummy bit, then the line is released.
// - 04h (RDDID): one dummy bit, ID1..ID3, then the line is released.
// - A released line keeps its last level (no pull on GPIO12).
// - ST7789: C1h (IDSET in the ILI9341 init) changes ID1; SWRESET restores it.

#include <drivers/display_detect.h>
#include <string.h>
#include <vector>

namespace PanelModel {

enum class Kind : uint8_t { Ili9341, St7789, Floating, StuckLow, StuckHigh, Noisy };

struct Write {
    uint8_t cmd;
    std::vector<uint8_t> data;
};

class Panel : public PanelBus {
public:
    Kind kind;
    uint32_t id;          // ST7789 ID1..ID3
    uint8_t dummyBits0B;  // 0 as measured; 1 for a panel with a dummy bit
    uint8_t madctl = 0;
    uint8_t id1Override = 0; // != 0: ID1 after IDSET
    bool lastBit = false;
    unsigned noise = 1;
    // A different ID on every RDDID read (noise during 04h only).
    bool unstableId = false;
    unsigned rddidReads = 0;
    std::vector<Write> writes;
    std::vector<uint8_t> reads;
    uint32_t delayTotal = 0;
    // delay after each write, in write order
    std::vector<uint32_t> delayAfter;

    explicit Panel(Kind k, uint32_t stId = DisplayDetect::ID_CYD2USB, uint8_t dummy0B = 0)
        : kind(k), id(stId), dummyBits0B(dummy0B) {}

    void write(uint8_t cmd, const uint8_t* data, uint8_t len) override {
        writes.push_back(Write {cmd, std::vector<uint8_t>(data, data + len)});
        delayAfter.push_back(0);
        if (cmd == 0x01) {
            madctl = 0;
            id1Override = 0;
        }
        if (cmd == 0x36 && len == 1) madctl = data[0];
        if (cmd == 0xC1 && len == 1 && kind == Kind::St7789) id1Override = data[0];
    }
    void read(uint8_t cmd, uint8_t* buf, uint8_t len) override {
        reads.push_back(cmd);
        // Bits the panel drives, MSB first.
        std::vector<bool> bits;
        if (kind == Kind::Ili9341 || kind == Kind::St7789) {
            if (cmd == 0x0B) {
                for (uint8_t i = 0; i < dummyBits0B; ++i) bits.push_back(false);
                pushByte(bits, madctl);
            } else if (cmd == 0x04) {
                uint32_t v = kind == Kind::Ili9341 ? 0 : (id1Override ? ((uint32_t)id1Override << 16) | (id & 0xFFFF) : id);
                if (unstableId) v ^= ++rddidReads;
                bits.push_back(false); // dummy bit
                pushByte(bits, v >> 16);
                pushByte(bits, v >> 8);
                pushByte(bits, v);
            }
        }
        for (uint8_t i = 0; i < len; ++i) {
            uint8_t b = 0;
            for (uint8_t k = 0; k < 8; ++k) {
                const size_t n = (size_t)i * 8 + k;
                bool bit;
                if (kind == Kind::StuckLow) bit = false;
                else if (kind == Kind::StuckHigh) bit = true;
                else if (kind == Kind::Noisy) {
                    noise = noise * 1103515245u + 12345u;
                    bit = (noise >> 16) & 1;
                } else if (n < bits.size()) bit = lastBit = bits[n];
                else bit = lastBit;
                b = (uint8_t)((b << 1) | (bit ? 1 : 0));
            }
            buf[i] = b;
        }
    }
    void delayMs(uint32_t ms) override {
        delayTotal += ms;
        if (!delayAfter.empty()) delayAfter.back() += ms;
    }

    unsigned writesOf(uint8_t cmd) const {
        unsigned n = 0;
        for (const Write& w : writes) n += w.cmd == cmd;
        return n;
    }

private:
    static void pushByte(std::vector<bool>& bits, uint8_t v) {
        for (int k = 7; k >= 0; --k) bits.push_back((v >> k) & 1);
    }
};

} // namespace PanelModel
