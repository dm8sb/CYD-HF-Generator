#include "settings.h"
#include <constants.h>
#include <utils/logger.h>
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
    s.sys.backlight = storage_.getU8("backlight", DEFAULT_BACKLIGHT_PERCENT);
    // Values below the minimum (e.g. 0 % from firmware 1.0.0) would leave the screen dark.
    if (s.sys.backlight > 100 || s.sys.backlight < BACKLIGHT_MIN_PERCENT) s.sys.backlight = DEFAULT_BACKLIGHT_PERCENT;
    s.sys.delta_step = FrequencyUtils::snapStep(storage_.getU32("delta", DEFAULT_DELTA_STEP));
    s.sys.freq_correction = constrain(storage_.getI32("corr", 0), -CORRECTION_MAX, CORRECTION_MAX);
    s.sys.default_drive_strength = storage_.getU8("drv_def", DEFAULT_DRIVE_MA);
    if (!isValidDrive(s.sys.default_drive_strength)) s.sys.default_drive_strength = DEFAULT_DRIVE_MA;
    // One global drive setting from CONFIG applies to all channels.
    s.ch1.drive_strength = s.sys.default_drive_strength;
    s.ch2.drive_strength = s.sys.default_drive_strength;
    s.ch3.drive_strength = s.sys.default_drive_strength;
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
    ok &= storage_.putU8("drv_def", s.sys.default_drive_strength);
    if (ok) Logger::info("SETTINGS", "Saved to NVS");
    else Logger::error("SETTINGS", "Saving to NVS failed");
    return ok;
}

void SettingsService::markDirty() {
    dirty_ = true;
    lastChange_ = millis();
}

SaveResult SettingsService::update(const PersistentState& state) {
    if (!dirty_ || millis() - lastChange_ < SETTINGS_AUTOSAVE_MS) return SaveResult::None;
    dirty_ = false;
    if (!available_) return SaveResult::None;
    saveFailed_ = !save_(state);
    return saveFailed_ ? SaveResult::Failed : SaveResult::Saved;
}
