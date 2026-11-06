#include "scpi.h"

#include <stdio.h>
#include <string.h>
#include <config.h>
#include <constants.h>
#include <utils/logger.h>

// ---------------------------------------------------------------------------
// Numbers

namespace ScpiNumber {

namespace {
bool digit(char c) { return c >= '0' && c <= '9'; }
bool blank(char c) { return c == ' ' || c == '\t'; }
char up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }

const Unit FREQ_UNITS[] = {{"HZ", 0}, {"KHZ", 3}, {"MHZ", 6}, {nullptr, 0}};

// 10^e for 0 <= e <= 19.
uint64_t pow10u(int e) {
    uint64_t p = 1;
    while (e-- > 0) p *= 10;
    return p;
}
}

Parse decimal(const char* s, size_t n, int8_t scaleExp, const Unit* units, int64_t* out) {
    if (!s || !out) return Parse::Syntax;
    size_t i = 0;
    while (i < n && blank(s[i])) ++i;
    bool negative = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) negative = s[i++] == '-';
    // Significant digits (leading zeros skipped) and the decimal exponent of the last digit.
    uint64_t mantissa = 0;
    int significant = 0;
    int64_t exp10 = 0;
    int digits = 0;
    while (i < n && digit(s[i])) {
        if (significant > 0 || s[i] != '0') {
            if (++significant > 18) return Parse::Syntax;
            mantissa = mantissa * 10 + (uint64_t)(s[i] - '0');
        }
        ++digits;
        ++i;
    }
    if (i < n && s[i] == '.') {
        ++i;
        int zeros = 0; // trailing zeros of the fraction are kept back, they change nothing
        while (i < n && digit(s[i])) {
            if (s[i] == '0') {
                ++zeros;
            } else {
                for (; zeros > 0; --zeros) {
                    if (significant > 0) {
                        if (++significant > 18) return Parse::Syntax;
                        mantissa *= 10;
                    }
                    --exp10;
                }
                if (++significant > 18) return Parse::Syntax;
                mantissa = mantissa * 10 + (uint64_t)(s[i] - '0');
                --exp10;
            }
            ++digits;
            ++i;
        }
    }
    if (digits == 0) return Parse::Syntax;
    if (i < n && (s[i] == 'E' || s[i] == 'e')) {
        ++i;
        bool expNegative = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) expNegative = s[i++] == '-';
        // Saturates above the token length: the digits can shift the decimal point by at most n
        // places, so a larger exponent decides the result alone (out of range or 0).
        const int64_t cap = (int64_t)n + 40;
        int64_t e = 0;
        int expDigits = 0;
        while (i < n && digit(s[i])) {
            if (e <= cap) e = e * 10 + (s[i] - '0');
            ++expDigits;
            ++i;
        }
        if (expDigits == 0) return Parse::Syntax;
        exp10 += expNegative ? -e : e;
    }
    while (i < n && blank(s[i])) ++i;
    // Unit: the rest of the token.
    if (i < n) {
        const size_t len = n - i;
        bool found = false;
        for (const Unit* u = units; u && u->name; ++u) {
            if (strlen(u->name) != len) continue;
            size_t k = 0;
            while (k < len && up(s[i + k]) == u->name[k]) ++k;
            if (k == len) {
                exp10 += u->exp10;
                found = true;
                break;
            }
        }
        if (!found) return Parse::Syntax;
    }
    exp10 += scaleExp;
    if (mantissa == 0) {
        *out = 0;
        return Parse::Ok;
    }
    uint64_t magnitude;
    if (exp10 >= 0) {
        if (exp10 > 18) return Parse::Range;
        const uint64_t p = pow10u((int)exp10);
        if (mantissa > (uint64_t)INT64_MAX / p) return Parse::Range;
        magnitude = mantissa * p;
    } else if (exp10 < -19) {
        magnitude = 0; // below half a unit
    } else {
        const uint64_t p = pow10u((int)-exp10);
        magnitude = mantissa / p;
        const uint64_t rest = mantissa % p;
        if (rest >= p - rest) ++magnitude; // half away from zero
    }
    if (magnitude > (uint64_t)INT64_MAX) return Parse::Range;
    *out = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return Parse::Ok;
}

