#include "settings.h"
#include <constants.h>
#include <utils/logger.h>
#include <utils/frequency_utils.h>

bool SettingsService::begin() {
    return storage_.begin(SETTINGS_NAMESPACE, false);
}

void SettingsService::load(PersistentState& s) {
    s.ch1.frequency = FrequencyUtils::clamp(storage_.getU64("ch1_freq", DEFAULT_FREQ_CH1));
    s.ch2.frequency = FrequencyUtils::clamp(storage_.getU64("ch2_freq", DEFAULT_FREQ_CH2));
    s.ch1.enabled = storage_.getBool("ch1_en", false);
    s.ch2.enabled = storage_.getBool("ch2_en", false);
    s.ch3.enabled = storage_.getBool("ch3_en", false);
    s.ch3.master = storage_.getU8("ch3_master", 1);
    if (s.ch3.master != 1 && s.ch3.master != 2) s.ch3.master = 1;
    s.ch3.phase = storage_.getU32("ch3_phase", 90);
    if (!(s.ch3.phase == 0 || s.ch3.phase == 90 || s.ch3.phase == 180 || s.ch3.phase == 270)) s.ch3.phase = 90;
    s.sys.backlight = storage_.getU8("backlight", DEFAULT_BACKLIGHT_PERCENT);
    if (s.sys.backlight > 100) s.sys.backlight = DEFAULT_BACKLIGHT_PERCENT;
    s.sys.delta_step = storage_.getU32("delta", DEFAULT_DELTA_STEP);
    if (s.sys.delta_step == 0) s.sys.delta_step = 1;
    s.sys.freq_correction = storage_.getI32("corr", 0);
    s.sys.default_drive_strength = storage_.getU8("drv_def", DEFAULT_DRIVE_MA);
    if (!(s.sys.default_drive_strength == 2 || s.sys.default_drive_strength == 4 || s.sys.default_drive_strength == 6 || s.sys.default_drive_strength == 8)) s.sys.default_drive_strength = DEFAULT_DRIVE_MA;
    // One global drive setting from CONFIG applies to all channels.
    s.ch1.drive_strength = s.sys.default_drive_strength;
    s.ch2.drive_strength = s.sys.default_drive_strength;
    s.ch3.drive_strength = s.sys.default_drive_strength;
    // Touch calibration is intentionally fixed to default values.
    // The runtime calibration UI is disabled; ignore any old NVS calibration.
    s.touch_cal.x_min = 300;
    s.touch_cal.x_max = 3800;
    s.touch_cal.y_min = 300;
    s.touch_cal.y_max = 3800;
    Logger::info("SETTINGS", "Loaded from NVS");
}

void SettingsService::save(const PersistentState& s) {
    storage_.putU64("ch1_freq", s.ch1.frequency);
    storage_.putU64("ch2_freq", s.ch2.frequency);
    storage_.putBool("ch1_en", s.ch1.enabled);
    storage_.putBool("ch2_en", s.ch2.enabled);
    storage_.putBool("ch3_en", s.ch3.enabled);
    storage_.putU8("ch3_master", s.ch3.master);
    storage_.putU32("ch3_phase", s.ch3.phase);
    storage_.putU8("backlight", s.sys.backlight);
    storage_.putU32("delta", s.sys.delta_step);
    storage_.putI32("corr", s.sys.freq_correction);
    storage_.putU8("drv_def", s.sys.default_drive_strength);
    // Per-channel drive and touch calibration are no longer persisted.
    // Drive is global via drv_def; touch always uses defaults.
    dirty_ = false;
    Logger::info("SETTINGS", "Saved to NVS");
}

void SettingsService::markDirty() {
    dirty_ = true;
    lastChange_ = millis();
}

bool SettingsService::update(const PersistentState& state) {
    if (dirty_ && millis() - lastChange_ >= SETTINGS_AUTOSAVE_MS) {
        save(state);
        return true;
    }
    return false;
}
