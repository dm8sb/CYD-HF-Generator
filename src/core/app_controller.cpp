#include "app_controller.h"

#include <constants.h>
#include <hal/i2c_bus.h>
#include <drivers/si5351_plan.h>
#include <utils/logger.h>
#include <utils/frequency_utils.h>
#include <utils/status_text.h>

static const char* MSG_HW_LOST = "Connection lost. Check the cable; SDA/SCL may be in either order. Reconnecting automatically.";

AppController::AppController(SettingsService& settings, SI5351Manager& si5351, BacklightDriver& backlight)
    : settings_(settings), si5351_(si5351), backlight_(backlight) {}

void AppController::begin(const PersistentState& loaded) {
    state_ = loaded;
    // Before connectHardware(): begin() writes the off state and turns spread spectrum off;
    // the spread option follows with the first PLL A setup.
    si5351_.setTuningMode(state_.sys.tuning_mode);
    si5351_.setSpread(state_.sys.spread);
    si5351_.setOffState(state_.sys.off_state);
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
        if (si5351_.checkAlive()) {
            if (anyFault_()) {
                quietRetry_ = true;
                reconfigureOutputs_();
                quietRetry_ = false;
                if (!anyFault_()) Logger::info("APP", "Output error cleared");
            }
            return;
        }
        link_ = LinkState::NotFound;
        bump_();
        reportError_("SI5351", MSG_HW_LOST);
        // Reconnect at once (audit C-12): after a power glitch the chip answers again right away,
        // so the outputs are back within one cycle instead of two.
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
    if (linkKnown_ && s == link_) return;
    linkKnown_ = true;
    link_ = s;
    bump_();
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
    // A failed save stays visible while its retries run (the change is still pending then).
    if (settings_.lastSaveFailed()) return SaveState::Failed;
    return settings_.isDirty() ? SaveState::Pending : SaveState::Saved;
}

Channel& AppController::channel_(uint8_t ch) { return (ch == 1) ? state_.ch1 : state_.ch2; }
const Channel& AppController::channel(uint8_t ch) const {
    static const Channel none {0, false};
    return ch == 1 ? state_.ch1 : ch == 2 ? state_.ch2 : none;
}
const Channel& AppController::channel_(uint8_t ch) const { return (ch == 1) ? state_.ch1 : state_.ch2; }

void AppController::markChanged_() {
    settings_.markDirty();
    bump_();
}

void AppController::reportError_(const char* title, const char* text) {
    if (quietRetry_) return;
    Logger::error(title, text);
    if (errorCb_) errorCb_(title, text);
}

// A failed output enable or register 24 write means the transfer failed, and the driver has
// already marked the SI5351 as lost (audit C-6): report that, not an output error.
void AppController::reportLinkLost_() { reportError_("SI5351", MSG_HW_LOST); }

void AppController::reportHardwareError_(const char* text) {
    reportError_("SI5351", si5351_.ready() ? text : MSG_HW_LOST);
}

bool AppController::masterEnabled_() const { return state_.ch3.master == 1 ? state_.ch1.enabled : state_.ch2.enabled; }
uint64_t AppController::masterFrequency_() const { return state_.ch3.master == 1 ? state_.ch1.frequency : state_.ch2.frequency; }

// CH3 runs as IQ follower only while requested and the master frequency allows it.
// Otherwise the master runs standalone and CLK2 stays off, but ch3.enabled keeps the user's choice.
bool AppController::iqConfigured_() const {
    return state_.ch3.enabled && masterFrequency_() >= iqMinHz() && masterFrequency_() <= iqMaxHz();
}

uint64_t AppController::iqMinHz() const { return ::iqMinHz(spreadOf(3)); }
uint64_t AppController::iqMaxHz() const { return ::iqMaxHz(spreadOf(3)); }

ChannelStatus AppController::channelStatus(uint8_t ch) const {
    if (!si5351_.ready()) return ChannelStatus::NoDevice;
    if (fault_[clkForChannel_(ch)]) return ChannelStatus::Error;
    return channel_(ch).enabled ? ChannelStatus::On : ChannelStatus::Off;
}

Ch3Status AppController::ch3Status() const {
    if (!state_.ch3.enabled) return Ch3Status::Off;
    if (!si5351_.ready()) return Ch3Status::NoDevice;
    if (masterFrequency_() < iqMinHz()) return Ch3Status::FreqTooLow;
    if (masterFrequency_() > iqMaxHz()) return Ch3Status::FreqTooHigh;
    if (fault_[CLK_CH3]) return Ch3Status::Error;
    if (!masterEnabled_()) return Ch3Status::MasterOff;
    return Ch3Status::Active;
}

