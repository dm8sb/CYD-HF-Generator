// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// What a widget event does (packed with an argument into the LVGL event user data). Shared by
// Gui and WifiPages (AF49b).
enum class GuiAction : uint8_t {
    Navigate,       // arg: PageId
    ChannelSwitch,  // arg: channel 1..3
    FreqUp,
    FreqDown,
    FreqTap,        // tap on the frequency: digit selects the step, unit opens the numpad
    SetStep,        // arg: DELTA_STEPS index
    Ch3Master,      // arg: 1/2
    Ch3Phase,       // arg: phase / 90
    CorrUp,
    CorrDown,
    CorrStepCycle,
    BacklightDown,
    BacklightUp,
    Drive,          // dropdowns: the selected index is the value
    Tuning,
    Spread,
    OffState,
    NumpadKey,
    TouchCal,       // CONFIG: start the touch calibration
    LockScreen,     // CONFIG: lock and go to MAIN (U-13)
    RotateScreen,   // CONFIG: screen upright / turned by 180 degrees (E-53)
    ResetDefaults,  // CONFIG: confirm, then defaults (correction kept)
    FactoryReset,   // CONFIG: confirm, then erase the NVS and restart
    Ch3MasterFreq,  // CH3 page: tap on the frequency opens the master's numpad (U-16)
    Ch3InfoTap,     // CH3 page: status line; switches the master on while CH3 waits for it
    WifiMode,       // CONFIG: Off / Client / Access point
    RemoteOutputs,  // CONFIG: allowed / local only (W-E25)
    WifiEdit,       // WIFI: arg slot 0/1, scan and choose a network
    WifiDelete,     // WIFI: arg slot, confirm and delete
    WifiRescan,
    WifiScanPage,   // arg 0 back, 1 forward
    WifiPick,       // arg row on the page
    WifiOther,      // hidden network: enter the name
    EntryShow,      // text entry: show / hide the password
    EntryChanged,
    EntryDone,      // keyboard OK
    ApRegenerate,   // AP page: new random password
    ApSet,          // AP page: type a password
    ApOpenToggle,   // AP page: open (with confirmation) or secure again
    DevName,        // device page: edit the name
    DevIpMode,      // device page: DHCP / Static (dropdown)
    DevIpField,     // device page: arg 0..3 address, mask, gateway, DNS
    DevWebPassword, // device page: set, change or remove the web password
    RemoteDisconnect, // CONFIG Net tab: disconnect all web and SCPI clients
    ConfigTab,        // CONFIG tab bar: arg tab 0..CONFIG_TAB_COUNT-1
    Ch2Couple         // CH2 page: arg 0 Free, 1 CH1 + offset (E-52)
};
