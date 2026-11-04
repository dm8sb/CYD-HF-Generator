#include "settings.h"
#include <constants.h>
#include <utils/logger.h>
#include <utils/touch_cal.h>
#include <limits.h>
#include <utils/frequency_utils.h>

bool SettingsService::begin() {
    available_ = storage_.begin(SETTINGS_NAMESPACE, false);
    return available_;
}

void SettingsService::load(PersistentState& s) {
    s.ch1.frequency = FrequencyUtils::clamp(storage_.getU64("ch1_freq", DEFAULT_FREQ_CH1));
    s.ch2.frequency = FrequencyUtils::clamp(storage_.getU64("ch2_freq", DEFAULT_FREQ_CH2));
    // All outputs start off; the on/off state is not restored (keys ch*_en from 1.1.0 are ignored).
    s.ch1.enabled = false;
    s.ch2.enabled = false;
    s.ch3.enabled = false;
    s.ch3.master = storage_.getU8("ch3_master", 1);
    if (s.ch3.master != 1 && s.ch3.master != 2) s.ch3.master = 1;
    s.ch3.phase = storage_.getU32("ch3_phase", 90);
    if (!isValidPhase(s.ch3.phase)) s.ch3.phase = 90;
    // Values below 5 % (e.g. 0 % from firmware 1.0.0) would leave the screen dark; older
    // firmware used 1 % steps, they are rounded to the current steps.
    const uint8_t bl = storage_.getU8("backlight", DEFAULT_BACKLIGHT_PERCENT);
    s.sys.backlight = (bl > 100 || bl < 5) ? DEFAULT_BACKLIGHT_PERCENT : snapBacklight(bl);
    s.sys.delta_step = FrequencyUtils::snapStep(storage_.getU32("delta", DEFAULT_DELTA_STEP));
    s.sys.freq_correction = constrain(storage_.getI32("corr", 0), -CORRECTION_MAX, CORRECTION_MAX);
    // One global drive setting from CONFIG applies to all channels.
    s.sys.drive_ma = storage_.getU8("drv_def", DEFAULT_DRIVE_MA);
    if (!isValidDrive(s.sys.drive_ma)) s.sys.drive_ma = DEFAULT_DRIVE_MA;
    // Missing keys (NVS from 1.2.0) give the behavior of 1.2.0.
    const uint8_t tune = storage_.getU8("tune", 0);
    s.sys.tuning_mode = tune <= (uint8_t)TuningMode::Integer ? (TuningMode)tune : TuningMode::Fractional;
    const uint8_t ssc = storage_.getU8("ssc", 0);
    s.sys.spread = ssc <= (uint8_t)Spread::Center05 ? (Spread)ssc : Spread::Off;
    if (s.sys.tuning_mode == TuningMode::Integer) s.sys.spread = Spread::Off;
    const uint8_t off = storage_.getU8("off_st", 0);
    s.sys.off_state = off <= (uint8_t)OffState::HighZ ? (OffState)off : OffState::Low;
    Logger::info("SETTINGS", available_ ? "Loaded from NVS" : "NVS unavailable; defaults loaded");
}

bool SettingsService::save_(const PersistentState& s) {
    bool ok = storage_.putU64("ch1_freq", s.ch1.frequency);
    ok &= storage_.putU64("ch2_freq", s.ch2.frequency);
    ok &= storage_.putU8("ch3_master", s.ch3.master);
    ok &= storage_.putU32("ch3_phase", s.ch3.phase);
    ok &= storage_.putU8("backlight", s.sys.backlight);
    ok &= storage_.putU32("delta", s.sys.delta_step);
    ok &= storage_.putI32("corr", s.sys.freq_correction);
    ok &= storage_.putU8("drv_def", s.sys.drive_ma);
    ok &= storage_.putU8("tune", (uint8_t)s.sys.tuning_mode);
    ok &= storage_.putU8("ssc", (uint8_t)s.sys.spread);
    ok &= storage_.putU8("off_st", (uint8_t)s.sys.off_state);
    if (ok) Logger::info("SETTINGS", "Saved to NVS");
    else Logger::error("SETTINGS", "Saving to NVS failed");
    return ok;
}

bool SettingsService::loadTouchCalibration(TouchCalibration* out) {
    if (!available_ || !out) return false;
    static constexpr int32_t NONE = INT32_MIN;
    const TouchCalibration c {storage_.getI32("tc_x0", NONE), storage_.getI32("tc_x1", NONE),
                              storage_.getI32("tc_y0", NONE), storage_.getI32("tc_y1", NONE)};
    if (c.x_min == NONE || c.x_max == NONE || c.y_min == NONE || c.y_max == NONE) return false;
    if (!TouchCal::plausible(c)) {
        Logger::error("SETTINGS", "Stored touch calibration implausible; using the default");
        return false;
    }
    *out = c;
    return true;
}

bool SettingsService::saveTouchCalibration(const TouchCalibration& cal) {
    if (!available_) return false;
    bool ok = storage_.putI32("tc_x0", cal.x_min);
    ok &= storage_.putI32("tc_x1", cal.x_max);
    ok &= storage_.putI32("tc_y0", cal.y_min);
    ok &= storage_.putI32("tc_y1", cal.y_max);
    if (!ok) Logger::error("SETTINGS", "Saving the touch calibration failed");
    return ok;
}

bool SettingsService::removeDisplayPanel() {
    if (!available_ || !storage_.remove("disp_panel")) return false;
    Logger::info("SETTINGS", "Stored display choice removed; the controller is detected");
    return true;
}

void SettingsService::markDirty() {
    dirty_ = true;
    retryPending_ = false;
    lastChange_ = millis();
}

SaveResult SettingsService::update(const PersistentState& state) {
    if (!dirty_) return SaveResult::None;
    const uint32_t wait = retryPending_ ? SETTINGS_RETRY_MS : SETTINGS_AUTOSAVE_MS;
    if (millis() - lastChange_ < wait) return SaveResult::None;
    if (!available_) {
        dirty_ = false;
        return SaveResult::None;
    }
    if (save_(state)) {
        dirty_ = retryPending_ = saveFailed_ = false;
        return SaveResult::Saved;
    }
    // Keep the change and try again later (audit C-13); only the first failure of a series
    // is reported, so a broken flash does not open a dialog every 30 s.
    const bool first = !saveFailed_;
    saveFailed_ = retryPending_ = true;
    lastChange_ = millis();
    return first ? SaveResult::Failed : SaveResult::None;
}
