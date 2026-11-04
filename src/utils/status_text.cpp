#include "status_text.h"

namespace StatusText {

Text channel(ChannelStatus s) {
    switch (s) {
        case ChannelStatus::On: return {"ON", Tone::Ok};
        case ChannelStatus::Error: return {"ERR", Tone::Error};
        case ChannelStatus::NoDevice: return {"NO HW", Tone::Error};
        case ChannelStatus::Off: break;
    }
    return {"OFF", Tone::Muted};
}

Text ch3(Ch3Status s) {
    switch (s) {
        case Ch3Status::Active: return {"ON", Tone::Ok};
        case Ch3Status::MasterOff:
        case Ch3Status::FreqTooLow:
        case Ch3Status::FreqTooHigh: return {"WAIT", Tone::Warn};
        case Ch3Status::Error: return {"ERR", Tone::Error};
        case Ch3Status::NoDevice: return {"NO HW", Tone::Error};
        case Ch3Status::Off: break;
    }
    return {"OFF", Tone::Muted};
}

}
