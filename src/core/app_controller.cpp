#include "app_controller.h"
#include <constants.h>
#include <utils/logger.h>
#include <utils/frequency_utils.h>

AppController::AppController(SettingsService& settings, SI5351Manager& si5351, BacklightDriver& backlight)
    : settings_(settings), si5351_(si5351), backlight_(backlight) {}

void AppController::begin(const PersistentState& loaded) {
    state_ = loaded;
    backlight_.setPercent(state_.sys.backlight);
}

void AppController::applyStoredChannelSettings() {
    reconfigureOutputs_();
    Logger::info("APP", "Stored channel settings applied");
}

void AppController::update() {
    settings_.update(state_);
}

Channel& AppController::channel_(uint8_t ch) { return (ch == 1) ? state_.ch1 : state_.ch2; }
const Channel& AppController::channel_(uint8_t ch) const { return (ch == 1) ? state_.ch1 : state_.ch2; }

void AppController::markChanged_() { settings_.markDirty(); }

void AppController::reportError_(const char* title, const char* text) {
    Logger::error(title, text);
    if (errorCb_) errorCb_(title, text);
}

bool AppController::masterEnabled_() const { return state_.ch3.master == 1 ? state_.ch1.enabled : state_.ch2.enabled; }
uint64_t AppController::masterFrequency_() const { return state_.ch3.master == 1 ? state_.ch1.frequency : state_.ch2.frequency; }

// CH3 runs as IQ follower only while requested and the master frequency allows it.
// Otherwise the master runs standalone and CLK2 stays off, but ch3.enabled keeps the user's choice.
bool AppController::iqConfigured_() const {
    return state_.ch3.enabled && masterFrequency_() >= IQ_MIN_HZ;
}

Ch3Status AppController::ch3Status() const {
    if (!state_.ch3.enabled) return Ch3Status::Off;
    if (!si5351_.ready()) return Ch3Status::NoDevice;
    if (masterFrequency_() < IQ_MIN_HZ) return Ch3Status::FreqTooLow;
    if (!masterEnabled_()) return Ch3Status::MasterOff;
    return Ch3Status::Active;
}

void AppController::configureStandalone_(uint8_t ch) {
    const Channel& c = channel_(ch);
    const uint8_t clk = clkForChannel_(ch);
    if (!si5351_.setupStandalone(clk, pllForChannel_(ch), c.frequency, c.drive_strength)) {
        reportError_("SI5351", "Frequency set failed");
        return;
    }
    si5351_.enableOutput(clk, c.enabled);
}

void AppController::configureMasterGroup_() {
    const uint8_t master = state_.ch3.master;
    if (!iqConfigured_()) {
        si5351_.enableOutput(CLK_CH3, false);
        configureStandalone_(master);
        return;
    }
    const uint8_t masterClk = clkForChannel_(master);
    if (!si5351_.setupIQPair(masterClk, CLK_CH3, pllForChannel_(master), masterFrequency_(),
                             channel_(master).drive_strength, state_.ch3.phase)) {
        reportError_("SI5351", "CH3 IQ setup failed");
        // Keep at least the master running.
        si5351_.enableOutput(CLK_CH3, false);
        configureStandalone_(master);
        return;
    }
    si5351_.enableOutput(masterClk, masterEnabled_());
    si5351_.enableOutput(CLK_CH3, masterEnabled_());
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
            si5351_.enableOutput(clkForChannel_(ch), en);
            if (ch == state_.ch3.master && iqConfigured_()) si5351_.enableOutput(CLK_CH3, en);
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
