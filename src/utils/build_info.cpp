#include "build_info.h"
#include <config.h>

// tools/build_info.py defines these for this file only, so a new build time recompiles
// nothing else. __DATE__/__TIME__ would go stale: the file itself never changes.
#ifndef BUILD_INFO_BRANCH
#define BUILD_INFO_BRANCH "unknown"
#endif
#ifndef BUILD_INFO_COMMIT
#define BUILD_INFO_COMMIT "unknown"
#endif
#ifndef BUILD_INFO_TIME
#define BUILD_INFO_TIME "unknown"
#endif

namespace BuildInfo {
const char* version() { return APP_VERSION; }
const char* branch() { return BUILD_INFO_BRANCH; }
const char* commit() { return BUILD_INFO_COMMIT; }
const char* time() { return BUILD_INFO_TIME; }
}
