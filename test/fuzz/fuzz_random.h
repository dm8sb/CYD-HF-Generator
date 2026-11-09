#pragma once

// Pseudo-random inputs with a fixed seed for the standalone driver and test_fuzz_random
// (W-S25: no libFuzzer, also on Windows/MinGW). Not coverage guided: inputs are mutations of
// the seeds or sequences of tokens that matter to the parsers, mixed with random bytes.

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

// xorshift64*: the same sequence on every platform and compiler.
struct FuzzRng {
    uint64_t s;
    explicit FuzzRng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }
    uint32_t below(uint32_t n) { return n ? (uint32_t)((next() >> 16) % n) : 0; }
};

inline const std::vector<std::string>& fuzzTokens() {
    static const char* const raw[] = {
        // framing and SCPI syntax
        "\n", "\r\n", ";", ";:", ":", "?", " ", "\t", ",", ".", "\"", "'", "\"\"", "#H", "#B", "#Q", "#",
        "*IDN?", "*RST", "*CLS", "*OPC?", "*TST?", "*ESE 255", "*SRE?", "*WAI", "OUTP", "OUTP2", "OUTP3",
        ":STAT", " ON", " OFF", "OUTP1:COND?", "OUTP:DRIV", "OUTP:OFFS HIZ", "OUTP:REM?", "FREQ", "FREQ?",
        ":CW", "FREQ:ACT?", "SOUR", "SOUR1:", "SOUR2:", "SOUR3:", "SOUR4:", "SOUR0:", "IQ:MAST", "IQ:RANG?",
        "PHAS", "SSC", " DOWN05", " CENT05", "SYST:TUN", " LNO", " INT", "SYST:CORR", "SYST:STEP",
        "SYST:BACK", "SYST:SI5351?", "SYST:HEAP?", "SYST:IDEN", "SYST:LOG?", "SYST:SAVE", "SYST:ERR?",
        "SYST:ERR:COUN?", "SYST:VERS?", "SOURce2:FREQuency:CW",
        // numbers and units
        "0", "1", "9", "-", "+", "E", "e", "E-", "E+", "0.5", "7.1", "455", "10000000", "160000000",
        "999999999999999999", "9223372036854775807", "9223372036854775808", "1E18", "1E19", "1E-19",
        "0.0000000000000000005", "E9999", " MHZ", " KHZ", " HZ", "mhz", "kHz", "Hz", "k", "M", " DEG",
        " PPM", " PCT", " MIN", " MAX", " DEF", " MAXimum", " NAN", " INF", " -INF", "#HFF", "#B101",
        "1 000 000", "7 100 000", "10,7", ",",
        // HTTP-like lines (W-S18)
        "GET / HTTP/1.1", "POST /api HTTP/1.0", "Host: ", "X-A: b", "HTTP/1.",
        // network text
        "192.168.0.1", "255.255.255.0", "0.0.0.0", "256", "010", "WIFI:", "\\", "S:", "P:", ";;",
        // bytes that sanitize and the framer must reject or map
        "\x01", "\x1b", "\x7f", "\x80", "\xbf", "\xc0\x80", "\xc2\x85",
        "\xc3\xa4", "\xc3", "\xe2\x82\xac", "\xe2\x82", "\xed\xa0\x80", "\xef\xbf\xbd", "\xf0\x9f\x98\x80",
        "\xf0\x9f", "\xf4\x90\x80\x80", "\xf8\x88\x80\x80\x80", "\xff",
        // JSON bodies (rest_json)
        "{", "}", "[", "]", "true", "false", "null", "\\u0000", "\\ud800", "\\udc00", "\\\"", "\"password\"",
        "\"enabled\"", "\"frequency\"", "\"frequencyText\"", "\"master\"", "\"phase\"", "\"drive\"", "\"tuning\"",
        "\"spread\"", "\"offState\"", "\"remoteOutputs\"", "\"lownoise\"", "\"center05\"", "\"hiz\"", "\"psk\":",
        "1e19", "-0", "4.0",
        // HTTP requests and the guard script (request_guard)
        "\r\n\r\n", "Host: 192.168.1.50", "Host: cyd-hf-1a2b.local", "Cookie: cyd_sid=$S0", "Origin: http://",
        "Sec-Fetch-Site: cross-site", "Content-Type: application/json", "Content-Length: 2", "X-CYD-Token: $T",
        "Upgrade: websocket", "Authorization: Basic ", "Transfer-Encoding: chunked", "!ok\n", "!bad\n",
        "!wait 30000\n", "!logout cyd_sid=$S0\n", "/api/", "/ws?t=$T", "[::1]", ".local", "$S1",
    };
    static std::vector<std::string> tokens;
    if (tokens.empty()) {
        for (size_t i = 0; i < sizeof(raw) / sizeof(raw[0]); ++i) tokens.push_back(raw[i]);
        tokens.push_back(std::string(1, '\0')); // NUL cannot be written in the list above
    }
    return tokens;
}

inline uint8_t fuzzRandomByte(FuzzRng& rng) {
    // Mostly printable ASCII, sometimes anything.
    return rng.below(4) ? (uint8_t)(0x20 + rng.below(0x5F)) : (uint8_t)rng.below(256);
}

// One input of at most maxLen bytes.
inline void fuzzMakeInput(FuzzRng& rng, const std::vector<std::string>& seeds, size_t maxLen, std::string& out) {
    const std::vector<std::string>& tokens = fuzzTokens();
    out.clear();
    const uint32_t kind = rng.below(10);
    if (kind < 5 && !seeds.empty()) {
        // Mutate a seed (or a splice of two).
        out = seeds[rng.below((uint32_t)seeds.size())];
        if (rng.below(4) == 0) out += seeds[rng.below((uint32_t)seeds.size())];
        const uint32_t edits = 1 + rng.below(8);
        for (uint32_t e = 0; e < edits; ++e) {
            const size_t pos = out.empty() ? 0 : rng.below((uint32_t)out.size() + 1);
            switch (rng.below(7)) {
                case 0: if (pos < out.size()) out[pos] = (char)(out[pos] ^ (1 << rng.below(8))); break;
                case 1: out.insert(pos, 1, (char)fuzzRandomByte(rng)); break;
                case 2: if (pos < out.size()) out.erase(pos, 1 + rng.below(4)); break;
                case 3: out.insert(pos, tokens[rng.below((uint32_t)tokens.size())]); break;
                case 4: {
                    if (out.empty()) break;
                    const size_t from = rng.below((uint32_t)out.size());
                    const size_t len = 1 + rng.below(16);
                    out.insert(pos, out.substr(from, len));
                    break;
                }
                case 5: if (pos < out.size()) out[pos] = (char)fuzzRandomByte(rng); break;
                default: out.insert(pos, std::string(1 + rng.below(300), (char)fuzzRandomByte(rng))); break;
            }
        }
    } else if (kind < 9) {
        // Tokens and random bytes.
        const uint32_t parts = 1 + rng.below(40);
        for (uint32_t p = 0; p < parts; ++p) {
            if (rng.below(3)) out += tokens[rng.below((uint32_t)tokens.size())];
            else out += (char)fuzzRandomByte(rng);
        }
    } else {
        // Plain random bytes.
        const size_t len = rng.below((uint32_t)maxLen + 1);
        for (size_t i = 0; i < len; ++i) out += (char)rng.below(256);
    }
    if (out.size() > maxLen) out.resize(maxLen);
}
