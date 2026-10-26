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
    void setGlobalDriveStrength(uint8_t strength);
    void setBacklight(uint8_t percent);
    void setDeltaStep(uint32_t step);
    void setCorrection(int32_t correction);

    const Channel& ch1() const { return state_.ch1; }
    const Channel& ch2() const { return state_.ch2; }
    const Channel& channel(uint8_t ch) const { return channel_(ch); }
    const Channel3& ch3() const { return state_.ch3; }
    const SystemSettings& system() const { return state_.sys; }
    const PersistentState& state() const { return state_; }

    bool hardwareReady() const { return si5351_.ready(); }
    bool hasUnsavedChanges() const { return settings_.isDirty(); }
    uint64_t ch3Frequency() const { return masterFrequency_(); }
    Ch3Status ch3Status() const;
    // ch 1..3; expected output frequency incl. SI5351 rounding (0 if not configured).
    double actualHz(uint8_t ch) const { return si5351_.actualHz(ch - 1); }

private:
    Channel& channel_(uint8_t ch);
    const Channel& channel_(uint8_t ch) const;
    uint8_t clkForChannel_(uint8_t ch) const { return ch - 1; }
    SI5351Manager::Pll pllForChannel_(uint8_t ch) const { return ch == 1 ? SI5351Manager::Pll::A : SI5351Manager::Pll::B; }
    void markChanged_();
    void reportError_(const char* title, const char* text);
    bool iqConfigured_() const;
    void configureStandalone_(uint8_t ch);
    void configureMasterGroup_();
    void configureChannel_(uint8_t ch);
    void reconfigureOutputs_();
    bool masterEnabled_() const;
    uint64_t masterFrequency_() const;

    SettingsService& settings_;
    SI5351Manager& si5351_;
    BacklightDriver& backlight_;
    PersistentState state_ {};
    ErrorCallback errorCb_ = nullptr;
};
