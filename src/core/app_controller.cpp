#include "app_controller.h"
#include <constants.h>
#include <hal/i2c_bus.h>
#include <utils/logger.h>
#include <utils/frequency_utils.h>

static const char* MSG_HW_LOST = "Connection lost. Check the cable; SDA/SCL may be in either order. Reconnecting automatically.";

AppController::AppController(SettingsService& settings, SI5351Manager& si5351, BacklightDriver& backlight)
    : settings_(settings), si5351_(si5351), backlight_(backlight) {}

void AppController::begin(const PersistentState& loaded) {
    state_ = loaded;
}

void AppController::applyStoredChannelSettings() {
    reconfigureOutputs_();
    Logger::info("APP", "Stored channel settings applied");
}

void AppController::update() {
    if (settings_.update(state_) == SaveResult::Failed) {
        reportError_("Settings", "Saving to flash (NVS) failed. Changes will be lost at power off.");
    }
    superviseHardware_();
}

void AppController::superviseHardware_() {
    const uint32_t now = millis();
    if (now - lastHwCheckMs_ < HW_CHECK_INTERVAL_MS) return;
    lastHwCheckMs_ = now;
    if (si5351_.ready()) {
        if (!si5351_.checkAlive()) {
            link_ = LinkState::NotFound;
            reportError_("SI5351", MSG_HW_LOST);
        } else if (anyFault_()) {
            quietRetry_ = true;
            reconfigureOutputs_();
            quietRetry_ = false;
            if (!anyFault_()) Logger::info("APP", "Output error cleared");
        }
        return;
    }
    if (connect_()) {
        Logger::info("APP", "Restoring outputs");
        reconfigureOutputs_();
    }
}

bool AppController::connect_() {
    bool timeout = false;
    bool foreign = false;
    const I2CMap first = I2CBus::map();
    for (uint8_t i = 0; i < 2; ++i) {
        const I2CMap m = i == 0 ? first : (first == I2CMap::Default ? I2CMap::Swapped : I2CMap::Default);
        if (!I2CBus::open(m)) continue;
        const uint8_t r = I2CBus::probe(SI5351Manager::I2C_ADDR);
        if (r == 5) {
            timeout = true;
            continue;
        }
        if (r != 0) {
            if (foreignOn_ == (int8_t)m) foreignOn_ = -1; // removed
            continue;
        }
        if (foreignOn_ == (int8_t)m) {
            // Not written again. A device that failed the read-only check gets another
            // chance once it passes it, e.g. an SI5351 that was still powering up.
            if (foreignKeptNoWrite_ || !si5351_.identify()) {
                foreign = true;
                continue;
            }
            foreignOn_ = -1;
        }
        // On failure the other assignment is still tried: weak pull-ups can fake an ACK.
        if (si5351_.begin(state_.sys.freq_correction)) {
            // A mark from a fake ACK on the other assignment must not outlive a rewiring.
            foreignOn_ = -1;
            I2CBus::setRunClock();
            // Weak pull-ups can pass at the detection clock but not at 400 kHz.
            if (!si5351_.identify()) {
                Logger::error("APP", "I2C fails at 400 kHz; staying at 100 kHz");
                I2CBus::open(m);
            }
            ++connections_;
            setLinkState_(LinkState::Connected);
            return true;
        }
        const SI5351Manager::Error e = si5351_.lastError();
        if (e == SI5351Manager::Error::NotSi5351 || e == SI5351Manager::Error::NoReadback) {
            foreignOn_ = (int8_t)m;
            foreignKeptNoWrite_ = e == SI5351Manager::Error::NoReadback;
            foreign = true;
        }
    }
    setLinkState_(foreign ? LinkState::UnknownDevice : timeout ? LinkState::Blocked : LinkState::NotFound);
    return false;
}

void AppController::setLinkState_(LinkState s) {
    if (s == link_) return;
    link_ = s;
    if (s == LinkState::Connected) {
        char line[64];
        snprintf(line, sizeof(line), "SI5351 at 0x60, SDA=%d SCL=%d%s", I2CBus::sdaPin(), I2CBus::sclPin(),
                 I2CBus::map() == I2CMap::Swapped ? " (swapped)" : "");
        Logger::info("APP", line);
    } else if (s == LinkState::NotFound) {
        Logger::info("APP", "SI5351 not found");
    } else {
        reportError_("SI5351", linkText(s));
    }
}

