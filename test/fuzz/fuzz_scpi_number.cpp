// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

// Fuzz target scpi_number: ScpiNumber::decimal and ScpiNumber::frequency (W-S19).
// Input: byte 0 = scale exponent (-4..4), byte 1 = unit table, rest = the number token.
// Invariants, against an independent reference (grammar check plus long double value):
// - Ok only for text of the grammar; Syntax for grammar text only with > 18 significant digits.
// - Ok: the int64 value is within 1/2 (plus 1e-15 relative) of the exact value, never INT64_MIN.
// - Range only when the exact value is outside int64; Ok whenever it is clearly inside.
// - frequency: Ok exactly when decimal with HZ/KHZ/MHZ is Ok and in FREQ_MIN_HZ..FREQ_MAX_HZ.

#include "fuzz_common.h"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <constants.h>
#include <remote/scpi.h>

namespace fuzz_scpi_number {

using ScpiNumber::Parse;
using ScpiNumber::Unit;

const Unit FREQ[] = {{"HZ", 0}, {"KHZ", 3}, {"MHZ", 6}, {nullptr, 0}};
const Unit DEG[] = {{"DEG", 0}, {nullptr, 0}};
const Unit PPM_PCT[] = {{"PPM", 0}, {"PCT", -2}, {nullptr, 0}};
const Unit* const TABLES[] = {nullptr, FREQ, DEG, PPM_PCT};

struct Reference {
    bool grammar;        // matches [blanks][sign]digits[.digits][E[sign]digits][blanks][unit]
    int significant;     // as ScpiNumber counts them
    long double value;   // exact value approximated (inf when huge)
};

bool digit(char c) { return c >= '0' && c <= '9'; }
bool blank(char c) { return c == ' ' || c == '\t'; }

Reference reference(const std::string& s, int scaleExp, const Unit* units) {
    Reference r {false, 0, 0};
    size_t i = 0;
    while (i < s.size() && blank(s[i])) ++i;
    bool negative = false;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) negative = s[i++] == '-';
    std::string whole, frac, exp;
    while (i < s.size() && digit(s[i])) whole += s[i++];
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && digit(s[i])) frac += s[i++];
    }
    if (whole.empty() && frac.empty()) return r;
    bool expNegative = false;
    if (i < s.size() && (s[i] == 'E' || s[i] == 'e')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) expNegative = s[i++] == '-';
        while (i < s.size() && digit(s[i])) exp += s[i++];
        if (exp.empty()) return r;
    }
    while (i < s.size() && blank(s[i])) ++i;
    int unitExp = 0;
    if (i < s.size()) {
        const std::string rest = s.substr(i);
        bool found = false;
        for (const Unit* u = units; u && u->name; ++u) {
            if (rest.size() != strlen(u->name)) continue;
            bool same = true;
            for (size_t k = 0; k < rest.size(); ++k) {
                const char c = (rest[k] >= 'a' && rest[k] <= 'z') ? (char)(rest[k] - 32) : rest[k];
                if (c != u->name[k]) same = false;
            }
            if (same) {
                unitExp = u->exp10;
                found = true;
                break;
            }
        }
        if (!found) return r;
    }
    r.grammar = true;
    // Significant digits: without leading zeros and without trailing zeros of the fraction.
    std::string f = frac;
    while (!f.empty() && f[f.size() - 1] == '0') f.erase(f.size() - 1);
    std::string all = whole + f;
    size_t lead = 0;
    while (lead < all.size() && all[lead] == '0') ++lead;
    r.significant = (int)(all.size() - lead);
    // Value: strtold of a normalized text, then the unit and scale.
    while (exp.size() > 1 && exp[0] == '0') exp.erase(0, 1);
    if (exp.size() > 6) exp = "999999"; // beyond any long double exponent anyway
    const std::string text = (whole.empty() ? "0" : whole) + "." + (frac.empty() ? "0" : frac) + "e" +
                             (expNegative ? "-" : "") + (exp.empty() ? "0" : exp);
    long double v = strtold(text.c_str(), nullptr);
    v *= powl(10.0L, (long double)(unitExp + scaleExp));
    r.value = negative ? -v : v;
    return r;
}

int run(const uint8_t* data, size_t size) {
    if (size < 2) return 0;
    const int8_t scaleExp = (int8_t)(data[0] % 9) - 4;
    const Unit* units = TABLES[data[1] % 4];
    const char* s = reinterpret_cast<const char*>(data + 2);
    const size_t n = size - 2;
    const std::string text(s, n);

    int64_t v = 12345;
    const Parse p = ScpiNumber::decimal(s, n, scaleExp, units, &v);
    const Reference ref = reference(text, scaleExp, units);

    // Any length, not only framed lines: REST may pass longer tokens.
    if (!ref.grammar) {
        FUZZ_CHECK(p == Parse::Syntax);
    } else if (ref.significant > 18) {
        FUZZ_CHECK(p == Parse::Syntax);
    } else {
        FUZZ_CHECK(p != Parse::Syntax);
        const long double limit = 9223372036854775807.0L;
        const long double mag = fabsl(ref.value);
        if (p == Parse::Ok) {
            FUZZ_CHECK(v != INT64_MIN);
            FUZZ_CHECK(fabsl((long double)v - ref.value) <= 0.5L + mag * 1e-15L);
        } else {
            FUZZ_CHECK(p == Parse::Range);
            FUZZ_CHECK(mag >= limit * (1 - 1e-15L));
        }
        if (mag <= limit * (1 - 1e-15L)) FUZZ_CHECK(p == Parse::Ok);
    }
    if (p != Parse::Ok) FUZZ_CHECK(v == 12345); // nothing written on failure

    // frequency() = decimal() with HZ/KHZ/MHZ plus the range check.
    int64_t hzSigned = 0;
    const Parse pd = ScpiNumber::decimal(s, n, 0, FREQ, &hzSigned);
    uint64_t hz = 777;
    const Parse pf = ScpiNumber::frequency(s, n, &hz);
    if (pd != Parse::Ok) {
        FUZZ_CHECK(pf == pd && hz == 777);
    } else if (hzSigned < (int64_t)FREQ_MIN_HZ || hzSigned > (int64_t)FREQ_MAX_HZ) {
        FUZZ_CHECK(pf == Parse::Range && hz == 777);
    } else {
        FUZZ_CHECK(pf == Parse::Ok && hz == (uint64_t)hzSigned);
    }

    // nullptr arguments are refused.
    FUZZ_CHECK(ScpiNumber::decimal(nullptr, n, 0, nullptr, &v) == Parse::Syntax);
    FUZZ_CHECK(ScpiNumber::decimal(s, n, 0, nullptr, nullptr) == Parse::Syntax);
    return 0;
}

} // namespace fuzz_scpi_number

FUZZ_TARGET(scpi_number, fuzz_scpi_number::run)
