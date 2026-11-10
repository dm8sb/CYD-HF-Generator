#pragma once
// Host screen renderer: LVGL (C) and the GUI share the fake clock of test/fakes/Arduino.h.
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t host_millis(void);
#ifdef __cplusplus
}
#endif
