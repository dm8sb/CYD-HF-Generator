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

void AppController::configureStandalone_(uint8_t ch, bool restoreOutput) {
    if (ch < 1 || ch > 2) return;
    Channel& c = channel_(ch);
    uint8_t clk = clkForChannel_(ch);
    si5351_.enableOutput(clk, false);
    si5351_.setDrive(clk, c.drive_strength);
    if (!si5351_.setFrequency(clk, c.frequency)) {
        reportError_("SI5351", "Frequency set failed");
        return;
    }
    si5351_.enableOutput(clk, restoreOutput && c.enabled);
}

bool AppController::masterEnabled_() const { return state_.ch3.master == 1 ? state_.ch1.enabled : state_.ch2.enabled; }
uint64_t AppController::masterFrequency_() const { return state_.ch3.master == 1 ? state_.ch1.frequency : state_.ch2.frequency; }
uint8_t AppController::masterDrive_() const { return state_.ch3.master == 1 ? state_.ch1.drive_strength : state_.ch2.drive_strength; }

void AppController::configureCH3Follow_(bool restoreOutputs) {
    uint8_t masterClk = (state_.ch3.master == 1) ? CLK_CH1 : CLK_CH2;
    const uint8_t followerClk = CLK_CH3;

    if (!state_.ch3.enabled) {
        si5351_.enableOutput(followerClk, false);
        configureStandalone_(state_.ch3.master, restoreOutputs);
        return;
    }

    if (!si5351_.canApplyPhase(masterFrequency_(), state_.ch3.phase)) {
        reportError_("SI5351", "Selected CH3 phase is not valid at this frequency; CH3 disabled");
        state_.ch3.enabled = false;
        si5351_.enableOutput(followerClk, false);
        configureStandalone_(state_.ch3.master, restoreOutputs);
        markChanged_();
        return;
    }

    si5351_.enableOutput(followerClk, false);
    si5351_.enableOutput(masterClk, false);

    bool ok = si5351_.setupIQPair(masterClk, followerClk, masterFrequency_(), masterDrive_(), state_.ch3.drive_strength, state_.ch3.phase);
    if (!ok) {
        reportError_("SI5351", "CH3 IQ setup failed");
        return;
    }

    if (restoreOutputs) {
        si5351_.enableOutput(masterClk, masterEnabled_());
        si5351_.enableOutput(followerClk, state_.ch3.enabled && masterEnabled_());
    }
}

void AppController::reconfigureOutputs_() {
    si5351_.enableOutput(CLK_CH3, false);
    if (state_.ch3.enabled) {
        uint8_t nonMaster = (state_.ch3.master == 1) ? 2 : 1;
        configureStandalone_(nonMaster, channel_(nonMaster).enabled);
        configureCH3Follow_(true);
    } else {
        configureStandalone_(1, state_.ch1.enabled);
        configureStandalone_(2, state_.ch2.enabled);
        si5351_.enableOutput(CLK_CH3, false);
    }
}

void AppController::setChannelFrequency(uint8_t ch, uint64_t freq) {
    if (ch < 1 || ch > 2) return;
    uint64_t rounded = FrequencyUtils::nearestSi5351Hz(freq);
    channel_(ch).frequency = rounded;
    Logger::valueU64("APP", ch == 1 ? "CH1 frequency" : "CH2 frequency", rounded);

    if (state_.ch3.enabled && state_.ch3.master == ch) configureCH3Follow_(true);
    else configureStandalone_(ch, true);
    markChanged_();
}

void AppController::adjustChannelFrequency(uint8_t ch, int64_t delta) {
    if (ch < 1 || ch > 2) return;
    int64_t f = (int64_t)channel_(ch).frequency + delta;
    if (f < (int64_t)FREQ_MIN_HZ) f = FREQ_MIN_HZ;
    if (f > (int64_t)FREQ_MAX_HZ) f = FREQ_MAX_HZ;
    setChannelFrequency(ch, (uint64_t)f);
}

void AppController::setChannelEnabled(uint8_t ch, bool en) {
    if (ch == 1 || ch == 2) {
        channel_(ch).enabled = en;
        if (state_.ch3.enabled && state_.ch3.master == ch) {
            configureCH3Follow_(true);
        } else {
            si5351_.enableOutput(clkForChannel_(ch), en);
        }
    } else if (ch == 3) {
        state_.ch3.enabled = en;
        reconfigureOutputs_();
    }
    markChanged_();
}

void AppController::setCH3Master(uint8_t master) {
    if (master != 1 && master != 2) return;
    state_.ch3.master = master;
    reconfigureOutputs_();
    markChanged_();
}

void AppController::setCH3Phase(uint16_t phase) {
    if (!(phase == 0 || phase == 90 || phase == 180 || phase == 270)) return;
    if (state_.ch3.enabled && !si5351_.canApplyPhase(masterFrequency_(), phase)) {
        reportError_("SI5351", "Phase is not valid at this frequency");
        return;
    }
    state_.ch3.phase = phase;
    if (state_.ch3.enabled) configureCH3Follow_(true);
    markChanged_();
}

void AppController::setDriveStrength(uint8_t ch, uint8_t strength) {
    if (!(strength == 2 || strength == 4 || strength == 6 || strength == 8)) return;
    if (ch == 1 || ch == 2) {
        channel_(ch).drive_strength = strength;
        if (state_.ch3.enabled && state_.ch3.master == ch) configureCH3Follow_(true);
        else configureStandalone_(ch, true);
    } else if (ch == 3) {
        state_.ch3.drive_strength = strength;
        configureCH3Follow_(true);
    }
    markChanged_();
}

void AppController::setDefaultDriveStrength(uint8_t strength) {
    setGlobalDriveStrength(strength);
}

void AppController::setGlobalDriveStrength(uint8_t strength) {
    if (!(strength == 2 || strength == 4 || strength == 6 || strength == 8)) return;
    state_.sys.default_drive_strength = strength;
    state_.ch1.drive_strength = strength;
    state_.ch2.drive_strength = strength;
    state_.ch3.drive_strength = strength;
    reconfigureOutputs_();
    markChanged_();
}

void AppController::setBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    state_.sys.backlight = percent;
    backlight_.setPercent(percent);
    markChanged_();
}

void AppController::setDeltaStep(uint32_t step) {
    if (step == 0) step = 1;
    state_.sys.delta_step = step;
    markChanged_();
}

void AppController::setCorrection(int32_t correction) {
    state_.sys.freq_correction = correction;
    si5351_.setCorrection(correction);
    applyStoredChannelSettings();
    markChanged_();
}

