// Fuzz target scpi: the SCPI session (remote/scpi.cpp on j123b567/scpi-parser) behind the
// LineFramer, as the USB and TCP servers run it (W-S17, W-S19, W-E12).
// Input: 6 configuration bytes, then the byte stream of a connection.
//   byte 0: bit 0 raw lines (split at LF only, no framer: robustness beyond the framer's
//           guarantees), bit 1 no snapshot available, bit 2 log lines for SYSTem:LOG?,
//           bits 3-4 source (Web, Scpi, Usb)
//   bytes 1-4: seed of the fake backend (results, refusals, snapshot values)
//   byte 5: snapshot variant
// The backend is a fake (no AppController): it checks every command the session hands over and
// answers with outcomes drawn from the seed, so all answer paths and query handlers are reached.
// Invariants: commands carry the session's source, channels and enum values in range,
// frequencies within FREQ_MIN_HZ..FREQ_MAX_HZ (W-S19); per framed line at most MAX_COMMANDS
// commands and at most MAX_ANSWER bytes of printable output ending with LF; the error queue
// holds at most 16 entries and drains to 0,"No error".

#include "fuzz_common.h"

#include <string>
#include <vector>
#include <constants.h>
#include <remote/line_framer.h>
#include <remote/scpi.h>
#include <utils/logger.h>

namespace fuzz_scpi {

// Longest answer to one line: 16 program units, each answer below 320 bytes (an error with its
// info text from the 256-byte heap is the longest).
const size_t MAX_ANSWER = LineFramer::MAX_COMMANDS * 320;
// Raw lines (no framer) may hold up to 129 program units.
const size_t MAX_RAW_ANSWER = (LineFramer::MAX_LINE / 2 + 1) * 320;

struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
};

class FuzzBackend : public ScpiBackend {
public:
    Source source = Source::Usb;
    bool noSnapshot = false;
    Rng rng {1};
    Snapshot snap {};
    unsigned submits = 0;

    Outcome submit(const Command& c) override {
        ++submits;
        FUZZ_CHECK(c.source == source);
        switch (c.type) {
            case CommandType::SetFrequency:
                FUZZ_CHECK(c.channel == 1 || c.channel == 2);
                FUZZ_CHECK(c.value >= (int64_t)FREQ_MIN_HZ && c.value <= (int64_t)FREQ_MAX_HZ);
                break;
            case CommandType::AdjustFrequency: FUZZ_CHECK(c.type != CommandType::AdjustFrequency); break;
            case CommandType::SetEnabled:
                FUZZ_CHECK(c.channel >= 1 && c.channel <= 3);
                FUZZ_CHECK(c.value == 0 || c.value == 1);
                break;
            case CommandType::Ch3Master:
            case CommandType::Ch3Phase: FUZZ_CHECK(c.channel == 3); break;
            case CommandType::Tuning: FUZZ_CHECK(c.value >= 0 && c.value <= 2); break;
            case CommandType::Spread: FUZZ_CHECK(c.value >= 0 && c.value <= 3); break;
            case CommandType::OffState: FUZZ_CHECK(c.value >= 0 && c.value <= 2); break;
            case CommandType::ResetRemote:
            case CommandType::SaveNow:
            case CommandType::Identify: FUZZ_CHECK(c.value == 0); break;
            case CommandType::Drive:
            case CommandType::Backlight:
            case CommandType::Step:
            case CommandType::Correction: break; // any int64: the controller checks the range
        }
        Outcome o;
        const uint32_t r = rng.next();
        o.push = (r & 15) == 0 ? PushResult::Full : (r & 15) == 1 ? PushResult::RateLimited : PushResult::Queued;
        o.result = (Result)((r >> 4) % 7);
        return o;
    }
    bool snapshot(Snapshot* out) override {
        if (noSnapshot) return false;
        *out = snap;
        return true;
    }
    void heap(uint32_t* freeBytes, uint32_t* minFree, uint32_t* largest) override {
        *freeBytes = rng.next();
        *minFree = rng.next();
        *largest = rng.next();
    }
};