void AppController::configureStandalone_(uint8_t ch) {
    if (!si5351_.ready()) return;
    const Channel& c = channel_(ch);
    const uint8_t clk = clkForChannel_(ch);
    const bool ok = si5351_.setupStandalone(clk, pllForChannel_(ch), c.frequency, state_.sys.drive_ma) &&
                    si5351_.enableOutput(clk, c.enabled);
    // A lost SI5351 is shown as NO HW, not as an output error.
    fault_[clk] = !ok && si5351_.ready();
    bump_(); // actual frequency or error state may have changed
    if (!ok) {
        char text[96];
        StatusText::outputErrorText(ch, 0, text, sizeof(text));
        reportHardwareError_(text);
    }
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
                             state_.sys.drive_ma, state_.ch3.phase)) {
        fault_[CLK_CH3] = si5351_.ready();
        char text[96];
        StatusText::outputErrorText(3, master, text, sizeof(text));
        reportHardwareError_(text);
        // Keep at least the master running.
        si5351_.powerDown(CLK_CH3);
        configureStandalone_(master);
        return;
    }
    fault_[masterClk] = fault_[CLK_CH3] = false;
    bump_();
    if (!si5351_.enableOutput(masterClk, masterEnabled_()) || !si5351_.enableOutput(CLK_CH3, masterEnabled_())) {
        reportLinkLost_();
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

Result AppController::outcome_(bool wasReady, uint8_t clk) const {
    if (!wasReady) return Result::Ok; // stored, shown as NO HW
    return (!si5351_.ready() || fault_[clk]) ? Result::HwError : Result::Ok;
}

Result AppController::setChannelFrequency(uint8_t ch, uint64_t freq) {
    if (ch < 1 || ch > 2) return Result::Invalid;
    if (freq < FREQ_MIN_HZ || freq > FREQ_MAX_HZ) return Result::OutOfRange;
    if (freq == channel_(ch).frequency) return Result::NoChange;
    channel_(ch).frequency = freq;
    Logger::debugValue("APP", ch == 1 ? "CH1 frequency" : "CH2 frequency", (int64_t)channel_(ch).frequency);
    const bool wasReady = si5351_.ready();
    configureChannel_(ch);
    markChanged_();
    return outcome_(wasReady, clkForChannel_(ch));
}

Result AppController::adjustChannelFrequency(uint8_t ch, int64_t delta) {
    if (ch < 1 || ch > 2) return Result::Invalid;
    int64_t f = (int64_t)channel_(ch).frequency + delta;
    if (f < (int64_t)FREQ_MIN_HZ) f = FREQ_MIN_HZ;
    if (f > (int64_t)FREQ_MAX_HZ) f = FREQ_MAX_HZ;
    return setChannelFrequency(ch, (uint64_t)f);
}

Result AppController::setChannelEnabled(uint8_t ch, bool en) {
    if (ch < 1 || ch > 3) return Result::Invalid;
    // Output states are not stored (A-4): no autosave, no flash write (audit C-5).
    if (ch == 3) {
        if (state_.ch3.enabled == en) return Result::NoChange;
        state_.ch3.enabled = en;
        bump_();
        const bool wasReady = si5351_.ready();
        if (wasReady) configureMasterGroup_();
        return outcome_(wasReady, CLK_CH3);
    }
    if (channel_(ch).enabled == en) return Result::NoChange;
    channel_(ch).enabled = en;
    bump_();
    if (!si5351_.ready()) return Result::Ok;
    bool ok = si5351_.enableOutput(clkForChannel_(ch), en);
    // CLK2 follows only a working IQ pair: in ERR it stays off (with off state High/High-Z
    // its driver is still powered and would output again).
    if (ch == state_.ch3.master && iqConfigured_() && !fault_[CLK_CH3]) {
        ok = ok && si5351_.enableOutput(CLK_CH3, en);
    }
    if (!ok) {
        reportLinkLost_();
        return Result::HwError;
    }
    return Result::Ok;
}

Result AppController::setCH3Master(uint8_t master) {
    if (master != 1 && master != 2) return Result::Invalid;
    if (master == state_.ch3.master) return Result::NoChange;
    state_.ch3.master = master;
    const bool wasReady = si5351_.ready();
    if (state_.ch3.enabled) reconfigureOutputs_();
    markChanged_();
    return state_.ch3.enabled ? outcome_(wasReady, CLK_CH3) : Result::Ok;
}

Result AppController::setCH3Phase(uint16_t phase) {
    if (!isValidPhase(phase)) return Result::Invalid;
    if (phase == state_.ch3.phase) return Result::NoChange;
    state_.ch3.phase = phase;
    const bool wasReady = si5351_.ready();
    const bool iq = wasReady && iqConfigured_();
    if (iq) configureMasterGroup_();
    markChanged_();
    return iq ? outcome_(wasReady, CLK_CH3) : Result::Ok;
}

Result AppController::setGlobalDriveStrength(uint8_t strength) {
    if (!isValidDrive(strength)) return Result::Invalid;
    if (strength == state_.sys.drive_ma) return Result::NoChange;
    state_.sys.drive_ma = strength;
    const bool wasReady = si5351_.ready();
    reconfigureOutputs_();
    markChanged_();
    return (wasReady && !si5351_.ready()) ? Result::HwError : Result::Ok;
}

Result AppController::setBacklight(uint8_t percent) {
    if (percent < BACKLIGHT_MIN_PERCENT || percent > 100) return Result::OutOfRange;
    percent = snapBacklight(percent);
    if (percent == state_.sys.backlight) return Result::NoChange;
    state_.sys.backlight = percent;
    backlight_.setPercent(percent);
    markChanged_();
    return Result::Ok;
}

Result AppController::stepBacklight(int8_t direction) {
    return setBacklight(snapBacklight(state_.sys.backlight + direction * BACKLIGHT_STEP_PERCENT));
}

Result AppController::setTuningMode(TuningMode mode) {
    if (mode > TuningMode::Integer) return Result::Invalid;
    if (mode == state_.sys.tuning_mode) return Result::NoChange;
    state_.sys.tuning_mode = mode;
    if (mode == TuningMode::Integer && state_.sys.spread != Spread::Off) {
        // Spread spectrum needs a fractional PLL A.
        state_.sys.spread = Spread::Off;
        si5351_.setSpread(Spread::Off);
    }
    si5351_.setTuningMode(mode);
    const bool wasReady = si5351_.ready();
    reconfigureOutputs_();
    markChanged_();
    return (wasReady && !si5351_.ready()) ? Result::HwError : Result::Ok;
}

Result AppController::setSpread(Spread spread) {
    if (spread > Spread::Center05) return Result::Invalid;
    if (spread == state_.sys.spread) return Result::NoChange;
    if (state_.sys.tuning_mode == TuningMode::Integer) return Result::Conflict;
    state_.sys.spread = spread;
    si5351_.setSpread(spread);
    const bool wasReady = si5351_.ready();
    reconfigureOutputs_();
    markChanged_();
    return (wasReady && !si5351_.ready()) ? Result::HwError : Result::Ok;
}

void AppController::resetToDefaults() {
    PersistentState d;
    SettingsService::defaults(d);
    d.sys.freq_correction = state_.sys.freq_correction;
    state_ = d;
    si5351_.setTuningMode(d.sys.tuning_mode);
    si5351_.setSpread(d.sys.spread);
    if (!si5351_.setOffState(d.sys.off_state)) reportLinkLost_();
    backlight_.setPercent(d.sys.backlight);
    reconfigureOutputs_();
    markChanged_();
    Logger::info("APP", "Settings reset to defaults; correction kept");
}

Result AppController::setOffState(OffState offState) {
    if (offState > OffState::HighZ) return Result::Invalid;
    if (offState == state_.sys.off_state) return Result::NoChange;
    state_.sys.off_state = offState;
    const bool wasReady = si5351_.ready();
    bool ok = si5351_.setOffState(offState);
    if (!ok) reportLinkLost_();
    // The power-down strategy of switched-off outputs depends on the off state.
    reconfigureOutputs_();
    markChanged_();
    return (wasReady && (!ok || !si5351_.ready())) ? Result::HwError : Result::Ok;
}

Spread AppController::spreadOf(uint8_t ch) const {
    const bool pllA = ch == 1 || (ch == 3 && state_.ch3.master == 1);
    return pllA ? state_.sys.spread : Spread::Off;
}

int AppController::i2cSdaPin() const { return I2CBus::sdaPin(); }
int AppController::i2cSclPin() const { return I2CBus::sclPin(); }
bool AppController::i2cSwapped() const { return I2CBus::map() == I2CMap::Swapped; }

Result AppController::setDeltaStep(uint32_t step) {
    if (FrequencyUtils::snapStep(step) != step) return Result::Invalid;
    if (step == state_.sys.delta_step) return Result::NoChange;
    state_.sys.delta_step = step;
    markChanged_();
    return Result::Ok;
}

Result AppController::setCorrection(int32_t correction) {
    if (correction < -CORRECTION_MAX || correction > CORRECTION_MAX) return Result::OutOfRange;
    if (correction == state_.sys.freq_correction) return Result::NoChange;
    state_.sys.freq_correction = correction;
    si5351_.setCorrection(correction);
    const bool wasReady = si5351_.ready();
    reconfigureOutputs_();
    markChanged_();
    return (wasReady && !si5351_.ready()) ? Result::HwError : Result::Ok;
}

Result AppController::adjustCorrection(int32_t delta) {
    return setCorrection(constrain(state_.sys.freq_correction + delta, -CORRECTION_MAX, CORRECTION_MAX));
}