LinkState AppController::linkState() const {
    if (si5351_.ready()) return LinkState::Connected;
    return link_ == LinkState::Connected ? LinkState::NotFound : link_;
}

bool AppController::isLinkText(const char* text) {
    const LinkState states[] = {LinkState::NotFound, LinkState::Blocked, LinkState::UnknownDevice};
    for (LinkState s : states) {
        if (strcmp(text, linkText(s)) == 0) return true;
    }
    return strcmp(text, MSG_HW_LOST) == 0;
}

const char* AppController::linkText(LinkState s) {
    switch (s) {
        case LinkState::Blocked:
            return "I2C line stays low: short, missing 3.3 V or defective module. Check the cable at CN1. "
                   "Searching automatically.";
        case LinkState::UnknownDevice:
            return "The device at I2C address 0x60 is not an SI5351. It is not programmed. Check the module.";
        default:
            return "SI5351 not found. Check the cable at CN1, 3.3 V and GND. SDA/SCL may be in either order. "
                   "Searching automatically.";
    }
}

SaveState AppController::saveState() const {
    if (!settings_.available()) return SaveState::Unavailable;
    if (settings_.isDirty()) return SaveState::Pending;
    return settings_.lastSaveFailed() ? SaveState::Failed : SaveState::Saved;
}

Channel& AppController::channel_(uint8_t ch) { return (ch == 1) ? state_.ch1 : state_.ch2; }
const Channel& AppController::channel_(uint8_t ch) const { return (ch == 1) ? state_.ch1 : state_.ch2; }

void AppController::markChanged_() { settings_.markDirty(); }

void AppController::reportError_(const char* title, const char* text) {
    if (quietRetry_) return;
    Logger::error(title, text);
    if (errorCb_) errorCb_(title, text);
}

void AppController::reportHardwareError_(const char* text) {
    reportError_("SI5351", si5351_.ready() ? text : MSG_HW_LOST);
}

bool AppController::masterEnabled_() const { return state_.ch3.master == 1 ? state_.ch1.enabled : state_.ch2.enabled; }
uint64_t AppController::masterFrequency_() const { return state_.ch3.master == 1 ? state_.ch1.frequency : state_.ch2.frequency; }

// CH3 runs as IQ follower only while requested and the master frequency allows it.
// Otherwise the master runs standalone and CLK2 stays off, but ch3.enabled keeps the user's choice.
bool AppController::iqConfigured_() const {
    return state_.ch3.enabled && masterFrequency_() >= IQ_MIN_HZ;
}

ChannelStatus AppController::channelStatus(uint8_t ch) const {
    if (!si5351_.ready()) return ChannelStatus::NoDevice;
    if (fault_[clkForChannel_(ch)]) return ChannelStatus::Error;
    return channel_(ch).enabled ? ChannelStatus::On : ChannelStatus::Off;
}

Ch3Status AppController::ch3Status() const {
    if (!state_.ch3.enabled) return Ch3Status::Off;
    if (!si5351_.ready()) return Ch3Status::NoDevice;
    if (masterFrequency_() < IQ_MIN_HZ) return Ch3Status::FreqTooLow;
    if (fault_[CLK_CH3]) return Ch3Status::Error;
    if (!masterEnabled_()) return Ch3Status::MasterOff;
    return Ch3Status::Active;
}

void AppController::configureStandalone_(uint8_t ch) {
    if (!si5351_.ready()) return;
    const Channel& c = channel_(ch);
    const uint8_t clk = clkForChannel_(ch);
    const bool ok = si5351_.setupStandalone(clk, pllForChannel_(ch), c.frequency, c.drive_strength) &&
                    si5351_.enableOutput(clk, c.enabled);
    // A lost SI5351 is shown as NO HW, not as an output error.
    fault_[clk] = !ok && si5351_.ready();
    if (!ok) reportHardwareError_("Frequency set failed");
}

