#pragma once

#include <Arduino.h>
#include <types.h>
#include "storage.h"

enum class SaveResult : uint8_t { None, Saved, Failed };

class SettingsService {
public:
    // false if NVS is unavailable; load() then returns defaults and nothing is saved.
    bool begin();
    bool available() const { return available_; }
    void load(PersistentState& state);
    void markDirty();
    // Saves SETTINGS_AUTOSAVE_MS after the last change.
    SaveResult update(const PersistentState& state);
    bool isDirty() const { return dirty_; }
    bool lastSaveFailed() const { return saveFailed_; }

private:
    bool save_(const PersistentState& state);

    Storage storage_;
    bool available_ = false;
    bool dirty_ = false;
    bool saveFailed_ = false;
    uint32_t lastChange_ = 0;
};
