#pragma once

#include <Arduino.h>
#include <constants.h>
#include <types.h>
#include "storage.h"

enum class SaveResult : uint8_t { None, Saved, Failed };

// NVS keys of PersistentState as bits: only marked keys are written (V-4).
namespace SettingKey {
enum : uint16_t {
    Ch1Freq = 1 << 0,
    Ch2Freq = 1 << 1,
    Ch3Master = 1 << 2,
    Ch3Phase = 1 << 3,
    Backlight = 1 << 4,
    Delta = 1 << 5,
    Corr = 1 << 6,
    Drive = 1 << 7,
    Tune = 1 << 8,
    Ssc = 1 << 9,
    OffSt = 1 << 10,
    All = (1 << 11) - 1
};
}

class SettingsService {
public:
    // false if NVS is unavailable; load() then returns defaults and nothing is saved.
    bool begin();
    bool available() const { return available_; }
    void load(PersistentState& state);
    // Marks keys for the autosave, which runs quietMs after the last change (5 s touch, 30 s
    // remote). A failed save keeps the keys and is retried every SETTINGS_RETRY_MS; Failed is
    // returned only for the first failure of a series.
    void markDirty(uint16_t keys = SettingKey::All, uint32_t quietMs = SETTINGS_AUTOSAVE_MS);
    // Forgets pending changes (*RST: the defaults are not saved until saveNow()).
    void clearDirty() { dirtyKeys_ = 0; retryPending_ = false; }
    // Writes every key at once (SYSTem:SAVE).
    bool saveNow(const PersistentState& state);
    SaveResult update(const PersistentState& state);
    bool isDirty() const { return dirtyKeys_ != 0; }
    bool lastSaveFailed() const { return saveFailed_; }
    // Deletes "disp_panel", the controller choice of the boot gesture (1.3.0-1.3.1). It took
    // precedence over the detection and nothing can reset it any more. True if it existed.
    bool removeDisplayPanel();
    // Factory reset: deletes all settings including correction and touch calibration.
    bool eraseAll();
    // The state of an empty NVS (what load() gives without stored keys).
    static void defaults(PersistentState& s);
    // Touch calibration of this board (U-26), outside PersistentState: read once at boot,
    // written when the calibration wizard succeeds. false: none stored or not plausible.
    bool loadTouchCalibration(TouchCalibration* out);
    bool saveTouchCalibration(const TouchCalibration& cal);

private:
    bool save_(const PersistentState& state, uint16_t keys);

    Storage storage_;
    bool available_ = false;
    uint16_t dirtyKeys_ = 0;
    uint32_t quietMs_ = SETTINGS_AUTOSAVE_MS;
    bool saveFailed_ = false;
    bool retryPending_ = false;
    uint32_t lastChange_ = 0;
};