void fillSnapshot(Snapshot& s, Rng& rng, uint8_t variant) {
    s = Snapshot {};
    s.state.ch1.frequency = FREQ_MIN_HZ + rng.next() % (FREQ_MAX_HZ - FREQ_MIN_HZ + 1);
    s.state.ch2.frequency = variant & 1 ? FREQ_MAX_HZ : FREQ_MIN_HZ;
    s.state.ch1.enabled = variant & 2;
    s.state.ch2.enabled = variant & 4;
    s.state.ch3.enabled = variant & 8;
    s.state.ch3.master = (variant & 16) ? 2 : 1;
    s.state.ch3.phase = (uint16_t)(90 * (rng.next() % 4));
    s.state.sys.delta_step = 1000;
    s.state.sys.freq_correction = (int32_t)(rng.next() % 10001) - 5000;
    s.state.sys.backlight = 80;
    s.state.sys.drive_ma = 8;
    s.state.sys.tuning_mode = (TuningMode)(rng.next() % 3);
    s.state.sys.spread = (Spread)(rng.next() % 4);
    s.state.sys.off_state = (OffState)(rng.next() % 3);
    s.status[0] = (ChannelStatus)(rng.next() % 4);
    s.status[1] = (ChannelStatus)(rng.next() % 4);
    s.ch3Status = (Ch3Status)(rng.next() % 7);
    for (int i = 0; i < 3; ++i) s.actualHz[i] = (rng.next() % 1600000000u) / 10.0;
    s.hardwareReady = variant & 32;
    s.link = (LinkState)(rng.next() % 4);
    s.iqMinHz = 4730000;
    s.iqMaxHz = FREQ_MAX_HZ;
    s.remoteOutputsAllowed = variant & 64;
    s.revision = rng.next();
}

struct Output {
    std::string text;
};
void capture(const char* data, size_t len, void* ctx) { static_cast<Output*>(ctx)->text.append(data, len); }

void checkAnswer(const std::string& a, bool framed) {
    FUZZ_CHECK(a.size() <= (framed ? MAX_ANSWER : MAX_RAW_ANSWER));
    if (a.empty()) return;
    FUZZ_CHECK(a[a.size() - 1] == '\n');
    if (!framed) return;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char c = (unsigned char)a[i];
        FUZZ_CHECK((c >= 0x20 && c <= 0x7E) || c == '\t' || c == '\n');
    }
}

void runLine(ScpiSession& session, FuzzBackend& backend, Output& out, const char* line, size_t len, bool framed,
             bool log) {
    if (log) Logger::info("FUZZ", "a log line for SYSTem:LOG?");
    out.text.clear();
    backend.submits = 0;
    session.execute(line, len);
    // In raw mode a line can carry more ';' and bytes the framer would have refused.
    if (framed) FUZZ_CHECK(backend.submits <= LineFramer::MAX_COMMANDS);
    checkAnswer(out.text, framed);
    const int32_t errors = session.errorCount();
    FUZZ_CHECK(errors >= 0 && errors <= 16);
}

int run(const uint8_t* data, size_t size) {
    if (size < 6) return 0;
    const uint8_t flags = data[0];
    const bool raw = flags & 1;
    const bool log = flags & 4;
    static const Source SOURCES[] = {Source::Usb, Source::Scpi, Source::Web, Source::Usb};

    FuzzBackend backend;
    backend.source = SOURCES[(flags >> 3) & 3];
    backend.noSnapshot = flags & 2;
    backend.rng.s = ((uint32_t)data[1] | (uint32_t)data[2] << 8 | (uint32_t)data[3] << 16 | (uint32_t)data[4] << 24) | 1u;
    fillSnapshot(backend.snap, backend.rng, data[5]);

    // The log ring buffer is global: start empty, collect lines only when asked.
    char drain[256];
    Logger::setOutput(Logger::Output::ToBuffer);
    while (Logger::readBuffered(drain, sizeof(drain)) > 0) {}

    Output out;
    ScpiSession session(backend, backend.source, "0123456789AB");
    session.setWriter(capture, &out);

    const uint8_t* p = data + 6;
    const size_t n = size - 6;
    if (raw) {
        size_t start = 0;
        for (size_t i = 0; i <= n; ++i) {
            if (i == n || p[i] == '\n') {
                // An exact-size copy: ASan reports reads past the line.
                std::vector<char> line(p + start, p + i);
                runLine(session, backend, out, line.empty() ? "" : line.data(), line.size(), false, log);
                start = i + 1;
            }
        }
    } else {
        LineFramer framer;
        for (size_t i = 0; i < n; ++i) {
            const LineFramer::Event e = framer.feed((char)p[i]);
            if (e == LineFramer::Event::Line) {
                std::vector<char> line(framer.line(), framer.line() + framer.length());
                runLine(session, backend, out, line.data(), line.size(), true, log);
            } else if (e != LineFramer::Event::None) {
                session.pushError(framerErrorCode(e));
                FUZZ_CHECK(session.errorCount() <= 16);
            }
        }
    }

    // The error queue drains completely.
    bool empty = false;
    for (int i = 0; i < 18 && !empty; ++i) {
        out.text.clear();
        session.execute("SYST:ERR?", 9);
        checkAnswer(out.text, !raw); // raw lines may bring other bytes into the error info
        empty = out.text == "0,\"No error\"\n";
    }
    FUZZ_CHECK(empty);
    FUZZ_CHECK(session.errorCount() == 0);
    Logger::setOutput(Logger::Output::Off);
    return 0;
}

} // namespace fuzz_scpi

FUZZ_TARGET(scpi, fuzz_scpi::run)
