#pragma once

#include <Arduino.h>
#include <types.h>
#include <services/settings.h>
#include <drivers/si5351_manager.h>
#include <drivers/backlight.h>

typedef void (*ErrorCallback)(const char* title, const char* text);

enum class SaveState : uint8_t { Saved, Pending, Failed, Unavailable };
// SI5351 connection. Blocked: an I2C line stays low. UnknownDevice: something at 0x60 fails
// the identity check and is not programmed until it is unplugged.
enum class LinkState : uint8_t { NotFound, Blocked, UnknownDevice, Connected };

class AppController {
public:
    AppController(SettingsService& settings, SI5351Manager& si5351, BacklightDriver& backlight);

    void begin(const PersistentState& loaded);
    // Finds the SI5351 with either SDA/SCL assignment and initializes it (boot).
    bool connectHardware() { return connect_(); }
    // Autosave and SI5351 supervision (health check, automatic reconnect).
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
    LinkState linkState() const;
    // Counts successful connections; the GUI closes outdated connection dialogs when it changes.
    uint16_t connectionCount() const { return connections_; }
    // Dialog text for a state other than Connected.
    static const char* linkText(LinkState s);
    // True for linkText() and the "Connection lost" text.
    static bool isLinkText(const char* text);
    SaveState saveState() const;
    uint64_t ch3Frequency() const { return masterFrequency_(); }
    ChannelStatus channelStatus(uint8_t ch) const;
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
    // Reports a lost SI5351 instead of `text` when the failure came from the I2C bus.
    void reportHardwareError_(const char* text);
    void superviseHardware_();
    // Tries the open SDA/SCL assignment first, then the other one.
    bool connect_();
    void setLinkState_(LinkState s);
    bool anyFault_() const { return fault_[0] || fault_[1] || fault_[2]; }
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
    uint32_t lastHwCheckMs_ = 0;
    // Starts as Connected so the first result is logged as a change.
    LinkState link_ = LinkState::Connected;
    // Assignment (I2CMap) with a device at 0x60 that is no SI5351, -1 = none.
    int8_t foreignOn_ = -1;
    // The marked device passed the read-only check but kept no register: never retried.
    bool foreignKeptNoWrite_ = false;
    uint16_t connections_ = 0;
    // Per CLK: last setup failed while the SI5351 answered. Cleared by the next successful setup.
    bool fault_[3] {};
    // Periodic retries of failed setups report to the log only, not as a dialog.
    bool quietRetry_ = false;
};