void AppController::configureMasterGroup_() {
    const uint8_t master = state_.ch3.master;
    if (!iqConfigured_()) {
        // An unused MultiSynth would only add spurs.
        si5351_.powerDown(CLK_CH3);
        fault_[CLK_CH3] = false;
        configureStandalone_(master);
        return;
    }
    const uint8_t masterClk = clkForChannel_(master);
    if (!si5351_.setupIQPair(masterClk, CLK_CH3, pllForChannel_(master), masterFrequency_(),
                             channel_(master).drive_strength, state_.ch3.phase)) {
        fault_[CLK_CH3] = si5351_.ready();
        reportHardwareError_("CH3 IQ setup failed");
        // Keep at least the master running.
        si5351_.powerDown(CLK_CH3);
        configureStandalone_(master);
        return;
    }
    fault_[masterClk] = fault_[CLK_CH3] = false;
    if (!si5351_.enableOutput(masterClk, masterEnabled_()) || !si5351_.enableOutput(CLK_CH3, masterEnabled_())) {
        reportHardwareError_("Output switch failed");
    }
}

void AppController::configureChannel_(uint8_t ch) {
    if (!si5351_.ready()) return;
    if (ch == state_.ch3.master) configureMasterGroup_();
    else configureStandalone_(ch);
}

void AppController::reconfigureOutputs_() {
    if (!si5351_.ready()) return;
    configureStandalone_(state_.ch3.master == 1 ? 2 : 1);
    configureMasterGroup_();
}

void AppController::setChannelFrequency(uint8_t ch, uint64_t freq) {
    if (ch < 1 || ch > 2) return;
    channel_(ch).frequency = FrequencyUtils::clamp(freq);
    Logger::valueU64("APP", ch == 1 ? "CH1 frequency" : "CH2 frequency", channel_(ch).frequency);
    configureChannel_(ch);
    markChanged_();
}

void AppController::adjustChannelFrequency(uint8_t ch, int64_t delta) {
    if (ch < 1 || ch > 2) return;
    int64_t f = (int64_t)channel_(ch).frequency + delta;
    if (f < (int64_t)FREQ_MIN_HZ) f = FREQ_MIN_HZ;
    if (f > (int64_t)FREQ_MAX_HZ) f = FREQ_MAX_HZ;
    if ((uint64_t)f == channel_(ch).frequency) return;
    setChannelFrequency(ch, (uint64_t)f);
}

void AppController::setChannelEnabled(uint8_t ch, bool en) {
    if (ch == 1 || ch == 2) {
        channel_(ch).enabled = en;
        if (si5351_.ready()) {
            bool ok = si5351_.enableOutput(clkForChannel_(ch), en);
            if (ch == state_.ch3.master && iqConfigured_()) ok = ok && si5351_.enableOutput(CLK_CH3, en);
            if (!ok) reportHardwareError_("Output switch failed");
        }
    } else if (ch == 3) {
        state_.ch3.enabled = en;
        if (si5351_.ready()) configureMasterGroup_();
    }
    markChanged_();
}

void AppController::setCH3Master(uint8_t master) {
    if ((master != 1 && master != 2) || master == state_.ch3.master) return;
    state_.ch3.master = master;
    if (state_.ch3.enabled) reconfigureOutputs_();
    markChanged_();
}

void AppController::setCH3Phase(uint16_t phase) {
    if (!isValidPhase(phase) || phase == state_.ch3.phase) return;
    state_.ch3.phase = phase;
    if (si5351_.ready() && iqConfigured_()) configureMasterGroup_();
    markChanged_();
}

void AppController::setGlobalDriveStrength(uint8_t strength) {
    if (!isValidDrive(strength)) return;
    state_.sys.default_drive_strength = strength;
    state_.ch1.drive_strength = strength;
    state_.ch2.drive_strength = strength;
    state_.ch3.drive_strength = strength;
    reconfigureOutputs_();
    markChanged_();
}

void AppController::setBacklight(uint8_t percent) {
    percent = constrain(percent, BACKLIGHT_MIN_PERCENT, (uint8_t)100);
    state_.sys.backlight = percent;
    backlight_.setPercent(percent);
    markChanged_();
}

void AppController::setDeltaStep(uint32_t step) {
    state_.sys.delta_step = FrequencyUtils::snapStep(step);
    markChanged_();
}

void AppController::setCorrection(int32_t correction) {
    correction = constrain(correction, -CORRECTION_MAX, CORRECTION_MAX);
    if (correction == state_.sys.freq_correction) return;
    state_.sys.freq_correction = correction;
    si5351_.setCorrection(correction);
    reconfigureOutputs_();
    markChanged_();
}
