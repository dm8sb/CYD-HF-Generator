#pragma once

#include <Arduino.h>
#include <types.h>
#include "storage.h"

class SettingsService {
public:
    bool begin();
    void load(PersistentState& state);
    void save(const PersistentState& state);
    void markDirty();
    bool update(const PersistentState& state);
    bool isDirty() const { return dirty_; }

private:
    Storage storage_;
    bool dirty_ = false;
    uint32_t lastChange_ = 0;
};
