#pragma once

#include <stdint.h>

struct Channel {
    uint64_t frequency;   // requested frequency in Hz (exact, 1 Hz resolution)
    bool enabled;
};

struct Channel3 {
    bool enabled;       // requested by the user; see Ch3Status for the actual output state
    uint8_t master;     // 1 or 2
    uint16_t phase;     // 0/90/180/270
};

// SI5351 divider strategy (CONFIG "Tuning"). Values are stored in NVS.
enum class TuningMode : uint8_t {
    Fractional = 0, // fixed PLL, fractional MultiSynth: finest steps, tuning without dropouts
    LowNoise = 1,   // even integer MultiSynth, fractional PLL
    Integer = 2     // integer PLL and MultiSynth: frequency on a raster
};

// Spread spectrum on PLL A, i.e. CH1 and CH3 with master CH1 (CONFIG "Spread CH1"). Stored in NVS.
enum class Spread : uint8_t { Off = 0, Down05 = 1, Down15 = 2, Center05 = 3 };

// Level of a switched-off output (CONFIG "Off state", SI5351 register 24). Stored in NVS.
enum class OffState : uint8_t { Low = 0, High = 1, HighZ = 2 };

struct SystemSettings {
    uint32_t delta_step;
    int32_t freq_correction; // 0.01 ppm
    uint8_t backlight;  // BACKLIGHT_MIN_PERCENT-100 % in BACKLIGHT_STEP_PERCENT steps
    uint8_t drive_ma;   // all outputs: 2, 4, 6, 8
    TuningMode tuning_mode;
    Spread spread;      // always Off in TuningMode::Integer
    OffState off_state;
};

struct TouchCalibration {
    int32_t x_min;
    int32_t x_max;
    int32_t y_min;
    int32_t y_max;
};

struct PersistentState {
    Channel ch1;
    Channel ch2;
    Channel3 ch3;
    SystemSettings sys;
};

// Actual state of CH1 / CH2; the switch shows the requested state.
enum class ChannelStatus : uint8_t {
    Off,
    On,
    Error,      // setup failed although the SI5351 answers; retried every HW_CHECK_INTERVAL_MS
    NoDevice    // SI5351 not found or lost
};

enum class Ch3Status : uint8_t {
    Off,        // CH3 switched off
    Active,     // IQ output running
    MasterOff,  // CH3 on, but master channel off
    FreqTooLow, // CH3 on, but master below IQ_MIN_HZ
    Error,      // IQ setup failed; CLK2 is powered down, the master keeps running
    NoDevice    // SI5351 not found
};

enum class PageId : uint8_t {
    Main,
    CH1,
    CH2,
    CH3,
    Config,
    Numpad
};