Parse frequency(const char* s, size_t n, uint64_t* hz) {
    int64_t v = 0;
    const Parse p = decimal(s, n, 0, FREQ_UNITS, &v);
    if (p != Parse::Ok) return p;
    if (v < (int64_t)FREQ_MIN_HZ || v > (int64_t)FREQ_MAX_HZ) return Parse::Range;
    *hz = (uint64_t)v;
    return Parse::Ok;
}

} // namespace ScpiNumber

// ---------------------------------------------------------------------------
// Handlers

namespace {

using ScpiNumber::Parse;
using ScpiNumber::Unit;

ScpiSession& session(scpi_t* c) { return *static_cast<ScpiSession*>(c->user_context); }

void error(scpi_t* c, int16_t code, const char* info = nullptr) {
    SCPI_ErrorPushEx(c, code, const_cast<char*>(info), 0);
}

// Result of a command as SCPI error (none for Ok and NoChange).
scpi_result_t answer(scpi_t* c, const ScpiBackend::Outcome& o) {
    switch (o.push) {
        case PushResult::Full: error(c, SCPI_ERROR_EXECUTION_ERROR, "Busy"); return SCPI_RES_ERR;
        case PushResult::RateLimited: error(c, SCPI_ERROR_EXECUTION_ERROR, "Too many commands"); return SCPI_RES_ERR;
        case PushResult::Queued: break;
    }
    switch (o.result) {
        case Result::Ok:
        case Result::NoChange: return SCPI_RES_OK;
        case Result::OutOfRange: error(c, SCPI_ERROR_DATA_OUT_OF_RANGE); break;
        case Result::Invalid: error(c, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE); break;
        case Result::Conflict: error(c, SCPI_ERROR_SETTINGS_CONFLICT, "Not in Integer tuning"); break;
        case Result::Denied: error(c, SCPI_ERROR_SETTINGS_CONFLICT, "Remote outputs: local only"); break;
        case Result::HwError: error(c, SCPI_ERROR_HARDWARE_ERROR, "SI5351"); break;
    }
    return SCPI_RES_ERR;
}

scpi_result_t submit(scpi_t* c, CommandType type, uint8_t channel, int64_t value) {
    // The library reports extra parameters only after the handler: refuse before executing.
    const lex_state_t& lex = c->param_list.lex_state;
    for (const char* p = lex.pos; p < lex.buffer + lex.len; ++p) {
        if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            error(c, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
            return SCPI_RES_ERR;
        }
    }
    Command cmd {};
    cmd.type = type;
    cmd.channel = channel;
    cmd.value = value;
    cmd.source = session(c).source();
    return answer(c, session(c).backend().submit(cmd));
}

bool state(scpi_t* c, Snapshot* s) {
    // A snapshot being written is retried; the loop task finishes it within microseconds.
    for (uint8_t i = 0; i < 100; ++i) {
        if (session(c).backend().snapshot(s)) return true;
    }
    error(c, SCPI_ERROR_SYSTEM_ERROR, "No state");
    return false;
}

// Header suffix ("OUTPut2") in lo..hi, default 1; -114 otherwise.
bool suffix(scpi_t* c, int32_t lo, int32_t hi, uint8_t* out) {
    int32_t n = 1;
    SCPI_CommandNumbers(c, &n, 1, 1);
    if (n < lo || n > hi) {
        error(c, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
        return false;
    }
    *out = (uint8_t)n;
    return true;
}

bool mnemonicIs(const scpi_parameter_t& p, const char* pattern) {
    return SCPI_Match(pattern, p.ptr, (size_t)p.len);
}

struct NumberSpec {
    int8_t scaleExp;
    const Unit* units;
    int64_t min, max, def; // MINimum, MAXimum, DEFault
};

// One numeric parameter as exact integer: decimal with optional unit, or MIN/MAX/DEF.
// NAN, INF, other words and hex/octal/binary: -224; strings and blocks: -104.
bool number(scpi_t* c, const NumberSpec& spec, int64_t* out) {
    scpi_parameter_t p;
    if (!SCPI_Parameter(c, &p, TRUE)) return false; // -109 pushed
    switch (p.type) {
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA:
        case SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX: {
            const Parse r = ScpiNumber::decimal(p.ptr, (size_t)p.len, spec.scaleExp, spec.units, out);
            if (r == Parse::Ok) return true;
            error(c, r == Parse::Range ? SCPI_ERROR_DATA_OUT_OF_RANGE : SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return false;
        }
        case SCPI_TOKEN_PROGRAM_MNEMONIC:
            if (mnemonicIs(p, "MINimum")) *out = spec.min;
            else if (mnemonicIs(p, "MAXimum")) *out = spec.max;
            else if (mnemonicIs(p, "DEFault")) *out = spec.def;
            else break;
            return true;
        case SCPI_TOKEN_HEXNUM:
        case SCPI_TOKEN_OCTNUM:
        case SCPI_TOKEN_BINNUM: break;
        default:
            error(c, SCPI_ERROR_DATA_TYPE_ERROR);
            return false;
    }
    error(c, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
    return false;
}

void resultFixed(scpi_t* c, double v, int decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    SCPI_ResultMnemonic(c, buf);
}

const scpi_choice_def_t SPREAD_CHOICES[] = {
    {"OFF", (int32_t)Spread::Off}, {"DOWN05", (int32_t)Spread::Down05},
    {"DOWN15", (int32_t)Spread::Down15}, {"CENT05", (int32_t)Spread::Center05}, SCPI_CHOICE_LIST_END};
const scpi_choice_def_t TUNING_CHOICES[] = {
    {"FRACtional", (int32_t)TuningMode::Fractional}, {"LNOise", (int32_t)TuningMode::LowNoise},
    {"INTeger", (int32_t)TuningMode::Integer}, SCPI_CHOICE_LIST_END};
const scpi_choice_def_t OFFSTATE_CHOICES[] = {
    {"LOW", (int32_t)OffState::Low}, {"HIGH", (int32_t)OffState::High}, {"HIZ", (int32_t)OffState::HighZ},
    SCPI_CHOICE_LIST_END};
const char* const TUNING_NAMES[] = {"FRAC", "LNO", "INT"};

scpi_result_t choice(scpi_t* c, const scpi_choice_def_t* choices, CommandType type) {
    int32_t v = 0;
    if (!SCPI_ParamChoice(c, choices, &v, TRUE)) return SCPI_RES_ERR; // -224 / -109 pushed
    return submit(c, type, 0, v);
}

// --- OUTPut ---------------------------------------------------------------

scpi_result_t outputState(scpi_t* c) {
    uint8_t ch;
    if (!suffix(c, 1, 3, &ch)) return SCPI_RES_ERR;
    scpi_bool_t on;
    if (!SCPI_ParamBool(c, &on, TRUE)) return SCPI_RES_ERR;
    return submit(c, CommandType::SetEnabled, ch, on ? 1 : 0);
}

scpi_result_t outputStateQ(scpi_t* c) {
    uint8_t ch;
    Snapshot s;
    if (!suffix(c, 1, 3, &ch) || !state(c, &s)) return SCPI_RES_ERR;
    const bool on = ch == 1 ? s.state.ch1.enabled : ch == 2 ? s.state.ch2.enabled : s.state.ch3.enabled;
    SCPI_ResultBool(c, on);
    return SCPI_RES_OK;
}

scpi_result_t outputStatusQ(scpi_t* c) {
    uint8_t ch;
    Snapshot s;
    if (!suffix(c, 1, 3, &ch) || !state(c, &s)) return SCPI_RES_ERR;
    const char* text = "OFF";
    if (ch < 3) {
        switch (s.status[ch - 1]) {
            case ChannelStatus::Off: text = "OFF"; break;
            case ChannelStatus::On: text = "ON"; break;
            case ChannelStatus::Error: text = "ERR"; break;
            case ChannelStatus::NoDevice: text = "NOHW"; break;
        }
    } else {
        switch (s.ch3Status) {
            case Ch3Status::Off: text = "OFF"; break;
            case Ch3Status::Active: text = "ON"; break;
            case Ch3Status::MasterOff:
            case Ch3Status::FreqTooLow:
            case Ch3Status::FreqTooHigh: text = "WAIT"; break;
            case Ch3Status::Error: text = "ERR"; break;
            case Ch3Status::NoDevice: text = "NOHW"; break;
        }
    }
    SCPI_ResultMnemonic(c, text);
    return SCPI_RES_OK;
}

const NumberSpec DRIVE_SPEC {0, nullptr, 2, 8, DEFAULT_DRIVE_MA};

scpi_result_t drive(scpi_t* c) {
    int64_t v;
    if (!number(c, DRIVE_SPEC, &v)) return SCPI_RES_ERR;
    return submit(c, CommandType::Drive, 0, v);
}

scpi_result_t driveQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt32(c, s.state.sys.drive_ma);
    return SCPI_RES_OK;
}

scpi_result_t offState(scpi_t* c) { return choice(c, OFFSTATE_CHOICES, CommandType::OffState); }

scpi_result_t offStateQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    const char* name = "LOW";
    SCPI_ChoiceToName(OFFSTATE_CHOICES, (int32_t)s.state.sys.off_state, &name);
    SCPI_ResultMnemonic(c, name);
    return SCPI_RES_OK;
}

scpi_result_t remoteQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultMnemonic(c, s.remoteOutputsAllowed ? "ALLOWED" : "LOCAL");
    return SCPI_RES_OK;
}

// --- SOURce ---------------------------------------------------------------

scpi_result_t frequency(scpi_t* c) {
    uint8_t ch;
    if (!suffix(c, 1, 3, &ch)) return SCPI_RES_ERR;
    const NumberSpec spec {0, nullptr, (int64_t)FREQ_MIN_HZ, (int64_t)FREQ_MAX_HZ,
                           (int64_t)(ch == 2 ? DEFAULT_FREQ_CH2 : DEFAULT_FREQ_CH1)};
    scpi_parameter_t p;
    int64_t hz = 0;
    // The frequency takes HZ/KHZ/MHZ; the rest of number() applies unchanged.
    if (!SCPI_Parameter(c, &p, TRUE)) return SCPI_RES_ERR;
    if (p.type == SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA || p.type == SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA_WITH_SUFFIX) {
        uint64_t v = 0;
        const Parse r = ScpiNumber::frequency(p.ptr, (size_t)p.len, &v);
        if (r != Parse::Ok) {
            error(c, r == Parse::Range ? SCPI_ERROR_DATA_OUT_OF_RANGE : SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
        hz = (int64_t)v;
    } else if (p.type == SCPI_TOKEN_PROGRAM_MNEMONIC) {
        if (mnemonicIs(p, "MINimum")) hz = spec.min;
        else if (mnemonicIs(p, "MAXimum")) hz = spec.max;
        else if (mnemonicIs(p, "DEFault")) hz = spec.def;
        else {
            error(c, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        }
    } else if (p.type == SCPI_TOKEN_HEXNUM || p.type == SCPI_TOKEN_OCTNUM || p.type == SCPI_TOKEN_BINNUM) {
        error(c, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
        return SCPI_RES_ERR;
    } else {
        error(c, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    if (ch == 3) {
        error(c, SCPI_ERROR_SETTINGS_CONFLICT, "CH3 follows its master");
        return SCPI_RES_ERR;
    }
    return submit(c, CommandType::SetFrequency, ch, hz);
}

uint64_t requested(const Snapshot& s, uint8_t ch) {
    if (ch == 3) ch = s.state.ch3.master;
    return ch == 2 ? s.state.ch2.frequency : s.state.ch1.frequency;
}

scpi_result_t frequencyQ(scpi_t* c) {
    uint8_t ch;
    Snapshot s;
    if (!suffix(c, 1, 3, &ch) || !state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt64(c, requested(s, ch));
    return SCPI_RES_OK;
}

scpi_result_t actualQ(scpi_t* c) {
    uint8_t ch;
    Snapshot s;
    if (!suffix(c, 1, 3, &ch) || !state(c, &s)) return SCPI_RES_ERR;
    resultFixed(c, s.actualHz[ch - 1], 1);
    return SCPI_RES_OK;
}

bool ch3Only(scpi_t* c) {
    uint8_t ch;
    if (!suffix(c, 3, 3, &ch)) return false;
    return true;
}

const NumberSpec MASTER_SPEC {0, nullptr, 1, 2, 1};
const Unit DEG_UNITS[] = {{"DEG", 0}, {nullptr, 0}};
const NumberSpec PHASE_SPEC {0, DEG_UNITS, 0, 270, 90};

scpi_result_t iqMaster(scpi_t* c) {
    int64_t v;
    if (!ch3Only(c) || !number(c, MASTER_SPEC, &v)) return SCPI_RES_ERR;
    return submit(c, CommandType::Ch3Master, 3, v);
}

scpi_result_t iqMasterQ(scpi_t* c) {
    Snapshot s;
    if (!ch3Only(c) || !state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt32(c, s.state.ch3.master);
    return SCPI_RES_OK;
}

scpi_result_t phase(scpi_t* c) {
    int64_t v;
    if (!ch3Only(c) || !number(c, PHASE_SPEC, &v)) return SCPI_RES_ERR;
    return submit(c, CommandType::Ch3Phase, 3, v);
}

scpi_result_t phaseQ(scpi_t* c) {
    Snapshot s;
    if (!ch3Only(c) || !state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt32(c, s.state.ch3.phase);
    return SCPI_RES_OK;
}

scpi_result_t iqRangeQ(scpi_t* c) {
    Snapshot s;
    if (!ch3Only(c) || !state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt64(c, s.iqMinHz);
    SCPI_ResultUInt64(c, s.iqMaxHz);
    return SCPI_RES_OK;
}

scpi_result_t spread(scpi_t* c) {
    uint8_t ch;
    if (!suffix(c, 1, 1, &ch)) return SCPI_RES_ERR;
    return choice(c, SPREAD_CHOICES, CommandType::Spread);
}

scpi_result_t spreadQ(scpi_t* c) {
    uint8_t ch;
    Snapshot s;
    if (!suffix(c, 1, 1, &ch) || !state(c, &s)) return SCPI_RES_ERR;
    const char* name = "OFF";
    SCPI_ChoiceToName(SPREAD_CHOICES, (int32_t)s.state.sys.spread, &name);
    SCPI_ResultMnemonic(c, name);
    return SCPI_RES_OK;
}

// --- SYSTem ---------------------------------------------------------------

scpi_result_t tuning(scpi_t* c) { return choice(c, TUNING_CHOICES, CommandType::Tuning); }

scpi_result_t tuningQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    const uint8_t t = (uint8_t)s.state.sys.tuning_mode;
    SCPI_ResultMnemonic(c, TUNING_NAMES[t <= 2 ? t : 0]);
    return SCPI_RES_OK;
}

const Unit PPM_UNITS[] = {{"PPM", 0}, {nullptr, 0}};
const NumberSpec CORRECTION_SPEC {2, PPM_UNITS, -CORRECTION_MAX, CORRECTION_MAX, 0};

scpi_result_t correction(scpi_t* c) {
    int64_t v;
    if (!number(c, CORRECTION_SPEC, &v)) return SCPI_RES_ERR;
    return submit(c, CommandType::Correction, 0, v);
}

scpi_result_t correctionQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    resultFixed(c, s.state.sys.freq_correction / 100.0, 2);
    return SCPI_RES_OK;
}

const Unit HZ_UNITS[] = {{"HZ", 0}, {"KHZ", 3}, {"MHZ", 6}, {nullptr, 0}};
const NumberSpec STEP_SPEC {0, HZ_UNITS, (int64_t)DELTA_STEPS[0], (int64_t)DELTA_STEPS[DELTA_STEP_COUNT - 1],
                            DEFAULT_DELTA_STEP};

scpi_result_t step(scpi_t* c) {
    int64_t v;
    if (!number(c, STEP_SPEC, &v)) return SCPI_RES_ERR;
    return submit(c, CommandType::Step, 0, v);
}

scpi_result_t stepQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt32(c, s.state.sys.delta_step);
    return SCPI_RES_OK;
}

const Unit PCT_UNITS[] = {{"PCT", 0}, {nullptr, 0}};
const NumberSpec BACKLIGHT_SPEC {0, PCT_UNITS, BACKLIGHT_MIN_PERCENT, 100, DEFAULT_BACKLIGHT_PERCENT};

scpi_result_t backlight(scpi_t* c) {
    int64_t v;
    if (!number(c, BACKLIGHT_SPEC, &v)) return SCPI_RES_ERR;
    return submit(c, CommandType::Backlight, 0, v);
}

scpi_result_t backlightQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultUInt32(c, s.state.sys.backlight);
    return SCPI_RES_OK;
}

scpi_result_t si5351Q(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    const char* text = "NFO"; // not found
    switch (s.link) {
        case LinkState::Connected: text = "CONN"; break;
        case LinkState::NotFound: text = "NFO"; break;
        case LinkState::Blocked: text = "BLOC"; break;
        case LinkState::UnknownDevice: text = "UNKN"; break;
    }
    SCPI_ResultMnemonic(c, text);
    return SCPI_RES_OK;
}

scpi_result_t heapQ(scpi_t* c) {
    uint32_t freeBytes = 0, minFree = 0, largest = 0;
    session(c).backend().heap(&freeBytes, &minFree, &largest);
    SCPI_ResultUInt32(c, freeBytes);
    SCPI_ResultUInt32(c, minFree);
    SCPI_ResultUInt32(c, largest);
    return SCPI_RES_OK;
}

scpi_result_t identify(scpi_t* c) { return submit(c, CommandType::Identify, 0, 0); }
scpi_result_t save(scpi_t* c) { return submit(c, CommandType::SaveNow, 0, 0); }

// The oldest buffered log line as a string, "" when none is left (W-E27).
scpi_result_t logQ(scpi_t* c) {
    char line[Logger::LINE_LEN + 2];
    line[0] = '\0';
    if (Logger::readBuffered(line, sizeof(line)) > 0) {
        const size_t n = strlen(line);
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';
    }
    SCPI_ResultText(c, line);
    return SCPI_RES_OK;
}

// --- IEEE 488.2 -----------------------------------------------------------

scpi_result_t tstQ(scpi_t* c) {
    Snapshot s;
    if (!state(c, &s)) return SCPI_RES_ERR;
    SCPI_ResultInt32(c, s.hardwareReady ? 0 : 1); // 1: no SI5351
    return SCPI_RES_OK;
}

// Patterns: short form upper case, [] optional, # numeric suffix (ARBEITSPLAN 3.5).
const scpi_command_t COMMANDS[] = {
    {"*CLS", SCPI_CoreCls, 0},
    {"*ESE", SCPI_CoreEse, 0},
    {"*ESE?", SCPI_CoreEseQ, 0},
    {"*ESR?", SCPI_CoreEsrQ, 0},
    {"*IDN?", SCPI_CoreIdnQ, 0},
    {"*OPC", SCPI_CoreOpc, 0},
    {"*OPC?", SCPI_CoreOpcQ, 0},
    {"*RST", SCPI_CoreRst, 0},
    {"*SRE", SCPI_CoreSre, 0},
    {"*SRE?", SCPI_CoreSreQ, 0},
    {"*STB?", SCPI_CoreStbQ, 0},
    {"*TST?", tstQ, 0},
    {"*WAI", SCPI_CoreWai, 0},

    {"OUTPut#[:STATe]", outputState, 0},
    {"OUTPut#[:STATe]?", outputStateQ, 0},
    // Not STATus: its short form STAT would also match STATe.
    {"OUTPut#:CONDition?", outputStatusQ, 0},
    {"OUTPut:DRIVe", drive, 0},
    {"OUTPut:DRIVe?", driveQ, 0},
    {"OUTPut:OFFState", offState, 0},
    {"OUTPut:OFFState?", offStateQ, 0},
    {"OUTPut:REMote?", remoteQ, 0},

    // The library knows no optional first node: [SOURce#:] is listed twice.
    {"FREQuency[:CW]", frequency, 0},
    {"FREQuency[:CW]?", frequencyQ, 0},
    {"FREQuency:ACTual?", actualQ, 0},
    {"SOURce#:FREQuency[:CW]", frequency, 0},
    {"SOURce#:FREQuency[:CW]?", frequencyQ, 0},
    {"SOURce#:FREQuency:ACTual?", actualQ, 0},
    {"SOURce#:IQ:MASTer", iqMaster, 0},
    {"SOURce#:IQ:MASTer?", iqMasterQ, 0},
    {"SOURce#:IQ:RANGe?", iqRangeQ, 0},
    {"SOURce#:PHASe", phase, 0},
    {"SOURce#:PHASe?", phaseQ, 0},
    {"SSC", spread, 0},
    {"SSC?", spreadQ, 0},
    {"SOURce#:SSC", spread, 0},
    {"SOURce#:SSC?", spreadQ, 0},

    {"SYSTem:TUNing", tuning, 0},
    {"SYSTem:TUNing?", tuningQ, 0},
    {"SYSTem:CORRection", correction, 0},
    {"SYSTem:CORRection?", correctionQ, 0},
    {"SYSTem:STEP", step, 0},
    {"SYSTem:STEP?", stepQ, 0},
    {"SYSTem:BACKlight", backlight, 0},
    {"SYSTem:BACKlight?", backlightQ, 0},
    {"SYSTem:SI5351?", si5351Q, 0},
    {"SYSTem:HEAP?", heapQ, 0},
    {"SYSTem:IDENtify", identify, 0},
    {"SYSTem:LOG?", logQ, 0},
    {"SYSTem:SAVE", save, 0},
    {"SYSTem:ERRor[:NEXT]?", SCPI_SystemErrorNextQ, 0},
    {"SYSTem:ERRor:COUNt?", SCPI_SystemErrorCountQ, 0},
    {"SYSTem:VERSion?", SCPI_SystemVersionQ, 0},
    SCPI_CMD_LIST_END};

const scpi_unit_def_t NO_UNITS[] = {SCPI_UNITS_LIST_END};

size_t writeCb(scpi_t* c, const char* data, size_t len) {
    session(c).write(data, len);
    return len;
}

int errorCb(scpi_t*, int_fast16_t) { return 0; }
scpi_result_t controlCb(scpi_t*, scpi_ctrl_name_t, scpi_reg_val_t) { return SCPI_RES_OK; }
scpi_result_t flushCb(scpi_t*) { return SCPI_RES_OK; }

// *RST (W-E19): outputs off, defaults in RAM, nothing saved.
scpi_result_t resetCb(scpi_t* c) { return submit(c, CommandType::ResetRemote, 0, 0); }

} // namespace

// ---------------------------------------------------------------------------
// Session

ScpiSession::ScpiSession(ScpiBackend& backend, Source source, const char* serial)
    : backend_(backend), source_(source) {
    snprintf(serial_, sizeof(serial_), "%s", serial ? serial : "0");
    iface_.error = errorCb;
    iface_.write = writeCb;
    iface_.control = controlCb;
    iface_.flush = flushCb;
    iface_.reset = resetCb;
    SCPI_Init(&ctx_, COMMANDS, &iface_, NO_UNITS, "CYD", "HF-Generator", serial_, APP_VERSION, input_,
              sizeof(input_), errors_, (int16_t)ERROR_QUEUE);
    SCPI_InitHeap(&ctx_, errorInfo_, sizeof(errorInfo_));
    ctx_.user_context = this;
}

void ScpiSession::setSerial(const char* serial) {
    snprintf(serial_, sizeof(serial_), "%s", serial ? serial : "0");
}

void ScpiSession::write(const char* data, size_t len) {
    if (writer_ && len) writer_(data, len, writerCtx_);
}

// From scpi-parser's parser_private.h (not an installed header).
extern "C" int scpiParser_detectProgramMessageUnit(scpi_parser_state_t* state, char* buffer, int len);

// scpi-parser resynchronizes one character after an invalid one and parses the rest of the
// unit as a new header: "FREQ 7MHZ 8MHZ" gives -101 and then -113 "MHZ". Blanking the rest
// of an invalid unit up to its ';' or the line end leaves one error per broken unit; the
// units after it run as usual (IEEE 488.2 resynchronizes at the separator).
static void blankAfterInvalid(char* line, int len) {
    scpi_parser_state_t st;
    int pos = 0;
    while (pos < len) {
        memset(&st, 0, sizeof(st));
        const int r = scpiParser_detectProgramMessageUnit(&st, line + pos, len - pos);
        if (r <= 0) break;
        pos += r;
        if (st.programHeader.type != SCPI_TOKEN_INVALID) continue;
        // The library consumed the invalid character line[pos - 1]; it may open a quote.
        const int start = pos;
        char quote = 0;
        for (int i = start - 1; i < len; ++i) {
            const char c = line[i];
            if (quote) {
                if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (i >= start && (c == ';' || c == '\n' || c == '\r')) {
                break;
            }
            if (i >= start) {
                line[i] = ' ';
                pos = i + 1;
            }
        }
    }
}

void ScpiSession::execute(const char* line, size_t len) {
    if (!line) return;
    if (len > LineFramer::MAX_LINE) len = LineFramer::MAX_LINE;
    memcpy(line_, line, len);
    line_[len] = '\n';
    line_[len + 1] = '\0';
    blankAfterInvalid(line_, (int)(len + 1));
    SCPI_Parse(&ctx_, line_, (int)(len + 1));
}

void ScpiSession::pushError(int16_t code) {
    if (code) SCPI_ErrorPush(&ctx_, code);
}

int32_t ScpiSession::errorCount() { return SCPI_ErrorCount(&ctx_); }
