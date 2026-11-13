// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <constants.h>
#include <types.h>
#include <services/settings.h>
#include <drivers/si5351_manager.h>
#include <drivers/backlight.h>

// Events instead of a single error callback (V-3). The GUI shows a dialog for them, later the
// SCPI error queue and the WebSocket get them too. Title and text are valid during the call.
enum class EventCategory : uint8_t { Link = 1, Output = 2, Settings = 3 };
enum class EventCode : uint8_t { LinkLost, LinkNotFound, LinkBlocked, LinkUnknownDevice, OutputSetupFailed, IqSetupFailed, SaveFailed };
// Where the command came from that led to the event; Internal = supervision, autosave.
// Web: browser and REST; Scpi: SCPI over TCP; Usb: SCPI over the USB serial port.
enum class Source : uint8_t { Touch, Web, Scpi, Usb, Internal };
inline bool isRemote(Source s) { return s == Source::Web || s == Source::Scpi || s == Source::Usb; }
struct Event {
    EventCode code;
    EventCategory category;
    Source source;
    uint8_t channel; // 1..3 for output events, else 0
    const char* title;
    const char* text;
};
typedef void (*EventListener)(const Event& e, void* ctx);

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

    // Up to MAX_LISTENERS receivers; false if full.
    static constexpr uint8_t MAX_LISTENERS = 3;
    bool addListener(EventListener fn, void* ctx);
    // Source of the following commands (Touch by default; remote servers set theirs).
    void setSource(Source s) {
        source_ = s;
        if (isRemote(s)) {
            lastRemote_ = s;
            lastRemoteMs_ = millis();
        }
    }
    // Last remote source and when it sent a command (status bar REMOTE <source>, W-E14);
    // Source::Touch while none did.
    Source lastRemote() const { return lastRemote_; }
    uint32_t lastRemoteMs() const { return lastRemoteMs_; }

    // Setters return a Result (V-1): set* reject values outside their range or set, adjust* and
    // stepBacklight() clamp. ch: 1/2 for frequencies, 1..3 for outputs.
    Result setChannelFrequency(uint8_t ch, uint64_t freq);
    Result adjustChannelFrequency(uint8_t ch, int64_t delta);
    // A remote source may not switch an output on while remote outputs are local only (Denied).
    Result setChannelEnabled(uint8_t ch, bool en);
    Result setCH3Master(uint8_t master);
    Result setCH3Phase(uint16_t phase);
    Result setGlobalDriveStrength(uint8_t strength);
    // BACKLIGHT_MIN_PERCENT..100, rounded to BACKLIGHT_STEP_PERCENT.
    Result setBacklight(uint8_t percent);
    Result stepBacklight(int8_t direction);
    // One of DELTA_STEPS.
    Result setDeltaStep(uint32_t step);
    // 0.01 ppm, at most +-CORRECTION_MAX.
    Result setCorrection(int32_t correction);
    Result adjustCorrection(int32_t delta);
    // Grundzustand for the next group (U-13): all settings and frequencies back to the defaults,
    // all outputs off; only the frequency correction (calibration) stays. Saved by the autosave.
    void resetToDefaults();
    // *RST (W-E19): outputs off, defaults in RAM (correction kept), pending changes dropped;
    // nothing is saved until saveNow(), a later change saves only its own key.
    void resetForRemote();
    // CONFIG "Remote outputs" (W-E25): allowed (default) or local only. Only the touch source
    // may change it (remote: Denied); saved at once, kept by resetToDefaults() and *RST.
    bool remoteOutputsAllowed() const { return remoteOutputsAllowed_; }
    Result setRemoteOutputsAllowed(bool allowed);
    // SYSTem:IDENtify (W-E28): backlight and LED blink for IDENTIFY_MS; a new call restarts it.
    void identify();
    bool identifying() const { return identifying_; }
    // Blink phase while identifying: true = lit.
    bool identifyLit() const { return identifyLit_; }
    // SYSTem:SAVE: writes every key at once.
    bool saveNow() { return settings_.saveNow(state_); }
    // CONFIG options; each reconfigures the outputs as needed. Spread is ignored (stays Off)
    // in TuningMode::Integer; switching to Integer turns it off.
    Result setTuningMode(TuningMode mode);
    Result setSpread(Spread spread);
    // A remote source may not select OffState::High while remote outputs are local only (Denied, D22).
    Result setOffState(OffState state);

    // ch 1 or 2; anything else gives an empty channel (frequency 0, off), never CH2 (S-12).
    const Channel& channel(uint8_t ch) const;
    const Channel3& ch3() const { return state_.ch3; }
    const SystemSettings& system() const { return state_.sys; }

    bool hardwareReady() const { return si5351_.ready(); }
    LinkState linkState() const;
    // Counts successful connections; the GUI closes outdated connection dialogs when it changes.
    uint16_t connectionCount() const { return connections_; }
    // Changes with every change of state, output, status, error flags, connection or actual
    // frequency (V-2); the GUI, the snapshot and the WebSocket push redraw on it.
    uint32_t revision() const { return revision_; }
    // Dialog text for a state other than Connected.
    static const char* linkText(LinkState s);
    SaveState saveState() const;
    uint64_t ch3Frequency() const { return masterFrequency_(); }
    ChannelStatus channelStatus(uint8_t ch) const;
    Ch3Status ch3Status() const;
    // ch 1..3; expected output frequency incl. SI5351 rounding (0 if not configured).
    // With spread spectrum this is the unmodulated (highest for down spread) frequency.
    double actualHz(uint8_t ch) const { return si5351_.actualHz(ch - 1); }
    // Spread spectrum that applies to ch 1..3 (PLL A: CH1, CH3 with master CH1).
    Spread spreadOf(uint8_t ch) const;
    // Master frequency range for CH3 IQ; spread spectrum on PLL A (master CH1) narrows it.
    uint64_t iqMinHz() const;
    uint64_t iqMaxHz() const;
    // Info for CONFIG: I2C pins of the SI5351 link (also while not connected) and identity.
    int i2cSdaPin() const;
    int i2cSclPin() const;
    bool i2cSwapped() const;
    bool si5351IdUnverified() const { return si5351_.idUnverified(); }

