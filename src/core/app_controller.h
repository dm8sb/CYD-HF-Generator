#pragma once

#include <Arduino.h>
#include <types.h>
#include <services/settings.h>
#include <drivers/si5351_manager.h>
#include <drivers/backlight.h>

typedef void (*ErrorCallback)(const char* title, const char* text);

class AppController {
public:
    AppController(SettingsService& settings, SI5351Manager& si5351, BacklightDriver& backlight);

    void begin(const PersistentState& loaded);
    void update();
    void applyStoredChannelSettings();

    void setErrorCallback(ErrorCallback cb) { errorCb_ = cb; }

    void setChannelFrequency(uint8_t ch, uint64_t freq);
    void adjustChannelFrequency(uint8_t ch, int64_t delta);
    void setChannelEnabled(uint8_t ch, bool en);
    void setCH3Master(uint8_t master);
    void setCH3Phase(uint16_t phase);
    void setDriveStrength(uint8_t ch, uint8_t strength);
    void setDefaultDriveStrength(uint8_t strength);
    void setGlobalDriveStrength(uint8_t strength);
    void setBacklight(uint8_t percent);
    void setDeltaStep(uint32_t step);
    void setCorrection(int32_t correction);

    const Channel& ch1() const { return state_.ch1; }
    const Channel& ch2() const { return state_.ch2; }
    const Channel3& ch3() const { return state_.ch3; }
    const SystemSettings& system() const { return state_.sys; }
    const PersistentState& state() const { return state_; }

private:
    Channel& channel_(uint8_t ch);
    const Channel& channel_(uint8_t ch) const;
    uint8_t clkForChannel_(uint8_t ch) const { return ch - 1; }
    void markChanged_();
    void reportError_(const char* title, const char* text);
    void configureStandalone_(uint8_t ch, bool restoreOutput);
    void configureCH3Follow_(bool restoreOutputs);
    void reconfigureOutputs_();
    bool masterEnabled_() const;
    uint64_t masterFrequency_() const;
    uint8_t masterDrive_() const;

    SettingsService& settings_;
    SI5351Manager& si5351_;
    BacklightDriver& backlight_;
    PersistentState state_ {};
    ErrorCallback errorCb_ = nullptr;
};
