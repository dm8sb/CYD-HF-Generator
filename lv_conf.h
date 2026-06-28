// Root-level LVGL configuration shim for PlatformIO library builds.
// Some PlatformIO/LVGL combinations do not add the project's include/
// directory while compiling LVGL itself. Keeping this small shim beside
// platformio.ini makes #include "lv_conf.h" resolve reliably.
#include "include/lv_conf.h"