private:
    Channel& channel_(uint8_t ch);
    const Channel& channel_(uint8_t ch) const;
    uint8_t clkForChannel_(uint8_t ch) const { return ch == 1 ? CLK_CH1 : CLK_CH2; }
    SI5351Manager::Pll pllForChannel_(uint8_t ch) const { return ch == 1 ? SI5351Manager::Pll::A : SI5351Manager::Pll::B; }
    // Marks the changed NVS keys; remote changes are saved later (W-E17).
    void markChanged_(uint16_t keys);
    void bump_() { ++revision_; }
    void report_(EventCode code, EventCategory category, const char* title, const char* text, uint8_t channel = 0);
    // An output or IQ setup failed: reports a lost SI5351 instead when the I2C bus failed.
    void reportOutputError_(EventCode code, uint8_t channel, const char* text);
    void reportLinkLost_();
    void superviseHardware_();
    void superviseOnce_();
    // Tries the open SDA/SCL assignment first, then the other one.
    bool connect_();
    void setLinkState_(LinkState s);
    bool anyFault_() const { return fault_[0] || fault_[1] || fault_[2]; }
    bool iqConfigured_() const;
    void configureStandalone_(uint8_t ch);
    void configureMasterGroup_();
    void configureChannel_(uint8_t ch);
    void reconfigureOutputs_();
    // HwError if the SI5351 was ready and the setup of clk failed or lost the connection.
    Result outcome_(bool wasReady, uint8_t clk) const;
    bool masterEnabled_() const;
    uint64_t masterFrequency_() const;

    SettingsService& settings_;
    SI5351Manager& si5351_;
    BacklightDriver& backlight_;
    PersistentState state_ {};
    struct Listener {
        EventListener fn;
        void* ctx;
    };
    Listener listeners_[MAX_LISTENERS] {};
    Source source_ = Source::Touch;
    Source lastRemote_ = Source::Touch;
    uint32_t lastRemoteMs_ = 0;
    bool remoteOutputsAllowed_ = true;
    void updateIdentify_();
    bool identifying_ = false;
    bool identifyLit_ = true;
    uint32_t identifyStartMs_ = 0;
    uint32_t lastHwCheckMs_ = 0;
    uint32_t revision_ = 0;
    LinkState link_ = LinkState::NotFound;
    // False until the first connection attempt: its result is always logged (finding M-1).
    bool linkKnown_ = false;
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
