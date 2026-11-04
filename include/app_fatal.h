#pragma once

// Last resort for internal errors that must not freeze the device with running outputs
// (audit C-3): logs the reason and restarts. After the restart all outputs are off.
// C linkage: LVGL (C) calls it through LV_ASSERT_HANDLER in lv_conf.h.
#ifdef __cplusplus
extern "C" {
#endif

__attribute__((noreturn)) void app_fatal_restart(const char* reason);

#ifdef __cplusplus
}
#endif
